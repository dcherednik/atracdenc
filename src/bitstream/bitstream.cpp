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
#include <cstdint>
#include "bitstream.h"

#ifndef BIGENDIAN_ORDER
#define NBITSTREAM__LITTLE_ENDIAN_CPU
#endif

namespace NBitStream {

union UBytes {
	uint32_t ui = 0;
	uint8_t bytes[4];
};

TBitStream::TBitStream(const char* buf, int size)
    : Buf(buf, buf+size)
{}

TBitStream::TBitStream()
{}

void TBitStream::Write(uint32_t val, int n) {
    if (n > 23 || n < 0)
        abort();
    const int bitsLeft = Buf.size() * 8 - BitsUsed;
    const int bitsReq = n - bitsLeft;
    const int bytesPos = BitsUsed / 8;
    const int overlap = BitsUsed % 8;

    if (overlap || bitsReq >= 0) {
        Buf.resize(Buf.size() + (bitsReq / 8 + (overlap ? 2 : 1 )), 0);
    }
    UBytes t;
    t.ui = (val << (32 - n) >> overlap);

    for (int i = 0; i < n/8 + (overlap ? 2 : 1); ++i) {
#ifdef NBITSTREAM__LITTLE_ENDIAN_CPU
        Buf[bytesPos+i] |= t.bytes[3-i];
#else
        Buf[bytesPos + i] |= t.bytes[i];
#endif
    }

    BitsUsed += n;
}

uint32_t TBitStream::Read(int n) {
    if (n >23 || n < 0)
        abort();
    const int bytesPos = ReadPos / 8;
    const int overlap = ReadPos % 8;

    UBytes t;
    for (int i = 0; i < n/8 + (overlap ? 2 : 1); ++i) {
#ifdef NBITSTREAM__LITTLE_ENDIAN_CPU
        t.bytes[3-i] = (uint8_t)Buf[bytesPos+i];
#else
        t.bytes[i] = (uint8_t)Buf[bytesPos+i];
#endif
    }
    
    t.ui = (t.ui << overlap >> (32 - n));
    ReadPos += n;
    return t.ui;
}

unsigned long long TBitStream::GetSizeInBits() const {
    return BitsUsed;
}

}
