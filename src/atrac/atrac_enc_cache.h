#pragma once

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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace NAtracDEnc {

// Codec-agnostic base for a single cached encoding unit (BFU / quant unit).
//
// A codec subclasses TUnit and quantizes the scaled spectrum into Mantisas,
// filling the bookkeeping fields needed to later write the unit into the
// stream. The actual computation lives in the user-supplied ProvideUnit
// factory (see TEncCache) so the cached hot path carries no extra virtual
// dispatch. The result is produced once per cache lifetime for a given key
// and then reused across the bit-allocation search.
class TUnit {
public:
    virtual ~TUnit() = default;

    const std::vector<int>& GetMantisas() const { return Mantisas; }
    uint32_t GetWordlen() const { return Wordlen; }
    float GetMultiplier() const { return Multiplier; }
    uint16_t GetConsumedBits() const { return ConsumedBits; }

protected:
    // Info needed to write the unit into the stream after encoding.
    std::vector<int> Mantisas;
    uint32_t Wordlen = 0;
    float Multiplier = 1.0f;
    uint16_t ConsumedBits = 0; // Number of bits consumed by the quantized spectrum
};

// Caches per-unit encoding results during the bit-allocation search.
//
// Within a single frame the scaled spectrum of a given (ch, bfu, wordlen)
// is fixed, so its quantized mantissas and bit consumption are deterministic.
// The binary search requests the same combinations repeatedly; this cache
// computes each one once.
//
// The key space is small and dense, so units are stored in a vector that is
// directly indexed by a user-supplied key function (nullptr slot == not yet
// computed) rather than in a std::map.
class TEncCache {
public:
    // Build the right TUnit subclass for this key and quantize `values` into
    // it. Invoked only on a cache miss. `opaque` carries user context
    // (e.g. scale tables / per-frame data).
    using TProvideUnit = TUnit* (*)(size_t ch, size_t bfu, size_t wordlen,
                                    const float* values, void* opaque);

    // Pack (ch, bfu, wordlen) into a dense vector index. Codec specific.
    using TMakeKey = size_t (*)(size_t ch, size_t bfu, size_t wordlen);

    // `numKeys` is the upper bound on MakeKey() values; the backing vector is
    // sized once to it. ProvideUnit/MakeKey must agree on this bound.
    TEncCache(size_t numKeys, TProvideUnit provideUnit, TMakeKey makeKey, void* opaque = nullptr);

    TEncCache(const TEncCache&) = delete;
    TEncCache& operator=(const TEncCache&) = delete;

    // Return the cached unit for (ch, bfu, wordlen), creating and computing
    // it via ProvideUnit on the first request.
    TUnit* GetOrCompute(size_t ch, size_t bfu, size_t wordlen, const float* values);

    // Drop all cached units. Call at frame boundaries.
    void Reset();

private:
    std::vector<std::unique_ptr<TUnit>> UnitBuffers; // direct-indexed by MakeKey()
    TProvideUnit ProvideUnit;
    TMakeKey MakeKey;
    void* Opaque;
};

} // namespace NAtracDEnc
