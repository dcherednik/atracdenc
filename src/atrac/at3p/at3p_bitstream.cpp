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

#include "at3p_bitstream.h"
#include "at3p_gha.h"
#include "at3p_tables.h"
#include <lib/bitstream/bitstream.h>
#include <env.h>
#include <util.h>

#include <iostream>

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

TAt3PBitStream::TAt3PBitStream(ICompressedOutput* container, uint16_t frameSz)
    : Container(container)
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

            //TODO: Add actual Envelope
            // start point present
            bs.Write(0, 1);
            // stop point present
            bs.Write(0, 1);
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

void TAt3PBitStream::WriteFrame(int channels, const TAt3PGhaData* tonalBlock)
{
    NBitStream::TBitStream bitStream;
    // First bit must be zero
    bitStream.Write(0, 1);
    // Channel block type
    // 0 - MONO block
    // 1 - STEREO block
    // 2 - Nobody know
    bitStream.Write(channels - 1, 2);

    int num_quant_units =  14;
    bitStream.Write(num_quant_units - 1, 5);

    for (int ch = 0; ch < channels; ch++) {
        bitStream.Write(0, 2); // channel wordlen mode
        for (int j = 0; j < num_quant_units; j++) {
            bitStream.Write(0, 3); // wordlen
        }
    }

    // Skip some bits to produce correct zero bitstream

    if (channels == 2) {
        bitStream.Write(0, 7);
    } else {
        bitStream.Write(0, 3);
    }

    // Bit indicate tonal block is used
    bitStream.Write((bool)tonalBlock, 1);
    if (tonalBlock) {
        WriteTonalBlock(bitStream, channels, tonalBlock);
    }

    bitStream.Write(0, 1); // no noise info

    // Terminator
    bitStream.Write(3, 2);

    std::vector<char> buf = bitStream.GetBytes();

    buf.resize(FrameSz);
    Container->WriteFrame(buf);
}

}
