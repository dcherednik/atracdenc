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
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

#include "config.h"
#include <cstring>

#ifdef NDEBUG
#define ASSERT(x) do { ((void)(x));} while (0)
#else
#include <cassert>
#define ASSERT(x) assert(x)
#endif

#if defined(_MSC_VER)
#    define atde_noinline __declspec(noinline)
#else
#    define atde_noinline __attribute__((noinline))
#endif

template<class T>
inline void SwapArray(T* p, const size_t len) {
    for (size_t i = 0, j = len - 1; i < len / 2; ++i, --j) {
        T tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
}

template<size_t N>
inline void InvertSpectrInPlase(float* in) {
    for (size_t i = 0; i < N; i+=2)
        in[i] *= -1;
}

template<size_t N>
inline std::vector<float> InvertSpectr(const float* in) {
    std::vector<float> buf(N);
    std::memcpy(&buf[0], in, N * sizeof(float));
    InvertSpectrInPlase<N>(&buf[0]);
    return buf;
}

inline uint16_t GetFirstSetBit(uint32_t x) {
    static const uint16_t multiplyDeBruijnBitPosition[32] = {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return multiplyDeBruijnBitPosition[(uint32_t)(x * 0x07C4ACDDU) >> 27];
}

inline uint32_t Div8Ceil(uint32_t in) {
    return 1 + (in - 1) / 8;
}

template<class T>
inline T CalcMedian(T* in, uint32_t len) {
    std::vector<T> tmp(in, in+len);
    std::sort(tmp.begin(), tmp.end());
    uint32_t pos = (len - 1) / 2;
    return tmp[pos];
}

template<class T>
inline T CalcEnergy(const std::vector<T>& in) {
    return std::accumulate(in.begin(), in.end(), 0.0,
        [](const T& a, const T& b) {
            return a + b * b;
        });
}

inline int ToInt(float x) {
#if defined(_MSC_VER) && !defined(_WIN64)
    int n;
    __asm {
        fld x
        fistp n
    }
    return n;
#else
    return lrint(x);
#endif
}
