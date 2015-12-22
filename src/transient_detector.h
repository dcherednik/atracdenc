#pragma once
#include <math.h>
#include <cstdint>
#include <vector>

namespace NAtracDEnc {
class TTransientDetector {
    const uint32_t ShortSz;
    const uint32_t BlockSz;
    const uint32_t NShortBlocks;
    static const uint32_t prevBufSz = 20;
    void HPFilter(const double* in, double* out);
    std::vector<double> HPFBuffer;
public:
    TTransientDetector(uint32_t shortSz, uint32_t blockSz)
        : ShortSz(shortSz)
        , BlockSz(blockSz)
        , NShortBlocks(blockSz/shortSz)
    {
        HPFBuffer.resize(BlockSz + prevBufSz); 
    }
    bool Detect(const double* buf);
};
}
