#include "atrac3_bitstream.h"
#include "../bitstream/bitstream.h"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdlib>

using NAtracDEnc::TScaledBlock;
using std::vector;

uint32_t TAtrac3BitStreamWriter::CLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream) {
    const uint32_t numBits = ClcLengthTab[selector];
    const uint32_t bitsUsed = (selector > 1) ? numBits * blockSize : numBits * blockSize / 2;
    if (!bitStream)
        return bitsUsed;
    if (selector > 1) {
        for (uint32_t i = 0; i < blockSize; ++i) {
            bitStream->Write(NBitStream::MakeSign(mantissas[i], numBits), numBits);
        }
    } else {
        for (uint32_t i = 0; i < blockSize / 2; ++i) {
            uint32_t code = MantissaToCLcIdx(mantissas[i * 2]) << 2;
            code |= MantissaToCLcIdx(mantissas[i * 2 + 1]);
            assert(numBits == 4);
            bitStream->Write(code, numBits);
        }
    }
    return bitsUsed;
}

uint32_t TAtrac3BitStreamWriter::VLCEnc(const uint32_t selector, const int mantissas[MAXSPECPERBLOCK], const uint32_t blockSize, NBitStream::TBitStream* bitStream) {
    const THuffEntry* huffTable = HuffTables[selector - 1].Table;
    const uint8_t tableSz = HuffTables[selector - 1].Sz;
    uint32_t bitsUsed = 0;
    if (selector > 1) {
        for (uint32_t i = 0; i < blockSize; ++i) {
            int m = mantissas[i];
            uint32_t huffS = (m < 0) ? (((uint32_t)(-m)) << 1) | 1 : ((uint32_t)m) << 1;
            if (huffS)
                huffS -= 1;
            assert(huffS < 256);
            assert(huffS < tableSz);
            bitsUsed += huffTable[huffS].Bits;
            if (bitStream)
                bitStream->Write(huffTable[huffS].Code, huffTable[huffS].Bits);
        }
    } else {
        assert(tableSz == 9); 
        for (uint32_t i = 0; i < blockSize / 2; ++i) {
            const int ma = mantissas[i * 2];
            const int mb = mantissas[i * 2 + 1];
            const uint32_t huffS = MantissasToVlcIndex(ma, mb);
            bitsUsed += huffTable[huffS].Bits;
            if (bitStream)
                bitStream->Write(huffTable[huffS].Code, huffTable[huffS].Bits);
        }
    }
    return bitsUsed;
}
std::pair<uint8_t, uint32_t> TAtrac3BitStreamWriter::CalcSpecsBitsConsumption(const vector<TScaledBlock>& scaledBlocks, const vector<uint32_t>& precisionPerEachBlocks, int* mantisas) {
    uint32_t bitsUsed = 5 + 1; //numBlocks + codingMode
    const uint32_t numBlocks = precisionPerEachBlocks.size();
    bitsUsed += numBlocks * 3; //used VLC or CLC table (precision)

    auto lambda = [=](bool clcMode, bool calcMant) {
        uint32_t bits = 0;
        for (uint32_t i = 0; i < numBlocks; ++i) {
            if (precisionPerEachBlocks[i] == 0)
                continue;
            bits += 6; //sfi
            const uint32_t first = BlockSizeTab[i];
            const uint32_t last = BlockSizeTab[i+1];
            const uint32_t blockSize = last - first;
            const double mul = MaxQuant[std::min(precisionPerEachBlocks[i], (uint32_t)7)];
            if (calcMant) {
                for (uint32_t j = 0, f = first; f < last; f++, j++) {
                    mantisas[f] = round(scaledBlocks[i].Values[j] * mul);
                }
            }
            bits += clcMode ? CLCEnc(precisionPerEachBlocks[i], mantisas + first, blockSize, nullptr) :
                VLCEnc(precisionPerEachBlocks[i], mantisas + first, blockSize, nullptr);
        }
        return bits;
    };
    const uint32_t clcBits = lambda(true, true);
    const uint32_t vlcBits = lambda(false, false);
    bool mode = clcBits <= vlcBits;
    //std::cout << "mode: " << mode << " " << clcBits << "  "  <<vlcBits << std::endl;
    return std::make_pair(mode, bitsUsed + (mode ? clcBits : vlcBits));
}
void TAtrac3BitStreamWriter::EncodeSpecs(const vector<TScaledBlock>& scaledBlocks, const vector<uint32_t>& precisionPerEachBlocks, NBitStream::TBitStream* bitStream) {
    const uint32_t numBlocks = precisionPerEachBlocks.size(); //number of blocks to save
    int mt[MAX_SPECS];
    const auto consumption = CalcSpecsBitsConsumption(scaledBlocks, precisionPerEachBlocks, mt);
    const uint32_t codingMode = consumption.first;//0 - VLC, 1 - CLC
    assert(numBlocks <= 32);
    bitStream->Write(numBlocks-1, 5);
    bitStream->Write(codingMode, 1);
    for (uint32_t i = 0; i < numBlocks; ++i) {
        uint32_t val = precisionPerEachBlocks[i]; //coding table used (VLC) or number of bits used (CLC)
        bitStream->Write(val, 3);
    }
    for (uint32_t i = 0; i < numBlocks; ++i) {
        if (precisionPerEachBlocks[i] == 0)
            continue;
        bitStream->Write(scaledBlocks[i].ScaleFactorIndex, 6);
    }
    for (uint32_t i = 0; i < numBlocks; ++i) {
        if (precisionPerEachBlocks[i] == 0)
            continue;

        const uint32_t first = BlockSizeTab[i];
        const uint32_t last = BlockSizeTab[i+1];
        const uint32_t blockSize = last - first;

        if (codingMode == 1) {
            CLCEnc(precisionPerEachBlocks[i], mt + first, blockSize, bitStream);
        } else {
            VLCEnc(precisionPerEachBlocks[i], mt + first, blockSize, bitStream);
        }
    }
}

