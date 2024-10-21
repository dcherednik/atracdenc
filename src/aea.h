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


TCompressedInputPtr CreateAeaInput(const std::string& filename);
TCompressedOutputPtr CreateAeaOutput(const std::string& filename, const std::string& title,
    size_t numChannel, uint32_t numFrames);
