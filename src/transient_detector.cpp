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

#include "transient_detector.h"
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <cassert>
#include <iostream>
namespace NAtracDEnc {

using std::vector;
static TFloat calculateRMS(const TFloat* in, uint32_t n) {
    TFloat s = 0;
    for (uint32_t i = 0; i < n; i++) {
        s += (in[i] * in[i]);
    }
    s /= n;
    return sqrt(s);
}

static TFloat calculatePeak(const TFloat* in, uint32_t n) {
    TFloat s = 0;
    for (uint32_t i = 0; i < n; i++) {
        TFloat absVal = std::abs(in[i]);
        if (absVal > s)
            s = absVal;
    }
    return s;
}

void TTransientDetector::HPFilter(const TFloat* in, TFloat* out) {
    static const TFloat fircoef[] = {
        -8.65163e-18 * 2.0, -0.00851586 * 2.0, -6.74764e-18 * 2.0, 0.0209036 * 2.0,
        -3.36639e-17 * 2.0, -0.0438162 * 2.0, -1.54175e-17 * 2.0, 0.0931738 * 2.0,
        -5.52212e-17 * 2.0, -0.313819 * 2.0
    };
    memcpy(HPFBuffer.data() + PrevBufSz, in, BlockSz * sizeof(TFloat));
    const TFloat* inBuf = HPFBuffer.data();
    for (size_t i = 0; i < BlockSz; ++i) {
        TFloat s = inBuf[i + 10];
        TFloat s2 = 0;
        for (size_t j = 0; j < ((FIRLen - 1) / 2) - 1 ; j += 2) {
            s += fircoef[j] * (inBuf[i + j] + inBuf[i + FIRLen - j]);
            s2 += fircoef[j + 1] * (inBuf[i + j + 1] + inBuf[i + FIRLen - j - 1]);
        }
        out[i] = (s + s2)/2;
    }
    memcpy(HPFBuffer.data(), in + (BlockSz - PrevBufSz),  PrevBufSz * sizeof(TFloat));
}


bool TTransientDetector::Detect(const TFloat* buf) {
    const uint16_t nBlocksToAnalize = NShortBlocks + 1;
    TFloat* rmsPerShortBlock = reinterpret_cast<TFloat*>(alloca(sizeof(TFloat) * nBlocksToAnalize));
    std::vector<TFloat> filtered(BlockSz);
    HPFilter(buf, filtered.data());
    bool trans = false;
    rmsPerShortBlock[0] = LastEnergy;
    for (uint16_t i = 1; i < nBlocksToAnalize; ++i) {
        rmsPerShortBlock[i] = 19.0 * log10(calculateRMS(&filtered[(size_t)(i - 1) * ShortSz], ShortSz));
        if (rmsPerShortBlock[i] - rmsPerShortBlock[i - 1] > 16) {
            trans = true;
            LastTransientPos = i;
        }
        if (rmsPerShortBlock[i - 1] - rmsPerShortBlock[i] > 20) {
            trans = true;
            LastTransientPos = i;
        }
    }
    LastEnergy = rmsPerShortBlock[NShortBlocks];
    return trans;
}

std::vector<TFloat> AnalyzeGain(const TFloat* in, const uint32_t len, const uint32_t maxPoints, bool useRms) {
    vector<TFloat> res;
    const uint32_t step = len / maxPoints;
    for (uint32_t pos = 0; pos < len; pos += step) {
        TFloat rms = useRms ? calculateRMS(in + pos, step) : calculatePeak(in + pos, step);
        res.emplace_back(rms);
    }
    return res;
}

} //namespace NAtracDEnc
