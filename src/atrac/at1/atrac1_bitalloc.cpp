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

#include "atrac1_bitalloc.h"
#include "atrac1.h"
#include <atrac/atrac_psy_common.h>
#include <atrac/atrac_scale.h>
#include <math.h>
#include <cassert>
#include <bitstream/bitstream.h>
#include <env.h>

namespace NAtracDEnc {
namespace NAtrac1 {

using std::vector;
using std::cerr;
using std::endl;
using std::pair;

static const float FixedBitAllocTableLong[TAtrac1Data::MaxBfus] = {
    7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4,
    4, 4, 3, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 0, 0, 0
};

static const float FixedBitAllocTableShort[TAtrac1Data::MaxBfus] = {
    6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,
    6, 6, 6, 6,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4,   0, 0, 0, 0, 0, 0, 0, 0
};

static const uint32_t BitBoostMask[TAtrac1Data::MaxBfus] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

TBitsBooster::TBitsBooster() noexcept {
    for (uint32_t i = 0; i < TAtrac1Data::MaxBfus; ++i) {
        if (BitBoostMask[i] == 0)
            continue;
        const uint32_t nBits = TAtrac1Data::SpecsPerBlock[i];
        BitsBoostMap.insert(pair<uint32_t, uint32_t>(nBits, i));
    }
    MaxBitsPerIteration = BitsBoostMap.size() ? (--BitsBoostMap.end())->first : 0;
    MinKey = BitsBoostMap.begin()->first;
}

uint32_t TBitsBooster::ApplyBoost(std::vector<uint32_t>* bitsPerEachBlock, uint32_t cur, uint32_t target) const noexcept {
    uint32_t surplus = target - cur;
    uint32_t key = (surplus > MaxBitsPerIteration) ? MaxBitsPerIteration : surplus;
    std::multimap<uint32_t, uint32_t>::const_iterator maxIt = BitsBoostMap.upper_bound(key);
    //the key too low
    if (maxIt == BitsBoostMap.begin())
        return surplus;

    while (surplus >= MinKey) {
        bool done = true;
        for (std::multimap<uint32_t, uint32_t>::const_iterator it = BitsBoostMap.begin(); it != maxIt; ++it) {
            const uint32_t curBits = it->first;
            const uint32_t curPos = it->second;

            assert(key >= curBits);
            if (curPos >= bitsPerEachBlock->size())
                break;
            if ((*bitsPerEachBlock)[curPos] == 16u)
                continue;
            const uint32_t nBitsPerSpec = (*bitsPerEachBlock)[curPos] ? 1 : 2;
            if ((*bitsPerEachBlock)[curPos] == 0u && curBits * 2 > surplus)
                continue;
            if (curBits * nBitsPerSpec > surplus)
                continue;
            (*bitsPerEachBlock)[curPos] += nBitsPerSpec;
            surplus -= curBits * nBitsPerSpec;

            done = false;
        }
        if (done)
            break;
    }

    return surplus;
}

static std::vector<float> At1ATHLong;

void CalcAt1ATH() noexcept {
    if (At1ATHLong.size()) {
        return;
    }
    At1ATHLong.reserve(TAtrac1Data::MaxBfus);
    auto ATHSpec = CalcATH(512, 44100);
    for (size_t bandNum = 0; bandNum < TAtrac1Data::NumQMF; ++bandNum) {
        for (size_t blockNum = TAtrac1Data::BlocksPerBand[bandNum]; blockNum < TAtrac1Data::BlocksPerBand[bandNum + 1]; ++blockNum) {
           const size_t specNumStart =  TAtrac1Data::SpecsStartLong[blockNum];
           float x = 999;
           for (size_t line = specNumStart; line < specNumStart + TAtrac1Data::SpecsPerBlock[blockNum]; line++) {
                x = fmin(x, ATHSpec[line]);
           }
           x = pow(10, 0.1 * x);
           At1ATHLong.push_back(x);
        }
    }
}

static vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                           const uint32_t bfuNum,
                                           const float spread,
                                           const float shift,
                                           const TAtrac1Data::TBlockSizeMod& blockSize,
                                           const float loudness) noexcept {
    vector<uint32_t> bitsPerEachBlock(bfuNum);
    for (size_t i = 0; i < bitsPerEachBlock.size(); ++i) {
        bool shortBlock = blockSize.LogCount[TAtrac1Data::BfuToBand(i)];
        const float fix = shortBlock ? FixedBitAllocTableShort[i] : FixedBitAllocTableLong[i];
        float ath = At1ATHLong[i] * loudness;
        //std::cerr << "block: " << i << " Loudness: " << loudness << " " << 10 * log10(scaledBlocks[i].MaxEnergy / ath) << std::endl;
        if (!shortBlock && scaledBlocks[i].Energy < ath) {
            bitsPerEachBlock[i] = 0;
        } else {
            int tmp = spread * ( (float)scaledBlocks[i].ScaleFactorIndex/3.2f) + (1.0f - spread) * fix - shift;
            if (tmp > 16) {
                bitsPerEachBlock[i] = 16;
            } else if (tmp < 2) {
                bitsPerEachBlock[i] = 0;
            } else {
                bitsPerEachBlock[i] = tmp;
            }
        }
    }
    return bitsPerEachBlock;
}

static uint32_t GetMaxUsedBfuId(const vector<uint32_t>& bitsPerEachBlock) noexcept {
    uint32_t idx = 7;
    for (;;) {
        uint32_t bfuNum = TAtrac1Data::BfuAmountTab[idx];
        if (bfuNum > bitsPerEachBlock.size()) {
            idx--;
        } else if (idx != 0) {
            assert(bfuNum == bitsPerEachBlock.size());
            uint32_t i = 0;
            while (idx && bitsPerEachBlock[bfuNum - 1 - i] == 0) {
                if (++i >= (TAtrac1Data::BfuAmountTab[idx] - TAtrac1Data::BfuAmountTab[idx-1])) {
                    idx--;
                    bfuNum -= i;
                    i = 0;
                }
                assert(bfuNum - i >= 1);
            }
            break;
        } else {
            break;
        }
    }
    return idx;
}

///////////////////////////////////////////////////////////////////////////

struct TEncodeCtx {
    const TBitsBooster* Booster;
    const std::vector<TScaledBlock>& ScaledBlocks;
    const TAtrac1Data::TBlockSizeMod& BlockSize;
    const float Loudness;
    uint32_t BfuIdx;
    float Spread;
    std::vector<uint8_t> BitsPerBlock;
    static TEncodeCtx* Cast(void* p) { return reinterpret_cast<TEncodeCtx*>(p); }
};

class TConfigure final : public IBitStreamPartEncoder {
public:
    static uint32_t CalcAvaliableBitsForBfus(size_t bfuNum) noexcept {
        return TAtrac1Data::SoundUnitSize * 8 -
            TAtrac1Data::BitsPerBfuAmountTabIdx - 32 - 2 - 3 -
            bfuNum * (TAtrac1Data::BitsPerIDWL + TAtrac1Data::BitsPerIDSF);
    }

