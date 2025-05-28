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

#include "at3p_gha.h"
#include "ff/atrac3plus.h"

#include <assert.h>
#include <atrac/atrac_psy_common.h>
#include <libgha/include/libgha.h>

#include <memory>

#include <algorithm>
#include <cstring>
#include <cmath>
#include <iostream>
#include <map>
#include <vector>

using std::map;
using std::vector;
using std::isnan;
using std::pair;
using std::max;
using std::min;

namespace NAtracDEnc {

namespace {

uint32_t GhaFreqToIndex(float f, uint32_t sb)
{
    return static_cast<uint32_t>(lrintf(1024.0f * (f / M_PI)) & 1023) | (sb << 10);
}

uint32_t GhaPhaseToIndex(float p)
{
    return static_cast<uint32_t>(lrintf(32.0 * ((p) / (2 * M_PI))) & 31);
}

uint32_t PhaseIndexToOffset(uint32_t ind)
{
    return (ind & 0x1F) << 6;
}

class TGhaProcessor : public IGhaProcessor {
    // Number of subbands to process;
    // No need to process all subbands.
    static constexpr size_t SUBBANDS = 8;
    static constexpr size_t SAMPLES_PER_SUBBAND = 128;
    static constexpr size_t LOOK_AHEAD = 64;
    static constexpr size_t GHA_SUBBAND_BUF_SZ = SAMPLES_PER_SUBBAND + LOOK_AHEAD;
    static constexpr size_t CHANNEL_BUF_SZ = SUBBANDS * GHA_SUBBAND_BUF_SZ;

    using TGhaInfoMap = map<uint32_t, struct gha_info>;
    using TWavesChannel = TAt3PGhaData::TWavesChannel;
    using TAmpSfTab = std::array<float, 64>;

    struct TChannelData {
        const float* SrcBuf;
        const float* SrcBufNext;
        float Buf[CHANNEL_BUF_SZ];
        pair<uint32_t, uint32_t> Envelopes[SUBBANDS] = {{TAt3PGhaData::INIT,TAt3PGhaData::INIT}};
        bool Gapless[SUBBANDS] = {false};
        uint8_t SubbandDone[SUBBANDS] = {0};
        TGhaInfoMap GhaInfos;
        float MaxToneMagnitude[SUBBANDS] = {0}; // Max magnitude of sine in the band. Used to stop processing when next extracted sine become significant less then max one
        float LastResuidalEnergy[SUBBANDS] = {0}; // Resuidal energy on the last round for subband. It is the second criteria to stop processing, we expect resuidal becaming less on the each round
        uint16_t LastAddedFreqIdx[SUBBANDS];

        void MarkSubbandDone(size_t sb) {
            SubbandDone[sb] = 16;
        }

        bool IsSubbandDone(size_t sb) const {
            return SubbandDone[sb] == 16;
        }
    };

    struct TChannelGhaCbCtx {
        TChannelGhaCbCtx(TChannelData* data, size_t sb)
            : Data(data)
            , Sb(sb)
        {}
        TChannelData* Data;
        size_t Sb;

        enum class EAdjustStatus {
            Error,
            Ok,
            Repeat
        } AdjustStatus;
        size_t FrameSz = 0;
    };

public:
    TGhaProcessor(bool stereo)
        : LibGhaCtx(gha_create_ctx(128))
        , Stereo(stereo)
    {
        gha_set_max_magnitude(LibGhaCtx, 32768);
        gha_set_upsample(LibGhaCtx, 1);

        if (!StaticInited) {
            ff_atrac3p_init_dsp_static();

            FillSubbandAth(&SubbandAth[0]);

            AmpSfTab = CreateAmpSfTab();
            for (int i = 0; i < 2048; i++) {
                SineTab[i] = sin(2 * M_PI * i / 2048);
            }

            StaticInited = true;
        }

        for (size_t ch = 0; ch < 2; ch++) {
           ChUnit.channels[ch].tones_info      = &ChUnit.channels[ch].tones_info_hist[0][0];
           ChUnit.channels[ch].tones_info_prev = &ChUnit.channels[ch].tones_info_hist[1][0];
        }

        ChUnit.waves_info      = &ChUnit.wave_synth_hist[0];
        ChUnit.waves_info_prev = &ChUnit.wave_synth_hist[1];
        ChUnit.waves_info->tones_present = false;
        ChUnit.waves_info_prev->tones_present = false;
    }

