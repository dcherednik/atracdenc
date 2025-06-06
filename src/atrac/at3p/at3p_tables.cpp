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

#include "util.h"
#include "at3p_tables.h"
#include "ff/atrac3plus_data.h"

#include <iostream>
#include <sstream>

namespace NAtracDEnc {
namespace NAt3p {

static struct TInvMantTab {
public:
    TInvMantTab() {
        Data[0] = 0.0; //unused (for zero lenght on the decoder)
        for (size_t i = 1; i < 8; i++) {
            Data[i] = 1.0 / atrac3p_mant_tab[i];
        }
    }
    float Data[8];
} InvMantTab_;

float InvMantTab(size_t i) { return InvMantTab_.Data[i]; }

static struct TScaleTableInitializer {
public:
    TScaleTableInitializer() {
        const std::array<float, 64> src = {
            0.027852058,  0.0350914,   0.044212341, 0.055704117,
            0.0701828,    0.088424683, 0.11140823,  0.1403656,
            0.17684937,   0.22281647,  0.2807312,   0.35369873,
            0.44563293,   0.5614624,   0.70739746,  0.89126587,
            1.1229248,    1.4147949,   1.7825317,   2.2458496,
            2.8295898,    3.5650635,   4.4916992,   5.6591797,
            7.130127,     8.9833984,   11.318359,   14.260254,
            17.966797,    22.636719,   28.520508,   35.933594,
            45.273438,    57.041016,   71.867188,   90.546875,
            114.08203,    143.73438,   181.09375,   228.16406,
            287.46875,    362.1875,    456.32812,   574.9375,
            724.375,      912.65625,   1149.875,    1448.75,
            1825.3125,    2299.75,     2897.5,      3650.625,
            4599.5,       5795.0,      7301.25,     9199.0,
            11590.0,      14602.5,     18398.0,     23180.0,
            29205.0,      36796.0,     46360.0,     58410.0
        };

        for (size_t i = 0; i < src.size(); i++) {
            TScaleTable::ScaleTable[i] = src[i] / src.back();
        }
    }
} ScaleTableInitializer_;

float TScaleTable::ScaleTable[64];
constexpr uint32_t TScaleTable::BlocksPerBand[NumQMF + 1];
constexpr uint32_t TScaleTable::SpecsPerBlock[MaxBfus];
constexpr uint32_t TScaleTable::BlockSizeTab[MaxBfus + 1];

static uint16_t atde_noinline GenHuffmanEncTable(const uint8_t *cb, const uint8_t *xlat, uint16_t outLen, TVlcElement* const out)
{
    uint16_t index = 0;
    uint16_t code = 0;
    for (int b = 1; b <= 12; b++) {
        for (int i = *cb++; i > 0; i--) {
            uint8_t val = xlat[index];
            if (val >= outLen) {
                std::stringstream ss;
                ss << "encoded value out of range, invalid haffman table, got: " << (int)val << " table len: " << (int)outLen;
                throw std::runtime_error(ss.str());
            }
            TVlcElement* cur = out + val;
            cur->Code = code;
            cur->Len = b;
            index++;
            code++;
        }
        code <<= 1;
    }
    return index;
}

template<typename T>
uint16_t GenHuffmanEncTable(const uint8_t *cb, const uint8_t *xlat, T& out)
{
    return GenHuffmanEncTable(cb, xlat, out.size(), out.data());
}

THuffTables::THuffTables()
{
    GenHuffmanEncTable(&atrac3p_tone_cbs[0][0], &atrac3p_tone_xlats[0], NumToneBands);

    for (size_t i = 0; i < VlcSpecs.size(); i++) {
        for (size_t j = 0; j < VlcSpecs[i].size(); j++) {
            VlcSpecs[i][j].Code = 0;
            VlcSpecs[i][j].Len = 0;
        }
    }

    for (size_t i = 0, x = 0; i < WordLens.size(); i++) {
        x += GenHuffmanEncTable(&atrac3p_wl_cbs[i][0], &atrac3p_wl_ct_xlats[x], WordLens[i]);
        x += GenHuffmanEncTable(&atrac3p_ct_cbs[i][0], &atrac3p_wl_ct_xlats[x], CodeTables[i]);
    }

    for (int i = 0, x = 0; i < 112; i++) {
        if (atrac3p_spectra_cbs[i][0] >= 0) {
            x += GenHuffmanEncTable((uint8_t*)&atrac3p_spectra_cbs[i][0], &atrac3p_spectra_xlats[x], VlcSpecs[i]);
        } else {
            VlcSpecs[i] = VlcSpecs[-atrac3p_spectra_cbs[i][0]];
        }
    }
}

} // namespace NAt3p
} // namespace NAtracDEnc
