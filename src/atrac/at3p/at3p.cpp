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

#include <atrac3p.h>

#include <atrac/atrac3plus_pqf/atrac3plus_pqf.h>

#include "at3p_bitstream.h"
#include "at3p_gha.h"
#include "at3p_mdct.h"
#include "at3p_tables.h"
#include <atrac/atrac_scale.h>

#include <cassert>
#include <vector>
#include <unordered_map>

using std::vector;

namespace NAtracDEnc {

class TAt3PEnc::TImpl {
public:
    TImpl(ICompressedOutput* out, int channels, TSettings settings)
        : BitStream(out, 2048)
        , ChannelCtx(channels)
        , GhaProcessor(MakeGhaProcessor0(channels == 2))
        , Settings(settings)
    {
        delay.NumToneBands = 0;
    }

    TPCMEngine::EProcessResult EncodeFrame(const float* data, int channels);
private:
    struct TChannelCtx {
        TChannelCtx()
            : PqfCtx(at3plus_pqf_create_a_ctx())
            , Specs(TAt3PEnc::NumSamples)
        {}

        ~TChannelCtx() {
            at3plus_pqf_free_a_ctx(PqfCtx);
        }

        at3plus_pqf_a_ctx_t PqfCtx;

        float* NextBuf = Buf1;
        float* CurBuf = nullptr;
        float Buf1[TAt3PEnc::NumSamples];
        float Buf2[TAt3PEnc::NumSamples];
        float PrevBuf[TAt3PEnc::NumSamples];
        TAt3pMDCT::THistBuf MdctBuf;
        std::vector<float> Specs;
    };