    ~TGhaProcessor()
    {
        gha_free_ctx(LibGhaCtx);
    }

    const TAt3PGhaData* DoAnalize(TBufPtr b1, TBufPtr b2, float *w1, float *w2) override;

private:
    void ApplyFilter(const TAt3PGhaData*, float *b1, float *b2);
    static void FillSubbandAth(float* out);
    static TAmpSfTab CreateAmpSfTab();
    static void CheckResuidalAndApply(float* resuidal, size_t size, void* self) noexcept;
    static void GenWaves(const TAt3PGhaData::TWaveParam* param, size_t numWaves, size_t reg_offset, float* out, size_t outLimit);

    void AdjustEnvelope(pair<uint32_t, uint32_t>& envelope, const pair<uint32_t, uint32_t>& src, uint32_t history);
    uint32_t FillFolowerRes(const TGhaInfoMap& lGha, const TChannelData* src, TGhaInfoMap::const_iterator& it, uint32_t leaderSb);

    uint32_t AmplitudeToSf(float amp) const;
    bool CheckNextFrame(const float* nextSrc, const vector<gha_info>& ghaInfos) const;

    bool DoRound(TChannelData& data, size_t& totalTones) const;
    bool PsyPreCheck(size_t sb, const struct gha_info& gha, const TChannelData& data) const;
    void FillResultBuf(const vector<TChannelData>& data);

    gha_ctx_t LibGhaCtx;
    TAt3PGhaData ResultBuf;
    TAt3PGhaData ResultBufHistory;

    const bool Stereo;

    static float SubbandAth[SUBBANDS];
    static float SineTab[2048];
    static bool StaticInited;
    static TAmpSfTab AmpSfTab;

