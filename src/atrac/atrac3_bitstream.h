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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once
#include "atrac3.h"
#include "atrac1.h"
#include "../aea.h"
#include "../oma.h"
#include "../atrac/atrac1.h"
#include "atrac_scale.h"
#include <vector>
#include <utility>

namespace NAtracDEnc {
namespace NAtrac3 {

struct TTonalBlock {
    TTonalBlock(const TAtrac3Data::TTonalVal* valPtr, uint8_t quantIdx, const TScaledBlock& scaledBlock)
        : ValPtr(valPtr)
        , QuantIdx(quantIdx)
        , ScaledBlock(scaledBlock)
    {}
    const TAtrac3Data::TTonalVal* ValPtr = nullptr;
    uint8_t QuantIdx = 0;
    TScaledBlock ScaledBlock;
};

class TAtrac3BitStreamWriter : public virtual TAtrac3Data {
    struct TTonalComponentsSubGroup {
        std::vector<uint8_t> SubGroupMap;
        std::vector<const TTonalBlock*> SubGroupPtr;
    };
    TOma* Container;
    const TContainerParams Params;
    const uint32_t BfuIdxConst;
    std::vector<char> OutBuffer;

    uint32_t CLCEnc(const uint32_t selector, const int mantissas[MaxSpecsPerBlock],
                    const uint32_t blockSize, NBitStream::TBitStream* bitStream);

    uint32_t VLCEnc(const uint32_t selector, const int mantissas[MaxSpecsPerBlock],
                    const uint32_t blockSize, NBitStream::TBitStream* bitStream);

    std::vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                             uint32_t bfuNum, TFloat spread, TFloat shift);

    std::pair<uint8_t, std::vector<uint32_t>> CreateAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                                               uint16_t bitsUsed, int mt[MaxSpecs]);

    std::pair<uint8_t, uint32_t> CalcSpecsBitsConsumption(const std::vector<TScaledBlock>& scaledBlocks,
                                                          const std::vector<uint32_t>& precisionPerEachBlocks,
                                                          int* mantisas);

    void EncodeSpecs(const std::vector<TScaledBlock>& scaledBlocks, NBitStream::TBitStream* bitStream,
                     uint16_t bitsUsed);

    uint8_t GroupTonalComponents(const std::vector<TTonalBlock>& tonalComponents,
                                 TTonalComponentsSubGroup groups[64]);

    uint16_t EncodeTonalComponents(const std::vector<TTonalBlock>& tonalComponents,
                                   NBitStream::TBitStream* bitStream, uint8_t numQmfBand);
public:
    TAtrac3BitStreamWriter(TOma* container, const TContainerParams& params, uint32_t bfuIdxConst) //no mono mode for atrac3
        : Container(container)
        , Params(params)
        , BfuIdxConst(bfuIdxConst)
    {

    }

    struct TSingleChannelElement {
        TAtrac3Data::SubbandInfo SubbandInfo;
        std::vector<TTonalBlock> TonalBlocks;
        std::vector<TScaledBlock> ScaledBlocks;
    };

    void WriteSoundUnit(const std::vector<TSingleChannelElement>& singleChannelElements);
};

} // namespace NAtrac3
} // namespace NAtracDEnc
