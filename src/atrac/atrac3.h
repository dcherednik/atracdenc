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

#pragma once
#include "../config.h"
#include <math.h>
#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>

namespace NAtracDEnc {
namespace NAtrac3 {

struct TContainerParams {
    const uint32_t Bitrate;
    const uint16_t FrameSz;
    const bool Js;
};

inline bool operator< (const TContainerParams& x, const TContainerParams& y)
{
    return x.Bitrate < y.Bitrate;
}
inline bool operator> (const TContainerParams& x, const TContainerParams& y)
{
    return x.Bitrate > y.Bitrate;
}
inline bool operator< (const TContainerParams& x, const unsigned int y)
{
    return x.Bitrate < y;
}
inline bool operator> (const TContainerParams& x, const unsigned int y)
{
    return x.Bitrate > y;
}

class TAtrac3Data {
public:
    class TBlockSizeMod {
    public:
        constexpr bool ShortWin(uint8_t) const noexcept {
            return false;
        }
    };

    static constexpr uint8_t MaxBfus = 32;
    static constexpr uint32_t NumSamples = 1024;

    static const uint32_t MDCTSz = 512;
    static float ScaleTable[64];
    static float EncodeWindow[256];
    static float DecodeWindow[256];
    static float GainLevel[16];
    static float GainInterpolation[31];
    static constexpr int32_t ExponentOffset = 4;
    static constexpr int32_t LocScale = 3;
    static constexpr int32_t LocSz = 1 << LocScale;
    static constexpr int32_t GainInterpolationPosShift = 15;

    static constexpr uint32_t NumSpecs = NumSamples;
    static const uint32_t frameSz = 152;
    static constexpr float MaxQuant[8] = {
        0.0,    1.5,    2.5,    3.5,
        4.5,    7.5,    15.5,   31.5
    };
    static constexpr uint32_t BlockSizeTab[33] = {
        0,    8,    16,    24,    32,    40,    48,    56,
        64,   80,   96,    112,   128,   144,   160,   176,
        192,  224,  256,   288,   320,   352,   384,   416,
        448,  480,  512,   576,   640,   704,   768,   896,
        1024
    };
    static constexpr uint32_t const * const SpecsStartShort = &BlockSizeTab[0];

    static constexpr uint32_t const * const SpecsStartLong = &BlockSizeTab[0];
    static constexpr uint32_t ClcLengthTab[8] = { 0, 4, 3, 3, 4, 4, 5, 6 };
    static constexpr int NumQMF = 4;
    static constexpr uint32_t MaxSpecs = NumSamples; //1024
    static constexpr uint32_t MaxSpecsPerBlock = 128;

