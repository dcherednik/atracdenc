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

#include "atrac1denc.h"
#include <lib/mdct/mdct_ut_common.h>
#include <gtest/gtest.h>

#include <vector>
using std::vector;
using namespace NAtracDEnc;
using namespace NAtrac1;

void CheckResult128(const vector<float>& a, const vector<float>& b) {
    float m = 0.0;
    for (int i = 0; i < a.size(); i++) {
        m = fmax(m, (float)a[i]);
    }

    auto eps = CalcEps(m);

    for (int i = 0; i < 96; ++i ) {
        EXPECT_NEAR(a[i], 4 * b[i+32], eps);
    }
}

void CheckResult256(const vector<float>& a, const vector<float>& b) {
    float m = 0.0;
    for (int i = 0; i < a.size(); i++) {
        m = fmax(m, (float)a[i]);
    }

    auto eps = CalcEps(m);

    for (int i = 0; i < 192; ++i ) {
        EXPECT_NEAR(a[i], 2 * b[i+32], eps);
    }
}


TEST(TAtrac1MDCT, TAtrac1MDCTLongEncDec) {
    TAtrac1MDCT mdct;
    vector<float> low(128 * 2);
    vector<float> mid(128 * 2);
    vector<float> hi(256 * 2);
    vector<float> specs(512 * 2);

    vector<float> lowRes(128 * 2);
    vector<float> midRes(128 * 2);
    vector<float> hiRes(256 * 2);
 
    for (int i = 0; i < 128; i++) {
        low[i] = mid[i] = i;
    }

    for (int i = 0; i < 256; i++) {
        hi[i] = i;
    }
    const TAtrac1Data::TBlockSizeMod blockSize(false, false, false);

    mdct.Mdct(&specs[0], &low[0], &mid[0], &hi[0], blockSize);

    mdct.IMdct(&specs[0], blockSize, &lowRes[0], &midRes[0], &hiRes[0]);

    CheckResult128(low, lowRes);
    CheckResult128(mid, midRes);
    CheckResult256(hi, hiRes);
}

TEST(TAtrac1MDCT, TAtrac1MDCTShortEncDec) {
    TAtrac1MDCT mdct;
    vector<float> low(128 * 2);
    vector<float> mid(128 * 2);
    vector<float> hi(256 * 2);
    vector<float> specs(512 * 2);

    vector<float> lowRes(128 * 2);
    vector<float> midRes(128 * 2);
    vector<float> hiRes(256 * 2);
 
    for (int i = 0; i < 128; i++) {
        low[i] = mid[i] = i;
    }
    const vector<float> lowCopy = low; //in case of short wondow AtracMDCT changed input buffer during calculation
    const vector<float> midCopy = mid;

    for (int i = 0; i < 256; i++) {
        hi[i] = i;
    }
    const vector<float> hiCopy = hi;

    const TAtrac1Data::TBlockSizeMod blockSize(true, true, true); //short

    mdct.Mdct(&specs[0], &low[0], &mid[0], &hi[0], blockSize);

    mdct.IMdct(&specs[0], blockSize, &lowRes[0], &midRes[0], &hiRes[0]);

    CheckResult128(lowCopy, lowRes);
    CheckResult128(midCopy, midRes);
    CheckResult256(hiCopy, hiRes);
}

