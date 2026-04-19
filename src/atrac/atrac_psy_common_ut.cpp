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

#include "atrac_psy_common.h"
#include "at1/atrac1.h"
#include "at3/atrac3.h"
#include "at3/atrac3_qmf.h"
#include "at3p/at3p_tables.h"
#include "mdct/mdct.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <type_traits>

using namespace NAtracDEnc;

namespace {

template <class TData>
size_t NumSpecs()
{
    return static_cast<size_t>(TData::SpecsStartLong[TData::MaxBfus - 1]) +
           static_cast<size_t>(TData::SpecsPerBlock[TData::MaxBfus - 1]);
}

template <class TData>
void VerifyImpulseMapsToSingleBfu()
{
    const size_t numBfu = TData::MaxBfus;
    const size_t numSpecs = NumSpecs<TData>();
    std::vector<float> baseEnergy(numSpecs, 1.0f);

    for (size_t bfu = 0; bfu < numBfu; ++bfu) {
        std::vector<float> mdctEnergy = baseEnergy;
        const size_t impulsePos = TData::SpecsStartLong[bfu];
        mdctEnergy[impulsePos] = 32.0f;

        const std::vector<float> flatness = CalcSpectralFlatnessPerBfu<TData>(mdctEnergy);
        ASSERT_EQ(flatness.size(), numBfu);
        EXPECT_LT(flatness[bfu], 0.95f) << "bfu=" << bfu;

        for (size_t i = 0; i < numBfu; ++i) {
            if (i == bfu) {
                continue;
            }
            EXPECT_NEAR(flatness[i], 1.0f, 1e-6f) << "bfu=" << bfu << " changed=" << i;
        }
    }
}

std::vector<float> CalcSineWindow(size_t n)
{
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<float> w(n);
    for (size_t i = 0; i < n; ++i) {
        w[i] = std::sin((kPi * (static_cast<float>(i) + 0.5f)) /
                        static_cast<float>(n));
    }
    return w;
}

template <class TSampleFn>
std::vector<float> BuildAtrac3EnergyViaQmfMdct(TSampleFn&& sampleFn)
{
    NAtrac3::TAtrac3Data initTables;
    (void)initTables;

    constexpr size_t kFrameSz = NAtrac3::TAtrac3Data::NumSamples; // 1024
    constexpr size_t kNumFrames = 2;
    constexpr size_t kSubbands = 4;
    constexpr size_t kSubbandSamples = 256;
    constexpr size_t kMdctInput = 512;

    std::array<float, kFrameSz * kNumFrames> pcm{};
    for (size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = sampleFn(i);
    }

    Atrac3AnalysisFilterBank analysis;
    NMDCT::TMDCT<kMdctInput> mdct512(1.0f);
    std::array<std::array<float, kMdctInput>, kSubbands> bandState{};
    std::array<std::array<float, kSubbandSamples>, kSubbands> subbands{};
    std::array<float*, kSubbands> subPtrs = {
        subbands[0].data(), subbands[1].data(), subbands[2].data(), subbands[3].data()
    };
    std::array<float, NAtrac3::TAtrac3Data::NumSpecs> specs = {};

    for (size_t frame = 0; frame < kNumFrames; ++frame) {
        analysis.Analysis(&pcm[frame * kFrameSz], subPtrs.data());

        for (size_t band = 0; band < kSubbands; ++band) {
            auto& state = bandState[band];
            for (size_t i = 0; i < kSubbandSamples; ++i) {
                state[kSubbandSamples + i] = subbands[band][i];
            }

            std::array<float, kMdctInput> tmp = {};
            std::copy_n(state.data(), kSubbandSamples, tmp.data());
            for (size_t i = 0; i < kSubbandSamples; ++i) {
                const float cur = state[kSubbandSamples + i];
                state[i] = NAtrac3::TAtrac3Data::EncodeWindow[i] * cur;
                tmp[kSubbandSamples + i] = NAtrac3::TAtrac3Data::EncodeWindow[kSubbandSamples - 1 - i] * cur;
            }

            const std::vector<float>& specBand = mdct512(tmp.data());
            float* dst = specs.data() + band * kSubbandSamples;
            std::copy_n(specBand.data(), kSubbandSamples, dst);
            if (band & 1) {
                std::reverse(dst, dst + kSubbandSamples);
            }
        }
    }

    std::vector<float> e(NAtrac3::TAtrac3Data::NumSpecs, 1e-12f);
    for (size_t i = 0; i < e.size(); ++i) {
        e[i] += specs[i] * specs[i];
    }
    return e;
}

template <class TData>
std::vector<float> BuildToneEnergy(float toneHz = 1000.0f)
{
    auto genTone = [toneHz](size_t i) {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kSampleRate = 44100.0f;
        const float phase = 2.0f * kPi * toneHz * static_cast<float>(i) / kSampleRate + 0.37f;
        return std::sin(phase);
    };

    if constexpr (std::is_same_v<TData, NAtrac1::TAtrac1Data>) {
        constexpr size_t n = 1024;
        std::vector<float> in(n);
        const std::vector<float> w = CalcSineWindow(n);
        for (size_t i = 0; i < n; ++i) {
            in[i] = genTone(i) * w[i];
        }
        NMDCT::TMDCT<n> mdct(n);
        const auto& spec = mdct(in.data());
        std::vector<float> e(spec.size(), 1e-12f);
        for (size_t i = 0; i < spec.size(); ++i) {
            e[i] += spec[i] * spec[i];
        }
        return e;
    } else if constexpr (std::is_same_v<TData, NAtrac3::TAtrac3Data>) {
        return BuildAtrac3EnergyViaQmfMdct([&](size_t i) {
            return genTone(i);
        });
    } else {
        static_assert(std::is_same_v<TData, NAt3p::TScaleTable>, "Unsupported codec table for tone energy");
        constexpr size_t n = 4096;
        std::vector<float> in(n);
        const std::vector<float> w = CalcSineWindow(n);
        for (size_t i = 0; i < n; ++i) {
            in[i] = genTone(i) * w[i];
        }
        NMDCT::TMDCT<n> mdct(n);
        const auto& spec = mdct(in.data());
        std::vector<float> e(spec.size(), 1e-12f);
        for (size_t i = 0; i < spec.size(); ++i) {
            e[i] += spec[i] * spec[i];
        }
        return e;
    }
}

template <class TData>
std::vector<float> BuildWhiteNoiseEnergy()
{
    std::mt19937 gen(0xC0FFEEu + static_cast<uint32_t>(NumSpecs<TData>()));
    std::normal_distribution<float> dist(0.0f, 1.0f);
    auto genNoise = [&gen, &dist](size_t, size_t) {
        return dist(gen);
    };

    if constexpr (std::is_same_v<TData, NAtrac1::TAtrac1Data>) {
        constexpr size_t n = 1024;
        std::vector<float> in(n);
        const std::vector<float> w = CalcSineWindow(n);
        for (size_t i = 0; i < n; ++i) {
            in[i] = genNoise(i, n) * w[i];
        }
        NMDCT::TMDCT<n> mdct(n);
        const auto& spec = mdct(in.data());
        std::vector<float> e(spec.size(), 1e-12f);
        for (size_t i = 0; i < spec.size(); ++i) {
            e[i] += spec[i] * spec[i];
        }
        return e;
    } else if constexpr (std::is_same_v<TData, NAtrac3::TAtrac3Data>) {
        return BuildAtrac3EnergyViaQmfMdct([&](size_t i) {
            return genNoise(i, NAtrac3::TAtrac3Data::NumSpecs * 2);
        });
    } else {
        static_assert(std::is_same_v<TData, NAt3p::TScaleTable>, "Unsupported codec table for noise energy");
        constexpr size_t n = 4096;
        std::vector<float> in(n);
        const std::vector<float> w = CalcSineWindow(n);
        for (size_t i = 0; i < n; ++i) {
            in[i] = genNoise(i, n) * w[i];
        }
        NMDCT::TMDCT<n> mdct(n);
        const auto& spec = mdct(in.data());
        std::vector<float> e(spec.size(), 1e-12f);
        for (size_t i = 0; i < spec.size(); ++i) {
            e[i] += spec[i] * spec[i];
        }
        return e;
    }
}

template <class TData>
std::vector<float> CalcBfuEnergy(const std::vector<float>& mdctEnergy)
{
    const size_t numBfu = TData::MaxBfus;
    std::vector<float> bfuEnergy(numBfu, 0.0f);
    for (size_t bfu = 0; bfu < numBfu; ++bfu) {
        const size_t start = TData::SpecsStartLong[bfu];
        const size_t len = TData::SpecsPerBlock[bfu];
        float sum = 0.0f;
        for (size_t i = start; i < start + len; ++i) {
            sum += mdctEnergy[i];
        }
        bfuEnergy[bfu] = sum;
    }
    return bfuEnergy;
}

float WeightedMean(const std::vector<float>& values, const std::vector<float>& weights)
{
    EXPECT_EQ(values.size(), weights.size());
    if (values.size() != weights.size()) {
        return 0.0f;
    }
    const float wsum = std::accumulate(weights.begin(), weights.end(), 0.0f);
    EXPECT_GT(wsum, 0.0f);
    if (wsum <= 0.0f) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < values.size(); ++i) {
        sum += values[i] * weights[i];
    }
    return sum / wsum;
}

