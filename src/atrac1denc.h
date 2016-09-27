#pragma once
#include "pcmengin.h"
#include "aea.h"
#include "oma.h"
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
    E_DECODE = 2,
    E_ATRAC3 = 4
};

class TAtrac1MDCT : public virtual NAtrac1::TAtrac1Data {
    NMDCT::TMDCT<512> Mdct512;
    NMDCT::TMDCT<256> Mdct256;
    NMDCT::TMDCT<64> Mdct64;
    NMDCT::TMIDCT<512> Midct512;
    NMDCT::TMIDCT<256> Midct256;
    NMDCT::TMIDCT<64> Midct64;
public:
    void IMdct(TFloat specs[512], const TBlockSize& mode, TFloat* low, TFloat* mid, TFloat* hi);
    void Mdct(TFloat specs[512], TFloat* low, TFloat* mid, TFloat* hi, const TBlockSize& blockSize);
    TAtrac1MDCT()
        : Mdct512(2)
        , Mdct256(1)
    {}
};

class TAtrac1Processor : public IProcessor<TFloat>, public TAtrac1MDCT, public virtual NAtrac1::TAtrac1Data {
    TCompressedIOPtr Aea;
    const NAtrac1::TAtrac1EncodeSettings Settings;

    TFloat PcmBufLow[2][256 + 16];
    TFloat PcmBufMid[2][256 + 16];
    TFloat PcmBufHi[2][512 + 16];

    int32_t PcmValueMax = 32767;
    int32_t PcmValueMin = -32767;

    Atrac1SynthesisFilterBank<TFloat> SynthesisFilterBank[2];
    Atrac1SplitFilterBank<TFloat> SplitFilterBank[2];

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
    TAtrac1Processor(TCompressedIOPtr&& aea, NAtrac1::TAtrac1EncodeSettings&& settings);
    TPCMEngine<TFloat>::TProcessLambda GetDecodeLambda() override;

    TPCMEngine<TFloat>::TProcessLambda GetEncodeLambda() override;
};
}
