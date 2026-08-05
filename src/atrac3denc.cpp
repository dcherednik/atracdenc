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
#include <limits>
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
    for (auto& ch : PrevOverlapGainScale)
        ch.fill(1.0f);
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

static float SafeEnergyScale(float originalEnergy, float modulatedEnergy)
{
    static constexpr float kEnergyEps = 1.0e-20f;
    if (originalEnergy <= kEnergyEps || modulatedEnergy <= kEnergyEps
        || !std::isfinite(originalEnergy) || !std::isfinite(modulatedEnergy)) {
        return 1.0f;
    }
    const float scale = originalEnergy / modulatedEnergy;
    return std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
}

static void BuildSampleDivisors(const std::vector<TAtrac3Data::SubbandInfo::TGainPoint>& pts, float outDiv[256])
{
    std::fill(outDiv, outDiv + 256, 1.0f);

    uint32_t pos = 0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const uint32_t lastPos = pts[i].Location << TAtrac3Data::LocScale;
        float level = TAtrac3Data::GainLevel[pts[i].Level];
        const int incPos = ((i + 1) < pts.size() ? pts[i + 1].Level : TAtrac3Data::ExponentOffset)
                         - pts[i].Level + TAtrac3Data::GainInterpolationPosShift;
        const float gainInc = TAtrac3Data::GainInterpolation[incPos];

        for (; pos < lastPos && pos < 256; ++pos)
            outDiv[pos] = level;
        for (; pos < lastPos + TAtrac3Data::LocSz && pos < 256; ++pos) {
            outDiv[pos] = level;
            level *= gainInc;
        }
    }
}

