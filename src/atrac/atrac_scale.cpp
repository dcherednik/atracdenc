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
#include "atrac/at3p/at3p_tables.h"
#include "util.h"
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

float QuantMantisas(const float* in, const uint32_t first, const uint32_t last, const float mul, bool ea, int* const mantisas)
{
    float e1 = 0.0;
    float e2 = 0.0;

    const float inv2 = 1.0 / (mul * mul);

    if (!ea) {
        for (uint32_t j = 0, f = first; f < last; f++, j++) {
            float t = in[j] * mul;
            e1 += in[j] * in[j];
            mantisas[f] = ToInt(t);
            e2 += mantisas[f] * mantisas[f] * inv2;
        }
        return e1 / e2;
    }

    std::vector<std::pair<float, int>> candidates;
    candidates.reserve(last - first);

    for (uint32_t j = 0, f = first; f < last; f++, j++) {
        float t = in[j] * mul;
        e1 += in[j] * in[j];
        mantisas[f] = ToInt(t);
        e2 += mantisas[f] * mantisas[f] * inv2;

        float delta = t - (std::truncf(t) + 0.5f);
        // 0 ... 0.25 ... 0.5 ... 0.75 ... 1
        //        ^----------------^ candidates to be rounded to opposite side
        // to decrease overall energy error in the band
        if (std::abs(delta) < 0.25f) {
            candidates.push_back({delta, f});
        }
    }

    if (candidates.empty()) {
        return e1 / e2;
    }

    static auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
        return std::abs(a.first) < std::abs(b.first);
    };

    std::sort(candidates.begin(), candidates.end(), cmp);

    if (e2 < e1) {
        for (const auto& x : candidates) {
            auto f = x.second;
            auto j = f - first;
            auto t = in[j] * mul;
            if (static_cast<float>(std::abs(mantisas[f])) < std::abs(t) && static_cast<float>(std::abs(mantisas[f])) < (mul - 1)) {
                int m = mantisas[f];
                if (m > 0) m++;
                if (m < 0) m--;
                if (m == 0) m = t > 0 ? 1 : -1;
                auto ex = e2;
                ex -= mantisas[f] * mantisas[f] * inv2;
                ex += m * m * inv2;
                if (std::abs(ex - e1) < std::abs(e2 - e1)) {
                    mantisas[f] = m;
                    e2 = ex;
                }
            }
        }
        return e1 / e2;
    }

    if (e2 > e1) {
        for (const auto& x : candidates) {
            auto f = x.second;
            auto j = f - first;
            auto t = in[j] * mul;
            if (static_cast<float>(std::abs(mantisas[f])) > std::abs(t)) {
                auto m = mantisas[f];
                if (m > 0) m--;
                if (m < 0) m++;

                auto ex = e2;
                ex -= mantisas[f] * mantisas[f] * inv2;
                ex += m * m * inv2;

                if (std::abs(ex - e1) < std::abs(e2 - e1)) {
                    mantisas[f] = m;
                    e2 = ex;
                }
            }
        }
        return e1 / e2;
    }
    return e1 / e2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

template
class TScaler<NAt3p::TScaleTable>;

} //namespace NAtracDEnc
