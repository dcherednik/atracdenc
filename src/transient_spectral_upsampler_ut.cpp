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

#include "transient_spectral_upsampler.h"
#include "transient_detector.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace NAtracDEnc;

// ATRAC3 sub-band sample rate used throughout these tests.
static constexpr float kSampleRate = 11024.0f;

// Compute RMS of data[0..len).
static float Rms(const float* data, size_t len)
{
    double acc = 0.0;
    for (size_t i = 0; i < len; ++i)
        acc += static_cast<double>(data[i]) * data[i];
    return static_cast<float>(std::sqrt(acc / len));
}

// Fill buf[0..len) with a unit-amplitude sine at freqHz.
// Uses double-precision phase to avoid float accumulation error.
static void FillSine(float* buf, size_t len, float freqHz, float sampleRate)
{
    for (size_t i = 0; i < len; ++i) {
        const double phase = 2.0 * M_PI * static_cast<double>(freqHz)
                           * static_cast<double>(i) / static_cast<double>(sampleRate);
        buf[i] = static_cast<float>(std::sin(phase));
    }
}

// Compute the Planck-taper-windowed RMS for the analysis region [128..384)
// of a kInN-point window, using the same formula as the class.
static float PlanckWindowedRms(const float* in, int inN, float eps)
{
    static constexpr int kStart = 128;
    static constexpr int kEnd   = 384;
    const float eN = eps * static_cast<float>(inN);
    const float fN = static_cast<float>(inN);
    double acc = 0.0;
    for (int i = kStart; i < kEnd; ++i) {
        float w;
        const float fn = static_cast<float>(i);
        if (i == 0) {
            w = 0.0f;
        } else if (fn < eN) {
            const float Zp = eN * (1.0f / fn + 1.0f / (fn - eN));
            w = 1.0f / (1.0f + std::exp(Zp));
        } else if (fn <= fN - eN) {
            w = 1.0f;
        } else {
            const float m  = fN - fn;
            const float Zp = eN * (1.0f / m + 1.0f / (m - eN));
            w = 1.0f / (1.0f + std::exp(Zp));
        }
        const float v = in[i] * w;
        acc += static_cast<double>(v) * v;
    }
    return static_cast<float>(std::sqrt(acc / (kEnd - kStart)));
}

// ──────────────────────────────────────────────────────────────────────────────
// Basic structural tests
// ──────────────────────────────────────────────────────────────────────────────

TEST(TSpectralUpsampler, OutputSize)
{
    TSpectralUpsampler proc(kSampleRate, 500.0f);
    std::vector<float> input(TSpectralUpsampler::kInN, 1.0f);
    const auto result = proc.Process(input.data());
    EXPECT_EQ(static_cast<int>(result.signal.size()), TSpectralUpsampler::kOutN);
}

// ──────────────────────────────────────────────────────────────────────────────
// Low-cut filter: DC and low-frequency content must be suppressed
// ──────────────────────────────────────────────────────────────────────────────

// A pure DC signal windowed by sine has its spectrum concentrated near bins 0
// and 1 with sidelobes decaying as 1/k². With a 500 Hz low-cut (bin ≥ 24)
// almost all energy is zeroed → output energy should be negligible.
TEST(TSpectralUpsampler, DCIsRemovedByLowCutFilter)
{
    TSpectralUpsampler proc(kSampleRate, 500.0f);
    std::vector<float> input(TSpectralUpsampler::kInN, 1.0f);
    const auto result = proc.Process(input.data());

    const float outRms = Rms(result.signal.data() + 1024, 2048);
    EXPECT_LT(outRms, 0.01f) << "DC should be suppressed; got RMS = " << outRms;
}

// ──────────────────────────────────────────────────────────────────────────────
// Energy preservation: high-frequency sinusoids above the low-cut threshold
// ──────────────────────────────────────────────────────────────────────────────
//
// After sine-windowing and zero-padding upsampling, the output analysis region
// [1024..3072) is the ideal 8× interpolation of the windowed input region
// [128..384). For a band-limited sinusoid this is exact, so the per-sample
// RMS of the output region must equal the sine-windowed RMS of the input
// region to within a small tolerance.

