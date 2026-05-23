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

#include "raw.h"

#include "file.h"
#include "utf8_file.h"
#include <memory>
#include <stdexcept>

namespace {

class TRaw : public ICompressedOutput {
public:
    TRaw(const std::string& filename, size_t numChannels, uint32_t frameSize)
        : Fp(NAtracDEnc::FOpenUtf8(filename, "wb"))
        , NumChannels(numChannels)
        , FrameSize(frameSize)
    {
        if (!Fp) {
            throw std::runtime_error("unable to open output file '" + filename + "'");
        }
    }

    void WriteFrame(std::vector<char> data) override {
        if (FrameSize) {
            data.resize(FrameSize); // zero-pads short frames; truncates trailing bitstream rounding bytes
        }
        if (fwrite(data.data(), 1, data.size(), Fp.get()) != data.size()) {
            throw std::runtime_error("Cannot write raw ATRAC data to file");
        }
    }

    std::string GetName() const override {
        return {};
    }

    size_t GetChannelNum() const override {
        return NumChannels;
    }

private:
    TFilePtr Fp;
    size_t NumChannels;
    uint32_t FrameSize;
};

} // namespace

TCompressedOutputPtr
CreateRawOutput(const std::string& filename, size_t numChannels, uint32_t frameSize)
{
    return std::unique_ptr<TRaw>(new TRaw(filename, numChannels, frameSize));
}
