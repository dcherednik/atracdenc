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
 * One-off offline generator for src/atrac/at3p/at3p_pqf_wideband_table.h.
 *
 * Phase 1 experiment tool (see plan: "wideband GHA + analytic PQF-domain
 * projection"). Unlike pqf_boundary_calibrate.cpp (which only measured the
 * two subbands immediately flanking each of the 7 GHA subband boundaries,
 * over a narrow +-40Hz window), this probes the REAL production PQF filter
 * (at3plus_pqf_do_analyse) with synthetic sinusoids swept across the FULL
 * 0-11025Hz range covered by GHA's 8 subbands, and measures the complex gain
 * in ALL 8 subbands at every probe frequency. This directly answers, from
 * measurement rather than assumption:
 *   - how far a tone's energy leaks beyond its immediate neighbor subband
 *     (the "aliasing" question -- does subband k+2, k+3... ever pick up
 *     non-negligible energy from a tone nominally in subband k?)
 *   - the sign/parity ("spectral inversion") pattern of each subband's local
 *     frequency axis, empirically, subband by subband
 *   - the same omega=0/pi model singularity found in the boundary-only tool
 *     recurs at EVERY subband's own edges (not just the 7 shared
 *     boundaries) -- repaired the same way, generically, wherever it's
 *     detected (not hardcoded to known boundary positions).
 *
 * Frequency/magnitude/phase are measured via Goertzel correlation +
 * golden-section search (NOT libgha's gha_search_omega_newton, which is
 * structurally ill-conditioned near omega=0/pi).
 *
 * Not part of the CMake build. To regenerate (run from repo root):
 *
 *   g++ -O2 -std=c++17 -I src -Dkiss_fft_scalar=float \
 *       src/atrac/at3p/tools/pqf_wideband_calibrate.cpp \
 *       src/atrac/atrac3plus_pqf/atrac3plus_pqf.c \
 *       src/lib/mdct/mdct.cpp \
 *       src/lib/fft/kissfft_impl/kiss_fft.c \
 *       -o /tmp/pqf_wideband_calibrate
 *   /tmp/pqf_wideband_calibrate > src/atrac/at3p/at3p_pqf_wideband_table.h
 *
 * Watch stderr: it prints a leakage-vs-distance summary (max magnitude
 * ratio, in dB relative to the home subband, seen at |sb - home| = 1, 2,
 * 3...) -- this is the data the "how many neighbor subbands do we need to
 * model" decision should be based on.
 */

#include <atrac/atrac3plus_pqf/atrac3plus_pqf.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr double kFs = 44100.0;
constexpr int kFrameSz = 2048;
constexpr int kSubbandSz = 128;
constexpr int kWarmupFrames = 4;
constexpr int kMeasureFrames = 16;
constexpr int kMeasureSamples = kMeasureFrames * kSubbandSz; // 2048
constexpr int kGhaSubbands = 8; // matches TGhaProcessor::SUBBANDS
constexpr double kSubbandBw = kFs / 32.0; // 1378.125 Hz
constexpr double kFreqMax = kGhaSubbands * kSubbandBw; // 11025 Hz

using Complex = std::complex<double>;

// Same proven methodology as pqf_boundary_calibrate.cpp -- see that file for
// the derivation of the magnitude/phase recovery formula.
Complex Goertzel(const float* x, int n, double omega) {
    Complex acc(0.0, 0.0);
    const double norm = 2.0 * M_PI / (n - 1);
    for (int i = 0; i < n; i++) {
        double w = 0.5 * (1.0 - std::cos(norm * i));
        Complex e(std::cos(omega * i), -std::sin(omega * i));
        acc += w * (double)x[i] * e;
    }
    return acc;
}

double Mag2(const float* x, int n, double omega) {
    Complex c = Goertzel(x, n, omega);
    return c.real() * c.real() + c.imag() * c.imag();
}

double GoldenSectionMaxOmega(const float* x, int n, double a, double b, int iters = 40) {
    const double gr = (std::sqrt(5.0) - 1.0) / 2.0;
    double c = b - gr * (b - a);
    double d = a + gr * (b - a);
    for (int i = 0; i < iters; i++) {
        if (Mag2(x, n, c) < Mag2(x, n, d)) {
            a = c;
        } else {
            b = d;
        }
        c = b - gr * (b - a);
        d = a + gr * (b - a);
    }
    return (a + b) / 2.0;
}

