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
#include "qmf/qmf.h"
#include <atrac/atrac_psy_common.h>
#include <bitstream/bitstream.h>
#include <util.h>
#include <env.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace NAtracDEnc {
namespace NAtrac3 {

using std::vector;

static const uint32_t FixedBitAllocTable[TAtrac3Data::MaxBfus] = {
  4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 1, 1, 1,
  1, 1, 1, 1,
  0, 0
};

#define EAQ 1

#ifdef EAQ

static constexpr size_t LOSY_NAQ_START = 18;
static constexpr size_t BOOST_NAQ_END = 10;

#else

static constexpr size_t LOSY_NAQ_START = 31;
static constexpr size_t BOOST_NAQ_END = 0;

#endif

namespace {

std::vector<float> ATH;

struct TTonalComponentsSubGroup {
    std::vector<uint8_t> SubGroupMap;
    std::vector<const TTonalBlock*> SubGroupPtr;
};

struct TEncodeCtx {
    const TAtrac3BitStreamWriter::TSingleChannelElement* Sce = nullptr;
    uint16_t TargetBits = 0;
    uint32_t BfuIdxConst = 0;
    float Loudness = 0.0f;
    bool AllocInitDone = false;
    float Spread = 0.0f;
    uint16_t NumBfu = 1;
    uint8_t CodingMode = 1; // 0 - VLC, 1 - CLC
    std::vector<uint32_t> PrecisionPerBlock = {0};
    std::vector<float> EnergyErr = {0.0f};
    std::array<int, TAtrac3Data::MaxSpecs> Mantissas{};

