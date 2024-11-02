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

#include "aea.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <memory>

using std::string;
using std::array;
using std::vector;
using std::unique_ptr;

class TAeaCommon {
protected:
    static constexpr uint32_t AeaMetaSize = 2048;
    struct TMeta {
        FILE* AeaFile;
        std::array<char, AeaMetaSize> AeaHeader;
    } Meta;
public:
    TAeaCommon(const TMeta& meta)
        : Meta(meta)
    {}
    size_t GetChannelNum() const;
    string GetName() const;
    virtual ~TAeaCommon();
};

size_t TAeaCommon::GetChannelNum() const {
    return Meta.AeaHeader[264];
}

string TAeaCommon::GetName() const {
    return string(&Meta.AeaHeader[4]);
}

TAeaCommon::~TAeaCommon() {
    fclose(Meta.AeaFile);
}

class TAeaInput : public ICompressedInput, public TAeaCommon {
    static TAeaCommon::TMeta ReadMeta(const string& filename);
public:
    TAeaInput(const string& filename);
    unique_ptr<TFrame> ReadFrame() override; 
    uint64_t GetLengthInSamples() const override;

    size_t GetChannelNum() const override {
        return TAeaCommon::GetChannelNum();
    }

    string GetName() const override {
        return TAeaCommon::GetName(); 
    }
};

TAeaInput::TAeaInput(const string& filename)
    : TAeaCommon(ReadMeta(filename))
{}

TAeaCommon::TMeta TAeaInput::ReadMeta(const string& filename) {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp)
        throw TAeaIOError("Can't open file to read", errno);
    array<char, AeaMetaSize> buf;
    if (fread(&buf[0], AeaMetaSize, 1, fp) != 1) {
        const int errnum = errno;
        fclose(fp);
        throw TAeaIOError("Can't read AEA header", errnum);
    }

    if ( buf[0] == 0x00 && buf[1] == 0x08 && buf[2] == 0x00 && buf[3] == 0x00 && buf[264] < 3 ) {
        return {fp, buf};
    }
    throw TAeaFormatError();
}

uint64_t TAeaInput::GetLengthInSamples() const {
#ifdef PLATFORM_WINDOWS
    const int fd = _fileno(Meta.AeaFile);
#else
    const int fd = fileno(Meta.AeaFile);
#endif
    struct stat sb;
    fstat(fd, &sb);
    const uint32_t nChannels = Meta.AeaHeader[264] ? Meta.AeaHeader[264] : 1;
    return (uint64_t)512 * ((sb.st_size - AeaMetaSize) / 212 / nChannels - 5); 
}

unique_ptr<ICompressedIO::TFrame> TAeaInput::ReadFrame() {
    unique_ptr<ICompressedIO::TFrame>frame(new TFrame(212));
    if(fread(frame->Get(), frame->Size(), 1, Meta.AeaFile) != 1) {
        const int errnum = errno;
        fclose(Meta.AeaFile);
        throw TAeaIOError("Can't read AEA frame", errnum);
    }
    return frame;
}

class TAeaOutput : public ICompressedOutput, public TAeaCommon {
    static TAeaCommon::TMeta CreateMeta(const string& filename, const string& title,
        size_t numChannel, uint32_t numFrames);

    bool FirstWrite = true;
public:
    TAeaOutput(const string& filename, const string& title, size_t numChannel, uint32_t numFrames);
    void WriteFrame(vector<char> data) override;

    size_t GetChannelNum() const override {
        return TAeaCommon::GetChannelNum();
    }
    string GetName() const override {
        return TAeaCommon::GetName(); 
    }
};

TAeaOutput::TAeaOutput(const string& filename, const string& title, size_t numChannels, uint32_t numFrames)
    : TAeaCommon(CreateMeta(filename, title, numChannels, numFrames))
{}

TAeaCommon::TMeta TAeaOutput::CreateMeta(const string& filename, const string& title,
    size_t channelsNum, uint32_t numFrames)
{
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp)
        throw TAeaIOError("Can't open file to write", errno);

    array<char, AeaMetaSize> buf;
    memset(&buf[0], 0, AeaMetaSize);
    buf[0] = 0x00;
    buf[1] = 0x08;
    buf[2] = 0x00;
    buf[3] = 0x00;
    strncpy(&buf[4], title.c_str(), 16);
    buf[19] = 0;
//    buf[210] = 0x08;
    *(uint32_t*)&buf[260] = numFrames;
    buf[264] = (char)channelsNum;

    if (fwrite(&buf[0], AeaMetaSize, 1, fp) != 1) {
        const int errnum = errno;
        fclose(fp);
        throw TAeaIOError("Can't write AEA header", errnum);
    }

    static char dummy[212];
    if (fwrite(&dummy[0], 212, 1, fp) != 1) {
        const int errnum = errno;
        fclose(fp);
        throw TAeaIOError("Can't write dummy frame", errnum);
    }

    return {fp, buf};
}

void TAeaOutput::WriteFrame(vector<char> data) {
    if (FirstWrite) {
        FirstWrite = false;
        return;
    }

    data.resize(212);

    if (fwrite(data.data(), data.size(), 1, Meta.AeaFile) != 1) {
        const int errnum = errno;
        fclose(Meta.AeaFile);
        throw TAeaIOError("Can't write AEA frame", errnum);
    }
}

TCompressedInputPtr CreateAeaInput(const std::string& filename) {
    return unique_ptr<TAeaInput>(new TAeaInput(filename));
}

TCompressedOutputPtr CreateAeaOutput(const string& filename, const string& title,
    size_t numChannels, uint32_t numFrames)
{
    return unique_ptr<TAeaOutput>(new TAeaOutput(filename, title, numChannels, numFrames));
}
