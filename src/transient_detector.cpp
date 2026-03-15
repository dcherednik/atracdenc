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

#include "transient_detector.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <cassert>
namespace NAtracDEnc {

using std::vector;
static float calculateRMS(const float* in, uint32_t n) {
    float s = 0;
    for (uint32_t i = 0; i < n; i++) {
        s += (in[i] * in[i]);
    }
    s /= n;
    return sqrt(s);
}

static float calculatePeak(const float* in, uint32_t n) {
    float s = 0;
    for (uint32_t i = 0; i < n; i++) {
        float absVal = std::abs(in[i]);
        if (absVal > s)
            s = absVal;
    }
    return s;
}

void TTransientDetector::HPFilter(const float* in, float* out) {
    static const float fircoef[] = {
        -8.65163e-18 * 2.0, -0.00851586 * 2.0, -6.74764e-18 * 2.0, 0.0209036 * 2.0,
        -3.36639e-17 * 2.0, -0.0438162 * 2.0, -1.54175e-17 * 2.0, 0.0931738 * 2.0,
        -5.52212e-17 * 2.0, -0.313819 * 2.0
    };
    memcpy(HPFBuffer.data() + PrevBufSz, in, BlockSz * sizeof(float));
    const float* inBuf = HPFBuffer.data();
    for (size_t i = 0; i < BlockSz; ++i) {
        float s = inBuf[i + 10];
        float s2 = 0;
        for (size_t j = 0; j < ((FIRLen - 1) / 2) - 1 ; j += 2) {
            s += fircoef[j] * (inBuf[i + j] + inBuf[i + FIRLen - j]);
            s2 += fircoef[j + 1] * (inBuf[i + j + 1] + inBuf[i + FIRLen - j - 1]);
        }
        out[i] = (s + s2)/2;
    }
    memcpy(HPFBuffer.data(), in + (BlockSz - PrevBufSz),  PrevBufSz * sizeof(float));
}


bool TTransientDetector::Detect(const float* buf) {
    const uint16_t nBlocksToAnalize = NShortBlocks + 1;
    float* rmsPerShortBlock = reinterpret_cast<float*>(alloca(sizeof(float) * nBlocksToAnalize));
    std::vector<float> filtered(BlockSz);
    HPFilter(buf, filtered.data());
    bool trans = false;
    rmsPerShortBlock[0] = LastEnergy;
    for (uint16_t i = 1; i < nBlocksToAnalize; ++i) {
        rmsPerShortBlock[i] = 19.0 * log10(calculateRMS(&filtered[(size_t)(i - 1) * ShortSz], ShortSz));
        if (rmsPerShortBlock[i] - rmsPerShortBlock[i - 1] > 16) {
            trans = true;
            LastTransientPos = i;
        }
        if (rmsPerShortBlock[i - 1] - rmsPerShortBlock[i] > 20) {
            trans = true;
            LastTransientPos = i;
        }
    }
    LastEnergy = rmsPerShortBlock[NShortBlocks];
    return trans;
}

std::vector<float> AnalyzeGain(const float* in, const uint32_t len, const uint32_t maxPoints, bool useRms) {
    vector<float> res;
    const uint32_t step = len / maxPoints;
    for (uint32_t pos = 0; pos < len; pos += step) {
        float rms = useRms ? calculateRMS(in + pos, step) : calculatePeak(in + pos, step);
        res.emplace_back(rms);
    }
    return res;
}

// Maps amplitude ratio x = region_rms / target to ATRAC3 gain Level index L such
// that GainLevel[L] = 2^(4-L) ≈ x.  Dividing the signal by GainLevel[L] normalises
// it to the target amplitude.
static uint16_t RelationToIdx(float x) {
    if (x <= 0.5f) {
        x = 1.0f / std::max(x, 0.00048828125f);
        return 4u + GetFirstSetBit(static_cast<uint32_t>(x));
    } else {
        x = std::min(x, 16.0f);
        return 4u - GetFirstSetBit(static_cast<uint32_t>(x));
    }
}

// Returns the RMS of in[start..end) or in[start] when the range is empty.
// RMS gives a smoother energy estimate than max, reducing over-attenuation
// from brief loudness spikes within a region.
static float RegionRMS(const std::vector<float>& in, int start, int end) {
    if (end <= start)
        return in[start];
    float sum = 0.0f;
    for (int i = start; i < end; ++i)
        sum += in[i] * in[i];
    return std::sqrt(sum / (end - start));
}

std::vector<TGainCurvePoint> CalcCurve(const std::vector<float>& in, TCurveBuilderCtx& ctx,
                                       std::optional<float> nextLevel,
                                       float minScore) {
    std::vector<TGainCurvePoint> curve;

    if (in.empty())
        return curve;

    // When the caller provides the next frame's first-subframe amplitude, use
    // it as the target: the last subframe of bufNext may be a release ramp
    // whose peak still equals the preceding plateau, so in.back() would give
    // the wrong (loud) target.  nextLevel tells us where the signal is going.
    const float target         = nextLevel.has_value() ? *nextLevel : in.back();
    const float savedLastLevel = ctx.LastLevel;
    // Store in.back() as the next call's savedLastLevel instead of target (nextLevel).
    // Both in.back() (last analysis subframe of this call) and gain[0] of the next
    // call (first analysis subframe of the next frame) are measured in the analysis
    // domain and represent adjacent time blocks.  Using nextLevel here would make
    // savedLastLevel a lookahead-domain estimate of the same samples that gain[0]
    // measures in the analysis domain — a different FFT context for the same 8
    // samples, which can produce false boundary transients.
    ctx.LastLevel = in.back();

    if (target < 1e-6f)
        return curve;

    // No valid prior-frame context: record the context for next frame but do not
    // emit a curve.  A zero savedLastLevel would produce scaleLevel=15 (÷2048),
    // causing extreme amplification in the gain modulator.
    if (savedLastLevel < 1e-6f)
        return curve;

    static const int kMaxTransientPoints = 6;  // keep space for explicit point 0
    const std::vector<int> transients =
        DetectTransients(in, savedLastLevel, nextLevel, minScore, kMaxTransientPoints);

    if (transients.empty())
        return curve;

    // Build the gain curve in location order.
    curve.reserve(transients.size());
    for (int i = 0; i < static_cast<int>(transients.size()); ++i) {
        const int loc = transients[i];
        const int prevLoc = (i > 0) ? transients[i - 1] : -1;
        const float regionAmp = RegionRMS(in, prevLoc + 1, loc);
        const uint16_t level = RelationToIdx(regionAmp / target);
        curve.push_back({level, static_cast<uint32_t>(loc)});
    }

    return curve;
}

std::vector<int> DetectTransients(const std::vector<float>& in, float savedLastLevel,
                                  std::optional<float> nextLevel,
                                  float minScore, int maxPoints) {
    std::vector<int> out;
    if (in.empty())
        return out;

    static const float kMinLevel = 1e-6f;

    std::vector<float> ext;
    ext.reserve(in.size() + 2);
    ext.push_back(savedLastLevel);
    ext.insert(ext.end(), in.begin(), in.end());
    if (nextLevel.has_value())
        ext.push_back(*nextLevel);

    struct TCandidate {
        int Location;
        float Score;
    };
    std::vector<TCandidate> candidates;
    candidates.reserve(ext.size());

    for (size_t j = 0; j + 2 < ext.size(); ++j) {
        const float a = ext[j];
        const float b = ext[j + 1];
        const float c = ext[j + 2];

        const bool rising  = a < b && b <= c;
        const bool falling = a >= b && b > c;
        if (!rising && !falling)
            continue;

        const float denom = rising ? a : c;
        if (denom < kMinLevel)
            continue;

        const float score = rising ? (c / denom) : (a / denom);
        if (score < minScore)
            continue;

        // ext[0]=savedLastLevel, so ext[j+1]=in[j]: the peak (middle of the triplet)
        // is at in[j].  We record loc=j as the gain curve location for that peak.
        const int loc = static_cast<int>(j);
        if (loc >= static_cast<int>(in.size()))
            continue;

        candidates.push_back({loc, score});
    }

    if (candidates.empty())
        return out;

    std::sort(candidates.begin(), candidates.end(),
              [](const TCandidate& a, const TCandidate& b) {
                  if (a.Score != b.Score)
                      return a.Score > b.Score;
                  return a.Location < b.Location;
              });
    if (candidates.size() > static_cast<size_t>(maxPoints))
        candidates.resize(static_cast<size_t>(maxPoints));

    std::sort(candidates.begin(), candidates.end(),
              [](const TCandidate& a, const TCandidate& b) {
                  return a.Location < b.Location;
              });

    out.reserve(candidates.size());
    for (const auto& c : candidates)
        out.push_back(c.Location);
    return out;
}

} //namespace NAtracDEnc
