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
#include "at3p_pqf_wideband_table.h"
#include "ff/atrac3plus.h"

#include <util.h>
#include <atrac/atrac_psy_common.h>
#include <libgha/include/libgha.h>

#include <memory>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <utility>
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

constexpr double kWidebandFs = 44100.0;
constexpr double kWidebandSubbandBw = kWidebandFs / 32.0; // 1378.125 Hz

double LerpAngleWideband(double a, double b, double t)
{
    double d = std::fmod(b - a + M_PI, 2 * M_PI);
    if (d < 0) d += 2 * M_PI;
    d -= M_PI;
    double r = a + d * t;
    while (r < 0) r += 2 * M_PI;
    while (r >= 2 * M_PI) r -= 2 * M_PI;
    return r;
}

struct TWidebandResponse {
    double Magnitude, Phase, Omega;
};

// Binary search + linear interpolation over the non-uniform calibration
// grid emitted by tools/pqf_wideband_calibrate.cpp (see
// at3p_pqf_wideband_table.h). Empirically measured by probing the real PQF
// analysis filter with synthetic sinusoids -- not hand-derived from the
// DCT-IV fold in atrac3plus_pqf.c.
TWidebandResponse LookupWidebandResponse(int sb, double freqHz)
{
    const TPqfWidebandPoint* t = PqfWidebandTables[sb];
    const size_t n = PqfWidebandTableSize;
    if (freqHz <= t[0].FreqHz) {
        return {t[0].Magnitude, t[0].Phase, t[0].Omega};
    }
    if (freqHz >= t[n - 1].FreqHz) {
        return {t[n - 1].Magnitude, t[n - 1].Phase, t[n - 1].Omega};
    }

    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (t[mid].FreqHz <= freqHz) lo = mid; else hi = mid;
    }
    const double f0 = t[lo].FreqHz, f1 = t[hi].FreqHz;
    const double frac = (f1 > f0) ? (freqHz - f0) / (f1 - f0) : 0.0;
    TWidebandResponse r;
    r.Magnitude = t[lo].Magnitude + (t[hi].Magnitude - t[lo].Magnitude) * frac;
    r.Omega = t[lo].Omega + (t[hi].Omega - t[lo].Omega) * frac;
    r.Phase = LerpAngleWideband(t[lo].Phase, t[hi].Phase, frac);
    return r;
}

// Analytically project one raw wideband tone into a single PQF subband, given
// that subband's precomputed response r (from LookupWidebandResponse). The
// even/odd phase-parity handling is empirical (see the long note at the
// discovery call site): even subbands compose the phase directly, odd subbands
// negate it. Factored out so discovery and the raw-domain (Option A) refit's
// re-projection share exactly the same scale/parity formula.
gha_info ProjectWidebandTone(int sb, const TWidebandResponse& r, const gha_info& wbTone)
{
    gha_info gha;
    gha.frequency = (float)r.Omega;
    gha.phase = (float)((sb % 2 == 0) ? (wbTone.phase + r.Phase) : (-wbTone.phase + r.Phase));
    gha.magnitude = (float)(wbTone.magnitude * r.Magnitude);
    return gha;
}

// Sum of squared error of a tone set against a PCM window -- the exact quantity
// gha_adjust_info's Newton step minimizes, using libgha's own sine convention
// (sin(frequency*n + phase), see gha_adjust_info_newton_md). Used to compare a
// refined tone set against the analytic one and keep whichever actually has the
// lower residual.
double ToneSetSSE(const float* pcm, const vector<gha_info>& tones, size_t n)
{
    double sse = 0.0;
    for (size_t i = 0; i < n; i++) {
        double v = pcm[i];
        for (const auto& t : tones) {
            v -= (double)t.magnitude * std::sin((double)t.frequency * (double)i + (double)t.phase);
        }
        sse += v * v;
    }
    return sse;
}

// Ports TGhaProcessorBase::CheckResuidalAndApply's energy-ratio scan to the
// 2048-sample wideband buffer. PQF decimation here is exactly 16x
// (2048/SAMPLES_PER_SUBBAND(128)==16), so a 64-wideband-sample chunk is
// exactly 4 subband-samples -- the SAME 32-point grid CheckResuidalAndApply
// scans, just derived from wideband-domain data instead of subband-domain
// data. Returns (start,end) in raw subband-sample-equivalent units (0-128,
// multiples of 4), directly compatible with AdjustEnvelope -- or nullopt if
// no reliably long contiguous "tone explains the energy here" run was found
// (mirrors CheckResuidalAndApply's len<4 reject).
//
// Deliberately does not port the Repeat/shrink-refit loop or
// LastResuidalEnergy diminishing-returns tracking: both are specific to
// gha_adjust_info's multi-round refinement callback protocol, which
// gha_extract_one's single-shot API has no equivalent of. A one-shot scan
// is the correct simplification here, not a missing feature.
std::optional<std::pair<uint32_t, uint32_t>> FindWidebandEnvelope(const float* before, const float* after)
{
    constexpr size_t kChunks = 32;
    constexpr size_t kChunkSz = 2048 / kChunks; // 64 wideband samples == 4 subband-samples

    uint32_t start = 0;
    uint32_t curStart = 0;
    uint32_t count = 0;
    uint32_t len = 0;
    bool found = false;

    for (size_t c = 0; c < kChunks; c++) {
        float energyIn = 0.0f, energyOut = 0.0f;
        for (size_t j = 0; j < kChunkSz; j++) {
            const size_t idx = c * kChunkSz + j;
            energyIn += before[idx] * before[idx];
            energyOut += after[idx] * after[idx];
        }

        if (energyIn / energyOut < 1) {
            count = 0;
            found = false;
            curStart = (uint32_t)(c + 1) * 4;
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
    }

    if (len < 4) {
        return std::nullopt;
    }

    const uint32_t end = start + len * 4;
    return std::make_pair(start, end);
}

// Shared base for both GHA strategies. Owns the machinery common to the legacy
// per-subband path and the wideband path: the 128-sample libgha context, the
// result/history buffers, the ff_atrac3p synthesis unit, the static
// psychoacoustic tables, and every helper used by both. The only thing that
// differs between strategies is how each channel's tones are discovered and
// refined into data.GhaInfos/data.Envelopes -- that is the AnalyzeChannels
// hook; DoAnalize (the shared setup + result/filter tail) is a final template
// method around it.
class TGhaProcessorBase : public IGhaProcessor {
protected:
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
            , AdjustStatus(EAdjustStatus::Ok)
            , FrameSz(0)
        {}
        TChannelData* Data;
        size_t Sb;

        enum class EAdjustStatus {
            Error,
            Ok,
            Repeat
        } AdjustStatus;
        size_t FrameSz;
    };

    explicit TGhaProcessorBase(bool stereo)
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

        memset(&ChUnit, 0, sizeof(ChUnit));

        for (size_t ch = 0; ch < 2; ch++) {
           ChUnit.channels[ch].tones_info      = &ChUnit.channels[ch].tones_info_hist[0][0];
           ChUnit.channels[ch].tones_info_prev = &ChUnit.channels[ch].tones_info_hist[1][0];
           for (size_t sb = 0; sb < ATRAC3P_SUBBANDS; sb++) {
               ChUnit.channels[ch].tones_info[sb].num_wavs = 0;
               ChUnit.channels[ch].tones_info_prev[sb].num_wavs = 0;
           }
        }

        ChUnit.waves_info      = &ChUnit.wave_synth_hist[0];
        ChUnit.waves_info_prev = &ChUnit.wave_synth_hist[1];
        ChUnit.waves_info->tones_present = false;
        ChUnit.waves_info_prev->tones_present = false;
    }

