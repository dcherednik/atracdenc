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

#include "compressed_io.h"
#include "at3p_gha.h"
#include <lib/bs_encode/encode.h>

namespace NAtracDEnc {

struct TScaledBlock;

struct TAt3PGhaData;

enum class ETonePackOrder : bool {
    ASC = false,
    DESC = true
};

struct TTonePackResult {
    struct TEntry {
        uint16_t Code;
        uint16_t Bits;
    };
    std::vector<TEntry> Data;
    int UsedBits;
    ETonePackOrder Order;
};

TTonePackResult CreateFreqBitPack(const TAt3PGhaData::TWaveParam* param, int len);

class TAt3PBitStream {
public:
    TAt3PBitStream(ICompressedOutput* container, uint16_t frameSz);
    void WriteFrame(int channels, const TAt3PGhaData* tonalData, const std::vector<std::vector<TScaledBlock>>& scaledBlocks);
private:
    ICompressedOutput* Container;
    TBitStreamEncoder Encoder;
    const uint32_t FrameSzToAllocBits;
    const uint16_t FrameSz;
};

}
