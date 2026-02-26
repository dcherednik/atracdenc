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

/*
 * Unit tests for TGainProcessor::Modulate and TGainProcessor::Demodulate.
 *
 * Mirror relationship:
 *   Modulate(gi) attenuates signal samples by the gain envelope before MDCT.
 *   Demodulate(giNow, giNext) re-amplifies them during IMDCT overlap-add.
 *
 * Direct algebraic mirror (without MDCT, per-sample):
 *   After Modulate(gi) applied to (B_cur, B_next):
 *     bufCur_mod[pos] = B_cur[pos] / scale          (all positions)
 *     bufNext_mod[pos] = B_next[pos] / level(pos)   (constant + transition)
 *     bufNext_mod[pos] = B_next[pos]                (remainder, Modulate leaves untouched)
 *
 *   Then Demodulate(giNow=gi, giNext=gi)(out, cur=bufNext_mod, prev=bufCur_mod):
 *     Constant region:  out = (B_next/level * scale + B_cur/scale) * level
 *                           = B_next*scale + B_cur*(level/scale)
 *                           = B_next*scale + B_cur   (when scale == level)
 *     Remainder region: out = B_next*scale + B_cur/scale
 *                           (bufNext untouched by Modulate, but Demodulate scales it)
 *     Neutral gain (scale=level=1): both reduce to B_next + B_cur (simple overlap-add)
 */

#define ATRAC_UT_PUBLIC

#include "atrac3denc.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using std::vector;
using namespace NAtracDEnc;
using namespace NAtrac3;

// Convenience aliases
using TAtrac3GP = TAtrac3MDCT::TAtrac3GainProcessor;
using TGP = TAtrac3Data::SubbandInfo::TGainPoint;

// Returns GainLevel[L] = 2^(ExponentOffset - L) = 2^(4 - L)
static float GainLevelAt(uint32_t L) {
    return std::pow(2.0f, static_cast<float>(TAtrac3Data::ExponentOffset) - static_cast<float>(L));
}

// ============================================================================
// Modulate tests
// ============================================================================

// Empty gain info must return a null (falsy) lambda — no attenuation needed.
TEST(TGainProcessor_Modulate, EmptyGain_ReturnsNullOp) {
    TAtrac3GP gp;
    vector<TGP> empty;
    auto fn = gp.Modulate(empty);
    EXPECT_FALSE(fn);
}

// bufCur must be uniformly divided by scale at every sample position.
// scale = GainLevel[Level]; Location merely controls where transition begins.
TEST(TGainProcessor_Modulate, BufCur_AllPositions_DividedByScale) {
    TAtrac3GP gp;
    // Level=2 -> scale = 2^(4-2) = 4.0; Location=31 -> lastPos=248,
    // transition [248,256). All 256 bufCur positions fall under the scale rule.
    vector<TGP> gi = {{2, 31}};
    auto fn = gp.Modulate(gi);
    ASSERT_TRUE(fn);

    const float input = 8.0f;
    vector<float> bufCur(256, input);
    vector<float> bufNext(256, 1.0f);
    fn(bufCur.data(), bufNext.data());

    const float scale = GainLevelAt(2); // 4.0
    for (int i = 0; i < 256; ++i)
        EXPECT_NEAR(bufCur[i], input / scale, 1e-6f) << "bufCur at pos=" << i;
}

// bufNext must be divided by level in the constant region [0, lastPos).
TEST(TGainProcessor_Modulate, BufNext_ConstantRegion_DividedByLevel) {
    TAtrac3GP gp;
    // Level=2 -> level=4.0; Location=31 -> constant region [0, 248).
    vector<TGP> gi = {{2, 31}};
    auto fn = gp.Modulate(gi);
    ASSERT_TRUE(fn);

    const float input = 8.0f;
    vector<float> bufCur(256, 1.0f);
    vector<float> bufNext(256, input);
    fn(bufCur.data(), bufNext.data());

    const float level = GainLevelAt(2); // 4.0
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(bufNext[i], input / level, 1e-6f) << "bufNext at pos=" << i;
}

// bufNext must NOT be modified in the remainder region [lastPos+LocSz, 256).
// The gain has already decayed to 1.0 there; only bufCur keeps its /scale rule.
TEST(TGainProcessor_Modulate, BufNext_Remainder_Unchanged) {
    TAtrac3GP gp;
    // Location=4 -> lastPos=32, LocSz=8 -> transition [32,40), remainder [40,256).
    vector<TGP> gi = {{2, 4}};
    auto fn = gp.Modulate(gi);
    ASSERT_TRUE(fn);

    const float sentinel = 7.77f;
    vector<float> bufCur(256, 1.0f);
    vector<float> bufNext(256, sentinel);
    fn(bufCur.data(), bufNext.data());

    for (int i = 40; i < 256; ++i)
        EXPECT_NEAR(bufNext[i], sentinel, 1e-6f) << "bufNext at pos=" << i;
}

// bufCur must still be divided by scale even in the remainder region.
TEST(TGainProcessor_Modulate, BufCur_Remainder_StillDividedByScale) {
    TAtrac3GP gp;
    // Location=4 -> remainder [40,256); scale=4.
    vector<TGP> gi = {{2, 4}};
    auto fn = gp.Modulate(gi);
    ASSERT_TRUE(fn);

    const float input = 12.0f;
    vector<float> bufCur(256, input);
    vector<float> bufNext(256, 1.0f);
    fn(bufCur.data(), bufNext.data());

    const float scale = GainLevelAt(2); // 4.0
    for (int i = 40; i < 256; ++i)
        EXPECT_NEAR(bufCur[i], input / scale, 1e-6f) << "bufCur at pos=" << i;
}

// ============================================================================
// Demodulate tests
// ============================================================================

// With both giNow and giNext empty: scale=1, no level -> simple overlap-add.
TEST(TGainProcessor_Demodulate, BothEmpty_SimpleOverlapAdd) {
    TAtrac3GP gp;
    vector<TGP> empty;
    auto fn = gp.Demodulate(empty, empty);

    vector<float> out(256);
    vector<float> cur(256, 3.0f);
    vector<float> prev(256, 5.0f);
    fn(out.data(), cur.data(), prev.data());

    // out[pos] = cur * 1 + prev = 8.0
    for (int i = 0; i < 256; ++i)
        EXPECT_NEAR(out[i], 8.0f, 1e-6f) << "at pos=" << i;
}

// scale is taken from giNext[0].Level; giNow empty means no level envelope.
TEST(TGainProcessor_Demodulate, ScaleFromGiNext_Applied) {
    TAtrac3GP gp;
    vector<TGP> empty;
    // Level=2 -> scale=4; location irrelevant for scale extraction
    vector<TGP> giNext = {{2, 0}};
    auto fn = gp.Demodulate(empty, giNext);

    vector<float> out(256);
    vector<float> cur(256, 3.0f);
    vector<float> prev(256, 5.0f);
    fn(out.data(), cur.data(), prev.data());

    // out[pos] = cur * 4 + prev = 12 + 5 = 17
    for (int i = 0; i < 256; ++i)
        EXPECT_NEAR(out[i], 17.0f, 1e-6f) << "at pos=" << i;
}

// In the constant region [0, lastPos) the level from giNow multiplies the
// whole overlap-add result.
TEST(TGainProcessor_Demodulate, GainNow_ConstantRegion_LevelApplied) {
    TAtrac3GP gp;
    // Level=2 -> level=4; Location=31 -> constant region [0, 248).
    vector<TGP> giNow = {{2, 31}};
    vector<TGP> empty;
    auto fn = gp.Demodulate(giNow, empty);

    vector<float> out(256);
    const float cur_val = 2.0f, prev_val = 1.0f;
    vector<float> cur(256, cur_val);
    vector<float> prev(256, prev_val);
    fn(out.data(), cur.data(), prev.data());

    // scale=1 (empty giNext), level=4: out = (cur*1 + prev)*4 = (2+1)*4 = 12
    const float level = GainLevelAt(2);
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(out[i], (cur_val + prev_val) * level, 1e-5f) << "at pos=" << i;
}