double CoarseSearchOmega(const float* x, int n) {
    const int kSteps = 256;
    double best = 0.0;
    double bestMag2 = -1.0;
    for (int i = 1; i < kSteps; i++) {
        double omega = M_PI * i / kSteps;
        double m2 = Mag2(x, n, omega);
        if (m2 > bestMag2) {
            bestMag2 = m2;
            best = omega;
        }
    }
    double lo = std::max(0.0, best - M_PI / kSteps);
    double hi = std::min(M_PI, best + M_PI / kSteps);
    return GoldenSectionMaxOmega(x, n, lo, hi);
}

struct TMeasured {
    double Omega;
    double Magnitude;
    double Phase; // x[n] ~= Magnitude * sin(Omega*n + Phase)
};

TMeasured Recover(const float* x, int n, double omega) {
    Complex X = Goertzel(x, n, omega);
    double mag = 4.0 * std::abs(X) / n;
    double phase = std::arg(X) + M_PI / 2.0;
    while (phase < 0) phase += 2 * M_PI;
    while (phase >= 2 * M_PI) phase -= 2 * M_PI;
    return {omega, mag, phase};
}

double ResidualCheck(const float* x, int n, const TMeasured& m) {
    double num = 0.0;
    for (int i = 0; i < n; i++) {
        double pred = m.Magnitude * std::sin(m.Omega * i + m.Phase);
        double diff = x[i] - pred;
        num += diff * diff;
    }
    return std::sqrt(num / n);
}

// Probe the real PQF with a continuous-phase unit-amplitude sine at
// `freqHz`, and return the last kMeasureSamples subband-domain samples for
// ALL 8 GHA subbands (one probe covers every subband at once -- the PQF
// call itself always computes all 16 subbands regardless of how many we
// read).
void ProbeAllSubbands(double freqHz, std::vector<std::vector<float>>* subbands) {
    at3plus_pqf_a_ctx_t ctx = at3plus_pqf_create_a_ctx();

    subbands->assign(kGhaSubbands, std::vector<float>(kMeasureSamples, 0.0f));

    std::vector<float> in(kFrameSz);
    std::vector<float> out(kFrameSz);

    const int totalFrames = kWarmupFrames + kMeasureFrames;
    const double omegaOrig = 2.0 * M_PI * freqHz / kFs;

    int measuredFrames = 0;
    // Zero-reference the probe's phase at the START OF THE MEASUREMENT
    // WINDOW (n0 = kWarmupFrames*kFrameSz), not at the probe's own n=0.
    // Recover() below fits the subband output as Mag*sin(Omega*i+Phase)
    // with i local to the measurement window (i=0 at n0); for that Phase to
    // be purely the filter's own phase response theta_sb(f) -- so that a
    // caller can later do predicted_phase = tone_phase + table_phase,
    // referencing both consistently to their own window starts -- the
    // INPUT must already have zero phase at n0. Without this, Phase would
    // be contaminated by (omegaOrig*n0 mod 2pi), which is an artifact of
    // this calibration run's arbitrary warmup length, not a filter
    // property, and silently breaks the additive phase-composition formula
    // used downstream (confirmed by a direct reconstruction test: skipping
    // this shift gave ~-5dB SNR reconstructing a plain mid-passband tone
    // that should be a near-perfect, trivial case).
    const long n0 = (long)kWarmupFrames * kFrameSz;
    for (int f = 0; f < totalFrames; f++) {
        for (int i = 0; i < kFrameSz; i++) {
            long n = (long)f * kFrameSz + i - n0;
            in[i] = (float)std::sin(omegaOrig * n);
        }
        at3plus_pqf_do_analyse(ctx, in.data(), out.data());

        if (f >= kWarmupFrames) {
            for (int sb = 0; sb < kGhaSubbands; sb++) {
                memcpy((*subbands)[sb].data() + measuredFrames * kSubbandSz,
                       out.data() + sb * kSubbandSz, sizeof(float) * kSubbandSz);
            }
            measuredFrames++;
        }
    }

    at3plus_pqf_free_a_ctx(ctx);
}

struct TWidebandPoint {
    double FreqHz;
    TMeasured Sb[kGhaSubbands];
    double Residual[kGhaSubbands] = {0};
};

