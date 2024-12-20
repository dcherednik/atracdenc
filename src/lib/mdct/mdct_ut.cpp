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
#include "mdct_ut_common.h"
#include <gtest/gtest.h>

#include <vector>
#include <cstdlib>

using std::vector;
using namespace NMDCT;

static vector<float> mdct(float* x, int N) {
    vector<float> res;
    for (int k = 0; k < N; k++) {
        float sum = 0;
        for (int n = 0; n < 2 * N; n++)
            sum += x[n]* cos((M_PI/N) * ((float)n + 0.5 + N/2) * ((float)k + 0.5));

        res.push_back(sum);
    }
    return res;
}

static vector<float> midct(float* x, int N) {
    vector<float> res;
    for (int n = 0; n < 2 * N; n++) {
        float sum = 0;
        for (int k = 0; k < N; k++)
            sum += (x[k] * cos((M_PI/N) * ((float)n + 0.5 + N/2) * ((float)k + 0.5)));

        res.push_back(sum);
    }
    return res;
}

TEST(TMdctTest, MDCT32) {
    const int N = 32;
    TMDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<float> res1 = mdct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N);
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MDCT64) {
    const int N = 64;
    TMDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<float> res1 = mdct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N);
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MDCT128) {
    const int N = 128;
    TMDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<float> res1 = mdct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N * 4);
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MDCT256) {
    const int N = 256;
    TMDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<float> res1 = mdct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N * 4);
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MDCT256_RAND) {
    const int N = 256;
    TMDCT<N> transform(N);
    vector<float> src(N);
    float m = 0.0;
    for (int i = 0; i < N; i++) {
        src[i] = rand();
        m = std::max(m, src[i]);
    }
    const vector<float> res1 = mdct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(m * 8);
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MIDCT32) {
    const int N = 32;
    TMIDCT<N> transform;
    vector<float> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<float> res1 = midct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N);
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MIDCT64) {
    const int N = 64;
    TMIDCT<N> transform;
    vector<float> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<float> res1 = midct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N);
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MIDCT128) {
    const int N = 128;
    TMIDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<float> res1 = midct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N);
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MIDCT256) {
    const int N = 256;
    TMIDCT<N> transform(N);
    vector<float> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<float> res1 = midct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(N * 2);
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}

TEST(TMdctTest, MIDCT256_RAND) {
    const int N = 256;
    TMIDCT<N> transform(N);
    vector<float> src(N);
    float m = 0.0;
    for (int i = 0; i < N/2; i++) {
        src[i] = rand();
        m = std::max(m, src[i]);
    }
    const vector<float> res1 = midct(&src[0], N/2);
    const vector<float> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    auto eps = CalcEps(m * 4);
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], eps);
    }
}