void TAtrac3BitStreamWriter::WriteSoundUnit(const TAtrac3SubbandInfo& subbandInfo, const vector<TScaledBlock>& scaledBlocks) {
    NBitStream::TBitStream bitStream;
    if (Params.Js) {
        //TODO
    } else {
        bitStream.Write(0x28, 6); //0x28 - id
    }
    const uint32_t numQmfBand = subbandInfo.GetQmfNum();
    bitStream.Write(numQmfBand - 1, 2);

    //write gain info
    for (uint32_t band = 0; band < numQmfBand; ++band) {
        const vector<TAtrac3SubbandInfo::TGainPoint>& GainPoints = subbandInfo.GetGainPoints(band);
        bitStream.Write(GainPoints.size(), 3);
        assert(GainPoints.size() == 0);
        for (const TAtrac3SubbandInfo::TGainPoint& point : GainPoints) {
            abort();
            bitStream.Write(point.Level, 4);
            bitStream.Write(point.Location, 5);
        }
    }

    //tonal components
    bitStream.Write(0, 5); //disabled

    vector<uint32_t> precisionPerEachBlocks(27,1);
    for (int i = 0; i < 2; i++) {
        //precisionPerEachBlocks[i] = 4;
    }

    //spec
    EncodeSpecs(scaledBlocks, precisionPerEachBlocks, &bitStream);

    if (!Container)
        abort();
    if (OutBuffer.empty()) {
        std::vector<char> channel = bitStream.GetBytes();
        //std::cout << channel.size() << std::endl;
        assert(channel.size() <= Params.FrameSz/2);
        channel.resize(Params.FrameSz/2);
        OutBuffer.insert(OutBuffer.end(), channel.begin(), channel.end());
    } else {
        std::vector<char> channel = bitStream.GetBytes();
        channel.resize(Params.FrameSz/2);
        //std::cout << channel.size() << std::endl;
        assert(channel.size() <= Params.FrameSz/2);
        OutBuffer.insert(OutBuffer.end(), channel.begin(), channel.end());
        Container->WriteFrame(OutBuffer);
        OutBuffer.clear();
    }

}
