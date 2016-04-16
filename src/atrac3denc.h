#pragma once
#include "pcmengin.h"
#include "oma.h"
#include "aea.h"
#include "atrac/atrac3.h"
#include "atrac/atrac3_qmf.h"

#include "atrac/atrac_scale.h"
#include "mdct/mdct.h"
#include "gain_processor.h"

#include <functional>
#include <array>
namespace NAtracDEnc {

class TAtrac3MDCT : public virtual TAtrac3Data {
    NMDCT::TMDCT<512> Mdct512;
    NMDCT::TMIDCT<512> Midct512;
public:
    typedef TGainProcessor<TAtrac3Data> TAtrac3GainProcessor;
    TAtrac3GainProcessor GainProcessor;
public:
    using TGainModulator = TAtrac3GainProcessor::TGainModulator;
    using TGainDemodulator = TAtrac3GainProcessor::TGainDemodulator;
    typedef std::array<TGainDemodulator, 4> TGainDemodulatorArray;
    typedef std::array<TGainModulator, 4> TGainModulatorArray;
    void Mdct(double specs[1024], double* bands[4], TGainModulatorArray gainModulators = TGainModulatorArray());
    void Midct(double specs[1024], double* bands[4], TGainDemodulatorArray gainDemodulators = TGainDemodulatorArray());
protected:
    TAtrac3MDCT::TGainModulatorArray MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si);
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
