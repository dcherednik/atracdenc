#include "atrac3_bitstream.h"
#include "atrac_psy_common.h"
#include "../bitstream/bitstream.h"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdlib>

#include <cstring>

namespace NAtracDEnc {
namespace NAtrac3 {

using std::vector;
using std::memset;

static const uint32_t FixedBitAllocTable[TAtrac3Data::MaxBfus] = {
  6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
  4, 4, 4, 3, 3, 3, 3, 3,
  3, 2, 2, 1,
  1, 0
};

uint32_t TAtrac3BitStreamWriter::CLCEnc(const uint32_t selector, const int mantissas[MaxSpecsPerBlock],
                                        const uint32_t blockSize, NBitStream::TBitStream* bitStream)
{
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

uint32_t TAtrac3BitStreamWriter::VLCEnc(const uint32_t selector, const int mantissas[MaxSpecsPerBlock],
                                        const uint32_t blockSize, NBitStream::TBitStream* bitStream)
{
    assert(selector > 0);
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

std::pair<uint8_t, uint32_t> TAtrac3BitStreamWriter::CalcSpecsBitsConsumption(const vector<TScaledBlock>& scaledBlocks,
                                                        const vector<uint32_t>& precisionPerEachBlocks, int* mantisas)
{
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
            const TFloat mul = MaxQuant[std::min(precisionPerEachBlocks[i], (uint32_t)7)];
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
    return std::make_pair(mode, bitsUsed + (mode ? clcBits : vlcBits));
}


std::pair<uint8_t, vector<uint32_t>> TAtrac3BitStreamWriter::CreateAllocation(const vector<TScaledBlock>& scaledBlocks,
                                                                              uint16_t bitsUsed, int mt[MaxSpecs])
{
    TFloat spread = AnalizeScaleFactorSpread(scaledBlocks);

    uint8_t numBfu = 32;
    vector<uint32_t> precisionPerEachBlocks(numBfu);
    uint8_t mode;
    for (;;) {
        precisionPerEachBlocks.resize(numBfu);
        uint32_t usedBfus = 0;
        for (auto v : precisionPerEachBlocks) {
            if (v)
                usedBfus++;
        }
        const uint32_t bitsAvaliablePerBfus = 8 * Params.FrameSz/2 - bitsUsed - 
            5 - 1 - (numBfu * 3) - (usedBfus * 6);
        TFloat maxShift = 15;
        TFloat minShift = -3;
        TFloat shift = 3.0;
        const uint32_t maxBits = bitsAvaliablePerBfus;
        const uint32_t minBits = bitsAvaliablePerBfus - 90;
        for (;;) {
            const vector<uint32_t>& tmpAlloc = CalcBitsAllocation(scaledBlocks, numBfu, spread, shift);
            const auto consumption = CalcSpecsBitsConsumption(scaledBlocks, tmpAlloc, mt);

            if (consumption.second < minBits) {
                if (maxShift - minShift < 0.1) {
                    precisionPerEachBlocks = tmpAlloc;
                    mode = consumption.first;
                    break;
                }
                maxShift = shift;
                shift -= (shift - minShift) / 2;
            } else if (consumption.second > maxBits) {
                minShift = shift;
                shift += (maxShift - shift) / 2;
            } else {
                precisionPerEachBlocks = tmpAlloc;
                mode = consumption.first;
                break;
            }
        }
        break;

    }
    return { mode, precisionPerEachBlocks };
}

void TAtrac3BitStreamWriter::EncodeSpecs(const vector<TScaledBlock>& scaledBlocks, NBitStream::TBitStream* bitStream,
                                         const uint16_t bitsUsed)
{
    int mt[MaxSpecs];
    auto allocation = CreateAllocation(scaledBlocks, bitsUsed, mt);
    const vector<uint32_t>& precisionPerEachBlocks = allocation.second;
    const uint32_t numBlocks = precisionPerEachBlocks.size(); //number of blocks to save
    const uint32_t codingMode = allocation.first;//0 - VLC, 1 - CLC

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

uint8_t TAtrac3BitStreamWriter::GroupTonalComponents(const std::vector<TTonalComponent>& tonalComponents,
                                                     TTonalComponentsSubGroup groups[64])
{
    for (const TTonalComponent& tc : tonalComponents) {
        assert(tc.ScaledBlock.Values.size() < 8);
        assert(tc.ScaledBlock.Values.size() > 0);
        assert(tc.QuantIdx >1);
        assert(tc.QuantIdx <8);
        groups[tc.QuantIdx * 8 + tc.ScaledBlock.Values.size()].SubGroupPtr.push_back(&tc);
    }
    uint8_t tcsgn = 0;
    //for each group
    for (uint8_t i = 0; i < 64; ++i) {
        uint8_t start_pos;
        uint8_t cur_pos = 0;
        //scan tonal components
        while (cur_pos < groups[i].SubGroupPtr.size()) {
            start_pos = cur_pos;
            ++tcsgn;
            groups[i].SubGroupMap.push_back(cur_pos);
            uint8_t groupLimiter = 0;
            //allow not grather than 8 components in one subgroup limited by 64 specs
            do {
                ++cur_pos;
                if (cur_pos == groups[i].SubGroupPtr.size())
                    break;
                if (groups[i].SubGroupPtr[cur_pos]->ValPtr->Pos - (groups[i].SubGroupPtr[start_pos]->ValPtr->Pos & ~63) < 64) {
                    ++groupLimiter;
                } else {
                    groupLimiter = 0;
                    start_pos = cur_pos;
                }
            } while (groupLimiter < 7);
        }
    }
    return tcsgn;
}

uint16_t TAtrac3BitStreamWriter::EncodeTonalComponents(const std::vector<TTonalComponent>& tonalComponents,
                                                       NBitStream::TBitStream* bitStream, uint8_t numQmfBand)
{
    const uint16_t bitsUsed = bitStream->GetSizeInBits();
    //group tonal components with same quantizer and len
    TTonalComponentsSubGroup groups[64];
    const uint8_t tcsgn = GroupTonalComponents(tonalComponents, groups);

    assert(tcsgn < 32);
    bitStream->Write(tcsgn, 5);
    if (tcsgn == 0) {
        for (int i = 0; i < 64; ++i)
            assert(groups[i].SubGroupPtr.size() == 0);
        return 5; //wrote 0 but 5 bits for tcsgn
    }
    //Coding mode:
    // 0 - All are VLC
    // 1 - All are CLC
    // 2 - Error
    // 3 - Own mode for each component

    //TODO: implement switch for best coding mode. Now VLC for all
    bitStream->Write(0, 2);

    uint8_t tcgnCheck = 0;
    //for each group of equal quantiser and len 
    for (uint8_t i = 0; i < 64; ++i) {
        const TTonalComponentsSubGroup& curGroup = groups[i];
        if (curGroup.SubGroupPtr.size() == 0) {
            assert(curGroup.SubGroupMap.size() == 0);
            continue;
        }
        assert(curGroup.SubGroupMap.size());
        for (uint8_t subgroup = 0; subgroup < curGroup.SubGroupMap.size(); ++subgroup) {
            const uint8_t subGroupStartPos = curGroup.SubGroupMap[subgroup];
            const uint8_t subGroupEndPos = (subgroup < curGroup.SubGroupMap.size() - 1) ?
                curGroup.SubGroupMap[subgroup+1] : curGroup.SubGroupPtr.size();
            assert(subGroupEndPos > subGroupStartPos);
            //number of coded values are same in group
            const uint8_t codedValues = curGroup.SubGroupPtr[0]->ScaledBlock.Values.size();

            //Number of tonal component for each 64spec block. Used to set qmf band flags and simplify band encoding loop
            uint8_t bandFlags[16];
            memset(bandFlags, 0, 16 * sizeof(uint8_t));
            assert(numQmfBand <= 4);
            for (uint8_t j = subGroupStartPos; j < subGroupEndPos; ++j) {
                //assert num of coded values are same in group
                assert(codedValues == curGroup.SubGroupPtr[j]->ScaledBlock.Values.size());
                uint8_t specBlock = (curGroup.SubGroupPtr[j]->ValPtr->Pos) >> 6;
                assert((specBlock >> 2) < numQmfBand);
                bandFlags[specBlock]++;
            }

            assert(numQmfBand == 4);

            tcgnCheck++;
            
            for (uint8_t j = 0; j < numQmfBand; ++j) {
                bitStream->Write((bool)(*(uint32_t*)&bandFlags[j<<2]), 1);
            }
            //write number of coded values for components in current group
            assert(codedValues > 0);
            bitStream->Write(codedValues - 1, 3);
            //write quant index
            assert((i >> 3) > 1);
            assert((i >> 3) < 8);
            assert(i);
            bitStream->Write(i >> 3, 3);
            uint8_t lastPos = subGroupStartPos;
            uint8_t checkPos = 0;
            for (uint16_t j = 0; j < 16; ++j) {
                if (!(*(uint32_t*)&bandFlags[j & 0xC])) { //discard two bits
                    continue;
                }

                const uint8_t codedComponents = bandFlags[j];
                assert(codedComponents < 8);
                bitStream->Write(codedComponents, 3);
                uint8_t k = lastPos;
                for (; k < lastPos + codedComponents; ++k) {
                    assert(curGroup.SubGroupPtr[k]->ValPtr->Pos >= j * 64);
                    uint16_t relPos = curGroup.SubGroupPtr[k]->ValPtr->Pos - j * 64;
                    assert(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex < 64);
                    bitStream->Write(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex, 6);

                    assert(relPos < 64);
                    
                    bitStream->Write(relPos, 6);

                    assert(curGroup.SubGroupPtr[k]->ScaledBlock.Values.size() < 8);
                    int mantisas[256];
                    const TFloat mul = MaxQuant[std::min((uint32_t)(i>>3), (uint32_t)7)];
                    assert(codedValues == curGroup.SubGroupPtr[k]->ScaledBlock.Values.size());
                    for (uint32_t z = 0; z < curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(); ++z) {
                        mantisas[z] = round(curGroup.SubGroupPtr[k]->ScaledBlock.Values[z] * mul);
                    }
                    //VLCEnc

                    assert(i);
                    VLCEnc(i>>3, mantisas, curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(), bitStream);


                }
                lastPos = k;
                checkPos = lastPos;
            }

            assert(subGroupEndPos == checkPos);
        }
    }
    assert(tcgnCheck == tcsgn);
    return bitStream->GetSizeInBits() - bitsUsed;
}

vector<uint32_t> TAtrac3BitStreamWriter::CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                                            const uint32_t bfuNum,
                                                            const TFloat spread,
                                                            const TFloat shift)
{
    vector<uint32_t> bitsPerEachBlock(bfuNum);
    for (size_t i = 0; i < bitsPerEachBlock.size(); ++i) {
        const uint32_t fix = FixedBitAllocTable[i];
        int tmp = spread * ( (TFloat)scaledBlocks[i].ScaleFactorIndex/3.2) + (1.0 - spread) * fix - shift; 
        if (tmp > 7) {
            bitsPerEachBlock[i] = 7;
        } else if (tmp < 0) {
            bitsPerEachBlock[i] = 0;
        } else {
            bitsPerEachBlock[i] = tmp;
        }
    }
    return bitsPerEachBlock;
}


void TAtrac3BitStreamWriter::WriteSoundUnit(const TAtrac3Data::SubbandInfo& subbandInfo,
                                            const std::vector<TTonalComponent>& tonalComponents,
                                            const vector<TScaledBlock>& scaledBlocks)
{
    NBitStream::TBitStream bitStream;
    if (Params.Js) {
        //TODO
    } else {
        bitStream.Write(0x28, 6); //0x28 - id
    }
    const uint8_t numQmfBand = subbandInfo.GetQmfNum();
    bitStream.Write(numQmfBand - 1, 2);

    //write gain info
    for (uint32_t band = 0; band < numQmfBand; ++band) {
        const vector<TAtrac3Data::SubbandInfo::TGainPoint>& GainPoints = subbandInfo.GetGainPoints(band);
        assert(GainPoints.size() < TAtrac3Data::SubbandInfo::MaxGainPointsNum);
        bitStream.Write(GainPoints.size(), 3);
        int s = 0;
        for (const TAtrac3Data::SubbandInfo::TGainPoint& point : GainPoints) {
            bitStream.Write(point.Level, 4);
            bitStream.Write(point.Location, 5);
            s++;
            assert(s < 8);
        }
    }
    const uint16_t bitsUsedByGainInfo = bitStream.GetSizeInBits() - 8;
    const uint16_t bitsUsedByTonal = EncodeTonalComponents(tonalComponents, &bitStream, numQmfBand);
    //spec
    EncodeSpecs(scaledBlocks, &bitStream, bitsUsedByTonal + bitsUsedByGainInfo);

    if (!Container)
        abort();
    if (OutBuffer.empty()) {
        std::vector<char> channel = bitStream.GetBytes();
        assert(channel.size() <= Params.FrameSz/2);
        channel.resize(Params.FrameSz/2);
        OutBuffer.insert(OutBuffer.end(), channel.begin(), channel.end());
    } else {
        std::vector<char> channel = bitStream.GetBytes();

        assert(channel.size() <= Params.FrameSz/2);
        channel.resize(Params.FrameSz/2);
        OutBuffer.insert(OutBuffer.end(), channel.begin(), channel.end());
        Container->WriteFrame(OutBuffer);
        OutBuffer.clear();
    }

}

} // namespace NAtrac3
} // namespace NAtracDEnc
