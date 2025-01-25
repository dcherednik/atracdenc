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

#include "at3p_mdct.h"
#include "util.h"

#include <array>

#include <iostream>
#include <vector>

using std::vector;

namespace NAtracDEnc {

static std::array<float, 128> SineWin;

static void InitSineWin() {
    if (SineWin[SineWin.size() - 1] == 0.0) {
        for (size_t i = 0; i < SineWin.size(); i++) {
            SineWin[i] = 2.0 * sinf((i + 0.5) * (M_PI / (2.0 * SineWin.size())));
        }
    }
}

TAt3pMDCT::TAt3pMDCT()
{
    InitSineWin();
}

void TAt3pMDCT::Do(float specs[2048], const TPcmBandsData& bands, THistBuf& work)
{
    for (size_t b = 0; b < 16; b++) {
        const float* srcBuff = bands[b];
        float* const curSpec = &specs[b*128];

        std::array<float, 256>& tmp = work[b];

        for (size_t i = 0; i < 128; i++) {
            tmp[128 + i] = SineWin[127 - i] * srcBuff[i];
        }

        const vector<float>& sp = Mdct(tmp.data());

        memcpy(curSpec, sp.data(), 128 * sizeof(float));

        if (b & 1) {
            SwapArray(curSpec, 128);
        }

        for (size_t i = 0; i < 128; i++) {
            tmp[i] = SineWin[i] * srcBuff[i];
        }
    }
}

TAt3pMIDCT::TAt3pMIDCT()
{
    InitSineWin();
}

void TAt3pMIDCT::Do(float specs[2048], TPcmBandsData& bands, THistBuf& work)
{
    for (size_t b = 0; b < 16; b++) {
        float* dstBuff = bands[b];
        float* const curSpec = &specs[b*128];

        std::array<float, 128>& tmp = work[b];

        if (b & 1) {
            SwapArray(curSpec, 128);
        }

        vector<float> inv  = Midct(curSpec);

        for (int j = 0; j < 128; ++j) {
            inv[j] *= SineWin[j];
            inv[255 - j] *= SineWin[j];
        }

        for (uint32_t j = 0; j < 128; ++j) {
            dstBuff[j] = inv[j] + tmp[j];
        }

        memcpy(tmp.data(), &inv[128], sizeof(float)*128);
    }
}

};