    static TEncodeCtx* Cast(void* p) {
        return reinterpret_cast<TEncodeCtx*>(p);
    }
};

uint32_t CLCEnc(const uint32_t selector, const int mantissas[TAtrac3Data::MaxSpecsPerBlock],
                const uint32_t blockSize, NBitStream::TBitStream* bitStream)
{
    const uint32_t numBits = TAtrac3Data::ClcLengthTab[selector];
    const uint32_t bitsUsed = (selector > 1) ? numBits * blockSize : numBits * blockSize / 2;
    if (!bitStream) {
        return bitsUsed;
    }
    if (selector > 1) {
        for (uint32_t i = 0; i < blockSize; ++i) {
            bitStream->Write(NBitStream::MakeSign(mantissas[i], numBits), numBits);
        }
    } else {
        for (uint32_t i = 0; i < blockSize / 2; ++i) {
            uint32_t code = TAtrac3Data::MantissaToCLcIdx(mantissas[i * 2]) << 2;
            code |= TAtrac3Data::MantissaToCLcIdx(mantissas[i * 2 + 1]);
            ASSERT(numBits == 4);
            bitStream->Write(code, numBits);
        }
    }
    return bitsUsed;
}

uint32_t VLCEnc(const uint32_t selector, const int mantissas[TAtrac3Data::MaxSpecsPerBlock],
                const uint32_t blockSize, NBitStream::TBitStream* bitStream)
{
    ASSERT(selector > 0);
    const TAtrac3Data::THuffEntry* huffTable = TAtrac3Data::HuffTables[selector - 1].Table;
    const uint8_t tableSz = TAtrac3Data::HuffTables[selector - 1].Sz;
    uint32_t bitsUsed = 0;
    if (selector > 1) {
        for (uint32_t i = 0; i < blockSize; ++i) {
            int m = mantissas[i];
            uint32_t huffS = (m < 0) ? (((uint32_t)(-m)) << 1) | 1 : ((uint32_t)m) << 1;
            if (huffS) {
                huffS -= 1;
            }
            ASSERT(huffS < 256);
            ASSERT(huffS < tableSz);
            bitsUsed += huffTable[huffS].Bits;
            if (bitStream) {
                bitStream->Write(huffTable[huffS].Code, huffTable[huffS].Bits);
            }
        }
    } else {
        ASSERT(tableSz == 9);
        for (uint32_t i = 0; i < blockSize / 2; ++i) {
            const int ma = mantissas[i * 2];
            const int mb = mantissas[i * 2 + 1];
            const uint32_t huffS = TAtrac3Data::MantissasToVlcIndex(ma, mb);
            bitsUsed += huffTable[huffS].Bits;
            if (bitStream) {
                bitStream->Write(huffTable[huffS].Code, huffTable[huffS].Bits);
            }
        }
    }
    return bitsUsed;
}

std::pair<uint8_t, uint32_t> CalcSpecsBitsConsumption(const TAtrac3BitStreamWriter::TSingleChannelElement& sce,
                                                      const vector<uint32_t>& precisionPerEachBlocks,
                                                      int* mantisas,
                                                      vector<float>& energyErr)
{
    const vector<TScaledBlock>& scaledBlocks = sce.ScaledBlocks;
    const uint32_t numBlocks = precisionPerEachBlocks.size();
    uint32_t bitsUsed = numBlocks * 3;

    auto lambda = [numBlocks, mantisas, &precisionPerEachBlocks, &scaledBlocks, &energyErr](bool clcMode, bool calcMant) {
        uint32_t bits = 0;
        for (uint32_t i = 0; i < numBlocks; ++i) {
            if (precisionPerEachBlocks[i] == 0) {
                continue;
            }
            bits += 6; // sfi
            const uint32_t first = TAtrac3Data::BlockSizeTab[i];
            const uint32_t last = TAtrac3Data::BlockSizeTab[i + 1];
            const uint32_t blockSize = last - first;
            const float mul = TAtrac3Data::MaxQuant[std::min(precisionPerEachBlocks[i], (uint32_t)7)];
            if (calcMant) {
                const float* values = scaledBlocks[i].Values.data();
                energyErr[i] = QuantMantisas(values, first, last, mul, i > LOSY_NAQ_START, mantisas);
            }
            bits += clcMode ? CLCEnc(precisionPerEachBlocks[i], mantisas + first, blockSize, nullptr)
                            : VLCEnc(precisionPerEachBlocks[i], mantisas + first, blockSize, nullptr);
        }
        return bits;
    };

    const uint32_t clcBits = lambda(true, true);
    const uint32_t vlcBits = lambda(false, false);
    const bool mode = clcBits <= vlcBits;
    return std::make_pair(mode, bitsUsed + (mode ? clcBits : vlcBits));
}

static inline bool CheckBfus(uint16_t* numBfu, const vector<uint32_t>& precisionPerEachBlocks)
{
    ASSERT(*numBfu);
    const uint16_t curLastBfu = *numBfu - 1;
    ASSERT(*numBfu == precisionPerEachBlocks.size());
    if (precisionPerEachBlocks[curLastBfu] == 0) {
        *numBfu = curLastBfu;
        return true;
    }
    return false;
}

bool ConsiderEnergyErr(const vector<float>& err, vector<uint32_t>& bits)
{
    if (err.size() < bits.size()) {
        abort();
    }

    bool adjusted = false;
    const size_t lim = std::min((size_t)BOOST_NAQ_END, bits.size());
    for (size_t i = 0; i < lim; i++) {
        const float e = err[i];
        if (((e > 0 && e < 0.7f) || e > 1.2f) & (bits[i] < 7)) {
            bits[i]++;
            adjusted = true;
        }
    }
    return adjusted;
}

vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                    const uint32_t bfuNum,
                                    const float spread,
                                    const float shift,
                                    const float loudness,
                                    const int gainBoostPerBand[TAtrac3Data::NumQMF])
{
    vector<uint32_t> bitsPerEachBlock(bfuNum);
    for (size_t i = 0; i < bitsPerEachBlock.size(); ++i) {
        const float ath = ATH[i] * loudness;

        uint32_t bfuBand = 0;
        for (uint32_t b = 1; b < TAtrac3Data::NumQMF; ++b) {
            if (i >= TAtrac3Data::BlocksPerBand[b]) {
                bfuBand = b;
            }
        }

        if (scaledBlocks[i].Energy < ath) {
            bitsPerEachBlock[i] = 0;
        } else {
            const uint32_t fix = FixedBitAllocTable[i];
            float x = 6;
            if (i < 3) {
                x = 2.8;
            } else if (i < 10) {
                x = 2.6;
            } else if (i < 15) {
                x = 3.3;
            } else if (i <= 20) {
                x = 3.6;
            } else if (i <= 28) {
                x = 4.2;
            }

            const int tmp = spread * ((float)scaledBlocks[i].ScaleFactorIndex / x) + (1.0f - spread) * fix - shift
                            + gainBoostPerBand[bfuBand];
            if (tmp > 7) {
                bitsPerEachBlock[i] = 7;
            } else if (tmp < 0) {
                bitsPerEachBlock[i] = 0;
            } else if (tmp == 0) {
                bitsPerEachBlock[i] = 1;
            } else {
                bitsPerEachBlock[i] = tmp;
            }
        }
    }
    return bitsPerEachBlock;
}

uint8_t GroupTonalComponents(const std::vector<TTonalBlock>& tonalComponents,
                             const vector<uint32_t>& allocTable,
                             TTonalComponentsSubGroup groups[64])
{
    for (const TTonalBlock& tc : tonalComponents) {
        ASSERT(tc.ScaledBlock.Values.size() < 8);
        ASSERT(tc.ScaledBlock.Values.size() > 0);
        ASSERT(tc.ValPtr->Bfu < allocTable.size());
        const auto quant = std::max((uint32_t)2, std::min(allocTable[tc.ValPtr->Bfu] + 1, (uint32_t)7));
        groups[quant * 8 + tc.ScaledBlock.Values.size()].SubGroupPtr.push_back(&tc);
    }

    uint8_t tcsgn = 0;
    for (uint8_t i = 0; i < 64; ++i) {
        size_t startPos;
        size_t curPos = 0;
        while (curPos < groups[i].SubGroupPtr.size()) {
            startPos = curPos;
            ++tcsgn;
            groups[i].SubGroupMap.push_back(static_cast<uint8_t>(curPos));
            uint8_t groupLimiter = 0;
            do {
                ++curPos;
                if (curPos == groups[i].SubGroupPtr.size()) {
                    break;
                }
                if (groups[i].SubGroupPtr[curPos]->ValPtr->Pos - (groups[i].SubGroupPtr[startPos]->ValPtr->Pos & ~63) < 64) {
                    ++groupLimiter;
                } else {
                    groupLimiter = 0;
                    startPos = curPos;
                }
            } while (groupLimiter < 7);
        }
    }
    return tcsgn;
}

uint16_t EncodeTonalComponents(const TAtrac3BitStreamWriter::TSingleChannelElement& sce,
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

    ASSERT(tcsgn < 32);

    bitsUsed += 5;
    if (bitStream)
        bitStream->Write(tcsgn, 5);

    if (tcsgn == 0) {
        for (int i = 0; i < 64; ++i)
            ASSERT(groups[i].SubGroupPtr.size() == 0);
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
    for (size_t i = 0; i < 64; ++i) {
        const TTonalComponentsSubGroup& curGroup = groups[i];
        if (curGroup.SubGroupPtr.size() == 0) {
            ASSERT(curGroup.SubGroupMap.size() == 0);
            continue;
        }
        ASSERT(curGroup.SubGroupMap.size());
        ASSERT(curGroup.SubGroupMap.size() < UINT8_MAX);
        for (size_t subgroup = 0; subgroup < curGroup.SubGroupMap.size(); ++subgroup) {
            const uint8_t subGroupStartPos = curGroup.SubGroupMap[subgroup];
            const uint8_t subGroupEndPos = (subgroup < curGroup.SubGroupMap.size() - 1) ?
                curGroup.SubGroupMap[subgroup+1] : (uint8_t)curGroup.SubGroupPtr.size();
            ASSERT(subGroupEndPos > subGroupStartPos);
            //number of coded values are same in group
            const uint8_t codedValues = (uint8_t)curGroup.SubGroupPtr[0]->ScaledBlock.Values.size();

            //Number of tonal component for each 64spec block. Used to set qmf band flags and simplify band encoding loop
            union {
                uint8_t c[16];
                uint32_t i[4] = {0};
            } bandFlags;
            ASSERT(numQmfBand <= 4);
            for (uint8_t j = subGroupStartPos; j < subGroupEndPos; ++j) {
                //assert num of coded values are same in group
                ASSERT(codedValues == curGroup.SubGroupPtr[j]->ScaledBlock.Values.size());
                uint8_t specBlock = (curGroup.SubGroupPtr[j]->ValPtr->Pos) >> 6;
                ASSERT((specBlock >> 2) < numQmfBand);
                bandFlags.c[specBlock]++;
            }

            ASSERT(numQmfBand == 4);

            tcgnCheck++;

            bitsUsed += numQmfBand;
            if (bitStream) {
                for (uint8_t j = 0; j < numQmfBand; ++j) {
                    bitStream->Write((bool)bandFlags.i[j], 1);
                }
            }

            //write number of coded values for components in current group
            ASSERT(codedValues > 0);
            bitsUsed += 3;
            if (bitStream)
                bitStream->Write(codedValues - 1, 3);
            //write quant index
            ASSERT((i >> 3) > 1);
            ASSERT((i >> 3) < 8);
            bitsUsed += 3;
            if (bitStream)
                bitStream->Write(i >> 3, 3);

            uint8_t lastPos = subGroupStartPos;
            uint8_t checkPos = 0;
            for (size_t j = 0; j < 16; ++j) {
                if (!(bandFlags.i[j >> 2])) {
                    continue;
                }

                const uint8_t codedComponents = bandFlags.c[j];
                ASSERT(codedComponents < 8);
                bitsUsed += 3;
                if (bitStream)
                    bitStream->Write(codedComponents, 3);
                uint16_t k = lastPos;
                for (; k < lastPos + codedComponents; ++k) {
                    ASSERT(curGroup.SubGroupPtr[k]->ValPtr->Pos >= j * 64);
                    uint16_t relPos = curGroup.SubGroupPtr[k]->ValPtr->Pos - j * 64;
                    ASSERT(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex < 64);

                    bitsUsed += 6;
                    if (bitStream)
                        bitStream->Write(curGroup.SubGroupPtr[k]->ScaledBlock.ScaleFactorIndex, 6);

                    ASSERT(relPos < 64);
                    bitsUsed += 6;
                    if (bitStream)
                        bitStream->Write(relPos, 6);

                    ASSERT(curGroup.SubGroupPtr[k]->ScaledBlock.Values.size() < 8);
                    int mantisas[256];
                    const float mul = TAtrac3Data::MaxQuant[std::min((uint32_t)(i>>3), (uint32_t)7)];
                    ASSERT(codedValues == curGroup.SubGroupPtr[k]->ScaledBlock.Values.size());
                    for (uint32_t z = 0; z < curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(); ++z) {
                        mantisas[z] = lrint(curGroup.SubGroupPtr[k]->ScaledBlock.Values[z] * mul);
                    }
                    //VLCEnc

                    ASSERT(i);
                    bitsUsed += VLCEnc(i>>3, mantisas, curGroup.SubGroupPtr[k]->ScaledBlock.Values.size(), bitStream);
                }
                lastPos = k;
                checkPos = lastPos;
            }

            ASSERT(subGroupEndPos == checkPos);
        }
    }
    ASSERT(tcgnCheck == tcsgn);
    if (bitStream)
        ASSERT(bitStream->GetSizeInBits() - bitsUsedOld == bitsUsed);

    return bitsUsed;
}

void EncodeSpecs(const TAtrac3BitStreamWriter::TSingleChannelElement& sce,
                 NBitStream::TBitStream* bitStream,
                 const vector<uint32_t>& precisionPerEachBlocks,
                 uint8_t codingMode,
                 const int mt[TAtrac3Data::MaxSpecs])
{
    const vector<TScaledBlock>& scaledBlocks = sce.ScaledBlocks;

    EncodeTonalComponents(sce, precisionPerEachBlocks, bitStream);

    const uint32_t numBlocks = precisionPerEachBlocks.size();
    ASSERT(numBlocks <= 32);
    bitStream->Write(numBlocks - 1, 5);
    bitStream->Write(codingMode, 1);

    for (uint32_t i = 0; i < numBlocks; ++i) {
        bitStream->Write(precisionPerEachBlocks[i], 3);
    }
    for (uint32_t i = 0; i < numBlocks; ++i) {
        if (precisionPerEachBlocks[i] == 0) {
            continue;
        }
        bitStream->Write(scaledBlocks[i].ScaleFactorIndex, 6);
    }
    for (uint32_t i = 0; i < numBlocks; ++i) {
        if (precisionPerEachBlocks[i] == 0) {
            continue;
        }

        const uint32_t first = TAtrac3Data::BlockSizeTab[i];
        const uint32_t last = TAtrac3Data::BlockSizeTab[i + 1];
        const uint32_t blockSize = last - first;

        if (codingMode == 1) {
            CLCEnc(precisionPerEachBlocks[i], mt + first, blockSize, bitStream);
        } else {
            VLCEnc(precisionPerEachBlocks[i], mt + first, blockSize, bitStream);
        }
    }
}

uint16_t CalcInitialNumBfu(uint32_t bfuIdxConst, uint16_t targetBits)
{
    uint16_t numBfu = bfuIdxConst ? bfuIdxConst : 32;

    // Limit number of BFU if target bitrate is not enough.
    // 3 bits to write each BFU without data.
    // 5 bits we need for tonal header.
    // 32 * 3 + 5 = 101.
    if (targetBits < 101) {
        uint16_t lim = 1;
        if (targetBits > 5) {
            lim = (targetBits - 5) / 3;
        }
        lim = std::max<uint16_t>(1, lim);
        numBfu = std::min(numBfu, lim);
    }

    return std::max<uint16_t>(1, numBfu);
}

class TConfigure final : public IBitStreamPartEncoder {
public:
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        TEncodeCtx* ctx = TEncodeCtx::Cast(frameData);

        if (ctx->Sce->ScaledBlocks.empty()) {
            ctx->AllocInitDone = true;
            ctx->NumBfu = 1;
            ctx->Spread = 0.0f;
            ctx->CodingMode = 1;
            ctx->PrecisionPerBlock.assign(1, 0);
            return EStatus::Ok;
        }

        if (!ctx->AllocInitDone) {
            ctx->Spread = AnalizeScaleFactorSpread(ctx->Sce->ScaledBlocks);
            ctx->NumBfu = CalcInitialNumBfu(ctx->BfuIdxConst, ctx->TargetBits);
            // Cap NumBfu to the actual number of scaled blocks the
            // encoder side prepared.  When the top QMF band was dropped
            // upstream (silent band 3), ScaledBlocks only carries the
            // first 30 BFUs and the allocator must not over-run them.
            if (ctx->NumBfu > ctx->Sce->ScaledBlocks.size()) {
                ctx->NumBfu = static_cast<uint16_t>(ctx->Sce->ScaledBlocks.size());
            }
            ctx->AllocInitDone = true;
        }
        ctx->PrecisionPerBlock.assign(ctx->NumBfu, 0);
        ctx->EnergyErr.assign(ctx->NumBfu, 0.0f);
        ba.Start(ctx->TargetBits, -8.0f, 20.0f);
        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream&) override {}

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
};

class TAlloc final : public IBitStreamPartEncoder {
public:
    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        TEncodeCtx* ctx = TEncodeCtx::Cast(frameData);

