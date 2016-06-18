#pragma once
#include "atrac1.h"
#include "atrac_scale.h"


namespace NAtracDEnc {
namespace NAtrac1 {

class TAtrac1Dequantiser : public TAtrac1Data {
public:
    TAtrac1Dequantiser();
    void Dequant(NBitStream::TBitStream* stream, const TBlockSize& bs, TFloat specs[512]);
};

} //namespace NAtrac1
} //namespace NAtracDEnc
