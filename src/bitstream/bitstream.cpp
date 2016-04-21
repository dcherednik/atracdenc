#include "bitstream.h"
namespace NBitStream {

union TMix {
	unsigned long long ull = 0;
	uint8_t bytes[8];
};

TBitStream::TBitStream(const char* buf, int size)
    : Buf(buf, buf+size)
{}
TBitStream::TBitStream()
{}

void TBitStream::Write(unsigned long long val, int n) {
	if (n > 23 || n < 0)
		abort();
    const int bitsLeft = Buf.size() * 8 - BitsUsed;
    const int bitsReq = n - bitsLeft;
    const int bytesPos = BitsUsed / 8;
    const int overlap = BitsUsed % 8;

    if (overlap || bitsReq > 0) {
        Buf.resize(Buf.size() + (bitsReq / 8 + (overlap ? 2 : 1 )), 0);
    }
	TMix t;
    t.ull	= val;
	t.ull = (t.ull << (64 - n) >> overlap);

	for (int i = 0; i < n/8 + (overlap ? 2 : 1); ++i) {
		Buf[bytesPos+i] |= t.bytes[7-i];
	}

    BitsUsed += n;
}
unsigned long long TBitStream::Read(int n) {
	if (n >23 || n < 0)
		abort();
    const int bytesPos = ReadPos / 8;
    const int overlap = ReadPos % 8;
	TMix t;
	for (int i = 0; i < n/8 + (overlap ? 2 : 1); ++i) {
		t.bytes[7-i] = (uint8_t)Buf[bytesPos+i];
	}
    
	t.ull = (t.ull << overlap >> (64 - n));
	ReadPos += n;
    return t.ull;
}

unsigned long long TBitStream::GetSizeInBits() const {
    return BitsUsed;
}

}
