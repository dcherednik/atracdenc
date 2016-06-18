#include "util.h"
#include <gtest/gtest.h>

#include <vector>


TEST(Util, SwapArrayTest) {

    TFloat arr[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    SwapArray(arr, 8);
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_NEAR((TFloat)i, arr[7-i], 0.000000000001);
    }
}

TEST(Util, GetFirstSetBitTest) {
    EXPECT_EQ(1, GetFirstSetBit(2));
    EXPECT_EQ(1, GetFirstSetBit(3));
    EXPECT_EQ(2, GetFirstSetBit(4));
    EXPECT_EQ(2, GetFirstSetBit(5));
    EXPECT_EQ(2, GetFirstSetBit(6));
    EXPECT_EQ(2, GetFirstSetBit(7));
    EXPECT_EQ(3, GetFirstSetBit(8));
    EXPECT_EQ(3, GetFirstSetBit(9));
    EXPECT_EQ(3, GetFirstSetBit(10));
}
