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
#include "atrac_scale.h"

namespace NAtracDEnc {

float AnalizeScaleFactorSpread(const std::vector<TScaledBlock>& scaledBlocks);
std::vector<float> CalcATH(int len, int sampleRate);

inline float TrackLoudness(float prevLoud, float l0, float l1)
{
    return 0.98 * prevLoud + 0.01 * (l0 + l1);
}

inline float TrackLoudness(float prevLoud, float l)
{
    return 0.98 * prevLoud + 0.02 * l;
}

std::vector<float> CreateLoudnessCurve(size_t sz);

} //namespace NAtracDEnc