public:
    ~TGhaProcessorBase() override
    {
        gha_free_ctx(LibGhaCtx);
    }

    const TAt3PGhaData* DoAnalize(TBufPtr b1, TBufPtr b2, float *w1, float *w2,
        const float* raw1Cur, const float* raw2Cur) final;

protected:
    // The one stage that differs between strategies: discover and refine each
    // channel's tones into data[ch].GhaInfos / data[ch].Envelopes, and return
    // the total number of tones found across all channels. raws holds the raw
    // (pre-PQF) per-channel signal, used only by the wideband strategy.
    virtual size_t AnalyzeChannels(std::vector<TChannelData>& data,
        const std::array<const float*, 2>& raws) = 0;

    void ApplyFilter(const TAt3PGhaData*, float *b1, float *b2);
    static void FillSubbandAth(float* out);
    static TAmpSfTab CreateAmpSfTab();
    static void CheckResuidalAndApply(float* resuidal, size_t size, void* self) noexcept;
    static void GenWaves(const TAt3PGhaData::TWaveParam* param, size_t numWaves, size_t reg_offset, float* out, size_t outLimit);

    void AdjustEnvelope(pair<uint32_t, uint32_t>& envelope, const pair<uint32_t, uint32_t>& src, uint32_t history);
    uint32_t FillFolowerRes(const TGhaInfoMap& lGha, const TChannelData* src, TGhaInfoMap::const_iterator& it, uint32_t leaderSb);

    uint32_t AmplitudeToSf(float amp) const;
    bool CheckNextFrame(const float* nextSrc, const vector<gha_info>& ghaInfos) const;

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

bool TGhaProcessorBase::StaticInited = false;
float TGhaProcessorBase::SubbandAth[SUBBANDS];
float TGhaProcessorBase::SineTab[2048];
TGhaProcessorBase::TAmpSfTab TGhaProcessorBase::AmpSfTab;

// Legacy per-subband ("narrowband") strategy: detect tones independently in
// each PQF subband via repeated DoRound passes.
class TSubbandGhaProcessor final : public TGhaProcessorBase {
public:
    explicit TSubbandGhaProcessor(bool stereo)
        : TGhaProcessorBase(stereo)
    {}

protected:
    size_t AnalyzeChannels(std::vector<TChannelData>& data,
        const std::array<const float*, 2>& raws) override;

private:
    bool DoRound(TChannelData& data, size_t& totalTones) const;
};

// Wideband strategy: extract tones from the raw pre-PQF signal by matching
// pursuit, project them into PQF subbands, then Newton-refine (subband-domain
// by default, or raw-domain with WidebandRefineMode == 1).
class TWidebandGhaProcessor final : public TGhaProcessorBase {
public:
    TWidebandGhaProcessor(bool stereo, int refineMode)
        : TGhaProcessorBase(stereo)
        , WidebandCtx(gha_create_ctx(2048))
        , WidebandRefineMode(refineMode)
    {
        gha_set_max_magnitude(WidebandCtx, 1e9);
        gha_set_upsample(WidebandCtx, 1);
    }

    ~TWidebandGhaProcessor() override
    {
        gha_free_ctx(WidebandCtx);
    }

protected:
    size_t AnalyzeChannels(std::vector<TChannelData>& data,
        const std::array<const float*, 2>& raws) override;

private:
    // Persistent state for ONE channel's wideband matching-pursuit
    // extraction, carried across multiple WidebandExtractOne calls so that
    // AnalyzeChannels can interleave extraction attempts between channels (see
    // WidebandExtractOne) instead of letting one channel run to completion
    // before the other gets a turn.
    // One raw pre-projection wideband tone accepted during discovery, together
    // with the exact (subband, freqIndex) GhaInfos entries its projection
    // created. The projection map lets the raw-domain (Option A) refit update
    // those specific entries in place after re-refining Info -- keeping the
    // GhaInfos count, per-subband envelopes and the shared tone budget exactly
    // as discovery left them (a clear-and-rebuild would desync all three).
    struct TAcceptedWbTone {
        gha_info Info;
        std::vector<std::pair<uint8_t, uint32_t>> Proj;
    };

    struct TWidebandExtractState {
        float WbScratch[2048];
        // Untouched copy of the original 2048 raw signal (WbScratch is consumed
        // by matching pursuit). The raw-domain refit needs the full original,
        // the 2048-domain analog of DoRound refitting against the full subband
        // PCM.
        float WbOriginal[2048];
        float MaxMagnitudeSeen = 0.0f;
        int IterCount = 0;
        // Per-subband envelope accumulated across the whole extraction
        // (not written into data.Envelopes[] until CommitWidebandEnvelopes):
        // two different wideband tones (different home subbands) can
        // legitimately both project into the same neighbor subband via the
        // home+-1 window, and whichever tone's insertion runs later must
        // not silently clobber the other's envelope -- union instead.
        bool SbHasEnvelope[SUBBANDS] = {false};
        pair<uint32_t, uint32_t> SbEnvelopeUnion[SUBBANDS];