    Atrac3pChanUnitCtx ChUnit;
};

bool TGhaProcessor::StaticInited = false;
float TGhaProcessor::SubbandAth[SUBBANDS];
float TGhaProcessor::SineTab[2048];
TGhaProcessor::TAmpSfTab TGhaProcessor::AmpSfTab;

void TGhaProcessor::FillSubbandAth(float* out)
{
    const auto ath = CalcATH(16 * 1024, 44100);
    #pragma GCC nounroll
    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        float m = 999.;
        for (size_t f = sb * 1024, i = 0; i < 1024; f++, i++) {
            m = fmin(m, ath[f]);
        }
        //m += 26; //Some gap to not encode too much
        out[sb] = pow(10, 0.1 * (m + 90)); //adjust to 0db level = 32768, convert to power
    }
}

TGhaProcessor::TAmpSfTab TGhaProcessor::CreateAmpSfTab()
{
    TAmpSfTab AmpSfTab;
    for (int i = 0; i < (int)AmpSfTab.size(); i++) {
        AmpSfTab[i] = exp2f((i - 3) / 4.0f);
    }
    return AmpSfTab;
}

void TGhaProcessor::GenWaves(const TAt3PGhaData::TWaveParam* param, size_t numWaves, size_t reg_offset, float* out, size_t outLimit)
{
    for (size_t w = 0; w < numWaves; w++, param++) {
        //std::cerr << "GenWaves : " << w << "  FreqIndex: " <<  param->FreqIndex << " phaseIndex: " << param->PhaseIndex << " ampSf " << param->AmpSf << std::endl;
        auto amp = AmpSfTab[param->AmpSf];
        auto inc = param->FreqIndex;
        auto pos = ((int)PhaseIndexToOffset(param->PhaseIndex) + ((int)reg_offset ^ 128) * inc) & 2047;

        for (size_t i = 0; i < outLimit; i++) {
            //std::cerr << "inc: " << inc << " pos: " << pos << std::endl;
            out[i] += SineTab[pos] * amp;
            pos     = (pos + inc) & 2047;
        }
    }
}

void TGhaProcessor::CheckResuidalAndApply(float* resuidal, size_t size, void* d) noexcept
{
    TChannelGhaCbCtx* ctx = (TChannelGhaCbCtx*)(d);
    const float* srcBuf = ctx->Data->SrcBuf + (ctx->Sb * SAMPLES_PER_SUBBAND);
    //std::cerr << "TGhaProcessor::CheckResuidal " << srcBuf[0] << " " << srcBuf[1] << " " << srcBuf[2] << " " << srcBuf[3] <<  std::endl;

    float resuidalEnergy = 0;

    uint32_t start = 0;
    uint32_t curStart = 0;
    uint32_t count = 0;
    uint32_t len = 0;
    bool found = false;

    if (size != SAMPLES_PER_SUBBAND)
        abort();

    for (size_t i = 0; i < SAMPLES_PER_SUBBAND; i += 4) {
        float energyIn = 0.0;
        float energyOut = 0.0;
        for (size_t j = 0; j < 4; j++) {
            energyIn += srcBuf[i + j] * srcBuf[i + j];
            energyOut += resuidal[i + j] * resuidal[i + j];
        }

        energyIn = sqrt(energyIn/4);
        energyOut = sqrt(energyOut/4);
        resuidalEnergy += energyOut;

        if (energyIn / energyOut < 1) {
            count = 0;
            found = false;
            curStart = i + 4;
        } else {
            count++;
            if (count > len) {
                len = count;
                if (!found) {
                    start = curStart;
                    found = true;
                }
            }
        }

        // std::cerr << " " << i << " rms : " << energyIn << " " << energyOut  << "\t\t\t" << ((energyOut < energyIn) ? "+" : "-")  << std::endl;
    }

    const auto sb = ctx->Sb;
    // Do not encode too short frame
    if (len < 4) {
        ctx->AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Error;
        return;
    }

    const uint32_t end = start + len * 4;

    if (ctx->AdjustStatus != TChannelGhaCbCtx::EAdjustStatus::Repeat && end != SAMPLES_PER_SUBBAND) {
        ctx->FrameSz = end;
        ctx->AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Repeat;
        return;
    }

    const float threshold = 1.05; //TODO: tune it
    if (static_cast<bool>(ctx->Data->LastResuidalEnergy[sb]) == false) {
        ctx->Data->LastResuidalEnergy[sb] = resuidalEnergy;
    } else if (ctx->Data->LastResuidalEnergy[sb] < resuidalEnergy * threshold) {
        ctx->AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Error;
        return;
    } else {
        ctx->Data->LastResuidalEnergy[sb] = resuidalEnergy;
    }

    auto& envelope = ctx->Data->Envelopes[sb];
    envelope.first = start;

    if (envelope.second == TAt3PGhaData::EMPTY_POINT && end != SAMPLES_PER_SUBBAND) {
        ctx->AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Error;
        return;
    }

    envelope.second = end;

    ctx->AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Ok;

    float* b = &ctx->Data->Buf[sb * GHA_SUBBAND_BUF_SZ];

    memcpy(b, resuidal, sizeof(float) * SAMPLES_PER_SUBBAND);
}

void TGhaProcessor::ApplyFilter(const TAt3PGhaData* d, float* b1, float* b2)
{
    for (size_t ch_num = 0; ch_num < 2; ch_num++)
        memset(ChUnit.channels[ch_num].tones_info, 0,
           sizeof(*ChUnit.channels[ch_num].tones_info) * ATRAC3P_SUBBANDS);

    if (d) {
        memset(ChUnit.waves_info->waves, 0, sizeof(ChUnit.waves_info->waves));
        ChUnit.waves_info->num_tone_bands = d->NumToneBands;
        ChUnit.waves_info->tones_present = true;
        ChUnit.waves_info->amplitude_mode = 1;
        ChUnit.waves_info->tones_index = 0;
    } else {
        ChUnit.waves_info->tones_present = false;
    }

    if (d) {
        for (size_t ch = 0; ch <= (size_t)Stereo; ch++) {
            for (int i = 0; i < ChUnit.waves_info->num_tone_bands; i++) {
                if (ch && d->ToneSharing[i]) {
                    continue;
                }

                ChUnit.channels[ch].tones_info[i].num_wavs = d->GetNumWaves(ch, i);

                const auto envelope = d->GetEnvelope(ch, i);
                if (envelope.first != TAt3PGhaData::EMPTY_POINT) {
                    // start point present
                    ChUnit.channels[ch].tones_info[i].pend_env.has_start_point = true;
                    ChUnit.channels[ch].tones_info[i].pend_env.start_pos = envelope.first;
                } else {
                    ChUnit.channels[ch].tones_info[i].pend_env.has_start_point = false;
                    ChUnit.channels[ch].tones_info[i].pend_env.start_pos = -1;
                }

                if (envelope.second != TAt3PGhaData::EMPTY_POINT) {
                    // stop point present
                    ChUnit.channels[ch].tones_info[i].pend_env.has_stop_point  = true;
                    ChUnit.channels[ch].tones_info[i].pend_env.stop_pos = envelope.second;
                } else {
                    ChUnit.channels[ch].tones_info[i].pend_env.has_stop_point  = false;
                    ChUnit.channels[ch].tones_info[i].pend_env.stop_pos = 32;
                }
            }

            for (int sb = 0; sb < ChUnit.waves_info->num_tone_bands; sb++) {
                if (d->GetNumWaves(ch, sb)) {
                    if (ChUnit.waves_info->tones_index + ChUnit.channels[ch].tones_info[sb].num_wavs > 48) {
                        std::cerr << "too many tones: " << ChUnit.waves_info->tones_index + ChUnit.channels[ch].tones_info[sb].num_wavs << std::endl;
                        abort();
                    }
                    ChUnit.channels[ch].tones_info[sb].start_index           = ChUnit.waves_info->tones_index;
                    ChUnit.waves_info->tones_index += ChUnit.channels[ch].tones_info[sb].num_wavs;
                }
            }

            Atrac3pWaveParam *iwav;
            for (int sb = 0; sb < ChUnit.waves_info->num_tone_bands; sb++) {
                if (d->GetNumWaves(ch, sb)) {
                    iwav = &ChUnit.waves_info->waves[ChUnit.channels[ch].tones_info[sb].start_index];
                    auto w = d->GetWaves(ch, sb);
                    ChUnit.channels[ch].tones_info[sb].num_wavs = w.second;
                    for (size_t j = 0; j < w.second; j++) {
                        iwav[j].freq_index = w.first[j].FreqIndex;
                        iwav[j].amp_index = w.first[j].AmpIndex;
                        iwav[j].amp_sf = w.first[j].AmpSf;
                        iwav[j].phase_index = w.first[j].PhaseIndex;
                    }
                }
            }
        }

        if (Stereo) {
            for (int i = 0; i < ChUnit.waves_info->num_tone_bands; i++) {
                if (d->ToneSharing[i]) {
                    ChUnit.channels[1].tones_info[i] = ChUnit.channels[0].tones_info[i];
                }

                if (d->SecondIsLeader) {
                    std::swap(ChUnit.channels[0].tones_info[i],
                        ChUnit.channels[1].tones_info[i]);
                }
            }
        }
    }

    for (size_t ch = 0; ch <= (size_t)Stereo; ch++) {
        float* x = (ch == 0) ? b1 : b2;
        if (ChUnit.waves_info->tones_present ||
            ChUnit.waves_info_prev->tones_present) {
            for (size_t sb = 0; sb < SUBBANDS; sb++) {
                if (ChUnit.channels[ch].tones_info[sb].num_wavs ||
                    ChUnit.channels[ch].tones_info_prev[sb].num_wavs) {

                    ff_atrac3p_generate_tones(&ChUnit, ch, sb,
                        (x + sb * 128));
                }
            }
        }
    }

    for (size_t ch = 0; ch <= (size_t)Stereo; ch++) {
        std::swap(ChUnit.channels[ch].tones_info, ChUnit.channels[ch].tones_info_prev);
    }

    std::swap(ChUnit.waves_info, ChUnit.waves_info_prev);
}

const TAt3PGhaData* TGhaProcessor::DoAnalize(TBufPtr b1, TBufPtr b2, float* w1, float* w2)
{
    vector<TChannelData> data((size_t)Stereo + 1);
    bool progress[2] = {false};

    for (size_t ch = 0; ch < data.size(); ch++) {
        const float* bCur = (ch == 0) ? b1[0] : b2[0];
        const float* bNext = (ch == 0) ? b1[1] : b2[1];
        data[ch].SrcBuf = bCur;
        data[ch].SrcBufNext = bNext;

        for (size_t sb = 0; sb < SUBBANDS; sb++, bCur += SAMPLES_PER_SUBBAND, bNext += SAMPLES_PER_SUBBAND) {
            constexpr auto copyCurSz = sizeof(float) * SAMPLES_PER_SUBBAND;
            constexpr auto copyNextSz = sizeof(float) * LOOK_AHEAD;
            memcpy(&data[ch].Buf[0] + sb * GHA_SUBBAND_BUF_SZ                      , bCur, copyCurSz);
            memcpy(&data[ch].Buf[0] + sb * GHA_SUBBAND_BUF_SZ + SAMPLES_PER_SUBBAND, bNext, copyNextSz);
        }
        //for (int i = 0; i < SAMPLES_PER_SUBBAND + LOOK_AHEAD; i++) {
            //std::cerr << i << " " << data[0].Buf[i] << std::endl;
        //}
    }

    size_t totalTones = 0;
    do {
        for (size_t ch = 0; ch < data.size(); ch++) {
            progress[ch] = DoRound(data[ch], totalTones);
        }
    } while ((progress[0] || progress[1]) && totalTones < 48);

    if (totalTones == 0) {
        ApplyFilter(nullptr, w1, w2);
        return nullptr;
    }

    FillResultBuf(data);

    ResultBufHistory = ResultBuf;

    ApplyFilter(&ResultBuf, w1, w2);

    return  &ResultBuf;
}

bool TGhaProcessor::CheckNextFrame(const float* nextSrc, const vector<gha_info>& ghaInfos) const
{
    vector<TAt3PGhaData::TWaveParam> t;
    t.reserve(ghaInfos.size());
    for (const auto& x : ghaInfos) {
        t.emplace_back(TAt3PGhaData::TWaveParam
            {
                // TODO: do not do it twice
                GhaFreqToIndex(x.frequency, 0),
                AmplitudeToSf(x.magnitude),
                1,
                GhaPhaseToIndex(x.phase)
            }
        );
    }

    float buf[LOOK_AHEAD] = {0.0};

    GenWaves(t.data(), t.size(), 0, buf, LOOK_AHEAD);

    float energyBefore = 0.0;
    float energyAfter = 0.0;

    for (size_t i = 0; i < LOOK_AHEAD; i++) {
        energyBefore += nextSrc[i] * nextSrc[i];
        float t = nextSrc[i] - buf[i];
        energyAfter += t * t;
        //std::cerr << buf[i] << " === " << nextSrc[i] << std::endl;
    }

    // std::cerr << "ENERGY: before: " << energyBefore << " after: " << energyAfter << std::endl;

    return energyAfter < energyBefore;
}

bool TGhaProcessor::DoRound(TChannelData& data, size_t& totalTones) const
{
    bool progress = false;
    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        if (data.IsSubbandDone(sb)) {
            continue;
        }

        if (totalTones >= 48) {
            return false;
        }

        const float* srcB = data.SrcBuf + (sb * SAMPLES_PER_SUBBAND);
        {
            auto cit = data.GhaInfos.lower_bound(sb << 10);
            vector<gha_info> tmp;
            for(auto it = cit; it != data.GhaInfos.end() && it->first < (sb + 1) << 10; it++) {
                // std::cerr << sb << " before: freq: " << it->second.frequency << " magn: " << it->second.magnitude << std::endl;
                tmp.push_back(it->second);
            }
            if (tmp.size() > 0) {
                TChannelGhaCbCtx ctx(&data, sb);
                do {
                    int ar = gha_adjust_info(srcB, tmp.data(), tmp.size(), LibGhaCtx, CheckResuidalAndApply, &ctx, ctx.FrameSz);
                    if (ar < 0) {
                        ctx.AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Error;
                    };
                } while (ctx.AdjustStatus == TChannelGhaCbCtx::EAdjustStatus::Repeat);

                if (ctx.AdjustStatus == TChannelGhaCbCtx::EAdjustStatus::Ok) {
                    std::sort(tmp.begin(), tmp.end(), [](const gha_info& a, const gha_info& b) {return a.frequency < b.frequency;});

                    bool dupFound = false;
                    {
                        auto idx1 = GhaFreqToIndex(tmp[0].frequency, sb);
                        for (size_t i = 1; i < tmp.size(); i++) {
                            auto idx2 = GhaFreqToIndex(tmp[i].frequency, sb);
                            if (idx2 == idx1) {
                                dupFound = true;
                                break;
                            } else {
                                idx1 = idx2;
                            }
                        }
                    }

                    if (!dupFound) {
                        // check is this tone set ok for the next one
                        if (data.Envelopes[sb].second == SAMPLES_PER_SUBBAND || data.Envelopes[sb].second == TAt3PGhaData::EMPTY_POINT) {
                            bool cont = CheckNextFrame(data.SrcBufNext + SAMPLES_PER_SUBBAND * sb, tmp);

                            if (data.Gapless[sb] == true && cont == false) {
                                data.GhaInfos.erase(data.LastAddedFreqIdx[sb]);
                                totalTones--;
                                data.MarkSubbandDone(sb);
                                continue;
                            } else if (data.Envelopes[sb].second == SAMPLES_PER_SUBBAND && cont == true) {
                                data.Envelopes[sb].second = TAt3PGhaData::EMPTY_POINT;
                                data.Gapless[sb] = true;
                            }
                        }

                        auto it = cit;
                        for (size_t i = 0; i < tmp.size(); i++) {
                            // std::cerr << sb << " after: freq: " << tmp[i].frequency << " magn: " << tmp[i].magnitude << std::endl;
                            it = data.GhaInfos.erase(it);
                        }
                        for (const auto& x : tmp) {
                            data.MaxToneMagnitude[sb] = std::max(data.MaxToneMagnitude[sb], x.magnitude);
                            const auto newIndex = GhaFreqToIndex(x.frequency, sb);
                            data.GhaInfos.insert({newIndex, x});
                        }
                    } else {
                        std::cerr << "jackpot! same freq index after adjust call, sb: " << sb << " " << std::endl;
                        data.GhaInfos.erase(data.LastAddedFreqIdx[sb]);
                        totalTones--;
                        data.MarkSubbandDone(sb);
                        continue;
                    }
                } else {
                    data.GhaInfos.erase(data.LastAddedFreqIdx[sb]);
                    totalTones--;
                    data.MarkSubbandDone(sb);
                    continue;
                }
            }
        }

        float* b = &data.Buf[sb * GHA_SUBBAND_BUF_SZ];
        struct gha_info res;

        gha_analyze_one(b, &res, LibGhaCtx);

        auto freqIndex = GhaFreqToIndex(res.frequency, sb);
        if (PsyPreCheck(sb, res, data) == false) {
            data.MarkSubbandDone(sb);
        } else {
            if (data.SubbandDone[sb] == 0) {
                bool ins = data.GhaInfos.insert({freqIndex, res}).second;
                data.LastAddedFreqIdx[sb] = freqIndex;
                assert(ins);
            } else {
                const auto it = data.GhaInfos.lower_bound(freqIndex);
                const size_t minFreqDistanse = 20; // Now we unable to handle tones with close frequency
                if (it != data.GhaInfos.end()) {
                    if (it->first == freqIndex) {
                        data.MarkSubbandDone(sb);
                        continue;
                    }

                    if (it->first - freqIndex < minFreqDistanse) {
                        data.MarkSubbandDone(sb);
                        continue;
                    }
                }
                if (it != data.GhaInfos.begin()) {
                    auto prev = it;
                    prev--;
                    if (freqIndex - prev->first < minFreqDistanse) {
                        data.MarkSubbandDone(sb);
                        continue;
                    }
                }
                if (data.SubbandDone[sb] == 15) {
                    data.MarkSubbandDone(sb);
                    continue;
                }
                data.GhaInfos.insert(it, {freqIndex, res});
                data.LastAddedFreqIdx[sb] = freqIndex;
            }

            data.SubbandDone[sb]++;
            totalTones++;
            progress = true;
        }

    }
    return progress;
}

