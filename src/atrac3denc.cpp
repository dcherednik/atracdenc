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

#include "atrac3denc.h"
#include "transient_detector.h"
#include "atrac/atrac_psy_common.h"
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <cmath>
namespace NAtracDEnc {

using namespace NMDCT;
using namespace NAtrac3;
using std::vector;

void TAtrac3MDCT::Mdct(float specs[1024], float* bands[4], float maxLevels[4], TGainModulatorArray gainModulators)
{
    for (int band = 0; band < 4; ++band) {
        float* srcBuff = bands[band];
        float* const curSpec = &specs[band*256];
        TGainModulator modFn = gainModulators[band];
        float tmp[512];
        memcpy(&tmp[0], srcBuff, 256 * sizeof(float));
        if (modFn) {
            modFn(&tmp[0], &srcBuff[256]);
        }
        float max = 0.0;
        for (int i = 0; i < 256; i++) {
            max = std::max(max, std::abs(srcBuff[256+i]));
            srcBuff[i] = TAtrac3Data::EncodeWindow[i] * srcBuff[256+i];
            tmp[256+i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[256+i];
        }
        const vector<float>& sp = Mdct512(&tmp[0]);
        assert(sp.size() == 256);
        memcpy(curSpec, sp.data(), 256 * sizeof(float));
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
        maxLevels[band] = max;
    }
}

void TAtrac3MDCT::Mdct(float specs[1024], float* bands[4], TGainModulatorArray gainModulators)
{
    static float dummy[4];
    Mdct(specs, bands, dummy, gainModulators);
}

void TAtrac3MDCT::Midct(float specs[1024], float* bands[4], TGainDemodulatorArray gainDemodulators)
{
    for (int band = 0; band < 4; ++band) {
        float* dstBuff = bands[band];
        float* curSpec = &specs[band*256];
        float* prevBuff = dstBuff + 256;
        TAtrac3GainProcessor::TGainDemodulator demodFn = gainDemodulators[band];
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
        vector<float> inv  = Midct512(curSpec);
        assert(inv.size()/2 == 256);
        for (int j = 0; j < 256; ++j) {
            inv[j] *= /*2 */ TAtrac3Data::DecodeWindow[j];
            inv[511 - j] *= /*2*/ TAtrac3Data::DecodeWindow[j];
        }
        if (demodFn) {
            demodFn(dstBuff, inv.data(), prevBuff);
        } else {
            for (uint32_t j = 0; j < 256; ++j) {
                dstBuff[j] = inv[j] + prevBuff[j];
            }
        }
        memcpy(prevBuff, &inv[256], sizeof(float)*256);
    }
}

TAtrac3Encoder::TAtrac3Encoder(TCompressedOutputPtr&& oma, TAtrac3EncoderSettings&& encoderSettings)
    : Oma(std::move(oma))
    , Params(std::move(encoderSettings))
    , LoudnessCurve(CreateLoudnessCurve(TAtrac3Data::NumSamples))
    , SingleChannelElements(Params.SourceChannels)
    , TransientParamsHistory(Params.SourceChannels, std::vector<TTransientParam>(4))
{}

TAtrac3Encoder::~TAtrac3Encoder()
{}

TAtrac3MDCT::TGainModulatorArray TAtrac3MDCT::MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si)
{
    switch (si.GetQmfNum()) {
        case 1:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), TAtrac3MDCT::TGainModulator(),
                TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator() }};
        }
        case 2:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator() }};
        }
        case 3:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                GainProcessor.Modulate(si.GetGainPoints(2)), TAtrac3MDCT::TGainModulator() }};
        }
        case 4:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                GainProcessor.Modulate(si.GetGainPoints(2)), GainProcessor.Modulate(si.GetGainPoints(3)) }};
        }
        default:
            assert(false);
            return {};

    }
}

float TAtrac3Encoder::LimitRel(float x)
{
    return std::min(std::max(x, TAtrac3Data::GainLevel[15]), TAtrac3Data::GainLevel[0]);
}

void TAtrac3Encoder::ResetTransientParamsHistory(int channel, int band)
{
    TransientParamsHistory[channel][band] = {-1 , 1, -1, 1, -1, 1};
}

void TAtrac3Encoder::SetTransientParamsHistory(int channel, int band, const TTransientParam& params)
{
    TransientParamsHistory[channel][band] = params;
}

const TAtrac3Encoder::TTransientParam& TAtrac3Encoder::GetTransientParamsHistory(int channel, int band) const
{
    return TransientParamsHistory[channel][band];
}

