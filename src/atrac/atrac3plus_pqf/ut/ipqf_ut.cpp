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

#include "atrac3plusdsp.h"
#include "../atrac3plus_pqf.h"
#include "../atrac3plus_pqf_data.h"

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cstdlib>

#define SAMPLES 8192

static int read_file(FILE* f, float buf[SAMPLES]) {
    size_t ret = fread(&buf[0], sizeof(buf[0]), SAMPLES, f);

    if (ret != SAMPLES) {
        return -1;
    }
    return 0;
}

static void create_chirp(int sz, float* buf) {
    int i = 0;
    for(i = 0; i < sz; i++) {
        float t = i;
        buf[i] = sinf((t + t * t * 0.5 / 2.0) * 2.0 * M_PI/(float)sz);
    }
}

TEST(ipqf, CheckOnRefData) {
    FILE* mr_f = fopen("test_data/ipqftest_pcm_mr.dat", "r");
    if (!mr_f) {
        fprintf(stderr, "unable to open multirate file\n");
	FAIL();
    }

    FILE* ref_f = fopen("test_data/ipqftest_pcm_out.dat", "r");
    if (!ref_f) {
        fclose(mr_f);
        fprintf(stderr, "unable to open reference file\n");
	FAIL();
    }

    float mr_data[SAMPLES] = {0};
    float ref_data[SAMPLES] = {0};

    if (read_file(mr_f, mr_data) < 0) {
        fprintf(stderr, "unable to read multirate file\n");
	FAIL();
    }

    if (read_file(ref_f, ref_data) < 0) {
        fprintf(stderr, "unable to read reference file\n");
	FAIL();
    }

    Atrac3pIPQFChannelCtx ctx;
    memset(&ctx, 0, sizeof(ctx));

    float tmp[SAMPLES] = {0};

    for (int i = 0; i < SAMPLES; i+= 2048) {
        ff_atrac3p_ipqf(&ctx, &mr_data[i], &tmp[i]);
    }

    const static float err = 1.0 / (float)(1<<26);
    for (int i = 0; i < SAMPLES; i++) {
        EXPECT_NEAR(tmp[i], ref_data[i], err);
    }
}

TEST(ipqf, CmpEnergy) {
   double e1 = 0.0;
   double e2 = 0.0;
   double e = 0.0;
   const static double err = 1.0 / (double)(1ull<<32);
   for (int i = 0; i < ATRAC3P_SUBBANDS; i++) {
       e1 = 0.0;
       e2 = 0.0;
       for (int j = 0; j < ATRAC3P_PQF_FIR_LEN; j++) {
           e1 += ff_ipqf_coeffs1[j][i] * ff_ipqf_coeffs1[j][i];
           e2 += ff_ipqf_coeffs2[j][i] * ff_ipqf_coeffs2[j][i];
       }
       if (i) {
           EXPECT_NEAR(e, e1 + e2, err);
       }
       e = e1 + e2;
   }
}

TEST(pqf, DC_Short) {
    int i = 0;
    float x[2048] = {0};
    float subbands[2048] = {0};
    for (i = 0; i < 2048; i++)
        x[i] = 1;

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);

    float tmp[2048] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));


    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);

    const static float err = 1.0 / (float)(1<<21);

    for (int i = 368; i < 2048; i++) {
        EXPECT_NEAR(tmp[i], x[i], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, DC_Long) {
    int i = 0;
    float x[4096] = {0};
    float subbands[4096] = {0};
    for (i = 0; i < 4096; i++)
        x[i] = 1;

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);
    at3plus_pqf_do_analyse(actx, x + 2048, subbands + 2048);

    float tmp[4096] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));

    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);
    ff_atrac3p_ipqf(&sctx, &subbands[2048], &tmp[2048]);

    const static float err = 1.0 / (float)(1<<21);

    for (int i = 368; i < 4096; i++) {
        EXPECT_NEAR(tmp[i], x[i-368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, Seq_Short) {
    int i = 0;
    float x[2048] = {0};
    float subbands[2048] = {0};

    for (i = 0; i < 2048; i++)
        x[i] = i;

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);

    float tmp[2048] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));


    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);

    const static float err = 2048.0 / (float)(1<<22);

    for (int i = 368; i < 2048; i++) {
        EXPECT_NEAR(tmp[i], x[i - 368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, Seq_Long) {
    int i = 0;
    float x[4096] = {0};
    float subbands[4096] = {0};
    for (i = 0; i < 4096; i++)
        x[i] = i;

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);
    at3plus_pqf_do_analyse(actx, x + 2048, subbands + 2048);

    float tmp[4096] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));

    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);
    ff_atrac3p_ipqf(&sctx, &subbands[2048], &tmp[2048]);

    const static float err = 4096.0 / (float)(1<<21);
    for (int i = 368; i < 4096; i++) {
        EXPECT_NEAR(tmp[i], x[i-368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, Chirp_Short) {
    int i = 0;
    float x[2048] = {0};
    float subbands[2048] = {0};

    create_chirp(2048, x);

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);

    float tmp[2048] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));

    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);

    const static float err = 1.0 / (float)(1<<21);

    for (int i = 368; i < 2048; i++) {
        EXPECT_NEAR(tmp[i], x[i - 368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, Chirp_Long) {
    int i = 0;
    float x[4096] = {0};
    float subbands[4096] = {0};

    create_chirp(4096, x);

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);
    at3plus_pqf_do_analyse(actx, x + 2048, subbands + 2048);

    float tmp[4096] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));

    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);
    ff_atrac3p_ipqf(&sctx, &subbands[2048], &tmp[2048]);

    const static float err = 1.0 / (float)(1<<21);
    for (int i = 368; i < 4096; i++) {
        EXPECT_NEAR(tmp[i], x[i-368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}

TEST(pqf, Noise_Long) {
    int i = 0;
    float x[4096] = {0};
    float subbands[4096] = {0};
    for (i = 0; i < 4096; i++)
        x[i] = (float)rand() / (float)RAND_MAX - 0.5;

    at3plus_pqf_a_ctx_t actx = at3plus_pqf_create_a_ctx();

    at3plus_pqf_do_analyse(actx, x, subbands);
    at3plus_pqf_do_analyse(actx, x + 2048, subbands + 2048);

    float tmp[4096] = {0};

    Atrac3pIPQFChannelCtx sctx;
    memset(&sctx, 0, sizeof(sctx));

    ff_atrac3p_ipqf(&sctx, &subbands[0], &tmp[0]);
    ff_atrac3p_ipqf(&sctx, &subbands[2048], &tmp[2048]);

    const static float err = 1.0 / (float)(1<<21);
    for (int i = 368; i < 4096; i++) {
        EXPECT_NEAR(tmp[i], x[i-368], err);
    }

    at3plus_pqf_free_a_ctx(actx);
}


