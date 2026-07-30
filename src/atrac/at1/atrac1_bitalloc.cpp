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
#include <algorithm>
#include <map>
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

// Blend between signal-adaptive and fixed-table bit allocation (see
// CalcBitsAllocation). AnalizeScaleFactorSpread() derives this from the standard
// deviation of the scale factor indices, but on ATRAC1 that measures worse than a
// constant across the EBU SQAM corpus: the heuristic drives spread towards 1 on
// sparse/tonal material, making allocation nearly proportional to band energy and
// starving quiet bands that have no loud neighbour to mask them.
//
// Mean noise-to-mask ratio over the 70-track corpus (lower is better):
//   heuristic -13.09 dB | 0.30 -13.72 | 0.35 -13.75 | 0.40 -13.76 | 0.45 -13.73
// and clamping the heuristic rather than replacing it is worse the wider the
// clamp, so the optimum is broad and the statistic itself is not carrying signal.
// ATRAC3 still uses AnalizeScaleFactorSpread(); it was not evaluated here.
static constexpr float BitAllocSpread = 0.4f;

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

// Bias applied to the middle and high QMF bands, scaled by how low-heavy the
// spectrum is. Tuned on the EBU SQAM corpus; see the comment in
// CalcBitsAllocation() and the commit message.
static constexpr float BandBiasGain = 0.3f;       // per unit of tilt
static constexpr float BandBiasTiltFloor = 7.0f;  // no bias below this tilt
static constexpr float BandBiasMax = 1.5f;        // cap, see gong regression
static constexpr float BandBiasHighRatio = 0.5f;  // high band gets half of it

// Mean scale factor index of the low band minus that of the middle band: a cheap
// measure of how much of the energy sits below 5.5 kHz.
static float CalcLowToMidTilt(const std::vector<TScaledBlock>& scaledBlocks,
                              const uint32_t bfuNum) noexcept {
    float sumLow = 0.0f, sumMid = 0.0f;
    uint32_t nLow = 0, nMid = 0;
    for (size_t i = 0; i < bfuNum; ++i) {
        switch (TAtrac1Data::BfuToBand(i)) {
            case 0: sumLow += scaledBlocks[i].ScaleFactorIndex; nLow++; break;
            case 1: sumMid += scaledBlocks[i].ScaleFactorIndex; nMid++; break;
            default: break;
        }
    }
    if (!nLow || !nMid)
        return 0.0f;
    return sumLow / nLow - sumMid / nMid;
}


// --- rate-distortion allocation -------------------------------------------
//
// CalcBitsAllocation above decides how many bits the frame spends and how many
// BFUs it transmits. It also decides each word length, from a formula, and
// nothing checks what that costs in distortion. Choosing the word lengths by
// search instead -- spending the same bits over the same BFUs, but putting each
// where it removes the most audible noise -- measures better on every statistic
// below.
//
// The distortion is exact, not modelled. ATRAC1 reconstructs a coefficient as
// q/(2^(w-1)-1), so the error a candidate word length leaves follows directly
// from the scaled coefficients the encoder already holds.

static const uint32_t MaxWordLen = 16;
static const float BarkStep = 0.5f;
static const uint32_t ShortMdctBins = 32;

static std::vector<float> At1BandZc;                    // band centre, Bark
static std::vector<float> At1BandAth;                   // band ATH, linear power
static std::vector<std::vector<float>> At1BandSpread;   // masker -> maskee
static std::vector<uint32_t> At1LineBand[2];            // [shortWin][spectral line]
static std::vector<std::vector<pair<uint32_t, float>>> At1BfuShare[2];

static float ToBark(float f) noexcept {
    return 13.0f * atanf(0.00076f * f) + 3.5f * atanf((f / 7500.0f) * (f / 7500.0f));
}

// Frequency of spectral line s. Under a short window each QMF band is
// transformed in 32-bin sub-blocks, so the frequency follows the line's position
// inside its sub-block, not its position in the buffer.
static float LineHz(uint32_t s, bool shortWin) noexcept {
    const float lo = s < 128 ? 0.0f : (s < 256 ? 5512.5f : 11025.0f);
    const float width = s < 256 ? 5512.5f : 11025.0f;
    const uint32_t base = s < 128 ? 0 : (s < 256 ? 128 : 256);
    const float pos = shortWin ? (s - base) % ShortMdctBins + 0.5f
                               : (s - base) + 0.5f;
    const float bins = shortWin ? ShortMdctBins : (s < 256 ? 128 : 256);
    return lo + pos / bins * width;
}