TAtrac3MDCT::TGainEnergyAnalysis TAtrac3MDCT::CalcGainEnergyScale(
    const float prevOverlap[256],
    const float curInput[256],
    const std::vector<TAtrac3Data::SubbandInfo::TGainPoint>& gainPoints,
    float prevOverlapScale)
{
    TGainEnergyAnalysis res;
    if (!std::isfinite(prevOverlapScale) || prevOverlapScale <= 0.0f)
        prevOverlapScale = 1.0f;

    const float prevDiv = gainPoints.empty()
        ? 1.0f
        : TAtrac3Data::GainLevel[gainPoints.front().Level];

    float prevStoredEnergy = 0.0f;
    for (uint32_t i = 0; i < 256; ++i)
        prevStoredEnergy += prevOverlap[i] * prevOverlap[i];

    const float prevOriginalEnergy = prevStoredEnergy * prevOverlapScale;
    const float prevModulatedEnergy = prevStoredEnergy / (prevDiv * prevDiv);

    float sampleDiv[256];
    BuildSampleDivisors(gainPoints, sampleDiv);

    float curOriginalEnergy = 0.0f;
    float curModulatedEnergy = 0.0f;
    float nextOriginalEnergy = 0.0f;
    float nextModulatedEnergy = 0.0f;
    for (uint32_t i = 0; i < 256; ++i) {
        const float cur = curInput[i];
        const float mod = cur / sampleDiv[i];
        const float winCur = TAtrac3Data::EncodeWindow[255 - i];
        const float winNext = TAtrac3Data::EncodeWindow[i];
        const float curWin = cur * winCur;
        const float modCurWin = mod * winCur;
        const float nextWin = cur * winNext;
        const float modNextWin = mod * winNext;
        curOriginalEnergy += curWin * curWin;
        curModulatedEnergy += modCurWin * modCurWin;
        nextOriginalEnergy += nextWin * nextWin;
        nextModulatedEnergy += modNextWin * modNextWin;
    }

    res.Scale.PrevHalf = SafeEnergyScale(prevOriginalEnergy, prevModulatedEnergy);
    res.Scale.CurHalf = SafeEnergyScale(curOriginalEnergy, curModulatedEnergy);
    res.Scale.Frame = SafeEnergyScale(prevOriginalEnergy + curOriginalEnergy,
                                      prevModulatedEnergy + curModulatedEnergy);
    res.NextOverlapScale = SafeEnergyScale(nextOriginalEnergy, nextModulatedEnergy);
    return res;
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
                                         TAtrac3Data::SubbandInfo* subbandInfo)
{
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

        const auto spectrumOrientation = (band == 1 || band == 3)
            ? ESpectrumOrientation::Inverted
            : ESpectrumOrientation::Direct;
        auto result = Upsampler.Process(upInput[band], spectrumOrientation);

        if (result.highFreqRatio < TSpectralUpsampler::kHighFreqThreshold) {
            if (YamlLog) {
                *YamlLog << std::fixed << std::setprecision(4)
                         << "        skip: low_hfr  # high_freq_ratio "
                         << result.highFreqRatio << " < threshold\n";
            }
            CurveCtx[channel][band].LastLevel = 0.0f;
            CurveCtx[channel][band].CarrierRippleHold = 0;
            PitchCurveCtx[channel][band].LastLevel = 0.0f;
            PitchCurveCtx[channel][band].CarrierRippleHold = 0;
            auto& harmonicCtx = HarmonicGainCtx[channel][band];
            harmonicCtx.PrevFilteredMagnitude.clear();
            harmonicCtx.LastPitchPeriod = 0.0f;
            harmonicCtx.PreviousPitchPeriod = 0.0f;
            harmonicCtx.LastCepstralProminenceDb = 0.0f;
            harmonicCtx.LastHighFreqRatio = 0.0f;
            harmonicCtx.PitchHistorySize = 0;
            continue;
        }

        // Short RMS retains the original transient resolution. Pitch RMS uses
        // an overlapping one-period window to remove carrier/beat phase from
        // a demonstrably stationary harmonic signal.
        std::vector<float> gainLow;
        std::vector<float> gainHigh;
        const auto shortGain = AnalyzeGain(result.signal.data() + 1024, 2048, 32, true,
                                           &gainLow, &gainHigh);
        const float shortNextLevel =
            AnalyzeGain(result.signal.data() + 3072, 64, 1, true)[0];

        auto& harmonicCtx = HarmonicGainCtx[channel][band];
        const float previousFramePitchPeriod = harmonicCtx.LastPitchPeriod;
        const float previousFrameCepstralProminenceDb =
            harmonicCtx.LastCepstralProminenceDb;
        const float previousFrameHighFreqRatio = harmonicCtx.LastHighFreqRatio;
        const float magnitudeChangeDb = CalcMagnitudeChangeDb(
            result.filteredMagnitude, harmonicCtx.PrevFilteredMagnitude);
        const auto octaveFoldedCents = [](float a, float b) {
            const float raw = 1200.0f * std::log2(a / b);
            return std::abs(raw - 1200.0f * std::round(raw / 1200.0f));
        };
        float pitchJitterCents = std::numeric_limits<float>::infinity();
        if (harmonicCtx.PitchHistorySize >= 2
            && result.pitchPeriod > 0.0f
            && harmonicCtx.LastPitchPeriod > 0.0f
            && harmonicCtx.PreviousPitchPeriod > 0.0f) {
            pitchJitterCents = 0.5f * (
                octaveFoldedCents(result.pitchPeriod, harmonicCtx.LastPitchPeriod)
                + octaveFoldedCents(harmonicCtx.LastPitchPeriod,
                                    harmonicCtx.PreviousPitchPeriod));
        }

        // Conservative operating point from the 04/spine offline study. It
        // selected about 35% of the conflicting 04 events and no spine event.
        // Keep the original short analysis whenever any evidence is uncertain.
        static constexpr float kMinCepstralProminenceDb = 1.53f;
        static constexpr float kMaxPitchJitterCents = 75.0f;
        static constexpr float kMaxMagnitudeChangeDb = 7.31f;
        // At very low HFR the cepstrum can be dominated by sub-cutoff tonal
        // content while the envelope being smoothed contains only HPF leakage.
        // The spine regression candidates all occupied HFR 0.06..0.15.
        static constexpr float kMinHfrForPitchRms = 0.20f;
        const bool usePitchRms = band < 3
            && result.highFreqRatio >= kMinHfrForPitchRms
            && result.cepstralProminenceDb >= kMinCepstralProminenceDb
            && pitchJitterCents <= kMaxPitchJitterCents
            && magnitudeChangeDb <= kMaxMagnitudeChangeDb;

        // point0 controls the scale of an entire overlapping MDCT half. Do not
        // derive that scale from one phase-sensitive 8-sample subframe when the
        // cepstrum shows the same periodic carrier on both sides of the frame
        // boundary. This test is intentionally separate from usePitchRms: it
        // only makes the boundary estimate robust and does not suppress the
        // transient-resolution short envelope for the rest of the frame.
        float boundaryPitchDeltaCents = std::numeric_limits<float>::infinity();
        if (result.pitchPeriod > 0.0f && previousFramePitchPeriod > 0.0f) {
            boundaryPitchDeltaCents = octaveFoldedCents(
                result.pitchPeriod, previousFramePitchPeriod);
        }
        static constexpr float kMaxBoundaryPitchDeltaCents = 100.0f;
        const bool usePitchBoundaryRms = band < 3
            && result.highFreqRatio >= kMinHfrForPitchRms
            && previousFrameHighFreqRatio >= kMinHfrForPitchRms
            && result.cepstralProminenceDb >= kMinCepstralProminenceDb
            && previousFrameCepstralProminenceDb >= kMinCepstralProminenceDb
            && boundaryPitchDeltaCents <= kMaxBoundaryPitchDeltaCents;

        const uint32_t pitchWindow = static_cast<uint32_t>(std::max<long>(
            64, std::lround(result.pitchPeriod * TSpectralUpsampler::kUpsample)));
        const auto pitchGain = AnalyzeGainOverlappingRms(
            result.signal.data(), static_cast<uint32_t>(result.signal.size()),
            1024, 2048, 32, pitchWindow);
        const float pitchNextLevel = AnalyzeGainOverlappingRms(
            result.signal.data(), static_cast<uint32_t>(result.signal.size()),
            3072, 64, 1, pitchWindow)[0];

        // Current analysis begins at QMF input sample 128, upsampled sample 1024.
        // Keep both windows inside the flat part of the Planck window so its
        // left taper cannot bias the previous-side RMS downwards.
        static constexpr uint32_t kBoundarySample =
            128 * TSpectralUpsampler::kUpsample;
        static constexpr uint32_t kPlanckFlatStartQmf = static_cast<uint32_t>(
            TSpectralUpsampler::kDefaultEps * TSpectralUpsampler::kInN + 0.999f);
        static constexpr uint32_t kMaxBoundaryWindow =
            (128 - kPlanckFlatStartQmf) * TSpectralUpsampler::kUpsample;
        const uint32_t boundaryWindow = std::min(pitchWindow, kMaxBoundaryWindow);
        float previousBoundaryRms = 0.0f;
        float currentBoundaryRms = 0.0f;
        if (usePitchBoundaryRms && boundaryWindow > 0) {
            const auto rms = [](const float* samples, uint32_t count) {
                double energy = 0.0;
                for (uint32_t i = 0; i < count; ++i)
                    energy += static_cast<double>(samples[i]) * samples[i];
                return static_cast<float>(std::sqrt(energy / count));
            };
            previousBoundaryRms = rms(
                result.signal.data() + kBoundarySample - boundaryWindow,
                boundaryWindow);
            currentBoundaryRms = rms(
                result.signal.data() + kBoundarySample, boundaryWindow);
        }

        harmonicCtx.PreviousPitchPeriod = harmonicCtx.LastPitchPeriod;
        harmonicCtx.LastPitchPeriod = result.pitchPeriod;
        harmonicCtx.LastCepstralProminenceDb = result.cepstralProminenceDb;
        harmonicCtx.LastHighFreqRatio = result.highFreqRatio;
        harmonicCtx.PitchHistorySize = std::min<uint8_t>(
            2, static_cast<uint8_t>(harmonicCtx.PitchHistorySize + 1));
        harmonicCtx.PrevFilteredMagnitude = std::move(result.filteredMagnitude);

        auto updateHpfOverlap = [](const std::vector<float>& envelope,
                                   TCurveBuilderCtx& ctx) {
            float current = 0.0f;
            for (float value : envelope)
                current += value;
            current /= static_cast<float>(envelope.size());
            const float previous = ctx.LastHpfEnergy;
            ctx.LastHpfEnergy = current;
            return (current > 1e-9f && previous > 1e-9f)
                ? previous / current : 1.0f;
        };
        const float shortHpfOverlapRatio =
            updateHpfOverlap(shortGain, CurveCtx[channel][band]);
        const float pitchHpfOverlapRatio =
            updateHpfOverlap(pitchGain, PitchCurveCtx[channel][band]);

        const float hpfOverlapRatio = usePitchRms
            ? pitchHpfOverlapRatio : shortHpfOverlapRatio;
        const float overlapFactor = std::min(1.5f, std::max(1.0f, hpfOverlapRatio));
        const float dynamicMinScore = kMinScore * overlapFactor;
        const float shortDynamicMinScore = kMinScore
            * std::min(1.5f, std::max(1.0f, shortHpfOverlapRatio));
        const float pitchDynamicMinScore = kMinScore
            * std::min(1.5f, std::max(1.0f, pitchHpfOverlapRatio));

        const std::vector<float>& gain = usePitchRms ? pitchGain : shortGain;
        const float nextLevel = usePitchRms ? pitchNextLevel : shortNextLevel;

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
        if (YamlLog) {
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        high_freq_ratio: " << result.highFreqRatio << "\n"
                     << "        cepstral_prominence_db: " << result.cepstralProminenceDb << "\n"
                     << "        pitch_period_qmf: " << result.pitchPeriod << "\n"
                     << "        pitch_jitter_cents: "
                     << (std::isfinite(pitchJitterCents) ? pitchJitterCents : -1.0f) << "\n"
                     << "        magnitude_change_db: "
                     << (std::isfinite(magnitudeChangeDb) ? magnitudeChangeDb : -1.0f) << "\n"
                     << "        pitch_rms_window: " << pitchWindow << "\n"
                     << "        gain_analysis: " << (usePitchRms ? "pitch_period" : "short") << "\n"
                     << "        boundary_pitch_delta_cents: "
                     << (std::isfinite(boundaryPitchDeltaCents)
                            ? boundaryPitchDeltaCents : -1.0f) << "\n"
                     << "        boundary_rms_analysis: "
                     << (usePitchBoundaryRms ? "pitch_period" : "short") << "\n"
                     << "        boundary_rms_window: "
                     << (usePitchBoundaryRms ? boundaryWindow : 0) << "\n"
                     << "        boundary_prev_rms: " << previousBoundaryRms << "\n"
                     << "        boundary_cur_rms: " << currentBoundaryRms << "\n"
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

        const float prevShortTarget = CurveCtx[channel][band].LastTarget;
        const float prevPitchTarget = PitchCurveCtx[channel][band].LastTarget;
        std::vector<TGainCurvePoint> curvePoints;
        if (usePitchRms) {
            // Keep the inactive short-domain history current for an immediate,
            // phase-consistent return when a real attack arrives.
            CalcCurve(shortGain, CurveCtx[channel][band], shortNextLevel,
                      shortDynamicMinScore, nullptr, &gainLow, &gainHigh);
            curvePoints = CalcCurve(pitchGain, PitchCurveCtx[channel][band],
                                    pitchNextLevel, pitchDynamicMinScore,
                                    YamlLog, nullptr, nullptr);
        } else {
            CalcCurve(pitchGain, PitchCurveCtx[channel][band], pitchNextLevel,
                      pitchDynamicMinScore, nullptr, nullptr, nullptr);
            curvePoints = CalcCurve(shortGain, CurveCtx[channel][band],
                                    shortNextLevel, shortDynamicMinScore,
                                    YamlLog, &gainLow, &gainHigh);
        }
        const float prevTarget = usePitchRms ? prevPitchTarget : prevShortTarget;
        const float curTarget = usePitchRms
            ? PitchCurveCtx[channel][band].LastTarget
            : CurveCtx[channel][band].LastTarget;

        if (curvePoints.empty()) {
            if (YamlLog) {
                *YamlLog << "        skip: no_curve\n";
            }
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
            curvePoints.clear();
        }

        if (YamlLog) {
            *YamlLog << std::fixed << std::setprecision(4)
                     << "        max_gain: " << maxGain << "\n";
        }

        // Band 3 is above ~16 kHz where pre-echo is largely inaudible.
        // Skip gain modulation there.
        if (band >= 3) {
            if (YamlLog) {
                *YamlLog << "        skip: band_ge_3"
                         << "  # inaudible HF; gain modulation disabled\n";
            }
            curvePoints.clear();
        }

        // Explicit point 0: correct cross-frame energy steps in the HPF domain.
        // Periodic carriers use equal pitch-sized RMS windows immediately before
        // and after the boundary. Other signals retain the short-envelope estimate.
        // Both paths account for the current curve first divisor.
        if (band < 3) {
            const auto curveBeforePoint0 = curvePoints;
            bool point0Changed = false;
            float point0PrevReference = prevTarget;

            // hpfRmsNextMod: mean of gain[sf] / GainLevel[pts[0].Level]
            // for the subframes strictly before the first curve point's ramp start.
            // These are the only samples the curve actually attenuates at constant level.
            float hpfRmsNextMod = 0.0f;
            bool hpfRmsNextModValid = false;
            const bool pitchBoundaryValid = usePitchBoundaryRms
                && previousBoundaryRms > 1e-6f && currentBoundaryRms > 1e-6f;
            if (pitchBoundaryValid) {
                const float divisor = curvePoints.empty()
                    ? 1.0f : TAtrac3Data::GainLevel[curvePoints[0].Level];
                point0PrevReference = previousBoundaryRms;
                hpfRmsNextMod = currentBoundaryRms / divisor;
                hpfRmsNextModValid = true;
            } else if (!curvePoints.empty() && curvePoints[0].Location > 0) {
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
                         << "        point0_reference: "
                         << (pitchBoundaryValid ? "pitch_boundary" : "short_target") << "\n"
                         << "        point0_prev_reference: " << point0PrevReference << "\n"
                         << "        hpf_rms_next_mod: " << hpfRmsNextMod << "\n";
            }

            if (hpfRmsNextModValid && point0PrevReference > 1e-6f
                && hpfRmsNextMod > 1e-6f) {
                const uint16_t point0Level = RelationToIdx(
                    point0PrevReference / hpfRmsNextMod);
                if (YamlLog) {
                    *YamlLog << "        point0_level: " << point0Level
                             << "  # RelationToIdx(point0_prev_reference/hpf_rms_next_mod)\n";
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
            // improves frame-boundary scale match to the selected boundary reference.
            if (point0Changed) {
                const float scoreBefore = CalcCurveEarlyMismatchScore(gain, curTarget, curveBeforePoint0);
                const float scoreAfter = CalcCurveEarlyMismatchScore(gain, curTarget, curvePoints);
                static constexpr float kPoint0WorseTol = 0.02f; // 2% tolerance
                static constexpr float kBoundaryKeepMargin = 0.20f; // 0.2 bits in log2 scale

                bool keepByBoundary = false;
                float boundaryErrBefore = 0.0f;
                float boundaryErrAfter = 0.0f;
                if (hpfRmsNextModValid && point0PrevReference > 1e-6f
                    && hpfRmsNextMod > 1e-6f) {
                    const auto firstLevel = [](const std::vector<TGainCurvePoint>& pts) -> uint16_t {
                        return pts.empty() ? static_cast<uint16_t>(TAtrac3Data::ExponentOffset) : pts[0].Level;
                    };
                    const float desiredScale = LimitRel(
                        point0PrevReference / hpfRmsNextMod);
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

TAtrac3Data::TTonalComponents TAtrac3Encoder::ExtractTonalComponents(float* specs,
                                                                     const std::vector<float>& flatnessPerBfu)
{
    TAtrac3Data::TTonalComponents res;
    static constexpr float kFlatnessThreshold = 0.01f;
    static constexpr uint32_t kMaxTonalLen = 5;
    // BFU below 8 is too short to get notisiable profit
    // BFU above 29 is hard to tune
    for (uint32_t blockNum = 8; blockNum < 29u; ++blockNum) {
        if (blockNum >= flatnessPerBfu.size()) {
            break;
        }

        const float flatness = flatnessPerBfu[blockNum];
        if (flatness >= kFlatnessThreshold) {
            continue;
        }

        const uint32_t specNumStart = TAtrac3Data::SpecsStartLong[blockNum];
        const uint32_t blockLen = TAtrac3Data::SpecsPerBlock[blockNum];
        const uint32_t specNumEnd = specNumStart + blockLen;
        if (specNumStart >= specNumEnd) {
            continue;
        }

        const uint32_t maxLen = std::min(kMaxTonalLen, blockLen);
        float bestScore = -1.0f;
        uint32_t bestStart = specNumStart;
        uint32_t bestLen = 1;
        for (uint32_t start = specNumStart; start < specNumEnd; ++start) {
            const uint32_t maxLenForStart = std::min(maxLen, specNumEnd - start);
            float score = 0.0f;
            for (uint32_t len = 1; len <= maxLenForStart; ++len) {
                score += std::abs(specs[start + len - 1]);
                if (score > bestScore) {
                    bestScore = score;
                    bestStart = start;
                    bestLen = len;
                }
            }
        }

        if (bestScore <= 0.0f) {
            continue;
        }

        /*
        std::cerr << "atrac3 tonal bfu=" << (uint32_t)blockNum
                  << " flatness=" << flatness
                  << " start=" << bestStart
                  << " len=" << bestLen
                  << " score=" << bestScore
                  << std::endl; */

        for (uint32_t n = 0; n < bestLen; ++n) {
            const uint32_t pos = bestStart + n;
            res.push_back({(uint16_t)pos, specs[pos], (uint8_t)blockNum});
            specs[pos] = 0.0f;
        }
    }

    return res;
}


void TAtrac3Encoder::MapTonalComponents(const TAtrac3Data::TTonalComponents& tonalComponents, vector<TTonalBlock>* componentMap)
{
    for (size_t i = 0; i < tonalComponents.size();) {
        const uint32_t startPos = i;
        uint32_t curPos;
        do {
            curPos = tonalComponents[i].Pos;
            ++i;
        } while ( i < tonalComponents.size() && tonalComponents[i].Pos == curPos + 1 && i - startPos < 7);
        const uint32_t len = i - startPos;
        float tmp[8];
        for (uint32_t j = 0; j < len; ++j)
            tmp[j] = tonalComponents[startPos + j].Val;
        const TScaledBlock& scaledBlock = Scaler.Scale(tmp, len);
        componentMap->push_back({&tonalComponents[startPos], scaledBlock});
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

        const bool jsStereo = Params.ConteinerParams->Js && meta.Channels == 2;
        float jsGainInput[2][4][TSpectralUpsampler::kInN];
        if (jsStereo) {
            for (uint32_t band = 0; band < 4; ++band) {
                for (uint32_t i = 0; i < TSpectralUpsampler::kInN; ++i) {
                    const float left = LookAheadBuf[0][band][i];
                    const float right = LookAheadBuf[1][band][i];
                    jsGainInput[0][band][i] = (left + right) * 0.5f;
                    jsGainInput[1][band][i] = (left - right) * 0.5f;
                }
            }

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

        TAtrac3Data::TTonalComponents tonals[2];

        for (uint32_t channel = 0; channel < meta.Channels; channel++) {
            auto& specs = (*buf)[channel].Specs;
            TSce* sce = &SingleChannelElements[channel];
            sce->TonalBlocks.clear();

            sce->SubbandInfo.Reset();
            for (auto& scale : sce->GainEnergyScale)
                scale = TGainEnergyScale{};
            if (!Params.NoGainControll) {
                // upInput[b]:
                //   [0..127]   prev tail (last 128 of previous frame)
                //   [128..383] current frame
                //   [384..511] first 128 of lookahead frame
                // In JointStereo mode the MDCT encodes M/S, so gain analysis
                // must use the same channel domain.
                // Ready to pass directly to TSpectralUpsampler::Process()
                const float* up[4] = {
                    jsStereo ? jsGainInput[channel][0] : LookAheadBuf[channel][0],
                    jsStereo ? jsGainInput[channel][1] : LookAheadBuf[channel][1],
                    jsStereo ? jsGainInput[channel][2] : LookAheadBuf[channel][2],
                    jsStereo ? jsGainInput[channel][3] : LookAheadBuf[channel][3]
                };
                CreateSubbandInfo(up, channel, &sce->SubbandInfo);
            }

            for (uint32_t band = 0; band < TAtrac3Data::NumQMF; ++band) {
                const uint32_t qmfIdx = channel + band * 2;
                const auto gainEnergy = CalcGainEnergyScale(PcmBuffer.GetFirst(qmfIdx),
                                                            PcmBuffer.GetSecond(qmfIdx),
                                                            sce->SubbandInfo.GetGainPoints(band),
                                                            PrevOverlapGainScale[channel][band]);
                sce->GainEnergyScale[band] = gainEnergy.Scale;
                PrevOverlapGainScale[channel][band] = gainEnergy.NextOverlapScale;
            }
            if (YamlLog && !Params.NoGainControll) {
                *YamlLog << std::fixed << std::setprecision(6)
                         << "    gain_energy_scale:\n";
                for (uint32_t band = 0; band < TAtrac3Data::NumQMF; ++band) {
                    const auto& scale = sce->GainEnergyScale[band];
                    *YamlLog << "      - {band: " << band
                             << ", prev_half: " << scale.PrevHalf
                             << ", cur_half: " << scale.CurHalf
                             << ", frame: " << scale.Frame
                             << ", next_overlap: " << PrevOverlapGainScale[channel][band]
                             << "}\n";
                }
            }

            float* maxOverlapLevels = PrevPeak[channel];
            {
                float* p[4] = {
                    PcmBuffer.GetFirst(channel),   PcmBuffer.GetFirst(channel + 2),
                    PcmBuffer.GetFirst(channel + 4), PcmBuffer.GetFirst(channel + 6)
                };
                Mdct(specs.data(), p, maxOverlapLevels, MakeGainModulatorArray(sce->SubbandInfo));
            }

            vector<float> mdctEnergy(specs.size(), 0.0f);
            float l = 0;
            for (size_t i = 0; i < specs.size(); i++) {
                float e = specs[i] * specs[i];
                mdctEnergy[i] = e;
                const uint32_t band = static_cast<uint32_t>(i / 256);
                l += e * sce->GainEnergyScale[band].Frame * LoudnessCurve[i];
            }

            sce->Loudness = l;

            if (!Params.NoTonalComponents) {
                const vector<float> flatnessPerBfu = CalcSpectralFlatnessPerBfu<TAtrac3Data>(mdctEnergy);
                tonals[channel] = ExtractTonalComponents(specs.data(), flatnessPerBfu);
                sce->TonalBlocks.clear();
                MapTonalComponents(tonals[channel], &sce->TonalBlocks);
            }

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
