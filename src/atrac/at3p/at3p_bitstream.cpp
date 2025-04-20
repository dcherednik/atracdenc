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

#include "at3p_bitstream_impl.h"
#include "at3p_bitstream.h"
#include "at3p_gha.h"
#include "at3p_tables.h"
#include <env.h>
#include <util.h>

#include "ff/atrac3plus_data.h"

#include <iostream>
#include <limits>
#include <memory>

namespace NAtracDEnc {

using namespace NAt3p;
using std::vector;
using std::make_pair;
using std::pair;

static THuffTables HuffTabs;

TTonePackResult CreateFreqBitPack(const TAt3PGhaData::TWaveParam* const param, int len)
{
    const int MaxBits = 10;

    int bits[2] = {MaxBits, MaxBits};

    std::vector<TTonePackResult::TEntry> res[2];
    res[0].reserve(len);
    res[1].reserve(len);

    // try ascending order
    {
        auto& t = res[0];
        uint16_t prevFreqIndex = param->FreqIndex & 1023;
        t.emplace_back(TTonePackResult::TEntry{prevFreqIndex, MaxBits});

        for (int i = 1; i < len; i++) {
            uint16_t curFreqIndex = param[i].FreqIndex & 1023;
            if (prevFreqIndex < 512) {
                t.emplace_back(TTonePackResult::TEntry{curFreqIndex, MaxBits});
                bits[0] += MaxBits;
            } else {
                uint16_t b = GetFirstSetBit(1023 - prevFreqIndex) + 1;
                uint16_t code = curFreqIndex - (1024 - (1 << b));
                t.emplace_back(TTonePackResult::TEntry{code, b});
                bits[0] += b;
            }
            prevFreqIndex = curFreqIndex;
        }
    }

    // try descending order
    if (len > 1) {
        auto& t = res[1];
        uint16_t prevFreqIndex = param[len - 1].FreqIndex & 1023;
        t.emplace_back(TTonePackResult::TEntry{prevFreqIndex, MaxBits});

        for (int i = len - 2; i >= 0; i--) {
            uint16_t curFreqIndex = param[i].FreqIndex & 1023;
            uint16_t b = GetFirstSetBit(prevFreqIndex) + 1;
            t.emplace_back(TTonePackResult::TEntry{curFreqIndex, b});
            bits[1] += b;
            prevFreqIndex = curFreqIndex;
        }

    }

    if (len == 1 || bits[0] < bits[1]) {
        return {res[0], bits[0], ETonePackOrder::ASC};
    } else {
        return {res[1], bits[1], ETonePackOrder::DESC};
    }
}

size_t TDumper::GetConsumption() const noexcept
{
    return std::accumulate(Buf.begin(), Buf.end(), 0,
        [](size_t acc, const std::pair<uint8_t, uint8_t>& x) noexcept -> size_t { return acc + x.second; });
}

IBitStreamPartEncoder::EStatus TConfigure::Encode(void* frameData, TBitAllocHandler&)
{
    TSpecFrame* frame = TSpecFrame::Cast(frameData);

    size_t numQuantUnits = 28;
    frame->NumQuantUnits = numQuantUnits;
    frame->WordLen.resize(numQuantUnits);

    for (size_t i = 0; i < frame->WordLen.size(); i++) {
        static uint8_t allocTable[32] = {
            7, 7, 7, 7, 7, 6, 6, 6,
            6, 6, 6, 5, 5, 5, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 4,
            3, 2, 1, 1, 1, 1, 1, 1
        };
        frame->WordLen[i].first = allocTable[i];
        frame->WordLen[i].second = allocTable[i];
    }

    frame->SfIdx.resize(numQuantUnits);

    for (size_t i = 0; i < frame->SfIdx.size(); i++) {
        frame->SfIdx[i].first = frame->Chs[0].ScaledBlocks.at(i).ScaleFactorIndex;
        if (frame->Chs.size() > 1)
            frame->SfIdx[i].second = frame->Chs[1].ScaledBlocks.at(i).ScaleFactorIndex;
    }

    frame->SpecTabIdx.resize(numQuantUnits);

    Insert(numQuantUnits - 1, 5);
    Insert(0, 1); //mute flag

    frame->AllocatedBits = GetConsumption();

    return EStatus::Ok;
}

IBitStreamPartEncoder::EStatus TWordLenEncoder::Encode(void* frameData, TBitAllocHandler&) {
    auto specFrame = TSpecFrame::Cast(frameData);

    ASSERT(specFrame->WordLen.size() > specFrame->NumQuantUnits);
    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {

        Insert(0, 2); // 0 - constant number of bits

        if (ch == 0) {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->WordLen[i].first, 3);
            }
        } else {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->WordLen[i].second, 3);
            }
        }
    }

    return EStatus::Ok;
}