template <class TData>
void VerifyToneVsNoiseFlatness(const char* codecName)
{
    const std::vector<float> toneEnergy = BuildToneEnergy<TData>();
    const std::vector<float> noiseEnergy = BuildWhiteNoiseEnergy<TData>();
    ASSERT_EQ(toneEnergy.size(), NumSpecs<TData>());
    ASSERT_EQ(noiseEnergy.size(), NumSpecs<TData>());

    const std::vector<float> toneFlatness = CalcSpectralFlatnessPerBfu<TData>(toneEnergy);
    const std::vector<float> noiseFlatness = CalcSpectralFlatnessPerBfu<TData>(noiseEnergy);

    const std::vector<float> toneBfuEnergy = CalcBfuEnergy<TData>(toneEnergy);
    const std::vector<float> noiseBfuEnergy = CalcBfuEnergy<TData>(noiseEnergy);

    const float toneWeightedFlatness = WeightedMean(toneFlatness, toneBfuEnergy);
    const float noiseWeightedFlatness = WeightedMean(noiseFlatness, noiseBfuEnergy);

    std::cerr << "[FlatnessUT] codec=" << codecName
              << " signal=tone weighted=" << std::fixed << std::setprecision(6)
              << toneWeightedFlatness << "\n";
    std::cerr << "[FlatnessUT] codec=" << codecName
              << " signal=noise weighted=" << std::fixed << std::setprecision(6)
              << noiseWeightedFlatness << "\n";

    EXPECT_GT(noiseWeightedFlatness, toneWeightedFlatness + 0.08f);
}

