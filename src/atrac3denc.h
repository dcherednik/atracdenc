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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once
#include "config.h"
#include "pcmengin.h"
#include "oma.h"
#include "aea.h"
#include "atrac/atrac3.h"
#include "atrac/atrac3_qmf.h"
#include "transient_detector.h"
#include "delay_buffer.h"
#include "util.h"

#include "atrac/atrac3_bitstream.h"
#include "atrac/atrac_scale.h"
#include "mdct/mdct.h"
#include "gain_processor.h"

#include <functional>
#include <array>
#include <cmath>
namespace NAtracDEnc {

///////////////////////////////////////////////////////////////////////////////

inline uint16_t RelationToIdx(TFloat x) {
    if (x <= 0.5) {
        x = 1.0 / std::max(x, 0.00048828125);
        return 4 + GetFirstSetBit(std::trunc(x));
    } else {
        x = std::min(x, 16.0);
        return 4 - GetFirstSetBit(std::trunc(x));
    }
}

///////////////////////////////////////////////////////////////////////////////

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
    TDelayBuffer<TFloat, 8, 256> PcmBuffer; //8 = 2 channels * 4 bands

    TFloat PrevPeak[2][4]; //2 channel, 4 band - peak level (after windowing), used to check overflow during scalling

    Atrac3SplitFilterBank<TFloat> SplitFilterBank[2];
    TScaler<TAtrac3Data> Scaler;
    std::vector<TTransientDetector> TransientDetectors;
    std::vector<NAtrac3::TAtrac3BitStreamWriter::TSingleChannelElement> SingleChannelElements;
    typedef std::array<uint8_t, NumSpecs> TonalComponentMask;
public:
    struct TTransientParam {
        int32_t Attack0Location; // Attack position relative to previous frame
        TFloat Attack0Relation;
        int32_t Attack1Location; // Attack position relative to previous sample
        TFloat Attack1Relation;
        int32_t ReleaseLocation;
        TFloat ReleaseRelation;
    };
private:
    std::vector<std::vector<TTransientParam>> TransientParamsHistory;
#ifdef ATRAC_UT_PUBLIC
public:
#endif
    TFloat LimitRel(TFloat x);
    TTransientParam CalcTransientParam(const std::vector<TFloat>& gain, TFloat lastMax);
    void CreateSubbandInfo(TFloat* in[4], uint32_t channel,
                           TTransientDetector* transientDetector,
                           TAtrac3Data::SubbandInfo* subbandInfo);
    void ResetTransientParamsHistory(int channel, int band);
    void SetTransientParamsHistory(int channel, int band, const TTransientParam& params);
    const TTransientParam& GetTransientParamsHistory(int channel, int band) const;
    TonalComponentMask AnalyzeTonalComponent(TFloat* specs);
    TTonalComponents ExtractTonalComponents(TFloat* specs, TTonalDetector fn);

    void MapTonalComponents(const TTonalComponents& tonalComponents, std::vector<NAtrac3::TTonalBlock>* componentMap);
public:
    TAtrac3Processor(TCompressedIOPtr&& oma, NAtrac3::TAtrac3EncoderSettings&& encoderSettings);
    ~TAtrac3Processor();
    TPCMEngine<TFloat>::TProcessLambda GetDecodeLambda() override;
    TPCMEngine<TFloat>::TProcessLambda GetEncodeLambda() override;
};
}
