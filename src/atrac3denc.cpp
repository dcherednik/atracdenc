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
#include <iomanip>
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
    , Upsampler(11025.0f, 800.0f)
{
    YamlLog = Params.YamlLog;
}

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

// Simulate the decoder's gain modulator on buf[0..255] using pts, then return
// the windowed (2*DecodeWindow) RMS of the result.  Used to compute point0.
static float CalcWindowedRmsAfterCurve(const float* buf,
                                       const std::vector<TGainCurvePoint>& pts) {
    if (pts.empty())
        return 0.0f;
    uint32_t pos = 0;
    float rms = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const uint32_t lastPos = pts[i].Location << TAtrac3Data::LocScale;
        float level = TAtrac3Data::GainLevel[pts[i].Level];
        const int incPos = ((i + 1) < pts.size() ? pts[i + 1].Level : TAtrac3Data::ExponentOffset)
                         - pts[i].Level + TAtrac3Data::GainInterpolationPosShift;
        const float gainInc = TAtrac3Data::GainInterpolation[incPos];
        for (; pos < lastPos && pos < 256; ++pos) {
            const float w = 2.0f * TAtrac3Data::DecodeWindow[pos];
            const float n = (buf[pos] / level) * w;
            rms += n * n;
        }
        for (; pos < lastPos + TAtrac3Data::LocSz && pos < 256; ++pos) {
            const float w = 2.0f * TAtrac3Data::DecodeWindow[pos];
            const float n = (buf[pos] / level) * w;
            rms += n * n;
            level *= gainInc;
        }
    }
    for (; pos < 256; ++pos) {
        const float w = 2.0f * TAtrac3Data::DecodeWindow[pos];
        const float n = buf[pos] * w;
        rms += n * n;
    }
    return std::sqrt(rms / 256.0f);
}

