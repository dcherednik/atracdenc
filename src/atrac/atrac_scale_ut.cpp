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

#include "atrac_scale.h"

#include <gtest/gtest.h>

using namespace NAtracDEnc;

TEST(Quant, SaveEnergyLost) {
    struct TTestData {
        const std::vector<float> In;
        const float S;
        const float Q;
        const float Diff;
    };

    const std::vector<TTestData> testData {
        {{-2.35, -0.84,  0.65, -1.39,  1.25, -0.41, -0.85,  0.89}, 2.35001, 2.5, 0.5},
        {{-1.26,  1.26, -1.26,  1.26, -1.26,  1.26, -1.26,  1.26}, 2.35001, 2.5, 0.4},
        {{-0.32,  0.13,  0.28,  0.35,  0.63,  0.86,  0.63,  0.04}, 1, 15.5, 0.03}
    };

    for (const auto& td : testData) {
        const std::vector<float>& in = td.In;
        const float scale = td.S;

        float e1 = 0.0;

        std::vector<float> scaled;
        scaled.reserve(in.size());

        for (auto x : in) {
            e1 += x * x;
            scaled.push_back(x / scale);
        }

        std::vector<int> mantisas;
        mantisas.resize(in.size());

        QuantMantisas(scaled.data(), 0, mantisas.size(), td.Q, true, mantisas.data());

        float e2 = 0.0;
        for (auto x : mantisas) {
            auto t = x * (scale / td.Q);
            e2 += t * t;
        }

        EXPECT_TRUE(std::abs(e2 - e1) < td.Diff);
        std::cerr << "(e1): " << e1 << " (e2): " << e2 << " (e2-e1) " << e2 - e1 << " (e2/e1) " << e2 / e1 << std::endl;
    }
}
