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
#include <cstring>
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

inline const wchar_t* FileModeToWide(const char* mode) {
    if (!mode) {
        return nullptr;
    }
    if (std::strcmp(mode, "r") == 0) {
        return L"r";
    }
    if (std::strcmp(mode, "w") == 0) {
        return L"w";
    }
    if (std::strcmp(mode, "a") == 0) {
        return L"a";
    }
    if (std::strcmp(mode, "rb") == 0) {
        return L"rb";
    }
    if (std::strcmp(mode, "wb") == 0) {
        return L"wb";
    }
    if (std::strcmp(mode, "ab") == 0) {
        return L"ab";
    }
    if (std::strcmp(mode, "rb+") == 0 || std::strcmp(mode, "r+b") == 0) {
        return L"rb+";
    }
    if (std::strcmp(mode, "wb+") == 0 || std::strcmp(mode, "w+b") == 0) {
        return L"wb+";
    }
    if (std::strcmp(mode, "ab+") == 0 || std::strcmp(mode, "a+b") == 0) {
        return L"ab+";
    }
    return nullptr;
}
#endif

inline FILE* FOpenUtf8(const std::string& path, const char* mode) {
#ifdef _WIN32
    const wchar_t* wmode = FileModeToWide(mode);
    if (!wmode) {
        return nullptr;
    }
    const std::wstring wpath = Utf8ToWidePath(path);
    return _wfopen(wpath.c_str(), wmode);
#else
    return fopen(path.c_str(), mode);
#endif
}

} // namespace NAtracDEnc