// Build the non-uniform probe grid: fine step near each of the 7 known
// subband boundaries and the two outer edges (0Hz, 11025Hz), coarse step
// elsewhere. The repair pass below does not rely on this grid knowing where
// boundaries are (it detects badness generically), but a fine step is still
// needed there to have enough repair-neighbor points close to the
// singularity, and a fine step at the outer edges checks whether subband0's
// DC edge and subband7's 11025Hz edge show the same singularity.
std::vector<double> BuildFrequencyGrid() {
    std::vector<double> special;
    for (int b = 0; b <= kGhaSubbands; b++) {
        special.push_back(b * kSubbandBw); // 0, 1378.125, ..., 11025
    }

    std::vector<double> grid;
    const double kFineStep = 1.0;
    const double kFineHalfWidth = 15.0;
    const double kCoarseStep = 20.0;

    for (double f = 0.0; f <= kFreqMax + 1e-6; f += kCoarseStep) {
        grid.push_back(f);
    }
    for (double c : special) {
        for (double f = std::max(0.0, c - kFineHalfWidth); f <= std::min(kFreqMax, c + kFineHalfWidth) + 1e-6;
             f += kFineStep) {
            grid.push_back(f);
        }
    }
    // A probe frequency of exactly 0Hz is degenerate (not a sine at all);
    // nudge it up slightly.
    for (double& f : grid) {
        if (f < 1.0) f = 1.0;
    }

    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }),
               grid.end());
    return grid;
}

std::vector<TWidebandPoint> SweepFullBand() {
    std::vector<double> grid = BuildFrequencyGrid();
    std::vector<TWidebandPoint> points;
    points.reserve(grid.size());

    for (size_t gi = 0; gi < grid.size(); gi++) {
        double freqHz = grid[gi];
        std::vector<std::vector<float>> subbands;
        ProbeAllSubbands(freqHz, &subbands);

        TWidebandPoint p;
        p.FreqHz = freqHz;
        for (int sb = 0; sb < kGhaSubbands; sb++) {
            double omega = CoarseSearchOmega(subbands[sb].data(), kMeasureSamples);
            TMeasured m = Recover(subbands[sb].data(), kMeasureSamples, omega);
            p.Sb[sb] = m;
            p.Residual[sb] = ResidualCheck(subbands[sb].data(), kMeasureSamples, m);
        }
        points.push_back(p);

        if (gi % 100 == 0) {
            std::fprintf(stderr, "swept %zu/%zu (freq=%.1f Hz)\n", gi, grid.size(), freqHz);
        }
    }
    return points;
}

// Generic repair, same principle as pqf_boundary_calibrate.cpp's
// RepairSingularity: detect bad points (magnitude below floor, or residual
// too large relative to magnitude -- the omega=0/pi model singularity) and
// linearly interpolate from the nearest good neighbors, per subband,
// wherever it occurs across the whole sweep (not just at known boundaries).
void RepairSingularities(std::vector<TWidebandPoint>* points) {
    const double kRelBad = 0.02;
    const double kMagFloor = 1.0;

    auto isBad = [&](size_t i, int sb) {
        double mag = (*points)[i].Sb[sb].Magnitude;
        double res = (*points)[i].Residual[sb];
        return mag < kMagFloor || (res / std::max(mag, kMagFloor)) > kRelBad;
    };

    auto unwrapLerp = [](double phaseA, double phaseB, double t) {
        double d = std::fmod(phaseB - phaseA + M_PI, 2 * M_PI);
        if (d < 0) d += 2 * M_PI;
        d -= M_PI;
        double r = phaseA + d * t;
        while (r < 0) r += 2 * M_PI;
        while (r >= 2 * M_PI) r -= 2 * M_PI;
        return r;
    };

    for (int sb = 0; sb < kGhaSubbands; sb++) {
        size_t n = points->size();
        size_t i = 0;
        int repairedRuns = 0;
        while (i < n) {
            if (!isBad(i, sb)) { i++; continue; }
            size_t j = i;
            while (j < n && isBad(j, sb)) j++;
            if (i == 0 || j == n) {
                std::fprintf(stderr, "warning: sb=%d bad run [%zu,%zu) touches sweep edge, cannot repair\n", sb, i, j);
                i = j;
                continue;
            }
            const TMeasured& a = (*points)[i - 1].Sb[sb];
            const TMeasured& b = (*points)[j].Sb[sb];
            double freqA = (*points)[i - 1].FreqHz;
            double freqB = (*points)[j].FreqHz;
            for (size_t k = i; k < j; k++) {
                double t = (points->at(k).FreqHz - freqA) / (freqB - freqA);
                TMeasured m;
                m.Magnitude = a.Magnitude + (b.Magnitude - a.Magnitude) * t;
                m.Omega = a.Omega + (b.Omega - a.Omega) * t;
                m.Phase = unwrapLerp(a.Phase, b.Phase, t);
                (*points)[k].Sb[sb] = m;
            }
            repairedRuns++;
            i = j;
        }
        if (repairedRuns > 0) {
            std::fprintf(stderr, "sb=%d: repaired %d singularity run(s)\n", sb, repairedRuns);
        }
    }
}

