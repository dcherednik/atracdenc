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

#include "../config.h"
#include "../fft/kissfft_impl/kiss_fft.h"
#include <vector>
#include <type_traits>

namespace NMDCT {

static_assert(sizeof(kiss_fft_scalar) == sizeof(TFloat), "size of fft_scalar is not equal to size of TFloat");

class TMDCTBase {
protected:
    const size_t N;
    const std::vector<TFloat> SinCos;
    kiss_fft_cpx*   FFTIn;
    kiss_fft_cpx*   FFTOut;
    kiss_fft_cfg    FFTPlan;
    TMDCTBase(size_t n, TFloat scale);
    virtual ~TMDCTBase();
};


template<size_t TN, typename TIO = TFloat>
class TMDCT : public TMDCTBase {
    std::vector<TIO> Buf;
public:
    TMDCT(float scale = 1.0)
        : TMDCTBase(TN, scale)
        , Buf(TN/2)
    {
    }
    const std::vector<TIO>& operator()(const TIO* in) {

        const size_t n2 = N >> 1;
        const size_t n4 = N >> 2;
        const size_t n34 = 3 * n4;
        const size_t n54 = 5 * n4;
        const TFloat* cos = &SinCos[0];
        const TFloat* sin = &SinCos[1];

        TFloat  *xr, *xi, r0, i0;
        TFloat  c, s;
        size_t n;

        xr = (TFloat*)FFTIn;
        xi = (TFloat*)FFTIn + 1;
        for (n = 0; n < n4; n += 2) {
            r0 = in[n34 - 1 - n] + in[n34 + n];
            i0 = in[n4 + n] - in[n4 - 1 - n];

            c = cos[n];
            s = sin[n];

            xr[n] = r0 * c + i0 * s;
            xi[n] = i0 * c - r0 * s;
        }

        for (; n < n2; n += 2) {
            r0 = in[n34 - 1 - n] - in[n - n4];
            i0 = in[n4 + n]    + in[n54 - 1 - n];

            c = cos[n];
            s = sin[n];

            xr[n] = r0 * c + i0 * s;
            xi[n] = i0 * c - r0 * s;
        }

        kiss_fft(FFTPlan, FFTIn, FFTOut);

        xr = (TFloat*)FFTOut;
        xi = (TFloat*)FFTOut + 1;
        for (n = 0; n < n2; n += 2) {
            r0 = xr[n];
            i0 = xi[n];

            c = cos[n];
            s = sin[n];

            Buf[n] = - r0 * c - i0 * s;
            Buf[n2 - 1 -n] = - r0 * s + i0 * c;
        }

        return Buf;
    }
};

template<size_t TN, typename TIO = TFloat>
class TMIDCT : public TMDCTBase {
    std::vector<TIO> Buf;
public:
    TMIDCT(float scale = TN)
        : TMDCTBase(TN, scale/2)
        , Buf(TN)
    {}
    const std::vector<TIO>& operator()(const TIO* in) {

        const size_t n2 = N >> 1;
        const size_t n4 = N >> 2;
        const size_t n34 = 3 * n4;
        const size_t n54 = 5 * n4;
        const TFloat* cos = &SinCos[0];
        const TFloat* sin = &SinCos[1];

        TFloat *xr, *xi, r0, i0, r1, i1;
        TFloat c, s;
        size_t n;

        xr = (TFloat*)FFTIn;
        xi = (TFloat*)FFTIn + 1;

        for (n = 0; n < n2; n += 2) {
            r0 = in[n];
            i0 = in[n2 - 1 - n];

            c = cos[n];
            s = sin[n];

            xr[n] = -2.0 * (i0 * s + r0 * c);
            xi[n] = -2.0 * (i0 * c - r0 * s);
        }

        kiss_fft(FFTPlan, FFTIn, FFTOut);

        xr = (TFloat*)FFTOut;
        xi = (TFloat*)FFTOut + 1;

        for (n = 0; n < n4; n += 2) {
            r0 = xr[n];
            i0 = xi[n];

            c = cos[n];
            s = sin[n];

            r1 = r0 * c + i0 * s;
            i1 = r0 * s - i0 * c;

            Buf[n34 - 1 - n] = r1;
            Buf[n34 + n] = r1;
            Buf[n4 + n] = i1;
            Buf[n4 - 1 - n] = -i1;
        }

        for (; n < n2; n += 2) {
            r0 = xr[n];
            i0 = xi[n];

            c = cos[n];
            s = sin[n];

            r1 = r0 * c + i0 * s;
            i1 = r0 * s - i0 * c;

            Buf[n34 - 1 - n] = r1;
            Buf[n - n4] = -r1;
            Buf[n4 + n] = i1;
            Buf[n54 - 1 - n] = i1;
        }
        return Buf;
    }
};

} //namespace NMDCT
