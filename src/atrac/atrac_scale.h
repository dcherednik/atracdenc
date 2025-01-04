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

#pragma once
#include <array>
#include <vector>
#include <map>
#include <cstdint>

#include "lib/bitstream/bitstream.h"
#include "../config.h"

namespace NAtracDEnc {

float QuantMantisas(const float* in, uint32_t first, uint32_t last, float mul, bool ea, int* mantisas);

struct TScaledBlock {
    TScaledBlock(uint8_t sfi) : ScaleFactorIndex(sfi) {}
    /* const */ uint8_t ScaleFactorIndex = 0;
    std::vector<float> Values;
    float MaxEnergy;
};

class TBlockSize;

template <class TBaseData>
class TScaler {
    std::map<float, uint8_t> ScaleIndex;
public:
    TScaler();
    TScaledBlock Scale(const float* in, uint16_t len);
    std::vector<TScaledBlock> ScaleFrame(const std::vector<float>& specs, const TBlockSize& blockSize);
};

class TBlockSize {
    static std::array<int, 4> Parse(NBitStream::TBitStream* stream) {
        //ATRAC1 - 3 subbands, ATRAC3 - 4 subbands.
        //TODO: rewrite
        std::array<int, 4> tmp;
        tmp[0] = 2 - stream->Read(2);
        tmp[1] = 2 - stream->Read(2);
        tmp[2] = 3 - stream->Read(2);
        stream->Read(2); //skip unused 2 bits
        return tmp;
    }
    static std::array<int, 4> Create(bool lowShort, bool midShort, bool hiShort) {
        std::array<int, 4> tmp;
        tmp[0] = lowShort ? 2 : 0;
        tmp[1] = midShort ? 2 : 0;
        tmp[2] = hiShort ? 3 : 0;
        return tmp;
    }
public:
    TBlockSize(NBitStream::TBitStream* stream)
        : LogCount(Parse(stream))
    {}
    TBlockSize(bool lowShort, bool midShort, bool hiShort)
        : LogCount(Create(lowShort, midShort, hiShort))
    {}
    TBlockSize()
        : LogCount({{0, 0, 0, 0}})
    {}
    std::array<int, 4> LogCount;
};

} //namespace NAtracDEnc
