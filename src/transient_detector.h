#pragma once
#include <math.h>
#include <cstdint>
#include <vector>

#include "config.h"

namespace NAtracDEnc {

class TTransientDetector {
    const uint32_t ShortSz;
    const uint32_t BlockSz;
    const uint32_t NShortBlocks;
    static const uint32_t PrevBufSz = 20;
    static const uint32_t FIRLen = 21;
    void HPFilter(const TFloat* in, TFloat* out);
    std::vector<TFloat> HPFBuffer;
    TFloat LastEnergy = 0.0;
    uint32_t LastTransientPos = 0;
public:
    TTransientDetector(uint32_t shortSz, uint32_t blockSz)
        : ShortSz(shortSz)
        , BlockSz(blockSz)
        , NShortBlocks(blockSz/shortSz)
    {
        HPFBuffer.resize(BlockSz + FIRLen); 
    }
    bool Detect(const TFloat* buf);
    uint32_t GetLastTransientPos() const { return LastTransientPos; }
};

std::vector<TFloat> AnalyzeGain(const TFloat* in, uint32_t len, uint32_t maxPoints, bool useRms);

}
