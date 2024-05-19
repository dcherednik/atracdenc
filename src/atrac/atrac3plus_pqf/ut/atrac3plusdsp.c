/*
 * ATRAC3+ compatible decoder
 *
 * Copyright (c) 2010-2013 Maxim Poliakovski
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 *  @file
 *  DSP functions for ATRAC3+ decoder.
 */

/**
 * This file was borrowed from FFmpeg to test analysis PQF implementation
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "atrac3plusdsp.h"

#include "../atrac3plus_pqf_data.h"

/* lookup table for fast modulo 23 op required for cyclic buffers of the IPQF */
static const int mod23_lut[26] = {
    23,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0
};

// Just for test
static void dct4(float* out, const float* x, int N, float scale) {
    for (int k = 0; k < N; k++) {
        double sum = 0;
        for (int n = 0; n < N; n++) {
            sum += x[n] * cos((M_PI/N) * ((double)n + 0.5) * ((double)k + 0.5));
        }
        out[N - 1 - k] = sum * scale;
    }
}

void ff_atrac3p_ipqf(Atrac3pIPQFChannelCtx *hist, const float *in, float *out)
{
    int i, s, sb, t, pos_now, pos_next;
    float idct_in[ATRAC3P_SUBBANDS];
    float idct_out[ATRAC3P_SUBBANDS];

    memset(out, 0, ATRAC3P_FRAME_SAMPLES * sizeof(*out));

    for (s = 0; s < ATRAC3P_SUBBAND_SAMPLES; s++) {
        /* pick up one sample from each subband */
        for (sb = 0; sb < ATRAC3P_SUBBANDS; sb++)
            idct_in[sb] = in[sb * ATRAC3P_SUBBAND_SAMPLES + s];

        dct4(idct_out, idct_in, ATRAC3P_SUBBANDS, 1.0/1024);

        /* append the result to the history */
        for (i = 0; i < 8; i++) {
            hist->buf1[hist->pos][i] = idct_out[i + 8];
            hist->buf2[hist->pos][i] = idct_out[7 - i];
        }

        pos_now  = hist->pos;
        pos_next = mod23_lut[pos_now + 2]; // pos_next = (pos_now + 1) % 23;

        for (t = 0; t < ATRAC3P_PQF_FIR_LEN; t++) {
            for (i = 0; i < 8; i++) {
                out[s * 16 + i + 0] += hist->buf1[pos_now][i]  * ff_ipqf_coeffs1[t][i] +
                                       hist->buf2[pos_next][i] * ff_ipqf_coeffs2[t][i];
                out[s * 16 + i + 8] += hist->buf1[pos_now][7 - i]  * ff_ipqf_coeffs1[t][i + 8] +
                                       hist->buf2[pos_next][7 - i] * ff_ipqf_coeffs2[t][i + 8];
            }
            pos_now  = mod23_lut[pos_next + 2]; // pos_now  = (pos_now  + 2) % 23;
            pos_next = mod23_lut[pos_now + 2];  // pos_next = (pos_next + 2) % 23;
        }

        hist->pos = mod23_lut[hist->pos]; // hist->pos = (hist->pos - 1) % 23;
    }
}
