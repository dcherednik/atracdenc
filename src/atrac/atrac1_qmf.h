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

#include "../qmf/qmf.h"

namespace NAtracDEnc {

template<class TIn>
class Atrac1AnalysisFilterBank {
    const static int nInSamples = 512;
    const static int delayComp = 39;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    std::vector<float> MidLowTmp;
    std::vector<float> DelayBuf;
public:
    Atrac1AnalysisFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(delayComp + 512);
    }
    void Analysis(TIn* pcm, float* low, float* mid, float* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(float) *  delayComp);
        Qmf1.Analysis(pcm, &MidLowTmp[0], &DelayBuf[delayComp]);
        Qmf2.Analysis(&MidLowTmp[0], low, mid);
        memcpy(hi, &DelayBuf[0], sizeof(float) * 256);

    }
};
template<class TOut>
class Atrac1SynthesisFilterBank {
    const static int nInSamples = 512;
    const static int delayComp = 39;
    TQmf<TOut, nInSamples> Qmf1;
    TQmf<TOut, nInSamples / 2> Qmf2;
    std::vector<float> MidLowTmp;
    std::vector<float> DelayBuf;
public:
    Atrac1SynthesisFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(delayComp + 512);
    }
    void Synthesis(TOut* pcm, float* low, float* mid, float* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(float) *  delayComp);
        memcpy(&DelayBuf[delayComp], hi, sizeof(float) * 256);
        Qmf2.Synthesis(&MidLowTmp[0], &low[0], &mid[0]);
        Qmf1.Synthesis(&pcm[0], &MidLowTmp[0], &DelayBuf[0]);
    }
};

} //namespace NAtracDEnc
