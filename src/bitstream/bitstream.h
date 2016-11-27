#pragma once
#include <vector>

#include <iostream>

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
