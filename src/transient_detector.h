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
#include <math.h>
#include <cstdint>
#include <vector>

#include "config.h"

namespace NAtracDEnc {

class TTransientDetector {
    const uint32_t ShortSz;
    const uint32_t BlockSz;
    const uint32_t NShortBlocks;
    static const uint32_t PrevBufSz = 20;
    static const uint32_t FIRLen = 21;
    void HPFilter(const TFloat* in, TFloat* out);
    std::vector<TFloat> HPFBuffer;
    TFloat LastEnergy = 0.0;
    uint32_t LastTransientPos = 0;
public:
    TTransientDetector(uint32_t shortSz, uint32_t blockSz)
        : ShortSz(shortSz)
        , BlockSz(blockSz)
        , NShortBlocks(blockSz/shortSz)
    {
        HPFBuffer.resize(BlockSz + FIRLen); 
    }
    bool Detect(const TFloat* buf);
    uint32_t GetLastTransientPos() const { return LastTransientPos; }
};

std::vector<TFloat> AnalyzeGain(const TFloat* in, uint32_t len, uint32_t maxPoints, bool useRms);

}
