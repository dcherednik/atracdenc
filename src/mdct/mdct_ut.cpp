#include "mdct.h"
#include <gtest/gtest.h>

#include <vector>
#include <cstdlib>

using std::vector;
using namespace NMDCT;

static vector<double> mdct(double* x, int N) {
    vector<double> res;
    for (int k = 0; k < N; k++) {
        double sum = 0;
        for (int n = 0; n < 2 * N; n++)
            sum += x[n]* cos((M_PI/N) * ((double)n + 0.5 + N/2) * ((double)k + 0.5));

        res.push_back(sum);
    }
    return res;
}  

static vector<double> midct(double* x, int N) {
    vector<double> res;
    for (int n = 0; n < 2 * N; n++) {
        double sum = 0;
        for (int k = 0; k < N; k++)
            sum += (x[k] * cos((M_PI/N) * ((double)n + 0.5 + N/2) * ((double)k + 0.5)));

        res.push_back(sum);
    }
    return res;
}


TEST(TBitStream, MDCT64) {
    const int N = 64;
    TMDCT<N> transform(N);
    vector<double> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<double> res1 = mdct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.0000000001);
    }
}

TEST(TBitStream, MDCT128) {
    const int N = 128;
    TMDCT<N> transform(N);
    vector<double> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<double> res1 = mdct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.0000000001);
    }
}

TEST(TBitStream, MDCT256) {
    const int N = 256;
    TMDCT<N> transform(N);
    vector<double> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = i;
    }
    const vector<double> res1 = mdct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.00000001);
    }
}

TEST(TBitStream, MDCT256_RAND) {
    const int N = 256;
    TMDCT<N> transform(N);
    vector<double> src(N);
    for (int i = 0; i < N; i++) {
        src[i] = rand();
    }
    const vector<double> res1 = mdct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < res1.size(); i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.01);
    }
}


TEST(TBitStream, MIDCT64) {
    const int N = 64;
    TMIDCT<N> transform(1);
    vector<double> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<double> res1 = midct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.0000000001);
    }
}

TEST(TBitStream, MIDCT128) {
    const int N = 128;
    TMIDCT<N> transform(1);
    vector<double> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<double> res1 = midct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.0000000001);
    }
}

TEST(TBitStream, MIDCT256) {
    const int N = 256;
    TMIDCT<N> transform(1);
    vector<double> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = i;
    }
    const vector<double> res1 = midct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.000000001);
    }
}

TEST(TBitStream, MIDCT256_RAND) {
    const int N = 256;
    TMIDCT<N> transform(1);
    vector<double> src(N);
    for (int i = 0; i < N/2; i++) {
        src[i] = rand();
    }
    const vector<double> res1 = midct(&src[0], N/2);
    const vector<double> res2 = transform(&src[0]);
    EXPECT_EQ(res1.size(), res2.size());
    for (int i = 0; i < N; i++) {
        EXPECT_NEAR(res1[i], res2[i], 0.01);
    }
}

