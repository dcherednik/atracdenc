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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once
#include <vector>
#include "../qmf/qmf.h"

namespace NAtracDEnc {

template<class TIn>
class Atrac3SplitFilterBank {
    const static int nInSamples = 1024;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    TQmf<TIn, nInSamples / 2> Qmf3;
    std::vector<TFloat> Buf1;
    std::vector<TFloat> Buf2;
public:
    Atrac3SplitFilterBank() {
        Buf1.resize(nInSamples);
        Buf2.resize(nInSamples);
    }
    void Split(TIn* pcm, TFloat* subs[4]) {
        Qmf1.Split(pcm, Buf1.data(), Buf2.data());
        Qmf2.Split(Buf1.data(), subs[0], subs[1]);
        Qmf3.Split(Buf2.data(), subs[3], subs[2]);
    }
};

} //namespace NAtracDEnc
