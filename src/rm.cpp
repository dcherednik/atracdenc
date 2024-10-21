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

#include "rm.h"

#include "lib/endian_tools.h"
#include <cstring>
#include <iostream>
#include <cmath>
#include <assert.h>

/*
 * This file result of reading https://wiki.multimedia.cx/index.php/RealMedia, FFMpeg code, and some experiments
 * Produce ra5 stream compatible with proprietary RA player
 */

/*
 * TODO:
 *  - title support
 */
namespace {

using std::string;

FILE* OpenFile(const string& filename) {
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp)
        throw std::runtime_error("Can't open file to write");
    return fp;
}

constexpr size_t RMF_HEADER_SZ = 18;
void WriteRMF(FILE* f) {
    char buf[RMF_HEADER_SZ] = {
        '.', 'R', 'M', 'F'};
    *reinterpret_cast<uint32_t*>(buf +  4) = swapbyte32_on_le(18);
    *reinterpret_cast<uint16_t*>(buf +  8) = 0; //chunk version
    *reinterpret_cast<uint32_t*>(buf + 10) = 0; //file version
    *reinterpret_cast<uint32_t*>(buf + 14) = swapbyte32_on_le(4); //number of headers
    if (fwrite(buf, RMF_HEADER_SZ, 1, f) != 1)
        throw std::runtime_error("Can't write RMF header");
}

constexpr size_t CODEC_DATA_SZ = 92;
constexpr char RA_MIME[] = "audio/x-pn-realaudio";
constexpr char RA_DESC[] = "Audio Stream";
static_assert(sizeof(char) == 1, "unexpected char size");
constexpr size_t MDPR_HEADER_SZ = 42 + sizeof(RA_MIME) + sizeof(RA_DESC) + CODEC_DATA_SZ;

void FillCodecData(char* buf, uint32_t frameSize, size_t numChannels, bool jointStereo, uint32_t bitrate) {
    *reinterpret_cast<uint32_t*>(buf +  0) = swapbyte32_on_le(CODEC_DATA_SZ - 4); // -4 - without size of `size` field
    buf[4] = '.';
    buf[5] = 'r';
    buf[6] = 'a';
    buf[7] = 0xfd;
    *reinterpret_cast<uint16_t*>(buf +  8) = swapbyte16_on_le(5); // version
    *reinterpret_cast<uint16_t*>(buf +  10) = 0; // unused
    buf[12] = '.';
    buf[13] = 'r';
    buf[14] = 'a';
    buf[15] = '5';
    *reinterpret_cast<uint32_t*>(buf +  16) = swapbyte32_on_le(0x01b53530); // copypaste from FFmpeg
    *reinterpret_cast<uint16_t*>(buf +  20) = swapbyte16_on_le(5); // version2
    *reinterpret_cast<uint32_t*>(buf +  22) = swapbyte32_on_le(0); // header size
    *reinterpret_cast<uint16_t*>(buf +  26) = swapbyte16_on_le(2); // flavor ???
    *reinterpret_cast<uint32_t*>(buf +  28) = swapbyte32_on_le(frameSize * 3); // codec frame size
    *reinterpret_cast<uint32_t*>(buf +  32) = swapbyte32_on_le(0x51540); // copypaste from FFmpeg
    *reinterpret_cast<uint32_t*>(buf +  36) = swapbyte32_on_le(bitrate / 8 * 60); // bytes per minute
    *reinterpret_cast<uint32_t*>(buf +  40) = swapbyte32_on_le(bitrate / 8 * 60); // maybe bytes per minute???
    *reinterpret_cast<uint16_t*>(buf +  44) = swapbyte16_on_le(1); // sub packet h (no interleave)
    *reinterpret_cast<uint16_t*>(buf +  46) = swapbyte16_on_le(frameSize * 3); // frame size
    *reinterpret_cast<uint16_t*>(buf +  48) = swapbyte16_on_le(frameSize); // sub packet sz
    *reinterpret_cast<uint16_t*>(buf +  50) = 0; // ???
    buf[52] = 0;
    buf[53] = 0;

    *reinterpret_cast<uint16_t*>(buf +  54) = swapbyte16_on_le(44100); // sample rate
    buf[56] = 0;
    buf[57] = 0;
    *reinterpret_cast<uint16_t*>(buf +  58) = swapbyte16_on_le(44100); // sample rate
    *reinterpret_cast<uint16_t*>(buf +  60) = 0;
    *reinterpret_cast<uint16_t*>(buf +  62) = swapbyte16_on_le(16); //sample size
    *reinterpret_cast<uint16_t*>(buf +  64) = swapbyte16_on_le(2); //channels
    buf[66] = 'g';
    buf[67] = 'e';
    buf[68] = 'n';
    buf[69] = 'r';
    buf[70] = 'a';
    buf[71] = 't';
    buf[72] = 'r';
    buf[73] = 'c';

    buf[74] = 0x01; // ???
    buf[75] = 0x07; // ???
    buf[76] = 0;

    buf[77] = 0;

    *reinterpret_cast<uint32_t*>(buf +  78) = swapbyte32_on_le(10);
    *reinterpret_cast<uint32_t*>(buf +  82) = swapbyte32_on_le(4);
    *reinterpret_cast<uint16_t*>(buf +  86) = swapbyte16_on_le(1024 * numChannels);
    *reinterpret_cast<uint16_t*>(buf +  88) = swapbyte16_on_le(0x88E);
    *reinterpret_cast<uint16_t*>(buf +  90) = swapbyte16_on_le(jointStereo ? 0x12 : 0x2);
}

