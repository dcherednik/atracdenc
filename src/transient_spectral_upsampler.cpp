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

#include "lib/fft/kissfft_impl/tools/kiss_fftr.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NAtracDEnc {

TSpectralUpsampler::TSpectralUpsampler(float sampleRate, float lowCutHz, float epsilon)
    // Round up so that lowCutHz itself is passed through.
    : LowCutBin(static_cast<int>(std::ceil(lowCutHz * kInN / sampleRate)))
    , Win(kInN)
    , FwdCfg(static_cast<void*>(kiss_fftr_alloc(kInN,  0, nullptr, nullptr)))
    , InvCfg(static_cast<void*>(kiss_fftr_alloc(kOutN, 1, nullptr, nullptr)))
{
    // Planck-taper window: smooth logistic taper; flat top = 1 in the middle.
    //
    //   Left taper  (0 < n < ε·N):         w[n] = 1 / (1 + exp(Z+(n)))
    //   Flat top    (ε·N ≤ n ≤ N·(1-ε)):   w[n] = 1
    //   Right taper (N·(1-ε) < n < N):     w[n] = 1 / (1 + exp(Z+(N-n)))
    //   Endpoints:  w[0] = w[N-1] = 0
    //
    //   where Z+(n) = ε·N · (1/n + 1/(n - ε·N))
    //
    // Flat-top range for N=512:
    //   ε=0.10 → eN=51.2,  flat top n in [52..460]
    //   ε=0.15 → eN=76.8,  flat top n in [77..435]
    //   ε=0.20 → eN=102.4, flat top n in [103..409]
    // All fully contain the analysis region [128..384).
    const float eN = epsilon * static_cast<float>(kInN);
    const float fN = static_cast<float>(kInN);
    for (int n = 0; n < kInN; ++n) {
        const float fn = static_cast<float>(n);
        if (n == 0) {
            Win[n] = 0.0f;
        } else if (fn < eN) {
            const float Zp = eN * (1.0f / fn + 1.0f / (fn - eN));
            Win[n] = 1.0f / (1.0f + std::exp(Zp));
        } else if (fn <= fN - eN) {
            Win[n] = 1.0f;
        } else {
            const float m  = fN - fn;
            const float Zp = eN * (1.0f / m + 1.0f / (m - eN));
            Win[n] = 1.0f / (1.0f + std::exp(Zp));
        }
    }
}

TSpectralUpsampler::~TSpectralUpsampler()
{
    kiss_fftr_free(static_cast<kiss_fftr_cfg>(FwdCfg));
    kiss_fftr_free(static_cast<kiss_fftr_cfg>(InvCfg));
}

TProcessResult TSpectralUpsampler::Process(const float* in) const
{
    // 1. Apply Planck-taper window.
    std::vector<float> windowed(kInN);
    for (int n = 0; n < kInN; ++n)
        windowed[n] = in[n] * Win[n];

    // 2. Forward real FFT: kInN real → kInN/2+1 complex bins.
    constexpr int kInBins = kInN / 2 + 1;  // 257
    std::vector<kiss_fft_cpx> fwdOut(kInBins);
    kiss_fftr(static_cast<kiss_fftr_cfg>(FwdCfg), windowed.data(), fwdOut.data());

    // 2a. Filtered high-frequency energy ratio.
    double totalE = 0.0, filtHighE = 0.0;
    for (int k = 0; k <= kInN / 2; ++k) {
        const double e = static_cast<double>(fwdOut[k].r) * fwdOut[k].r
                       + static_cast<double>(fwdOut[k].i) * fwdOut[k].i;
        totalE += e;
        float H = 0.0f;
        if (LowCutBin == 0) {
            H = 1.0f;
        } else if (k >= LowCutBin + 2) {
            H = 1.0f;
        } else if (k >= LowCutBin) {
            const int i = k - LowCutBin + 1;
            H = 0.5f * (1.0f - std::cos(static_cast<float>(M_PI) * i / 2.0f));
        }
        filtHighE += e * H * H;
    }
    const float highFreqRatio = (totalE > 0.0)
                              ? static_cast<float>(filtHighE / totalE) : 0.0f;

    // 3. Build the kOutN/2+1 frequency-domain input for the inverse FFT.
    constexpr int kOutBins = kOutN / 2 + 1;  // 2049
    std::vector<kiss_fft_cpx> invIn(kOutBins, {0.0f, 0.0f});

    const float scale = static_cast<float>(kUpsample);

    // Full passband bins
    const int passbandStart = (LowCutBin == 0) ? 0 : LowCutBin + 2;
    for (int k = passbandStart; k < kInN / 2; ++k)
        invIn[k] = {fwdOut[k].r * scale, fwdOut[k].i * scale};

    // 3-bin raised-cosine transition
    if (LowCutBin > 0) {
        for (int i = 1; i < 3; ++i) {
            const int k = LowCutBin - 1 + i;
            if (k >= kInN / 2) continue;
            const float w = 0.5f * (1.0f - std::cos(static_cast<float>(M_PI) * i / 2.0f));
            invIn[k] = {fwdOut[k].r * scale * w, fwdOut[k].i * scale * w};
        }
    }

    // Nyquist bin
    if (LowCutBin + 2 <= kInN / 2)
        invIn[kInN / 2] = {fwdOut[kInN / 2].r * scale * 0.5f, 0.0f};

    // 4. Inverse real FFT: kOutBins complex → kOutN real.
    std::vector<float> output(kOutN);
    kiss_fftri(static_cast<kiss_fftr_cfg>(InvCfg), invIn.data(), output.data());

    // 5. Normalize
    const float norm = 1.0f / static_cast<float>(kOutN);
    for (float& v : output)
        v *= norm;

    return TProcessResult{std::move(output), highFreqRatio};
}

// ===== 性能优化：只计算highFreqRatio，跳过昂贵的4096点IFFT =====
float TSpectralUpsampler::CalcHighFreqRatio(const float* in) const
{
    // 1. Apply Planck-taper window.
    float windowed[kInN];
    for (int n = 0; n < kInN; ++n)
        windowed[n] = in[n] * Win[n];

    // 2. Forward real FFT: kInN real → kInN/2+1 complex bins.
    constexpr int kInBins = kInN / 2 + 1;
    kiss_fft_cpx fwdOut[kInBins];
    kiss_fftr(static_cast<kiss_fftr_cfg>(FwdCfg), windowed, fwdOut);

    // 3. Compute high-frequency energy ratio.
    double totalE = 0.0, filtHighE = 0.0;
    for (int k = 0; k <= kInN / 2; ++k) {
        const double e = static_cast<double>(fwdOut[k].r) * fwdOut[k].r
                       + static_cast<double>(fwdOut[k].i) * fwdOut[k].i;
        totalE += e;
        float H = 0.0f;
        if (LowCutBin == 0) {
            H = 1.0f;
        } else if (k >= LowCutBin + 2) {
            H = 1.0f;
        } else if (k >= LowCutBin) {
            const int i = k - LowCutBin + 1;
            H = 0.5f * (1.0f - std::cos(static_cast<float>(M_PI) * i / 2.0f));
        }
        filtHighE += e * H * H;
    }
    return (totalE > 0.0) ? static_cast<float>(filtHighE / totalE) : 0.0f;
}

} // namespace NAtracDEnc
