#include "atrac1_bitalloc.h"
#include "atrac1_scale.h"
#include "atrac1.h"
#include <math.h>
#include "../bitstream/bitstream.h"
namespace NAtrac1 {

using std::vector;
using std::cerr;
using std::endl;

static const uint32_t FixedBitAllocTableLong[MAX_BFUS] = {
    6, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
    6, 6, 5, 5,  5, 5, 5, 5,  5,  5,  5,  5,  5,  4, 4,  4, 5,
    5, 4, 4,  3,  3,  3, 3, 2,  0,  0,  0,  0,  0,  0,  0
};

//returns 1 for tone-like, 0 - noise-like
static double AnalizeSpread(const std::vector<TScaledBlock>& scaledBlocks) {
    double s = 0.0;
    for (int i = 0; i < scaledBlocks.size(); ++i) {
        s += scaledBlocks[i].ScaleFactorIndex;
    }
    s /= scaledBlocks.size();
    double sigma = 0.0;
    double xxx = 0.0;
    for (int i = 0; i < scaledBlocks.size(); ++i) {
        xxx = (scaledBlocks[i].ScaleFactorIndex - s);
        xxx *= xxx;
        sigma += xxx;
    }
    sigma /= scaledBlocks.size();
    sigma = sqrt(sigma);
    if (sigma > 14.0)
        sigma = 14.0;
    return sigma/14.0;
}

vector<uint32_t> TAtrac1SimpleBitAlloc::CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks, const double spread, const double shift) {
    vector<uint32_t> bitsPerEachBlock;
    bitsPerEachBlock.resize(scaledBlocks.size());
    for (int i = 0; i < scaledBlocks.size(); ++i) {
        int tmp = spread * ( (double)scaledBlocks[i].ScaleFactorIndex/5.2) + (1.0 - spread) * (FixedBitAllocTableLong[i] + 1) - shift;
        if (tmp > 16) {
            bitsPerEachBlock[i] = 16;
        } else if (tmp < 2) {
            bitsPerEachBlock[i] = 0;
        } else {
            bitsPerEachBlock[i] = tmp;
        }
    }
    return bitsPerEachBlock;	
}

uint32_t TAtrac1SimpleBitAlloc::Write(const std::vector<TScaledBlock>& scaledBlocks) {
    uint32_t bfuIdx = 7;
    vector<uint32_t> bitsPerEachBlock;
    double spread = AnalizeSpread(scaledBlocks);
    bitsPerEachBlock.resize(scaledBlocks.size());
    const uint32_t bitsAvaliablePerBfus =  SoundUnitSize * 8 - BitsPerBfuAmountTabIdx - 32 - 2 - 3 - bitsPerEachBlock.size() * (BitsPerIDWL + BitsPerIDSF);
    double maxShift = 4;
    double minShift = -2;
    double shift = 3.0;
    const uint32_t maxBits = bitsAvaliablePerBfus;
    const uint32_t minBits = bitsAvaliablePerBfus - 110;
 
    for(;;) {   
        const vector<uint32_t>& tmpAlloc = CalcBitsAllocation(scaledBlocks, spread, shift);
        uint32_t bitsUsed = 0;
        for (int i = 0; i < tmpAlloc.size(); i++) {
            bitsUsed += SpecsPerBlock[i] * tmpAlloc[i];
        }

        if (bitsUsed < minBits) {
            maxShift = shift;
            shift -= (shift - minShift) / 2;
        } else if (bitsUsed > maxBits) {
            minShift = shift;
            shift += (maxShift - shift) / 2;
        } else {
            bitsPerEachBlock = tmpAlloc;
            break;
        }
    }
    WriteBitStream(bitsPerEachBlock, scaledBlocks, bfuIdx);
    return BfuAmountTab[bfuIdx];
}

void TAtrac1BitStreamWriter::WriteBitStream(const vector<uint32_t>& bitsPerEachBlock, const std::vector<TScaledBlock>& scaledBlocks, uint32_t bfuAmountIdx) {
    NBitStream::TBitStream bitStream;
    int bitUsed = 0;
    if (bfuAmountIdx >= (1 << BitsPerBfuAmountTabIdx)) {
        cerr << "Wrong bfuAmountIdx (" << bfuAmountIdx << "), frame skiped" << endl;
        return;
    }
    bitStream.Write(0x2, 2);
    bitUsed+=2;

    bitStream.Write(0x2, 2);
    bitUsed+=2;

    bitStream.Write(0x3, 2);
    bitStream.Write(0, 2);
    bitUsed+=4;

    bitStream.Write(bfuAmountIdx, BitsPerBfuAmountTabIdx);
    bitUsed += BitsPerBfuAmountTabIdx;

    bitStream.Write(0, 2);
    bitStream.Write(0, 3);
    bitUsed+= 5;

    for (const auto wordLength : bitsPerEachBlock) {
        const auto tmp = wordLength ? (wordLength - 1) : 0;
        bitStream.Write(tmp, 4);
        bitUsed+=4;
    }
    for (int i = 0; i < bitsPerEachBlock.size(); ++i) {
        bitStream.Write(scaledBlocks[i].ScaleFactorIndex, 6);
        bitUsed+=6;
    }
    for (int i = 0; i < bitsPerEachBlock.size(); ++i) {
        const auto wordLength = bitsPerEachBlock[i];
        if (wordLength == 0 || wordLength == 1)
            continue;

        const double multiple = ((1 << (wordLength - 1)) - 1);
        for (const double val : scaledBlocks[i].Values) {
            const int tmp = val * multiple;
            const int testwl = bitsPerEachBlock[i] ? (bitsPerEachBlock[i] - 1) : 0;
            const int a = !!testwl + testwl;
            if (a != wordLength) {
                cerr << "wordlen error " << a << " " << wordLength << endl;
                abort();
            }
            bitStream.Write(NBitStream::MakeSign(tmp, wordLength), wordLength);
            bitUsed+=wordLength;
        }
    }

    bitStream.Write(0x0, 8);
    bitStream.Write(0x0, 8);

    bitUsed+=16;
    bitStream.Write(0x0, 8);

    bitUsed+=8;
    if (bitUsed > SoundUnitSize * 8) {
        cerr << "ATRAC1 bitstream corrupted, used: " << bitUsed << " exp: " << SoundUnitSize * 8 << endl;
        abort();
    }
    Container->WriteFrame(bitStream.GetBytes());
}

}
