#pragma once
#include "atrac3.h"
#include "atrac1.h"
#include "../aea.h"
#include "../oma.h"
#include "../atrac/atrac1.h"
#include "atrac_scale.h"
#include <vector>
#include <utility>

static const uint32_t MAXSPECPERBLOCK = 128;

class TAtrac3BitStreamWriter : public virtual TAtrac3Data {
    TOma* Container;
    const TContainerParams Params;
    std::vector<char> OutBuffer;
    uint32_t CLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream);
    uint32_t VLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream);
    std::pair<uint8_t, uint32_t> CalcSpecsBitsConsumption(const std::vector<NAtracDEnc::TScaledBlock>& scaledBlocks, const std::vector<uint32_t>& precisionPerEachBlocks, int* mantisas);
    void EncodeSpecs(const std::vector<NAtracDEnc::TScaledBlock>& scaledBlocks, const std::vector<uint32_t>& bitsPerEachBlock, NBitStream::TBitStream* bitStream);
public:
    TAtrac3BitStreamWriter(TOma* container, const TContainerParams& params) //no mono mode for atrac3
        : Container(container)
        , Params(params)
    {

    }
    void WriteSoundUnit(const TAtrac3Data::SubbandInfo& subbandInfo, const std::vector<NAtracDEnc::TScaledBlock>& scaledBlocks);
};
