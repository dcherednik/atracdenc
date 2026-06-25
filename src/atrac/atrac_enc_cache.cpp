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

#include "atrac_enc_cache.h"

namespace NAtracDEnc {

TEncCache::TEncCache(size_t numKeys, TProvideUnit provideUnit, TMakeKey makeKey, void* opaque)
    : UnitBuffers(numKeys)
    , ProvideUnit(provideUnit)
    , MakeKey(makeKey)
    , Opaque(opaque)
{
}

TUnit* TEncCache::GetOrCompute(size_t ch, size_t bfu, size_t wordlen, const float* values)
{
    const size_t key = MakeKey(ch, bfu, wordlen);

    std::unique_ptr<TUnit>& slot = UnitBuffers[key];
    if (!slot) {
        slot.reset(ProvideUnit(ch, bfu, wordlen, values, Opaque));
    }

    return slot.get();
}

void TEncCache::Reset()
{
    // Keep the vector sized; just drop the cached units for the next frame.
    for (std::unique_ptr<TUnit>& slot : UnitBuffers) {
        slot.reset();
    }
}

} // namespace NAtracDEnc
