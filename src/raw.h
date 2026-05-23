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

#include "compressed_io.h"

// Writes raw codec frames with no container header.
// If frameSize > 0, each frame is zero-padded to that exact size (so consumers
// can parse fixed-size frames); a frame larger than frameSize is an error.
// frameSize == 0 means "write frames verbatim, whatever size the encoder produced".
TCompressedOutputPtr
CreateRawOutput(const std::string& filename, size_t numChannels, uint32_t frameSize = 0);
