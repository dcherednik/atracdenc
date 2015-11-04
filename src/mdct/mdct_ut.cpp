#include "mdct.h"
#include <gtest/gtest.h>

#include <vector>

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
