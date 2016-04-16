#include "util.h"
#include <gtest/gtest.h>

#include <vector>


TEST(Util, SwapArrayTest) {

    double arr[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    SwapArray(arr, 8);
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_NEAR((double)i, arr[7-i], 0.000000000001);
    }

}
