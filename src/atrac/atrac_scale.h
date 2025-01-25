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

template <class TBaseData>
class TScaler {
    std::map<float, uint8_t> ScaleIndex;
public:
    TScaler();
    TScaledBlock Scale(const float* in, uint16_t len);
    std::vector<TScaledBlock> ScaleFrame(const std::vector<float>& specs, const typename TBaseData::TBlockSizeMod& blockSize);
};

} //namespace NAtracDEnc
