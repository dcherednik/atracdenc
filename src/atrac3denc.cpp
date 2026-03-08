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
            inv[j] *= 2 * TAtrac3Data::DecodeWindow[j];
            inv[511 - j] *= 2 * TAtrac3Data::DecodeWindow[j];
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
    , Upsampler(11025.0f, 600.0f)
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

void TAtrac3Encoder::CreateSubbandInfo(const float* upInput[4],
                                         uint32_t channel,
                                         TAtrac3Data::SubbandInfo* subbandInfo)
{
    for (int band = 0; band < 4; ++band) {
        auto result = Upsampler.Process(upInput[band]);

        if (result.highFreqRatio < TSpectralUpsampler::kHighFreqThreshold) {
            CurveCtx[channel][band].LastLevel = 0.0f;
            continue;
        }

        // Analysis region [1024..3072) = current frame upsampled (8x)
        const auto gain = AnalyzeGain(result.signal.data() + 1024, 2048, 32, true);

        // nextLevel from first 64-sample subframe of upsampled lookahead [3072..3072+64)
        const float nextLevel = AnalyzeGain(result.signal.data() + 3072, 64, 1, true)[0];

        auto curvePoints = CalcCurve(gain, CurveCtx[channel][band], nextLevel);
        if (curvePoints.empty()) continue;

        std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve;
        curve.reserve(curvePoints.size());
        for (const auto& p : curvePoints)
            curve.push_back({p.Level, p.Location});

        subbandInfo->AddSubbandCurve(band, std::move(curve));
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

        // QMF-filter into the appropriate slot of LookAheadBuf:
        //   first call  → current slot  [128..383]
        //   later calls → lookahead slot [384..639]
        const int qmfOffset = LookAheadPending ? 128 : 384;
        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            float src[TAtrac3Data::NumSamples];
            for (size_t i = 0; i < TAtrac3Data::NumSamples; ++i) {
                src[i] = data[i * meta.Channels + channel] / 4.0;
            }
            float* p[4] = {
                &LookAheadBuf[channel][0][qmfOffset],
                &LookAheadBuf[channel][1][qmfOffset],
                &LookAheadBuf[channel][2][qmfOffset],
                &LookAheadBuf[channel][3][qmfOffset]
            };
            AnalysisFilterBank[channel].Analysis(&src[0], p);
        }

        if (LookAheadPending) {
            LookAheadPending = false;
            return TPCMEngine::EProcessResult::LOOK_AHEAD;
        }

        // Copy current slot [128..383] into PcmBuffer.GetSecond for MDCT
        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            for (int b = 0; b < 4; b++) {
                memcpy(PcmBuffer.GetSecond(channel + b * 2),
                       &LookAheadBuf[channel][b][128], 256 * sizeof(float));
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
                // upInput[b] = &LookAheadBuf[channel][b][0]:
                //   [0..127]   prev tail (last 128 of previous frame)
                //   [128..383] current frame (pre-matrixing)
                //   [384..511] first 128 of lookahead frame
                // Ready to pass directly to TSpectralUpsampler::Process()
                const float* up[4] = {
                    LookAheadBuf[channel][0], LookAheadBuf[channel][1],
                    LookAheadBuf[channel][2], LookAheadBuf[channel][3]
                };
                CreateSubbandInfo(up, channel, &sce->SubbandInfo);
            }

            float* maxOverlapLevels = PrevPeak[channel];
            {
                float* p[4] = {
                    PcmBuffer.GetFirst(channel),   PcmBuffer.GetFirst(channel + 2),
                    PcmBuffer.GetFirst(channel + 4), PcmBuffer.GetFirst(channel + 6)
                };
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

        bitStreamWriter->WriteSoundUnit(SingleChannelElements, Loudness / LoudFactor);

        // Advance look-ahead state: shift buffer left by 256 samples per band
        //   old [256..383] (last 128 of current) → [0..127]  new prev tail
        //   old [384..639] (lookahead)            → [128..383] new current
        //   [384..639] will be filled by the next QMF call
        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            for (int b = 0; b < 4; b++) {
                memmove(LookAheadBuf[channel][b],
                        LookAheadBuf[channel][b] + 256, 384 * sizeof(float));
            }
        }

        return TPCMEngine::EProcessResult::PROCESSED;
    };
}

} //namespace NAtracDEnc