TAtrac3Encoder::TTransientParam TAtrac3Encoder::CalcTransientParam(const std::vector<float>& gain, const float lastMax)
{
    int32_t attack0Location = -1; // position where gain is risen up, -1 - no attack
    float attack0Relation = 1;

    const float attackThreshold = 2;

    {
        // pre-echo searching
        // relative to previous half frame
        for (uint32_t i = 0; i < gain.size(); i++) {
            const float tmp = gain[i] / lastMax;
            if (tmp > attackThreshold) {
                attack0Relation = tmp;
                attack0Location = i;
                break;
            }
         }
    }

    int32_t attack1Location = -1;
    float attack1Relation = 1;
    {
        // pre-echo searching
        // relative to previous subsamples block
        float q = gain[0];
        for (uint32_t i = 1; i < gain.size(); i++) {
            const float tmp = gain[i] / q;
            if (tmp > attackThreshold) {
                attack1Relation = tmp;
                attack1Location = i;
            }
            q = std::max(q, gain[i]);
        }
    }

    int32_t releaseLocation = -1; // position where gain is fallen down, -1 - no release
    float releaseRelation = 1;

    const float releaseTreshold = 2;
    {
        // post-echo searching
        // relative to current frame
        float q = gain.back();
        for (uint32_t i = gain.size() - 2; i > 0; --i) {
            const float tmp = gain[i] / q;
            if (tmp > releaseTreshold) {
                releaseRelation = tmp;
                releaseLocation = i;
                break;
            }
            q = std::max(q, gain[i]);
        }
    }

    return {attack0Location, attack0Relation, attack1Location, attack1Relation, releaseLocation, releaseRelation};
}

void TAtrac3Encoder::CreateSubbandInfo(float* in[4],
                                         uint32_t channel,
                                         TAtrac3Data::SubbandInfo* subbandInfo)
{

    auto relToIdx = [](float rel) {
        rel = 1.0/rel;
        return (uint32_t)(RelationToIdx(rel));
    };

    for (int band = 0; band < 4; ++band) {

        const float* srcBuff = in[band];

        const float* const lastMax = &PrevPeak[channel][band];

        std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve;
        const std::vector<float> gain = AnalyzeGain(srcBuff, 256, 32, false);

        auto transientParam = CalcTransientParam(gain, *lastMax);
        bool hasTransient = false;

        if (transientParam.Attack0Location == -1 && transientParam.Attack1Location == -1 && transientParam.ReleaseLocation == -1) {
            // No transient
            ResetTransientParamsHistory(channel, band);
            continue;
        }
        if (transientParam.Attack0Location == -1 && transientParam.Attack1Location == -1) {
            // Release only in current frame - PostEcho. Not implemented yet.
            // Note: "Hole like" transient also possible (if value is grather in next frame),
            // but we keep peak value of this frame, so in next frame we will use this peak value
            // for searching attack.
            // Handling "Hole like" transients also not implemented. But it should be masked
            SetTransientParamsHistory(channel, band, transientParam);
            continue;
        }

        auto transientParamHistory = GetTransientParamsHistory(channel, band);

        if (transientParamHistory.Attack0Location == -1 && transientParamHistory.Attack1Location == -1 && transientParamHistory.ReleaseLocation == -1 &&
            transientParam.Attack0Location != -1 /*&& transientParam.Attack1Location == -1*/ && transientParam.ReleaseLocation == -1) {
            // No transient at previous frame, but transient (attack) at border of first and second half - simplest way, just scale the first half.

            //std::cout << "CASE 1: " << transientParam.Attack0Location << " " << transientParam.Attack0Relation << std::endl;
            auto idx = relToIdx(transientParam.Attack0Relation);
            //std::cout << "PREV PEAK: " << *lastMax << " " << idx << std::endl;
            curve.push_back({idx, (uint32_t)transientParam.Attack0Location});
            hasTransient = true;
        }

        //std::cout << "transient params band: " << band <<  " atack0loc: " << transientParam.Attack0Location << " atack0rel: " << transientParam.Attack0Relation <<
        //                            " atack1loc: " << transientParam.Attack1Location << " atack1rel: " << transientParam.Attack1Relation <<
        //                            " releaseloc: " << transientParam.ReleaseLocation << " releaserel: "<< transientParam.ReleaseRelation << std::endl;

        //for (int i = 0; i < 256; i++) {
        //    std::cout << i << "   " << srcBuff[i] << "  |  " << srcBuff[i-256] << std::endl;
        //}
 

        SetTransientParamsHistory(channel, band, transientParam);
        if (hasTransient) {
            subbandInfo->AddSubbandCurve(band, std::move(curve));
        }
    }
}

