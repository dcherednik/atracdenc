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
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>

#include "compressed_io.h"


class TAeaIOError : public std::exception {
    const int ErrNum = 0;
    const char* Text;
public:
    TAeaIOError(const char* str, int err)
        : ErrNum(err)
        , Text(str)
    {
        (void)ErrNum; //TODO: use it
    }
    virtual const char* what() const throw() {
        return Text;
    }
};

class TAeaFormatError {
};

typedef ICompressedIO IAtrac1IO;

class TAea : public IAtrac1IO {
    static constexpr uint32_t AeaMetaSize = 2048;
    struct TMeta {
        FILE* AeaFile;
        std::array<char, AeaMetaSize> AeaHeader;
    } Meta;
    static TAea::TMeta ReadMeta(const std::string& filename);
    static TAea::TMeta CreateMeta(const std::string& filename, const std::string& title, int numChannel, uint32_t numFrames);
    bool FirstWrite = true;
public:
		TAea(const std::string& filename);
        TAea(const std::string& filename, const std::string& title, int numChannel, uint32_t numFrames);
		~TAea();
        std::unique_ptr<TFrame> ReadFrame() override; 
        void WriteFrame(std::vector<char> data) override;
        std::string GetName() const override;
        int GetChannelNum() const override;
        long long GetLengthInSamples() const override;
};


