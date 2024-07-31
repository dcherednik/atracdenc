#pragma once

#include <config.h>
#include <libgha/include/libgha.h>

#include <vector>

static_assert(sizeof(TFloat) == sizeof(float), "TFloat must be float32");
namespace NAtracDEnc {

struct TAt3PGhaData {
    struct TWaveParam {
        uint32_t FreqIndex;
        uint32_t AmpSf;
        uint32_t AmpIndex;
        uint32_t PhaseIndex;
        struct gha_info orig_gha_info;
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
    uint8_t SecondChBands[16];

    size_t GetNumWaves(size_t ch, size_t sb) const {
        return Waves[ch].WaveSbInfos.at(sb).WaveNums;
    }

    std::pair<const TWaveParam*, size_t> GetWaves(size_t ch, size_t sb) const {
        const auto& sbInfo = Waves[ch].WaveSbInfos.at(sb);
        return { &Waves[ch].WaveParams[sbInfo.WaveIndex], sbInfo.WaveNums };
    }
};

} // namespace NAtracDEnc

