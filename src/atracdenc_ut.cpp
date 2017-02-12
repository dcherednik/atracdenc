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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "atrac1denc.h"
#include <gtest/gtest.h>

#include <vector>
using std::vector;
using namespace NAtracDEnc;

void CheckResult128(const vector<TFloat>& a, const vector<TFloat>& b) {
    for (int i = 0; i < 96; ++i ) {
        EXPECT_NEAR(a[i], 4 * b[i+32], 0.0000001);
    }
}
void CheckResult256(const vector<TFloat>& a, const vector<TFloat>& b) {
    for (int i = 0; i < 192; ++i ) {
        EXPECT_NEAR(a[i], 2 * b[i+32], 0.0000001);
    }
}


TEST(TAtrac1MDCT, TAtrac1MDCTLongEncDec) {
    TAtrac1MDCT mdct;
    vector<TFloat> low(128 * 2);
    vector<TFloat> mid(128 * 2);
    vector<TFloat> hi(256 * 2);
    vector<TFloat> specs(512 * 2);

    vector<TFloat> lowRes(128 * 2);
    vector<TFloat> midRes(128 * 2);
    vector<TFloat> hiRes(256 * 2);
 
    for (int i = 0; i < 128; i++) {
        low[i] = mid[i] = i;
    }

    for (int i = 0; i < 256; i++) {
        hi[i] = i;
    }
    const TBlockSize blockSize(false, false, false);

    mdct.Mdct(&specs[0], &low[0], &mid[0], &hi[0], blockSize);

    mdct.IMdct(&specs[0], blockSize, &lowRes[0], &midRes[0], &hiRes[0]);

    CheckResult128(low, lowRes);
    CheckResult128(mid, midRes);
    CheckResult256(hi, hiRes);
}

TEST(TAtrac1MDCT, TAtrac1MDCTShortEncDec) {
    TAtrac1MDCT mdct;
    vector<TFloat> low(128 * 2);
    vector<TFloat> mid(128 * 2);
    vector<TFloat> hi(256 * 2);
    vector<TFloat> specs(512 * 2);

    vector<TFloat> lowRes(128 * 2);
    vector<TFloat> midRes(128 * 2);
    vector<TFloat> hiRes(256 * 2);
 
    for (int i = 0; i < 128; i++) {
        low[i] = mid[i] = i;
    }
    const vector<TFloat> lowCopy = low; //in case of short wondow AtracMDCT changed input buffer during calculation
    const vector<TFloat> midCopy = mid;

    for (int i = 0; i < 256; i++) {
        hi[i] = i;
    }
    const vector<TFloat> hiCopy = hi;

    const TBlockSize blockSize(true, true, true); //short

    mdct.Mdct(&specs[0], &low[0], &mid[0], &hi[0], blockSize);

    mdct.IMdct(&specs[0], blockSize, &lowRes[0], &midRes[0], &hiRes[0]);

    CheckResult128(lowCopy, lowRes);
    CheckResult128(midCopy, midRes);
    CheckResult256(hiCopy, hiRes);
}

