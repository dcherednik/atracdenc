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

// Apply the gain curve to raw bufNext and return its MDCT-input-domain RMS.
// bufNext is raw (not pre-windowed); at MDCT time it will be multiplied by
// EncodeWindow[255-pos] (second half of the overlap window).  We apply that
// same weight here so the result is comparable to rmsCur (which already has
// EncodeWindow[i] baked in from the previous frame's MDCT prep).
static float CalcWindowedRmsAfterCurve(const float* buf,
                                       const std::vector<TGainCurvePoint>& pts,
                                       std::ostream* yamlLog = nullptr) {
    // Collect per-sample modulated+windowed values for optional logging.
    float modBuf[256];
    uint32_t pos = 0;
    float rms = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const uint32_t lastPos = pts[i].Location << TAtrac3Data::LocScale;
        float level = TAtrac3Data::GainLevel[pts[i].Level];
        const int incPos = ((i + 1) < pts.size() ? pts[i + 1].Level : TAtrac3Data::ExponentOffset)
                         - pts[i].Level + TAtrac3Data::GainInterpolationPosShift;
        const float gainInc = TAtrac3Data::GainInterpolation[incPos];
        for (; pos < lastPos && pos < 256; ++pos) {
            const float w = TAtrac3Data::EncodeWindow[255 - pos];
            const float n = (buf[pos] / level) * w;
            modBuf[pos] = n;
            rms += n * n;
        }
        for (; pos < lastPos + TAtrac3Data::LocSz && pos < 256; ++pos) {
            const float w = TAtrac3Data::EncodeWindow[255 - pos];
            const float n = (buf[pos] / level) * w;
            modBuf[pos] = n;
            rms += n * n;
            level *= gainInc;
        }
    }
    for (; pos < 256; ++pos) {
        const float w = TAtrac3Data::EncodeWindow[255 - pos];
        const float n = buf[pos] * w;
        modBuf[pos] = n;
        rms += n * n;
    }
    if (yamlLog) {
        // Log 32 subframe RMS values of the modulated+windowed bufNext.
        const uint32_t subSz = 256 / 32;
        *yamlLog << "        rms_next_mod_subframes: ";
        *yamlLog << std::fixed << std::setprecision(6) << "[";
        for (uint32_t sf = 0; sf < 32; ++sf) {
            float sfRms = 0.0f;
            for (uint32_t s = 0; s < subSz; ++s)
                sfRms += modBuf[sf * subSz + s] * modBuf[sf * subSz + s];
            sfRms = std::sqrt(sfRms / subSz);
            if (sf) *yamlLog << ", ";
            *yamlLog << sfRms;
        }
        *yamlLog << "]\n";
    }
    return std::sqrt(rms / 256.0f);
}

// Build 32 subframe-average divisors (gain levels) that Modulate would apply
// to bufNext for a given curve.
static void BuildSubframeDivisors(const std::vector<TGainCurvePoint>& pts, float outDiv[32]) {
    float sampleDiv[256];
    std::fill(sampleDiv, sampleDiv + 256, 1.0f);

    uint32_t pos = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const uint32_t lastPos = pts[i].Location << TAtrac3Data::LocScale;
        float level = TAtrac3Data::GainLevel[pts[i].Level];
        const int incPos = ((i + 1) < pts.size() ? pts[i + 1].Level : TAtrac3Data::ExponentOffset)
                         - pts[i].Level + TAtrac3Data::GainInterpolationPosShift;
        const float gainInc = TAtrac3Data::GainInterpolation[incPos];

        for (; pos < lastPos && pos < 256; ++pos) {
            sampleDiv[pos] = level;
        }
        for (; pos < lastPos + TAtrac3Data::LocSz && pos < 256; ++pos) {
            sampleDiv[pos] = level;
            level *= gainInc;
        }
    }

    for (uint32_t sf = 0; sf < 32; ++sf) {
        float sum = 0.0f;
        for (uint32_t s = 0; s < 8; ++s)
            sum += sampleDiv[sf * 8 + s];
        outDiv[sf] = sum / 8.0f;
    }
}

