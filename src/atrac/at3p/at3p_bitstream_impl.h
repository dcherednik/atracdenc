#pragma once

/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <lib/bitstream/bitstream.h>
#include <lib/bs_encode/encode.h>
#include <atrac/atrac_scale.h>
#include <vector>

namespace NAtracDEnc {

struct TSpecFrame {
    TSpecFrame(size_t sz, size_t channels,
               const std::vector<std::vector<TScaledBlock>>& scaledBlocks)
        : SizeBits(sz)
        , AllocatedBits(0)
    {
        Chs.reserve(channels);
        for (size_t i = 0; i < channels; i++) {
            Chs.emplace_back(TChannel(scaledBlocks[i]));
        }
    }

    const size_t SizeBits;
    size_t NumQuantUnits;
    std::vector<std::pair<uint8_t, uint8_t>> WordLen;
    std::vector<std::pair<uint8_t, uint8_t>> SfIdx;
    std::vector<std::pair<uint8_t, uint8_t>> SpecTabIdx;

    struct TChannel {
        TChannel(const std::vector<TScaledBlock>& scaledBlock)
            : ScaledBlocks(scaledBlock)
        {}
        const std::vector<TScaledBlock>& ScaledBlocks;
    };

    std::vector<TChannel> Chs;
    size_t AllocatedBits;
    static TSpecFrame* Cast(void* p) { return reinterpret_cast<TSpecFrame*>(p); }
};

class TDumper : public IBitStreamPartEncoder {
public:
    void Dump(NBitStream::TBitStream& bs) override {
        for (const auto& pair : Buf) {
            bs.Write(pair.first, pair.second);
        }
        Buf.clear();
    }
protected:
    size_t GetConsumption() const noexcept;
    // value, nbits
    void Insert(uint16_t value, uint8_t nbits) { Buf.emplace_back(std::make_pair(value, nbits)); }
    std::vector<std::pair<uint16_t, uint8_t>> Buf;
};

class TConfigure : public TDumper {
public:
    TConfigure() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
};

class TWordLenEncoder : public TDumper {
public:
    TWordLenEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
private:
};

class TSfIdxEncoder : public TDumper {
public:
    TSfIdxEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
private:
};

class TQuantUnitsEncoder : public TDumper {
public:
    TQuantUnitsEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
    static void EncodeQuSpectra(const int* qspec, const size_t num_spec, const size_t idx,
        std::vector<std::pair<uint16_t, uint8_t>>& data);
    static void EncodeCodeTab(bool useFullTable, size_t channels,
        size_t numQuantUnits, const std::vector<std::pair<uint8_t, uint8_t>>& specTabIdx,
        std::vector<std::pair<uint16_t, uint8_t>>& data);

private:
    class TUnit {
    public:
        static size_t MakeKey(size_t ch, size_t qu, size_t worlen);
        TUnit(size_t qu, size_t wordlen);
        size_t GetOrCompute(const float* val, std::vector<std::pair<uint16_t, uint8_t>>& res);
        const std::vector<int>& GetMantisas() const { return Mantisas; }

    private:
        uint32_t Wordlen;
        float Multiplier;
        uint16_t ConsumedBits; // Number of bits consumed by QuSpectr

        std::vector<int> Mantisas;
    };
    // The key is <ch_id, unit_id, wordlen>
    // will be used to cache unit encoding result duting bit allocation
    std::map<size_t, TUnit> UnitBuffers;
};

}
