#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

#include "config.h"

template<class T>
inline void SwapArray(T* p, const size_t len) {
    for (size_t i = 0, j = len - 1; i < len / 2; ++i, --j) {
        T tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
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

template<class T>
inline uint16_t Log2FloatToIdx(T x, uint16_t shift) {
    T t = x * shift;
    return GetFirstSetBit(t);
}

template<class T>
inline T CalcMedian(T* in, uint32_t len) {
    std::vector<T> tmp(in, in+len);
    std::sort(tmp.begin(), tmp.end());
    uint32_t pos = (len - 1) / 2;
    return tmp[pos];
}