    TAt3pMDCT Mdct;
    TScaler<NAt3p::TScaleTable> Scaler;
    TAt3PBitStream BitStream;
    vector<TChannelCtx> ChannelCtx;
    std::unique_ptr<IGhaProcessor> GhaProcessor;
    TAt3PGhaData delay;
    const TSettings Settings;
};

TPCMEngine::EProcessResult TAt3PEnc::TImpl::
EncodeFrame(const float* data, int channels)
{
    int needMore = 0;
    for (int ch = 0; ch < channels; ch++) {
        float src[TAt3PEnc::NumSamples];
        for (size_t i = 0; i < NumSamples; ++i) {
            src[i] = data[i * channels  + ch];
        }

        at3plus_pqf_do_analyse(ChannelCtx[ch].PqfCtx, src, ChannelCtx[ch].NextBuf);
        if (ChannelCtx[ch].CurBuf == nullptr) {
            assert(ChannelCtx[ch].NextBuf == ChannelCtx[ch].Buf1);
            ChannelCtx[ch].CurBuf = ChannelCtx[ch].Buf2;
            std::swap(ChannelCtx[ch].NextBuf, ChannelCtx[ch].CurBuf);
            needMore++;
        }
    }

    if (needMore == channels) {
        return TPCMEngine::EProcessResult::LOOK_AHEAD;
    }

    assert(needMore == 0);

    float* b1Prev = ChannelCtx[0].PrevBuf;
    const float* b1Cur = ChannelCtx[0].CurBuf;
    const float* b1Next = ChannelCtx[0].NextBuf;
    float* b2Prev = (channels == 2) ? ChannelCtx[1].PrevBuf : nullptr;
    const float* b2Cur = (channels == 2) ? ChannelCtx[1].CurBuf : nullptr;
    const float* b2Next = (channels == 2) ? ChannelCtx[1].NextBuf : nullptr;


    const TAt3PGhaData* p = nullptr;
    if (delay.NumToneBands) {
        p = &delay;
    }

    const TAt3PGhaData* tonalBlock = GhaProcessor->DoAnalize({b1Cur, b1Next}, {b2Cur, b2Next}, b1Prev, b2Prev);

    std::vector<std::vector<TScaledBlock>> scaledBlocks;
    for (int ch = 0; ch < channels; ch++) {
        float* x = (ch == 0) ? b1Prev : b2Prev;
        auto& c = ChannelCtx[ch];
        TAt3pMDCT::TPcmBandsData p;
        float tmp[2048];
        //TODO: scale window
        if (Settings.UseGha & TSettings::GHA_WRITE_RESIUDAL) {
            for (size_t i = 0; i < 2048; i++) {
                //TODO: find why we need to add the 0.5db
                tmp[i] = x[i] / (32768.0 / 1.122018);
            }
        } else {
            for (size_t i = 0; i < 2048; i++) {
                tmp[i] = 0.0;
            }
        }
        for (size_t b = 0; b < 16; b++) {
            p[b] = tmp + b * 128;
        }
        Mdct.Do(c.Specs.data(), p, c.MdctBuf);

        const auto& block = Scaler.ScaleFrame(c.Specs, NAt3p::TScaleTable::TBlockSizeMod());
        scaledBlocks.push_back(block);
    }

    BitStream.WriteFrame(channels, p, scaledBlocks);

    for (int ch = 0; ch < channels; ch++) {
        if (Settings.UseGha & TSettings::GHA_PASS_INPUT) {
            memcpy(ChannelCtx[ch].PrevBuf, ChannelCtx[ch].CurBuf, sizeof(float) * TAt3PEnc::NumSamples);
        } else {
            memset(ChannelCtx[ch].PrevBuf, 0, sizeof(float) * TAt3PEnc::NumSamples);
        }
        std::swap(ChannelCtx[ch].NextBuf, ChannelCtx[ch].CurBuf);
    }
    if (tonalBlock && (Settings.UseGha & TSettings::GHA_WRITE_TONAL)) {
        delay = *tonalBlock;
    } else {
        delay.NumToneBands = 0;
    }

    return TPCMEngine::EProcessResult::PROCESSED;
}

TAt3PEnc::TAt3PEnc(TCompressedOutputPtr&& out, int channels, TSettings settings)
    : Out(std::move(out))
    , Channels(channels)
    , Impl(new TImpl(Out.get(), Channels, settings))
{
}

TPCMEngine::TProcessLambda TAt3PEnc::GetLambda() {
    return [this](float* data, const TPCMEngine::ProcessMeta&) {
        return Impl->EncodeFrame(data, Channels);
    };
}

static void SetGha(const std::string& str, TAt3PEnc::TSettings& settings) {
    int mask = std::stoi(str);
    if (mask > 7 || mask < 0) {
        throw std::runtime_error("invalud value of GHA processing mask");
    }

    if (mask & TAt3PEnc::TSettings::GHA_PASS_INPUT)
        std::cerr << "GHA_PASS_INPUT" << std::endl;
    if (mask & TAt3PEnc::TSettings::GHA_WRITE_RESIUDAL)
        std::cerr << "GHA_WRITE_RESIUDAL" << std::endl;
    if (mask & TAt3PEnc::TSettings::GHA_WRITE_TONAL)
        std::cerr << "GHA_WRITE_TONAL" << std::endl;

    settings.UseGha = mask;
}



void TAt3PEnc::ParseAdvancedOpt(const char* opt, TSettings& settings) {
    typedef void (*processFn)(const std::string& str, TSettings& settings);
    static std::unordered_map<std::string, processFn> keys {
        {"ghadbg", &SetGha}
    };

    if (opt == nullptr)
        return;

    const char* start = opt;
    bool vState = false; //false - key state, true - value state
    processFn handler = nullptr;

    while (opt) {
        if (!vState) {
            if (*opt == ',') {
                throw std::runtime_error("unexpected \",\" just after key.");
//                if (opt - start > 0) {
//                }
//                opt++;
//                start = opt;
            } else if (*opt == '=') {
                auto it = keys.find(std::string(start, opt - start));
                if (it == keys.end()) {
                    throw std::runtime_error(std::string("unexpected advanced option \"")
                        + std::string(start, opt - start));
                }
                handler = it->second;
                vState = true;
                opt++;
                start = opt;
            } else if (!*opt) {
                throw std::runtime_error("unexpected end of key token");
//                if (opt - start > 0) {
//                }
//                opt = nullptr;
            } else {
                opt++;
            }
        } else {
            if (*opt == ',') {
                if (opt - start > 0) {
                    handler(std::string(start, opt - start), settings);
                }
                opt++;
                start = opt;
                vState = false;
            } else if (*opt == '=') {
                throw std::runtime_error("unexpected \"=\" inside value token.");
            } else if (!*opt) {
                if (opt - start > 0) {
                    handler(std::string(start, opt - start), settings);
                }
                opt = nullptr;
            } else {
                opt++;
            }
        }
    }
}

}