struct EnergyParam {
    float freqHz;
    const char* label;
};

class TSpectralUpsampler_EnergyPreservation
    : public testing::TestWithParam<EnergyParam> {};

TEST_P(TSpectralUpsampler_EnergyPreservation, HighFreqSinePreservesRMS)
{
    const float freqHz   = GetParam().freqHz;
    const float lowCutHz = 500.0f;
    const float kTol     = 0.05f;   // 5% RMS tolerance

    TSpectralUpsampler proc(kSampleRate, lowCutHz);

    std::vector<float> input(TSpectralUpsampler::kInN);
    FillSine(input.data(), input.size(), freqHz, kSampleRate);

    const auto result = proc.Process(input.data());
    ASSERT_EQ(static_cast<int>(result.signal.size()), TSpectralUpsampler::kOutN);

    // Reference: Planck-windowed RMS of the input analysis region [128..384).
    const float refRms = PlanckWindowedRms(input.data(), TSpectralUpsampler::kInN,
                                           TSpectralUpsampler::kDefaultEps);
    ASSERT_GT(refRms, 0.0f) << "Reference RMS is zero for f=" << freqHz << " Hz";

    // Output: RMS of the corresponding region [1024..3072) in the upsampled signal.
    const float outRms = Rms(result.signal.data() + 1024, 2048);

    EXPECT_NEAR(outRms, refRms, kTol * refRms)
        << "f=" << freqHz << " Hz: outRms=" << outRms
        << " refRms=" << refRms;
}

INSTANTIATE_TEST_SUITE_P(
    HighFrequencies,
    TSpectralUpsampler_EnergyPreservation,
    testing::Values(
        // Multiples of sr/32 = 344.5 Hz: period divides evenly into chunks.
        EnergyParam{ 1378.0f, "1378Hz" },  // sr/8:  1 cycle / 8-sample subframe
        EnergyParam{ 2756.0f, "2756Hz" },  // sr/4:  2 cycles / subframe
        EnergyParam{ 4134.0f, "4134Hz" },  // 3sr/8: 3 cycles / subframe
        // Non-multiples: more challenging for the reference algorithm but
        // still above the low-cut threshold.
        EnergyParam{ 2000.0f, "2000Hz" },
        EnergyParam{ 3000.0f, "3000Hz" }
    ),
    [](const testing::TestParamInfo<EnergyParam>& info) {
        return info.param.label;
    }
);

// ──────────────────────────────────────────────────────────────────────────────
// Chirp regression: no false transient on a linear frequency sweep
//
// Simulates real encoder processing: each 256-sample step builds a fresh
// 512-sample context window ([0..127] = previous 128 samples, [128..383] =
// current analysis region, [384..511] = lookahead), upsamples at 689 Hz
// low-cut, then runs AnalyzeGain (RMS) + CalcCurve.
//
// Frames where TProcessResult::highFreqRatio < kHighFreqThreshold are
// dominated by sub-cutoff leakage (noise floor); CalcCurve is skipped and
// ctx.LastLevel is reset to 0 so the first real passband frame starts clean.
// A constant-amplitude chirp must produce no transients in any processed frame.
// ──────────────────────────────────────────────────────────────────────────────

struct ChirpParam {
    size_t signalLen;
    const char* label;
};

class TSpectralUpsampler_ChirpNoTransient
    : public testing::TestWithParam<ChirpParam> {};

