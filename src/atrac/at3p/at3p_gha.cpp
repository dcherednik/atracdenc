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

#include <assert.h>
#include <atrac/atrac_psy_common.h>
#include <libgha/include/libgha.h>

#include <memory>

#include <algorithm>
#include <cstring>
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
    return static_cast<uint32_t>(lrintf(32.0 * (p / (2 * M_PI))) & 31);
}

class TGhaProcessor : public IGhaProcessor {
    // Number of subbands to process;
    // No need to process all subbands.
    static const size_t SUBBANDS = 13;
    static const size_t CHANNEL_BUF_SZ = SUBBANDS * 128;;

    using TGhaInfoMap = map<uint32_t, struct gha_info>;
    using TWavesChannel = TAt3PGhaData::TWavesChannel;
    using TAmpSfTab = std::array<float, 64>;

    struct TChannelData {
        const float* SrcBuf;
        float Buf[CHANNEL_BUF_SZ];
        pair<uint32_t, uint32_t> Envelopes[SUBBANDS] = {{0,0}};
        uint8_t SubbandDone[SUBBANDS] = {0};
        TGhaInfoMap GhaInfos;

        void MarkSubbandDone(size_t sb) {
            SubbandDone[sb] = 16;
        }

        bool IsSubbandDone(size_t sb) const {
            return SubbandDone[sb] == 16;
        }
    };

public:
    TGhaProcessor(float* b1, float* b2)
        : B1(b1)
        , B2(b2)
        , LibGhaCtx(gha_create_ctx(128))
        , AmpSfTab(CreateAmpSfTab())
    {
        FillSubbandAth(&SubbandAth[0]);
    }

    const TAt3PGhaData* DoAnalize() override;
private:
    static void FillSubbandAth(float* out) {
        const auto ath = CalcATH(16 * 1024, 44100);
        #pragma GCC nounroll
        for (size_t sb = 0; sb < SUBBANDS; sb++) {
            float m = 999.;
            for (size_t f = sb * 1024, i = 0; i < 1024; f++, i++) {
                m = fmin(m, ath[f]);
            }
            m += 26; //Some gap to not encode too much
            out[sb] = pow(10, 0.1 * (m + 90)); //adjust to 0db level = 32768, convert to power
        }
    }

    static TAmpSfTab CreateAmpSfTab() {
        TAmpSfTab AmpSfTab;
        for (int i = 0; i < (int)AmpSfTab.size(); i++) {
            AmpSfTab[i] = exp2f((i - 3) / 4.0f);
        }
        return AmpSfTab;
    }

    uint32_t FillFolowerRes(const TGhaInfoMap& lGha, const TGhaInfoMap& fGha, TGhaInfoMap::const_iterator& it, uint32_t leaderSb);

    uint32_t AmplitudeToSf(float amp);

    bool DoRound(TChannelData& data, size_t& totalTones) const;
    bool PsyPreCheck(size_t sb, const struct gha_info& gha) const;
    pair<uint32_t, uint32_t> FindPos(const float* src, const float* analized, float magn, uint32_t freqIdx) const;
    void FillResultBuf(const vector<TChannelData>& data);

    bool IsStereo() const {
        return (bool)B2;
    }

    float* const B1;
    float* const B2;
    gha_ctx_t LibGhaCtx;
    const TAmpSfTab AmpSfTab;
    float SubbandAth[SUBBANDS];
    TAt3PGhaData ResultBuf;
};

const TAt3PGhaData* TGhaProcessor::DoAnalize()
{
    vector<TChannelData> data((size_t)IsStereo() + 1);
    bool progress[2] = {false};

    for (size_t ch = 0; ch < data.size(); ch++) {
        const float* b = (ch == 0) ? B1 : B2;
        data[ch].SrcBuf = b;
        memcpy(&data[ch].Buf[0], b, sizeof(float) * CHANNEL_BUF_SZ);
    }

    size_t totalTones = 0;
    do {
        for (size_t ch = 0; ch < data.size(); ch++) {
            progress[ch] = DoRound(data[ch], totalTones);
        }
    } while ((progress[0] || progress[1]) && totalTones < 48);

    if (totalTones == 0) {
        return nullptr;
    }

    FillResultBuf(data);

    return &ResultBuf;
}

