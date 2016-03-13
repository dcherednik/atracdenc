#include "oma.h"
#include <stdlib.h>

TOma::TOma(const std::string& filename, const std::string& title, int numChannel, uint32_t numFrames, int cid, uint32_t framesize) {
    oma_info_t info;
    info.codec = cid;
    info.samplerate = 44100;
    info.channel_format = OMA_STEREO;
    info.framesize = framesize;
    File = oma_open(filename.c_str(), OMAM_W, &info);
    if (!File)
        abort();
}

TOma::~TOma() {
    oma_close(File);
}

std::unique_ptr<ICompressedIO::TFrame> TOma::ReadFrame() {
    abort();
    return nullptr;
}

void TOma::WriteFrame(std::vector<char> data) {
    if (oma_write(File, &data[0], 1) == -1) {
        fprintf(stderr, "write error\n");
        abort();
    }
}

std::string TOma::GetName() const {
    abort();
    return {};
}

int TOma::GetChannelNum() const {
    return 2; //for ATRAC3
}
long long TOma::GetLengthInSamples() const {
    abort();
    return 0;
}