IBitStreamPartEncoder::EStatus TSfIdxEncoder::Encode(void* frameData, TBitAllocHandler&) {
    auto specFrame = TSpecFrame::Cast(frameData);

    if (specFrame->SfIdx.empty()) {
        return EStatus::Ok;
    }

    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {

        Insert(0, 2); // 0 - constant number of bits

        if (ch == 0) {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->SfIdx[i].first, 6);
            }
        } else {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->SfIdx[i].second, 6);
            }
        }
    }

    return EStatus::Ok;
}

void TQuantUnitsEncoder::EncodeCodeTab(bool useFullTable, size_t channels,
    size_t numQuantUnits, const std::vector<std::pair<uint8_t, uint8_t>>& specTabIdx,
    std::vector<std::pair<uint16_t, uint8_t>>& data)
{
    data.emplace_back(useFullTable, 1); // use full table

    for (size_t ch = 0; ch < channels; ch++) {

        data.emplace_back(0, 1); // table type

        data.emplace_back(0, 2); // 0 - constant number of bits

        data.emplace_back(0, 1); // num_coded_vals equal to used_quant_units

        if (ch == 0) {
            for (size_t i = 0; i < numQuantUnits; i++) {
                data.emplace_back(specTabIdx[i].first, useFullTable + 2);
            }
        } else {
            for (size_t i = 0; i < numQuantUnits; i++) {
                data.emplace_back(specTabIdx[i].second, useFullTable + 2);
            }
        }
    }
}

void TQuantUnitsEncoder::EncodeQuSpectra(const int* qspec, const size_t num_spec, const size_t idx,
    std::vector<std::pair<uint16_t, uint8_t>>& data) {
    const Atrac3pSpecCodeTab *tab = &atrac3p_spectra_tabs[idx];
    const std::array<TVlcElement, 256>& vlcTab = HuffTabs.VlcSpecs[idx];

    size_t groupSize = tab->group_size;
    size_t numCoeffs = tab->num_coeffs;
    size_t bitsCoeff = tab->bits;
    bool isSigned  = tab->is_signed;

    for (size_t pos = 0; pos < num_spec;) {
        if (groupSize != 1) {
            // TODO: Acording to FFmpeg it should be possible
            // to skip group, if all rest of coeffs is zero
            // but this should be checked with real AT3P decoder
            data.emplace_back(1, 1);
        }

        for (size_t j = 0; j < groupSize; j++) {
            uint32_t val = 0;
            int8_t signs[4] = {0};
            for (size_t i = 0; i < numCoeffs; i++) {
                int16_t t = qspec[pos++];
#ifndef NDEBUG
                {
                    uint16_t x = std::abs(t);
                    x >>= (uint16_t)(bitsCoeff - (int)isSigned);
                    ASSERT(x == 0);
                }
#endif
                if (!isSigned && t != 0) {
                    signs[i] = t > 0 ? 1 : -1;
                    if (t < 0)
                        t = -t;
                } else {
                    t = t & ((1u << (bitsCoeff)) - 1);
                }
                t <<= (bitsCoeff * i);
                val |= t;
            }

            ASSERT(val > 255);

            const TVlcElement& el = vlcTab.at(val);

            data.emplace_back(el.Code, el.Len);
            for (size_t i = 0; i < 4; i++) {
                if (signs[i] != 0) {
                    if (signs[i] > 0) {
                        data.emplace_back(0, 1);
                    } else {
                        data.emplace_back(1, 1);
                    }
                }
            }
        }
    }
}

size_t TQuantUnitsEncoder::TUnit::MakeKey(size_t ch, size_t qu, size_t worlen) {
    ASSERT(qu < 32);
    ASSERT(worlen < 8);
    return (ch << 8) | (qu << 3) | worlen;
}

TQuantUnitsEncoder::TUnit::TUnit(size_t qu, size_t wordlen)
    : Wordlen(wordlen)
    , Multiplier(1.0f / atrac3p_mant_tab[wordlen])
{
    Mantisas.resize(TScaleTable::SpecsPerBlock[qu]);
}