// Score how well the curve keeps early-frame modulated HPF envelope near target,
// with a small penalty for abrupt divisor changes (a leakage proxy).
static float CalcCurveEarlyMismatchScore(const std::vector<float>& gain,
                                         float target,
                                         const std::vector<TGainCurvePoint>& pts) {
    if (gain.size() != 32 || target <= 1e-9f)
        return 0.0f;

    float div[32];
    BuildSubframeDivisors(pts, div);

    uint32_t maxLoc = 0;
    for (const auto& p : pts)
        maxLoc = std::max(maxLoc, p.Location);
    const uint32_t evalSf = std::min<uint32_t>(32, std::max<uint32_t>(3, maxLoc + 3));

    static constexpr float kEps = 1e-9f;
    float fit = 0.0f;
    for (uint32_t sf = 0; sf < evalSf; ++sf) {
        const float mod = gain[sf] / std::max(div[sf], kEps);
        const float e = std::log2(std::max(mod, kEps) / std::max(target, kEps));
        fit += e * e;
    }
    fit /= evalSf;

    float leak = 0.0f;
    float wsum = 0.0f;
    for (uint32_t sf = 0; sf + 1 < evalSf; ++sf) {
        const float a = std::log2(std::max(div[sf], kEps));
        const float b = std::log2(std::max(div[sf + 1], kEps));
        const float d = b - a;
        const float w = 0.5f * (gain[sf] + gain[sf + 1]);
        leak += d * d * w;
        wsum += w;
    }
    if (wsum > kEps)
        leak /= wsum;

    static constexpr float kLeakWeight = 0.25f;
    return fit + kLeakWeight * leak;
}

