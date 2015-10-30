#pragma once
#include "atrac1_scale.h"
#include "../bitstream/bitstream.h"
#include "../aea.h"
#include "../atrac/atrac1.h"
#include <vector>
#include <cstdint>

namespace NAtrac1 {

class IAtrac1BitAlloc {
public:
    IAtrac1BitAlloc() {};
    virtual ~IAtrac1BitAlloc() {};
    virtual uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks) = 0;
};

class TAtrac1BitStreamWriter : public TAtrac1Data {
    TAea* Container;
public:
    explicit TAtrac1BitStreamWriter(TAea* container)
        : Container(container)
    {};
    void WriteBitStream(const std::vector<uint32_t>& bitsPerEachBlock, const std::vector<TScaledBlock>& scaledBlocks, uint32_t bfuAmountIdx);
};

class TAtrac1SimpleBitAlloc : public TAtrac1BitStreamWriter, public IAtrac1BitAlloc {
    std::vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks, const double spread, const double shift);
public:
    using TAtrac1BitStreamWriter::TAtrac1BitStreamWriter;
    ~TAtrac1SimpleBitAlloc() {};
     uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks) override;
};

}
