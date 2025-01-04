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

#include "atrac_scale.h"
#include "atrac1.h"
#include "atrac3.h"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace NAtracDEnc {

using std::vector;
using std::map;

using std::cerr;
using std::endl;

using std::abs;

static const float MAX_SCALE = 1.0;

template<class TBaseData>
TScaler<TBaseData>::TScaler() {
    for (int i = 0; i < 64; i++) {
        ScaleIndex[TBaseData::ScaleTable[i]] = i;
    }
}

template<class TBaseData>
TScaledBlock TScaler<TBaseData>::Scale(const float* in, uint16_t len) {
    float maxAbsSpec = 0;
    for (uint16_t i = 0; i < len; ++i) {
        const float absSpec = abs(in[i]);
        if (absSpec > maxAbsSpec) {
            maxAbsSpec = absSpec;
        }
    }
    if (maxAbsSpec > MAX_SCALE) {
        cerr << "Scale error: absSpec > MAX_SCALE, val: " << maxAbsSpec << endl;
        maxAbsSpec = MAX_SCALE;
    }
    const map<float, uint8_t>::const_iterator scaleIter = ScaleIndex.lower_bound(maxAbsSpec);
    const float scaleFactor = scaleIter->first;
    const uint8_t scaleFactorIndex = scaleIter->second;
    TScaledBlock res(scaleFactorIndex);
    float maxEnergy = 0.0;
    for (uint16_t i = 0; i < len; ++i) {
        float scaledValue = in[i] / scaleFactor;
        float energy = in[i] * in[i];
        maxEnergy = std::max(maxEnergy, energy);
        if (abs(scaledValue) >= 1.0) {
            if (abs(scaledValue) > 1.0) {
                cerr << "clipping, scaled value: "<< scaledValue << endl;
            }
            scaledValue = (scaledValue > 0) ? 0.99999 : -0.99999;
        }
        res.Values.push_back(scaledValue);
    }
    res.MaxEnergy = maxEnergy;
    return res;
}

template<class TBaseData>
vector<TScaledBlock> TScaler<TBaseData>::ScaleFrame(const vector<float>& specs, const typename TBaseData::TBlockSizeMod& blockSize) {
    vector<TScaledBlock> scaledBlocks;
    scaledBlocks.reserve(TBaseData::MaxBfus);
    for (uint8_t bandNum = 0; bandNum < TBaseData::NumQMF; ++bandNum) {
        const bool shortWinMode = blockSize.ShortWin(bandNum);

        for (uint8_t blockNum = TBaseData::BlocksPerBand[bandNum]; blockNum < TBaseData::BlocksPerBand[bandNum + 1]; ++blockNum) {
            const uint16_t specNumStart = shortWinMode ? TBaseData::SpecsStartShort[blockNum] :
                                                         TBaseData::SpecsStartLong[blockNum];
            scaledBlocks.emplace_back(Scale(&specs[specNumStart], TBaseData::SpecsPerBlock[blockNum]));
        }
    }
    return scaledBlocks;
}

template
class TScaler<NAtrac1::TAtrac1Data>;

template
class TScaler<NAtrac3::TAtrac3Data>;

} //namespace NAtracDEnc
