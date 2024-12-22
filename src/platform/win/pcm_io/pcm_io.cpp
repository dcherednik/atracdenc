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

#include "pcm_io_impl.h"
#include "mf/pcm_io_mf.h"
#include "win32/pcm_io_win32.h"

#include <endian_tools.h>

#include <iostream>
#include <windows.h>


void ConvertToPcmBufferFromLE(const BYTE* audioData, TPCMBuffer& buf, size_t sz, size_t shift, size_t channelsNum) {
    if (channelsNum == 1) {
        for (size_t i = 0; i < sz; i++) {
            *(buf[i + shift] + 0) = (*(int16_t*)(audioData + i * 2 + 0)) / (float)32768.0;
        }
    } else {
        for (size_t i = 0; i < sz; i++) {
            *(buf[i + shift] + 0) = (*(int16_t*)(audioData + i * 4 + 0)) / (float)32768.0;
            *(buf[i + shift] + 1) = (*(int16_t*)(audioData + i * 4 + 2)) / (float)32768.0;
        }
    }
}

void ConvertToPcmBufferFromBE(const BYTE* audioData, TPCMBuffer& buf, size_t sz, size_t shift, size_t channelsNum) {
    if (channelsNum == 1) {
        for (size_t i = 0; i < sz; i++) {
            *(buf[i + shift] + 0) = conv_ntoh((*(int16_t*)(audioData + i * 2 + 0))) / (float)32768.0;
        }
    } else {
        for (size_t i = 0; i < sz; i++) {
            *(buf[i + shift] + 0) = conv_ntoh((*(int16_t*)(audioData + i * 4 + 0))) / (float)32768.0;
            *(buf[i + shift] + 1) = conv_ntoh((*(int16_t*)(audioData + i * 4 + 2))) / (float)32768.0;
        }
    }
}

IPCMProviderImpl* CreatePCMIOReadImpl(const std::string& path) {
    if (path == "-") {
        return CreatePCMIOStreamWin32ReadImpl();
    }
    return CreatePCMIOMFReadImpl(path);
}

IPCMProviderImpl* CreatePCMIOWriteImpl(const std::string& path, int channels, int sampleRate) {
    return CreatePCMIOMFWriteImpl(path, channels, sampleRate);
}
