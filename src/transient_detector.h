#pragma once
#include <math.h>
#include <cstdint>
#include <vector>

namespace NAtracDEnc {
class TTransientDetector {
    const uint32_t ShortSz;
    const uint32_t BlockSz;
    const uint32_t NShortBlocks;
    static const uint32_t PrevBufSz = 20;
    static const uint32_t FIRLen = 21;
    void HPFilter(const double* in, double* out);
    std::vector<double> HPFBuffer;
    double LastEnergy = 0.0;
public:
    TTransientDetector(uint32_t shortSz, uint32_t blockSz)
        : ShortSz(shortSz)
        , BlockSz(blockSz)
        , NShortBlocks(blockSz/shortSz)
    {
        HPFBuffer.resize(BlockSz + FIRLen); 
    }
    bool Detect(const double* buf);
};
}
