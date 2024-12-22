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
#include <vector>
#include "../qmf/qmf.h"

namespace NAtracDEnc {

template<class TIn>
class Atrac3AnalysisFilterBank {
    const static int nInSamples = 1024;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    TQmf<TIn, nInSamples / 2> Qmf3;
    std::vector<float> Buf1;
    std::vector<float> Buf2;
public:
    Atrac3AnalysisFilterBank() {
        Buf1.resize(nInSamples);
        Buf2.resize(nInSamples);
    }
    void Analysis(TIn* pcm, float* subs[4]) {
        Qmf1.Analysis(pcm, Buf1.data(), Buf2.data());
        Qmf2.Analysis(Buf1.data(), subs[0], subs[1]);
        Qmf3.Analysis(Buf2.data(), subs[3], subs[2]);
    }
};

} //namespace NAtracDEnc
