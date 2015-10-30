#include "aea.h"

#include <sys/types.h>
#include <sys/stat.h>

using std::string;


TAea::TMeta TAea::ReadMeta(const string& filename) {
    FILE* fp = fopen(filename.c_str(), "r");
    if (!fp)
        throw TAeaIOError("Can't open file to read", errno);
    std::array<char, AeaMetaSize> buf;
    if (fread(&buf[0], TAea::AeaMetaSize, 1, fp) != 1) {
        const int errnum = errno;
        fclose(fp);
        throw TAeaIOError("Can't read AEA header", errnum);
    }

    if ( buf[0] == 0x00 && buf[1] == 0x08 && buf[2] == 0x00 && buf[3] == 0x00 && buf[264] < 3 ) {
        return {fp, buf};
    }
    throw TAeaFormatError();

}

TAea::TMeta TAea::CreateMeta(const string& filename, const string& title, int channelNum) {
    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp)
        throw TAeaIOError("Can't open file to write", errno);
    std::array<char, AeaMetaSize> buf;
    memset(&buf[0], 0, AeaMetaSize);
    buf[0] = 0x00;
    buf[1] = 0x08;
    buf[2] = 0x00;
    buf[3] = 0x00;
    strncpy(&buf[4], title.c_str(), 16);
    buf[19] = 0;
//    buf[210] = 0x08;
    buf[264] = (char)channelNum;
    if (fwrite(&buf[0], TAea::AeaMetaSize, 1, fp) != 1) {
        const int errnum = errno;
        fclose(fp);
        throw TAeaIOError("Can't read AEA header", errnum);
    }
    return {fp, buf};
}


TAea::TAea(const string& filename)
    : Meta(ReadMeta(filename)) {
    }

TAea::TAea(const string& filename, const string& title, int channelNum)
    : Meta(CreateMeta(filename, title, channelNum)) {
    }

TAea::~TAea() {
    fclose(Meta.AeaFile);
}

std::string TAea::GetName() const {
    return string(&Meta.AeaHeader[4]);
}

std::unique_ptr<TAea::TFrame> TAea::ReadFrame() {
    std::unique_ptr<TAea::TFrame>frame(new TFrame);
    if(fread(frame.get(), frame->size(), 1, Meta.AeaFile) != 1) {
        const int errnum = errno;
        fclose(Meta.AeaFile);
        throw TAeaIOError("Can't read AEA frame", errnum);
    }
    return frame;
}
/*
void TAea::WriteFrame(std::unique_ptr<TAea::TFrame>&& frame) {
    if (fwrite(frame.get(), frame->size(), 1, Meta.AeaFile) != 1) {
        const int errnum = errno;
        fclose(Meta.AeaFile);
        throw TAeaIOError("Can't write AEA frame", errnum);
    }
}
*/
void TAea::WriteFrame(std::vector<char> data) {
    data.resize(212);
    if (fwrite(data.data(), data.size(), 1, Meta.AeaFile) != 1) {
        const int errnum = errno;
        fclose(Meta.AeaFile);
        throw TAeaIOError("Can't write AEA frame", errnum);
    }
}

int TAea::GetChannelNum() const {
    return Meta.AeaHeader[264];
}
uint32_t TAea::GetLengthInSamples() const {
    const int fd = fileno(Meta.AeaFile);
    struct stat sb;
    fstat(fd, &sb);
    const uint32_t nChannels = Meta.AeaHeader[264] ? Meta.AeaHeader[264] : 1;
    return 512 * ((sb.st_size - TAea::AeaMetaSize) / 212 / nChannels - 5); 
}
