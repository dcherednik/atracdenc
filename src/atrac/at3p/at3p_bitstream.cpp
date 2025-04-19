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

    for (size_t i = 0; i < frame->SpecTabIdx.size(); i++) {
        frame->SpecTabIdx[i].first = 7;
        frame->SpecTabIdx[i].second = 7;
    }

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

IBitStreamPartEncoder::EStatus TCodeTabEncoder::Encode(void* frameData, TBitAllocHandler&) {
    auto specFrame = TSpecFrame::Cast(frameData);

    const size_t useFullTable = 1;
    Insert(useFullTable, 1); // use full table

    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {

        Insert(0, 1); // table type

        Insert(0, 2); // 0 - constant number of bits

        Insert(0, 1); // num_coded_vals equal to used_quant_units

        if (ch == 0) {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->SpecTabIdx[i].first, useFullTable + 2);
            }
        } else {
            for (size_t i = 0; i < specFrame->NumQuantUnits; i++) {
                Insert(specFrame->SpecTabIdx[i].second, useFullTable + 2);
            }
        }
    }

    return EStatus::Ok;
}

void TQuantUnitsEncoder::EncodeQuSpectra(const int* qspec, const size_t num_spec, const size_t idx) {
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
            Insert(1, 1);
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

            Insert(el.Code, el.Len);
            for (size_t i = 0; i < 4; i++) {
                if (signs[i] != 0) {
                    if (signs[i] > 0) {
                        Insert(0, 1);
                    } else {
                        Insert(1, 1);
                    }
                }
            }
        }
    }
}

IBitStreamPartEncoder::EStatus TQuantUnitsEncoder::Encode(void* frameData, TBitAllocHandler&) {
    auto specFrame = TSpecFrame::Cast(frameData);
    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {
        auto& chData = specFrame->Chs.at(ch);
        auto scaledBlocks = chData.ScaledBlocks;

        int * const mant = chData.Mant;
        for (size_t qu = 0; qu < specFrame->NumQuantUnits; qu++) {
            auto lenIdx = (ch == 0) ?
                        specFrame->WordLen.at(qu).first :
                        specFrame->WordLen.at(qu).second;
            const uint32_t first = TScaleTable::BlockSizeTab[qu];
            const uint32_t last = TScaleTable::BlockSizeTab[qu+1];
            auto mul = 1/atrac3p_mant_tab[lenIdx];

            const float* values = scaledBlocks.at(qu).Values.data();

            QuantMantisas(values, first, last, mul, false, mant);
        }
    }

    for (size_t ch = 0; ch < specFrame->Chs.size(); ch++) {
        auto& chData = specFrame->Chs.at(ch);
        int * const mant = chData.Mant;
        for (size_t qu = 0; qu < specFrame->NumQuantUnits; qu++) {
            const size_t numSpecs = TScaleTable::BlockSizeTab[qu + 1] -
                TScaleTable::BlockSizeTab[qu];
            size_t tabIndex = (ch == 0) ?
                specFrame->SpecTabIdx.at(qu).first :
                specFrame->SpecTabIdx.at(qu).second;
            size_t wordLen = (ch == 0) ?
                specFrame->WordLen.at(qu).first :
                specFrame->WordLen.at(qu).second;

            tabIndex = tabIndex * 7 + wordLen - 1;

            EncodeQuSpectra(&mant[TScaleTable::BlockSizeTab[qu]], numSpecs, tabIndex);
        }
        if (true /*frame.NumUsedQuantUnits > 2*/) {
            size_t numPwrSpec = atrac3p_subband_to_num_powgrps[atrac3p_qu_to_subband[specFrame->NumQuantUnits - 1]];
            for (size_t i = 0; i < numPwrSpec; i++) {
                Insert(15, 4);
            }
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
    parts.emplace_back(new TCodeTabEncoder());
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
