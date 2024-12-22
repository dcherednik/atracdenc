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

#include "mdct.h"
#include "dct.h"
#include <iostream>

namespace NMDCT {

static std::vector<float> CalcSinCos(size_t n, float scale)
{
    std::vector<float> tmp(n >> 1);
    const float alpha = 2.0 * M_PI / (8.0 * n);
    const float omiga = 2.0 * M_PI / n;
    scale = sqrt(scale/n); 
    for (size_t i = 0; i < (n >> 2); ++i) {
        tmp[2 * i + 0] = scale * cos(omiga * i + alpha);
        tmp[2 * i + 1] = scale * sin(omiga * i + alpha);
    }
    return tmp;
}

TMDCTBase::TMDCTBase(size_t n, float scale)
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

struct atde_dct_ctx {
    atde_dct_ctx(float scale)
        : mdct(scale)
    {}
    NMDCT::TMIDCT<32, float> mdct;
};

atde_dct_ctx_t atde_create_dct4_16(float scale)
{
    return new atde_dct_ctx(32.0 * scale);
}

void atde_free_dct_ctx(atde_dct_ctx_t ctx)
{
    delete ctx;
}

void atde_do_dct4_16(atde_dct_ctx_t ctx, const float* in, float* out)
{
    //TODO: rewrire more optimal
    const auto& x = ctx->mdct(in);

    for (int i = 0; i < 16; i++) {
        out[i] = x[i + 8] * -1.0;
    }
}

