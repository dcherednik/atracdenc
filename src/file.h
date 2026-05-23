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

#include <cstdio>
#include <memory>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

struct TFileCloser {
    void operator()(FILE* Fp) const {
#ifdef PLATFORM_WINDOWS
        if (Fp) {
            const int fd = _fileno(Fp);
            if (fflush(Fp) == 0 && fd != -1) {
                _commit(fd);
            }
            fclose(Fp);
        }
#else
        if (Fp) {
            const int fd = fileno(Fp);
            if (fflush(Fp) == 0 && fd != -1) {
                fsync(fd);
            }
            fclose(Fp);
        }
#endif
    }
};

using TFilePtr = std::unique_ptr<FILE, TFileCloser>;
