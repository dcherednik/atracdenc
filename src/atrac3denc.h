#pragma once
#include "pcmengin.h"
#include "oma.h"
#include "aea.h"
#include "atrac/atrac3.h"
#include "atrac/atrac3_qmf.h"

#include "atrac/atrac_scale.h"
#include "mdct/mdct.h"

namespace NAtracDEnc {

class TAtrac3MDCT : public virtual TAtrac3Data {
    NMDCT::TMDCT<512> Mdct512;
public:
    void Mdct(double specs[1024], double* bands[4]);
};

class TAtrac3Processor : public IProcessor<double>, public TAtrac3MDCT, public virtual TAtrac3Data {
    TAeaPtr Oma;
    const TContainerParams Params;
    double PcmBuffer[2][4][256 + 256]; //2 channel, 4 band, 256 sample + 256 for overlap buffer
    Atrac3SplitFilterBank<double> SplitFilterBank[2];
    TScaler<TAtrac3Data> Scaler;
public:
    TAtrac3Processor(TAeaPtr&& oma, const TContainerParams& params);
    ~TAtrac3Processor();
    TPCMEngine<double>::TProcessLambda GetDecodeLambda() override;
    TPCMEngine<double>::TProcessLambda GetEncodeLambda() override;
};
}
