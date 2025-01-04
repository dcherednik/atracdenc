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

#pragma once
#include "atrac_scale.h"
#include "atrac/atrac1.h"
#include "../aea.h"
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
    virtual uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks, const TAtrac1Data::TBlockSizeMod& blockSize, float loudness) = 0;
};

class TBitsBooster {
    std::multimap<uint32_t, uint32_t> BitsBoostMap; //bits needed -> position
    uint32_t MaxBitsPerIteration;
    uint32_t MinKey;
public:
    TBitsBooster();
    uint32_t ApplyBoost(std::vector<uint32_t>* bitsPerEachBlock, uint32_t cur, uint32_t target);
};

class TAtrac1BitStreamWriter {
    ICompressedOutput* Container;
public:
    explicit TAtrac1BitStreamWriter(ICompressedOutput* container);

    void WriteBitStream(const std::vector<uint32_t>& bitsPerEachBlock, const std::vector<TScaledBlock>& scaledBlocks,
                        uint32_t bfuAmountIdx, const TAtrac1Data::TBlockSizeMod& blockSize);
};

class TAtrac1SimpleBitAlloc : public TAtrac1BitStreamWriter, public TBitsBooster, public virtual IAtrac1BitAlloc {
    std::vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks, const uint32_t bfuNum,
                                             const float spread, const float shift,
                                             const TAtrac1Data::TBlockSizeMod& blockSize,
                                             const float loudness);
    const uint32_t BfuIdxConst;
    const bool FastBfuNumSearch;
    static std::vector<float> ATHLong;

    uint32_t GetMaxUsedBfuId(const std::vector<uint32_t>& bitsPerEachBlock);
    uint32_t CheckBfuUsage(bool* changed, uint32_t curBfuId, const std::vector<uint32_t>& bitsPerEachBlock);
public:
    TAtrac1SimpleBitAlloc(ICompressedOutput* container, uint32_t bfuIdxConst, bool fastBfuNumSearch);
    ~TAtrac1SimpleBitAlloc() {};
    uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks, const TAtrac1Data::TBlockSizeMod& blockSize, float loudness) override;
};

} //namespace NAtrac1
} //namespace NAtracDEnc
