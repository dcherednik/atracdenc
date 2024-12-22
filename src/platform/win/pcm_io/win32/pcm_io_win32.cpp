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

#include "../../../wav.h"
#include "../../../env.h"

#include <endian_tools.h>

#include "pcm_io_win32.h"
#include "../pcm_io_impl.h"

#include<windows.h>

static std::string GetLastErrorAsString()
{
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string();

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    LocalFree(messageBuffer);

    return message;
}

class TPCMIOStreamWin32 : public IPCMProviderImpl {
public:
    TPCMIOStreamWin32() {
        ReadHandle_ = GetStdHandle(STD_INPUT_HANDLE);
        if (ReadHandle_ == INVALID_HANDLE_VALUE) {
            const std::string msg = "Unable to open input stream " + GetLastErrorAsString();
            throw std::exception(msg.data());
        }

        static const DWORD headerSz = 24;
        Buf_.resize(headerSz);

        DWORD read;
        if (ReadFile(ReadHandle_, Buf_.data(), headerSz, &read, NULL)) {
            if (read != headerSz) {
                throw std::exception("Not enough data to determinate format.");
            }

            const uint32_t offset = ParseHeader();
            if (offset < headerSz) {
                throw std::exception("incorrect data offset");
            }

            uint32_t toSkip = offset - headerSz;
            while (toSkip) {
                char c;
                bool success;
                success = ReadFile(ReadHandle_, &c, 1, &read, NULL);
                if (!success || !read) {
                    throw std::exception("Unable to seek to data position");
                }
                toSkip--;
            }
        } else {
            const std::string msg = "Unable to read header from pipe " + GetLastErrorAsString();
            throw std::exception(msg.data());
        }
    }

    size_t Read(TPCMBuffer& buf, size_t sz) override {
        if (Finished_)
            return 0;

        DWORD bytesToRead = sz * 2 * ChannelsNum_; // 16 bit per sampple
        Buf_.resize(bytesToRead); 

        DWORD pos = 0;

        while (pos < bytesToRead) {
            DWORD read;
            bool success;
            success = ReadFile(ReadHandle_, Buf_.data() + pos, bytesToRead - pos, &read, NULL);
            if (!success || !read) {
                Finished_ = true;
                break;
            }
            pos += read;
        }

        size_t toConvert = pos / 2 / ChannelsNum_;
        ConvertToPcmBufferFromBE(Buf_.data(), buf, toConvert, 0, ChannelsNum_);

        return toConvert;
    }

    size_t Write(const TPCMBuffer& buf, size_t sz) override {
        abort();
        return 0;
    }

    size_t GetChannelsNum() const override {
        return ChannelsNum_;
    }

    size_t GetSampleRate() const override {
        return SampleRate_;
    }

    size_t GetTotalSamples() const override {
        return (size_t)-1;
    }

private:
    static uint32_t ExtractValue(BYTE* buf) {
        uint32_t tmp;
        memcpy(&tmp, buf, 4);
        return conv_ntoh(tmp);
    }

    uint32_t ParseHeader() {
        static const std::vector<BYTE> magic = { '.', 's', 'n', 'd' };
        if (!std::equal(Buf_.begin(), Buf_.begin() + magic.size(), magic.begin())) {
            throw std::exception("Input stream must have AU(SND) format");
        }

        uint32_t dataOffset = ExtractValue(&Buf_[4]);
        //uint32_t dataSize = ExtractValue(&Buf_[8]);

        uint32_t encoding = ExtractValue(&Buf_[12]);
        if (encoding != 3) {
            throw std::exception("Expected PCM 16 bit format");
        }

        uint32_t sampleRate = ExtractValue(&Buf_[16]);
        if (sampleRate != 44100) {
            throw std::exception("Expected 44100Hz sampe rate");
        }

        ChannelsNum_ = ExtractValue(&Buf_[20]);
        if (ChannelsNum_ != 1 && ChannelsNum_ != 2) {
            throw std::exception("Expected 1 or 2 channels");
        }

        return dataOffset;
    }

    HANDLE ReadHandle_;
    uint32_t ChannelsNum_ = 2;
    const uint32_t SampleRate_ = 44100;

    std::vector<BYTE> Buf_;
    bool Finished_ = false;
};

IPCMProviderImpl* CreatePCMIOStreamWin32ReadImpl() {
    return new TPCMIOStreamWin32();
}