bool TGhaProcessor::DoRound(TChannelData& data, size_t& totalTones) const
{
    bool progress = false;
    for (size_t sb = 0; sb < SUBBANDS; sb++) {
        if (data.IsSubbandDone(sb)) {
            continue;
        }

        if (totalTones > 48) {
            return false;
        }

        float* b = &data.Buf[sb * 128];
        struct gha_info res;

        gha_analyze_one(b, &res, LibGhaCtx);

#if 0
        {
            const float* srcB = data.SrcBuf + (sb * 128);
            auto it = data.GhaInfos.lower_bound(sb << 10);
            vector<gha_info> tmp;
            for(; it != data.GhaInfos.end() && it->first < (sb + 1) << 10; it++) {
                tmp.push_back(it->second);
            }
            if (tmp.size() > 1) {
                for (const auto& x : tmp) {
                    std::cerr << "to adjust: " << x.frequency << " " << x.magnitude << std::endl;
                }
                int ar = gha_adjust_info(srcB, tmp.data(), tmp.size(), LibGhaCtx);
                for (const auto& x : tmp) {
                    std::cerr << "after adjust: " << x.frequency << " " << x.magnitude << " ar: " << ar << std::endl;
                }

            }
        }
#endif
        auto freqIndex = GhaFreqToIndex(res.frequency, sb);
        //std::cerr << "sb: " << sb << " findex: " << freqIndex << " magn " << res.magnitude <<  std::endl;
        if (PsyPreCheck(sb, res) == false) {
            data.MarkSubbandDone(sb);
            //std::cerr << "PreCheck failed" << std::endl;
        } else {
            const float* tone = gha_get_analyzed(LibGhaCtx);
            auto pos = FindPos(b, tone, res.magnitude, freqIndex);
            if (pos.second == 0) {
                data.MarkSubbandDone(sb);
            } else {
                auto& envelope = data.Envelopes[sb];
                // Envelope for whole subband not initialized
                if (envelope.second == 0) {
                    assert(data.SubbandDone[sb] == 0);
                    envelope = pos;
                    bool ins = data.GhaInfos.insert({freqIndex, res}).second;
                    assert(ins);
                } else {
                    //TODO:
                    // Nth sine for subband was out of existed envelope. This case requires more
                    // complicated analisys - just skip it for a while
                    if ((pos.first < envelope.first && pos.second <= envelope.first) ||
                        (pos.first >= envelope.second)) {
                        data.MarkSubbandDone(sb);
                        continue;
                    }

                    const auto it = data.GhaInfos.lower_bound(freqIndex);

                    const size_t minFreqDistanse = 40; // Now we unable to handle tones with close frequency
                    if (it != data.GhaInfos.end()) {
                        if (it->first == freqIndex) {
                            //std::cerr << "already added: " << freqIndex << std::endl;
                            data.MarkSubbandDone(sb);
                            continue;
                        }
                        //std::cerr << "right: " << it->first << " " << freqIndex << std::endl;
                        if (it->first - freqIndex < minFreqDistanse) {
                            data.MarkSubbandDone(sb);
                            continue;
                        }
                    }
                    if (it != data.GhaInfos.begin()) {
                        auto prev = it;
                        prev--;
                        //std::cerr << "left: " << prev->first << " " << freqIndex << std::endl;
                        if (freqIndex - prev->first < minFreqDistanse) {
                            data.MarkSubbandDone(sb);
                            continue;
                        }
                    }
                    data.GhaInfos.insert(it, {freqIndex, res});
                    envelope.first = max(envelope.first, pos.first);
                    envelope.second = min(envelope.second, pos.second);
                }
                // Here we have updated envelope, we are ready to add extracted tone

                for (size_t j = envelope.first; j < envelope.second; j++) {
                    b[j] -= tone[j] * res.magnitude;
                }

                data.SubbandDone[sb]++;
                totalTones++;
                progress = true;

                //std::cerr << "envelop " << envelope.first << " " << envelope.second << std::endl;
            }
        }

    }
    return progress;
}

bool TGhaProcessor::PsyPreCheck(size_t sb, const struct gha_info& gha) const
{
    if (isnan(gha.magnitude)) {
        return false;
    }

    //std::cerr << "sb: " << sb << " " << gha.magnitude << " ath: " << SubbandAth[sb] << std::endl;
    // TODO: improve it
    // Just to start. Actualy we need to consider spectral leakage during MDCT
    if ((gha.magnitude * gha.magnitude) > SubbandAth[sb]) {
        return true;
    } else {
        return false;
    }
    return true;
}