// A 0.5 Bark grid over the spectral lines, and for each BFU the share of its
// lines falling in each band. Quantisation noise is uniform per line, so that
// share is how much of the BFU's noise lands in each band.
static void CalcAt1BarkGrid() noexcept {
    if (At1BandZc.size())
        return;
    const uint32_t nBands = (uint32_t)(ToBark(22050.0f) / BarkStep) + 1;
    At1BandZc.resize(nBands);
    for (uint32_t b = 0; b < nBands; ++b)
        At1BandZc[b] = (b + 0.5f) * BarkStep;

    At1BandSpread.assign(nBands, std::vector<float>(nBands, 0.0f));
    for (uint32_t b = 0; b < nBands; ++b) {
        for (uint32_t c = 0; c < nBands; ++c) {
            const float dz = At1BandZc[b] - At1BandZc[c] + 0.474f;
            const float sf = 15.81f + 7.5f * dz - 17.5f * sqrtf(1.0f + dz * dz);
            At1BandSpread[b][c] = powf(10.0f, 0.1f * sf);
        }
    }

    for (uint32_t m = 0; m < 2; ++m) {
        At1LineBand[m].resize(TAtrac1Data::NumSamples);
        for (uint32_t s = 0; s < TAtrac1Data::NumSamples; ++s) {
            const uint32_t b = (uint32_t)(ToBark(LineHz(s, m != 0)) / BarkStep);
            At1LineBand[m][s] = std::min(b, nBands - 1);
        }
        At1BfuShare[m].assign(TAtrac1Data::MaxBfus, {});
        for (uint32_t i = 0; i < TAtrac1Data::MaxBfus; ++i) {
            const uint32_t start = m ? TAtrac1Data::SpecsStartShort[i]
                                     : TAtrac1Data::SpecsStartLong[i];
            const uint32_t n = TAtrac1Data::SpecsPerBlock[i];
            std::map<uint32_t, uint32_t> hist;
            for (uint32_t k = 0; k < n; ++k)
                hist[At1LineBand[m][start + k]]++;
            for (const auto& kv : hist)
                At1BfuShare[m][i].emplace_back(kv.first, (float)kv.second / n);
        }
    }

    // Band ATH: the quietest line in the band. Bands with no line of their own
    // inherit the one below.
    const auto athSpec = CalcATH(TAtrac1Data::NumSamples, 44100);
    std::vector<float> athDb(nBands, 1e9f);
    for (uint32_t s = 0; s < TAtrac1Data::NumSamples; ++s) {
        const uint32_t b = At1LineBand[0][s];
        athDb[b] = fmin(athDb[b], athSpec[s]);
    }
    float last = 0.0f;
    for (uint32_t b = 0; b < nBands; ++b)
        if (athDb[b] < 1e8f) { last = athDb[b]; break; }
    At1BandAth.resize(nBands);
    for (uint32_t b = 0; b < nBands; ++b) {
        if (athDb[b] < 1e8f)
            last = athDb[b];
        At1BandAth[b] = powf(10.0f, 0.1f * last);
    }
}