        if (ctx->Sce->ScaledBlocks.empty()) {
            Ctx = ctx;
            return EStatus::Ok;
        }

        const float shift = ba.Continue();
        vector<uint32_t> tmpAlloc = CalcBitsAllocation(ctx->Sce->ScaledBlocks,
                                                       ctx->NumBfu,
                                                       ctx->Spread,
                                                       shift,
                                                       ctx->Loudness,
                                                       ctx->Sce->GainBoostPerBand);

        ctx->EnergyErr.assign(ctx->NumBfu, 0.0f);
        std::pair<uint8_t, uint32_t> consumption;
        do {
            consumption = CalcSpecsBitsConsumption(*ctx->Sce, tmpAlloc, ctx->Mantissas.data(), ctx->EnergyErr);
        } while (ConsiderEnergyErr(ctx->EnergyErr, tmpAlloc));

        uint32_t totalBits = consumption.second + EncodeTonalComponents(*ctx->Sce, tmpAlloc, nullptr);

        if (ba.Submit(totalBits)) {
            if (!ctx->BfuIdxConst && ctx->NumBfu > 1) {
                uint16_t numBfu = ctx->NumBfu;
                if (CheckBfus(&numBfu, tmpAlloc)) {
                    ctx->NumBfu = numBfu;
                    return EStatus::Repeat;
                }
            }
            ctx->PrecisionPerBlock = std::move(tmpAlloc);
            ctx->CodingMode = consumption.first;
            Ctx = ctx;
        }

        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream& bs) override {
        if (!Ctx) {
            return;
        }
        EncodeSpecs(*Ctx->Sce, &bs, Ctx->PrecisionPerBlock, Ctx->CodingMode, Ctx->Mantissas.data());
        Ctx = nullptr;
    }

