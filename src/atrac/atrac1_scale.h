#pragma once
#include <vector>
#include <map>
#include <cstdint>

#include "atrac1.h"

namespace NAtrac1 {

struct TScaledBlock {
	TScaledBlock(uint8_t sfi) : ScaleFactorIndex(sfi) {}
    const uint8_t ScaleFactorIndex = 0;
    std::vector<double> Values;
};

class TScaler : public TAtrac1Data {
    static std::map<double, uint8_t>ScaleIndex;
public:
    TScaler();
    std::vector<TScaledBlock> Scale(const std::vector<double>& specs, const TBlockSize& blockSize);
};
}
