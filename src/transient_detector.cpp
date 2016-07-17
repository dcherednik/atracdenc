#include "transient_detector.h"
#include <stdlib.h>
#include <string.h>

namespace NAtracDEnc {

static double calculateRMS(const double* in, uint32_t n) {
    double s = 0;
    for (uint32_t i = 0; i < n; i++) {
        s += in[i] * in[i];
    }
    s /= n;
    return sqrt(s);
}

void TTransientDetector::HPFilter(const double* in, double* out) {
    static const double fircoef[] = {
        -8.65163e-18 * 2.0, -0.00851586 * 2.0, -6.74764e-18 * 2.0, 0.0209036 * 2.0,
        -3.36639e-17 * 2.0, -0.0438162 * 2.0, -1.54175e-17 * 2.0, 0.0931738 * 2.0,
        -5.52212e-17 * 2.0, -0.313819 * 2.0
    };
    memcpy(HPFBuffer.data() + PrevBufSz, in, BlockSz * sizeof(double));
    const double* inBuf = HPFBuffer.data();
    for (int i = 0; i < BlockSz; ++i) {
        double s = inBuf[i + 10];
        double s2 = 0;
        for (int j = 0; j < ((FIRLen - 1) / 2) - 1 ; j += 2) {
            s += fircoef[j] * (inBuf[i + j] + inBuf[i + FIRLen - j]);
            s2 += fircoef[j + 1] * (inBuf[i + j + 1] + inBuf[i + FIRLen - j - 1]);
        }
        out[i] = (s + s2)/2;
    }
    memcpy(HPFBuffer.data(), in + (BlockSz - PrevBufSz),  PrevBufSz * sizeof(double));
}


bool TTransientDetector::Detect(const double* buf) {
    const uint32_t nBlocksToAnalize = NShortBlocks + 1;
    double* rmsPerShortBlock = reinterpret_cast<double*>(alloca(sizeof(double) * nBlocksToAnalize));
    std::vector<double> filtered(BlockSz);
    HPFilter(buf, filtered.data());
    bool trans = false;
    rmsPerShortBlock[0] = LastEnergy;
    for (uint32_t i = 1; i < nBlocksToAnalize; ++i) {
        rmsPerShortBlock[i] = 19.0 * log10(calculateRMS(&filtered[(i - 1) * ShortSz], ShortSz));
        if (rmsPerShortBlock[i] - rmsPerShortBlock[i - 1] > 16) {
            trans = true;
        }
        if (rmsPerShortBlock[i - 1] - rmsPerShortBlock[i] > 20) {
            trans = true;
        }
    }
    LastEnergy = rmsPerShortBlock[NShortBlocks];
    return trans;
}

}