        // Set per subband by RefineWidebandTones: true if the subband's tones
        // were successfully Newton-refined against the real subband PCM (so
        // its envelope was written by CheckResuidalAndApply as a side effect
        // and CommitWidebandEnvelopes must NOT overwrite it with the analytic
        // union), false if the refit was rejected and the analytic projection
        // + union envelope was kept.
        bool SbRefined[SUBBANDS] = {false};

        // The raw pre-projection 2048-domain tones accepted this frame (in
        // extraction order) with their projection maps. Consumed only by the
        // raw-domain (Option A) refit; the subband-domain (Option B) refit
        // ignores it.
        vector<TAcceptedWbTone> AcceptedWbTones;
    };

    void InitWidebandExtractState(TWidebandExtractState& state, const float* rawPcm) const;
    bool WidebandExtractOne(TChannelData& data, TWidebandExtractState& state, size_t& totalTones) const;
    void RefineWidebandTones(TChannelData& data, TWidebandExtractState& state) const;
    void RefineWidebandTonesSubbandDomain(TChannelData& data, TWidebandExtractState& state) const;
    void RefineWidebandTonesRawDomain(TChannelData& data, TWidebandExtractState& state) const;
    void CommitWidebandEnvelopes(TChannelData& data, const TWidebandExtractState& state) const;

    gha_ctx_t WidebandCtx;
    const int WidebandRefineMode; // 0 = subband-domain refit, 1 = raw-domain refit
};

void TGhaProcessorBase::FillSubbandAth(float* out)
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

TGhaProcessorBase::TAmpSfTab TGhaProcessorBase::CreateAmpSfTab()
{
    TAmpSfTab AmpSfTab;
    for (int i = 0; i < (int)AmpSfTab.size(); i++) {
        AmpSfTab[i] = exp2f((i - 3) / 4.0f);
    }
    return AmpSfTab;
}

void TGhaProcessorBase::GenWaves(const TAt3PGhaData::TWaveParam* param, size_t numWaves, size_t reg_offset, float* out, size_t outLimit)
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

void TGhaProcessorBase::CheckResuidalAndApply(float* resuidal, size_t size, void* d) noexcept
{
    TChannelGhaCbCtx* ctx = (TChannelGhaCbCtx*)(d);
    const float* srcBuf = ctx->Data->SrcBuf + (ctx->Sb * SAMPLES_PER_SUBBAND);
    //std::cerr << "TGhaProcessorBase::CheckResuidal " << srcBuf[0] << " " << srcBuf[1] << " " << srcBuf[2] << " " << srcBuf[3] <<  std::endl;

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

void TGhaProcessorBase::ApplyFilter(const TAt3PGhaData* d, float* b1, float* b2)
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

// Shared template method: set up per-channel TChannelData, delegate tone
// discovery/refinement to the strategy's AnalyzeChannels, then run the common
// result-build + filter tail. The two strategies differ only in AnalyzeChannels.
const TAt3PGhaData* TGhaProcessorBase::DoAnalize(TBufPtr b1, TBufPtr b2, float* w1, float* w2,
    const float* raw1Cur, const float* raw2Cur)
{
    vector<TChannelData> data((size_t)Stereo + 1);

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

    const size_t totalTones = AnalyzeChannels(data, {raw1Cur, raw2Cur});

    if (totalTones == 0) {
        ApplyFilter(nullptr, w1, w2);
        return nullptr;
    }

    FillResultBuf(data);

    ResultBufHistory = ResultBuf;

    ApplyFilter(&ResultBuf, w1, w2);

    return  &ResultBuf;
}

// Legacy per-subband strategy: repeated DoRound passes over all subbands until
// no pass makes progress (or the shared 48-tone budget is hit). raws unused.
size_t TSubbandGhaProcessor::AnalyzeChannels(std::vector<TChannelData>& data,
    const std::array<const float*, 2>& /*raws*/)
{
    size_t totalTones = 0;
    bool progress[2] = {false};
    do {
        for (size_t ch = 0; ch < data.size(); ch++) {
            progress[ch] = DoRound(data[ch], totalTones);
        }
    } while ((progress[0] || progress[1]) && totalTones < 48);
    return totalTones;
}

// Wideband strategy: round-robin matching-pursuit extraction between channels,
// then per-channel refinement and envelope commit.
size_t TWidebandGhaProcessor::AnalyzeChannels(std::vector<TChannelData>& data,
    const std::array<const float*, 2>& raws)
{
    size_t totalTones = 0;
    TWidebandExtractState wbState[2];
    for (size_t ch = 0; ch < data.size(); ch++) {
        InitWidebandExtractState(wbState[ch], raws[ch]);
    }

    // Round-robin between channels, one matching-pursuit extraction attempt at
    // a time, same fairness contract as the legacy DoRound dispatch loop:
    // without this, a dense channel 0 (e.g. a busy left channel) can run its
    // own extraction all the way to the shared 48-tone budget before channel 1
    // gets a single attempt.
    bool wbProgress[2] = {data.size() > 0, data.size() > 1};
    do {
        for (size_t ch = 0; ch < data.size(); ch++) {
            wbProgress[ch] = wbProgress[ch] && WidebandExtractOne(data[ch], wbState[ch], totalTones);
        }
    } while ((wbProgress[0] || (data.size() > 1 && wbProgress[1])) && totalTones < 48);

    // Refinement stage: Newton-refine each subband's projected tones (Option B
    // by default, Option A when WidebandRefineMode == 1).
    for (size_t ch = 0; ch < data.size(); ch++) {
        RefineWidebandTones(data[ch], wbState[ch]);
    }

    for (size_t ch = 0; ch < data.size(); ch++) {
        CommitWidebandEnvelopes(data[ch], wbState[ch]);
    }
    return totalTones;
}

bool TGhaProcessorBase::CheckNextFrame(const float* nextSrc, const vector<gha_info>& ghaInfos) const
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

bool TSubbandGhaProcessor::DoRound(TChannelData& data, size_t& totalTones) const
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
                        // std::cerr << "jackpot! same freq index after adjust call, sb: " << sb << " " << std::endl;
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
                ASSERT(ins);
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

bool TGhaProcessorBase::PsyPreCheck(size_t sb, const struct gha_info& gha, const TChannelData& data) const
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

void TWidebandGhaProcessor::InitWidebandExtractState(TWidebandExtractState& state, const float* rawPcm) const
{
    memcpy(state.WbScratch, rawPcm, sizeof(state.WbScratch));
    memcpy(state.WbOriginal, rawPcm, sizeof(state.WbOriginal));
    state.MaxMagnitudeSeen = 0.0f;
    state.IterCount = 0;
    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        state.SbHasEnvelope[sb] = false;
    }
}

