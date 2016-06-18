#include "transient_detector.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>

using std::vector;
using namespace NAtracDEnc;
TEST(AnalyzeGain, AnalyzeGainSimple) {

    TFloat in[256];
    for (int i = 0; i < 256; ++i) {
        if (i <= 24) {
            in[i] = 1.0;
        } else if ( i > 24 && i <= 32) {
            in[i] = 8.0;
        } else if ( i > 32 && i <= 66) {
            in[i] = 128.0;
        } else {
            in[i] = 0.5;
        }
    }
    vector<TFloat> res = AnalyzeGain(in, 256, 32);
    EXPECT_EQ(res.size(), 32);    

//    for (TFloat v : res)
//        std::cout << v << std::endl;
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(res[i], 1.0);
    for (int i = 3; i < 4; ++i)
        EXPECT_EQ(res[i], 8.0);
    for (int i = 4; i < 9; ++i)
        EXPECT_EQ(res[i], 128.0);
    for (int i = 9; i < 32; ++i)
        EXPECT_EQ(res[i], 0.5);
}