// In the remainder [lastPos+LocSz, MDCTSz/2) there is no level multiplication;
// only scale from giNext is active.
TEST(TGainProcessor_Demodulate, GainNow_Remainder_NoLevelMultiplication) {
    TAtrac3GP gp;
    // Location=4 -> remainder [40, 256).
    vector<TGP> giNow = {{2, 4}};
    vector<TGP> empty;
    auto fn = gp.Demodulate(giNow, empty);

    vector<float> out(256);
    vector<float> cur(256, 2.0f);
    vector<float> prev(256, 3.0f);
    fn(out.data(), cur.data(), prev.data());

    // scale=1 (empty giNext), remainder: out = cur*1 + prev = 5.0 (no level)
    for (int i = 40; i < 256; ++i)
        EXPECT_NEAR(out[i], 5.0f, 1e-6f) << "at pos=" << i;
}

// Both giNow and giNext can be non-empty simultaneously; scale and level are
// applied together.
TEST(TGainProcessor_Demodulate, BothNonEmpty_ScaleAndLevelCombined) {
    TAtrac3GP gp;
    // giNow Level=2 -> level=4 in constant [0,248); giNext Level=1 -> scale=8.
    vector<TGP> giNow = {{2, 31}};
    vector<TGP> giNext = {{1, 0}};
    auto fn = gp.Demodulate(giNow, giNext);

    vector<float> out(256);
    vector<float> cur(256, 2.0f);
    vector<float> prev(256, 1.0f);
    fn(out.data(), cur.data(), prev.data());

    // Constant [0,248): out = (cur*8 + prev)*4 = (16+1)*4 = 68
    const float scale = GainLevelAt(1); // 8.0
    const float level = GainLevelAt(2); // 4.0
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(out[i], (2.0f * scale + 1.0f) * level, 1e-5f) << "at pos=" << i;
}

// ============================================================================
// Mirror tests: algebraic inverse composition Demodulate(Modulate(x))
// ============================================================================

// Neutral gain (Level = ExponentOffset = 4 -> scale = level = 1.0):
// Modulate divides by 1 (no-op numerically), Demodulate multiplies by 1.
// Result should equal the plain overlap-add: B_next + B_cur.
TEST(TGainProcessor_Mirror, NeutralGain_EqualsSimpleOverlapAdd) {
    TAtrac3GP gp;
    // Level=4 -> GainLevel[4] = 2^(4-4) = 1.0; Location=31 -> whole buffer constant.
    vector<TGP> gi = {{4, 31}};

    const float B_cur_val = 3.0f, B_next_val = 5.0f;
    vector<float> bufCur(256, B_cur_val);
    vector<float> bufNext(256, B_next_val);

    auto modFn = gp.Modulate(gi);
    ASSERT_TRUE(modFn);
    modFn(bufCur.data(), bufNext.data());

    vector<float> out(256);
    auto demodFn = gp.Demodulate(gi, gi);
    demodFn(out.data(), bufNext.data(), bufCur.data());

    // scale = level = 1: out = B_next*1 + B_cur*(1/1) = B_next + B_cur
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(out[i], B_next_val + B_cur_val, 1e-5f) << "at pos=" << i;
}

// Constant region mirror: out = B_next * scale + B_cur * (level / scale).
// When scale == level this simplifies to B_next * scale + B_cur.
TEST(TGainProcessor_Mirror, ConstantRegion_AlgebraicIdentity) {
    TAtrac3GP gp;
    // Level=2 -> scale = level = 4.0; Location=31 -> constant region [0,248).
    vector<TGP> gi = {{2, 31}};
    const float scale = GainLevelAt(2); // 4.0

    const float B_cur_val = 4.0f, B_next_val = 8.0f;
    vector<float> bufCur(256, B_cur_val);
    vector<float> bufNext(256, B_next_val);

    auto modFn = gp.Modulate(gi);
    modFn(bufCur.data(), bufNext.data());
    // After Modulate: bufCur[*]=1.0, bufNext[0..247]=2.0

    vector<float> out(256);
    auto demodFn = gp.Demodulate(gi, gi);
    // cur = bufNext_mod, prev = bufCur_mod
    demodFn(out.data(), bufNext.data(), bufCur.data());

    // Constant [0,248):
    //   out = (bufNext_mod * scale + bufCur_mod) * level
    //       = (B_next/level * scale + B_cur/scale) * level
    //       = B_next * scale + B_cur * level/scale
    //       = B_next * scale + B_cur            (scale == level)
    const float expected = B_next_val * scale + B_cur_val;  // 8*4 + 4 = 36
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(out[i], expected, 1e-5f) << "at pos=" << i;
}

// Remainder region mirror: Modulate leaves bufNext untouched but Demodulate
// still applies scale to it. bufCur was divided by scale in Modulate.
// out = B_next * scale + B_cur / scale.
TEST(TGainProcessor_Mirror, RemainderRegion_AlgebraicIdentity) {
    TAtrac3GP gp;
    // Location=4 -> lastPos=32, LocSz=8, remainder [40,256).
    // Level=2 -> scale=4.
    vector<TGP> gi = {{2, 4}};
    const float scale = GainLevelAt(2); // 4.0

    const float B_cur_val = 8.0f, B_next_val = 4.0f;
    vector<float> bufCur(256, B_cur_val);
    vector<float> bufNext(256, B_next_val);

    auto modFn = gp.Modulate(gi);
    modFn(bufCur.data(), bufNext.data());
    // Remainder: bufCur[40..255] = 8/4 = 2.0; bufNext[40..255] = 4.0 (unchanged)

    vector<float> out(256);
    auto demodFn = gp.Demodulate(gi, gi);
    demodFn(out.data(), bufNext.data(), bufCur.data());

    // Remainder [40,256):
    //   out = cur * scale + prev
    //       = bufNext_mod * scale + bufCur_mod
    //       = B_next * scale + B_cur / scale
    const float expected = B_next_val * scale + B_cur_val / scale;  // 4*4 + 8/4 = 18
    for (int i = 40; i < 256; ++i)
        EXPECT_NEAR(out[i], expected, 1e-5f) << "at pos=" << i;
}

// Two-point gain: verify the constant region before the first point and the
// region between two points both satisfy the mirror identity.
// gi = [{Level=0, loc=4}, {Level=2, loc=20}]
//   scale         = GainLevel[0] = 16.0
//   point 0 level = GainLevel[0] = 16.0, constant [0, 32)
//   point 1 level = GainLevel[2] =  4.0, constant [32+8, 160) = [40, 160)
//   transition between point 0 and point 1: [32, 40)
//   transition from point 1 to neutral:     [160, 168)
//   remainder:                              [168, 256)
TEST(TGainProcessor_Mirror, TwoPoints_ConstantSegmentsIdentity) {
    TAtrac3GP gp;
    vector<TGP> gi = {{0, 4}, {2, 20}};
    const float scale = GainLevelAt(0); // 16.0

    const float B_cur_val = 16.0f, B_next_val = 8.0f;
    vector<float> bufCur(256, B_cur_val);
    vector<float> bufNext(256, B_next_val);

    auto modFn = gp.Modulate(gi);
    modFn(bufCur.data(), bufNext.data());

    vector<float> out(256);
    auto demodFn = gp.Demodulate(gi, gi);
    demodFn(out.data(), bufNext.data(), bufCur.data());

    // First constant region [0, 32): level = GainLevel[0] = 16 = scale
    // out = B_next * scale + B_cur * level/scale = B_next * scale + B_cur
    {
        const float lev0 = GainLevelAt(0); // 16.0
        const float expected = B_next_val * scale + B_cur_val * lev0 / scale; // 8*16 + 16 = 144
        for (int i = 0; i < 32; ++i)
            EXPECT_NEAR(out[i], expected, 1e-4f) << "first constant at pos=" << i;
    }

    // Second constant region [40, 160): level = GainLevel[2] = 4
    // out = B_next * scale + B_cur * level/scale = 8*16 + 16*(4/16) = 128 + 4 = 132
    {
        const float lev1 = GainLevelAt(2); // 4.0
        const float expected = B_next_val * scale + B_cur_val * lev1 / scale; // 128 + 4 = 132
        for (int i = 40; i < 160; ++i)
            EXPECT_NEAR(out[i], expected, 1e-4f) << "second constant at pos=" << i;
    }

    // Remainder [168, 256): out = B_next * scale + B_cur / scale = 128 + 1 = 129
    {
        const float expected = B_next_val * scale + B_cur_val / scale; // 8*16 + 16/16 = 129
        for (int i = 168; i < 256; ++i)
            EXPECT_NEAR(out[i], expected, 1e-4f) << "remainder at pos=" << i;
    }
}

// ============================================================================
// Frequency-domain test: gain modulation reduces spectral energy
// ============================================================================

