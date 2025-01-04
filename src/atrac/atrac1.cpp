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

#include "atrac1.h"
#include "bitstream/bitstream.h"

namespace NAtracDEnc {
namespace NAtrac1 {

constexpr uint32_t TAtrac1Data::BlocksPerBand[NumQMF + 1];
constexpr uint32_t TAtrac1Data::SpecsPerBlock[MaxBfus];
constexpr uint32_t TAtrac1Data::SpecsStartLong[MaxBfus];
constexpr uint32_t TAtrac1Data::SpecsStartShort[MaxBfus];
constexpr uint32_t TAtrac1Data::BfuAmountTab[8];
float TAtrac1Data::ScaleTable[64] = {0};
float TAtrac1Data::SineWindow[32] = {0};

const static TAtrac1Data Atrac1Data;

std::array<int, 3> TAtrac1Data::TBlockSizeMod::Parse(NBitStream::TBitStream* stream) {
    std::array<int, 3> tmp;
    tmp[0] = 2 - stream->Read(2);
    tmp[1] = 2 - stream->Read(2);
    tmp[2] = 3 - stream->Read(2);
    stream->Read(2); //skip unused 2 bits
    return tmp;
}

} //namespace NAtrac1
} //namespace NAtracDEnc
