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

#include "atrac3_bitstream.h"
#include "atrac_psy_common.h"
#include "../bitstream/bitstream.h"
#include "../util.h"
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
  5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
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

static inline int ToInt(double x) {
#if defined(_MSC_VER) && !defined(_WIN64)
    int n;
    __asm {
        fld x
        fistp n
    }
    return n;
#else
    return lrint(x);
#endif
}

static inline void CalcMantisas(const TFloat* values, const uint32_t first, const uint32_t last, const TFloat mul, int* mantisas) {
    for (uint32_t j = 0, f = first; f < last; f++, j++) {
        mantisas[f] = ToInt(values[j] * mul);
    }
}

std::pair<uint8_t, uint32_t> TAtrac3BitStreamWriter::CalcSpecsBitsConsumption(const TSingleChannelElement& sce,
                                                        const vector<uint32_t>& precisionPerEachBlocks, int* mantisas)
{

    const vector<TScaledBlock>& scaledBlocks = sce.ScaledBlocks;
    const uint32_t numBlocks = precisionPerEachBlocks.size();
    uint32_t bitsUsed = numBlocks * 3;

    auto lambda = [this, numBlocks, mantisas, &precisionPerEachBlocks, &scaledBlocks](bool clcMode, bool calcMant) {
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
                const TFloat* values = scaledBlocks[i].Values.data();
                CalcMantisas(values, first, last, mul, mantisas);
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

//true - should reencode
//false - not need to
static inline bool CheckBfus(uint16_t* numBfu, const vector<uint32_t>& precisionPerEachBlocks)
{
    assert(*numBfu);
    uint16_t curLastBfu = *numBfu - 1;
    //assert(curLastBfu < precisionPerEachBlocks.size());
    assert(*numBfu == precisionPerEachBlocks.size());
    if (precisionPerEachBlocks[curLastBfu] == 0) {
        *numBfu = curLastBfu;
        return true;
    }
    return false;
}

static const std::pair<uint8_t, vector<uint32_t>> DUMMY_ALLOC{1, vector<uint32_t>{0}};

std::pair<uint8_t, vector<uint32_t>> TAtrac3BitStreamWriter::CreateAllocation(const TSingleChannelElement& sce,
                                                                              const uint16_t targetBits, int mt[MaxSpecs])
{
    const vector<TScaledBlock>& scaledBlocks = sce.ScaledBlocks;
    if (scaledBlocks.empty()) {
        return DUMMY_ALLOC;
    }

    TFloat spread = AnalizeScaleFactorSpread(scaledBlocks);

    uint16_t numBfu = BfuIdxConst ? BfuIdxConst : 32;

    // Limit number of BFU if target bitrate is not enough
    // 3 bits to write each bfu without data
    // 5 bits we need for tonal header
    // 32 * 3 + 5 = 101
    if (targetBits < 101) {
        uint16_t lim = (targetBits - 5) / 3;
        numBfu = std::min(numBfu, lim);
    }

    vector<uint32_t> precisionPerEachBlocks(numBfu);
    uint8_t mode;
    bool cont = true;
    while (cont) {
        precisionPerEachBlocks.resize(numBfu);
        TFloat maxShift = 20;
        TFloat minShift = -8;
        for (;;) {
            TFloat shift = (maxShift + minShift) / 2;
            const vector<uint32_t>& tmpAlloc = CalcBitsAllocation(scaledBlocks, numBfu, spread, shift);
            auto consumption = CalcSpecsBitsConsumption(sce, tmpAlloc, mt);

            auto bitsUsedByTonal = EncodeTonalComponents(sce, tmpAlloc, nullptr);
//            std::cerr << consumption.second << " |tonal: " << bitsUsedByTonal << " target: " << targetBits << " shift " << shift << " max | min " << maxShift << " " << minShift << " numBfu: " << numBfu << std::endl;
            consumption.second += bitsUsedByTonal;

            if (consumption.second < targetBits) {
                if (maxShift - minShift < 0.1) {
                    precisionPerEachBlocks = tmpAlloc;
                    mode = consumption.first;
                    if (numBfu > 1) {
                        cont = !BfuIdxConst && CheckBfus(&numBfu, precisionPerEachBlocks);
                    } else {
                        cont = false;
                    }
                    break;
                }
                maxShift = shift - 0.01;
            } else if (consumption.second > targetBits) {
                minShift = shift + 0.01;
            } else {
                precisionPerEachBlocks = tmpAlloc;
                mode = consumption.first;
                cont = !BfuIdxConst && CheckBfus(&numBfu, precisionPerEachBlocks);;
                break;
            }
        }
    }
    //std::cerr << "==" << std::endl;
    return { mode, precisionPerEachBlocks };
}

void TAtrac3BitStreamWriter::EncodeSpecs(const TSingleChannelElement& sce, NBitStream::TBitStream* bitStream,
                                         const std::pair<uint8_t, vector<uint32_t>>& allocation, const int mt[MaxSpecs])
{

    const vector<TScaledBlock>& scaledBlocks = sce.ScaledBlocks;
    const vector<uint32_t>& precisionPerEachBlocks = allocation.second;
    EncodeTonalComponents(sce, precisionPerEachBlocks, bitStream);
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

uint8_t TAtrac3BitStreamWriter::GroupTonalComponents(const std::vector<TTonalBlock>& tonalComponents,
                                                     const vector<uint32_t>& allocTable,
                                                     TTonalComponentsSubGroup groups[64])
{
    for (const TTonalBlock& tc : tonalComponents) {
        assert(tc.ScaledBlock.Values.size() < 8);
        assert(tc.ScaledBlock.Values.size() > 0);
        assert(tc.ValPtr->Bfu < allocTable.size());
        auto quant = std::max((uint32_t)2, std::min(allocTable[tc.ValPtr->Bfu] + 1, (uint32_t)7));
        //std::cerr << " | " << tc.ValPtr->Pos << " | " << (int)tc.ValPtr->Bfu << " | " << quant << std::endl;
        groups[quant * 8 + tc.ScaledBlock.Values.size()].SubGroupPtr.push_back(&tc);
    }

    //std::cerr << "=====" << std::endl;
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

uint16_t TAtrac3BitStreamWriter::EncodeTonalComponents(const TSingleChannelElement& sce,
                                                       const vector<uint32_t>& allocTable,
                                                       NBitStream::TBitStream* bitStream)
{
    const uint16_t bitsUsedOld = bitStream ? (uint16_t)bitStream->GetSizeInBits() : 0;
    const std::vector<TTonalBlock>& tonalComponents = sce.TonalBlocks;
    const TAtrac3Data::SubbandInfo& subbandInfo = sce.SubbandInfo;
    const uint8_t numQmfBand = subbandInfo.GetQmfNum();
    uint16_t bitsUsed = 0;

    //group tonal components with same quantizer and len
    TTonalComponentsSubGroup groups[64];
    const uint8_t tcsgn = GroupTonalComponents(tonalComponents, allocTable, groups);

    assert(tcsgn < 32);

    bitsUsed += 5;
    if (bitStream)
        bitStream->Write(tcsgn, 5);

    if (tcsgn == 0) {
        for (int i = 0; i < 64; ++i)
            assert(groups[i].SubGroupPtr.size() == 0);
        return bitsUsed;
    }
    //Coding mode:
    // 0 - All are VLC
    // 1 - All are CLC
    // 2 - Error
    // 3 - Own mode for each component

    //TODO: implement switch for best coding mode. Now VLC for all
    bitsUsed += 2;
    if (bitStream)
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
        assert(curGroup.SubGroupMap.size() < UINT8_MAX);
        for (uint8_t subgroup = 0; subgroup < curGroup.SubGroupMap.size(); ++subgroup) {
            const uint8_t subGroupStartPos = curGroup.SubGroupMap[subgroup];
            const uint8_t subGroupEndPos = (subgroup < curGroup.SubGroupMap.size() - 1) ?
                curGroup.SubGroupMap[subgroup+1] : (uint8_t)curGroup.SubGroupPtr.size();
            assert(subGroupEndPos > subGroupStartPos);
            //number of coded values are same in group
            const uint8_t codedValues = (uint8_t)curGroup.SubGroupPtr[0]->ScaledBlock.Values.size();

            //Number of tonal component for each 64spec block. Used to set qmf band flags and simplify band encoding loop
            union {
                uint8_t c[16];
                uint32_t i[4] = {0};
            } bandFlags;
            assert(numQmfBand <= 4);
            for (uint8_t j = subGroupStartPos; j < subGroupEndPos; ++j) {
                //assert num of coded values are same in group
                assert(codedValues == curGroup.SubGroupPtr[j]->ScaledBlock.Values.size());
                uint8_t specBlock = (curGroup.SubGroupPtr[j]->ValPtr->Pos) >> 6;
                assert((specBlock >> 2) < numQmfBand);
                bandFlags.c[specBlock]++;
            }

            assert(numQmfBand == 4);

            tcgnCheck++;
            
            bitsUsed += numQmfBand;
            if (bitStream) {
                for (uint8_t j = 0; j < numQmfBand; ++j) {
                    bitStream->Write((bool)bandFlags.i[j], 1);
                }
            }
            //write number of coded values for components in current group
            assert(codedValues > 0);
            bitsUsed += 3;
            if (bitStream)
                bitStream->Write(codedValues - 1, 3);
            //write quant index
            assert((i >> 3) > 1);
            assert((i >> 3) < 8);
            assert(i);
            bitsUsed += 3;
            if (bitStream)
                bitStream->Write(i >> 3, 3);
            uint8_t lastPos = subGroupStartPos;
            uint8_t checkPos = 0;
            for (uint16_t j = 0; j < 16; ++j) {
                if (!(bandFlags.i[j >> 2])) {
                    continue;
                }

                const uint8_t codedComponents = bandFlags.c[j];
                assert(codedComponents < 8);
                bitsUsed += 3;
                if (bitStream)
                    bitStream->Write(codedComponents, 3);
                uint8_t k = lastPos;
                for (; k < lastPos + codedComponents; ++k) {
                    assert(curGroup.SubGroupPtr[k]->ValPtr->Pos >= j * 64);
                    uint16_t relPos = curGroup.SubGroupPtr[k]->ValPtr->Pos - j * 64;
                    assert(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex < 64);

                    bitsUsed += 6;
                    if (bitStream)
                        bitStream->Write(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex, 6);

                    assert(relPos < 64);
                    
                    bitsUsed += 6;
                    if (bitStream)
                        bitStream->Write(relPos, 6);

                    assert(curGroup.SubGroupPtr[k]->ScaledBlock.Values.size() < 8);
                    int mantisas[256];
                    const TFloat mul = MaxQuant[std::min((uint32_t)(i>>3), (uint32_t)7)];
                    assert(codedValues == curGroup.SubGroupPtr[k]->ScaledBlock.Values.size());
                    for (uint32_t z = 0; z < curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(); ++z) {
                        mantisas[z] = lrint(curGroup.SubGroupPtr[k]->ScaledBlock.Values[z] * mul);
                    }
                    //VLCEnc

                    assert(i);
                    bitsUsed += VLCEnc(i>>3, mantisas, curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(), bitStream);


                }
                lastPos = k;
                checkPos = lastPos;
            }

            assert(subGroupEndPos == checkPos);
        }
    }
    assert(tcgnCheck == tcsgn);
    if (bitStream)
        assert(bitStream->GetSizeInBits() - bitsUsedOld == bitsUsed);
    return bitsUsed;
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

void WriteJsParams(NBitStream::TBitStream* bs)
{
    bs->Write(0, 1);
    bs->Write(7, 3);
    for (int i = 0; i < 4; i++) {
        bs->Write(3, 2);
    }
}

//  0.5 - M only (mono)
//  0.0 - Uncorrelated
// -0.5 - S only
static TFloat CalcMSRatio(TFloat mEnergy, TFloat sEnergy) {
    TFloat total = sEnergy + mEnergy;
    if (total > 0)
        return mEnergy / total - 0.5;

    // No signal - nothing to shift
    return 0;
}

static int32_t CalcMSBytesShift(uint32_t frameSz,
                                const vector<TAtrac3BitStreamWriter::TSingleChannelElement>& elements,
                                const int32_t b[2])
{
    const int32_t totalUsedBits = 0 - b[0] - b[1];
    assert(totalUsedBits > 0);

    const int32_t maxAllowedShift = (frameSz / 2 - Div8Ceil(totalUsedBits));

    if (elements[1].ScaledBlocks.empty()) {
        return maxAllowedShift;
    } else {
        TFloat ratio = CalcMSRatio(elements[0].Energy, elements[1].Energy);
        //std::cerr << ratio << std::endl;
        return std::max(std::min(ToInt(frameSz * ratio), maxAllowedShift), -maxAllowedShift);
    }
}

void TAtrac3BitStreamWriter::WriteSoundUnit(const vector<TSingleChannelElement>& singleChannelElements)
{

    assert(singleChannelElements.size() == 1 || singleChannelElements.size() == 2);

    const int halfFrameSz = Params.FrameSz >> 1;

    NBitStream::TBitStream bitStreams[2];

    int32_t bitsToAlloc[2] = {-6, -6}; // 6 bits used always to write num blocks and coding mode
                                       // See EncodeSpecs

    for (uint32_t channel = 0; channel < singleChannelElements.size(); channel++) {
        const TSingleChannelElement& sce = singleChannelElements[channel];
        const TAtrac3Data::SubbandInfo& subbandInfo = sce.SubbandInfo;

        NBitStream::TBitStream* bitStream = &bitStreams[channel];

        if (Params.Js && channel == 1) {
            WriteJsParams(bitStream);
            bitStream->Write(3, 2);
        } else {
            bitStream->Write(0x28, 6); //0x28 - id
        }

        const uint8_t numQmfBand = subbandInfo.GetQmfNum();
        assert(numQmfBand > 0);
        bitStream->Write(numQmfBand - 1, 2);

        //write gain info
        for (uint32_t band = 0; band < numQmfBand; ++band) {
            const vector<TAtrac3Data::SubbandInfo::TGainPoint>& GainPoints = subbandInfo.GetGainPoints(band);
            assert(GainPoints.size() < TAtrac3Data::SubbandInfo::MaxGainPointsNum);
            bitStream->Write(GainPoints.size(), 3);
            int s = 0;
            for (const TAtrac3Data::SubbandInfo::TGainPoint& point : GainPoints) {
                bitStream->Write(point.Level, 4);
                bitStream->Write(point.Location, 5);
                s++;
                assert(s < 8);
            }
        }

        const int16_t bitsUsedByGainInfoAndHeader = (int16_t)bitStream->GetSizeInBits();
        bitsToAlloc[channel] -= bitsUsedByGainInfoAndHeader;
    }

    int mt[2][MaxSpecs];
    std::pair<uint8_t, vector<uint32_t>> allocations[2];

    const int32_t msBytesShift = Params.Js ? CalcMSBytesShift(Params.FrameSz, singleChannelElements, bitsToAlloc) : 0; // positive - gain to m, negative to s. Must be zero if no joint stereo mode

    bitsToAlloc[0] += 8 * (halfFrameSz + msBytesShift);
    bitsToAlloc[1] += 8 * (halfFrameSz - msBytesShift);

    for (uint32_t channel = 0; channel < singleChannelElements.size(); channel++) {
        const TSingleChannelElement& sce = singleChannelElements[channel];
        allocations[channel] = CreateAllocation(sce, bitsToAlloc[channel], mt[channel]);
    }

    for (uint32_t channel = 0; channel < singleChannelElements.size(); channel++) {
        const TSingleChannelElement& sce = singleChannelElements[channel];
        NBitStream::TBitStream* bitStream = &bitStreams[channel];

        EncodeSpecs(sce, bitStream, allocations[channel], mt[channel]);

        if (!Container)
            abort();

        std::vector<char> channelData = bitStream->GetBytes();

        if (Params.Js && channel == 1) {
            channelData.resize(halfFrameSz - msBytesShift);
            OutBuffer.insert(OutBuffer.end(), channelData.rbegin(), channelData.rend());
        } else {
            channelData.resize(halfFrameSz + msBytesShift);
            OutBuffer.insert(OutBuffer.end(), channelData.begin(), channelData.end());
        }
    }

    //No mone mode for atrac3, just make duplicate of first channel
    if (singleChannelElements.size() == 1 && !Params.Js) {
        int sz = OutBuffer.size();
        assert(sz == halfFrameSz);
        OutBuffer.resize(sz << 1);
        std::copy_n(OutBuffer.begin(), sz, OutBuffer.begin() + sz);
    }

    Container->WriteFrame(OutBuffer);
    OutBuffer.clear();
}

} // namespace NAtrac3
} // namespace NAtracDEnc
