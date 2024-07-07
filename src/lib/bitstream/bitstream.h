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
#include <iostream>
#include <vector>

namespace NBitStream {

static inline int MakeSign(int val, unsigned bits) {
    unsigned shift = 8 * sizeof(int) - bits;
    union { unsigned u; int s; } v = { (unsigned) val << shift };
    return v.s >> shift;
}

class TBitStream {
    std::vector<char> Buf;
    int BitsUsed = 0;
    int ReadPos = 0;
    public:
        TBitStream(const char* buf, int size);
        TBitStream();
        void Write(uint32_t val, int n);
        uint32_t Read(int n);
        unsigned long long GetSizeInBits() const;
        uint32_t GetBufSize() const { return Buf.size(); };
        const std::vector<char>& GetBytes() const {
			return Buf;
		}
};
} //NBitStream
