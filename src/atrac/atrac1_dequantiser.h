#pragma once
#include "atrac1.h"


namespace NAtrac1 {

class TAtrac1Dequantiser : public TAtrac1Data {
public:
    TAtrac1Dequantiser();
    void Dequant(NBitStream::TBitStream* stream, const TBlockSize& bs, double specs[512]);
};
}
