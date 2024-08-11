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

#include <config.h>

#include <memory>
#include <vector>

static_assert(sizeof(TFloat) == sizeof(float), "TFloat must be float32");
namespace NAtracDEnc {

struct TAt3PGhaData {
    struct TWaveParam {
        uint32_t FreqIndex;
        uint32_t AmpSf;
        uint32_t AmpIndex;
        uint32_t PhaseIndex;
    };
    struct TWaveSbInfo {
        size_t WaveIndex;
        size_t WaveNums;
    };
    struct TWavesChannel {
        std::vector<TWaveSbInfo> WaveSbInfos;
        std::vector<TWaveParam> WaveParams;
    };
    std::array<TWavesChannel, 2> Waves;

    uint8_t NumToneBands;
    bool SecondChBands[16];

    size_t GetNumWaves(size_t ch, size_t sb) const {
        return Waves[ch].WaveSbInfos.at(sb).WaveNums;
    }

    std::pair<const TWaveParam*, size_t> GetWaves(size_t ch, size_t sb) const {
        const auto& sbInfo = Waves[ch].WaveSbInfos.at(sb);
        return { &Waves[ch].WaveParams[sbInfo.WaveIndex], sbInfo.WaveNums };
    }
};

class IGhaProcessor {
public:
    virtual ~IGhaProcessor() {}
    virtual const TAt3PGhaData* DoAnalize() = 0;
};

std::unique_ptr<IGhaProcessor> MakeGhaProcessor0(float* b1, float* b2);

} // namespace NAtracDEnc

