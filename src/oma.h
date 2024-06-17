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

#include "lib/liboma/include/oma.h"

class TOma : public ICompressedOutput {
    OMAFILE* File;
public:
    TOma(const std::string& filename, const std::string& title, uint8_t numChannel,
        uint32_t numFrames, int cid, uint32_t framesize, bool jointStereo);
    ~TOma();
    void WriteFrame(std::vector<char> data) override;
    std::string GetName() const override;
    uint8_t GetChannelNum() const override;
};
