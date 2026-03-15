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
#include "atrac/at3/atrac3.h"
#include "atrac/at3/atrac3_qmf.h"
#include "delay_buffer.h"
#include "util.h"

#include "atrac/at3/atrac3_bitstream.h"
#include "atrac/atrac_scale.h"
#include "lib/mdct/mdct.h"
#include "gain_processor.h"
#include "transient_detector.h"
#include "transient_spectral_upsampler.h"

#include <algorithm>
#include <functional>
#include <array>
#include <cmath>
namespace NAtracDEnc {

///////////////////////////////////////////////////////////////////////////////

inline uint16_t RelationToIdx(float x) {
    if (x <= 0.5) {
        x = 1.0 / std::max(x, (float)0.00048828125);
        return 4 + GetFirstSetBit((int32_t)std::trunc(x));
    } else {
        x = std::min(x, (float)16.0);
        return 4 - GetFirstSetBit((int32_t)std::trunc(x));
    }
}

///////////////////////////////////////////////////////////////////////////////

class TAtrac3MDCT {
    using TAtrac3Data = NAtrac3::TAtrac3Data;
    NMDCT::TMDCT<512> Mdct512;
    NMDCT::TMIDCT<512> Midct512;
public:
    typedef TGainProcessor<TAtrac3Data> TAtrac3GainProcessor;
    TAtrac3GainProcessor GainProcessor;
    TAtrac3MDCT()
        : Mdct512(1)
    {}
public:
    using TGainModulator = TAtrac3GainProcessor::TGainModulator;
    using TGainDemodulator = TAtrac3GainProcessor::TGainDemodulator;
    typedef std::array<TGainDemodulator, 4> TGainDemodulatorArray;
    typedef std::array<TGainModulator, 4> TGainModulatorArray;
    void Mdct(float specs[1024],
              float* bands[4],
              float maxLevels[4],
              TGainModulatorArray gainModulators);
    void Mdct(float specs[1024],
              float* bands[4],
              TGainModulatorArray gainModulators = TGainModulatorArray());
    void Midct(float specs[1024],
               float* bands[4],
               TGainDemodulatorArray gainDemodulators = TGainDemodulatorArray());
protected:
    TAtrac3MDCT::TGainModulatorArray MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si);
};

class TAtrac3Encoder : public IProcessor, public TAtrac3MDCT {
    using TAtrac3Data = NAtrac3::TAtrac3Data;
    TCompressedOutputPtr Oma;
    const NAtrac3::TAtrac3EncoderSettings Params;
    const std::vector<float> LoudnessCurve;
    TDelayBuffer<float, 8, 256> PcmBuffer; //8 = 2 channels * 4 bands

    float PrevPeak[2][4] = {{0.0}}; //2 channel, 4 band - peak level (after windowing), used to check overflow during scalling

    Atrac3AnalysisFilterBank AnalysisFilterBank[2];

    TScaler<TAtrac3Data> Scaler;
    std::vector<NAtrac3::TAtrac3BitStreamWriter::TSingleChannelElement> SingleChannelElements;
private:
    bool LookAheadPending = true;
    // [channel][band][prev_128 | current_256 | lookahead_256]
    // &LookAheadBuf[ch][b][0] is the 512-sample input for TSpectralUpsampler
    float LookAheadBuf[2][4][640] = {};
    TCurveBuilderCtx CurveCtx[2][4] = {};
    TSpectralUpsampler Upsampler;
    static constexpr float LoudFactor = 0.006;
    float Loudness = LoudFactor;
#ifdef ATRAC_UT_PUBLIC
public:
#endif
    float LimitRel(float x);
    void CreateSubbandInfo(const float* upInput[4], uint32_t channel,
                           TAtrac3Data::SubbandInfo* subbandInfo,
                           int gainBoostPerBand[TAtrac3Data::NumQMF]);
    void Matrixing();

public:
    TAtrac3Encoder(TCompressedOutputPtr&& oma, NAtrac3::TAtrac3EncoderSettings&& encoderSettings);
    ~TAtrac3Encoder();
    TPCMEngine::TProcessLambda GetLambda() override;
};
}