// Performs ONE matching-pursuit extraction step for this channel: finds the
// next-strongest wideband tone, projects it into its home subband +-1
// window, and inserts it (all candidate subbands at once) if accepted.
// Returns true if a tone was inserted and the caller should try again
// (subject to its own round-robin budget), false once this channel's
// extraction is genuinely finished for the frame (decayed below threshold,
// below ATH everywhere, hit the global tone budget, or ran out of
// iterations) -- mirrors DoRound's bool-progress contract so DoAnalize can
// interleave extraction attempts between channels instead of letting one
// channel's matching pursuit run to completion before the other gets a
// turn (a dense channel 0 could otherwise consume the whole shared 48-tone
// budget before channel 1 is ever tried).
bool TWidebandGhaProcessor::WidebandExtractOne(TChannelData& data, TWidebandExtractState& state, size_t& totalTones) const
{
    // Global (both-channels) budget: ApplyFilter aborts if the combined
    // tone count across channels exceeds 48 in one frame. totalTones is
    // shared across both channels' state in DoAnalize, same contract as
    // DoRound's totalTones parameter.
    constexpr size_t kMaxTotalTones = 48;
    constexpr size_t kMinFreqDistance = 20; // same guard DoRound uses
    constexpr double kFreqMax = SUBBANDS * kWidebandSubbandBw; // 11025 Hz -- above this, no home subband exists
    constexpr double kProjectGateRelative = 0.02; // -34dB relative to home subband's own response
    // Hard cap on EXTRACTION ATTEMPTS for this channel across the whole
    // frame, independent of accepted tones. gha_extract_one always removes
    // real energy from WbScratch, so this isn't literally infinite, but for
    // dense, non-sparse real audio (unlike a clean synthetic tone), matching
    // pursuit can take an enormous number of vanishingly-small-decrement
    // iterations to either fill the tone budget or decay below the 1/10
    // threshold. A single call here may retry internally past several
    // rejected candidates (see the "continue"s below) before returning,
    // all counted against this same budget. Confirmed empirically: a real
    // track hung (still running after 2+ minutes on a single 2048-sample
    // frame) without this cap.
    constexpr int kMaxExtractIterations = 200;

    struct TCandidate {
        size_t Sb;
        uint32_t FreqIndex;
        struct gha_info Gha;
    };

    float wbBefore[2048];
    while (state.IterCount < kMaxExtractIterations) {
        if (totalTones >= kMaxTotalTones) {
            return false;
        }
        state.IterCount++;

        memcpy(wbBefore, state.WbScratch, sizeof(state.WbScratch));
        struct gha_info info;
        gha_extract_one(state.WbScratch, &info, WidebandCtx);

        if (isnan(info.magnitude) || info.magnitude <= 0) {
            return false;
        }

        const double freqHz = info.frequency * kWidebandFs / (2.0 * M_PI);
        if (freqHz >= kFreqMax) {
            continue;
        }

        if (state.MaxMagnitudeSeen > 0 && info.magnitude < state.MaxMagnitudeSeen / 10) {
            return false;
        }

        const int homeSb = std::clamp((int)std::floor(freqHz / kWidebandSubbandBw), 0, (int)SUBBANDS - 1);
        const double homeMag = LookupWidebandResponse(homeSb, freqHz).Magnitude;

        vector<TCandidate> candidates;
        bool anyPassedAth = false;

        for (int sb = std::max(0, homeSb - 1); sb <= std::min((int)SUBBANDS - 1, homeSb + 1); sb++) {
            const TWidebandResponse r = LookupWidebandResponse(sb, freqHz);
            if (r.Magnitude < kProjectGateRelative * std::max(homeMag, 1.0)) {
                continue;
            }

            // Empirical scale/parity handling lives in ProjectWidebandTone:
            // even subbands compose the wideband tone's phase directly, odd
            // subbands NEGATE it (not a constant offset -- a first guess of
            // +pi only matched at one frequency by coincidence). Consistent
            // with alternating-band spectral inversion in a cosine-modulated
            // filter bank; likely PQF's matrixing() writing
            // samples[i*128]=res[15-i], i.e. each subband reading a DCT-IV
            // coefficient at a parity-dependent index. The raw-domain refit
            // re-projects through the SAME helper so its formula never drifts
            // from discovery's.
            const gha_info gha = ProjectWidebandTone(sb, r, info);

            if (!PsyPreCheck(sb, gha, data)) {
                continue;
            }
            anyPassedAth = true;

            const auto freqIndex = GhaFreqToIndex(gha.frequency, sb);

            const auto it = data.GhaInfos.lower_bound(freqIndex);
            bool tooClose = false;
            if (it != data.GhaInfos.end() && (it->first == freqIndex || it->first - freqIndex < kMinFreqDistance)) {
                tooClose = true;
            }
            if (!tooClose && it != data.GhaInfos.begin()) {
                auto prev = it;
                prev--;
                if (freqIndex - prev->first < kMinFreqDistance) {
                    tooClose = true;
                }
            }
            if (tooClose || data.SubbandDone[sb] >= 15) {
                continue;
            }

            candidates.push_back({(size_t)sb, freqIndex, gha});
        }

        if (!anyPassedAth) {
            // Signal below the hearing threshold everywhere this tone could
            // land -- matching pursuit extracts in roughly decreasing
            // magnitude order, so later tones are unlikely to fare better.
            return false;
        }

        if (candidates.empty()) {
            // This particular tone didn't yield any usable insertion
            // (dedup/per-subband cap); its energy is already subtracted
            // from WbScratch by gha_extract_one, so keep going.
            continue;
        }

        if (totalTones + candidates.size() > kMaxTotalTones) {
            return false;
        }

        // Where, within this frame, is the tone gha_extract_one just found
        // actually present -- computed once per tone (a property of the
        // tone's temporal presence in the signal, shared across every
        // subband it projects into), not per candidate subband. On
        // nullopt (couldn't reliably localize, mirroring
        // CheckResuidalAndApply's len<4 reject), reject the tone for every
        // subband: GhaInfos.insert must always be paired with a same-frame
        // Envelopes[] write (see CommitWidebandEnvelopes's guardrail), so
        // "insert anyway with a fallback envelope" is not a safe option
        // here.
        const auto env = FindWidebandEnvelope(wbBefore, state.WbScratch);
        if (!env) {
            continue;
        }

        TAcceptedWbTone accepted;
        accepted.Info = info;
        accepted.Proj.reserve(candidates.size());
        for (const auto& c : candidates) {
            data.GhaInfos.insert({c.FreqIndex, c.Gha});
            data.MaxToneMagnitude[c.Sb] = std::max(data.MaxToneMagnitude[c.Sb], c.Gha.magnitude);
            data.SubbandDone[c.Sb]++;
            if (!state.SbHasEnvelope[c.Sb]) {
                state.SbEnvelopeUnion[c.Sb] = *env;
                state.SbHasEnvelope[c.Sb] = true;
            } else {
                state.SbEnvelopeUnion[c.Sb].first = std::min(state.SbEnvelopeUnion[c.Sb].first, env->first);
                state.SbEnvelopeUnion[c.Sb].second = std::max(state.SbEnvelopeUnion[c.Sb].second, env->second);
            }
            accepted.Proj.emplace_back((uint8_t)c.Sb, c.FreqIndex);
            totalTones++;
        }

        state.MaxMagnitudeSeen = std::max(state.MaxMagnitudeSeen, info.magnitude);
        // Record the raw pre-projection tone plus the exact GhaInfos entries
        // its projection created, so the raw-domain refit can re-refine Info
        // and update those entries in place (see RefineWidebandTonesRawDomain).
        // Recorded only on accept, so it stays in lockstep with GhaInfos.
        state.AcceptedWbTones.push_back(std::move(accepted));
        return true;
    }
    return false;
}

