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

#include <functional>
#include <memory>
#include <cerrno>
#include <algorithm>
#include <string>

#include <sys/stat.h>
#include <string.h>

#include "wav.h"
#include "pcmengin.h"

IPCMProviderImpl* CreatePCMIOReadImpl(const std::string& path);

IPCMProviderImpl* CreatePCMIOWriteImpl(const std::string& path, int channels, int sampleRate);

TWav::TWav(const std::string& path)
    : Impl(CreatePCMIOReadImpl(path))
{ }

TWav::TWav(const std::string& path, size_t channels, size_t sampleRate)
    : Impl(CreatePCMIOWriteImpl(path, channels, sampleRate))
{ }

TWav::~TWav() {
}

uint64_t TWav::GetTotalSamples() const {
    return Impl->GetTotalSamples();
}

size_t TWav::GetChannelNum() const {
    return Impl->GetChannelsNum();
}

size_t TWav::GetSampleRate() const {
    return Impl->GetSampleRate();
}

//bool TWav::IsFormatSupported() const {
//    switch (File.format() & SF_FORMAT_TYPEMASK) {
//        case SF_FORMAT_WAV:
//        case SF_FORMAT_AIFF:
//            return true;
//        default:
//            return false;
//    }
//}
