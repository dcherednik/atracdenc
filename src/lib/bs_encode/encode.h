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

#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

namespace NBitStream {
class TBitStream;
}

namespace NAtracDEnc {

class TBitAllocHandler {
public:
    void Start(size_t targetBits, float minLambda, float maxLambda) noexcept;
    float Continue() noexcept;
    bool Check(size_t gitBits) const noexcept;
    void Submit(size_t gotBits) noexcept;

    // Returns consumption of all previous encoded parts (except part from this method called)
    uint32_t GetCurGlobalConsumption() const noexcept;
};

class IBitStreamPartEncoder {
public:
    using TPtr = std::unique_ptr<IBitStreamPartEncoder>;
    enum class EStatus {
        Ok, // Ok, go to the next stage
        Repeat, // Repeat from first stage
    };

    virtual ~IBitStreamPartEncoder() = default;
    virtual EStatus Encode(void* frameData, TBitAllocHandler& ba) = 0;
    virtual void Dump(NBitStream::TBitStream& bs) = 0;
    virtual void Reset() noexcept {};
    virtual uint32_t GetConsumption() const noexcept = 0;
};

class TBitStreamEncoder {
public:
    class TImpl;
    explicit TBitStreamEncoder(std::vector<IBitStreamPartEncoder::TPtr>&& encoders);
    ~TBitStreamEncoder();

    void Do(void* frameData, NBitStream::TBitStream& bs);
    TBitStreamEncoder(const TBitStreamEncoder&) = delete;
    TBitStreamEncoder& operator=(const TBitStreamEncoder&) = delete;
private:
    std::vector<IBitStreamPartEncoder::TPtr> Encoders;
    TImpl* Impl;
};

}