// Dispatches to the selected wideband refinement strategy (WidebandRefineMode,
// set via the "ghawbrefine" advanced option). Both strategies take the same
// discovery output (GhaInfos + envelope unions in state) and leave it in a
// state CommitWidebandEnvelopes can finalize; they differ only in the domain
// the Newton refit runs in.
void TWidebandGhaProcessor::RefineWidebandTones(TChannelData& data, TWidebandExtractState& state) const
{
    if (WidebandRefineMode == 1) {
        RefineWidebandTonesRawDomain(data, state);
    } else {
        RefineWidebandTonesSubbandDomain(data, state);
    }
}

// Option B (default). Newton-refine each subband's projected tones against that
// subband's own real 128-sample PCM, exactly as DoRound does for the legacy
// path -- the analytic wideband projection is only a first guess, and
// gha_adjust_info + CheckResuidalAndApply jointly minimize the actual
// per-subband residual (the quantity ff_atrac3p subtracts and encodes). On any
// sign of an ill-conditioned fit (boundary tones can produce near-singular
// Hessians) the whole subband's refit is rejected and the analytic projection
// is kept, so this can only improve or leave a subband unchanged, never worse.
void TWidebandGhaProcessor::RefineWidebandTonesSubbandDomain(TChannelData& data, TWidebandExtractState& state) const
{
    // Drift guard bounds (see plan Step 3): reject a refit whose refined tone
    // strays too far in frequency or magnitude from the analytic projection.
    // The analytic projection is already a good, well-conditioned estimate
    // (found in the 2048 domain), so a large jump signals a spurious local
    // fit from a near-singular per-subband solve rather than a real
    // improvement. Conservative to start; loosen only if verification shows
    // legitimate refits being rejected.
    constexpr uint32_t kMaxRefineDriftIdx = 40;
    constexpr float kMaxRefineMagRatio = 4.0f;

    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        auto cit = data.GhaInfos.lower_bound(sb << 10);
        vector<gha_info> tmp;
        for (auto it = cit; it != data.GhaInfos.end() && it->first < (sb + 1) << 10; it++) {
            tmp.push_back(it->second);
        }
        if (tmp.empty()) {
            continue;
        }

        // Untouched copy for the fallback path: gha_adjust_info mutates tmp in
        // place, so the analytic values must be preserved separately.
        const vector<gha_info> tmpAnalytic = tmp;

        // Exactly DoRound's refit call: LibGhaCtx is the 128-sample context,
        // and the pcm pointer is the raw subband buffer CheckResuidalAndApply
        // also indexes internally (data.SrcBuf + sb*128). Never WidebandCtx
        // here -- CheckResuidalAndApply aborts unless size == 128.
        const float* srcB = data.SrcBuf + (sb * SAMPLES_PER_SUBBAND);
        TChannelGhaCbCtx ctx(&data, sb);
        do {
            int ar = gha_adjust_info(srcB, tmp.data(), tmp.size(), LibGhaCtx, CheckResuidalAndApply, &ctx, ctx.FrameSz);
            if (ar < 0) {
                ctx.AdjustStatus = TChannelGhaCbCtx::EAdjustStatus::Error;
            }
        } while (ctx.AdjustStatus == TChannelGhaCbCtx::EAdjustStatus::Repeat);

        bool accept = (ctx.AdjustStatus == TChannelGhaCbCtx::EAdjustStatus::Ok);

        // Dup check (DoRound's dupFound): two refined tones can collapse onto
        // the same quantized frequency index, which would corrupt the wave set.
        if (accept) {
            std::sort(tmp.begin(), tmp.end(), [](const gha_info& a, const gha_info& b) { return a.frequency < b.frequency; });
            auto idx1 = GhaFreqToIndex(tmp[0].frequency, sb);
            for (size_t i = 1; i < tmp.size() && accept; i++) {
                auto idx2 = GhaFreqToIndex(tmp[i].frequency, sb);
                if (idx2 == idx1) {
                    accept = false;
                } else {
                    idx1 = idx2;
                }
            }
        }

        // Drift guard: pair each refined tone with the nearest analytic one by
        // frequency index and require both frequency and magnitude to stay
        // within bounds. tmpAnalytic is sorted the same way (matching pursuit
        // inserts by index, so GhaInfos was already frequency-ordered within
        // the subband; sort defensively to be safe).
        if (accept) {
            vector<gha_info> analyticSorted = tmpAnalytic;
            std::sort(analyticSorted.begin(), analyticSorted.end(), [](const gha_info& a, const gha_info& b) { return a.frequency < b.frequency; });
            for (size_t i = 0; i < tmp.size() && accept; i++) {
                const uint32_t rIdx = GhaFreqToIndex(tmp[i].frequency, sb);
                const uint32_t aIdx = GhaFreqToIndex(analyticSorted[i].frequency, sb);
                const uint32_t d = (rIdx > aIdx) ? (rIdx - aIdx) : (aIdx - rIdx);
                if (d > kMaxRefineDriftIdx) {
                    accept = false;
                    break;
                }
                const float am = analyticSorted[i].magnitude;
                const float rm = tmp[i].magnitude;
                if (isnan(rm) || rm <= 0.0f || am <= 0.0f ||
                    rm > am * kMaxRefineMagRatio || am > rm * kMaxRefineMagRatio) {
                    accept = false;
                    break;
                }
            }
        }

        // Explicit "never worse" guard. libgha's Newton step has no line search,
        // and CheckResuidalAndApply measures the residual against the raw PCM
        // (not against the analytic projection), so a refit that clears the
        // callback status and the drift guard can still have HIGHER SSE than the
        // analytic tones. Compare the two tone sets directly against the real
        // subband PCM and keep the refined set only if it genuinely lowers the
        // residual. (Compared on the fitted float params; the emitted params are
        // quantized, but the drift guard already bounds how far quantization can
        // diverge, so this is a sound proxy for "the refit did not make it
        // worse".)
        if (accept) {
            const double sseRefined = ToneSetSSE(srcB, tmp, SAMPLES_PER_SUBBAND);
            const double sseAnalytic = ToneSetSSE(srcB, tmpAnalytic, SAMPLES_PER_SUBBAND);
            if (!(sseRefined < sseAnalytic)) {
                accept = false;
            }
        }

        if (!accept) {
            // Fallback: keep the analytic projection untouched. GhaInfos still
            // holds the original tones; the analytic union envelope
            // (state.SbEnvelopeUnion[sb]) is used by CommitWidebandEnvelopes.
            // SbRefined[sb] stays false. totalTones is not touched (we did not
            // add or remove a tone, only attempted to refine an existing set).
            //
            // CheckResuidalAndApply may have partially written data.Envelopes[sb]
            // before returning Error; reset it to the INIT sentinel so the
            // fallback union path in CommitWidebandEnvelopes owns it cleanly.
            data.Envelopes[sb] = {TAt3PGhaData::INIT, TAt3PGhaData::INIT};
            data.LastResuidalEnergy[sb] = 0.0f;
            continue;
        }

        // Accept: replace the subband's GhaInfos entries with the refined tones
        // at their new frequency indices (DoRound lines 740-749).
        {
            auto it = cit;
            for (size_t i = 0; i < tmpAnalytic.size(); i++) {
                it = data.GhaInfos.erase(it);
            }
            for (const auto& x : tmp) {
                data.MaxToneMagnitude[sb] = std::max(data.MaxToneMagnitude[sb], x.magnitude);
                const auto newIndex = GhaFreqToIndex(x.frequency, sb);
                data.GhaInfos.insert({newIndex, x});
            }
        }

        // CheckResuidalAndApply wrote data.Envelopes[sb] (start,end in raw
        // units). Apply the same gapless collapse DoRound does: if the tone
        // fills the frame and continues into the next, mark the stop point
        // EMPTY_POINT (seamless continuation).
        if (data.Envelopes[sb].second == SAMPLES_PER_SUBBAND) {
            if (CheckNextFrame(data.SrcBufNext + SAMPLES_PER_SUBBAND * sb, tmp)) {
                data.Envelopes[sb].second = TAt3PGhaData::EMPTY_POINT;
            }
        }
        state.SbRefined[sb] = true;
    }
}