pair<uint32_t, uint32_t> TGhaProcessor::FindPos(const float* src, const float* tone, float magn, uint32_t freqIdx) const
{
    freqIdx &= 1023; // clear subband part
    size_t windowSz = 64;
    // TODO: place to make experiments
    // we should compare rms on some window. Optimal window sile depends on frequency so right now
    // here just some heuristic to match window size to freq index

    // Ideas:
    // - generate FIR filter for each frequncy index and use it fo filter source signal before compare level
    // - may be just perform search all sinusoid components, run N dimension GHA optimization, and then find positions
    if (freqIdx > 256) {
        windowSz = 8;
    } else if (freqIdx > 128) {
        windowSz = 16;
    } else if (freqIdx > 64) {
        windowSz = 32;
    } else {
        windowSz = 64;
    }

    uint32_t start = 0;
    uint32_t count = 0;
    uint32_t len = 0;
    bool found = false;

    for (size_t i = 0; i < 128; i += windowSz) {
        float rmsIn = 0.0;
        float rmsOut = 0.0;
        for (size_t j = 0; j < windowSz; j++) {
            rmsIn += src[i + j] * src[i + j];
            float r = src[i + j] - tone[i + j] * magn;
            rmsOut += r * r;
        }
        rmsIn /= windowSz;
        rmsOut /= windowSz;
        rmsIn = sqrt(rmsIn);
        rmsOut = sqrt(rmsOut);

        //std::cerr << i << " rms : " << rmsIn << " " << rmsOut  << "\t\t\t" << ((rmsOut < rmsIn) ? "+" : "-")  << std::endl;
        if (rmsIn / rmsOut < 1) {
            count = 0;
            found = false;
        } else {
            count++;
            if (count > len) {
                len = count;
                if (!found) {
                    start = i;
                    found = true;
                }
            }
        }

    }

    //std::cerr << "start pos: " << start << " len: " << len * windowSz << " end: " << start + len * windowSz << std::endl;

    auto end = start + len * windowSz;

    //if (end - start < 64) {
    //    return {0, 0};
    //}

    return {start, end};
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

    ResultBuf.SecondIsLeader = leader;
    ResultBuf.NumToneBands = usedContiguousSb[leader];

    TGhaInfoMap::const_iterator leaderStartIt;
    TGhaInfoMap::const_iterator folowerIt;
    if (data.size() == 2) {
        TWavesChannel& fWaves = ResultBuf.Waves[1];
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
    uint32_t nextSb = 0;
    uint32_t index = 0;

    uint32_t nextFolowerSb = 1;
    while (it != ghaInfos.end()) {
        const auto sb = ((it->first) >> 10);
        if (sb >= usedContiguousSb[leader]) {
            break;
        }

        const auto freqIndex = it->first & 1023;
        const auto phaseIndex = GhaPhaseToIndex(it->second.phase);
        const auto ampSf = AmplitudeToSf(it->second.magnitude);

        waves.WaveSbInfos[sb].WaveNums++;
        if (sb != nextSb) {
            // cur subband done
            // update index sb -> wave position index
            waves.WaveSbInfos[sb].WaveIndex = index;

            // process folower if present
            if (data.size() == 2) {
                nextFolowerSb = FillFolowerRes(data[leader].GhaInfos, data[!leader].GhaInfos, folowerIt, nextSb);
                leaderStartIt = it;
            }

            nextSb = sb;
        }
        waves.WaveParams.push_back(TAt3PGhaData::TWaveParam{freqIndex, ampSf, 1, phaseIndex});
        it++;
        index++;
    }

    if (data.size() == 2 && nextFolowerSb <= usedContiguousSb[leader]) {
        FillFolowerRes(data[leader].GhaInfos, data[!leader].GhaInfos, folowerIt, nextSb);
    }
}

uint32_t TGhaProcessor::FillFolowerRes(const TGhaInfoMap& lGhaInfos, const TGhaInfoMap& fGhaInfos, TGhaInfoMap::const_iterator& it, const uint32_t curSb)
{
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
    }
    return nextSb;
}

uint32_t TGhaProcessor::AmplitudeToSf(float amp)
{
    auto it = std::upper_bound(AmpSfTab.begin(), AmpSfTab.end(), amp);
    if (it != AmpSfTab.begin()) {
        it--;
    }
    return it - AmpSfTab.begin();
}

} // namespace

std::unique_ptr<IGhaProcessor> MakeGhaProcessor0(float* b1, float* b2)
{
    return std::unique_ptr<TGhaProcessor>(new TGhaProcessor(b1, b2));
}

} // namespace NAtracDEnc