// ---------------------------------------------------------------------------
// Optional gnuplot visualisation.
//
// Set the environment variable ATRAC_GAIN_GNUPLOT to any non-empty value
// before running the tests to open an interactive gnuplot window showing MDCT
// bin energy with and without gain modulation, e.g.:
//
//   ATRAC_GAIN_GNUPLOT=1 ./gain_processor_ut
//
// Requires gnuplot to be installed and on PATH.  Each test opens its own
// persistent window; close it or press Ctrl-C when done.
// ---------------------------------------------------------------------------
static void MaybePlotMdctEnergy(const char*          title,
                                const vector<float>& specs_nomod,
                                const vector<float>& specs_mod,
                                int                  kHfStart)
{
    if (!std::getenv("ATRAC_GAIN_GNUPLOT"))
        return;

    FILE* gp = popen("gnuplot -persistent", "w");
    if (!gp) {
        std::fprintf(stderr, "[gnuplot] popen failed – is gnuplot installed?\n");
        return;
    }

    std::fprintf(gp, "set title '%s'\n", title);
    std::fprintf(gp, "set xlabel 'MDCT bin'\n");
    std::fprintf(gp, "set ylabel 'Energy (coeff^{2})'\n");
    std::fprintf(gp, "set logscale y\n");
    std::fprintf(gp, "set grid\n");
    std::fprintf(gp, "set key top right\n");
    // Vertical dashed line at the HF leakage boundary
    std::fprintf(gp,
        "set arrow from %d, graph 0 to %d, graph 1 "
        "nohead lc rgb 'red' lw 1 dt 2\n", kHfStart, kHfStart);
    std::fprintf(gp,
        "set label 'HF start (bin %d)' at %d, graph 0.08 "
        "left offset 0.5,0 tc rgb 'red' font ',9'\n", kHfStart, kHfStart);

    // Two inline datasets: nomod first, then mod.
    std::fprintf(gp,
        "plot '-' with lines lw 2 lc rgb '#0060c0' title 'no modulation', "
             "'-' with lines lw 2 lc rgb '#c04000' title 'with modulation'\n");

    for (int k = 0; k < 256; ++k)
        std::fprintf(gp, "%d %g\n", k, specs_nomod[k] * specs_nomod[k]);
    std::fprintf(gp, "e\n");

    for (int k = 0; k < 256; ++k)
        std::fprintf(gp, "%d %g\n", k, specs_mod[k] * specs_mod[k]);
    std::fprintf(gp, "e\n");

    pclose(gp);
}