// Option A (opt-in via ghawbrefine=1). Jointly Newton-refine the raw
// pre-projection wideband tones against the full original 2048-sample signal
// (WidebandCtx), then re-project the refined parameters into the subbands each
// tone already occupies. This refines in the well-conditioned wideband domain
// (no subband-boundary ill-conditioning at all), unlike the subband-domain
// refit; the trade-off is that it minimizes the 2048-domain residual rather
// than the exact per-subband residual that gets encoded, so it only helps to
// the extent the calibration table is accurate. Left non-default because the
// subband-domain refit already beats legacy on real audio; this is the
// experimental alternative kept for A/B comparison.
void TWidebandGhaProcessor::RefineWidebandTonesRawDomain(TChannelData& data, TWidebandExtractState& state) const
{
    auto& tones = state.AcceptedWbTones;
    if (tones.empty()) {
        return;
    }

    // Batch size bounds the joint Newton dimension. gha_adjust_info_newton_md
    // allocas ~7 * dim * 2048 * 8 bytes (~115 KB per tone at sz=2048); a full
    // 48-tone solve would be ~5.5 MB on the stack. 6 keeps it under ~700 KB
    // while preserving the coupling that matters -- adjacent (frequency-sorted)
    // tones interact, distant ones are ~orthogonal, so batching by frequency
    // barely changes the fit.
    constexpr size_t kRefineBatch = 6;
    // Reject a refined tone that strayed too far from its analytic estimate --
    // the sign of a divergent / ill-posed solve. ~60 Hz mirrors Option B's
    // 40-freq-index bound (40/1024 * 1378 Hz subband bandwidth ~= 54 Hz).
    constexpr double kMaxRefineDriftHz = 60.0;
    constexpr float kMaxRefineMagRatio = 4.0f;

    // Frequency-sort so batches group interacting neighbors; Proj travels with
    // each tone. Snapshot the analytic (pre-refit) tones for the drift guard
    // and for the collision fallback below.
    std::sort(tones.begin(), tones.end(),
        [](const TAcceptedWbTone& a, const TAcceptedWbTone& b) { return a.Info.frequency < b.Info.frequency; });
    vector<gha_info> analytic(tones.size());
    for (size_t i = 0; i < tones.size(); i++) {
        analytic[i] = tones[i].Info;
    }

    // Batched joint refit against the full original signal. cb=nullptr: we only
    // want refined params, no residual callback (gha.c guards `if (cb && ...)`);
    // size_limit=0 uses the full 2048. A batch that fails to solve keeps its
    // analytic values (tones[].Info unchanged for that batch).
    for (size_t base = 0; base < tones.size(); base += kRefineBatch) {
        const size_t k = std::min(kRefineBatch, tones.size() - base);
        gha_info batch[kRefineBatch];
        for (size_t j = 0; j < k; j++) {
            batch[j] = tones[base + j].Info;
        }
        int rv = gha_adjust_info(state.WbOriginal, batch, k, WidebandCtx, nullptr, nullptr, 0);
        if (rv < 0) {
            continue;
        }
        for (size_t j = 0; j < k; j++) {
            tones[base + j].Info = batch[j];
        }
    }

    // Per-tone drift guard: fall back to the analytic tone if the refit
    // diverged (NaN / non-positive magnitude, too-large frequency or magnitude
    // move). The chosen source is what we re-project below.
    vector<const gha_info*> src(tones.size());
    for (size_t i = 0; i < tones.size(); i++) {
        const gha_info& refined = tones[i].Info;
        const gha_info& orig = analytic[i];
        const double fRef = refined.frequency * kWidebandFs / (2.0 * M_PI);
        const double fOrig = orig.frequency * kWidebandFs / (2.0 * M_PI);
        bool ok = !isnan(refined.magnitude) && refined.magnitude > 0.0f &&
                  std::abs(fRef - fOrig) <= kMaxRefineDriftHz &&
                  refined.magnitude <= orig.magnitude * kMaxRefineMagRatio &&
                  orig.magnitude <= refined.magnitude * kMaxRefineMagRatio;
        src[i] = ok ? &refined : &orig;
    }

    // Upgrade the analytic projections that discovery already left in GhaInfos
    // to the refined parameters, strictly IN PLACE. In wideband mode GhaInfos
    // holds EXACTLY the projection entries recorded in the Proj maps (discovery
    // is the only inserter) and every oldKey is globally unique, so we never
    // clear/rebuild: for each drift-guard-passing tone we re-key each of its
    // entries within its own subband and replace the value; a tone that failed
    // the guard is simply left analytic. Crucially, on any within-subband key
    // collision the tone is left analytic and NEVER dropped -- so every
    // subband keeps exactly its discovery tone set, which keeps the per-subband
    // envelope union and the tone count consistent (the earlier clear-and-
    // rebuild could silently drop a projection on a double collision, leaving a
    // subband's union describing a tone that no longer existed). The map KEY
    // carries the emitted frequency index (FillResultBuf uses key & 1023); the
    // value carries amp/phase. Envelopes stay from the discovery union
    // (SbRefined left false), same as the analytic wideband path.
    for (size_t i = 0; i < tones.size(); i++) {
        if (src[i] != &tones[i].Info) {
            continue; // drift guard rejected this tone -> keep its analytic entries
        }
        const gha_info& s = tones[i].Info; // refined
        const double freqHz = s.frequency * kWidebandFs / (2.0 * M_PI);
        for (const auto& pr : tones[i].Proj) {
            const int sb = pr.first;
            const uint32_t oldKey = pr.second;
            const TWidebandResponse r = LookupWidebandResponse(sb, freqHz);
            const gha_info gha = ProjectWidebandTone(sb, r, s);
            const uint32_t newKey = GhaFreqToIndex(gha.frequency, sb);

            auto oldIt = data.GhaInfos.find(oldKey);
            if (oldIt == data.GhaInfos.end()) {
                continue; // defensive: entry already moved (cannot happen given unique oldKeys)
            }
            if (newKey == oldKey) {
                oldIt->second = gha; // same quantized index, just refine amp/phase
            } else if (data.GhaInfos.find(newKey) == data.GhaInfos.end()) {
                data.GhaInfos.erase(oldIt);
                data.GhaInfos.insert({newKey, gha});
            } else {
                continue; // within-subband collision -> keep analytic, never drop
            }
            data.MaxToneMagnitude[sb] = std::max(data.MaxToneMagnitude[sb], gha.magnitude);
        }
    }
}

