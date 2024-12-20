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

#include "util.h"
#include <gtest/gtest.h>

#include <vector>

TEST(Util, SwapArrayTest) {
    float arr[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    SwapArray(arr, 8);
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_NEAR((float)i, arr[7-i], 0.000000000001);
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

TEST(Util, CalcEnergy) {

    EXPECT_NEAR((float)0.0, CalcEnergy(std::vector<float>{0.0}), 0.000000000001);
    EXPECT_NEAR((float)1.0, CalcEnergy(std::vector<float>{1.0}), 0.000000000001);
    EXPECT_NEAR((float)2.0, CalcEnergy(std::vector<float>{1.0, 1.0}), 0.000000000001);
    EXPECT_NEAR((float)5.0, CalcEnergy(std::vector<float>{2.0, 1.0}), 0.000000000001);
    EXPECT_NEAR((float)5.0, CalcEnergy(std::vector<float>{1.0, 2.0}), 0.000000000001);
    EXPECT_NEAR((float)8.0, CalcEnergy(std::vector<float>{2.0, 2.0}), 0.000000000001);
}
