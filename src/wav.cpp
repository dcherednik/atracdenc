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
 * License along with FFmpeg; if not, write to the Free Software
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

static int fileext_to_libsndfmt(const std::string& filename) {
    int fmt = SF_FORMAT_WAV; //default fmt
    if (filename == "-")
        return SF_FORMAT_AU;
    size_t pos = filename.find_last_of(".");
    if (pos == std::string::npos || pos == filename.size() - 1) //no dot or filename.
        return fmt;
    std::string ext = filename.substr(pos+1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    if (ext == "AU") {
        fmt = SF_FORMAT_AU;
    } else if (ext == "AIFF") {
        fmt = SF_FORMAT_AIFF;
    } else if (ext == "PCM" || ext == "RAW") {
        fmt = SF_FORMAT_RAW;
    }
    return fmt;
}

TWav::TWav(const std::string& filename)
    : File(SndfileHandle(filename)) {
    File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_TRUE);
}

TWav::TWav(const std::string& filename, int channels, int sampleRate)
    : File(SndfileHandle(filename, SFM_WRITE, fileext_to_libsndfmt(filename) | SF_FORMAT_PCM_16, channels, sampleRate)) {
    File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_TRUE);
}

TWav::~TWav() {
}

uint64_t TWav::GetTotalSamples() const {
    return File.frames();
}

uint32_t TWav::GetChannelNum() const {
    return File.channels();
}

uint32_t TWav::GetSampleRate() const {
    return File.samplerate();
}

bool TWav::IsFormatSupported() const {
    switch (File.format() & SF_FORMAT_TYPEMASK) {
        case SF_FORMAT_WAV:
        case SF_FORMAT_AIFF:
            return true;
        default:
            return false;
    }
}
