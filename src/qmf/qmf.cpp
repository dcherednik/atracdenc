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

#include "qmf.h"

#include <lib/fft/kissfft_impl/tools/kiss_fftr.h>

#include <iostream>

static const float TapHalf[24] = {
    -0.00001461907,  -0.00009205479, -0.000056157569,  0.00030117269,
    0.0002422519,    -0.00085293897, -0.0005205574,    0.0020340169,
    0.00078333891,   -0.0042153862,  -0.00075614988,   0.0078402944,
    -0.000061169922, -0.01344162,    0.0024626821,     0.021736089,
    -0.007801671,    -0.034090221,   0.01880949,       0.054326009,
    -0.043596379,    -0.099384367,   0.13207909,       0.46424159
};

float TQmfCommon::QmfWindow[48];

TQmfCommon::TQmfCommon() noexcept
{
    if (QmfWindow[0]) {
        return;
    }
    const int sz = sizeof(QmfWindow)/sizeof(QmfWindow[0]);
    for (size_t i = 0 ; i < sz/2; i++) {
        QmfWindow[i] = QmfWindow[ sz - 1 - i] = TapHalf[i] * 2.0;
    }
}

bool TQmfCommon::CalcFreqResp(size_t sz, float* buf) noexcept
{
    size_t fftSz = sz * 2;
    if (fftSz < 48) {
        return false;
    }

    kiss_fftr_cfg fftCtx = kiss_fftr_alloc(fftSz, 0, NULL, NULL);
    if (!fftCtx) {
        return false;
    }

    kiss_fft_scalar* input = (kiss_fft_scalar*)KISS_FFT_MALLOC(sizeof(kiss_fft_scalar) * fftSz);
    if (!input) {
        kiss_fftr_free(fftCtx);
        return false;
    }

    memset(input, 0, (sizeof(kiss_fft_scalar) * fftSz));

    const size_t start = (sz - 48) / 2;
    for (size_t i = start, j = 0; j < 48; i++, j++) {
        input[i] = QmfWindow[j] / 2.0;
    }

    kiss_fft_cpx* res = (kiss_fft_cpx*)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * (sz + 1));
    if (!res) {
        kiss_fftr_free(input);
        kiss_fftr_free(fftCtx);
        return false;
    }

    kiss_fftr(fftCtx, input, res);

    for (size_t i = 0; i < sz; i++) {
        buf[i] = res[i].r * res[i].r + res[i].i * res[i].i;
    }

    kiss_fftr_free(res);
    kiss_fftr_free(input);
    kiss_fftr_free(fftCtx);

    return true;
}