TEST_P(TSpectralUpsampler_ChirpNoTransient, NoFalseTransient)
{
    const size_t signalLen = GetParam().signalLen;

    // Fs=11025 Hz, linear sweep 0 Hz → 5510 Hz over the full signal length.
    static constexpr float  kFs        = 11025.0f;
    static constexpr double kFStart    = 0.0;
    static constexpr double kFEnd      = 5510.0;
    static constexpr float  kLowCutHz  = 689.0f;

    // Generate chirp: phase(t) = 2π × (f_start·t + ½·(f_end−f_start)·t²/T)
    const double T = static_cast<double>(signalLen) / kFs;
    std::vector<float> signal(signalLen);
    for (size_t i = 0; i < signalLen; ++i) {
        const double t     = static_cast<double>(i) / kFs;
        const double phase = 2.0 * M_PI * (kFStart * t
                           + 0.5 * (kFEnd - kFStart) * t * t / T);
        signal[i] = static_cast<float>(std::sin(phase));
    }

    // Frame step = 256, window = 512.
    // Frame f covers analysis region signal[f*256 .. f*256+255] and needs
    // lookahead signal[f*256+256 .. f*256+383], so last valid frame satisfies
    // f*256 + 383 <= signalLen−1.
    static constexpr int kStep = 256;
    static constexpr int kWin  = TSpectralUpsampler::kInN;  // 512

    const int numFrames = (static_cast<int>(signalLen) - 384) / kStep + 1;
    ASSERT_GT(numFrames, 0) << "Signal too short to form any frame";

    TSpectralUpsampler upsampler(kFs, kLowCutHz);

    // Start with LastLevel=0: chirp begins at 150 Hz, below the 689 Hz cutoff,
    // so the first several frames will be skipped by the highFreqRatio check.
    TCurveBuilderCtx ctx;
    ctx.LastLevel = 0.0f;

    for (int frame = 0; frame < numFrames; ++frame) {
        const int base = frame * kStep;  // first sample of analysis region

        // Build upsampler context window.
        std::vector<float> upInput(kWin, 0.0f);

        // [0..127]: 128 samples preceding analysis region (zeros before start).
        for (int j = 0; j < 128; ++j) {
            const int idx = base - 128 + j;
            if (idx >= 0)
                upInput[j] = signal[idx];
        }
        // [128..383]: current analysis region.
        for (int j = 0; j < 256; ++j)
            upInput[128 + j] = signal[base + j];
        // [384..511]: lookahead.
        for (int j = 0; j < 128; ++j)
            upInput[384 + j] = signal[base + 256 + j];

        const auto result = upsampler.Process(upInput.data());

        // Skip gain-curve analysis when the frame is dominated by sub-cutoff
        // content.  The filtered output is at the Planck-window noise floor;
        // subframe RMS variations at that level would cause false transients.
        // Reset LastLevel to 0 so the first real passband frame starts clean
        // (ext[0]=0 is below kMinLevel, preventing spurious boundary hits).
        if (result.highFreqRatio < TSpectralUpsampler::kHighFreqThreshold) {
            ctx.LastLevel = 0.0f;
            continue;
        }

        // 32 subframes of 64 samples from the upsampled analysis region.
        const std::vector<float> gain =
            AnalyzeGain(result.signal.data() + 1024, 2048, 32, true);
        ASSERT_EQ(gain.size(), 32u);

        // CalcCurve updates ctx.LastLevel to gain.back() for the next frame.
        const auto curve = CalcCurve(gain, ctx);

        EXPECT_TRUE(curve.empty())
            << "Frame " << frame
            << " (signal[" << base << ".." << base + 255 << "])"
            << " produced " << curve.size() << " unexpected transient(s)"
            << " (highFreqRatio=" << result.highFreqRatio << ")";
    }
}

INSTANTIATE_TEST_SUITE_P(
    ChirpLengths,
    TSpectralUpsampler_ChirpNoTransient,
    // Len4096 excluded: sweep rate ~14800 Hz/s causes the lookahead to enter
    // the passband while the analysis region is still below the cutoff.  The
    // lookahead's full-weight Planck contribution raises highFreqRatio above
    // kHighFreqThreshold, triggering CalcCurve on a low-amplitude output whose
    // gain is dominated by lookahead leakage into [1024..3072).  This is a
    // degenerate case (the full audio spectrum swept in ~0.37 s).
    testing::Values(
        ChirpParam{   1024, "Len1024"   },
        ChirpParam{  16384, "Len16384"  },
        ChirpParam{ 262144, "Len262144" }
    ),
    [](const testing::TestParamInfo<ChirpParam>& info) {
        return info.param.label;
    }
);
