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
#include <cstdint>
#include <array>
#include <map>
#include <math.h>
#include "config.h"

namespace NBitStream {
    class TBitStream;
}

namespace NAtracDEnc {
namespace NAtrac1 {

class TAtrac1EncodeSettings {
public:
    enum class EWindowMode {
        EWM_NOTRANSIENT,
        EWM_AUTO
    };
private:
    const uint32_t BfuIdxConst = 0;
    const bool FastBfuNumSearch = false;
    EWindowMode WindowMode = EWindowMode::EWM_AUTO;
    const uint32_t WindowMask = 0;
public:
    TAtrac1EncodeSettings()
    {}
    TAtrac1EncodeSettings(uint32_t bfuIdxConst, bool fastBfuNumSearch, EWindowMode windowMode, uint32_t windowMask)
        : BfuIdxConst(bfuIdxConst)
        , FastBfuNumSearch(fastBfuNumSearch)
        , WindowMode(windowMode)
        , WindowMask(windowMask)
    {}
    uint32_t GetBfuIdxConst() const { return BfuIdxConst; }
    bool GetFastBfuNumSearch() const { return FastBfuNumSearch; }
    EWindowMode GetWindowMode() const {return WindowMode; }
    uint32_t GetWindowMask() const {return WindowMask; }
};

class TAtrac1Data {
public:
    class TBlockSizeMod {
        static std::array<int, 3> Parse(NBitStream::TBitStream* stream);
        static std::array<int, 3> Create(bool lowShort, bool midShort, bool hiShort) {
            std::array<int, 3> tmp;
            tmp[0] = lowShort ? 2 : 0;
            tmp[1] = midShort ? 2 : 0;
            tmp[2] = hiShort ? 3 : 0;
            return tmp;
        }
    public:
        bool ShortWin(uint8_t band) const noexcept {
            return LogCount[band];
        }

        TBlockSizeMod(NBitStream::TBitStream* stream)
            : LogCount(Parse(stream))
        {}

        TBlockSizeMod(bool lowShort, bool midShort, bool hiShort)
            : LogCount(Create(lowShort, midShort, hiShort))
        {}

        TBlockSizeMod()
            : LogCount({{0, 0, 0}})
        {}

        std::array<int, 3> LogCount;
    };
    static constexpr uint8_t MaxBfus = 52;
    static constexpr uint8_t NumQMF = 3;

    static constexpr uint32_t SpecsPerBlock[MaxBfus] = {
        8,  8,  8,  8,  4,  4,  4,  4,  8,  8,  8,  8,  6,  6,  6,  6, 6, 6, 6, 6,  // low band
        6,  6,  6,  6,  7,  7,  7,  7,  9,  9,  9,  9,  10, 10, 10, 10,             // middle band
        12, 12, 12, 12, 12, 12, 12, 12, 20, 20, 20, 20, 20, 20, 20, 20              // high band
    };
    static constexpr uint32_t BlocksPerBand[NumQMF + 1] = {0, 20, 36, 52};
    static constexpr uint32_t SpecsStartLong[MaxBfus] = {
        0,   8,   16,  24,  32,  36,  40,  44,  48,  56,  64,  72,  80,  86,  92,  98, 104, 110, 116, 122,
        128, 134, 140, 146, 152, 159, 166, 173, 180, 189, 198, 207, 216, 226, 236, 246,
        256, 268, 280, 292, 304, 316, 328, 340, 352, 372, 392, 412, 432, 452, 472, 492,
    };
    static constexpr uint32_t SpecsStartShort[MaxBfus] = {
        0,   32,  64,  96,  8,   40,  72,  104, 12,  44,  76,  108, 20,  52,  84,  116, 26,  58,  90, 122,
        128, 160, 192, 224, 134, 166, 198, 230, 141, 173, 205, 237, 150, 182, 214, 246,
        256, 288, 320, 352, 384, 416, 448, 480, 268, 300, 332, 364, 396, 428, 460, 492
    };
    static const uint32_t SoundUnitSize = 212;
    static constexpr uint32_t BfuAmountTab[8] = {20,  28,  32,  36, 40, 44, 48, 52};
    static const uint32_t BitsPerBfuAmountTabIdx = 3;
    static const uint32_t BitsPerIDWL = 4;
    static const uint32_t BitsPerIDSF = 6;

    static float ScaleTable[64];
    static float SineWindow[32];
    static uint32_t BfuToBand(uint32_t i) {
        if (i < 20)
            return 0;
        if (i < 36)
            return 1;
        return 2;
    }
public:
    static const uint32_t NumSamples = 512;
    TAtrac1Data() {
        if (ScaleTable[0] == 0) {
            for (uint32_t i = 0; i < 64; i++) {
                ScaleTable[i] = pow(2.0, (double)(i / 3.0 - 21.0));
            }
        }
        if (SineWindow[0] == 0) {
            for (uint32_t i = 0; i < 32; i++) {
                SineWindow[i] = sin((i + 0.5) * (M_PI / (2.0 * 32.0)));
            }
        }
    }
};

} //namespace NAtrac1
} //namespace NAtracDEnc
