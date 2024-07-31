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

#include "at3p_tables.h"
#include "ff/atrac3plus_data.h"

#include <iostream>

namespace NAtracDEnc {
namespace NAt3p {

static void __attribute__ ((noinline)) GenHuffmanEncTable(const uint8_t *cb, const uint8_t *xlat, uint16_t outLen, TVlcElement* out)
{
    uint16_t index = 0;
    uint16_t code = 0;
    for (int b = 1; b <= 12; b++) {
        for (int i = *cb++; i > 0; i--) {
            uint8_t val = xlat[index];
            if (val > outLen) {
                throw std::runtime_error("encoded value out of range, invalid haffman table");
            }
            TVlcElement* cur = out + val;
            cur->Code = code;
            cur->Len = b;
            index++;
            code++;
        }
        code <<= 1;
    }
}

template<typename T>
void GenHuffmanEncTable(const uint8_t *cb, const uint8_t *xlat, T& out)
{
    GenHuffmanEncTable(cb, xlat, out.size(), out.data());
}

THuffTables::THuffTables()
{
    GenHuffmanEncTable(&atrac3p_tone_cbs[0][0], &atrac3p_tone_xlats[0], NumToneBands);
}

} // namespace NAt3p
} // namespace NAtracDEnc
