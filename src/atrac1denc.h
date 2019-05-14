/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once
#include "pcmengin.h"
#include "aea.h"
#include "oma.h"
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
        : Mdct512(1)
        , Mdct256(0.5)
        , Mdct64(0.5)
        , Midct512(512*2)
        , Midct256(256*2)
        , Midct64(64*2)
    {}
};

class TAtrac1Processor : public IProcessor<TFloat>, public TAtrac1MDCT, public virtual NAtrac1::TAtrac1Data {
    TCompressedIOPtr Aea;
    const NAtrac1::TAtrac1EncodeSettings Settings;

    TFloat PcmBufLow[2][256 + 16];
    TFloat PcmBufMid[2][256 + 16];
    TFloat PcmBufHi[2][512 + 16];

    int32_t PcmValueMax = 1;
    int32_t PcmValueMin = -1;

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
