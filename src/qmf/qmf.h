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
#include <string.h>

#include "../config.h"

class TQmfCommon {
protected:
    TQmfCommon() noexcept;
    static float QmfWindow[48];
public:
    // Compute the frequency response.
    static bool CalcFreqResp(size_t sz, float* buf) noexcept;
};

template <size_t nIn>
class TQmf : public TQmfCommon {
    static const float TapHalf[24];

    float PcmBuffer[nIn + 46];
    float PcmBufferMerge[nIn + 46];
public:
    TQmf() noexcept {
        for (size_t i = 0; i < sizeof(PcmBuffer)/sizeof(PcmBuffer[0]); i++) {
            PcmBuffer[i] = 0;
            PcmBufferMerge[i] = 0;
        }
    }

    void Analysis(const float* in, float* lower, float* upper) noexcept {
        float temp;

        memcpy(&PcmBuffer[0], &PcmBuffer[nIn], 46 * sizeof(float));

        memcpy(&PcmBuffer[46], in, nIn * sizeof(float));

        for (size_t j = 0; j < nIn; j+=2) {
            lower[j/2] = upper[j/2] = 0.0;
            for (size_t i = 0; i < 24; i++)  {
                lower[j/2] += QmfWindow[2*i] * PcmBuffer[48-1+j-(2*i)];
                upper[j/2] += QmfWindow[(2*i)+1] * PcmBuffer[48-1+j-(2*i)-1];
            }
            temp = upper[j/2];
            upper[j/2] = lower[j/2] - upper[j/2];
            lower[j/2] += temp;
        }
    }

    void Synthesis(float* out, const float* lower, const float* upper) noexcept {
        float* newPart = &PcmBufferMerge[46];
        for (size_t i = 0; i < nIn; i+=4) {
            newPart[i+0] = lower[i/2] + upper[i/2];
            newPart[i+1] = lower[i/2] - upper[i/2];
            newPart[i+2] = lower[i/2 + 1] + upper[i/2 + 1];
            newPart[i+3] = lower[i/2 + 1] - upper[i/2 + 1];
        }

        float* winP = &PcmBufferMerge[0];
        for (size_t j = nIn/2; j != 0; j--) {
            float s1 = 0;
            float s2 = 0;
            for (size_t i = 0; i < 48; i+=2) {
                s1 += winP[i] * QmfWindow[i];
                s2 += winP[i+1] * QmfWindow[i+1];
            }
            out[0] = s2;
            out[1] = s1;
            winP += 2;
            out += 2;
        }
        memcpy(&PcmBufferMerge[0], &PcmBufferMerge[nIn], 46 * sizeof(float));
    }
};

