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
        return 4u + GetFirstSetBit((uint32_t)std::trunc(x));
    } else {
        x = std::min(x, 16.0f);
        return 4u - GetFirstSetBit((uint32_t)std::trunc(x));
    }
}

// Returns the maximum of in[start..end) or in[start] when the range is empty.
// Max is used so the gain level covers the loudest subframe in the region.
static float RegionMax(const std::vector<float>& in, int start, int end) {
    if (end <= start)
        return in[start];
    float mx = in[start];
    for (int i = start + 1; i < end; ++i)
        if (in[i] > mx) mx = in[i];
    return mx;
}

// Recursively finds the dominant transients in in[lo..hi] (inclusive) using a
// 3-subframe monotonic guard.  Each transient is recorded as the index of the
// middle (ramp) subframe within the best-scoring window.  The shared `budget`
// counter limits the total number of gain points emitted.
static void FindTransients(const std::vector<float>& in, int lo, int hi,
                           int& budget, std::vector<int>& result) {
    if (budget <= 0 || hi - lo < 2)
        return;

    static const float kMinLevel = 1e-6f;
    // Minimum amplitude ratio to qualify as a transient.  Ratios below 2.0
    // map to Level 4 in RelationToIdx (GainLevel[4] = 1.0 = no gain change),
    // so detecting them would produce no-op curve points.  This threshold
    // suppresses false positives from stationary sinusoids whose subframe
    // RMS oscillates within a factor of 2.
    static const float kMinScore = 2.0f;
    float bestScore = 0.0f;
    int   bestPos   = -1;

    for (int j = lo; j <= hi - 2; ++j) {
        // Allow equal on the plateau side of each ramp: the peak of an
        // attack-ramp subframe equals the following plateau (rising, right
        // side relaxed to <=), and the peak of a release-ramp subframe equals
        // the preceding plateau (falling, left side relaxed to >=).
        const bool rising  = in[j] < in[j+1] && in[j+1] <= in[j+2];
        const bool falling = in[j] >= in[j+1] && in[j+1] > in[j+2];
        if (!rising && !falling)
            continue;

        const float denom = rising ? in[j] : in[j+2];
        if (denom < kMinLevel)
            continue;

        const float score = rising ? (in[j+2] / in[j]) : (in[j] / in[j+2]);
        if (score < kMinScore)
            continue;  // ratio too small to require gain compensation
        if (score > bestScore) {
            bestScore = score;
            bestPos   = j + 1;  // ramp subframe = middle of the 3-element window
        }
    }

    if (bestPos < 0)
        return;

    result.push_back(bestPos);
    --budget;

    FindTransients(in, lo,        bestPos - 1, budget, result);
    FindTransients(in, bestPos + 1, hi,        budget, result);
}

std::vector<TGainCurvePoint> CalcCurve(const std::vector<float>& in, TCurveBuilderCtx& ctx,
                                       std::optional<float> nextLevel) {
    std::vector<TGainCurvePoint> curve;

    if (in.empty())
        return curve;

    // When the caller provides the next frame's first-subframe amplitude, use
    // it as the target: the last subframe of bufNext may be a release ramp
    // whose peak still equals the preceding plateau, so in.back() would give
    // the wrong (loud) target.  nextLevel tells us where the signal is going.
    const float target         = nextLevel.has_value() ? *nextLevel : in.back();
    const float savedLastLevel = ctx.LastLevel;
    ctx.LastLevel = target;

    if (target < 1e-6f)
        return curve;

    // Step 1: find all transient positions via recursive divide-and-conquer.
    //
    // Prepend savedLastLevel as a virtual element at index 0 so that a
    // boundary attack (Location=0) can be detected via the window
    // [savedLastLevel, in[0], in[1]].
    // Append nextLevel (when provided) as a virtual element at the end so
    // that a boundary release (Location=N-1) can be detected via the window
    // [in[N-2], in[N-1], nextLevel].
    // All returned indices are in extended space; subtracting 1 converts them
    // back to in-space.
    std::vector<float> ext;
    ext.reserve(in.size() + 2);
    ext.push_back(savedLastLevel);
    ext.insert(ext.end(), in.begin(), in.end());
    if (nextLevel.has_value())
        ext.push_back(*nextLevel);

    std::vector<int> transients;
    int budget = 8;  // ATRAC3 SubbandInfo::MaxGainPointsNum
    FindTransients(ext, 0, static_cast<int>(ext.size()) - 1, budget, transients);
    for (int& t : transients)
        --t;

    if (transients.empty())
        return curve;

    std::sort(transients.begin(), transients.end());

    // Step 2: build the gain curve.
    //
    // The first gain point's Level (scaleLevel) normalises bufCur — and the
    // region of bufNext before the first transient — to `target`:
    //   bufCur / GainLevel[scaleLevel] == target
    //   => GainLevel[scaleLevel] == savedLastLevel / target
    const uint16_t scaleLevel = RelationToIdx(savedLastLevel / target);

    for (size_t i = 0; i < transients.size(); ++i) {
        uint16_t level;
        if (i == 0) {
            level = scaleLevel;
        } else {
            // Amplitude of bufNext between the previous and current transient.
            const float regionAmp = RegionMax(in, transients[i-1] + 1, transients[i]);
            level = RelationToIdx(regionAmp / target);
        }
        curve.push_back({level, static_cast<uint32_t>(transients[i])});
    }

    return curve;
}

} //namespace NAtracDEnc