size_t TQuantUnitsEncoder::TUnit::GetOrCompute(const float* val, std::vector<std::pair<uint16_t, uint8_t>>& res)
{
    QuantMantisas(val, 0, Mantisas.size(), Multiplier, false, Mantisas.data());

    std::vector<std::pair<uint16_t, uint8_t>> tmp;

    size_t bestTab = 0;
    size_t consumed = std::numeric_limits<size_t>::max();

    for (size_t i = 0, tabIndex = Wordlen - 1; i < 8; i++, tabIndex += 7) {

        EncodeQuSpectra(Mantisas.data(), Mantisas.size(), tabIndex, tmp);

        size_t t = std::accumulate(tmp.begin(), tmp.end(), 0,
            [](size_t acc, const std::pair<uint8_t, uint16_t>& x) noexcept -> size_t
                { return acc + x.second; });

        if (t < consumed) {
            consumed = t;
            ConsumedBits = t;
            bestTab = i;
            res.clear();
            res.swap(tmp);
            tmp.clear();
        } else {
            tmp.clear();
        }
    }
    return bestTab;
}

IBitStreamPartEncoder::EStatus TQuantUnitsEncoder::Encode(void* frameData, TBitAllocHandler&)
{
    auto specFrame = TSpecFrame::Cast(frameData);
    std::vector<
            std::vector<std::pair<uint16_t, uint8_t>>> data;
    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {
        auto& chData = specFrame->Chs.at(ch);
        auto scaledBlocks = chData.ScaledBlocks;

        for (size_t qu = 0; qu < specFrame->NumQuantUnits; qu++) {
            size_t len = (ch == 0) ?
                specFrame->WordLen.at(qu).first :
                specFrame->WordLen.at(qu).second;

            size_t key = TUnit::MakeKey(ch, qu, len);

            TUnit* unit;
            //  try_emplace
            auto it = UnitBuffers.find(key);
            if (it == UnitBuffers.end()) {
                unit = &(UnitBuffers.emplace(key, TUnit(qu, len)).first->second);
            } else {
                unit = &it->second;
            }

            const float* values = scaledBlocks.at(qu).Values.data();

            data.push_back(std::vector<std::pair<uint16_t, uint8_t>>());
            auto tabIdx = unit->GetOrCompute(values, data.back());

            if (ch == 0) {
                specFrame->SpecTabIdx[qu].first = tabIdx;
            } else {
                specFrame->SpecTabIdx[qu].second = tabIdx;
            }
        }
        if (true /*frame.NumUsedQuantUnits > 2*/) {
            size_t numPwrSpec = atrac3p_subband_to_num_powgrps[atrac3p_qu_to_subband[specFrame->NumQuantUnits - 1]];
            data.push_back(std::vector<std::pair<uint16_t, uint8_t>>());
            auto& t = data.back();
            for (size_t i = 0; i < numPwrSpec; i++) {
                t.emplace_back(15, 4);
            }
        }
    }

    {
        std::vector<std::pair<uint16_t, uint8_t>> tabIdxData;
        EncodeCodeTab(true, specFrame->Chs.size(), specFrame->NumQuantUnits, specFrame->SpecTabIdx, tabIdxData);
        for (size_t i = 0; i < tabIdxData.size(); i++) {
            Insert(tabIdxData[i].first, tabIdxData[i].second);
        }
    }

    for (const auto& x : data) {
        for (size_t i = 0; i < x.size(); i++) {
            Insert(x[i].first, x[i].second);
        }
    }

    return EStatus::Ok;
}

static std::vector<IBitStreamPartEncoder::TPtr> CreateEncParts()
{
    vector<IBitStreamPartEncoder::TPtr> parts;
    parts.emplace_back(new TConfigure());
    parts.emplace_back(new TWordLenEncoder());
    parts.emplace_back(new TSfIdxEncoder());
    parts.emplace_back(new TQuantUnitsEncoder());

    return parts;
}

TAt3PBitStream::TAt3PBitStream(ICompressedOutput* container, uint16_t frameSz)
    : Container(container)
    , Encoder(CreateEncParts())
    , FrameSz(frameSz)
{
    NEnv::SetRoundFloat();
}

static void WriteSubbandFlags(NBitStream::TBitStream& bs, const bool* flags, int numFlags)
{

    int sum = 0;
    for (int i = 0; i < numFlags; i++) {
        sum += (uint32_t)flags[i];
    }

    if (sum == 0) {
        bs.Write(0, 1);
    } else if (sum == numFlags) {
        bs.Write(1, 1);
        bs.Write(0, 1);
    } else {
        bs.Write(1, 1);
        bs.Write(1, 1);
        for (int i = 0; i < numFlags; i++) {
           bs.Write(flags[i], 1);
        }
    }
}