/*
 * Frequency-domain test: gain modulation eliminates spectral leakage from a
 * frame-boundary amplitude step (quiet → loud).
 *
 * Naïve approach (Level=1, scale=8, attenuation): divides bufCur from 1→0.125
 * and bufNext from 8→1.0.  A step of magnitude 0.875 remains at the boundary;
 * leakage is reduced but NOT eliminated.
 *
 * Correct approach (Level=7, scale=0.125, amplification):
 *   GainLevel[7] = 2^(4-7) = 0.125  →  dividing by 0.125 amplifies by 8.
 *   bufCur (quiet, A=1) ×8 = 8 → matches A_loud.
 *   bufNext[0..7] is pre-shaped with the gain-interpolation ramp so that the
 *   modulation cancels exactly (signal[k] = A_quiet×gainInc^k, level=0.125×gainInc^k,
 *   modulated = 8×sin uniformly).
 *   bufNext[8..255] = A_loud = 8 (UNTOUCHED remainder) → 8×sin. ✓
 *
 * The entire MDCT window therefore sees a uniform 8×sin → near-zero HF leakage.
 *
 * Signal:
 *   Frame 0:          A_quiet = 1  (primes the overlap buffer)
 *   bufNext [0..7]:   A_quiet × gainInc^k  (ramp matching Level 7→neutral transition)
 *   bufNext [8..255]: A_loud  = 8  (untouched by Modulate → stays at 8 = 8×sin ✓)
 *
 * Gain point: {7, 0}
 *   Location=0 → lastPos=0 → no constant region; transition starts at bufNext[0].
 *   After 8 transition steps: level = 0.125 × gainInc^8 = 0.125×8 = 1.0 (neutral).
 *   Remainder [8..255]: bufNext untouched ✓
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;
    const float f       = 0.125f;

    // gainInc for the Level 7→neutral (4) transition: 2^(+3/8) ≈ 1.2968.
    // After 8 steps: 0.125 × gainInc^8 = 0.125×8 = 1.0 (neutral). ✓
    const float gainInc = std::pow(2.0f, 3.0f / 8.0f);

    auto sineAt = [f](size_t i) {
        return std::sin((float(M_PI) / 2.0f) * float(i) * f);
    };

    // Build signal across 3 frames:
    //   frame 0 = all quiet (primes overlap)
    //   frame 1 = ramp [0..7] + loud [8..255]  (the gain-modulated frame)
    //   frame 2 = loud continuation (no transient; bufCur already at A_loud level)
    vector<float> signal(kHalf * 3);

    // Frame 0: all quiet.
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Frame 1 bufNext[0..7]: amplitude = A_quiet × gainInc^k.
    // The gain transition divides by 0.125×gainInc^k → modulated = A_quiet/0.125 × sin = 8×sin. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc)
            signal[kHalf + k] = A_quiet * g * sineAt(kHalf + k);
    }
    // Frame 1 bufNext[8..255]: A_loud, untouched by Modulate → 8×sin. ✓
    for (size_t i = kHalf + 8; i < kHalf * 2; ++i)
        signal[i] = A_loud * sineAt(i);

    // Frame 2: continues as A_loud (signal is steady-state loud after the step).
    for (size_t i = kHalf * 2; i < kHalf * 3; ++i)
        signal[i] = A_loud * sineAt(i);

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap with quiet signal.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: ramp + loud.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // Level=7 → scale=0.125 (amplify ×8); Location=0 → lastPos=0,
            // transition immediately at bufNext[0..7], no constant region.
            si.AddSubbandCurve(0, {{7, 0}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: loud continuation — no compensating gain needed.
        // Modulated: bufCur = EncodeWindow × (A_loud×sin) exactly (modifiedBufNext was
        // uniform A_loud×sin from the Location=0 transition). bufNext = A_loud×sin.
        // → TDAC pair perfectly uniform at A_loud → very low HF leakage.
        // Nomod: bufCur has a tiny ramp at [0..7] (near-zero window), rest = A_loud×sin.
        // → same low HF leakage. Mod ≤ nomod (both near zero).
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs2.data(), p2);

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 30;

    // Frame 1: HF energy above the sine fundamental (≈ bin 16 for f=0.125).
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    // With modulation: MDCT input is 8×sin uniformly → near-zero HF leakage.
    // Without modulation: amplitude jumps from quiet(1) in bufCur to loud(8) in
    // bufNext, causing substantial HF spectral leakage.
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: loud continuation — both cases produce a clean MDCT.
    // Mod bufCur is exactly A_loud×sin; nomod bufCur has a tiny near-zero ramp at
    // [0..7].  Modulated HF ≤ unmodulated HF (both close to zero).
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LE(hf2_mod, hf2_nomod);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy Frame 1\\n"
        "Quiet->Loud at frame boundary, Level=7 (scale=0.125), ramp bufNext[0..7]",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy Frame 2\\n"
        "Loud continuation — no compensation, both frames clean",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: Mdct(Modulate) → Midct(Demodulate) recovers original signal
    // with one-frame delay.  Frame 2 has no compensating gain — the LOUD bufCur
    // from the modulated frame 1 naturally matches the LOUD bufNext of frame 2.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{7, 0}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{7, 0}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                mdct.Mdct(sp.data(), p);
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{7, 0}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * Second frequency-domain test: LOUD→QUIET release transient at bufNext[64].
 *
 * Signal: LOUD (A=8) for bufCur and bufNext[0..63]; release ramp [64..71]
 * matching the gain interpolation rate; QUIET (A=1) for bufNext[72..255].
 *
 * Two-point gain envelope: {{4,8},{7,31}}
 *   scale = GainLevel[4] = 1.0  →  bufCur unchanged at A_loud (no step vs bufNext ✓)
 *
 *   Constant  [ 0, 64):  bufNext / 1.0              (LOUD unchanged ✓)
 *   Transition[64, 72):  level ramps 1→0.125  (Level 4→7, gainInc = 2^(-3/8))
 *   Constant  [72,248):  bufNext / 0.125 = ×8  (quiet → LOUD ✓)
 *   Transition[248,256): level ramps 0.125→1   (Level 7→neutral, same rate as gainInc_atk)
 *                        EncodeWindow[7..0] ≈ 0.023..0.0015 → near-zero window;
 *                        gain spike there contributes negligible leakage.
 *
 * The signal ramp at [64..71] is pre-shaped to match gainInc_rel = 2^(-3/8):
 *   signal[64+k] = A_loud × gainInc_rel^k
 *   level  [64+k] = 1.0   × gainInc_rel^k
 *   modulated     = A_loud × sin  (exact cancellation ✓)
 *
 * The entire modulated MDCT input is uniformly A_loud×sin → near-zero HF leakage.
 *
 * Without modulation: amplitude is high for bufCur and bufNext[0..71], then
 * drops to A_quiet for [72..255], causing substantial HF spectral leakage.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_TransientInFrame) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;
    const float f       = 0.125f;

    // Release transition (Level 4→7): gainInc = 2^(-3/8) so after 8 steps 1.0→0.125.
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f); // ≈ 0.7706

    auto sineAt = [f](size_t i) {
        return std::sin((float(M_PI) / 2.0f) * float(i) * f);
    };

    // Build signal across 3 frames:
    //   frame 0 = all LOUD (primes overlap)
    //   frame 1 = loud [0..63] + ramp [64..71] + quiet [72..255]  (the gain-modulated frame)
    //   frame 2 = quiet continuation (signal stays quiet after the release)
    vector<float> signal(kHalf * 3);

    // Frame 0: all loud (primes bufCur at A_loud level, scale=1 leaves it unchanged).
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_loud * sineAt(i);

    // Frame 1 bufNext: loud constant [0..63].
    for (size_t i = kHalf; i < kHalf + 64; ++i)
        signal[i] = A_loud * sineAt(i);

    // Release ramp [64..71]: amplitude = A_loud × gainInc_rel^k.
    // level at pos (64+k) = 1.0 × gainInc_rel^k → modulated = A_loud×sin. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf + 64 + k] = A_loud * g * sineAt(kHalf + 64 + k);
    }

    // Post-release quiet [72..255]: A_quiet; divided by level=0.125 → A_loud×sin. ✓
    for (size_t i = kHalf + 72; i < kHalf * 2; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Frame 2: bufNext pre-shaped for {{1,1}} compensation (mirrors the pattern in
    // TAtrac3MDCTGain1PointCompensateWithScaleDc).
    // The modulated bufCur carries A_loud×sin (EncodeWindow × 8×sin).
    // Compensation Level=1 (scale=8) divides ALL bufCur by 8, and also:
    //   Location=1 (lastPos=8): bufNext[0..7] divided by 8 + transition [8..15].
    // To make modifiedBufNext uniform at A_quiet:
    //   bufNext[0..7]   = A_loud   → /8 = A_quiet ✓
    //   bufNext[8+k]    = A_loud × gainInc_rel^k  → /(8×gainInc_rel^k) = A_quiet ✓
    //   bufNext[16..255]= A_quiet  → untouched = A_quiet ✓
    // This gives perfectly uniform A_quiet×sin → near-zero HF leakage.
    for (size_t i = kHalf * 2; i < kHalf * 2 + 8; ++i)
        signal[i] = A_loud * sineAt(i);
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf * 2 + 8 + k] = A_loud * g * sineAt(kHalf * 2 + 8 + k);
    }
    for (size_t i = kHalf * 2 + 16; i < kHalf * 3; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap with loud signal.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: release transient signal.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // {4,8}:  scale=1.0 (neutral), lastPos=64 — LOUD region unchanged
            // {7,31}: scale=0.125, lastPos=248 — quiet post-release amplified ×8;
            //         final transition [248..255] is at near-zero MDCT window.
            si.AddSubbandCurve(0, {{4, 8}, {7, 31}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: quiet continuation.
        // Modulated: bufCur = EncodeWindow × (A_loud×sin) — the entire modifiedBufNext
        //   was at A_loud×sin level.  bufNext = A_quiet×sin → mismatch.
        //   Compensating gain {{1,1}}: Level=1 → scale=8, divides all bufCur by 8,
        //   bringing EncodeWindow×A_loud down to EncodeWindow×A_quiet — matching bufNext.
        //   Location=1 (lastPos=8) also attenuates bufNext[0..7] to ease the transition.
        //   This significantly reduces HF leakage vs the uncompensated case.
        // Nomod: bufCur has the LOUD→QUIET step inside it (at ~position 64–72),
        //   windowed by EncodeWindow — produces more HF leakage than the compensated mod.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si2;
            // Level=1 → scale=8 (attenuate ×1/8): compensates the A_loud bufCur.
            // Location=1 → lastPos=8: bufNext[0..7] also attenuated + short transition.
            si2.AddSubbandCurve(0, {{1, 1}});
            mdct.Mdct(specs2.data(), p2,
                { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs2.data(), p2);
        }

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 30;

    // Frame 1: HF energy above the sine fundamental.
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    // With modulation: MDCT input is A_loud×sin uniformly → near-zero HF leakage.
    // Without modulation: amplitude drops from A_loud to A_quiet in bufNext → HF leakage.
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: nomod has a windowed LOUD→QUIET step in bufCur; mod uses compensating
    // gain {{1,1}} to scale the A_loud bufCur back down → less HF leakage.
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LE(hf2_mod, hf2_nomod);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_TransientInFrame Frame 1\\n"
        "LOUD->QUIET at bufNext[64], gain {{4,8},{7,31}}, ramp-shaped release",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_TransientInFrame Frame 2\\n"
        "Quiet continuation, compensating gain {{1,1}}",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: Mdct(Modulate) → Midct(Demodulate) recovers original signal
    // with one-frame delay.  Frame 2 uses compensating gain {{1,1}}.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{4, 8}, {7, 31}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{4, 8}, {7, 31}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                TAtrac3Data::SubbandInfo si2;
                si2.AddSubbandCurve(0, {{1, 1}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{4, 8}, {7, 31}});
                siNext.AddSubbandCurve(0, {{1, 1}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * Third frequency-domain test: attack AND release transient within one frame.
 *
 * Signal: quiet(A=1) for bufCur and bufNext[0..31], a LOUD burst(A=8) for
 * bufNext[40..95], then quiet(A=1) for bufNext[104..255].  The attack and
 * release edges are pre-shaped with the gain-interpolation ramp rate so that
 * the modulation cancels them exactly.
 *
 * Two-point gain envelope: {{4,4},{1,12}}
 *   scale = GainLevel[4] = 1.0  →  bufCur unchanged (quiet stays quiet ✓)
 *
 *   Constant  [ 0, 32):  bufNext / 1.0    = A_quiet (unchanged ✓)
 *   Transition[32, 40):  level ramps 1→8  (Level 4→1, gainInc = 2^(+3/8))
 *   Constant  [40, 96):  bufNext / 8      = A_loud/8 = A_quiet (attenuated ✓)
 *   Transition[96,104):  level ramps 8→1  (Level 1→neutral, gainInc = 2^(-3/8))
 *   Remainder[104,256):  bufNext untouched = A_quiet (already at target ✓)
 *
 * The signal ramps at [32..39] and [96..103] are constructed to match gainInc
 * exactly (signal[32+k] = A_quiet×gainInc_atk^k, signal[96+k] = A_loud×gainInc_rel^k)
 * so Modulate divides them out perfectly → modulated MDCT input is uniformly
 * A_quiet throughout → near-zero HF leakage.
 *
 * Without modulation the window sees amplitude change from quiet(1) through the
 * burst(8) and back, producing substantial HF leakage in the unmodulated spectrum.
 *
 * Frame 2: plain quiet (A=1); no compensating gain needed.  The modulated
 * bufCur is EncodeWindow×(A_quiet×sin) which already matches the quiet bufNext
 * → MDCT input uniform → near-zero leakage.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_AttackAndRelease) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;
    const float f       = 0.125f;

    // Gain interpolation rates for the two transitions:
    //   Attack  (Level 4→1): gainInc = 2^(+3/8) so after 8 steps 1.0→8.0
    //   Release (Level 1→4): gainInc = 2^(-3/8) so after 8 steps 8.0→1.0
    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f); // ≈ 1.2968
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f); // ≈ 0.7706

    auto sineAt = [f](size_t i) {
        return std::sin((float(M_PI) / 2.0f) * float(i) * f);
    };

    // Build signal across 3 frames:
    //   frame 0 = all quiet (primes overlap)
    //   frame 1 = quiet [0..31] + attack ramp [32..39] + burst [40..95]
    //             + release ramp [96..103] + quiet [104..255]
    //   frame 2 = quiet continuation (signal stays quiet after the burst)
    vector<float> signal(kHalf * 3);

    // Frame 0: all quiet.
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Frame 1 bufNext:
    // Pre-attack quiet [0..31]
    for (size_t i = kHalf; i < kHalf + 32; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Attack ramp [32..39]: amplitude = A_quiet × gainInc_atk^k.
    // At gain-transition pos (32+k), level = 1.0 × gainInc_atk^k, so
    // modulated = (A_quiet × gainInc_atk^k) / (1.0 × gainInc_atk^k) = A_quiet. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + 32 + k] = A_quiet * g * sineAt(kHalf + 32 + k);
    }

    // Burst body [40..95]: A_loud; divided by level=8.0 → A_loud/8 = A_quiet. ✓
    for (size_t i = kHalf + 40; i < kHalf + 96; ++i)
        signal[i] = A_loud * sineAt(i);

    // Release ramp [96..103]: amplitude = A_loud × gainInc_rel^k.
    // At gain-transition pos (96+k), level = 8.0 × gainInc_rel^k, so
    // modulated = (A_loud × gainInc_rel^k) / (8.0 × gainInc_rel^k) = A_quiet. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf + 96 + k] = A_loud * g * sineAt(kHalf + 96 + k);
    }

    // Post-release quiet [104..255]: A_quiet; in remainder (untouched) → A_quiet. ✓
    for (size_t i = kHalf + 104; i < kHalf * 2; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Frame 2: quiet continuation; no compensating gain needed.
    for (size_t i = kHalf * 2; i < kHalf * 3; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap with quiet signal.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: burst signal.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // {4,4}: scale=1.0,  lastPos=32 — quiet prefix unchanged
            // {1,12}: level=8.0, lastPos=96 — loud burst attenuated ÷8 → A_quiet
            //         remainder [104..255] untouched = A_quiet ✓
            si.AddSubbandCurve(0, {{4, 4}, {1, 12}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: quiet continuation; no compensating gain needed.
        // Modulated bufCur is EncodeWindow×(A_quiet×sin) — uniform → low HF.
        // Nomod bufCur carries the burst amplitude shape → HF leakage.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs2.data(), p2);

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 30;

    // Frame 1: HF energy above the sine fundamental.
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    // With modulation the MDCT input is 8×sin uniformly → near-zero HF leakage.
    // Without modulation the burst amplitude envelope produces real HF leakage.
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: nomod has the burst amplitude shape in bufCur → HF leakage.
    // Mod has uniform A_quiet bufCur → near-zero HF leakage.
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LT(hf2_mod * 10.0f, hf2_nomod);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_AttackAndRelease Frame 1\\n"
        "Burst bufNext[40..95], gain {{4,4},{1,12}}, ramp-shaped edges",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_AttackAndRelease Frame 2\\n"
        "Quiet continuation, no compensating gain",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: Mdct(Modulate) → Midct(Demodulate) recovers original signal
    // with one-frame delay.  Frame 2 needs no compensating gain.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{4, 4}, {1, 12}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{4, 4}, {1, 12}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                mdct.Mdct(sp.data(), p);
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{4, 4}, {1, 12}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * LOUD->QUIET->LOUD dip transient within one frame.
 *
 * Signal: loud(A=8) for bufCur and bufNext[0..31], a quiet dip(A=1) for
 * bufNext[40..95], then loud(A=8) again for bufNext[104..255].  The release
 * and attack edges are pre-shaped with the gain-interpolation ramp rate so
 * that the modulation cancels them exactly.
 *
 * Strategy: keep the loud prefix and tail at their original level; amplify
 * only the quiet dip to match.  scale=1.0 leaves bufCur (loud) unchanged.
 *
 * Two-point gain envelope: {{4,4},{7,12}}
 *   scale = GainLevel[4] = 1.0  ->  bufCur (loud) unchanged ✓
 *
 *   Constant  [ 0, 32):  bufNext / 1.0      = A_loud  (loud prefix, unchanged ✓)
 *   Transition[32, 40):  level ramps 1->0.125 (Level 4->7, gainInc = 2^(-3/8))
 *   Constant  [40, 96):  bufNext / 0.125    = A_loud  (quiet dip amplified x8 ✓)
 *   Transition[96,104):  level ramps 0.125->1 (Level 7->neutral, gainInc = 2^(+3/8))
 *   Remainder[104,256):  bufNext untouched  = A_loud  (loud tail already at target ✓)
 *
 * The signal ramps at [32..39] and [96..103] are constructed to match gainInc
 * exactly (signal[32+k] = A_loud*gainInc_rel^k, signal[96+k] = A_quiet*gainInc_atk^k)
 * so Modulate divides them out perfectly -> modulated MDCT input is uniformly
 * A_loud*sin throughout the window -> near-zero HF leakage.
 *
 * Frame 2: plain loud (A=8); no compensating gain needed.  The modulated
 * bufCur is EncodeWindow*(A_loud*sin) which already matches the loud bufNext
 * -> MDCT input uniform -> near-zero leakage.  Without modulation, frame 2's
 * bufCur carries the loud->quiet->loud shape -> HF leakage.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_ReleaseAndAttack) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;
    const float f       = 0.125f;

    // Gain interpolation rates for the two transitions:
    //   Release (Level 1->4): gainInc = 2^(-3/8) so after 8 steps 8.0->1.0
    //   Attack  (Level 4->1): gainInc = 2^(+3/8) so after 8 steps 1.0->8.0
    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f); // ≈ 1.2968
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f); // ≈ 0.7706

    auto sineAt = [f](size_t i) {
        return std::sin((float(M_PI) / 2.0f) * float(i) * f);
    };

    // Build signal across 3 frames:
    //   frame 0 = all loud (primes overlap)
    //   frame 1 = loud [0..31] + release ramp [32..39] + quiet dip [40..95]
    //             + attack ramp [96..103] + loud [104..255]
    //   frame 2 = pre-shaped for compensating gain {{7,1}}
    vector<float> signal(kHalf * 3);

    // Frame 0: all loud.
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_loud * sineAt(i);

    // Frame 1 bufNext:
    // Loud prefix [0..31].
    for (size_t i = kHalf; i < kHalf + 32; ++i)
        signal[i] = A_loud * sineAt(i);

    // Release ramp [32..39]: amplitude = A_loud * gainInc_rel^k.
    // At gain-transition pos (32+k), level = 8 * gainInc_rel^k, so
    // modulated = (A_loud * gainInc_rel^k) / (8 * gainInc_rel^k) = 1.0*sin. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf + 32 + k] = A_loud * g * sineAt(kHalf + 32 + k);
    }

    // Quiet dip [40..95]: A_quiet; divided by level=1.0 -> unchanged at 1.0*sin. ✓
    for (size_t i = kHalf + 40; i < kHalf + 96; ++i)
        signal[i] = A_quiet * sineAt(i);

    // Attack ramp [96..103]: amplitude = A_quiet * gainInc_atk^k.
    // At gain-transition pos (96+k), level = 1.0 * gainInc_atk^k, so
    // modulated = (A_quiet * gainInc_atk^k) / (gainInc_atk^k) = 1.0*sin. ✓
    {
        float g = 1.0f;
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + 96 + k] = A_quiet * g * sineAt(kHalf + 96 + k);
    }

    // Loud suffix [104..255]: A_loud; in remainder, untouched = A_loud. ✓
    for (size_t i = kHalf + 104; i < kHalf * 2; ++i)
        signal[i] = A_loud * sineAt(i);

    // Frame 2: plain loud continuation.  No compensation needed: the modulated
    // bufCur is EncodeWindow*(A_loud*sin) which already matches loud bufNext.
    for (size_t i = kHalf * 2; i < kHalf * 3; ++i)
        signal[i] = A_loud * sineAt(i);

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap with loud signal.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: dip signal.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // {4,4}:  scale=1.0, lastPos=32  — loud prefix unchanged
            // {7,12}: scale=0.125, lastPos=96 — quiet dip amplified x8 -> A_loud;
            //         remainder [104..255] untouched (already at A_loud).
            si.AddSubbandCurve(0, {{4, 4}, {7, 12}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: plain loud continuation, no compensating gain.
        // Mod:   bufCur = EncodeWindow*(A_loud*sin) — uniform, matches loud bufNext.
        // Nomod: bufCur = EncodeWindow*[loud->quiet->loud] — HF leakage from the dip.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs2.data(), p2);

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 30;

    // Frame 1: HF energy above the sine fundamental.
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    // With modulation the MDCT input is 1.0*sin uniformly -> near-zero HF leakage.
    // Without modulation the dip amplitude envelope produces real HF leakage.
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: nomod bufCur carries loud->quiet->loud shape -> HF leakage.
    // Mod bufCur is uniform A_loud (from frame 1's modulated output) -> less HF.
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LT(hf2_mod * 10.0f, hf2_nomod);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_ReleaseAndAttack Frame 1\\n"
        "Dip bufNext[40..95], gain {{4,4},{7,12}}, ramp-shaped edges",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_ReleaseAndAttack Frame 2\\n"
        "Loud continuation, no compensating gain needed",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: Mdct(Modulate) -> Midct(Demodulate) recovers original signal
    // with one-frame delay.  Frame 2 has no compensating gain.
    //   frame 1 Midct: Demodulate(siCur=empty, siNext={{4,4},{7,12}})
    //     scale = GainLevel[4] = 1.0; giNow=empty -> no effect on prev[].
    //   frame 2 Midct: Demodulate(siCur={{4,4},{7,12}}, siNext=empty)
    //     scale = 1.0; giNow={{4,4},{7,12}} de-amplifies the overlap prev[].
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{4, 4}, {7, 12}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{4, 4}, {7, 12}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                mdct.Mdct(sp.data(), p);
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{4, 4}, {7, 12}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * Fourth frequency-domain test: DC signal shaped exactly as in
 * TAtrac3MDCTGain1PointCompensateWithScaleDc.
 *
 * Signal: DC=1, with a LOUD burst (DC=8) in frame 1 pre-shaped using gain-
 * interpolation ramps at both edges, and frame 2 pre-shaped so the compensating
 * gain {{1,1}} produces a clean DC=1 output.
 *
 * Frame 0: DC=1 (primes overlap)
 * Frame 1 bufNext:
 *   [0..7]:   DC=1                        constant region  (/ 0.125 = 8 ✓)
 *   [8..15]:  gainInc_atk^k               transition ramp  (/ (0.125×gainInc_atk^k) = 8 ✓)
 *   [16..255]: DC=8                        remainder untouched → 8 ✓
 *
 * Frame 2 bufNext (pre-shaped for {{1,1}} compensation):
 *   [0..7]:   DC=8                        (/ scale=8 → 1 ✓)
 *   [8..15]:  8 × gainInc_rel^k           (/ (8×gainInc_rel^k) → 1 ✓)
 *   [16..255]: DC=1                        (untouched → 1 ✓)
 *
 * With modulation: both MDCT inputs are uniform DC → near-zero HF leakage.
 * Without modulation: amplitude steps (1→8 in frame 1, 8→1 in frame 2) produce
 * substantial HF leakage in both frames.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_DcSignal) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;

    // Level 7→neutral (4): gainInc = 2^(+3/8) ≈ 1.2968  (= 1.29684 from reference test)
    // Level 1→neutral (4): gainInc = 2^(-3/8) ≈ 0.7706
    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f);
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f);

    // Signal shaped exactly as TAtrac3MDCTGain1PointCompensateWithScaleDc.
    vector<float> signal(kHalf * 3);

    // Frame 0: DC=1 (primes overlap bufCur).
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet;

    // Frame 1 bufNext:
    for (size_t i = kHalf; i < kHalf + 8; ++i)          // [0..7]:  DC=1
        signal[i] = A_quiet;
    {
        float g = 1.0f;                                  // [8..15]: ramp gainInc_atk^k
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + 8 + k] = g;
    }
    for (size_t i = kHalf + 16; i < kHalf * 2; ++i)     // [16..255]: DC=8
        signal[i] = A_loud;

    // Frame 2 bufNext (pre-shaped for {{1,1}} compensation):
    for (size_t i = kHalf * 2; i < kHalf * 2 + 8; ++i)  // [0..7]:  DC=8
        signal[i] = A_loud;
    {
        float g = 1.0f;                                  // [8..15]: 8 × gainInc_rel^k
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf * 2 + 8 + k] = A_loud * g;
    }
    for (size_t i = kHalf * 2 + 16; i < kHalf * 3; ++i) // [16..255]: DC=1
        signal[i] = A_quiet;

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: DC=1 → ramp → DC=8.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // Level=7 (scale=0.125, ×8); Location=1 (lastPos=8).
            si.AddSubbandCurve(0, {{7, 1}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: DC=8 → ramp → DC=1 (pre-shaped) + compensating gain.
        // Mod:   bufCur=EncodeWindow×DC=8; gain {{1,1}} divides bufCur by 8 and gives
        //        modifiedBufNext=DC=1 uniformly → perfectly clean DC=1 MDCT.
        // Nomod: bufCur carries the 1→8 step; bufNext adds another 8→1 → HF leakage.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si2;
            // Level=1 (scale=8, ÷8); Location=1 (lastPos=8).
            si2.AddSubbandCurve(0, {{1, 1}});
            mdct.Mdct(specs2.data(), p2,
                { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs2.data(), p2);
        }

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    // For DC, main MDCT energy is in the lowest bins (0-3); leakage from amplitude
    // steps appears at bins ≥ 4.
    const int kHfStart = 4;

    // Frame 1: nomod has 1→8 step in MDCT window; mod is uniform DC=8.
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: nomod has 1→8 in bufCur and 8→1 in bufNext; mod is uniform DC=1.
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LT(hf2_mod * 10.0f, hf2_nomod);
    EXPECT_GT(hf2_nomod, 0.0f);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_DcSignal Frame 1\\n"
        "DC: 1->ramp->8, gain {{7,1}}",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_DcSignal Frame 2\\n"
        "DC: 8->ramp->1, compensating gain {{1,1}}",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: mirrors TAtrac3MDCTGain1PointCompensateWithScaleDc reconstruction.
    // Frame 2 uses compensating gain {{1,1}}.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{7, 1}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{7, 1}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                TAtrac3Data::SubbandInfo si2;
                si2.AddSubbandCurve(0, {{1, 1}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{7, 1}});
                siNext.AddSubbandCurve(0, {{1, 1}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}


/*
 * Exploratory test: QUIET->LOUD transient inside bufNext.
 *
 * Signal:
 *   frame 0 = all quiet (A=1)  — primes overlap
 *   frame 1 = quiet [0..63] + loud [64..255]  — QUIET->LOUD step at bufNext[64]
 *   frame 2 = all loud (A=8)   — continuation
 *
 * Fill in si1.AddSubbandCurve / si2.AddSubbandCurve to explore how different
 * gain choices affect HF leakage in each frame.  Key cases to try:
 *
 *   si1={{1,8}}  — covers only quiet prefix; loud [72..255] in REMAINDER (untouched)
 *                  → contrast 0.125:8 = worse than unmodulated 1:8
 *
 *   si1={{1,31}} — covers all of bufNext (no remainder); loud [64..247] in
 *                  constant region, divided by 8 → step 0.125:1 instead of 1:8
 *
 *   si1=empty, si2={{1,31}}
 *                — next-frame approach: loud bufNext_2 covered by gain on frame 2
 *
 * Compare hf1_mod / hf2_mod vs hf1_nomod / hf2_nomod to quantify the effect.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_QuietToLoudTransient) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f);
    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;
    const float f       = 0.125f;

    auto sineAt = [f](size_t i) {
        return std::sin((float(M_PI) / 2.0f) * float(i) * f);
    };

    // Build signal across 3 frames:
    //   frame 0 = all quiet (primes overlap buffer at A_quiet level)
    //   frame 1 = quiet [0..63] + loud [64..255] — QUIET->LOUD step at bufNext[64]
    //   frame 2 = all loud (continuation)
    vector<float> signal(kHalf * 3);

    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet * sineAt(i);

    for (size_t i = kHalf; i < kHalf + 64; ++i)
        signal[i] = A_quiet * sineAt(i);
    {
        float g = 1.0f;                                  // [8..15]: ramp gainInc_atk^k
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + 56 + k] *= g;
    }

    for (size_t i = kHalf + 64; i < kHalf * 2; ++i)
        signal[i] = A_loud * sineAt(i);

    for (size_t i = kHalf * 2; i < kHalf * 3; ++i)
        signal[i] = A_loud * sineAt(i);

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap with quiet signal.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: QUIET->LOUD step at bufNext[64].
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si1;
            si1.AddSubbandCurve(0, {{7, 7}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si1.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: all loud continuation.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si2;
            /* si2.AddSubbandCurve(0, {...}); */
            mdct.Mdct(specs2.data(), p2,
                { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs2.data(), p2);
        }

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 30;

    float hf1_nomod = 0.0f, hf1_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf1_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf1_mod   += specs1_mod[k]   * specs1_mod[k];
    }

    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }

    EXPECT_LT(hf1_mod * 10.0f, hf1_nomod);
    EXPECT_GT(hf1_nomod, 0.0f);

    EXPECT_LE(hf2_mod * 10.0f, hf2_nomod);
    EXPECT_GT(hf2_nomod, 0.0f);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_QuietToLoudTransient Frame 1\\n"
        "QUIET->LOUD at bufNext[64]",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_QuietToLoudTransient Frame 2\\n"
        "Loud continuation",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: Mdct(Modulate) -> Midct(Demodulate) recovers original signal
    // with one-frame delay.  Frame 2 has no compensating gain (loud bufCur and
    // loud bufNext are already matched after frame 1 amplified the overlap).
    //
    //   frame 1 Midct: Demodulate(siCur=empty, siNext={{7,7}})
    //     scale = GainLevel[7] = 0.125 pre-scales cur, undoing the x8 amplification
    //     on frame 1's bufNext that Modulate applied.
    //
    //   frame 2 Midct: Demodulate(siCur={{7,7}}, siNext=empty)
    //     scale = 1.0 (no frame-2 gain); giNow={{7,7}} de-amplifies the overlap
    //     (prev[]) that was stored from frame 1's IMDCT second half.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{7, 7}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{7, 7}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                mdct.Mdct(sp.data(), p);
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{7, 7}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * Fifth frequency-domain test: DC signal shaped as in
 * TAtrac3MDCTGain1PointCompensateWithScaleDc2.
 *
 * Identical to DcSignal for frame 1.  The only differences are in frame 2:
 *
 *   DcSignal  (Dc1): burst extends 8 samples into frame 2's bufNext[0..7]
 *                    → frame 2 bufNext: DC=8[0..7] + ramp[8..15] + DC=1[16..]
 *                    → compensation {{1,1}}  (Location=1, lastPos=8, constant+transition)
 *
 *   DcSignal2 (Dc2): burst ends exactly at the frame boundary; ramp begins
 *                    at bufNext[0] with no leading constant DC=8 region
 *                    → frame 2 bufNext: ramp[0..7]  + DC=1[8..]
 *                    → compensation {{1,0}}  (Location=0, lastPos=0, transition only)
 *
 * Frame 2 cancellation with {{1,0}} (Location=0 → no constant region):
 *   transition [0..7]: bufNext[k] = 8×gainInc_rel^k; level[k] = 8×gainInc_rel^k
 *                      modifiedBufNext[k] = 1 ✓
 *   remainder [8..255]: DC=1 untouched → 1 ✓
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_DcSignal2) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;

    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f);
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f);

    vector<float> signal(kHalf * 3);

    // Frame 0: DC=1.
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet;

    // Frame 1 bufNext (identical to DcSignal):
    for (size_t i = kHalf; i < kHalf + 8; ++i)          // [0..7]:  DC=1
        signal[i] = A_quiet;
    {
        float g = 1.0f;                                  // [8..15]: ramp gainInc_atk^k
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + 8 + k] = g;
    }
    for (size_t i = kHalf + 16; i < kHalf * 2; ++i)     // [16..255]: DC=8
        signal[i] = A_loud;

    // Frame 2 bufNext (pre-shaped for {{1,0}} compensation):
    // Burst ends at the frame boundary — ramp starts at bufNext[0], no leading DC=8.
    {
        float g = 1.0f;                                  // [0..7]: 8 × gainInc_rel^k
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf * 2 + k] = A_loud * g;
    }
    for (size_t i = kHalf * 2 + 8; i < kHalf * 3; ++i)  // [8..255]: DC=1
        signal[i] = A_quiet;

    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: DC=1 → ramp → DC=8  (same as DcSignal).
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            si.AddSubbandCurve(0, {{7, 1}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: ramp → DC=1 + compensating gain {{1,0}}.
        // Mod:   transition [0..7] cancels ramp exactly → modifiedBufNext=DC=1 uniformly.
        // Nomod: bufCur has the 1→8 step; bufNext has the 8→1 ramp at [0..7] → HF leakage.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si2;
            // Level=1 (scale=8); Location=0 (lastPos=0) — transition starts immediately.
            si2.AddSubbandCurve(0, {{1, 0}});
            mdct.Mdct(specs2.data(), p2,
                { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs2.data(), p2);
        }

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    const int kHfStart = 4;

    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LT(hf2_mod * 10.0f, hf2_nomod);
    EXPECT_GT(hf2_nomod, 0.0f);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_DcSignal2 Frame 1\\n"
        "DC: 1->ramp->8, gain {{7,1}}",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_DcSignal2 Frame 2\\n"
        "DC: ramp->1 (no leading constant), compensating gain {{1,0}}",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip: mirrors TAtrac3MDCTGain1PointCompensateWithScaleDc2 reconstruction.
    // Frame 2 uses compensating gain {{1,0}}.
    {
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{7, 1}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{7, 1}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                TAtrac3Data::SubbandInfo si2;
                si2.AddSubbandCurve(0, {{1, 0}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si2.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{7, 1}});
                siNext.AddSubbandCurve(0, {{1, 0}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}

/*
 * Sixth frequency-domain test: DC signal shaped as in
 * TAtrac3MDCTGain2PointsCompensateWithoutScaleDc2.
 *
 * Frame 1 bufNext contains a symmetric burst spanning all 256 samples:
 *   [0..7]:   rising ramp  (gainInc_atk^k = 1.0 → ~6.16)
 *   [8..247]: DC=8
 *   [248..255]: falling ramp (A_loud * gainInc_rel^k = 8.0 → ~1.30)
 *
 * Gain {{4, 0}, {1, 31}} ("WithoutScale"):
 *   scale = GainLevel[4] = 1.0  → bufCur unchanged
 *   Point 0 (Level=4, Location=0): transition [0..7] divides bufNext by gainInc_atk^k
 *   Point 1 (Level=1, Location=31): constant [8..247] divides by 8.0
 *                                    transition [248..255] divides by 8*gainInc_rel^k
 *   → modulated bufNext = DC=1 throughout → minimal HF leakage in frame 1.
 *
 * Frame 2: plain DC=1, no compensating gain.
 *   Mod:   bufCur = EncodeWindow×DC=1 (uniform) → minimal HF.
 *   Nomod: bufCur = EncodeWindow×burst (large at [248..255]) → HF leakage.
 */
TEST(TGainProcessor_FreqDomain, GainModulation_ReducesSpectralEnergy_2PointsWithoutScaleDc2) {
    TAtrac3MDCT mdct;
    static const size_t kBandSz = 512;
    static const size_t kHalf   = 256;

    const float A_loud  = 8.0f;
    const float A_quiet = 1.0f;

    const float gainInc_atk = std::pow(2.0f,  3.0f / 8.0f);
    const float gainInc_rel = std::pow(2.0f, -3.0f / 8.0f);

    vector<float> signal(kHalf * 3);

    // Frame 0: DC=1.
    for (size_t i = 0; i < kHalf; ++i)
        signal[i] = A_quiet;

    // Frame 1 bufNext: symmetric burst covering all 256 samples.
    {
        float g = 1.0f;                                    // [0..7]: rising ramp gainInc_atk^k
        for (int k = 0; k < 8; ++k, g *= gainInc_atk)
            signal[kHalf + k] = g;
    }
    for (size_t i = kHalf + 8; i < kHalf + 248; ++i)      // [8..247]: DC=8
        signal[i] = A_loud;
    {
        float g = 1.0f;                                    // [248..255]: 8 * gainInc_rel^k
        for (int k = 0; k < 8; ++k, g *= gainInc_rel)
            signal[kHalf + 248 + k] = A_loud * g;
    }

    // Frame 2 bufNext: plain DC=1 (no pre-shaping needed — scale=1.0 means
    // bufCur is already EncodeWindow×DC=1 from the modulated bufNext above).
    for (size_t i = kHalf * 2; i < kHalf * 3; ++i)
        signal[i] = A_quiet;

    // Returns {frame1_specs, frame2_specs}.
    auto runFrames = [&](bool withModulation)
        -> std::pair<vector<float>, vector<float>>
    {
        vector<float> b0(kBandSz, 0.0f), b1(kBandSz, 0.0f),
                      b2(kBandSz, 0.0f), b3(kBandSz, 0.0f);
        vector<float> specs1(1024), specs2(1024);

        // Frame 0: prime overlap.
        memcpy(b0.data() + kHalf, signal.data(), kHalf * sizeof(float));
        float* p0[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs1.data(), p0);

        // Frame 1: symmetric burst.
        memcpy(b0.data() + kHalf, signal.data() + kHalf, kHalf * sizeof(float));
        float* p1[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        if (withModulation) {
            TAtrac3Data::SubbandInfo si;
            // Level=4 (scale=1.0, bufCur unchanged); two points span all 256 samples.
            si.AddSubbandCurve(0, {{4, 0}, {1, 31}});
            mdct.Mdct(specs1.data(), p1,
                { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator(),
                  TAtrac3MDCT::TGainModulator() });
        } else {
            mdct.Mdct(specs1.data(), p1);
        }

        // Frame 2: DC=1, no compensating gain in either case.
        // Mod:   bufCur = EncodeWindow×DC=1 (smooth) → minimal HF.
        // Nomod: bufCur = EncodeWindow×burst (large values at [248..255]) → HF leakage.
        memcpy(b0.data() + kHalf, signal.data() + kHalf * 2, kHalf * sizeof(float));
        float* p2[4] = { b0.data(), b1.data(), b2.data(), b3.data() };
        mdct.Mdct(specs2.data(), p2);

        return {specs1, specs2};
    };

    auto result_nomod = runFrames(false);
    auto result_mod   = runFrames(true);
    const auto& specs1_nomod = result_nomod.first;
    const auto& specs1_mod   = result_mod.first;
    const auto& specs2_nomod = result_nomod.second;
    const auto& specs2_mod   = result_mod.second;

    // DC energy in bins 0-3; leakage appears at bins ≥ 4.
    const int kHfStart = 4;

    // Frame 1: nomod has burst in MDCT second half → HF leakage;
    //           mod flattens it to DC=1 → minimal HF.
    float hf_nomod = 0.0f, hf_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf_nomod += specs1_nomod[k] * specs1_nomod[k];
        hf_mod   += specs1_mod[k]   * specs1_mod[k];
    }
    EXPECT_LT(hf_mod * 10.0f, hf_nomod);
    EXPECT_GT(hf_nomod, 0.0f);

    // Frame 2: nomod has EncodeWindow×burst in bufCur (large at [248..255]) → HF leakage;
    //           mod has EncodeWindow×DC=1 → uniform → minimal HF.
    float hf2_nomod = 0.0f, hf2_mod = 0.0f;
    for (int k = kHfStart; k < 256; ++k) {
        hf2_nomod += specs2_nomod[k] * specs2_nomod[k];
        hf2_mod   += specs2_mod[k]   * specs2_mod[k];
    }
    EXPECT_LT(hf2_mod * 10.0f, hf2_nomod);
    EXPECT_GT(hf2_nomod, 0.0f);

    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_2PointsWithoutScaleDc2 Frame 1\n"
        "Symmetric burst, gain {{4,0},{1,31}}",
        specs1_nomod, specs1_mod, kHfStart);
    MaybePlotMdctEnergy(
        "GainModulation_ReducesSpectralEnergy_2PointsWithoutScaleDc2 Frame 2\n"
        "DC=1, no compensating gain (scale=1.0)",
        specs2_nomod, specs2_mod, kHfStart);

    // Round-trip reconstruction: Mdct(Modulate) → Midct(Demodulate) must recover
    // the original signal with a one-frame delay, mirroring the reference check in
    // TAtrac3MDCTGain2PointsCompensateWithoutScaleDc2.
    {
        // Encode buffers: [0..255]=bufCur (overlap), [256..511]=bufNext (user loads).
        vector<float> enc0(kBandSz, 0.0f), enc1(kBandSz, 0.0f),
                      enc2(kBandSz, 0.0f), enc3(kBandSz, 0.0f);
        // Decode buffers: [0..255]=output (written by Midct), [256..511]=overlap state.
        vector<float> dec0(kBandSz, 0.0f), dec1(kBandSz, 0.0f),
                      dec2(kBandSz, 0.0f), dec3(kBandSz, 0.0f);
        vector<float> signalRes(kHalf * 3, 0.0f);
        vector<float> sp(1024);

        for (int frame = 0; frame < 3; ++frame) {
            memcpy(enc0.data() + kHalf, signal.data() + frame * kHalf, kHalf * sizeof(float));
            float* p[4] = { enc0.data(), enc1.data(), enc2.data(), enc3.data() };
            float* t[4] = { dec0.data(), dec1.data(), dec2.data(), dec3.data() };

            if (frame == 1) {
                // Encode: burst with {{4,0},{1,31}}.
                TAtrac3Data::SubbandInfo si;
                si.AddSubbandCurve(0, {{4, 0}, {1, 31}});
                mdct.Mdct(sp.data(), p,
                    { mdct.GainProcessor.Modulate(si.GetGainPoints(0)),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator(),
                      TAtrac3MDCT::TGainModulator() });
                // Decode: siCur=empty, siNext={{4,0},{1,31}}.
                TAtrac3Data::SubbandInfo siCur, siNext;
                siNext.AddSubbandCurve(0, {{4, 0}, {1, 31}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else if (frame == 2) {
                // Encode: DC=1, no modulation.
                mdct.Mdct(sp.data(), p);
                // Decode: siCur={{4,0},{1,31}}, siNext=empty.
                TAtrac3Data::SubbandInfo siCur, siNext;
                siCur.AddSubbandCurve(0, {{4, 0}, {1, 31}});
                mdct.Midct(sp.data(), t,
                    { mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator(),
                      TAtrac3MDCT::TGainDemodulator() });
            } else {
                // Frame 0: prime overlap, no gain.
                mdct.Mdct(sp.data(), p);
                mdct.Midct(sp.data(), t);
            }

            memcpy(signalRes.data() + frame * kHalf, dec0.data(), kHalf * sizeof(float));
        }

        // One-frame delay: signalRes[kHalf + i] ≈ signal[i].
        for (size_t i = kHalf; i < kHalf * 3; ++i)
            EXPECT_NEAR(signal[i - kHalf], signalRes[i], 0.00001f);
    }
}


// Mirror with asymmetric scales (giNow.Level != giNext.Level):
// scale comes from giNext, level from giNow.
// Constant: out = B_next * scale_next + B_cur * level_now / scale_modulate
// where scale_modulate is the scale used during Modulate (= GainLevel[giCur[0].Level]).
TEST(TGainProcessor_Mirror, AsymmetricGains_ConstantRegion) {
    TAtrac3GP gp;
    // Modulate with Level=0 -> scale_mod = 16
    vector<TGP> giMod = {{0, 31}};
    // Demodulate giNow Level=0 (level=16), giNext Level=2 (scale=4)
    vector<TGP> giNow = {{0, 31}};
    vector<TGP> giNext = {{2, 0}};

    const float scale_mod = GainLevelAt(0); // 16.0
    const float scale_dem = GainLevelAt(2); // 4.0  (from giNext)
    const float level_dem = GainLevelAt(0); // 16.0 (from giNow)

    const float B_cur_val = 16.0f, B_next_val = 8.0f;
    vector<float> bufCur(256, B_cur_val);
    vector<float> bufNext(256, B_next_val);

    auto modFn = gp.Modulate(giMod);
    modFn(bufCur.data(), bufNext.data());

    vector<float> out(256);
    auto demodFn = gp.Demodulate(giNow, giNext);
    demodFn(out.data(), bufNext.data(), bufCur.data());

    // Constant [0,248):
    //   out = (bufNext_mod * scale_dem + bufCur_mod) * level_dem
    //       = (B_next/level_mod * scale_dem + B_cur/scale_mod) * level_dem
    //       = (8/16 * 4 + 16/16) * 16
    //       = (0.5*4 + 1) * 16
    //       = (2 + 1) * 16 = 48
    const float expected = (B_next_val / level_dem * scale_dem + B_cur_val / scale_mod) * level_dem;
    for (int i = 0; i < 248; ++i)
        EXPECT_NEAR(out[i], expected, 1e-4f) << "at pos=" << i;
}