bool TGhaProcessor::PsyPreCheck(size_t sb, const struct gha_info& gha, const TChannelData& data) const
{
    if (isnan(gha.magnitude)) {
        return false;
    }

    //std::cerr << "sb: " << sb << " " << gha.magnitude << " ath: " << SubbandAth[sb] << " max: " << data.MaxToneMagnitude[sb] << std::endl;
    // TODO: improve it
    // Just to start. Actualy we need to consider spectral leakage during MDCT
    if ((gha.magnitude * gha.magnitude) > SubbandAth[sb]) {
        // Stop processing for sb if next extracted tone 23db less then maximal one
        // TODO: tune
        if (gha.magnitude > data.MaxToneMagnitude[sb] / 10) {
            return true;
        }
    }

    return false;
}

void TGhaProcessor::AdjustEnvelope(pair<uint32_t, uint32_t>& envelope, const pair<uint32_t, uint32_t>& src, uint32_t history)
{
    if (src.first == 0 && history == TAt3PGhaData::EMPTY_POINT) {
        envelope.first = TAt3PGhaData::EMPTY_POINT;
    } else {
        if (src.first == TAt3PGhaData::EMPTY_POINT) {
            abort(); //impossible right now
            envelope.first = TAt3PGhaData::EMPTY_POINT;
        } else {
            envelope.first = src.first / 4;
        }
    }
    if (src.second == TAt3PGhaData::EMPTY_POINT) {
        envelope.second = src.second;
    } else {
        if (src.second == 0)
            abort();
        envelope.second = (src.second - 1) / 4;
        if (envelope.second >= 32)
            abort();
    }
}