void TAtrac3Encoder::CreateSubbandInfo(const float* upInput[4],
                                         uint32_t channel,
                                         TAtrac3Data::SubbandInfo* subbandInfo,
                                         int gainBoostPerBand[TAtrac3Data::NumQMF])
{
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
        std::vector<float> gainLow;
        std::vector<float> gainHigh;
        const auto gain = AnalyzeGain(result.signal.data() + 1024, 2048, 32, true,
                                      &gainLow, &gainHigh);

        // nextLevel from first 64-sample subframe of upsampled lookahead [3072..3072+64)
        const float nextLevel = AnalyzeGain(result.signal.data() + 3072, 64, 1, true)[0];

        // HPF-domain overlap ratio: mean HPF RMS of previous frame vs current frame.
        // This is domain-matched with gain[] (both HPF-upsampled), unlike full-band
        // overlapRatio which is inflated by bass energy that has nothing to do with
        // whether an HPF-domain transient should be protected.
        float curHpfEnergy = 0.0f;
        for (float v : gain) curHpfEnergy += v;
        curHpfEnergy /= static_cast<float>(gain.size());
        const float prevHpfEnergy = CurveCtx[channel][band].LastHpfEnergy;
        CurveCtx[channel][band].LastHpfEnergy = curHpfEnergy;
        const float hpfOverlapRatio = (curHpfEnergy > 1e-9f && prevHpfEnergy > 1e-9f)
            ? (prevHpfEnergy / curHpfEnergy) : 1.0f;

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

        // Dynamic min-score: raise threshold when prev HPF frame was louder.
        // Uses HPF-domain ratio so bass-heavy prev frames don't suppress real HPF transients.
        const float overlapFactor = std::min(1.5f, std::max(1.0f, hpfOverlapRatio));
        const float dynamicMinScore = kMinScore * overlapFactor;

        if (YamlLog) {
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        high_freq_ratio: " << result.highFreqRatio << "\n"
                     << "        overlap_ratio: " << overlapRatio
                     << "  # prev_E/cur_E full-band; >1 means prev frame louder\n"
                     << "        hpf_overlap_ratio: " << hpfOverlapRatio
                     << "  # prev_HPF/cur_HPF; used for transient suppression decisions\n"
                     << "        dynamic_min_score: " << dynamicMinScore << "\n"
                     << "        next_level: " << nextLevel << "\n"
                     << "        gain: ";
            YamlWriteFloatSeq(*YamlLog, gain, 4);
            *YamlLog << "  # 32 subframe RMS values\n";
        }

        const float prevTarget = CurveCtx[channel][band].LastTarget;
        auto curvePoints = CalcCurve(gain, CurveCtx[channel][band], nextLevel,
                                     dynamicMinScore, YamlLog,
                                     &gainLow, &gainHigh);
        const float curTarget = CurveCtx[channel][band].LastTarget;

        // HACK: frame 22 ch=1 band=0 — emit {level=4, loc=7}, {level=0, loc=20}, no point0
        const bool hackOverride = false;(FrameNum == 1406 && channel == 0 && band == 1);
        if (hackOverride) {
            curvePoints = {{1,2}, {0,10}, {1,14}, {2,19}, {3,27}};
        }

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

        float maxGain = 0.0f;
        for (float g : gain) maxGain = std::max(maxGain, g);
        const float frameEndLevel = gain.back();
        const float ratio = maxGain / (frameEndLevel + 1e-9f);

        // Minimum signal gate: suppress curves on near-silent frames.
        // Firing on noise-floor content wastes bitrate and can produce extreme
        // Level values against a tiny target.
        // Use curvePoints.clear() (not continue) so point0 still runs for any
        // genuine cross-frame energy step at the OLA boundary.
        static constexpr float kMinSignalThreshold = 1e-4f;
        if (maxGain < kMinSignalThreshold) {
            if (YamlLog)
                *YamlLog << std::fixed << std::setprecision(6)
                         << "        skip: below_min_signal  # maxGain " << maxGain << "\n";
            gainBoostPerBand[band] = 0;
            curvePoints.clear();
        }

        // Amplifying-only curves require reliable HPF analysis.  When HFR is low
        // the HPF gain[] does not represent full-band energy: a tiny HPF transient
        // can produce level 9 (×32 amplification) on a loud full-band signal,
        // catastrophically over-inflating MDCT coefficients.
        static constexpr float kMinHfrForAmplify = 0.3f;
        if (result.highFreqRatio < kMinHfrForAmplify) {
            if (YamlLog)
                *YamlLog << "        skip: amplify_low_hfr\n";
            gainBoostPerBand[band] = 0;
            curvePoints.clear();
        }

        int levelBoost = 0;

        // Scale boost: compensate for Demodulate's `scale = GainLevel[giNext[0].Level]`.
        // When decoding frame N, scale = GainLevel[frame N+1's first gain point Level].
        // Frame N+1's CalcCurve: scaleLevel = RelationToIdx(gain.back()_N / nextLevel_{N+2}).
        // We have the full frame N+1 in the lookahead [3072..5119].  Use min(lookaheadGain)
        // as a conservative proxy for nextLevel_{N+2} (≈ quietest level reachable in N+1,
        // a lower bound on frame N+2's start level).
        int scaleBoost = 0;
        {
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
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        max_gain: " << maxGain << "\n"
                     << "        ratio: " << ratio
                     << "  # max_gain/frame_end_level, transient strength\n"
                     << "        level_boost: " << levelBoost << "\n"
                     << "        scale_boost: " << scaleBoost << "\n"
                     << "        total_boost: " << totalBoost << "\n";
        }

        // Band 3 is above ~16 kHz where pre-echo is largely inaudible.
        // Skip gain modulation there; if a transient was detected,
        // redirect the bit boost to band 0 so audible-range reconstruction
        // gets the extra bits instead of the inaudible high-frequency band.
        if (band >= 3) {
            if (YamlLog) {
                *YamlLog << "        skip: band_ge_3"
                         << "  # inaudible HF; boost redirected to band 0\n"
                         << "        redirected_boost: " << totalBoost << "\n";
            }
            gainBoostPerBand[band] = 0;
            gainBoostPerBand[0] = std::min(gainBoostPerBand[0] + totalBoost, kLevelBoostCap + 1);
            curvePoints.clear();
        }

        if (band < 3) {
            if (YamlLog)
                *YamlLog << "        gain_boost: " << totalBoost << "\n";
            gainBoostPerBand[band] = totalBoost;
        }


        // Explicit point 0: correct cross-frame energy step in the HPF domain.
        // Compare prevTarget (what the previous frame's curve was targeting, in the
        // HPF gain[] domain) against the mean HPF level of the pre-ramp zone of
        // bufNext after applying the current curve's attenuation.  Both quantities
        // are in the same filtered domain, avoiding LF-content distortion.
        if (!hackOverride && band < 3) {
            const auto curveBeforePoint0 = curvePoints;
            bool point0Changed = false;

            // hpfRmsNextMod: mean of gain[sf] / GainLevel[pts[0].Level]
            // for the subframes strictly before the first curve point's ramp start.
            // These are the only samples the curve actually attenuates at constant level.
            float hpfRmsNextMod = 0.0f;
            bool hpfRmsNextModValid = false;
            if (!curvePoints.empty() && curvePoints[0].Location > 0) {
                const uint32_t nBefore = curvePoints[0].Location;  // subSz==8 == LocScale shift
                const float divisor = TAtrac3Data::GainLevel[curvePoints[0].Level];
                float sum = 0.0f;
                for (uint32_t sf = 0; sf < nBefore; ++sf)
                    sum += gain[sf];
                hpfRmsNextMod = (sum / nBefore) / divisor;
                hpfRmsNextModValid = true;
            } else if (curvePoints.empty()) {
                float sum = 0.0f;
                for (float v : gain) sum += v;
                hpfRmsNextMod = sum / gain.size();
                hpfRmsNextModValid = true;
            }

            if (YamlLog) {
                *YamlLog << std::fixed << std::setprecision(6)
                         << "        prev_target: " << prevTarget << "\n"
                         << "        hpf_rms_next_mod: " << hpfRmsNextMod << "\n";
            }

            if (hpfRmsNextModValid && prevTarget > 1e-6f && hpfRmsNextMod > 1e-6f) {
                const uint16_t point0Level = RelationToIdx(prevTarget / hpfRmsNextMod);
                if (YamlLog) {
                    *YamlLog << "        point0_level: " << point0Level
                             << "  # RelationToIdx(prev_target/hpf_rms_next_mod)\n";
                }
                auto it = std::find_if(curvePoints.begin(), curvePoints.end(),
                                       [](const TGainCurvePoint& p) { return p.Location == 0; });
                if (it != curvePoints.end()) {
                    if (it->Level != point0Level) {
                        it->Level = point0Level;
                        point0Changed = true;
                    }
                } else if (point0Level != 4 || !curvePoints.empty()) {
                    curvePoints.insert(curvePoints.begin(), {point0Level, 0});
                    point0Changed = true;
                }
            }

            // Guard: keep point0 only if it does not worsen local envelope fit.
            // Additional boundary protection: keep point0 if it materially
            // improves frame-boundary scale match to prevTarget/hpfRmsNextMod.
            if (point0Changed) {
                const float scoreBefore = CalcCurveEarlyMismatchScore(gain, curTarget, curveBeforePoint0);
                const float scoreAfter = CalcCurveEarlyMismatchScore(gain, curTarget, curvePoints);
                static constexpr float kPoint0WorseTol = 0.02f; // 2% tolerance
                static constexpr float kBoundaryKeepMargin = 0.20f; // 0.2 bits in log2 scale

                bool keepByBoundary = false;
                float boundaryErrBefore = 0.0f;
                float boundaryErrAfter = 0.0f;
                if (hpfRmsNextModValid && prevTarget > 1e-6f && hpfRmsNextMod > 1e-6f) {
                    const auto firstLevel = [](const std::vector<TGainCurvePoint>& pts) -> uint16_t {
                        return pts.empty() ? static_cast<uint16_t>(TAtrac3Data::ExponentOffset) : pts[0].Level;
                    };
                    const float desiredScale = LimitRel(prevTarget / hpfRmsNextMod);
                    const float scaleBefore = TAtrac3Data::GainLevel[firstLevel(curveBeforePoint0)];
                    const float scaleAfter = TAtrac3Data::GainLevel[firstLevel(curvePoints)];
                    static constexpr float kEps = 1e-9f;
                    boundaryErrBefore = std::abs(std::log2(std::max(scaleBefore, kEps) / std::max(desiredScale, kEps)));
                    boundaryErrAfter = std::abs(std::log2(std::max(scaleAfter, kEps) / std::max(desiredScale, kEps)));
                    keepByBoundary = (boundaryErrAfter + kBoundaryKeepMargin < boundaryErrBefore);
                    if (YamlLog) {
                        *YamlLog << std::fixed << std::setprecision(6)
                                 << "        point0_guard_boundary_err_before: " << boundaryErrBefore << "\n"
                                 << "        point0_guard_boundary_err_after: " << boundaryErrAfter << "\n";
                    }
                }

                if (!keepByBoundary && scoreAfter > scoreBefore * (1.0f + kPoint0WorseTol)) {
                    curvePoints = curveBeforePoint0;
                    if (YamlLog) {
                        *YamlLog << std::fixed << std::setprecision(6)
                                 << "        point0_guard: reverted  # score_after " << scoreAfter
                                 << " > score_before " << scoreBefore << "\n";
                    }
                } else if (YamlLog) {
                    *YamlLog << std::fixed << std::setprecision(6)
                             << "        point0_guard: kept  # score_before " << scoreBefore
                             << ", score_after " << scoreAfter;
                    if (keepByBoundary) {
                        *YamlLog << ", boundary_err_before " << boundaryErrBefore
                                 << ", boundary_err_after " << boundaryErrAfter;
                    }
                    *YamlLog << "\n";
                }
            }
        }

        // If explicit point0 has the same level as the next point, it does not
        // change modulation shape and only wastes 9 bits in the bitstream.
        if (curvePoints.size() >= 2
            && curvePoints[0].Location == 0
            && curvePoints[0].Level == curvePoints[1].Level) {
            curvePoints.erase(curvePoints.begin());
        }

        if (YamlLog) {
            *YamlLog << "        curve_final:\n";
            for (const auto& p : curvePoints) {
                *YamlLog << "          - {level: " << p.Level
                         << ", loc: " << p.Location << "}\n";
            }
        }

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
