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
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <cstdint>

class ICompressedIO {
public:
    class TFrame {
        size_t Sz;
        char* Data;
        TFrame(const TFrame& src) = delete;
        TFrame() = delete;
    public:
        TFrame(size_t sz)
            : Sz(sz)
        {
            Data = new char[Sz];
        }
        ~TFrame() {
            delete[] Data;
        }
        size_t Size() const { return Sz; }
        char* Get() { return Data; }
    };
    virtual std::string GetName() const = 0;
    virtual size_t GetChannelNum() const = 0;
    virtual ~ICompressedIO() {}
};

class ICompressedInput : public ICompressedIO {
public:
    virtual std::unique_ptr<TFrame> ReadFrame() = 0;
    virtual uint64_t GetLengthInSamples() const = 0;
};

class ICompressedOutput : public ICompressedIO {
public:
    virtual void WriteFrame(std::vector<char> data) = 0;
};

typedef std::unique_ptr<ICompressedInput> TCompressedInputPtr;
typedef std::unique_ptr<ICompressedOutput> TCompressedOutputPtr;