void TAtrac3Encoder::Matrixing()
{
    for (uint32_t subband = 0; subband < 4; subband++) {
        float* pair[2] = {PcmBuffer.GetSecond(subband * 2), PcmBuffer.GetSecond(subband * 2 + 1)};
        float tmp[2];
        for (uint32_t sample = 0; sample < 256; sample++) {
            tmp[0] = pair[0][sample];
            tmp[1] = pair[1][sample];
            pair[0][sample] = (tmp[0] + tmp[1]) / 2.0;
            pair[1][sample] = (tmp[0] - tmp[1]) / 2.0;
        }
    }
}

TPCMEngine::TProcessLambda TAtrac3Encoder::GetLambda()
{
    std::shared_ptr<TAtrac3BitStreamWriter> bitStreamWriter(new TAtrac3BitStreamWriter(Oma.get(), *Params.ConteinerParams, Params.BfuIdxConst));

    struct TChannelData {
        TChannelData()
            : Specs(TAtrac3Data::NumSamples)
        {}

        vector<float> Specs;
    };

    using TData = vector<TChannelData>;
    auto buf = std::make_shared<TData>(2);

    return [this, bitStreamWriter, buf](float* data, const TPCMEngine::ProcessMeta& meta) {
        using TSce = TAtrac3BitStreamWriter::TSingleChannelElement;

        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            float src[TAtrac3Data::NumSamples];

            for (size_t i = 0; i < TAtrac3Data::NumSamples; ++i) {
                src[i] = data[i * meta.Channels  + channel] / 4.0;
            }

            {
                float* p[4] = {PcmBuffer.GetSecond(channel), PcmBuffer.GetSecond(channel+2), PcmBuffer.GetSecond(channel+4), PcmBuffer.GetSecond(channel+6)};
                AnalysisFilterBank[channel].Analysis(&src[0], p);
            }
        }

        if (Params.ConteinerParams->Js && meta.Channels == 2) {
            Matrixing();
        }

        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            auto& specs = (*buf)[channel].Specs;
            TSce* sce = &SingleChannelElements[channel];

            sce->SubbandInfo.Reset();
            if (!Params.NoGainControll) {
                float* p[4] = {PcmBuffer.GetSecond(channel), PcmBuffer.GetSecond(channel+2), PcmBuffer.GetSecond(channel+4), PcmBuffer.GetSecond(channel+6)};
                CreateSubbandInfo(p, channel, &sce->SubbandInfo); //4 detectors per band
            }

            float* maxOverlapLevels = PrevPeak[channel];

            {
                float* p[4] = {PcmBuffer.GetFirst(channel), PcmBuffer.GetFirst(channel+2), PcmBuffer.GetFirst(channel+4), PcmBuffer.GetFirst(channel+6)};
                Mdct(specs.data(), p, maxOverlapLevels, MakeGainModulatorArray(sce->SubbandInfo));
            }

            float l = 0;
            for (size_t i = 0; i < specs.size(); i++) {
                float e = specs[i] * specs[i];
                l += e * LoudnessCurve[i];
            }

            sce->Loudness = l;

            //TBlockSize for ATRAC3 - 4 subband, all are long (no short window)
            sce->ScaledBlocks = Scaler.ScaleFrame(specs, TAtrac3Data::TBlockSizeMod());
        }

        if (meta.Channels == 2 && !Params.ConteinerParams->Js) {
            const TSce& sce0 = SingleChannelElements[0];
            const TSce& sce1 = SingleChannelElements[1];
            Loudness = TrackLoudness(Loudness, sce0.Loudness, sce1.Loudness);
        } else {
            // 1 channel or Js. In case of Js we do not use side channel to adjust loudness
            const TSce& sce0 = SingleChannelElements[0];
            Loudness = TrackLoudness(Loudness, sce0.Loudness);
        }

        if (Params.ConteinerParams->Js && meta.Channels == 1) {
            // In case of JointStereo and one input channel (mono input) we need to construct one empty SCE to produce
            // correct bitstream
            SingleChannelElements.resize(2);
            // Set 1 subband
            SingleChannelElements[1].SubbandInfo.Info.resize(1);
        }

        bitStreamWriter->WriteSoundUnit(SingleChannelElements, Loudness);
        return TPCMEngine::EProcessResult::PROCESSED;
    };
}

} //namespace NAtracDEnc
