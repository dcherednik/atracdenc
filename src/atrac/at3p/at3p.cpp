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

#include "at3p_bitstream.h"

namespace NAtracDEnc {

class TAt3PEnc::TImpl {
public:
    TImpl(ICompressedOutput* out)
        : BitStream(out, 2048)
    {}
    void EncodeFrame(const TFloat* data, int channels);
private:
    TAt3PBitStream BitStream;
};

void TAt3PEnc::TImpl::
EncodeFrame(const TFloat* data, int channels)
{
    BitStream.WriteFrame(channels);
}

TAt3PEnc::TAt3PEnc(TCompressedOutputPtr&& out, int channels)
    : Out(std::move(out))
    , Channels(channels)
{
}

TPCMEngine<TFloat>::TProcessLambda TAt3PEnc::GetLambda() {
    Impl.reset(new TImpl(Out.get()));

    return [this](TFloat* data, const TPCMEngine<TFloat>::ProcessMeta&) {
        Impl->EncodeFrame(data, Channels);
    };
}

}