static void WriteTonalBlock(NBitStream::TBitStream& bs, int channels, const TAt3PGhaData* tonalBlock)
{
    //GHA amplidude mode 1
    bs.Write(1, 1);

    //Num tone bands
    const TVlcElement& tbHuff = HuffTabs.NumToneBands[tonalBlock->NumToneBands - 1];
    bs.Write(tbHuff.Code, tbHuff.Len);

    if (channels == 2) {
        WriteSubbandFlags(bs, tonalBlock->ToneSharing, tonalBlock->NumToneBands);
        WriteSubbandFlags(bs, &tonalBlock->SecondIsLeader, 1);
        bs.Write(0, 1);
    }

    for (int ch = 0; ch < channels; ch++) {
        if (ch) {
            // each channel has own envelope
            bs.Write(0, 1);
        }
        // Envelope data
        for (int i = 0; i < tonalBlock->NumToneBands; i++) {
            if (ch && tonalBlock->ToneSharing[i]) {
                continue;
            }

            const auto envelope = tonalBlock->GetEnvelope(ch, i);

            if (envelope.first != TAt3PGhaData::EMPTY_POINT) {
                // start point present
                bs.Write(1, 1);
                bs.Write(envelope.first, 5);
            } else {
                bs.Write(0, 1);
            }

            if (envelope.second != TAt3PGhaData::EMPTY_POINT) {
                // stop point present
                bs.Write(1, 1);
                bs.Write(envelope.second, 5);
            } else {
                bs.Write(0, 1);
            }
        }

        // Num waves
        int mode = 0; //TODO: Calc mode
        bs.Write(mode, ch + 1);
        for (int i = 0; i < tonalBlock->NumToneBands; i++) {
            if (ch && tonalBlock->ToneSharing[i]) {
                continue;
            }
            bs.Write(tonalBlock->GetNumWaves(ch, i), 4);
        }
        // Tones freq
        if (ch) {
            // 0 - independed
            // 1 - delta to leader
            bs.Write(0, 1);
        }

        for (int i = 0; i < tonalBlock->NumToneBands; i++) {
            if (ch && tonalBlock->ToneSharing[i]) {
                continue;
            }

            auto numWaves = tonalBlock->GetNumWaves(ch, i);
            if (numWaves == 0) {
                continue;
            }

            const auto w = tonalBlock->GetWaves(ch, i);
            const auto pkt = CreateFreqBitPack(w.first, w.second);

            if (numWaves > 1) {
                bs.Write(static_cast<bool>(pkt.Order), 1);
            }

            for (const auto& d : pkt.Data) {
                bs.Write(d.Code, d.Bits);
            }
        }

        // Amplitude
        mode = 0; //TODO: Calc mode
        bs.Write(mode, ch + 1);

        for (int i = 0; i < tonalBlock->NumToneBands; i++) {
            if (ch && tonalBlock->ToneSharing[i]) {
                continue;
            }

            auto numWaves = tonalBlock->GetNumWaves(ch, i);
            if (numWaves == 0) {
                continue;
            }

            const auto w = tonalBlock->GetWaves(ch, i);
            for (size_t j = 0; j < numWaves; j++) {
                bs.Write(w.first[j].AmpSf, 6);
            }
        }

        // Phase
        for (int i = 0; i < tonalBlock->NumToneBands; i++) {
            if (ch && tonalBlock->ToneSharing[i]) {
                continue;
            }

            auto numWaves = tonalBlock->GetNumWaves(ch, i);
            if (numWaves == 0) {
                continue;
            }

            const auto w = tonalBlock->GetWaves(ch, i);
            for (size_t j = 0; j < w.second; j++) {
                bs.Write(w.first[j].PhaseIndex, 5);
            }
        }
    }
}

void TAt3PBitStream::WriteFrame(int channels, const TAt3PGhaData* /*tonalBlock*/, const std::vector<std::vector<TScaledBlock>>& scaledBlocks)
{
    NBitStream::TBitStream bitStream;
    // First bit must be zero
    bitStream.Write(0, 1);
    // Channel block type
    // 0 - MONO block
    // 1 - STEREO block
    // 2 - Nobody know
    bitStream.Write(channels - 1, 2);

    TSpecFrame frame(FrameSz * 8, channels, scaledBlocks);

    Encoder.Do(&frame, bitStream);

    if (channels == 2) {
        bitStream.Write(0, 2); //swap_channels and negate_coeffs
    }

    for (size_t ch = 0; ch < frame.Chs.size(); ch++) {
        bitStream.Write(0, 1); // window shape
    }

    for (size_t ch = 0; ch < frame.Chs.size(); ch++) {
        bitStream.Write(0, 1); //gain comp
    }

    bitStream.Write(0, 1);
    bitStream.Write(0, 1); // no noise info
    // Terminator
    bitStream.Write(3, 2);

    std::vector<char> buf = bitStream.GetBytes();

    buf.resize(FrameSz);
    Container->WriteFrame(buf);
}

}
