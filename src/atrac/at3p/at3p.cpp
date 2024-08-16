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

#include <vector>

using std::vector;

namespace NAtracDEnc {

class TAt3PEnc::TImpl {
public:
    TImpl(ICompressedOutput* out, int channels)
        : BitStream(out, 2048)
        , ChannelCtx(channels)
        , GhaProcessor(MakeGhaProcessor0(ChannelCtx[0].SubbandsBuf, channels == 2 ? ChannelCtx[1].SubbandsBuf : nullptr))
    {}

    void EncodeFrame(const TFloat* data, int channels);
private:
    struct TChannelCtx {
        TChannelCtx()
            : PqfCtx(at3plus_pqf_create_a_ctx())
        {}

        ~TChannelCtx() {
            at3plus_pqf_free_a_ctx(PqfCtx);
        }

        at3plus_pqf_a_ctx_t PqfCtx;
        TFloat SubbandsBuf[TAt3PEnc::NumSamples];
    };

    TAt3PBitStream BitStream;
    vector<TChannelCtx> ChannelCtx;
    std::unique_ptr<IGhaProcessor> GhaProcessor;
};

void TAt3PEnc::TImpl::
EncodeFrame(const TFloat* data, int channels)
{

    for (int ch = 0; ch < channels; ch++) {
        float src[TAt3PEnc::NumSamples];
        for (size_t i = 0; i < NumSamples; ++i) {
            src[i] = data[i * channels  + ch];
        }

        at3plus_pqf_do_analyse(ChannelCtx[ch].PqfCtx, src, ChannelCtx[ch].SubbandsBuf);
    }

    auto tonalBlock = GhaProcessor->DoAnalize();
    BitStream.WriteFrame(channels, tonalBlock);
}

TAt3PEnc::TAt3PEnc(TCompressedOutputPtr&& out, int channels)
    : Out(std::move(out))
    , Channels(channels)
{
}

TPCMEngine<TFloat>::TProcessLambda TAt3PEnc::GetLambda() {
    Impl.reset(new TImpl(Out.get(), Channels));

    return [this](TFloat* data, const TPCMEngine<TFloat>::ProcessMeta&) {
        Impl->EncodeFrame(data, Channels);
    };
}

}
