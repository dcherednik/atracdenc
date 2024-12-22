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

#include "atrac3.h"
#include <algorithm>

namespace NAtracDEnc {
namespace NAtrac3 {

constexpr uint32_t TAtrac3Data::BlockSizeTab[33];
constexpr uint32_t TAtrac3Data::ClcLengthTab[8];
constexpr float TAtrac3Data::MaxQuant[8];
constexpr uint32_t TAtrac3Data::BlocksPerBand[4 + 1];
constexpr uint32_t TAtrac3Data::SpecsPerBlock[33];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable1[HuffTable1Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable2[HuffTable2Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable3[HuffTable3Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable5[HuffTable5Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable6[HuffTable6Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable7[HuffTable7Sz];
constexpr TAtrac3Data::THuffTablePair TAtrac3Data::HuffTables[7];

constexpr TContainerParams TAtrac3Data::ContainerParams[8];
float TAtrac3Data::EncodeWindow[256] = {0};
float TAtrac3Data::DecodeWindow[256] = {0};
float TAtrac3Data::ScaleTable[64] = {0};
float TAtrac3Data::GainLevel[16];
float TAtrac3Data::GainInterpolation[31];

static const TAtrac3Data Atrac3Data;

const TContainerParams* TAtrac3Data::GetContainerParamsForBitrate(uint32_t bitrate) {
    // Set default to LP2 mode
    if (bitrate == 0) {
        bitrate = 132300;
    }
    return std::lower_bound(ContainerParams, ContainerParams+8, bitrate);
}

} // namespace NAtrac3
} // namespace NAtracDEnc
