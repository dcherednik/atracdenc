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

#include <array>
#include <cstdint>
#include <cstddef>

namespace NAtracDEnc {

namespace NAt3p {

float InvMantTab(size_t i);

struct TVlcElement {
    int16_t Code;
    int16_t Len;
};

struct THuffTables {
    THuffTables();
    std::array<TVlcElement, 16> NumToneBands;
    std::array<std::array<TVlcElement, 256>, 112> VlcSpecs;
    std::array<std::array<TVlcElement, 8>, 4> WordLens;
    std::array<std::array<TVlcElement, 8>, 4> CodeTables;
};

struct TScaleTable {

    class TBlockSizeMod {
    public:
        constexpr bool ShortWin(uint8_t) const noexcept {
            return false;
        }
    };

    static constexpr uint32_t MaxBfus = 32;
    static constexpr uint32_t NumQMF = 16;
    static float ScaleTable[64];

    static constexpr uint32_t BlocksPerBand[NumQMF + 1] =
    {
        0, 8, 12, 16, 18, 20, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
    };
    static constexpr uint32_t SpecsPerBlock[MaxBfus] = {
        16,   16,   16,   16,   16,   16,   16,   16,
        32,   32,   32,   32,   32,   32,   32,   32,
        64,   64,   64,   64,   64,   64,   128,  128,
        128,  128,  128,  128,  128,  128,  128,  128
    };
    static constexpr uint32_t BlockSizeTab[MaxBfus + 1] = {
        0,    16,   32,   48,   64,   80,   96,   112,
        128,  160,  192,  224,  256,  288,  320,  352,
        384,  448,  512,  576,  640,  704,  768,  896,
        1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920,
        2048
    };
    static constexpr uint32_t const * const SpecsStartShort = &BlockSizeTab[0];

    static constexpr uint32_t const * const SpecsStartLong = &BlockSizeTab[0];
};

}

} // namespace NAtracDEnc
