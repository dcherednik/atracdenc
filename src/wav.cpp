#include <functional>
#include <memory>
#include <cerrno>

#include <sys/stat.h>

#include "wav.h"
#include "pcmengin.h"


using namespace std;

TWavHeader TWav::CreateHeader(int channels, uint32_t length) {
    TWavHeader header;
    uint32_t dataLen = length * 2 * channels;
    const char dataString[] = "data";
    header.RIFF[0] = 0x52;
    header.RIFF[1] = 0x49;
    header.RIFF[2] = 0x46;
    header.RIFF[3] = 0x46;
    header.ChunkSize = dataLen + sizeof(TWavHeader) - 8;;
    header.WAVE[0] = 0x57;
    header.WAVE[1] = 0x41;
    header.WAVE[2] = 0x56;
    header.WAVE[3] = 0x45;
    header.fmt[0] = 0x66;
    header.fmt[1] = 0x6d;
    header.fmt[2] = 0x74;
    header.fmt[3] = 0x20;
    header.Subchunk1Size = 16;
    header.AudioFormat = 1;
    header.NumOfChan = channels;
    header.SamplesPerSec = 44100; //samplerate;
    header.bytesPerSec = 44100 * ((16 * channels) / 8);
    header.blockAlign = 2 * channels;
    header.bitsPerSample = 16;
    strncpy(&header.Subchunk2ID[0], dataString, sizeof(dataString));
    header.Subchunk2Size = dataLen;

    return header;
}

TWav::TMeta TWav::ReadWavHeader(const string& filename) {
    FILE* fd = fopen(filename.c_str(), "rb");
    if (!fd)
        throw TWavIOError("Can't open file to read", errno);
    TWavHeader headerBuf;
    if (fread(&headerBuf, sizeof(TWavHeader), 1, fd) != 1) {
        const int errnum = errno;
        fclose(fd);
        throw TWavIOError("Can't read WAV header", errnum);
    }
    return {fd, headerBuf};
}

TWav::TMeta TWav::CreateFileAndHeader(const string& filename, const TWavHeader& header, bool overwrite) {
    (void)overwrite;

    FILE* fd = fopen(filename.c_str(), "wb");
    if (!fd)
        throw TWavIOError("Can't open file to write", errno);
    if (fwrite(&header, sizeof(TWavHeader), 1, fd) != 1) {
        const int errnum = errno;
        fclose(fd);
        throw TWavIOError("Can't write WAV header", errnum);
    }
    return {fd, header};
}

TWav::TWav(const string& filename)
    : Meta(ReadWavHeader(filename)) {
}

TWav::TWav(const string& filename, const TWavHeader& header, bool overwrite)
    : Meta(CreateFileAndHeader(filename, header, overwrite)) {
}
TWav::~TWav() {
    fclose(Meta.WavFile);
}

const TWavHeader& TWav::GetWavHeader() const {
    return Meta.Header;
}


