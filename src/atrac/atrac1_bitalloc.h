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
    std::vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks, const uint32_t bfuNum, const double spread, const double shift);
    const uint32_t BfuIdxConst;
    const bool FastBfuNumSearch;
    uint32_t GetMaxUsedBfuId(const std::vector<uint32_t>& bitsPerEachBlock);
    uint32_t CheckBfuUsage(bool* changed, uint32_t curBfuId, const std::vector<uint32_t>& bitsPerEachBlock);
public:
    explicit TAtrac1SimpleBitAlloc(TAea* container, uint32_t bfuIdxConst, bool fastBfuNumSearch)
        : TAtrac1BitStreamWriter(container)
        , BfuIdxConst(bfuIdxConst)
        , FastBfuNumSearch(fastBfuNumSearch)
    {}
    ~TAtrac1SimpleBitAlloc() {};
     uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks) override;
};

}
