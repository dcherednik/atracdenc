#pragma once
#include "pcmengin.h"
#include "aea.h"
#include "atrac_encode_settings.h"
#include "atrac/atrac1.h"
#include "atrac/atrac1_qmf.h"
#include "atrac/atrac1_scale.h"
#include "mdct/mdct.h"

namespace NAtracDEnc {

enum EMode {
    E_ENCODE = 1,
    E_DECODE = 2
};

class TAtrac1MDCT : public virtual TAtrac1Data {
    NMDCT::TMDCT<512> Mdct512;
    NMDCT::TMDCT<256> Mdct256;
    NMDCT::TMDCT<64> Mdct64;
    NMDCT::TMIDCT<512> Midct512;
    NMDCT::TMIDCT<256> Midct256;
public:
    void IMdct(double specs[512], const TBlockSize& mode, double* low, double* mid, double* hi);
    void Mdct(double specs[512], double* low, double* mid, double* hi, const TBlockSize& blockSize);
    TAtrac1MDCT()
        : Mdct512(2)
        , Mdct256(1)
    {}
};

class TAtrac1Processor : public TAtrac1MDCT, public virtual TAtrac1Data {
    const bool MixChannel;
    TAeaPtr Aea;

    double PcmBufLow[2][256 + 16];
    double PcmBufMid[2][256 + 16];
    double PcmBufHi[2][512 + 16];

    Atrac1SynthesisFilterBank<double> SynthesisFilterBank[2];
    Atrac1SplitFilterBank<double> SplitFilterBank[2];
 
    NAtrac1::TScaler Scaler;

public:
    TAtrac1Processor(TAeaPtr&& aea, bool mono = false);
    TPCMEngine<double>::TProcessLambda GetDecodeLambda();

    TPCMEngine<double>::TProcessLambda GetEncodeLambda(const TAtrac1EncodeSettings& settings);
};
}
