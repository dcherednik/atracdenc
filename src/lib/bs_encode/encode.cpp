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

#include "encode.h"

#include <iostream>

namespace NAtracDEnc {

using NBitStream::TBitStream;

class TBitStreamEncoder::TImpl : public TBitAllocHandler {
public:
    explicit TImpl(std::vector<IBitStreamPartEncoder::TPtr>&& encoders);
    void DoStart(size_t targetBits, float minLambda, float maxLambda) noexcept;
    float DoContinue() noexcept;
    void DoSubmit(size_t gotBits) noexcept;
    bool DoCheck(size_t gotBits) const noexcept;
    void DoRun(void* frameData, TBitStream& bs);
    uint32_t DoGetCurGlobalConsumption() const noexcept;
private:
    std::vector<IBitStreamPartEncoder::TPtr> Encoders;
    size_t CurEncPos;
    size_t RepeatEncPos;

    size_t TargetBits;
    float MinLambda;
    float MaxLambda;
    float CurLambda;
    float LastLambda;

    bool NeedRepeat = false;
};

TBitStreamEncoder::TImpl::TImpl(std::vector<IBitStreamPartEncoder::TPtr>&& encoders)
    : Encoders(std::move(encoders))
    , CurEncPos(0)
    , RepeatEncPos(0)
{
}

void TBitStreamEncoder::TImpl::DoStart(size_t targetBits, float minLambda, float maxLambda) noexcept
{
    TargetBits = targetBits;
    MinLambda = minLambda;
    MaxLambda = maxLambda;
}

float TBitStreamEncoder::TImpl::DoContinue() noexcept
{
    if (MaxLambda <= MinLambda) {
        return LastLambda;
    }

    CurLambda = (MaxLambda + MinLambda) / 2.0;
    RepeatEncPos = CurEncPos;
    return CurLambda;
}

void TBitStreamEncoder::TImpl::DoSubmit(size_t gotBits) noexcept
{
    if  (MaxLambda <= MinLambda) {
        NeedRepeat = false;
    } else {
        if (gotBits < TargetBits) {
            LastLambda = CurLambda;
            MaxLambda = CurLambda - 0.01f;
            NeedRepeat = true;
        } else if (gotBits > TargetBits) {
            MinLambda = CurLambda + 0.01f;
            NeedRepeat = true;
        } else {
            NeedRepeat = false;
        }
    }
}

bool TBitStreamEncoder::TImpl::DoCheck(size_t gotBits) const noexcept
{
    return gotBits < TargetBits;
}

void TBitStreamEncoder::TImpl::DoRun(void* frameData, TBitStream& bs)
{
    bool cont = false;
    do {
        for (CurEncPos = RepeatEncPos; CurEncPos < Encoders.size(); CurEncPos++) {
            auto status = Encoders[CurEncPos]->Encode(frameData, *this);
            if (NeedRepeat) {
                NeedRepeat = false;
                cont = true;
                break;
            } else {
                cont = false;
            }
            if (status == IBitStreamPartEncoder::EStatus::Repeat) {
                cont = true;
                for (size_t i = 0; i < CurEncPos; i++) {
                    Encoders[i]->Reset();
                }
                RepeatEncPos = 0;
                break;
            }
        }
    } while (cont);

    for (size_t i = 0; i < Encoders.size(); i++) {
        Encoders[i]->Dump(bs);
    }
}

uint32_t TBitStreamEncoder::TImpl::DoGetCurGlobalConsumption() const noexcept
{
    uint32_t consumption = 0;
    for (size_t i = 0; i < CurEncPos; i++) {
        consumption += Encoders[i]->GetConsumption();
    }
    return consumption;
}

/////

TBitStreamEncoder::TBitStreamEncoder(std::vector<IBitStreamPartEncoder::TPtr>&& encoders)
    : Impl(new TBitStreamEncoder::TImpl(std::move(encoders)))
{}

TBitStreamEncoder::~TBitStreamEncoder()
{
    delete Impl;
}

void TBitStreamEncoder::Do(void* frameData, TBitStream& bs)
{
    Impl->DoRun(frameData, bs);
}

/////

void TBitAllocHandler::Start(size_t targetBits, float minLambda, float maxLambda) noexcept
{
    TBitStreamEncoder::TImpl* self = static_cast<TBitStreamEncoder::TImpl*>(this);
    self->DoStart(targetBits, minLambda, maxLambda);
}

float TBitAllocHandler::Continue() noexcept
{
    TBitStreamEncoder::TImpl* self = static_cast<TBitStreamEncoder::TImpl*>(this);
    return self->DoContinue();
}

void TBitAllocHandler::Submit(size_t gotBits) noexcept
{
    TBitStreamEncoder::TImpl* self = static_cast<TBitStreamEncoder::TImpl*>(this);
    self->DoSubmit(gotBits);
}

bool TBitAllocHandler::Check(size_t gotBits) const noexcept
{
    const TBitStreamEncoder::TImpl* self = static_cast<const TBitStreamEncoder::TImpl*>(this);
    return self->DoCheck(gotBits);
}

uint32_t TBitAllocHandler::GetCurGlobalConsumption() const noexcept
{
    const TBitStreamEncoder::TImpl* self = static_cast<const TBitStreamEncoder::TImpl*>(this);
    return self->DoGetCurGlobalConsumption();
}

}
