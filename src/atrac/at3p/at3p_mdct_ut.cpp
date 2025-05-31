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

#include "at3p_mdct.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>

using namespace NAtracDEnc;

TEST(TAtpMDCT, ZeroOneBlock) {
    std::array<float, 2048> specs;
    static std::array<float, 2048> zero;

    {
        TAt3pMDCT mdct;

        static TAt3pMDCT::THistBuf buff;

        TAt3pMDCT::TPcmBandsData pcm;
        for (size_t i = 0; i < pcm.size(); i++) {
            pcm[i] = &zero[i * 128];
        }

        mdct.Do(specs.data(), pcm, buff);

        for(auto s: specs)
            EXPECT_NEAR(s, 0.0, 0.0000000001);
    }

    {
        TAt3pMIDCT midct;

        TAt3pMIDCT::TPcmBandsData pcm;
        static TAt3pMIDCT::THistBuf buff;
        for (size_t i = 0; i < pcm.size(); i++) {
            pcm[i] = &zero[i * 128];
        }

        midct.Do(specs.data(), pcm, buff);

        for(size_t i = 0; i < zero.size(); ++i)
            EXPECT_NEAR(zero[i], 0.0, 0.0000000001);
    }
}


TEST(TAtpMDCT, DC) {
    std::array<float, 4096> specs;
    std::array<float, 2048> dc;

    for (size_t i = 0; i < dc.size(); i++) {
        dc[i] = 1.0;
    }

    {
        TAt3pMDCT mdct;

        static TAt3pMDCT::THistBuf buff;

        TAt3pMDCT::TPcmBandsData pcm;
        for (size_t i = 0; i < pcm.size(); i++) {
            pcm[i] = &dc[i * 128];
        }

        mdct.Do(specs.data(),        pcm, buff);
        mdct.Do(specs.data() + 2048, pcm, buff);
    }

    {
        TAt3pMIDCT midct;

        std::array<float, 2048> result;

        TAt3pMIDCT::TPcmBandsData pcm;

        static TAt3pMIDCT::THistBuf buff;

        for (size_t i = 0; i < pcm.size(); i++) {
            pcm[i] = &result[i * 128];
        }

        midct.Do(specs.data(),        pcm, buff);
        midct.Do(specs.data() + 2048, pcm, buff);

        for(size_t i = 0; i < result.size(); ++i) {
            EXPECT_NEAR(result[i], dc[i], 0.000001);
        }
    }
}
