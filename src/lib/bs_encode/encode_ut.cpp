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
#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <bitstream/bitstream.h>

using namespace NAtracDEnc;

static size_t SomeBitFn1(float lambda) {
    return sqrt(lambda * (-1.0f)) * 300;
}

static size_t SomeBitFn2(float lambda) {
    return 1 + (SomeBitFn1(lambda) & (~(size_t)7));
}

class TPartEncoder1 : public IBitStreamPartEncoder {
public:
    explicit TPartEncoder1(size_t expCalls)
        : ExpCalls(expCalls)
    {}

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        EncCalls++;
        ba.Start(1000, -15, -1);
        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream& bs) override {
         EXPECT_EQ(EncCalls, ExpCalls);
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
private:
    const size_t ExpCalls;
    size_t EncCalls = 0;
};

template<size_t (*F)(float)>
class TPartEncoder2 : public IBitStreamPartEncoder {
public:
    explicit TPartEncoder2(size_t expCalls)
        : ExpCalls(expCalls)
    {}

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        EncCalls++;
        auto lambda = ba.Continue();
        auto bits = F(lambda);
        ba.Submit(bits);
        Bits = bits;
        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream& bs) override {
         EXPECT_EQ(EncCalls, ExpCalls);
         for (size_t i = 0; i < Bits; i++) {
             bs.Write(1, 1);
         }
    }

    uint32_t GetConsumption() const noexcept override {
        return 1 * Bits;
    }
private:
    const size_t ExpCalls;
    size_t EncCalls = 0;
    size_t Bits = 0;
};

class TPartEncoder3 : public IBitStreamPartEncoder {
public:
    explicit TPartEncoder3(size_t expCalls)
        : ExpCalls(expCalls)
    {}

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        EncCalls++;
        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream& bs) override {
         EXPECT_EQ(EncCalls, ExpCalls);
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
private:
    const size_t ExpCalls;
    size_t EncCalls = 0;
};

class TPartEncoder4 : public IBitStreamPartEncoder {
public:
    TPartEncoder4() = default;

    EStatus Encode(void* frameData, TBitAllocHandler& ba) override {
        if (EncCalls == 0) {
            EncCalls++;
            return EStatus::Repeat;
        }

        return EStatus::Ok;
    }

    void Dump(NBitStream::TBitStream& bs) override {
         EXPECT_EQ(EncCalls, 1);
    }

    uint32_t GetConsumption() const noexcept override {
        return 0;
    }
private:
    size_t EncCalls = 0;
};

TEST(BsEncode, SimpleAlloc) {
    std::vector<IBitStreamPartEncoder::TPtr> encoders;
    encoders.emplace_back(std::make_unique<TPartEncoder1>(1));
    encoders.emplace_back(std::make_unique<TPartEncoder2<SomeBitFn1>>(8));
    encoders.emplace_back(std::make_unique<TPartEncoder3>(1));

    NBitStream::TBitStream bs;
    TBitStreamEncoder encoder(std::move(encoders));
    encoder.Do(nullptr, bs);

    EXPECT_EQ(bs.GetSizeInBits(), 1000);
}

TEST(BsEncode, AllocWithRepeat) {
    std::vector<IBitStreamPartEncoder::TPtr> encoders;
    encoders.emplace_back(std::make_unique<TPartEncoder1>(2));
    encoders.emplace_back(std::make_unique<TPartEncoder2<SomeBitFn1>>(16));
    encoders.emplace_back(std::make_unique<TPartEncoder3>(2));
    encoders.emplace_back(std::make_unique<TPartEncoder4>());

    NBitStream::TBitStream bs;
    TBitStreamEncoder encoder(std::move(encoders));
    encoder.Do(nullptr, bs);

    EXPECT_EQ(bs.GetSizeInBits(), 1000);
}

TEST(BsEncode, NotExactAlloc) {
    std::vector<IBitStreamPartEncoder::TPtr> encoders;
    encoders.emplace_back(std::make_unique<TPartEncoder1>(1));
    encoders.emplace_back(std::make_unique<TPartEncoder2<SomeBitFn2>>(11));
    encoders.emplace_back(std::make_unique<TPartEncoder3>(1));

    NBitStream::TBitStream bs;
    TBitStreamEncoder encoder(std::move(encoders));
    encoder.Do(nullptr, bs);

    EXPECT_EQ(bs.GetSizeInBits(), 993);
}


