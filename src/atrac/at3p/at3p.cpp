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

using std::vector;

namespace NAtracDEnc {

class TAt3PEnc::TImpl {
public:
    TImpl(ICompressedOutput* out, int channels)
        : BitStream(out, 2048)
        , ChannelCtx(channels)
        , GhaProcessor(MakeGhaProcessor0(channels == 2))
    {}

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
        TAt3pMDCT::THistBuf MdctBuf;
        std::vector<float> Specs;
    };

    TAt3pMDCT Mdct;
    TScaler<NAt3p::TScaleTable> Scaler;
    TAt3PBitStream BitStream;
    vector<TChannelCtx> ChannelCtx;
    std::unique_ptr<IGhaProcessor> GhaProcessor;
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

    const float* b1Cur = ChannelCtx[0].CurBuf;
    const float* b1Next = ChannelCtx[0].NextBuf;
    const float* b2Cur = (channels == 2) ? ChannelCtx[1].CurBuf : nullptr;
    const float* b2Next = (channels == 2) ? ChannelCtx[1].NextBuf : nullptr;

    auto tonalBlock = GhaProcessor->DoAnalize({b1Cur, b1Next}, {b2Cur, b2Next});

    std::vector<std::vector<TScaledBlock>> scaledBlocks;
    for (int ch = 0; ch < channels; ch++) {
        auto& c = ChannelCtx[ch];
        TAt3pMDCT::TPcmBandsData p;
        float tmp[2048];
        //TODO: scale window
        for (size_t i = 0; i < 2048; i++) {
            tmp[i] = c.CurBuf[i] / 32768.0;
        }
        for (size_t b = 0; b < 16; b++) {
            p[b] = tmp + b * 128;
        }
        Mdct.Do(c.Specs.data(), p, c.MdctBuf);

        const auto& block = Scaler.ScaleFrame(c.Specs, NAt3p::TScaleTable::TBlockSizeMod());
        scaledBlocks.push_back(block);
    }

    BitStream.WriteFrame(channels, tonalBlock, scaledBlocks);

    for (int ch = 0; ch < channels; ch++) {
        std::swap(ChannelCtx[ch].NextBuf, ChannelCtx[ch].CurBuf);
    }

    return TPCMEngine::EProcessResult::PROCESSED;
}

TAt3PEnc::TAt3PEnc(TCompressedOutputPtr&& out, int channels)
    : Out(std::move(out))
    , Channels(channels)
{
}

TPCMEngine::TProcessLambda TAt3PEnc::GetLambda() {
    Impl.reset(new TImpl(Out.get(), Channels));

    return [this](float* data, const TPCMEngine::ProcessMeta&) {
        return Impl->EncodeFrame(data, Channels);
    };
}

}
