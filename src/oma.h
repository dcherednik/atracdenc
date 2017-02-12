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

#pragma once

#include "compressed_io.h"
#include "oma/liboma/include/oma.h"


class TOma : public ICompressedIO {
    OMAFILE* File;
public:
    TOma(const std::string& filename, const std::string& title, int numChannel, uint32_t numFrames, int cid, uint32_t framesize);
    ~TOma();
    std::unique_ptr<TFrame> ReadFrame() override;
    void WriteFrame(std::vector<char> data) override;
    std::string GetName() const override;
    int GetChannelNum() const override;
    long long GetLengthInSamples() const override;
};
