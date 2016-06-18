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

struct TTonalComponent {
    TTonalComponent(const TAtrac3Data::TTonalVal* valPtr, uint8_t quantIdx, const TScaledBlock& scaledBlock)
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
        std::vector<const TTonalComponent*> SubGroupPtr;
    };
    TOma* Container;
    const TContainerParams Params;
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

    uint8_t GroupTonalComponents(const std::vector<TTonalComponent>& tonalComponents,
                                 TTonalComponentsSubGroup groups[64]);

    uint16_t EncodeTonalComponents(const std::vector<TTonalComponent>& tonalComponents,
                                   NBitStream::TBitStream* bitStream, uint8_t numQmfBand);
public:
    TAtrac3BitStreamWriter(TOma* container, const TContainerParams& params) //no mono mode for atrac3
        : Container(container)
        , Params(params)
    {

    }
    void WriteSoundUnit(const TAtrac3Data::SubbandInfo& subbandInfo,
                        const std::vector<TTonalComponent>& tonalComponents,
                        const std::vector<TScaledBlock>& scaledBlocks);
};

} // namespace NAtrac3
} // namespace NAtracDEnc
