#pragma once
#include "atrac3.h"
#include "atrac1.h"
#include "../aea.h"
#include "../oma.h"
#include "../atrac/atrac1.h"
#include "atrac_scale.h"
#include <vector>

static const uint32_t MAXSPECPERBLOCK = 128;

class TAtrac3BitStreamWriter : public virtual TAtrac3Data {
    TOma* Container;
    const TContainerParams Params;
    std::vector<char> OutBuffer;
    void CLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream);
    void VLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream);
    void EncodeSpecs(const std::vector<NAtracDEnc::TScaledBlock>& scaledBlocks, const std::vector<uint32_t>& bitsPerEachBlock, NBitStream::TBitStream* bitStream);
public:
    TAtrac3BitStreamWriter(TOma* container, const TContainerParams& params) //no mono mode for atrac3
        : Container(container)
        , Params(params)
    {

    }
    void WriteSoundUnit(const TAtrac3SubbandInfo& subbandInfo, const std::vector<NAtracDEnc::TScaledBlock>& scaledBlocks);
};