    void Dump(NBitStream::TBitStream& /*bs*/) override {};

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        TEncodeCtx* ctx = TEncodeCtx::Cast(frameData);

        ctx->BitsPerBlock.resize(TAtrac1Data::BfuAmountTab[ctx->BfuIdx]);

        ba.Start(CalcAvaliableBitsForBfus(ctx->BitsPerBlock.size()), -3, 15);

        return EStatus::Ok;
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
};

class TBfuAlloc final : public IBitStreamPartEncoder {
    uint32_t BfuIdxConst;
    vector<uint32_t> BitsPerEachBlock;
    TEncodeCtx* Ctx;
public:
    explicit TBfuAlloc(uint32_t bfuIdxConst)
        : BfuIdxConst(bfuIdxConst)
    {}

    void Dump(NBitStream::TBitStream& bs) override {

        bs.Write(0x2 - Ctx->BlockSize.LogCount[0], 2);

        bs.Write(0x2 - Ctx->BlockSize.LogCount[1], 2);

        bs.Write(0x3 - Ctx->BlockSize.LogCount[2], 2);
        bs.Write(0, 2);

        bs.Write(Ctx->BfuIdx, TAtrac1Data::BitsPerBfuAmountTabIdx);

        bs.Write(0, 2);
        bs.Write(0, 3);

        for (const auto wordLength : BitsPerEachBlock) {
            const auto tmp = wordLength ? (wordLength - 1) : 0;
            bs.Write(tmp, 4);
        }

        for (size_t i = 0; i < BitsPerEachBlock.size(); ++i) {
            bs.Write(Ctx->ScaledBlocks[i].ScaleFactorIndex, 6);
        }

        for (size_t i = 0; i < BitsPerEachBlock.size(); ++i) {
            const auto wordLength = BitsPerEachBlock[i];
            if (wordLength == 0 || wordLength == 1)
                continue;

            const float multiple = ((1 << (wordLength - 1)) - 1);
            for (const float val : Ctx->ScaledBlocks[i].Values) {
                const int tmp = lrint(val * multiple);
                const uint32_t testwl = BitsPerEachBlock[i] ? (BitsPerEachBlock[i] - 1) : 0;
                const uint32_t a = !!testwl + testwl;
                if (a != wordLength) {
                    cerr << "wordlen error " << a << " " << wordLength << endl;
                    abort();
                }
                bs.Write(NBitStream::MakeSign(tmp, wordLength), wordLength);
            }
        }

        bs.Write(0x0, 8);
        bs.Write(0x0, 8);

        bs.Write(0x0, 8);

        BitsPerEachBlock.clear();
        Ctx = nullptr;
    };

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        TEncodeCtx* ctx = TEncodeCtx::Cast(frameData);

