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

#include <array>
#include <memory>
#include <vector>

namespace NAtracDEnc {

struct TAt3PGhaData {
    static constexpr uint32_t EMPTY_POINT = static_cast<uint32_t>(-1);
    static constexpr uint32_t INIT = static_cast<uint32_t>(-2);
    struct TWaveParam {
        uint32_t FreqIndex;
        uint32_t AmpSf;
        uint32_t AmpIndex;
        uint32_t PhaseIndex;
    };
    struct TWaveSbInfo {
        size_t WaveIndex;
        size_t WaveNums;
        std::pair<uint32_t, uint32_t> Envelope = {EMPTY_POINT, EMPTY_POINT};
    };
    struct TWavesChannel {
        std::vector<TWaveSbInfo> WaveSbInfos;
        std::vector<TWaveParam> WaveParams;
    };
    std::array<TWavesChannel, 2> Waves;

    uint8_t NumToneBands;

    bool ToneSharing[16];
    bool SecondIsLeader;

    size_t GetNumWaves(size_t ch, size_t sb) const {
        return Waves[ch].WaveSbInfos.at(sb).WaveNums;
    }

    std::pair<uint32_t, uint32_t> GetEnvelope(size_t ch, size_t sb) const {
        return Waves[ch].WaveSbInfos.at(sb).Envelope;
    }

    std::pair<const TWaveParam*, size_t> GetWaves(size_t ch, size_t sb) const {
        const auto& sbInfo = Waves[ch].WaveSbInfos.at(sb);
        return { &Waves[ch].WaveParams[sbInfo.WaveIndex], sbInfo.WaveNums };
    }
};

class IGhaProcessor {
public:
    using TBufPtr = std::array<const float*, 2>;
    virtual ~IGhaProcessor() {}
    virtual const TAt3PGhaData* DoAnalize(TBufPtr b1, TBufPtr b2, float* w1, float* w2) = 0;
};

std::unique_ptr<IGhaProcessor> MakeGhaProcessor0(bool stereo);

} // namespace NAtracDEnc

