#pragma once
#include <cstdint>


template<class T>
inline void SwapArray(T* p, const size_t len) {
    for (size_t i = 0, j = len - 1; i < len / 2; ++i, --j) {
        T tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
}
