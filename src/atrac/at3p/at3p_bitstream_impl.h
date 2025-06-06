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

#include "atrac/at3p/at3p_gha.h"
#include <lib/bitstream/bitstream.h>
#include <lib/bs_encode/encode.h>
#include <atrac/atrac_scale.h>
#include <vector>

namespace NAtracDEnc {

namespace NAt3p {
struct TVlcElement;
}

struct TSpecFrame {
    TSpecFrame(uint32_t sz, uint32_t numQuantUnits, size_t channels,
               const TAt3PGhaData* tonalBlock,
               const std::vector<std::vector<TScaledBlock>>& scaledBlocks)
        : SizeBits(sz)
        , NumQuantUnits(numQuantUnits)
        , TonalBlock(tonalBlock)
        , AllocatedBits(0)
    {
        Chs.reserve(channels);
        for (size_t i = 0; i < channels; i++) {
            Chs.emplace_back(TChannel(scaledBlocks[i]));
        }
    }

    const uint32_t SizeBits;
    uint32_t NumQuantUnits;
    const TAt3PGhaData* TonalBlock;
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
    void Reset() noexcept override {
        Buf.clear();
    }
    uint32_t GetConsumption() const noexcept override;
protected:

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
    void VlEncode(const std::array<NAt3p::TVlcElement, 8>& wlTab, size_t idx, size_t sz, const int8_t* data) noexcept;
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

class TTonalComponentEncoder : public TDumper {
public:
    TTonalComponentEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
    //TODO: find the common way
    void Dump(NBitStream::TBitStream& bs) override {
        TDumper::Dump(bs);
        BitsUsed = 0;
    }
private:
    EStatus CheckFrameDone(TSpecFrame* frame, TBitAllocHandler& ba) noexcept;
    void WriteTonalBlock(size_t channels, const TAt3PGhaData* tonalBlock);
    void WriteSubbandFlags(const bool* flags, size_t numFlags);
    size_t BitsUsed = 0;
};


}
