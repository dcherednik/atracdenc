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
        int Mant[2048];
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

class TCodeTabEncoder : public TDumper {
public:
    TCodeTabEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
private:
};

class TQuantUnitsEncoder : public TDumper {
public:
    TQuantUnitsEncoder() = default;
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override;
private:
    void EncodeQuSpectra(const int* qspec, const size_t num_spec, const size_t idx);
};




}
