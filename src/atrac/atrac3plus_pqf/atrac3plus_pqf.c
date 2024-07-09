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

/*
 * Musepack pqf implementation was used as reference.
 * see svn.musepack.net
 */

#include <stdlib.h>

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "atrac3plus_pqf.h"
#include "atrac3plus_pqf_data.h"

#include "lib/mdct/dct.h"

/*
 * Number of subbands to split input signal
 */
#define SUBBANDS_NUM 16
/*
 * Number of samples in each subband
 */
#define SUBBAND_SIZE 128
/*
 * Size of filter prototype
 */
#define PROTO_SZ 384

#define FRAME_SZ ((SUBBANDS_NUM * SUBBAND_SIZE))
#define OVERLAP_SZ ((PROTO_SZ - SUBBANDS_NUM))


static float fir[PROTO_SZ];

struct at3plus_pqf_a_ctx {
    float buf[FRAME_SZ + OVERLAP_SZ];
    atde_dct_ctx_t dct_ctx;
};

static void init(void)
{
    static int inited = 0;

    if (inited)
        return;

    inited = 1;

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < ATRAC3P_PQF_FIR_LEN; j++) {
	    if (i >= 8) {
	        fir[j + 96  + (i - 8) * 12] = ff_ipqf_coeffs1[j][i];
	        fir[j + 288 + (i - 8) * 12] = ff_ipqf_coeffs2[j][i];
	    } else {
	        fir[j + 192 + i * 12] = ff_ipqf_coeffs2[j][i];
	        fir[j + 0   + i * 12] = ff_ipqf_coeffs1[j][i];
	    }
        }
    }
}

static void vectoring(const float* const x, double* y)
{
    for (int i = 0; i < 32; i++) {
        y[i] = 0;
        for (int j = 0; j < ATRAC3P_PQF_FIR_LEN; j++) {
            y[i] += fir[i * 12 + j] * x[j * 32 + i];
        }
    }
}

static void matrixing(atde_dct_ctx_t ctx, const double* y, float* samples )
{
    float yy[SUBBANDS_NUM];
    float res[SUBBANDS_NUM];

    for (int i = 0; i < 8; i++) {
        yy[i] = y[i + 8] + y[7 - i];
        yy[i + 8] = y[i + 16] + y[31 - i];
    }

    atde_do_dct4_16(ctx, yy, res);

    for (int i = 0; i < SUBBANDS_NUM; i++) {
        samples[i * SUBBAND_SIZE] = res[SUBBANDS_NUM - 1 - i];
    }
}

at3plus_pqf_a_ctx_t at3plus_pqf_create_a_ctx()
{
    at3plus_pqf_a_ctx_t ctx = (at3plus_pqf_a_ctx_t)malloc(sizeof(struct at3plus_pqf_a_ctx));

    for (int i = 0; i < FRAME_SZ + OVERLAP_SZ; i++) {
        ctx->buf[i] = 0.0;
    }

    ctx->dct_ctx = atde_create_dct4_16(128 * 512.0);

    init();

    return ctx;
}

void at3plus_pqf_free_a_ctx(at3plus_pqf_a_ctx_t ctx)
{
    atde_free_dct_ctx(ctx->dct_ctx);

    free(ctx);
}

void at3plus_pqf_do_analyse(at3plus_pqf_a_ctx_t ctx, const float* in, float* out)
{
    double y[SUBBANDS_NUM * 2];

    float* const buf = ctx->buf;

    const float* x = buf;

    memcpy(buf + OVERLAP_SZ, in, sizeof(in[0]) * FRAME_SZ);

    for (int i = 0; i < SUBBAND_SIZE; i++) {
        vectoring(x, y);
        matrixing (ctx->dct_ctx, y, &out[i]);
        x += SUBBANDS_NUM;
    }

    memcpy(buf, buf + FRAME_SZ, sizeof(buf[0]) * OVERLAP_SZ);
}