        const bool autoBfu = !BfuIdxConst;

        float shift = ba.Continue();

        vector<uint32_t> tmpAlloc = CalcBitsAllocation(ctx->ScaledBlocks, TAtrac1Data::BfuAmountTab[ctx->BfuIdx],
            ctx->Spread, shift, ctx->BlockSize, ctx->Loudness);

        uint32_t bitsUsed = 0;
        for (size_t i = 0; i < tmpAlloc.size(); i++) {
            bitsUsed += TAtrac1Data::SpecsPerBlock[i] * tmpAlloc[i];
        }

        if (ba.Submit(bitsUsed)) {

            if (autoBfu) {
                uint32_t usedBfuId = GetMaxUsedBfuId(tmpAlloc);
                if (usedBfuId < ctx->BfuIdx) {
                    ctx->BfuIdx--;
                    return EStatus::Repeat;
                }
            }

            BitsPerEachBlock = std::move(tmpAlloc);

            ctx->Booster->ApplyBoost(&BitsPerEachBlock, bitsUsed, TConfigure::CalcAvaliableBitsForBfus(BitsPerEachBlock.size()));

            Ctx = ctx;
        }

        return EStatus::Ok;
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
};

static std::vector<IBitStreamPartEncoder::TPtr> CreateEncParts(uint32_t bfuIdxConst) noexcept
{
    vector<IBitStreamPartEncoder::TPtr> parts;
    parts.emplace_back(new TConfigure());
    parts.emplace_back(new TBfuAlloc(bfuIdxConst));
    return parts;
}

TAt1BitAlloc::TAt1BitAlloc(ICompressedOutput* container, uint32_t bfuIdxConst) noexcept
    : Encoder(CreateEncParts(bfuIdxConst))
    , Container(container)
    , BfuIdxConst(bfuIdxConst)
{
    NEnv::SetRoundFloat();
    CalcAt1ATH();
}

uint32_t TAt1BitAlloc::Write(const std::vector<TScaledBlock>& scaledBlocks, const TAtrac1Data::TBlockSizeMod& blockSize, float loudness)
{
    uint32_t bfuIdx = BfuIdxConst ? BfuIdxConst - 1 : 7;
    vector<uint8_t> bitsPerEachBlock(TAtrac1Data::BfuAmountTab[bfuIdx]);
    TEncodeCtx ctx {
        .Booster = this,
        .ScaledBlocks = scaledBlocks,
        .BlockSize = blockSize,
        .Loudness = loudness,
        .BfuIdx = bfuIdx,
        .Spread = AnalizeScaleFactorSpread(scaledBlocks),
        .BitsPerBlock = bitsPerEachBlock,
    };

    NBitStream::TBitStream bitStream;

    Encoder.Do(&ctx, bitStream);

    std::vector<char> buf = bitStream.GetBytes();

    Container->WriteFrame(buf);

    return 0;
}

} //namespace NAtrac1
} //namespace NAtracDEnc
