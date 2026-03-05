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

#include <vector>

namespace NAtracDEnc {

// Result of TSpectralUpsampler::Process().
struct TProcessResult {
    // kOutN upsampled samples; analysis region maps to [1024..3072).
    std::vector<float> signal;

    // Fraction of spectral energy in bins [LowCutBin, kInN/2] relative to
    // total spectral energy, computed from the Planck-windowed input FFT
    // *before* the HPF is applied.  Range [0, 1].
    //   ≈ 0 : frame is dominated by sub-cutoff content (stopband leakage only)
    //   ≈ 1 : frame is dominated by supra-cutoff content (full passband)
    float highFreqRatio;
};

// Preprocesses a 512-sample context window for improved spectral analysis.
//
// Input layout (512 samples total):
//   [0  .. 127] : tail of the previous analysis frame
//   [128.. 383] : current analysis region (will be passed to AnalyzeGain)
//   [384.. 511] : look-ahead into the next frame
//
// Processing steps:
//   1. Apply a Planck-taper window with parameter epsilon across all 512
//      samples.  The flat top (w=1) covers n in [ε·N, N·(1-ε)], which
//      fully contains the analysis region [128..384) for ε ≤ 0.25.
//   2. Forward real FFT (512-point).
//   3. Low-cut filter: zero all bins below lowCutHz (high-pass).
//   4. 8× upsample in the FFT domain by zero-padding the high-frequency
//      region and scaling remaining bins to preserve amplitude.
//   5. Inverse real FFT (4096-point).
//
// The analysis region maps to [1024..3071] in the 4096-sample output.
// Passing each kInN/kUpsample-sample subframe of this region to AnalyzeGain
// gives 8× more stable RMS estimates than operating on the raw 8-sample
// subframes, which helps suppress false transient detections for low-
// frequency and chirp-like signals.
class TSpectralUpsampler {
public:
    static constexpr int   kInN        = 512;               // input window size
    static constexpr int   kUpsample   = 8;                 // upsample factor
    static constexpr int   kOutN       = kInN * kUpsample;  // 4096
    static constexpr float kDefaultEps = 0.15f;             // Planck-taper ε

    // Minimum TProcessResult::highFreqRatio to consider gain-curve analysis
    // meaningful.  Frames below this threshold are dominated by sub-cutoff
    // leakage; the filtered output is at the noise floor and CalcCurve should
    // be skipped (set ctx.LastLevel = 0 and continue).
    // 5 % accounts for main-lobe spectral leakage from tones just below the
    // cutoff into the 3-bin transition region (H² = 0.25 at LowCutBin).
    static constexpr float kHighFreqThreshold = 0.05f;

    // sampleRate : sample rate of the input (e.g. 11024 Hz for ATRAC3 sub-band)
    // lowCutHz   : frequencies below this are zeroed in the FFT domain
    // epsilon    : Planck-taper taper fraction in (0, 0.5); default 0.15
    TSpectralUpsampler(float sampleRate, float lowCutHz, float epsilon = kDefaultEps);
    ~TSpectralUpsampler();

    // Process a kInN-sample input window.
    // Returns the upsampled signal and its high-frequency energy ratio.
    TProcessResult Process(const float* in) const;

private:
    const int          LowCutBin;  // first kept bin (inclusive); bins [0,LowCutBin) are zeroed
    std::vector<float> Win;
    void*              FwdCfg;     // kiss_fftr_cfg: kInN-point  forward real FFT plan
    void*              InvCfg;     // kiss_fftr_cfg: kOutN-point inverse real FFT plan
};

} // namespace NAtracDEnc
