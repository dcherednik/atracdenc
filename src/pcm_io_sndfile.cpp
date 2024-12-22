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

#include "wav.h"

#include <sndfile.hh>
#include <algorithm>

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

class TPCMIOSndFile : public IPCMProviderImpl {
public:
    TPCMIOSndFile(const std::string& filename)
        : File(SndfileHandle(filename))
    {
        File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_TRUE);
    }
    TPCMIOSndFile(const std::string& filename, int channels, int sampleRate)
        : File(SndfileHandle(filename, SFM_WRITE, fileext_to_libsndfmt(filename) | SF_FORMAT_PCM_16, channels, sampleRate))
    {
        File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_TRUE);
    }
public:
    size_t GetChannelsNum() const override {
        return File.channels();
    }
    size_t GetSampleRate() const override {
        return File.samplerate();
    }
    size_t GetTotalSamples() const override {
        return File.frames();
    }
    size_t Read(TPCMBuffer& buf, size_t sz) override {
        return File.readf(buf[0], sz);
    }
    size_t Write(const TPCMBuffer& buf, size_t sz) override {
        return File.writef(buf[0], sz);
    }
private:
    mutable SndfileHandle File;
};

IPCMProviderImpl* CreatePCMIOReadImpl(const std::string& path) {
    return new TPCMIOSndFile(path); 
}

IPCMProviderImpl* CreatePCMIOWriteImpl(const std::string& path, int channels, int sampleRate) {
    return new TPCMIOSndFile(path, channels, sampleRate);
}
