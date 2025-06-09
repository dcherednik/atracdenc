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

static std::array<float, 128> SineWin128;
static std::array<float, 64> SineWin64;

static void InitSineWin() {
    if (SineWin128[SineWin128.size() - 1] == 0.0) {
        for (size_t i = 0; i < SineWin128.size(); i++) {
            SineWin128[i] = 2.0 * sinf((i + 0.5) * (M_PI / (2.0 * SineWin128.size())));
        }
    }
    if (SineWin64[SineWin64.size() - 1] == 0.0) {
        for (size_t i = 0; i < SineWin64.size(); i++) {
            SineWin64[i] = 2.0 * sinf((i + 0.5) * (M_PI / (2.0 * SineWin64.size())));
        }
    }
}

TAt3pMDCT::TAt3pMDCT()
{
    InitSineWin();
}

void TAt3pMDCT::Do(float specs[2048], const TPcmBandsData& bands, THistBuf& work, TAt3pMDCTWin winType)
{
    for (size_t b = 0, flag = 1; b < 16; b++, flag <<= 1) {
        const float* srcBuff = bands[b];
        float* const curSpec = &specs[b*128];

        std::array<float, 256>& tmp = work[b];

        if (winType.Flags & flag) {
            for (size_t i = 0; i < 64; i++) {
                tmp[128 + i] = srcBuff[i] * 2.0;
            }
            for (size_t i = 0; i < 64; i++) {
                tmp[160 + i] = SineWin64[63 - i] * srcBuff[32 + i];
            }
            memset(&tmp[224], 0, sizeof(float) * 32);
        } else {
            for (size_t i = 0; i < 128; i++) {
                tmp[128 + i] = SineWin128[127 - i] * srcBuff[i];
            }
        }

        const vector<float>& sp = Mdct(tmp.data());

        memcpy(curSpec, sp.data(), 128 * sizeof(float));

        if (b & 1) {
            SwapArray(curSpec, 128);
        }

        if (winType.Flags & flag) {
            memset(&tmp[0], 0, sizeof(float) * 32);
            for (size_t i = 0; i < 64; i++) {
                tmp[i + 32] = SineWin64[i] * srcBuff[i + 32];
            }
            for (size_t i = 0; i < 32; i++) {
                tmp[i + 96] = srcBuff[i + 96] * 2.0;
            }
        } else {
            for (size_t i = 0; i < 128; i++) {
                tmp[i] = SineWin128[i] * srcBuff[i];
            }
        }
    }
}

TAt3pMIDCT::TAt3pMIDCT()
{
    InitSineWin();
}

void TAt3pMIDCT::Do(float specs[2048], TPcmBandsData& bands, THistBuf& work, TAt3pMDCTWin winType)
{
    for (size_t b = 0, flag = 1; b < 16; b++, flag <<= 1) {
        float* dstBuff = bands[b];
        float* const curSpec = &specs[b*128];

        std::array<float, 128>& tmp = work.Buf[b];

        if (b & 1) {
            SwapArray(curSpec, 128);
        }

        vector<float> inv  = Midct(curSpec);

        if (work.Win.Flags & flag) {
            memset(&inv[0], 0, sizeof(float) * 32);
            for (size_t j = 0; j < 64; ++j) {
                inv[j + 32] *= SineWin64[j];
            }
            for (size_t j = 96; j < 128; j++) {
                inv[j] *= 2.0;
            }
        } else {
            for (size_t j = 0; j < 128; ++j) {
                inv[j] *= SineWin128[j];
            }
        }

        if (winType.Flags & flag) {
            for (size_t j = 128; j < 160; ++j) {
                inv[j] *= 2.0;
            }
            for (size_t j = 0; j < 64; ++j) {
                inv[223 - j] *= SineWin64[j];
            }
            memset(&inv[224], 0, sizeof(float) * 32);
        } else {
            for (size_t j = 0; j < 128; ++j) {
                inv[255 - j] *= SineWin128[j];
            }
        }

        for (uint32_t j = 0; j < 128; ++j) {
            dstBuff[j] = inv[j] + tmp[j];
        }

        memcpy(tmp.data(), &inv[128], sizeof(float)*128);
    }
    work.Win = winType;
}

};
