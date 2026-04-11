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
#include "atrac3.h"
#include <compressed_io.h>
#include <atrac/atrac_scale.h>
#include <lib/bs_encode/encode.h>
#include <vector>
#include <utility>

namespace NBitStream {
    class TBitStream;
}

namespace NAtracDEnc {
namespace NAtrac3 {

struct TTonalBlock {
    TTonalBlock(const TAtrac3Data::TTonalVal* valPtr, const TScaledBlock& scaledBlock)
        : ValPtr(valPtr)
        , ScaledBlock(scaledBlock)
    {}
    const TAtrac3Data::TTonalVal* ValPtr = nullptr;
    TScaledBlock ScaledBlock;
};

class TAtrac3BitStreamWriter {
public:
    struct TSingleChannelElement {
        TAtrac3Data::SubbandInfo SubbandInfo;
        std::vector<TTonalBlock> TonalBlocks;
        std::vector<TScaledBlock> ScaledBlocks;
        float Loudness;
        // Per-band bit-allocation boost to compensate for gain-demodulation noise
        // amplification.  Combines the level boost (from the current frame's gain
        // curve) and the scale boost (estimated from the next frame's first gain
        // point).  Set by CreateSubbandInfo; read by the allocation stage.
        int GainBoostPerBand[TAtrac3Data::NumQMF] = {};
    };
private:
    ICompressedOutput* Container;
    const TContainerParams Params;
    const uint32_t BfuIdxConst;
    TBitStreamEncoder Encoder;
    std::vector<char> OutBuffer;
public:
    TAtrac3BitStreamWriter(ICompressedOutput* container, const TContainerParams& params, uint32_t bfuIdxConst);

    void WriteSoundUnit(const std::vector<TSingleChannelElement>& singleChannelElements, float laudness);
};

} // namespace NAtrac3
} // namespace NAtracDEnc