// 1 / masking threshold per BFU, resolved on the Bark grid rather than per BFU.
// A threshold per BFU credits each BFU with masking itself, which is wrong for a
// loud narrow tone: block floating point spreads its quantisation noise over
// every line of the BFU, including where the tone masks nothing.
static vector<float> CalcBarkWeights(const std::vector<TScaledBlock>& scaledBlocks,
                                     const uint32_t bfuNum,
                                     const TAtrac1Data::TBlockSizeMod& blockSize,
                                     const float loudness) noexcept {
    CalcAt1BarkGrid();
    const uint32_t nBands = (uint32_t)At1BandZc.size();

    vector<float> power(nBands, 0.0f);
    for (uint32_t i = 0; i < bfuNum; ++i) {
        const bool sw = blockSize.LogCount[TAtrac1Data::BfuToBand(i)] != 0;
        const float scale = TAtrac1Data::ScaleTable[scaledBlocks[i].ScaleFactorIndex];
        const uint32_t start = sw ? TAtrac1Data::SpecsStartShort[i]
                                  : TAtrac1Data::SpecsStartLong[i];
        for (uint32_t k = 0; k < scaledBlocks[i].Values.size(); ++k) {
            const float v = scaledBlocks[i].Values[k] * scale;
            power[At1LineBand[sw][start + k]] += v * v;
        }
    }

    // Tonality from the flatness of the whole frame: tonal maskers mask less.
    double logSum = 0.0, arith = 0.0;
    for (uint32_t b = 0; b < nBands; ++b) {
        const double e = std::max((double)power[b], 1e-30);
        logSum += log(e);
        arith += e;
    }
    const double geo = exp(logSum / nBands);
    const float sfmDb = 10.0f * log10f((float)std::max(geo / std::max(arith / nBands, 1e-30), 1e-30));
    const float tonal = std::min(1.0f, std::max(0.0f, sfmDb / -60.0f));

    vector<float> thr(nBands);
    for (uint32_t b = 0; b < nBands; ++b) {
        float mask = 0.0f;
        for (uint32_t c = 0; c < nBands; ++c)
            mask += At1BandSpread[b][c] * power[c];
        const float offsetDb = tonal * (14.5f + At1BandZc[b]) + (1.0f - tonal) * 5.5f;
        thr[b] = std::max(mask / powf(10.0f, 0.1f * offsetDb), At1BandAth[b] * loudness);
    }

    vector<float> weight(bfuNum, 0.0f);
    for (uint32_t i = 0; i < bfuNum; ++i) {
        const bool sw = blockSize.LogCount[TAtrac1Data::BfuToBand(i)] != 0;
        float w = 0.0f;
        for (const auto& bs : At1BfuShare[sw][i])
            w += bs.second / std::max(thr[bs.first], 1e-30f);
        weight[i] = w;
    }
    return weight;
}

// noise[w] = squared quantisation error in this BFU at word length w, in the
// absolute (pre-scaling) domain. w = 0 and 1 both mean the BFU is dropped.
static void CalcNoiseTable(const TScaledBlock& block, float* noise) noexcept {
    const double scale = TAtrac1Data::ScaleTable[block.ScaleFactorIndex];
    const double s2 = scale * scale;
    double dropped = 0.0;
    for (const float v : block.Values)
        dropped += (double)v * v;
    noise[0] = noise[1] = (float)(dropped * s2);
    for (uint32_t w = 2; w <= MaxWordLen; ++w) {
        const double lim = (1 << (w - 1)) - 1;
        double err = 0.0;
        for (const float v : block.Values) {
            const double d = (double)v - lrint(v * lim) / lim;
            err += d * d;
        }
        noise[w] = (float)(err * s2);
    }
}

// Word lengths spending `budget` bits over `bfuNum` BFUs, chosen by search.
static vector<uint32_t> CalcGreedyAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                             const uint32_t bfuNum,
                                             const TAtrac1Data::TBlockSizeMod& blockSize,
                                             const float loudness,
                                             const uint32_t budget) noexcept {
    vector<float> noise(bfuNum * (MaxWordLen + 1));
    for (uint32_t i = 0; i < bfuNum; ++i)
        CalcNoiseTable(scaledBlocks[i], &noise[i * (MaxWordLen + 1)]);

    const vector<float> weight = CalcBarkWeights(scaledBlocks, bfuNum, blockSize, loudness);

    // Spend each bit where it removes the most threshold-weighted noise.
    //
    // Each step considers every reachable word length, not just the next one up.
    // A block-floating-point rate-distortion curve has PLATEAUS: a coefficient of
    // 0.841 quantises to 1/1 at word length 2 and to 3/3 at 3, the same
    // reconstructed value, so that step buys nothing, while 4 drops the noise
    // 9 dB. A search taking only strictly positive single steps is trapped behind
    // the plateau and starves the band holding a loud tone down to two bits.
    vector<uint32_t> wordLen(bfuNum, 0);
    uint32_t spent = 0;
    for (;;) {
        uint32_t bestBfu = bfuNum, bestNext = 0;
        float bestGain = 0.0f, bestCost = 1.0f;
        for (uint32_t i = 0; i < bfuNum; ++i) {
            const float* n = &noise[i * (MaxWordLen + 1)];
            const uint32_t nLines = TAtrac1Data::SpecsPerBlock[i];
            // 1 is not a legal ATRAC1 word length, so an unused BFU starts at 2.
            const uint32_t from = wordLen[i] ? wordLen[i] : 1;
            for (uint32_t next = from + 1; next <= MaxWordLen; ++next) {
                const uint32_t cost = nLines * (next - wordLen[i]);
                if (spent + cost > budget)
                    break;
                const float gain = (n[wordLen[i]] - n[next]) * weight[i];
                // gain/cost > bestGain/bestCost, without the divisions
                if (gain > 0.0f && gain * bestCost > bestGain * cost) {
                    bestBfu = i;
                    bestNext = next;
                    bestGain = gain;
                    bestCost = (float)cost;
                }
            }
        }
        if (bestBfu == bfuNum)
            break;
        spent += TAtrac1Data::SpecsPerBlock[bestBfu] * (bestNext - wordLen[bestBfu]);
        wordLen[bestBfu] = bestNext;
    }
    return wordLen;
}

