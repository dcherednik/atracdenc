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

#include "mdct.h"
#include <iostream>

namespace NMDCT {

static std::vector<TFloat> CalcSinCos(size_t n, TFloat scale)
{
    std::vector<TFloat> tmp(n >> 1);
    const TFloat alpha = 2.0 * M_PI / (8.0 * n);
    const TFloat omiga = 2.0 * M_PI / n;
    scale = sqrt(scale/n); 
    for (size_t i = 0; i < (n >> 2); ++i) {
        tmp[2 * i + 0] = scale * cos(omiga * i + alpha);
        tmp[2 * i + 1] = scale * sin(omiga * i + alpha);
    }
    return tmp;
}

TMDCTBase::TMDCTBase(size_t n, TFloat scale)
    : N(n)
    , SinCos(CalcSinCos(n, scale))
{
    FFTIn = (kiss_fft_cpx*) malloc(sizeof(kiss_fft_cpx) * N >> 2);
    FFTOut = (kiss_fft_cpx*) malloc(sizeof(kiss_fft_cpx) * N >> 2);
    FFTPlan = kiss_fft_alloc(N >> 2, false, nullptr, nullptr);
}

TMDCTBase::~TMDCTBase()
{
    kiss_fft_free(FFTPlan);
    free(FFTOut);
    free(FFTIn);
}

} // namespace NMDCT
