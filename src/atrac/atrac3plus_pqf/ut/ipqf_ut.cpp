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

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SAMPLES 8192

static int read_file(FILE* f, float buf[SAMPLES]) {
    size_t ret = fread(&buf[0], sizeof(buf[0]), SAMPLES, f);

    if (ret != SAMPLES) {
        return -1;
    }
    return 0;
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
        //fprintf(stdout, "%f %f\n", tmp[i], ref_data[i]);
        EXPECT_NEAR(tmp[i], ref_data[i], err);
    }
}