void TWidebandGhaProcessor::CommitWidebandEnvelopes(TChannelData& data, const TWidebandExtractState& state) const
{
    // Commit the accumulated per-subband envelope, deciding gapless
    // continuation the same way DoRound does: reuse CheckNextFrame
    // (unchanged) on this subband's own next-frame lookahead
    // (data.SrcBufNext, already populated unconditionally at the top of
    // DoAnalize) against ALL of this subband's current tones together (not
    // per wideband-extraction-iteration -- a subband can hold contributions
    // from multiple iterations, and checking continuation once per subband
    // avoids redundant/conflicting decisions).
    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        // Refined subbands already have their envelope written by
        // CheckResuidalAndApply (measured against the real subband residual)
        // and gapless-collapsed inside RefineWidebandTones -- do not overwrite
        // it with the analytic union here.
        if (state.SbRefined[sb]) {
            continue;
        }
        if (!state.SbHasEnvelope[sb]) {
            continue;
        }
        vector<gha_info> tmp;
        auto cit = data.GhaInfos.lower_bound(sb << 10);
        for (auto it = cit; it != data.GhaInfos.end() && it->first < (sb + 1) << 10; it++) {
            tmp.push_back(it->second);
        }
        const bool cont = !tmp.empty() && CheckNextFrame(data.SrcBufNext + SAMPLES_PER_SUBBAND * sb, tmp);
        const auto& u = state.SbEnvelopeUnion[sb];
        data.Envelopes[sb] = {u.first, (u.second == SAMPLES_PER_SUBBAND && cont) ? TAt3PGhaData::EMPTY_POINT : u.second};
    }

    // FillResultBuf no longer requires a contiguous-from-0 run (it now sets
    // NumToneBands to highestUsedSb+1 and leaves empty subbands in between
    // as legitimate NumWaves==0 entries -- the wire format already supports
    // that, see WriteTonalBlock's "if (numWaves == 0) continue;"). So unlike
    // an earlier version of this code, there is no need to synthesize
    // anchor tones purely to plug gaps below the highest used subband.

    // Invariant every insert in WidebandExtractOne must preserve:
    // GhaInfos.insert(sb) is always paired with a same-frame Envelopes[sb]
    // write. Envelopes[] is array-initialized with a single aggregate
    // initializer, so only Envelopes[0] starts at {INIT,INIT};
    // Envelopes[1..7] value-initialize to {0,0}, and AdjustEnvelope aborts
    // on an untouched {0,0} slot (its src.second==0 guard). A future edit
    // that inserts without writing the envelope crashes the encoder here,
    // not just misencodes -- this check exists to fail loudly and locally.
    for (const auto& kv : data.GhaInfos) {
        size_t sb = kv.first >> 10;
        ASSERT(data.Envelopes[sb].first != 0 || data.Envelopes[sb].second != 0);
    }
}

