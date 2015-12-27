#include <functional>
#include <memory>
#include <cerrno>

#include <sys/stat.h>
#include <string.h>

#include "wav.h"
#include "pcmengin.h"


TWav::TWav(const std::string& filename)
    : File(SndfileHandle(filename)) {
    //disable scaling short -> [-1.0, 1.0]
    File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_FALSE);
}

TWav::TWav(const std::string& filename, int channels, int sampleRate)
    : File(SndfileHandle(filename, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, channels, sampleRate)) {
    //disable scaling short -> [-1.0, 1.0]
    File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_FALSE);
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
