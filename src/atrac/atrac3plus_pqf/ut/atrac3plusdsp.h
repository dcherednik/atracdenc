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

#ifndef ATRAC3PLUSDSP_H
#define ATRAC3PLUSDSP_H

#define ATRAC3P_SUBBANDS        16  ///< number of PQF subbands
#define ATRAC3P_SUBBAND_SAMPLES 128 ///< number of samples per subband
#define ATRAC3P_FRAME_SAMPLES   (ATRAC3P_SUBBAND_SAMPLES * ATRAC3P_SUBBANDS)

#define ATRAC3P_PQF_FIR_LEN 12

typedef struct Atrac3pIPQFChannelCtx {
    float buf1[ATRAC3P_PQF_FIR_LEN * 2][8];
    float buf2[ATRAC3P_PQF_FIR_LEN * 2][8];
    int pos;
} Atrac3pIPQFChannelCtx;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

extern const float ipqf_coeffs1[ATRAC3P_PQF_FIR_LEN][16];
extern const float ipqf_coeffs2[ATRAC3P_PQF_FIR_LEN][16];
void ff_atrac3p_ipqf(Atrac3pIPQFChannelCtx *hist, const float *in, float *out);

#ifdef __cplusplus
}
#endif /* __cplusplus */



#endif