// The core "aliasing" diagnostic: for each probe point, find the home
// subband (max magnitude), then for every OTHER subband record its
// magnitude ratio (dB) relative to home, bucketed by |sb - home|. Prints the
// worst-case (max) ratio per distance across the whole sweep -- this is the
// data the neighbor-count decision should be based on.
void PrintLeakageDiagnostic(const std::vector<TWidebandPoint>& points) {
    double worstDb[kGhaSubbands] = {0};
    for (double& v : worstDb) v = -1e300;

    for (const auto& p : points) {
        int home = 0;
        for (int sb = 1; sb < kGhaSubbands; sb++) {
            if (p.Sb[sb].Magnitude > p.Sb[home].Magnitude) home = sb;
        }
        double homeMag = std::max(p.Sb[home].Magnitude, 1.0);
        for (int sb = 0; sb < kGhaSubbands; sb++) {
            int dist = std::abs(sb - home);
            if (dist == 0) continue;
            double ratio = p.Sb[sb].Magnitude / homeMag;
            double db = 20.0 * std::log10(std::max(ratio, 1e-9));
            worstDb[dist] = std::max(worstDb[dist], db);
        }
    }

    std::fprintf(stderr, "\n=== leakage-vs-distance diagnostic ===\n");
    for (int dist = 1; dist < kGhaSubbands; dist++) {
        if (worstDb[dist] < -200) continue; // never observed at this distance
        std::fprintf(stderr, "  |sb - home| = %d : worst-case leakage = %.1f dB\n", dist, worstDb[dist]);
    }
    std::fprintf(stderr, "=======================================\n\n");
}

std::string FormatFloatLiteral(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    std::string s(buf);
    if (s.find_first_of(".eE") == std::string::npos) {
        s += ".0";
    }
    s += "f";
    return s;
}

void EmitTable(FILE* out, const std::vector<TWidebandPoint>& points) {
    std::fprintf(out, "/* GENERATED FILE. See src/atrac/at3p/tools/pqf_wideband_calibrate.cpp */\n");
    std::fprintf(out, "#pragma once\n\n");
    std::fprintf(out, "#include <cstddef>\n\n");
    std::fprintf(out, "namespace NAtracDEnc {\n\n");
    std::fprintf(out, "struct TPqfWidebandPoint {\n");
    std::fprintf(out, "    float FreqHz;\n");
    std::fprintf(out, "    float Magnitude, Phase, Omega;\n");
    std::fprintf(out, "};\n\n");
    std::fprintf(out, "constexpr size_t PqfWidebandTableSize = %zu;\n", points.size());
    std::fprintf(out, "constexpr size_t PqfWidebandSubbands = %d;\n\n", kGhaSubbands);

    for (int sb = 0; sb < kGhaSubbands; sb++) {
        std::fprintf(out, "static const TPqfWidebandPoint PqfWidebandTable%d[] = {\n", sb);
        for (const auto& p : points) {
            std::fprintf(out, "    { %s, %s, %s, %s },\n", FormatFloatLiteral(p.FreqHz).c_str(),
                         FormatFloatLiteral(p.Sb[sb].Magnitude).c_str(), FormatFloatLiteral(p.Sb[sb].Phase).c_str(),
                         FormatFloatLiteral(p.Sb[sb].Omega).c_str());
        }
        std::fprintf(out, "};\n\n");
    }

    std::fprintf(out, "static const TPqfWidebandPoint* const PqfWidebandTables[PqfWidebandSubbands] = {\n");
    for (int sb = 0; sb < kGhaSubbands; sb++) {
        std::fprintf(out, "    PqfWidebandTable%d,\n", sb);
    }
    std::fprintf(out, "};\n\n");
    std::fprintf(out, "} // namespace NAtracDEnc\n");
}

} // namespace

int main() {
    std::fprintf(stderr, "sweeping full band 0-%.1f Hz across %d subbands...\n", kFreqMax, kGhaSubbands);
    std::vector<TWidebandPoint> points = SweepFullBand();
    std::fprintf(stderr, "swept %zu points total\n", points.size());

    PrintLeakageDiagnostic(points);
    RepairSingularities(&points);

    EmitTable(stdout, points);
    return 0;
}