void TGhaProcessor::FillResultBuf(const vector<TChannelData>& data)
{
    uint32_t usedContiguousSb[2] = {0, 0};
    uint32_t numTones[2] = {0, 0};

    // TODO: This can be improved. Bitstream allows to set leader/folower flag for each band.
    for (size_t ch = 0; ch < data.size(); ch++) {
        int cur = -1;
        for (const auto& info : data[ch].GhaInfos) {
            int sb = info.first >> 10;
            if (sb == cur + 1) {
                usedContiguousSb[ch]++;
                numTones[ch]++;
                cur = sb;
            } else if (sb == cur) {
                numTones[ch]++;
                continue;
            } else {
                break;
            }
        }
    }

    bool leader = usedContiguousSb[1] > usedContiguousSb[0];

    std::vector<TWavesChannel> history;
    history.reserve(data.size());

    history.push_back(ResultBuf.Waves[0]);

    ResultBuf.SecondIsLeader = leader;
    ResultBuf.NumToneBands = usedContiguousSb[leader];

    TGhaInfoMap::const_iterator leaderStartIt;
    TGhaInfoMap::const_iterator folowerIt;
    if (data.size() == 2) {
        TWavesChannel& fWaves = ResultBuf.Waves[1];

        history.push_back(fWaves);

        fWaves.WaveParams.clear();
        fWaves.WaveSbInfos.clear();
        // Yes, see bitstream code
        fWaves.WaveSbInfos.resize(usedContiguousSb[leader]);
        folowerIt = data[!leader].GhaInfos.begin();
        leaderStartIt = data[leader].GhaInfos.begin();
    }

    const auto& ghaInfos = data[leader].GhaInfos;
    TWavesChannel& waves = ResultBuf.Waves[0];
    waves.WaveParams.clear();
    waves.WaveSbInfos.clear();
    waves.WaveSbInfos.resize(usedContiguousSb[leader]);
    auto it = ghaInfos.begin();
    uint32_t prevSb = 0;
    uint32_t index = 0;

    if (usedContiguousSb[leader] == 0) {
        return;
    }

    while (it != ghaInfos.end()) {
        const auto sb = ((it->first) >> 10);
        if (sb >= usedContiguousSb[leader]) {
            break;
        }

        const auto freqIndex = it->first & 1023;
        const auto phaseIndex = GhaPhaseToIndex(it->second.phase);
        const auto ampSf = AmplitudeToSf(it->second.magnitude);

        waves.WaveSbInfos[sb].WaveNums++;
        if (sb != prevSb) {
            uint32_t histStop = TAt3PGhaData::INIT;
            if (ResultBufHistory.Waves[0].WaveSbInfos.size() > prevSb) {
                histStop = ResultBufHistory.Waves[0].WaveSbInfos[prevSb].Envelope.second;
            }
            AdjustEnvelope(waves.WaveSbInfos[prevSb].Envelope, data[leader].Envelopes[prevSb], histStop);

            // update index sb -> wave position index
            waves.WaveSbInfos[sb].WaveIndex = index;

            // process folower if present
            if (data.size() == 2) {
                FillFolowerRes(data[leader].GhaInfos, &data[!leader], folowerIt, prevSb);
                leaderStartIt = it;
            }

            prevSb = sb;
        }
        waves.WaveParams.push_back(TAt3PGhaData::TWaveParam{freqIndex, ampSf, 1, phaseIndex});
        it++;
        index++;
    }

    uint32_t histStop = (uint32_t)-2;
    if (ResultBufHistory.Waves[0].WaveSbInfos.size() > prevSb) {
        histStop = ResultBufHistory.Waves[0].WaveSbInfos[prevSb].Envelope.second;
    }

    TGhaProcessor::AdjustEnvelope(waves.WaveSbInfos[prevSb].Envelope, data[leader].Envelopes[prevSb], histStop);

    if (data.size() == 2) {
        FillFolowerRes(data[leader].GhaInfos, &data[!leader], folowerIt, prevSb);
    }
}

