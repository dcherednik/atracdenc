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
// YAML debug logging for the ATRAC3 gain control pipeline.
//
// Inspired by edge264's YAML logging approach (FOSDEM 2026):
//   - Every gain control decision is logged as a YAML stream (one document
//     per frame) so each frame's state is self-contained and grep-able.
//   - Field names match C++ variable names to make code↔log navigation easy.
//   - Comments explain enum values and computed context (e.g. # prev_E/cur_E).
//   - The output can be post-processed with Python for analysis or used to
//     craft custom gain curves for stress testing.
//
// Enable with --yaml-log <file> on the command line.
// Each YAML document covers one frame; bands/channels nest under it.
//
// Example output:
//   ---
//   frame: 42
//   time: 0.952  # seconds
//   channels:
//     - channel: 0
//       bands:
//         - band: 0
//           high_freq_ratio: 0.8721
//           overlap_ratio: 1.2340  # prev_E/cur_E; >1 means prev frame louder
//           dynamic_min_score: 2.2134
//           gain: [0.1200, 0.1500, 0.8900, ...]  # 32 subframe RMS values
//           next_level: 1.0800
//           curve_raw:
//             - {level: 2, loc: 8}
//           rms_cur: 0.023000
//           rms_next_mod: 0.019000
//           point0_level: 4
//           crossover: 12
//           curve_final:
//             - {level: 4, loc: 0}
//             - {level: 3, loc: 12}
//           max_gain: 5.1000
//           ratio: 4.7300
//           level_boost: 1
//           scale_boost: 0
//           total_boost: 1
//           gain_boost: 1

#include <ostream>
#include <iomanip>
#include <vector>

namespace NAtracDEnc {

// Restore ostream float format on scope exit.
struct TYamlFmtGuard {
    std::ostream& os;
    std::ios_base::fmtflags flags;
    std::streamsize prec;
    TYamlFmtGuard(std::ostream& o, int precision)
        : os(o), flags(o.flags()), prec(o.precision()) {
        o << std::fixed << std::setprecision(precision);
    }
    ~TYamlFmtGuard() { os.flags(flags); os.precision(prec); }
};

// Write a float array as a compact YAML inline sequence.
inline void YamlWriteFloatSeq(std::ostream& out, const float* v, size_t n, int precision = 4) {
    TYamlFmtGuard fmt(out, precision);
    out << "[";
    for (size_t i = 0; i < n; ++i) {
        if (i) out << ", ";
        out << v[i];
    }
    out << "]";
}

// Write a float vector as a compact YAML inline sequence.
inline void YamlWriteFloatSeq(std::ostream& out, const std::vector<float>& v, int precision = 4) {
    YamlWriteFloatSeq(out, v.data(), v.size(), precision);
}

} // namespace NAtracDEnc
