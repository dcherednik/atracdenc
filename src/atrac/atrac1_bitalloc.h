#pragma once
#include "atrac_scale.h"
#include "../bitstream/bitstream.h"
#include "../aea.h"
#include "../atrac/atrac1.h"
#include <vector>
#include <map>
#include <cstdint>

namespace NAtracDEnc {
namespace NAtrac1 {

using NAtracDEnc::TScaledBlock;

class IAtrac1BitAlloc {
public:
    IAtrac1BitAlloc() {};
    virtual ~IAtrac1BitAlloc() {};
    virtual uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks, const TBlockSize& blockSize) = 0;
};

class TBitsBooster : public virtual TAtrac1Data {
    std::multimap<uint32_t, uint32_t> BitsBoostMap; //bits needed -> position
    uint32_t MaxBitsPerIteration;
    uint32_t MinKey;
public:
    TBitsBooster();
    uint32_t ApplyBoost(std::vector<uint32_t>* bitsPerEachBlock, uint32_t cur, uint32_t target);
};

class TAtrac1BitStreamWriter : public virtual TAtrac1Data {
    TAea* Container;
public:
    explicit TAtrac1BitStreamWriter(TAea* container)
        : Container(container)
    {};
    void WriteBitStream(const std::vector<uint32_t>& bitsPerEachBlock, const std::vector<TScaledBlock>& scaledBlocks,
                        uint32_t bfuAmountIdx, const TBlockSize& blockSize);
};

class TAtrac1SimpleBitAlloc : public TAtrac1BitStreamWriter, public TBitsBooster, public virtual IAtrac1BitAlloc {
    std::vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks, const uint32_t bfuNum,
                                             const TFloat spread, const TFloat shift, const TBlockSize& blockSize);
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
     uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks, const TBlockSize& blockSize) override;
};

} //namespace NAtrac1
} //namespace NAtracDEnc
