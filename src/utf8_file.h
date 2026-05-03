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
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NAtracDEnc {

#ifdef _WIN32
inline std::wstring Utf8ToWidePath(const std::string& path) {
    const int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path.c_str(), -1, nullptr, 0);
    if (!len) {
        throw std::runtime_error("unable to convert UTF-8 path to UTF-16: " + path);
    }

    std::wstring res(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path.c_str(), -1, res.data(), len);
    res.pop_back();
    return res;
}
#endif

inline FILE* FOpenUtf8(const std::string& path, const char* mode) {
#ifdef _WIN32
    const std::wstring wpath = Utf8ToWidePath(path);
    const std::wstring wmode(mode, mode + std::char_traits<char>::length(mode));
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return fopen(path.c_str(), mode);
#endif
}

} // namespace NAtracDEnc
