#pragma once
#include <vector>
#include <map>
#include <cstdint>

#include "atrac1.h"
namespace NAtracDEnc {

struct TScaledBlock {
	TScaledBlock(uint8_t sfi) : ScaleFactorIndex(sfi) {}
    const uint8_t ScaleFactorIndex = 0;
    std::vector<double> Values;
};

template <class TBaseData>
class TScaler : public TBaseData {
    std::map<double, uint8_t>ScaleIndex;
public:
    TScaler() {
        for (int i = 0; i < 64; i++) {
            ScaleIndex[TBaseData::ScaleTable[i]] = i;
        }
    }
    std::vector<TScaledBlock> Scale(const std::vector<double>& specs, const TBlockSize& blockSize);
};

}