void TGhaProcessorBase::AdjustEnvelope(pair<uint32_t, uint32_t>& envelope, const pair<uint32_t, uint32_t>& src, uint32_t history)
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

void TGhaProcessorBase::FillResultBuf(const vector<TChannelData>& data)
{
    // NumToneBands is highestUsedSb+1, NOT a count of "contiguous" subbands:
    // the wire format tolerates NumWaves==0 for any subband inside that
    // range (WriteTonalBlock's "if (numWaves == 0) continue;" skips only the
    // per-tone frequency/amplitude payload, the envelope/num-waves slots are
    // still emitted), so a subband with no tones in the middle of the used
    // range is legitimate and does not need to be filled with a synthetic
    // tone. Confirmed empirically: without this, a 0-10kHz chirp whose swept
    // tone's home subband reached 2+ lost the whole tone (silence from
    // ~3.5kHz to 10kHz) because the old "stop at the first gap" walk
    // truncated NumToneBands right after subband 0.
    uint32_t numToneBandsUsed[2] = {0, 0};
    for (size_t ch = 0; ch < data.size(); ch++) {
        int maxSb = -1;
        for (const auto& info : data[ch].GhaInfos) {
            maxSb = max(maxSb, (int)(info.first >> 10));
        }
        numToneBandsUsed[ch] = (maxSb >= 0) ? (uint32_t)(maxSb + 1) : 0;
    }

    bool leader = numToneBandsUsed[1] > numToneBandsUsed[0];

    std::vector<TWavesChannel> history;
    history.reserve(data.size());

    history.push_back(ResultBuf.Waves[0]);

    ResultBuf.SecondIsLeader = leader;
    ResultBuf.NumToneBands = numToneBandsUsed[leader];

    TGhaInfoMap::const_iterator folowerIt;
    if (data.size() == 2) {
        TWavesChannel& fWaves = ResultBuf.Waves[1];

        history.push_back(fWaves);

        fWaves.WaveParams.clear();
        fWaves.WaveSbInfos.clear();
        // Yes, see bitstream code
        fWaves.WaveSbInfos.resize(numToneBandsUsed[leader]);
        folowerIt = data[!leader].GhaInfos.begin();
    }

    const auto& ghaInfos = data[leader].GhaInfos;
    TWavesChannel& waves = ResultBuf.Waves[0];
    waves.WaveParams.clear();
    waves.WaveSbInfos.clear();
    waves.WaveSbInfos.resize(numToneBandsUsed[leader]);

    if (numToneBandsUsed[leader] == 0) {
        return;
    }

    // Walk subband indices explicitly (0..NumToneBands-1), not just the
    // subbands that happen to have leader entries -- a gap subband still
    // needs its WaveNums==0 default left alone and still needs to give the
    // follower channel a chance to contribute independent tones there.
    auto it = ghaInfos.begin();
    for (uint32_t sb = 0; sb < numToneBandsUsed[leader]; sb++) {
        const uint32_t index = (uint32_t)waves.WaveParams.size();
        while (it != ghaInfos.end() && ((it->first) >> 10) == sb) {
            const auto freqIndex = it->first & 1023;
            const auto phaseIndex = GhaPhaseToIndex(it->second.phase);
            const auto ampSf = AmplitudeToSf(it->second.magnitude);

            waves.WaveSbInfos[sb].WaveNums++;
            waves.WaveParams.push_back(TAt3PGhaData::TWaveParam{freqIndex, ampSf, 1, phaseIndex});
            it++;
        }

        if (waves.WaveSbInfos[sb].WaveNums > 0) {
            waves.WaveSbInfos[sb].WaveIndex = index;

            uint32_t histStop = TAt3PGhaData::INIT;
            if (ResultBufHistory.Waves[0].WaveSbInfos.size() > sb) {
                histStop = ResultBufHistory.Waves[0].WaveSbInfos[sb].Envelope.second;
            }
            AdjustEnvelope(waves.WaveSbInfos[sb].Envelope, data[leader].Envelopes[sb], histStop);
        }
        // else: leave the resize()-provided default {EMPTY_POINT, EMPTY_POINT}
        // -- data[leader].Envelopes[sb] was never written for an empty
        // subband (see the insert/envelope-write invariant), so it must not
        // be passed through AdjustEnvelope here.

        if (data.size() == 2) {
            FillFolowerRes(data[leader].GhaInfos, &data[!leader], folowerIt, sb);
        }
    }
}

uint32_t TGhaProcessorBase::FillFolowerRes(const TGhaInfoMap& lGhaInfos, const TChannelData* src, TGhaInfoMap::const_iterator& it, const uint32_t curSb)
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

uint32_t TGhaProcessorBase::AmplitudeToSf(float amp) const
{
    auto it = std::upper_bound(AmpSfTab.begin(), AmpSfTab.end(), amp);
    if (it != AmpSfTab.begin()) {
        it--;
    }
    return it - AmpSfTab.begin();
}

} // namespace

std::unique_ptr<IGhaProcessor> MakeGhaProcessor0(bool stereo, bool wideband, int refineMode)
{
    if (wideband) {
        return std::unique_ptr<IGhaProcessor>(new TWidebandGhaProcessor(stereo, refineMode));
    }
    return std::unique_ptr<IGhaProcessor>(new TSubbandGhaProcessor(stereo));
}

} // namespace NAtracDEnc
