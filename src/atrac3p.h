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

#include "config.h"
#include "pcmengin.h"
#include "compressed_io.h"

namespace NAtracDEnc {

class TAt3PEnc : public IProcessor {
public:
    struct TSettings {
        enum GhaProcessingFlags : uint8_t {
            GHA_PASS_INPUT =     1,
            GHA_WRITE_TONAL =    1 << 1,
            GHA_WRITE_RESIUDAL = 1 << 2,

            GHA_ENABLED = GHA_PASS_INPUT | GHA_WRITE_TONAL | GHA_WRITE_RESIUDAL
        };
        uint8_t UseGha;

        TSettings()
            : UseGha(GHA_ENABLED)
        {}
    };
    TAt3PEnc(TCompressedOutputPtr&& out, int channels, TSettings settings);
    TPCMEngine::TProcessLambda GetLambda() override;
    static constexpr int NumSamples = 2048;
    static void ParseAdvancedOpt(const char* opt, TSettings& settings);

private:
    TCompressedOutputPtr Out;
    int Channels;
    class TImpl;
    std::unique_ptr<TImpl> Impl;
};

}