void VerifyAtrac3ToneFrequencyFlatness(float toneHz)
{
    const std::vector<float> toneEnergy = BuildToneEnergy<NAtrac3::TAtrac3Data>(toneHz);
    const std::vector<float> noiseEnergy = BuildWhiteNoiseEnergy<NAtrac3::TAtrac3Data>();

    const std::vector<float> toneFlatness = CalcSpectralFlatnessPerBfu<NAtrac3::TAtrac3Data>(toneEnergy);
    const std::vector<float> noiseFlatness = CalcSpectralFlatnessPerBfu<NAtrac3::TAtrac3Data>(noiseEnergy);
    const std::vector<float> toneBfuEnergy = CalcBfuEnergy<NAtrac3::TAtrac3Data>(toneEnergy);
    const std::vector<float> noiseBfuEnergy = CalcBfuEnergy<NAtrac3::TAtrac3Data>(noiseEnergy);

    for (size_t bfu = 0; bfu < toneFlatness.size(); ++bfu) {
        std::cerr << "[FlatnessUT] codec=atrac3 signal=tone freq_hz=" << toneHz
                  << " bfu=" << bfu
                  << " flatness=" << std::fixed << std::setprecision(6)
                  << toneFlatness[bfu] << "\n";
    }
    for (size_t bfu = 0; bfu < noiseFlatness.size(); ++bfu) {
        std::cerr << "[FlatnessUT] codec=atrac3 signal=noise"
                  << " bfu=" << bfu
                  << " flatness=" << std::fixed << std::setprecision(6)
                  << noiseFlatness[bfu] << "\n";
    }

    const float toneWeightedFlatness = WeightedMean(toneFlatness, toneBfuEnergy);
    const float noiseWeightedFlatness = WeightedMean(noiseFlatness, noiseBfuEnergy);

    std::cerr << "[FlatnessUT] codec=atrac3 signal=tone freq_hz=" << toneHz
              << " weighted=" << std::fixed << std::setprecision(6)
              << toneWeightedFlatness << "\n";
    std::cerr << "[FlatnessUT] codec=atrac3 signal=noise weighted="
              << std::fixed << std::setprecision(6)
              << noiseWeightedFlatness << "\n";

    EXPECT_GT(noiseWeightedFlatness, toneWeightedFlatness + 0.05f);
}

} // namespace

TEST(AtracPsyCommon, SpectralFlatnessUniformBlock)
{
    const uint32_t start[1] = {0};
    const uint32_t size[1] = {8};
    const std::vector<float> mdctEnergy(8, 4.0f);
    const std::vector<float> flatness = CalcSpectralFlatnessPerBfu(mdctEnergy, start, size, 1);
    ASSERT_EQ(flatness.size(), 1u);
    EXPECT_NEAR(flatness[0], 1.0f, 1e-6f);
}

TEST(AtracPsyCommon, SpectralFlatnessBfuMappingAtrac1)
{
    VerifyImpulseMapsToSingleBfu<NAtrac1::TAtrac1Data>();
}

TEST(AtracPsyCommon, SpectralFlatnessBfuMappingAtrac3)
{
    VerifyImpulseMapsToSingleBfu<NAtrac3::TAtrac3Data>();
}

TEST(AtracPsyCommon, SpectralFlatnessBfuMappingAtrac3Plus)
{
    VerifyImpulseMapsToSingleBfu<NAt3p::TScaleTable>();
}

TEST(AtracPsyCommon, SpectralFlatnessToneVsNoiseAtrac1)
{
    VerifyToneVsNoiseFlatness<NAtrac1::TAtrac1Data>("atrac1");
}

TEST(AtracPsyCommon, SpectralFlatnessToneVsNoiseAtrac3)
{
    VerifyToneVsNoiseFlatness<NAtrac3::TAtrac3Data>("atrac3");
}

TEST(AtracPsyCommon, SpectralFlatnessToneVsNoiseAtrac3Plus)
{
    VerifyToneVsNoiseFlatness<NAt3p::TScaleTable>("atrac3plus");
}

TEST(AtracPsyCommon, SpectralFlatness10kToneAtrac3)
{
    VerifyAtrac3ToneFrequencyFlatness(10000.0f);
}
