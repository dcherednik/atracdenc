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
#include <cstring>

namespace NAtracDEnc {

template<class T, int N, int S>
class TDelayBuffer {
public:
    TDelayBuffer() {
        static_assert(std::is_pod<T>::value, "T must be a POD type.");
        memset(Buffer_, 0, sizeof(T) * N * S * 2);
    }

    void Shift(bool erace = true) {
        for (int i = 0; i < N; i++) {
            memcpy(&Buffer_[i][0], &Buffer_[i][S], sizeof(T) * S);
            if (erace)
                memset(&Buffer_[i][S], 0, sizeof(T) * S);
        }
    }

    T* GetFirst(int i) {
        return &Buffer_[i][0];
    }

    T* GetSecond(int i) {
        return &Buffer_[i][S];
    }

private:
    T Buffer_[N][S*2];    
};

} // namespace NAtracDEnc
