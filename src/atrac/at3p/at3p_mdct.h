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

#include <array>
#include <cstdint>
#include <config.h>

#include "lib/mdct/mdct.h"

namespace NAtracDEnc {

class TAt3pMDCT;
class TAt3pMIDCT;

class TAt3pMDCTWin {
    friend class TAt3pMDCT;
    friend class TAt3pMIDCT;
public:
    enum EWinType {
        SINE = 0,
        STEEP = 1,
    };

    TAt3pMDCTWin() noexcept
        : Flags(0)
    {}

    TAt3pMDCTWin(EWinType win) noexcept
        : Flags(win == STEEP ? (uint16_t)-1 : 0)
    {}

    void SetSteepWin(size_t sb) noexcept {
        Flags |= (1 << sb);
    }

    bool IsAllSine() const noexcept {
        return Flags == 0;
    }

    bool IsAllSteep(uint8_t sbNum) const noexcept {
        uint8_t mask = (1 << sbNum) - 1;
        return (Flags & mask) == mask;
    }

    bool IsSbSteep(uint8_t sb) const noexcept {
        return Flags & (1 << sb);
    }
private:
    uint16_t Flags;
};

class TAt3pMDCT {
public:
    TAt3pMDCT();
    using THistBuf = std::array<std::array<float, 256>, 16>;
    using TPcmBandsData = std::array<const float*, 16>;

    void Do(float specs[2048], const TPcmBandsData& bands, THistBuf& work, TAt3pMDCTWin winType);
private:
    NMDCT::TMDCT<256> Mdct;
};

class TAt3pMIDCT {
public:
    TAt3pMIDCT();
    struct THistBuf {
        std::array<std::array<float, 128>, 16> Buf;
        TAt3pMDCTWin Win;
    };
    using TPcmBandsData = std::array<float*, 16>;

    void Do(float specs[2048], TPcmBandsData& bands, THistBuf& work, TAt3pMDCTWin winType);
private:
    NMDCT::TMIDCT<256> Midct;
};

}
