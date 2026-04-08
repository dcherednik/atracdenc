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
#include "atrac1.h"
#include <atrac/atrac_scale.h>
#include <lib/bs_encode/encode.h>
#include <compressed_io.h>
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
    TBitsBooster() noexcept;
    uint32_t ApplyBoost(std::vector<uint32_t>* bitsPerEachBlock, uint32_t cur, uint32_t target) const noexcept;
};

class TAt1BitAlloc : public TBitsBooster, public IAtrac1BitAlloc {
public:
    TAt1BitAlloc(ICompressedOutput* container, uint32_t bfuIdxConst) noexcept;
    uint32_t Write(const std::vector<TScaledBlock>& scaledBlocks, const TAtrac1Data::TBlockSizeMod& blockSize, float loudness) override;
private:
    TBitStreamEncoder Encoder;
    ICompressedOutput* Container;
    const uint32_t BfuIdxConst;
};

} //namespace NAtrac1
} //namespace NAtracDEnc