void TAtrac3Encoder::CreateSubbandInfo(const float* upInput[4],
                                         uint32_t channel,
                                         TAtrac3Data::SubbandInfo* subbandInfo,
                                         int gainBoostPerBand[TAtrac3Data::NumQMF])
{
    static constexpr float kMinAggressiveRatio = 10.0f;  // allow Level <=2 only for >= 10x transients
    static constexpr float kMaxOverlapRatio = 1.2f;      // reject if prev overlap > 1.2x current energy
    static constexpr uint32_t kSoftMinLevel = 3;         // default soften extreme levels to 2x
    static constexpr float kLowOverlapRelax = 0.6f;      // allow softer min level when overlap is small
    static constexpr int kLevelBoostCap = 1;             // cap level boost to reduce bit starvation
    static constexpr int kScaleBoostCap = 2;             // allow extra scale boost in low-risk cases
    static constexpr float kMinScore = 1.9f;

    // YAML: channel header (one channel per CreateSubbandInfo call)
    if (YamlLog) {
        *YamlLog << "  - channel: " << channel << "\n"
                 << "    bands:\n";
    }

    for (int band = 0; band < 4; ++band) {
        // YAML: band header emitted immediately so every band has an entry
        if (YamlLog) {
            *YamlLog << "      - band: " << band << "\n";
        }

        auto result = Upsampler.Process(upInput[band]);

        if (result.highFreqRatio < TSpectralUpsampler::kHighFreqThreshold) {
            if (YamlLog) {
                *YamlLog << std::fixed << std::setprecision(4)
                         << "        skip: low_hfr  # high_freq_ratio "
                         << result.highFreqRatio << " < threshold\n";
            }
            CurveCtx[channel][band].LastLevel = 0.0f;
            continue;
        }

        // Analysis region [1024..3072) = current frame upsampled (8x)
        const auto gain = AnalyzeGain(result.signal.data() + 1024, 2048, 32, true);

        // nextLevel from first 64-sample subframe of upsampled lookahead [3072..3072+64)
        const float nextLevel = AnalyzeGain(result.signal.data() + 3072, 64, 1, true)[0];

        const float* bufCur  = PcmBuffer.GetFirst(channel + band * 2);
        const float* bufNext = PcmBuffer.GetSecond(channel + band * 2);

        if (YamlLog) {
            *YamlLog << "        pcm_qmf:  # 256 raw QMF samples, non-modulated, non-windowed\n"
                     << "          ";
            YamlWriteFloatSeq(*YamlLog, bufNext, 256, 6);
            *YamlLog << "\n";
        }

        // Compute overlapRatio early so we can scale minScore accordingly.
        // High overlapRatio (prev frame >> current) means gain curves are
        // more likely to misfire; raise the threshold to suppress them.
        float overlapE = 0.0f, curE = 0.0f;
        for (int i = 0; i < 256; i++) {
            overlapE += bufCur[i] * bufCur[i];
            curE     += bufNext[i] * bufNext[i];
        }
        const float overlapRatio = overlapE / (curE + 1e-9f);

        // Dynamic min-score: linearly scale up from 1× at overlapRatio=1 to
        // 1.5× at overlapRatio=2 (capped).  Below 1 (attack frame) unchanged.
        const float overlapFactor = std::min(1.5f, std::max(1.0f, overlapRatio));
        const float dynamicMinScore = kMinScore * overlapFactor;

        if (YamlLog) {
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        high_freq_ratio: " << result.highFreqRatio << "\n"
                     << "        overlap_ratio: " << overlapRatio
                     << "  # prev_E/cur_E; >1 means prev frame louder\n"
                     << "        dynamic_min_score: " << dynamicMinScore << "\n"
                     << "        next_level: " << nextLevel << "\n"
                     << "        gain: ";
            YamlWriteFloatSeq(*YamlLog, gain, 4);
            *YamlLog << "  # 32 subframe RMS values\n";
        }

        auto curvePoints = CalcCurve(gain, CurveCtx[channel][band], nextLevel, dynamicMinScore);

        if (curvePoints.empty()) {
            if (YamlLog) {
                *YamlLog << "        skip: no_curve\n";
            }
            gainBoostPerBand[band] = 0;
            continue;
        }

        if (YamlLog) {
            *YamlLog << "        curve_raw:\n";
            for (const auto& p : curvePoints) {
                *YamlLog << "          - {level: " << p.Level
                         << ", loc: " << p.Location << "}\n";
            }
        }

        // Explicit point 0: choose scale so windowed-RMS of bufCur matches bufNext
        // as modulated by the existing curve (decoder domain: 2*DecodeWindow).
        {
            float rmsCur = 0.0f;
            for (int i = 0; i < 256; ++i) {
                const float w = 2.0f * TAtrac3Data::DecodeWindow[i];
                const float c = bufCur[i] * w;
                rmsCur += c * c;
            }
            rmsCur = std::sqrt(rmsCur / 256.0f);

            const float rmsNextMod = CalcWindowedRmsAfterCurve(bufNext, curvePoints);
            if (rmsCur > 1e-6f && rmsNextMod > 1e-6f) {
                const uint16_t point0Level = RelationToIdx(rmsCur / rmsNextMod);
                if (YamlLog) {
                    *YamlLog << std::fixed << std::setprecision(6)
                             << "        rms_cur: " << rmsCur << "\n"
                             << "        rms_next_mod: " << rmsNextMod << "\n"
                             << "        point0_level: " << point0Level
                             << "  # RelationToIdx(rms_cur/rms_next_mod)\n";
                }
                auto it = std::find_if(curvePoints.begin(), curvePoints.end(),
                                       [](const TGainCurvePoint& p) { return p.Location == 0; });
                if (it != curvePoints.end()) {
                    it->Level = point0Level;
                } else {
                    curvePoints.insert(curvePoints.begin(), {point0Level, 0});
                }
            }
        }
        // Delay the first non-zero point to the overlap-to-current crossover (conservative).
        {
            float rmsCurSub[32] = {};
            float rmsNextSub[32] = {};
            for (int s = 0; s < 32; ++s) {
                float accCur = 0.0f;
                float accNext = 0.0f;
                const int base = s * 8;
                for (int i = 0; i < 8; ++i) {
                    const int idx = base + i;
                    const float w = 2.0f * TAtrac3Data::DecodeWindow[idx];
                    const float c = bufCur[idx] * w;
                    const float n = bufNext[idx] * w;
                    accCur += c * c;
                    accNext += n * n;
                }
                rmsCurSub[s] = std::sqrt(accCur / 8.0f);
                rmsNextSub[s] = std::sqrt(accNext / 8.0f);
            }

            uint32_t crossover = 32;
            for (uint32_t s = 0; s < 32; ++s) {
                if (rmsNextSub[s] >= rmsCurSub[s]) {
                    crossover = s;
                    break;
                }
            }

            if (YamlLog) {
                *YamlLog << "        crossover: " << crossover
                         << "  # first subframe where next_E >= cur_E (32=none)\n";
            }

            const size_t point1Idx = (!curvePoints.empty() && curvePoints[0].Location == 0) ? 1 : 0;
            if (crossover < 32 && crossover >= 2 && point1Idx < curvePoints.size()) {
                const uint32_t curLoc = curvePoints[point1Idx].Location;
                if (crossover > curLoc) {
                    uint32_t target = std::min<uint32_t>(crossover, curLoc + 6);
                    uint32_t nextLoc = (point1Idx + 1 < curvePoints.size())
                                     ? curvePoints[point1Idx + 1].Location : 32;
                    if (target >= nextLoc && nextLoc > 0)
                        target = nextLoc - 1;
                    if (target > curLoc)
                        curvePoints[point1Idx].Location = target;
                }
            }
        }

        float maxGain = 0.0f;
        for (float g : gain) maxGain = std::max(maxGain, g);
        const float frameEndLevel = gain.back();
        const float ratio = maxGain / (frameEndLevel + 1e-9f);

        // Delay early attack points when overlap dominates to reduce pre-echo.
        if (!curvePoints.empty() && curvePoints[0].Location > 0) {
            const uint32_t loc = curvePoints[0].Location;
            const bool rising = (loc > 0 && loc + 1 < gain.size())
                ? (gain[loc - 1] < gain[loc] && gain[loc] <= gain[loc + 1])
                : false;
            if (rising && overlapRatio > 0.9f) {
                const uint32_t nextLoc = (curvePoints.size() > 1) ? curvePoints[1].Location : 32;
                uint32_t newLoc = std::min<uint32_t>(loc + 2, 31);
                if (newLoc >= nextLoc && nextLoc > 0)
                    newLoc = nextLoc - 1;
                curvePoints[0].Location = newLoc;
            }
        }
        // Soft-cap first point level under overlap dominance to reduce pre-echo.
        if (!curvePoints.empty() && overlapRatio > 0.9f && curvePoints[0].Level < 4)
            curvePoints[0].Level = 4;

        // Level boost: compensate for Demodulate's `level` factor on cur[].
        // level = GainLevel[min point Level], extra bits = 4 - minLevel.
        int levelBoost = 0;
        {
            uint32_t minLevel = 15;
            bool anyActive = false;
            for (const auto& p : curvePoints) {
                minLevel = std::min(minLevel, p.Level);
                if (p.Level < 4) anyActive = true;
            }
            if (!anyActive) {
                if (YamlLog) {
                    *YamlLog << "        skip: no_active_points\n";
                }
                gainBoostPerBand[band] = 0;
                continue;
            }
            if (minLevel <= 2) {
                if (ratio < kMinAggressiveRatio || overlapRatio > kMaxOverlapRatio) {
                    if (YamlLog) {
                        *YamlLog << std::fixed << std::setprecision(4)
                                 << "        skip: aggressive_suppressed"
                                 << "  # ratio=" << ratio << " overlap=" << overlapRatio << "\n";
                    }
                    gainBoostPerBand[band] = 0;
                    continue;
                }
                const uint32_t softMin = (overlapRatio < kLowOverlapRelax) ? 2u : kSoftMinLevel;
                for (auto& p : curvePoints) {
                    if (p.Level < softMin)
                        p.Level = softMin;
                }
            }

            // Scale constraint: curve[0].Level sets decoder scale for previous frame.
            if (!curvePoints.empty() && curvePoints[0].Level < 3)
                curvePoints[0].Level = 3;

            // Re-scan after constraints may have raised some levels.
            minLevel = 4;
            for (const auto& p : curvePoints)
                minLevel = std::min(minLevel, p.Level);
            if (minLevel < 4)
                levelBoost = static_cast<int>(4 - minLevel);
        }
        levelBoost = std::min(levelBoost, kLevelBoostCap);

        // Scale boost: compensate for Demodulate's `scale = GainLevel[giNext[0].Level]`.
        // When decoding frame N, scale = GainLevel[frame N+1's first gain point Level].
        // Frame N+1's CalcCurve: scaleLevel = RelationToIdx(gain.back()_N / nextLevel_{N+2}).
        // We have the full frame N+1 in the lookahead [3072..5119].  Use min(lookaheadGain)
        // as a conservative proxy for nextLevel_{N+2} (≈ quietest level reachable in N+1,
        // a lower bound on frame N+2's start level).
        int scaleBoost = 0;
        if (frameEndLevel > 1e-6f) {
            static constexpr size_t kLookaheadOffset = 3072;
            const size_t outSz = result.signal.size();
            if (outSz > kLookaheadOffset + 64) {
                const uint32_t lookaheadPoints =
                    static_cast<uint32_t>(std::min<size_t>(1024, outSz - kLookaheadOffset) / 64);
                if (lookaheadPoints > 0) {
                    const auto lookaheadGain = AnalyzeGain(result.signal.data() + kLookaheadOffset,
                                                           lookaheadPoints * 64,
                                                           lookaheadPoints, true);
                    const float lookaheadMin = *std::min_element(lookaheadGain.begin(), lookaheadGain.end());
                    if (lookaheadMin > 1e-6f) {
                        const uint32_t estimatedNextScaleLevel = RelationToIdx(frameEndLevel / lookaheadMin);
                        if (estimatedNextScaleLevel < 4u)
                            scaleBoost = static_cast<int>(4u - estimatedNextScaleLevel);
                    }
                }
            }
        }

        const int scaleCap = (overlapRatio < kLowOverlapRelax) ? kScaleBoostCap : kLevelBoostCap;
        scaleBoost = std::min(scaleBoost, scaleCap);
        const int totalBoost = std::min(levelBoost + scaleBoost, kLevelBoostCap);

        if (YamlLog) {
            *YamlLog << "        curve_final:\n";
            for (const auto& p : curvePoints) {
                *YamlLog << "          - {level: " << p.Level
                         << ", loc: " << p.Location << "}\n";
            }
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        max_gain: " << maxGain << "\n"
                     << "        ratio: " << ratio
                     << "  # max_gain/frame_end_level, transient strength\n"
                     << "        level_boost: " << levelBoost << "\n"
                     << "        scale_boost: " << scaleBoost << "\n"
                     << "        total_boost: " << totalBoost << "\n";
        }

        // Bands 2-3 are above ~11 kHz where pre-echo is largely inaudible.
        // Skip gain modulation there entirely; if a transient was detected,
        // redirect the bit boost to band 0 so audible-range reconstruction
        // gets the extra bits instead of the inaudible high-frequency bands.
        if (band >= 2) {
            if (YamlLog) {
                *YamlLog << "        skip: band_ge_2"
                         << "  # inaudible HF; boost redirected to band 0\n"
                         << "        redirected_boost: " << totalBoost << "\n";
            }
            gainBoostPerBand[band] = 0;
            gainBoostPerBand[0] = std::min(gainBoostPerBand[0] + totalBoost, kLevelBoostCap + 1);
            continue;
        }

        if (YamlLog) {
            *YamlLog << "        gain_boost: " << totalBoost << "\n";
        }

        gainBoostPerBand[band] = totalBoost;

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

        // YAML frame header: one document per frame, channels nest below.
        if (YamlLog) {
            const float timeSec = static_cast<float>(FrameNum) * TAtrac3Data::NumSamples / 44100.0f;
            *YamlLog << "---\nframe: " << FrameNum << "\n"
                     << std::fixed << std::setprecision(3)
                     << "time: " << timeSec << "  # seconds\n"
                     << "channels:\n";
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
                std::fill(sce->GainBoostPerBand,
                          sce->GainBoostPerBand + TAtrac3Data::NumQMF, 0);
                CreateSubbandInfo(up, channel, &sce->SubbandInfo, sce->GainBoostPerBand);
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

        ++FrameNum;
        return TPCMEngine::EProcessResult::PROCESSED;
    };
}

} //namespace NAtracDEnc