uint32_t TGhaProcessor::FillFolowerRes(const TGhaInfoMap& lGhaInfos, const TChannelData* src, TGhaInfoMap::const_iterator& it, const uint32_t curSb)
{
    uint32_t histStop = (uint32_t)-2;
    if (ResultBufHistory.Waves[1].WaveSbInfos.size() > curSb) {
        histStop = ResultBufHistory.Waves[1].WaveSbInfos[curSb].Envelope.second;
    }

    const TGhaInfoMap& fGhaInfos = src->GhaInfos;

    TWavesChannel& waves = ResultBuf.Waves[1];

    uint32_t folowerSbMode = 0; // 0 - no tones, 1 - sharing band, 2 - own tones set
    uint32_t nextSb = 0;
    uint32_t added = 0;

    while (it != fGhaInfos.end()) {
        uint32_t sb = ((it->first) >> 10);
        if (sb > curSb) {
            nextSb = sb;
            break;
        }

        // search same indedx in the leader and set coresponding bit
        folowerSbMode |= uint8_t(lGhaInfos.find(it->first) == lGhaInfos.end()) + 1u;

        const auto freqIndex = it->first & 1023;
        const auto phaseIndex = GhaPhaseToIndex(it->second.phase);
        const auto ampSf = AmplitudeToSf(it->second.magnitude);

        waves.WaveParams.push_back(TAt3PGhaData::TWaveParam{freqIndex, ampSf, 1, phaseIndex});

        it++;
        added++;
    }

    switch (folowerSbMode) {
        case 0:
            ResultBuf.ToneSharing[curSb] = false;
            waves.WaveSbInfos[curSb].WaveNums = 0;
            break;
        case 1:
            ResultBuf.ToneSharing[curSb] = true;
            waves.WaveParams.resize(waves.WaveParams.size() - added);
            break;
        default:
            ResultBuf.ToneSharing[curSb] = false;
            waves.WaveSbInfos[curSb].WaveIndex = waves.WaveParams.size() - added;
            waves.WaveSbInfos[curSb].WaveNums = added;
            AdjustEnvelope(waves.WaveSbInfos[curSb].Envelope, src->Envelopes[curSb], histStop);
    }
    return nextSb;
}

uint32_t TGhaProcessor::AmplitudeToSf(float amp) const
{
    auto it = std::upper_bound(AmpSfTab.begin(), AmpSfTab.end(), amp);
    if (it != AmpSfTab.begin()) {
        it--;
    }
    return it - AmpSfTab.begin();
}

} // namespace

std::unique_ptr<IGhaProcessor> MakeGhaProcessor0(bool stereo)
{
    return std::unique_ptr<TGhaProcessor>(new TGhaProcessor(stereo));
}

} // namespace NAtracDEnc