    static constexpr uint32_t BlocksPerBand[NumQMF + 1] = {0, 18, 26, 30, 32};
    static constexpr uint32_t SpecsPerBlock[33] = {
        8,    8,    8,     8,     8,    8,    8,  8,  
        16,   16,   16,    16,    16,   16,   16, 16, 
        32,   32,   32,    32,    32,   32,   32, 32,
        32,   32,   64,    64,    64,   64,   128, 128,
        128
    };
    struct THuffEntry {
        const uint8_t Code;
        const uint8_t Bits;
    };
    static constexpr uint8_t HuffTable1Sz = 9;
    static constexpr THuffEntry HuffTable1[HuffTable1Sz] = {
        { 0x0, 1 },
        { 0x4, 3 }, { 0x5, 3 },
        { 0xC, 4 }, { 0xD, 4 }, 
        { 0x1C, 5 }, { 0x1D, 5 }, { 0x1E, 5 }, { 0x1F, 5 }
    };
    static constexpr uint8_t HuffTable2Sz = 5;
    static constexpr THuffEntry HuffTable2[HuffTable2Sz] = {
        { 0x0, 1 },
        { 0x4, 3 }, { 0x5, 3 }, { 0x6, 3 }, { 0x7, 3 }
    };
    static constexpr uint8_t HuffTable3Sz = 7;
    static constexpr THuffEntry HuffTable3[HuffTable3Sz] = {
        { 0x0, 1 },
        { 0x4, 3}, { 0x5, 3 },
        { 0xC, 4 }, { 0xD, 4 }, { 0xE, 4 }, { 0xF, 4 }
    };
    static constexpr uint8_t HuffTable5Sz = 15;
    static constexpr THuffEntry HuffTable5[HuffTable5Sz] = {
        { 0x0, 2 },
        { 0x2, 3 }, { 0x3, 3 },
        { 0x8, 4 }, { 0x9, 4 }, { 0xA, 4 }, { 0xB, 4 }, //{ 0xC, 4 }, { 0xD, 4 },
        { 0x1C, 5 }, { 0x1D, 5 },
        { 0x3C, 6 }, { 0x3D, 6 }, { 0x3E, 6 }, { 0x3F, 6},
        { 0xC, 4 }, { 0xD, 4 } //TODO: is it right table???
    };
    static constexpr uint8_t HuffTable6Sz = 31;
    static constexpr THuffEntry HuffTable6[HuffTable6Sz] = {
        { 0x0, 3 },
        { 0x2, 4 }, { 0x3, 4 }, { 0x4, 4 }, { 0x5, 4 }, { 0x6, 4 }, { 0x7, 4 }, //{ 0x8, 4 }, { 0x9, 4 },
        { 0x14, 5 }, { 0x15, 5 }, { 0x16, 5 }, { 0x17, 5 }, { 0x18, 5 }, { 0x19, 5 },
        { 0x34, 6 }, { 0x35, 6 }, { 0x36, 6 }, { 0x37, 6 }, { 0x38, 6 }, { 0x39, 6 }, { 0x3A, 6 }, { 0x3B, 6 },
        { 0x78, 7 }, { 0x79, 7 }, { 0x7A, 7 }, { 0x7B, 7 }, { 0x7C, 7 }, { 0x7D, 7 }, { 0x7E, 7 }, { 0x7F, 7 },
        { 0x8, 4 }, { 0x9, 4 } //TODO: is it right table???
    };
    static constexpr uint8_t HuffTable7Sz = 63;
    static constexpr THuffEntry HuffTable7[HuffTable7Sz] = {
        { 0x0, 3 },
        //{ 0x2, 4 }, { 0x3, 4 },
        { 0x8, 5 }, { 0x9, 5 }, { 0xA, 5}, { 0xB, 5 }, { 0xC, 5 }, { 0xD, 5 }, { 0xE, 5}, { 0xF, 5 }, { 0x10, 5 },
                                                                                                            { 0x11, 5 },
        { 0x24, 6 }, { 0x25, 6 }, { 0x26, 6 }, { 0x27, 6 }, { 0x28, 6 }, { 0x29, 6 }, { 0x2A, 6 }, { 0x2B, 6 },
        { 0x2C, 6 }, { 0x2D, 6 }, { 0x2E, 6 }, { 0x2F, 6 }, { 0x30, 6 }, { 0x31, 6 }, { 0x32, 6 }, { 0x33, 6 },
        { 0x68, 7 }, { 0x69, 7 }, { 0x6A, 7 }, { 0x6B, 7 }, { 0x6C, 7 }, { 0x6D, 7 }, { 0x6E, 7 },
        { 0x6F, 7 }, { 0x70, 7 }, { 0x71, 7 }, { 0x72, 7 }, { 0x73, 7 }, { 0x74, 7 }, { 0x75, 7 },
        { 0xEC, 8 }, { 0xED, 8 }, { 0xEE, 8 }, { 0xEF, 8 }, { 0xF0, 8 }, { 0xF1, 8 }, { 0xF2, 8 }, { 0xF3, 8 },
                                                                                               { 0xF4, 8 }, { 0xF5, 8 },
        { 0xF6, 8 }, { 0xF7, 8 }, { 0xF8, 8 }, { 0xF9, 8 }, { 0xFA, 8 }, { 0xFB, 8 }, { 0xFC, 8 }, { 0xFD, 8 },
                                                                                               { 0xFE, 8 }, { 0xFF, 8 },
        { 0x2, 4 }, { 0x3, 4 } //TODO: is it right table???
    };

    struct THuffTablePair {
        const THuffEntry* Table;
        const uint32_t Sz;
    };

