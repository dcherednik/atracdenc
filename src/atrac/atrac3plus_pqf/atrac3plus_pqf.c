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
#include "atrac3plus_pqf_prototype.h"

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
#define EXTRA_SZ ((PROTO_SZ - SUBBANDS_NUM))

const uint16_t at3plus_pqf_frame_sz = FRAME_SZ;
const uint16_t at3plus_pqf_proto_sz = PROTO_SZ;

static float C[PROTO_SZ];
static float M[256];

struct at3plus_pqf_a_ctx {
    float buf[FRAME_SZ + EXTRA_SZ];
};

static void init(void)
{
    int i;
    int j;
    const float* const t = at3plus_pqf_prototype;
    static int inited = 0;

    if (inited)
        return;
    inited = 1;

    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            M[i*16 + j] = (float)cos(((2 * i + 1) * j & 127) * M_PI / 32);
        }
    }

    for (i = 0; i < 6; i++) {
        for (j = 0; j < 32; j++) {
            if (i & 1) {
                C[j * 12 + i] = 1.489 * t[j * 2 + 64 * i];
                C[j * 12 + 6 + i] = 1.489 * t[j * 2 + 1 + 64 * i];
            } else {
                C[j * 12 + i] = -1.489 * t[j * 2 + 64 * i];
                C[j * 12 + 6 + i] = -1.489 * t[j * 2 + 1 + 64 * i];
            }
        }
    }
}

static void vectoring(const float* x, float* y)
{
    int i;
    int j;
    const float* c = C;

    for ( i = 0; i < SUBBANDS_NUM * 2; i++, c += 12, x += 1, y += 1 ) {
        y[0] = 0;
        for (j = 0; j < 12; j++) {
            y[0] += c[j] * x[j * 32];
        }
    }
}

static void matrixing(const float* mi, const float* y, float* samples )
{
    int  i;
    for (i = 0; i < SUBBANDS_NUM; i++, mi += 16, samples += SUBBAND_SIZE) {
        samples[0] =          y[8]        + mi[ 1] * (y[7]+y[9])
                   + mi[ 2] * (y[6]+y[10]) + mi[ 3] * (y[5]+y[11])
                   + mi[ 4] * (y[4]+y[12]) + mi[ 5] * (y[3]+y[13])
                   + mi[ 6] * (y[2]+y[14]) + mi[ 7] * (y[ 1]+y[15])
                   + mi[ 8] * (y[ 0]+y[16])
                   + mi[15] * (y[23]-y[25]) + mi[14] * (y[22]-y[26])
                   + mi[13] * (y[21]-y[27]) + mi[12] * (y[20]-y[28])
                   + mi[11] * (y[19]-y[29]) + mi[10] * (y[18]-y[30])
                   + mi[ 9] * (y[17]-y[31]);
    }
}

static void a_init(at3plus_pqf_a_ctx_t ctx)
{
    float y[SUBBANDS_NUM * 2];
    float out[FRAME_SZ];
    float* x;
    int n, i;

    float* buf = ctx->buf;
    init();

    memcpy ( buf + FRAME_SZ, buf, EXTRA_SZ * sizeof(*buf) );
    x      = buf + FRAME_SZ;
 
    for ( n = 0; n < SUBBAND_SIZE; n++ ) {
        x -= SUBBANDS_NUM;

        for (i = 0; i < SUBBANDS_NUM; i++)
            x[i] = 0.0;

        vectoring(x, y);
        matrixing (M, y, &out[n]);
    }
}

at3plus_pqf_a_ctx_t at3plus_pqf_create_a_ctx()
{
    int i = 0;
    at3plus_pqf_a_ctx_t ctx = (at3plus_pqf_a_ctx_t)malloc(sizeof(struct at3plus_pqf_a_ctx));
    for (i = 0; i < FRAME_SZ + EXTRA_SZ; i++) {
        ctx->buf[i] = 0.0;
    }

    a_init(ctx);
    return ctx;
}

void at3plus_pqf_free_a_ctx(at3plus_pqf_a_ctx_t ctx)
{
    free(ctx);
}

void at3plus_pqf_do_analyse(at3plus_pqf_a_ctx_t ctx, const float* in, float* out)
{
    float y[SUBBANDS_NUM * 2];
    float* x;
    const float* pcm;
    int n, i;

    float* buf = ctx->buf;

    memcpy(buf + FRAME_SZ, buf, EXTRA_SZ * sizeof(float));
    x = buf + FRAME_SZ;

    pcm = in + (SUBBANDS_NUM - 1);

    for (n = 0; n < SUBBAND_SIZE; n++, pcm += SUBBANDS_NUM * 2) {
        x -= SUBBANDS_NUM;
        for (i = 0; i < SUBBANDS_NUM; i++) {
            x[i] = *pcm--;
        }
        vectoring(x, y);
        matrixing (M, y, &out[n]);
    }
}

const float* at3plus_pqf_get_proto(void)
{
    return at3plus_pqf_prototype;
}

