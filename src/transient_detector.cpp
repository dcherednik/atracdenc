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
#include <array>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <ostream>
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

std::vector<float> AnalyzeGain(const float* in, const uint32_t len, const uint32_t maxPoints, bool useRms,
                               std::vector<float>* subframeLow,
                               std::vector<float>* subframeHigh) {
    vector<float> res;
    const uint32_t step = len / maxPoints;
    if (subframeLow) {
        subframeLow->clear();
        subframeLow->reserve(maxPoints);
    }
    if (subframeHigh) {
        subframeHigh->clear();
        subframeHigh->reserve(maxPoints);
    }

    for (uint32_t pos = 0; pos < len; pos += step) {
        const float val = useRms ? calculateRMS(in + pos, step) : calculatePeak(in + pos, step);
        res.emplace_back(val);

        if (subframeLow || subframeHigh) {
            // Approximate within-subframe distribution by splitting each analysis
            // subframe into micro-chunks and taking a robust inter-quantile range.
            constexpr uint32_t kChunks = 8;
            const uint32_t chunkSz = std::max(1u, step / kChunks);
            std::vector<float> micro;
            micro.reserve((step + chunkSz - 1) / chunkSz);
            for (uint32_t off = 0; off < step; off += chunkSz) {
                const uint32_t n = std::min(chunkSz, step - off);
                const float m = useRms ? calculateRMS(in + pos + off, n)
                                       : calculatePeak(in + pos + off, n);
                micro.push_back(m);
            }
            std::sort(micro.begin(), micro.end());
            const size_t loIdx = micro.size() / 4;
            const size_t hiIdx = (micro.size() * 3) / 4;
            if (subframeLow)
                subframeLow->push_back(micro[loIdx]);
            if (subframeHigh)
                subframeHigh->push_back(micro[hiIdx]);
        }
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

template <size_t Radius>
static void MedianFilter(const std::vector<float>& in, std::vector<float>& out) {
    const int n = static_cast<int>(in.size());
    std::array<float, Radius * 2 + 1> w{};
    assert(out.size() == in.size());

    for (int i = 0; i < n; ++i) {
        const int lo = std::max(0, i - static_cast<int>(Radius));
        const int hi = std::min(n - 1, i + static_cast<int>(Radius));
        size_t wn = 0;
        for (int j = lo; j <= hi; ++j)
            w[wn++] = in[j];
        std::sort(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(wn));
        out[i] = w[wn / 2];
    }
}

// Find the maximum sustained amplitude level that persists for at least
// minContiguous consecutive subframes.  A 3-point median filter is applied
// first to reject isolated spikes.  Also detects whether the frame ends in
// a release (signal drops well below the plateau and does not recover).
struct TPlateauResult {
    float Level;        // 0 if no plateau found
    float MaxRaw;       // max of raw in[] (unfiltered)
    bool  ReleaseAtEnd; // true if signal decays after plateau by frame end
};

static TPlateauResult FindPlateau(const std::vector<float>& in, int minContiguous) {
    const int n = static_cast<int>(in.size());
    float maxRaw = 0.0f;
    for (int i = 0; i < n; ++i) maxRaw = std::max(maxRaw, in[i]);
    if (n < minContiguous)
        return {0.0f, maxRaw, false};

    // 3-point median filter: suppresses short local oscillations.
    std::vector<float> filtered(n);
    MedianFilter<1>(in, filtered);

    // Sliding window minimum of size minContiguous; take the maximum of those
    // minima.  This gives the highest level that is sustained for the required
    // number of consecutive subframes.
    float bestLevel = 0.0f;
    int   bestEnd   = -1;
    for (int j = 0; j + minContiguous <= n; ++j) {
        float minVal = filtered[j];
        for (int k = 1; k < minContiguous; ++k)
            minVal = std::min(minVal, filtered[j + k]);
        if (minVal > bestLevel) {
            bestLevel = minVal;
            bestEnd   = j + minContiguous - 1;
        }
    }

    if (bestLevel < 1e-6f)
        return {0.0f, maxRaw, false};

    // Extend plateau rightward while the filtered signal stays at plateau level
    while (bestEnd + 1 < n && filtered[bestEnd + 1] >= bestLevel)
        ++bestEnd;

    // Release detection: signal decays to near-silence by frame end.
    // Hard tail guard: if the last subframe is below 10% of the plateau, it is
    // unambiguously a release regardless of what the ring-down looks like in
    // between (a gradual ring-down can keep anyHighAfter=true and incorrectly
    // suppress release detection without this guard).
    // Soft tail: also trigger when the tail is below 50% of the plateau AND no
    // post-plateau subframe recovers above 70%.
    static constexpr float kHardTailRatio  = 0.1f;
    static constexpr float kReleaseHighTol  = 0.7f;
    static constexpr float kReleaseTailRatio = 0.5f;
    bool releaseAtEnd = false;
    if (bestEnd < n - 1) {
        if (in[n - 1] < bestLevel * kHardTailRatio) {
            releaseAtEnd = true;
        } else {
            bool anyHighAfter = false;
            for (int i = bestEnd + 1; i < n; ++i) {
                if (in[i] >= bestLevel * kReleaseHighTol) {
                    anyHighAfter = true;
                    break;
                }
            }
            releaseAtEnd = !anyHighAfter && (in[n - 1] < bestLevel * kReleaseTailRatio);
        }
    }

    return {bestLevel, maxRaw, releaseAtEnd};
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

// Transient score around a subframe boundary:
// R = max(max_right / max_left, max_left / max_right) over short windows.
// loc is the right-side start index of the boundary (1..n-1).
static float BoundaryTransientScore(const std::vector<float>& env, int loc, int win) {
    const int n = static_cast<int>(env.size());
    assert(loc > 0 && loc < n);
    const int leftStart = std::max(0, loc - win);
    const int leftEnd = loc;
    const int rightStart = loc;
    const int rightEnd = std::min(n, loc + win);

    float leftMax = 0.0f;
    for (int i = leftStart; i < leftEnd; ++i)
        leftMax = std::max(leftMax, env[i]);
    float rightMax = 0.0f;
    for (int i = rightStart; i < rightEnd; ++i)
        rightMax = std::max(rightMax, env[i]);

    static constexpr float kEps = 1e-9f;
    const float attack = (rightMax + kEps) / (leftMax + kEps);
    const float release = (leftMax + kEps) / (rightMax + kEps);
    return std::max(attack, release);
}

std::vector<TGainCurvePoint> CalcCurve(const std::vector<float>& in, TCurveBuilderCtx& ctx,
                                       std::optional<float> nextLevel,
                                       float minScore,
                                       std::ostream* yamlLog,
                                       const std::vector<float>* subframeLow,
                                       const std::vector<float>* subframeHigh) {
    std::vector<TGainCurvePoint> curve;

    if (in.empty())
        return curve;

    static constexpr int kMinPlateauLen = 3;
    const TPlateauResult plateau = FindPlateau(in, kMinPlateauLen);
    static constexpr float kMinPlateauFraction = 0.4f;
    const bool usePlateau = plateau.Level > 1e-6f
                            && !plateau.ReleaseAtEnd
                            && plateau.Level >= plateau.MaxRaw * kMinPlateauFraction;
    // For non-plateau frames always use the actual last subframe as the staircase
    // target: that is where the signal returns to within this frame.  nextLevel
    // (the lookahead first-subframe estimate) can be 6× higher than in.back() on
    // release frames, causing the staircase to wrongly amplify the tail region.
    const float target = usePlateau ? plateau.Level : in.back();
    if (yamlLog) {
        *yamlLog << std::fixed << std::setprecision(6)
                 << "        plateau_level: " << plateau.Level << "\n"
                 << "        plateau_max_raw: " << plateau.MaxRaw << "\n"
                 << "        plateau_release: " << (plateau.ReleaseAtEnd ? "true" : "false") << "\n"
                 << "        target: " << target
                 << "  # source: " << (usePlateau ? "plateau" : "in.back") << "\n";
    }

    const float savedLastLevel = ctx.LastLevel;
    const float savedLastTarget = ctx.LastTarget;
    ctx.LastLevel = in.back();
    ctx.LastTarget = target;

    if (target < 1e-6f)
        return curve;

    // No valid prior-frame context: skip curve emission on the first frame.
    if (savedLastLevel < 1e-6f)
        return curve;

    const int n = static_cast<int>(in.size());

    // Apply 3-point median filter to suppress short gain oscillations that would
    // otherwise produce spurious level transitions in the staircase scan.
    std::vector<float> filtered(n);
    MedianFilter<1>(in, filtered);

    float maxGain = 0.0f;
    for (float v : in)
        maxGain = std::max(maxGain, v);

    // Sticky quantisation is safe only in relatively stable regimes. Disable it
    // for deep release/transient shapes where preserving all transitions matters
    // more than suppressing +/-1 chatter.
    static constexpr float kStickyMaxIntraFrameRatio = 7.0f;  // max_gain / target
    static constexpr float kStickyMaxInterFrameRatio = 10.0f;  // prev_target / target
    const float intraRatio = maxGain / std::max(target, 1e-9f);
    float interRatio = 1.0f;
    if (savedLastTarget > 1e-6f) {
        const float hi = std::max(savedLastTarget, target);
        const float lo = std::min(savedLastTarget, target);
        interRatio = hi / std::max(lo, 1e-9f);
    }
    const bool stickyFrameEligible = subframeLow && subframeHigh
        && subframeLow->size() == in.size()
        && subframeHigh->size() == in.size()
        && intraRatio <= kStickyMaxIntraFrameRatio
        && interRatio <= kStickyMaxInterFrameRatio;

    if (yamlLog) {
        *yamlLog << std::fixed << std::setprecision(4)
                 << "        sticky_frame_eligible: " << (stickyFrameEligible ? "true" : "false") << "\n"
                 << "        sticky_intra_ratio: " << intraRatio << "\n"
                 << "        sticky_inter_ratio: " << interRatio << "\n";
    }

    // Per-subframe attenuation/amplification level relative to target.
    //
    // Optional range-aware sticky quantisation:
    // if centre quantisation toggles by +/-1 from previous level but the
    // subframe's [low, high] ratio band still supports the previous level,
    // keep previous. This suppresses long-run boundary chatter (e.g. 7<->8).
    std::vector<uint16_t> sfLevel(n);
    for (int i = 0; i < n; ++i) {
        const float ratioCenter = filtered[i] / target;
        uint16_t level = RelationToIdx(ratioCenter);
        if (i > 0 && stickyFrameEligible) {
            float ratioLo = (*subframeLow)[i] / target;
            float ratioHi = (*subframeHigh)[i] / target;
            if (ratioLo > ratioHi)
                std::swap(ratioLo, ratioHi);
            const uint16_t idxLo = RelationToIdx(ratioLo);
            const uint16_t idxHi = RelationToIdx(ratioHi);
            const uint16_t minIdx = std::min(idxLo, idxHi);
            const uint16_t maxIdx = std::max(idxLo, idxHi);
            const uint16_t prev = sfLevel[i - 1];
            const uint16_t idxSpan = maxIdx - minIdx;
            if (idxSpan <= 1u
                && std::abs(static_cast<int>(level) - static_cast<int>(prev)) == 1
                && prev >= minIdx && prev <= maxIdx) {
                level = prev;
            }
        }
        sfLevel[i] = level;
    }

    // Find targetSf: index of the first subframe (from the right) after the last
    // non-neutral subframe.  Everything at targetSf and beyond is at neutral —
    // no gain curve coverage needed there.
    // NOTE: the last subframe (sf = n-1) is always at neutral by design — the
    // ATRAC3 gain ramp always returns to neutral before the frame boundary.
    // Skipping sf = n-1 ensures targetSf <= n-1, keeping all emitted loc values
    // within the 5-bit field (0..31) and preventing bitstream overflow.
    int targetSf = 0;
    for (int sf = n - 2; sf >= 0; --sf) {
        if (sfLevel[sf] != 4u) {
            targetSf = sf + 1;
            break;
        }
    }

    // Entire frame is at neutral — nothing to encode.
    if (targetSf == 0)
        return curve;

    std::vector<float> boundaryScore(static_cast<size_t>(n + 1), 1.0f);
    static constexpr int kTransientScoreWindow = 3;
    for (int loc = 1; loc <= targetSf; ++loc)
        boundaryScore[static_cast<size_t>(loc)] =
            BoundaryTransientScore(filtered, loc, kTransientScoreWindow);
    if (yamlLog) {
        *yamlLog << std::fixed << std::setprecision(4)
                 << "        transient_min_score: " << minScore << "\n"
                 << "        transient_window: " << kTransientScoreWindow << "\n";
    }

    // Scan leftward from targetSf, collecting one curve point per level transition.
    // Adjacent subframes at the same level share a single point (no redundant points).
    // The point at loc=L covers the constant-level region ending just before L*LocSz.
    struct TTransition {
        int      Loc;    // ATRAC3 location index
        uint16_t Level;  // gain level at this point
        int      Delta;  // |level change| here, used for priority trimming
    };
    std::vector<TTransition> trans;
    {
        uint16_t prev = 4u;  // neutral anchor at targetSf
        for (int sf = targetSf - 1; sf >= 0; --sf) {
            const uint16_t lev = sfLevel[sf];
            if (lev != prev) {
                const int loc = sf + 1;
                const int delta = std::abs(static_cast<int>(lev) - static_cast<int>(prev));
                const float score = boundaryScore[static_cast<size_t>(loc)];

                // Keep strong level jumps regardless of score.
                // For +/-1 toggles, require transient evidence around the boundary.
                // Always keep the rightmost transition (loc==targetSf) so non-neutral
                // regions remain anchored to neutral at the frame tail.
                const bool keep = (loc == targetSf) || (delta >= 2) || (score >= minScore);
                if (keep) {
                    trans.push_back({loc, lev, delta});
                    prev = lev;
                } else if (yamlLog) {
                    *yamlLog << std::fixed << std::setprecision(4)
                             << "        transition_pruned: {loc: " << loc
                             << ", delta: " << delta
                             << ", score: " << score << "}\n";
                }
            }
        }
        std::reverse(trans.begin(), trans.end());
    }

    if (trans.empty())
        return curve;

    // Trim to point budget: keep transitions with the largest |DeltaLevel| first
    // (they correct the most severe MDCT energy mismatches).
    // Ties resolved by RIGHTMOST location: transitions near the frame-end attack
    // region bracket the actual transient and prevent deep-ATT points from being
    // orphaned with no adjacent return-to-neutral.  Sounds perceptually better
    // than leftmost tie-break even though regression metrics worsen — similar
    // to the band-2 gain case where metrics don't capture HF-specific quality.
    static constexpr int kMaxCurvePoints = 6;  // leave room for external point0
    if (static_cast<int>(trans.size()) > kMaxCurvePoints) {
        std::stable_sort(trans.begin(), trans.end(),
            [](const TTransition& a, const TTransition& b) {
                if (a.Delta != b.Delta)
                    return a.Delta > b.Delta;
                return a.Loc > b.Loc;
            });
        trans.resize(static_cast<size_t>(kMaxCurvePoints));
        std::sort(trans.begin(), trans.end(),
            [](const TTransition& a, const TTransition& b) {
                return a.Loc < b.Loc;
            });
    }

    curve.reserve(trans.size());
    for (const auto& t : trans)
        curve.push_back({t.Level, static_cast<uint32_t>(t.Loc)});

    return curve;
}

} //namespace NAtracDEnc
