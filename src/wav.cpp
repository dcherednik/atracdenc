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
    //disable scaling short -> [-1.0, 1.0]
    File.command(SFC_SET_NORM_DOUBLE /*| SFC_SET_NORM_FLOAT*/, nullptr, SF_FALSE);
}

TWav::TWav(const std::string& filename, int channels, int sampleRate)
    : File(SndfileHandle(filename, SFM_WRITE, fileext_to_libsndfmt(filename) | SF_FORMAT_PCM_16, channels, sampleRate)) {
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