    static constexpr THuffTablePair HuffTables[7] {
        { HuffTable1, HuffTable1Sz },
        { HuffTable2, HuffTable2Sz },
        { HuffTable3, HuffTable3Sz },
        { HuffTable1, HuffTable1Sz },
        { HuffTable5, HuffTable5Sz },
        { HuffTable6, HuffTable6Sz },
        { HuffTable7, HuffTable7Sz }
    };
public:
    TAtrac3Data() {
        if (ScaleTable[0] == 0) {
            for (uint32_t i = 0; i < 64; i++) {
                ScaleTable[i] = pow(2.0, (double)(i / 3.0 - 21.0));
            }
        }
        for (int i = 0; i < 256; i++) {
            EncodeWindow[i] = (sin(((i + 0.5) / 256.0 - 0.5) * M_PI) + 1.0)/* * 0.5*/;
        }
        for (int i = 0; i < 256; i++) {
            const double a = EncodeWindow[i];
            const double b = EncodeWindow[255-i];
            DecodeWindow[i] = 2.0 * a / (a*a + b*b);
        }
        for (int i = 0; i < 16; i++) {
            GainLevel[i] = pow(2.0, ExponentOffset - i);
        }
        for (int i = 0; i < 31; i++) {
            GainInterpolation[i] = pow(2.0, -1.0 / LocSz * (i - 15));
        }
    }
    static uint32_t MantissaToCLcIdx(int32_t mantissa) {
        assert(mantissa > -3 && mantissa < 2);
        const uint32_t mantissa_clc_rtab[4] = { 2, 3, 0, 1};
        return mantissa_clc_rtab[mantissa + 2];
    }
    static uint32_t MantissasToVlcIndex(int32_t a, int32_t b) {
        assert(a > -2 && a < 2); 
        assert(b > -2 && b < 2); 
        const uint32_t mantissas_vlc_rtab[9] = { 8, 4, 7, 2, 0, 1, 6, 3, 5 };
        const uint8_t idx = 3 * (a + 1) + (b + 1);
        return mantissas_vlc_rtab[idx];
    }
    static constexpr TContainerParams ContainerParams[8] = {
        { 66150, 192, true },
        { 93713, 272, true },
        { 104738, 304, false },
        { 132300, 384, false },
        { 146081, 424, false },
        { 176400, 512, false },
        { 264600, 768, false },
        { 352800, 1024, false }
    };
    static const TContainerParams* GetContainerParamsForBitrate(uint32_t bitrate);

    struct SubbandInfo {
        static const uint32_t MaxGainPointsNum = 8;
        struct TGainPoint {
            uint32_t Level;
            uint32_t Location;
        };
        std::vector<std::vector<TGainPoint>> Info;
        SubbandInfo()
        {
            Info.resize(4);
        }
        void AddSubbandCurve(uint16_t n, std::vector<TGainPoint>&& curve) {
            Info[n] = std::move(curve);
        }
        uint32_t GetQmfNum() const {
            return Info.size();
        }
        const std::vector<TGainPoint>& GetGainPoints(uint32_t i) const {
            return Info[i];
        }
        void Reset()
        {
            for (auto& v : Info) {
                v.clear();
            }
        }
    };

    struct TTonalVal {
        const uint16_t Pos;
        const double Val;
        const uint8_t Bfu;
    };
    typedef std::vector<TTonalVal> TTonalComponents;
};


struct TAtrac3EncoderSettings {
    TAtrac3EncoderSettings(uint32_t bitrate, bool noGainControll,
                           bool noTonalComponents, uint8_t sourceChannels, uint32_t bfuIdxConst)
        : ConteinerParams(TAtrac3Data::GetContainerParamsForBitrate(bitrate))
        , NoGainControll(noGainControll)
        , NoTonalComponents(noTonalComponents)
        , SourceChannels(sourceChannels)
        , BfuIdxConst(bfuIdxConst)
    { }
    const TContainerParams* ConteinerParams;
    const bool NoGainControll;
    const bool NoTonalComponents;
    const uint8_t SourceChannels;
    const uint32_t BfuIdxConst;
};

} // namespace NAtrac3
} // namespace NAtracDEnc