static vector<uint32_t> CalcBitsAllocation(const std::vector<TScaledBlock>& scaledBlocks,
                                           const uint32_t bfuNum,
                                           const float spread,
                                           const float shift,
                                           const TAtrac1Data::TBlockSizeMod& blockSize,
                                           const float loudness) noexcept {
    vector<uint32_t> bitsPerEachBlock(bfuNum);

    // Sustained tonal material (woodwinds, bowed strings) leaves the low band far
    // below the masking threshold while 8-11 kHz is audibly starved: those BFUs
    // end up with a word length near zero in 40-49% of frames. Shift a little of
    // the allocation upwards when the spectrum is low-heavy.
    //
    // A constant bias does not work -- it helps tonal material and costs
    // spectrally flat material about as much, because percussion has no spare
    // margin in the low band to give away. Scaling it by how low-heavy the
    // spectrum is avoids that; the tilt is derived from data already to hand.
    const float tilt = CalcLowToMidTilt(scaledBlocks, bfuNum);
    const float midBias = std::min(BandBiasMax,
                                   BandBiasGain * std::max(0.0f, tilt - BandBiasTiltFloor));
    const float bandBias[TAtrac1Data::NumQMF] = {0.0f, midBias, midBias * BandBiasHighRatio};

    for (size_t i = 0; i < bitsPerEachBlock.size(); ++i) {
        bool shortBlock = blockSize.LogCount[TAtrac1Data::BfuToBand(i)];
        const float fix = shortBlock ? FixedBitAllocTableShort[i] : FixedBitAllocTableLong[i];
        float ath = At1ATHLong[i] * loudness;
        //std::cerr << "block: " << i << " Loudness: " << loudness << " " << 10 * log10(scaledBlocks[i].MaxEnergy / ath) << std::endl;
        if (!shortBlock && scaledBlocks[i].Energy < ath) {
            bitsPerEachBlock[i] = 0;
        } else {
            int tmp = spread * ( (float)scaledBlocks[i].ScaleFactorIndex/3.2f) + (1.0f - spread) * fix - shift
                    + bandBias[TAtrac1Data::BfuToBand(i)];
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

            // How many bits this frame spends and how many BFUs it transmits are
            // now fixed. Spend exactly those bits over exactly those BFUs, but
            // choose the word lengths by search rather than by formula.
            uint32_t budget = 0;
            for (size_t i = 0; i < BitsPerEachBlock.size(); ++i)
                budget += TAtrac1Data::SpecsPerBlock[i] * BitsPerEachBlock[i];
            BitsPerEachBlock = CalcGreedyAllocation(ctx->ScaledBlocks, BitsPerEachBlock.size(),
                                                    ctx->BlockSize, ctx->Loudness, budget);

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
    TEncodeCtx ctx{
        this,
        scaledBlocks,
        blockSize,
        loudness,
        bfuIdx,
        BitAllocSpread,
        bitsPerEachBlock,
    };

    NBitStream::TBitStream bitStream;

    Encoder.Do(&ctx, bitStream);

    std::vector<char> buf = bitStream.GetBytes();

    Container->WriteFrame(buf);

    return 0;
}

} //namespace NAtrac1
} //namespace NAtracDEnc
