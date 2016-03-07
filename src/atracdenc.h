#pragma once
#include "pcmengin.h"
#include "aea.h"
#include "atrac_encode_settings.h"
#include "transient_detector.h"
#include "atrac/atrac1.h"
#include "atrac/atrac1_qmf.h"
#include "atrac/atrac_scale.h"
#include "mdct/mdct.h"

#include <assert.h>
#include <vector>

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

class TAtrac1Processor : public IProcessor<double>, public TAtrac1MDCT, public virtual TAtrac1Data {
    TAeaPtr Aea;
    const TAtrac1EncodeSettings Settings;

    double PcmBufLow[2][256 + 16];
    double PcmBufMid[2][256 + 16];
    double PcmBufHi[2][512 + 16];

    int32_t PcmValueMax = 32767;
    int32_t PcmValueMin = -32767;

    Atrac1SynthesisFilterBank<double> SynthesisFilterBank[2];
    Atrac1SplitFilterBank<double> SplitFilterBank[2];

    class TTransientDetectors {
        std::vector<TTransientDetector> transientDetectorLow;
        std::vector<TTransientDetector> transientDetectorMid;
        std::vector<TTransientDetector> transientDetectorHi;
    public:
        TTransientDetectors()
            : transientDetectorLow(2, TTransientDetector(16, 128))
            , transientDetectorMid(2, TTransientDetector(16, 128))
            , transientDetectorHi(2, TTransientDetector(16, 256))
        {}
        TTransientDetector& GetDetector(uint32_t channel, uint32_t band) {
            switch (band) {
                case 0:
                    return transientDetectorLow[channel];
                break;
                case 1:
                    return transientDetectorMid[channel];
                break;
                case 2:
                    return transientDetectorHi[channel];
                break;
                default:
                    assert(false);
                    return transientDetectorLow[channel];
            }
        }
    };
    TAtrac1Processor::TTransientDetectors TransientDetectors;
 
    TScaler<TAtrac1Data> Scaler;

public:
    TAtrac1Processor(TAeaPtr&& aea, TAtrac1EncodeSettings&& settings);
    TPCMEngine<double>::TProcessLambda GetDecodeLambda() override;

    TPCMEngine<double>::TProcessLambda GetEncodeLambda() override;
};
}
