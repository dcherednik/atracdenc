#include "atracdenc.h"
#include <gtest/gtest.h>

#include <vector>
using std::vector;
using namespace NAtracDEnc;

void CheckResult128(const vector<double>& a, const vector<double>& b) {
    for (int i = 0; i < 96; ++i ) {
        EXPECT_NEAR(a[i], 4 * b[i+32], 0.0000001);
    }
}
void CheckResult256(const vector<double>& a, const vector<double>& b) {
    for (int i = 0; i < 192; ++i ) {
        EXPECT_NEAR(a[i], 2 * b[i+32], 0.0000001);
    }
}


TEST(TAtrac1MDCT, TAtrac1MDCTLongEncDec) {
    TAtrac1MDCT mdct;
    vector<double> low(128 * 2);
    vector<double> mid(128 * 2);
    vector<double> hi(256 * 2);
    vector<double> specs(512 * 2);

    vector<double> lowRes(128 * 2);
    vector<double> midRes(128 * 2);
    vector<double> hiRes(256 * 2);
 
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
    vector<double> low(128 * 2);
    vector<double> mid(128 * 2);
    vector<double> hi(256 * 2);
    vector<double> specs(512 * 2);

    vector<double> lowRes(128 * 2);
    vector<double> midRes(128 * 2);
    vector<double> hiRes(256 * 2);
 
    for (int i = 0; i < 128; i++) {
        low[i] = mid[i] = i;
    }
    const vector<double> lowCopy = low; //in case of short wondow AtracMDCT changed input buffer during calculation
    const vector<double> midCopy = mid;

    for (int i = 0; i < 256; i++) {
        hi[i] = i;
    }
    const vector<double> hiCopy = hi;

    const TBlockSize blockSize(true, true, true); //short

    mdct.Mdct(&specs[0], &low[0], &mid[0], &hi[0], blockSize);

    mdct.IMdct(&specs[0], blockSize, &lowRes[0], &midRes[0], &hiRes[0]);

    CheckResult128(lowCopy, lowRes);
    CheckResult128(midCopy, midRes);
    CheckResult256(hiCopy, hiRes);
}

