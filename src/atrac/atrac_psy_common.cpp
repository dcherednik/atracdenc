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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "atrac_psy_common.h"

namespace NAtracDEnc {

//returns 1 for tone-like, 0 - noise-like
TFloat AnalizeScaleFactorSpread(const std::vector<TScaledBlock>& scaledBlocks) {
    TFloat s = 0.0;
    for (size_t i = 0; i < scaledBlocks.size(); ++i) {
        s += scaledBlocks[i].ScaleFactorIndex;
    }
    s /= scaledBlocks.size();
    TFloat sigma = 0.0;
    TFloat t = 0.0;
    for (size_t i = 0; i < scaledBlocks.size(); ++i) {
        t = (scaledBlocks[i].ScaleFactorIndex - s);
        t *= t;
        sigma += t;
    }
    sigma /= scaledBlocks.size();
    sigma = sqrt(sigma);
    if (sigma > 14.0)
        sigma = 14.0;
    return sigma/14.0;
}

} //namespace NAtracDEnc
