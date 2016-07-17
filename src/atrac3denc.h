#pragma once
#include "config.h"
#include "pcmengin.h"
#include "oma.h"
#include "aea.h"
#include "atrac/atrac3.h"
#include "atrac/atrac3_qmf.h"
#include "transient_detector.h"

#include "atrac/atrac3_bitstream.h"
#include "atrac/atrac_scale.h"
#include "mdct/mdct.h"
#include "gain_processor.h"

#include <functional>
#include <array>
namespace NAtracDEnc {

class TAtrac3MDCT : public NAtrac3::TAtrac3Data {
    NMDCT::TMDCT<512> Mdct512;
    NMDCT::TMIDCT<512> Midct512;
public:
    typedef TGainProcessor<TAtrac3Data> TAtrac3GainProcessor;
    TAtrac3GainProcessor GainProcessor;
    TAtrac3MDCT()
        : Mdct512(2)
    {}
public:
    using TGainModulator = TAtrac3GainProcessor::TGainModulator;
    using TGainDemodulator = TAtrac3GainProcessor::TGainDemodulator;
    typedef std::array<TGainDemodulator, 4> TGainDemodulatorArray;
    typedef std::array<TGainModulator, 4> TGainModulatorArray;
    void Mdct(TFloat specs[1024],
              TFloat* bands[4],
              TFloat maxLevels[4],
              TGainModulatorArray gainModulators);
    void Mdct(TFloat specs[1024],
              TFloat* bands[4],
              TGainModulatorArray gainModulators = TGainModulatorArray());
    void Midct(TFloat specs[1024],
               TFloat* bands[4],
               TGainDemodulatorArray gainDemodulators = TGainDemodulatorArray());
protected:
    TAtrac3MDCT::TGainModulatorArray MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si);
};

//returns threshhold
typedef std::function<float(const TFloat* p, uint16_t len)> TTonalDetector;

class TAtrac3Processor : public IProcessor<TFloat>, public TAtrac3MDCT {
    TCompressedIOPtr Oma;
    const NAtrac3::TAtrac3EncoderSettings Params;
    TFloat PcmBuffer[2][4][256 + 256]; //2 channel, 4 band, 256 sample + 256 for overlap buffer

    TFloat LastLevels[2][4]; //2 channel, 4 band - level of last subblock, used to create curve
    TFloat LastHPLevels[2][4]; //2 channel, 4 band - level of last HP filtered subblock, used for transient detection
    TFloat PrevPeak[2][4]; //2 channel, 4 band - peak level (after windowing), used to check overflow during scalling

    Atrac3SplitFilterBank<TFloat> SplitFilterBank[2];
    TScaler<TAtrac3Data> Scaler;
    std::vector<TTransientDetector> TransientDetectors;
    typedef std::array<uint8_t, NumSpecs> TonalComponentMask;
#ifdef ATRAC_UT_PUBLIC
public:
#endif
    uint32_t CheckLevelOverflow(TFloat max, uint32_t levelIdx);
    std::vector<SubbandInfo::TGainPoint> FilterCurve(const std::vector<SubbandInfo::TGainPoint>& curve,
                                                     uint32_t threshold);
    TFloat LimitRel(TFloat x);
    std::vector<TFloat> CalcBaseLevel(TFloat prev, const std::vector<TFloat>& gain);
    TAtrac3Data::SubbandInfo CreateSubbandInfo(TFloat* in[4], uint32_t channel, TTransientDetector* transientDetector);
    TonalComponentMask AnalyzeTonalComponent(TFloat* specs);
    TTonalComponents ExtractTonalComponents(TFloat* specs, TTonalDetector fn);

    std::vector<NAtrac3::TTonalComponent> MapTonalComponents(const TTonalComponents& tonalComponents);
public:
    TAtrac3Processor(TCompressedIOPtr&& oma, NAtrac3::TAtrac3EncoderSettings&& encoderSettings);
    ~TAtrac3Processor();
    TPCMEngine<TFloat>::TProcessLambda GetDecodeLambda() override;
    TPCMEngine<TFloat>::TProcessLambda GetEncodeLambda() override;
};
}
