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


#include "at3p_bitstream.h"
#include <lib/bitstream/bitstream.h>
#include <env.h>
#include <iostream>

namespace NAtracDEnc {

TAt3PBitStream::TAt3PBitStream(ICompressedOutput* container, uint16_t frameSz)
    : Container(container)
    , FrameSz(frameSz)
{
    NEnv::SetRoundFloat();
}

void TAt3PBitStream::WriteFrame(int channels)
{
    NBitStream::TBitStream bitStream;
    // First bit must be zero
    bitStream.Write(0, 1);
    // Channel block type
    // 0 - MONO block
    // 1 - STEREO block
    // 2 - Nobody know
    bitStream.Write(channels - 1, 2);

    // Skip some bits to produce correct zero bitstream
    bitStream.Write(0, 10);
    if (channels == 2) {
        bitStream.Write(0, 14);
    } else {
        bitStream.Write(0, 5);
    }
    // Terminator
    bitStream.Write(3, 2);

    std::vector<char> buf = bitStream.GetBytes();

    buf.resize(FrameSz);
    Container->WriteFrame(buf);
}

}