void WriteDATA(FILE* f, uint32_t numFrames) {
    constexpr size_t DATA_HEADER_SZ = 18;

    char buf[DATA_HEADER_SZ] = {
        'D', 'A', 'T', 'A'};
    *reinterpret_cast<uint32_t*>(buf +  4) = 0xffffffff; //size of whole data chunk, will be patched at the end
    *reinterpret_cast<uint16_t*>(buf +  8) = 0; //chunk version
    *reinterpret_cast<uint32_t*>(buf +  10) = swapbyte32_on_le(numFrames);
    *reinterpret_cast<uint32_t*>(buf +  14) = swapbyte32_on_le(0); //offset next DATA header (not used)

    if (fwrite(buf, DATA_HEADER_SZ, 1, f) != 1)
        throw std::runtime_error("Can't write DATA header");
}

void scramble_data(const char* input, char* out, size_t bytes) {
    const uint32_t* buf = reinterpret_cast<const uint32_t*>(input);
    uint32_t* o = reinterpret_cast<uint32_t*>(out);
    uint32_t c = swapbyte32_on_le(0x537F6103U);
    static_assert(sizeof(uint32_t) / sizeof(char) == 4, "unexpected uint32_t size");

    for (size_t i = 0; i < bytes / 4; i++) {
        o[i] = c ^ buf[i];
    }
}

} //namespace

class TRm : public ICompressedOutput {
public:
    TRm(const std::string& filename, const std::string& /*title*/, size_t numChannels,
        uint32_t numFrames, uint32_t frameSize, bool jointStereo)
        : File_(OpenFile(filename))
	, FrameDuration_((1000.0 * 1024.0 / 44100.0)) // ms
        , Bitrate_(8 * frameSize * 44100.0 / 1024.0)
	, Timestamp_(0)
        , FrameNum_(0)
    {
        WriteRMF(File_);
        WritePROP(frameSize, numFrames);
        WriteMDPR(frameSize, numFrames, numChannels, jointStereo);
	DataHeaderPos_ = ftell(File_); // Store position of data header, to update it at finish
        WriteDATA(File_, numFrames);
    }

    ~TRm() {
	// TODO: change ICompressedOutput iface to remove this logic from dtor.
        int64_t endDataPos = ftell(File_);
        int64_t dataChunkSz = endDataPos - DataHeaderPos_;
        if (dataChunkSz <= 0xffffffff) {
            if (fseek(File_, DataHeaderPos_ + 4, SEEK_SET) == 0) {
                char tmp[sizeof(uint32_t)];
                *reinterpret_cast<uint32_t*>(&tmp[0]) = swapbyte32_on_le(dataChunkSz);
                if (fwrite(tmp, sizeof(uint32_t), 1, File_) != 1) {
                    fprintf(stderr, "Unable to update data chink size");
                }
            } else {
                fprintf(stderr, "Unable to navigate to data chink header");
            }
        } else {
            fprintf(stderr, "Too many data for RM container. Encoded data is writen, but format is incorrect.");
        }

        fclose(File_);
    }

    void WriteFrame(std::vector<char> data) override {
        static std::vector<char> tmp(data.size());
        scramble_data(&data[0], &tmp[0], data.size());
        WriteAudioPacket(tmp);
        FrameNum_++;
    }

    std::string GetName() const override {
        return {};
    }

    size_t GetChannelNum() const override {
	    return 0;
    }

private:
    FILE* File_;
    const double FrameDuration_;
    const uint32_t Bitrate_;
    double Timestamp_;
    uint32_t FrameNum_;

    int64_t DataHeaderPos_;

    void WriteAudioPacket(const std::vector<char>& data) {
	switch (FrameNum_ % 3) {
	    case 0: {
                char buf[12];
                *reinterpret_cast<uint16_t*>(buf +  0) = swapbyte16_on_le(0); //packet version
                *reinterpret_cast<uint16_t*>(buf +  2) = swapbyte16_on_le(3 * data.size() + 12); //packet size
                *reinterpret_cast<uint16_t*>(buf +  4) = swapbyte16_on_le(0); //stream number
                *reinterpret_cast<uint32_t*>(buf +  6) = swapbyte32_on_le((uint32_t)Timestamp_); //timestamp
                buf[10] = 0;
                buf[11] = 0x02; // This flag and Timestamp calculation are important to play file by original RA player 

                if (fwrite(buf, 12, 1, File_) != 1)
                    throw std::runtime_error("Can't write packet header");//, errno);
             }
	     break;
             case 2:
                Timestamp_ += (FrameDuration_ * 3.0);
	     break;
	}
        if (fwrite(&data[0], data.size(), 1, File_) != 1)
            throw std::runtime_error("Can`t write codec data");
    }

