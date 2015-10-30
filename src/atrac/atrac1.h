#pragma once
#include <cstdint>
#include <array>
#include <map>
#include <math.h>
#include "../bitstream/bitstream.h"
const int QMF_BANDS = 3;
const int MAX_BFUS = 52;

class TAtrac1Data {
protected:
	static constexpr uint32_t SpecsPerBlock[MAX_BFUS] = {
        8,  8,  8,  8,  4,  4,  4,  4,  8,  8,  8,  8,  6,  6,  6,  6, 6, 6, 6, 6,  // low band
        6,  6,  6,  6,  7,  7,  7,  7,  9,  9,  9,  9,  10, 10, 10, 10,             // middle band
        12, 12, 12, 12, 12, 12, 12, 12, 20, 20, 20, 20, 20, 20, 20, 20              // high band
	};
	static constexpr uint32_t BlocksPerBand[QMF_BANDS + 1] = {0, 20, 36, 52};
	static constexpr uint32_t SpecsStartLong[MAX_BFUS] = {
        0,   8,   16,  24,  32,  36,  40,  44,  48,  56,  64,  72,  80,  86,  92,  98, 104, 110, 116, 122,
        128, 134, 140, 146, 152, 159, 166, 173, 180, 189, 198, 207, 216, 226, 236, 246,
        256, 268, 280, 292, 304, 316, 328, 340, 352, 372, 392, 412, 432, 452, 472, 492,
	};
    static constexpr uint32_t SpecsStartShort[MAX_BFUS] = {
        0,   32,  64,  96,  8,   40,  72,  104, 12,  44,  76,  108, 20,  52,  84,  116, 26,  58,  90, 122,
        128, 160, 192, 224, 134, 166, 198, 230, 141, 173, 205, 237, 150, 182, 214, 246,
        256, 288, 320, 352, 384, 416, 448, 480, 268, 300, 332, 364, 396, 428, 460, 492
    };
    static const uint32_t SoundUnitSize = 212;
    static constexpr uint32_t BfuAmountTab[8] = {20,  28,  32,  36, 40, 44, 48, 52};
	static const uint32_t BitsPerBfuAmountTabIdx = 3;
	static const uint32_t BitsPerIDWL = 4;
	static const uint32_t BitsPerIDSF = 6;
    static const uint32_t NumSamples = 512;

    static double ScaleTable[64];
    static double SineWindow[32];
public:
    TAtrac1Data() {
        if (ScaleTable[0] == 0) {
            for (uint32_t i = 0; i < 64; i++) {
                ScaleTable[i] = pow(2.0, (double)(i - 15.0) / 3.0);
            }
        }
        if (SineWindow[0] == 0) {
            for (uint32_t i = 0; i < 32; i++) {
                SineWindow[i] = sin((i + 0.5) * (M_PI / (2.0 * 32.0)));
            }
        }
    }
};

class TBlockSize {
    static std::array<int, QMF_BANDS> Parse(NBitStream::TBitStream* stream) {
        std::array<int,QMF_BANDS> tmp;
        tmp[0] = 2 - stream->Read(2);
        tmp[1] = 2 - stream->Read(2);
        tmp[2] = 3 - stream->Read(2);
        stream->Read(2); //skip unused 2 bits
        return tmp;
    }
public:
    TBlockSize(NBitStream::TBitStream* stream)
        : LogCount(Parse(stream))
    {}
    TBlockSize()
        : LogCount({0,0,0}) //windows are long
    {}
    const std::array<int,QMF_BANDS> LogCount;
};


