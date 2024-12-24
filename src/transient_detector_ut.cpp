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

#include "transient_detector.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>

using std::vector;
using namespace NAtracDEnc;
TEST(AnalyzeGain, AnalyzeGainSimple) {

    float in[256];
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
    vector<float> res = AnalyzeGain(in, 256, 32, false);
    EXPECT_EQ(res.size(), 32);    

//    for (float v : res)
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