    void WritePROP(uint32_t frameSize, uint32_t numFrames) {
        constexpr size_t PROP_HEADER_SZ = 50;
        char buf[PROP_HEADER_SZ] = {
            'P', 'R', 'O', 'P'};
        *reinterpret_cast<uint32_t*>(buf +  4) = swapbyte32_on_le(50);
        *reinterpret_cast<uint16_t*>(buf +  8) = 0; //chunk version
        *reinterpret_cast<uint32_t*>(buf +  10) = swapbyte32_on_le(Bitrate_); // max bitrate
        *reinterpret_cast<uint32_t*>(buf +  14) = swapbyte32_on_le(Bitrate_); // avg bitrate
        *reinterpret_cast<uint32_t*>(buf +  18) = swapbyte32_on_le(frameSize); // max packet size
        *reinterpret_cast<uint32_t*>(buf +  22) = swapbyte32_on_le(frameSize); // avg packet size
        *reinterpret_cast<uint32_t*>(buf +  26) = swapbyte32_on_le(numFrames); // nb packets
        *reinterpret_cast<uint32_t*>(buf +  30) = swapbyte32_on_le((uint32_t)(numFrames * FrameDuration_)); // duration, ms, FFmpeg use this duration
        *reinterpret_cast<uint32_t*>(buf +  34) = 0; // preroll
        *reinterpret_cast<uint32_t*>(buf +  38) = 0; // index chunk offset
        *reinterpret_cast<uint32_t*>(buf +  42) = swapbyte32_on_le(RMF_HEADER_SZ + PROP_HEADER_SZ + MDPR_HEADER_SZ); // data chunk offset
        *reinterpret_cast<uint16_t*>(buf +  46) = swapbyte16_on_le(1); // nb streams
        *reinterpret_cast<uint16_t*>(buf +  48) = swapbyte16_on_le(1 | 2); // flags
        if (fwrite(buf, PROP_HEADER_SZ, 1, File_) != 1)
            throw std::runtime_error("Can't write PROP header");//, errno);
    }

    void WriteMDPR(uint32_t frameSize, uint32_t numFrames, size_t numChannels, bool jointStereo) {
        char buf[MDPR_HEADER_SZ] = {
            'M', 'D', 'P', 'R'};
        *reinterpret_cast<uint32_t*>(buf +  4) = swapbyte32_on_le(MDPR_HEADER_SZ);
        *reinterpret_cast<uint16_t*>(buf +  8) = 0; //chunk version
        *reinterpret_cast<uint16_t*>(buf +  10) = 0; //stream id
        *reinterpret_cast<uint32_t*>(buf +  12) = swapbyte32_on_le(Bitrate_); //max bitrate
        *reinterpret_cast<uint32_t*>(buf +  16) = swapbyte32_on_le(Bitrate_); //avg bitrate
        *reinterpret_cast<uint32_t*>(buf +  20) = swapbyte32_on_le(frameSize); //max packet size
        *reinterpret_cast<uint32_t*>(buf +  24) = swapbyte32_on_le(frameSize); //avg packet size
        *reinterpret_cast<uint32_t*>(buf +  28) = swapbyte32_on_le(0); //start time
        *reinterpret_cast<uint32_t*>(buf +  32) = 0; //preroll
        *reinterpret_cast<uint32_t*>(buf +  36) = swapbyte32_on_le(uint32_t(numFrames * FrameDuration_)); //duration, ms, RA player use this duration
        *reinterpret_cast<uint8_t*>(buf +  40) = sizeof(RA_DESC); //stream desc len
        memcpy(buf + 41, RA_DESC, sizeof(RA_DESC));
        *reinterpret_cast<uint8_t*>(buf +  41 + sizeof(RA_DESC)) = sizeof(RA_MIME); //stream mime type len
        memcpy(buf + 42 + sizeof(RA_DESC), RA_MIME, sizeof(RA_MIME));

        FillCodecData(buf + 42 + sizeof(RA_MIME) + sizeof(RA_DESC), frameSize, numChannels, jointStereo, Bitrate_);

        if (fwrite(buf, MDPR_HEADER_SZ, 1, File_) != 1)
            throw std::runtime_error("Can't write MDPR header");//, errno);
    }
};

TCompressedOutputPtr CreateRmOutput(const std::string& filename, const std::string& title, size_t numChannel,
    uint32_t numFrames, uint32_t framesize, bool jointStereo) {
    return std::unique_ptr<TRm>(new TRm(filename, title, numChannel, numFrames, framesize, jointStereo));
}