    void Reset() noexcept override {
        Ctx = nullptr;
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }

private:
    TEncodeCtx* Ctx = nullptr;
};

std::vector<IBitStreamPartEncoder::TPtr> CreateEncParts()
{
    std::vector<IBitStreamPartEncoder::TPtr> parts;
    parts.emplace_back(new TConfigure());
    parts.emplace_back(new TAlloc());
    return parts;
}

} // anonymous namespace

TAtrac3BitStreamWriter::TAtrac3BitStreamWriter(ICompressedOutput* container, const TContainerParams& params, uint32_t bfuIdxConst)
    : Container(container)
    , Params(params)
    , BfuIdxConst(bfuIdxConst)
    , Encoder(CreateEncParts())
{
    NEnv::SetRoundFloat();
    if (!ATH.empty()) {
        return;
    }

    ATH.reserve(TAtrac3Data::MaxBfus);
    const auto ATHSpec = CalcATH(1024, 44100);
    for (size_t bandNum = 0; bandNum < TAtrac3Data::NumQMF; ++bandNum) {
        for (size_t blockNum = TAtrac3Data::BlocksPerBand[bandNum]; blockNum < TAtrac3Data::BlocksPerBand[bandNum + 1]; ++blockNum) {
            const size_t specNumStart = TAtrac3Data::SpecsStartLong[blockNum];
            float x = 999;
            for (size_t line = specNumStart; line < specNumStart + TAtrac3Data::SpecsPerBlock[blockNum]; line++) {
                x = fmin(x, ATHSpec[line]);
            }
            x = pow(10, 0.1f * x);
            ATH.push_back(x);
        }
    }
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
static float CalcMSRatio(float mEnergy, float sEnergy) {
    float total = sEnergy + mEnergy;
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
    ASSERT(totalUsedBits > 0);

    const int32_t maxAllowedShift = (frameSz / 2 - Div8Ceil(totalUsedBits));

    if (elements[1].ScaledBlocks.empty()) {
        return maxAllowedShift;
    } else {
        float ratio = CalcMSRatio(elements[0].Loudness, elements[1].Loudness);
        //std::cerr << ratio << std::endl;
        return std::max(std::min(ToInt(frameSz * ratio), maxAllowedShift), -maxAllowedShift);
    }
}

void TAtrac3BitStreamWriter::WriteSoundUnit(const vector<TSingleChannelElement>& singleChannelElements, float laudness)
{

    ASSERT(singleChannelElements.size() == 1 || singleChannelElements.size() == 2);

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
        ASSERT(numQmfBand > 0);
        bitStream->Write(numQmfBand - 1, 2);

        //write gain info
        for (uint32_t band = 0; band < numQmfBand; ++band) {
            const vector<TAtrac3Data::SubbandInfo::TGainPoint>& GainPoints = subbandInfo.GetGainPoints(band);
            ASSERT(GainPoints.size() < TAtrac3Data::SubbandInfo::MaxGainPointsNum);
            bitStream->Write(GainPoints.size(), 3);
            int s = 0;
            for (const TAtrac3Data::SubbandInfo::TGainPoint& point : GainPoints) {
                bitStream->Write(point.Level, 4);
                bitStream->Write(point.Location, 5);
                s++;
                ASSERT(s < 8);
            }
        }

        const int16_t bitsUsedByGainInfoAndHeader = (int16_t)bitStream->GetSizeInBits();
        bitsToAlloc[channel] -= bitsUsedByGainInfoAndHeader;
    }

    const int32_t msBytesShift = Params.Js ? CalcMSBytesShift(Params.FrameSz, singleChannelElements, bitsToAlloc) : 0; // positive - gain to m, negative to s. Must be zero if no joint stereo mode

    bitsToAlloc[0] += 8 * (halfFrameSz + msBytesShift);
    bitsToAlloc[1] += 8 * (halfFrameSz - msBytesShift);

    for (uint32_t channel = 0; channel < singleChannelElements.size(); channel++) {
        const TSingleChannelElement& sce = singleChannelElements[channel];
        NBitStream::TBitStream* bitStream = &bitStreams[channel];

        TEncodeCtx ctx;
        ctx.Sce = &sce;
        ctx.TargetBits = static_cast<uint16_t>(std::max<int32_t>(1, bitsToAlloc[channel]));
        ctx.BfuIdxConst = BfuIdxConst;
        ctx.Loudness = laudness;

        Encoder.Do(&ctx, *bitStream);

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
        ASSERT(sz == halfFrameSz);
        OutBuffer.resize(sz << 1);
        std::copy_n(OutBuffer.begin(), sz, OutBuffer.begin() + sz);
    }

    Container->WriteFrame(OutBuffer);
    OutBuffer.clear();
}

} // namespace NAtrac3
} // namespace NAtracDEnc
