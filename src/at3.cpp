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

#include "at3.h"

#include "lib/endian_tools.h"
#include <cstring>
#include <iostream>
#include <cmath>
#include <assert.h>

/*
 * ATRAC3-in-WAV file format.
 *
 * Documented for example here:
 *   - ffmpeg: libavcodec/atrac3.c (atrac3_decode_init() talks about "extradata")
 *   - libnetmd: libnetmd/secure.c (netmd_write_wav_header() has "ATRAC extensions")
 */

namespace {

// Based on http://soundfile.sapp.org/doc/WaveFormat/ + ffmpeg/libnetmd docs
#ifdef _MSC_VER
#pragma pack(push, 1)
struct
#else
struct __attribute__((packed))
#endif
At3WaveHeader {
    // "RIFF" "WAVE" header
    char riff_chunk_id[4];
    uint32_t chunk_size;
    char riff_format[4];

    // "fmt " subchunk
    char subchunk1_id[4];
    uint32_t subchunk1_size;

    // WAVEFORMAT
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    // WAVEFORMATEX cbSize
    uint16_t extradata_size; // 14

    // atrac3 extradata
    uint16_t unknown0; // always 1
    uint32_t bytes_per_frame; // PCM bytes represented per frame = 1024 samples * 2ch * 2B = 0x1000
    uint16_t coding_mode; // 1 = joint stereo, 0 = stereo
    uint16_t coding_mode2; // same as <coding_mode>
    uint16_t unknown1; // always 1
    uint16_t unknown2; // always 0

    // "fact" subchunk — required by Sony's psp_at3tool decoder and by ffmpeg
    // for encoder-delay compensation.  Without it, PSP tool rejects files
    // > ~40 s with "input file is illegal file or over 2G Byte".
    char fact_id[4];
    uint32_t fact_size;       // 8
    uint32_t total_samples;   // total PCM samples per channel
    uint32_t samples_per_frame; // 1024 for ATRAC3

    // "data" subchunk
    char subchunk2_id[4];
    uint32_t subchunk2_size;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

class TAt3 : public ICompressedOutput {
public:
    TAt3(const std::string &filename, size_t numChannels,
        uint32_t numFrames, uint32_t frameSize, bool jointStereo)
        : fp(fopen(filename.c_str(), "wb"))
        , FrameSize(frameSize)
        , FramesWritten(0)
    {
        if (!fp) {
            throw std::runtime_error("Cannot open file to write");
        }

        struct At3WaveHeader header;
        memset(&header, 0, sizeof(header));

        uint64_t file_size = sizeof(struct At3WaveHeader) + uint64_t(numFrames) * uint64_t(frameSize);

        if (file_size >= UINT32_MAX) {
            throw std::runtime_error("File size is too big for this file format");
        }

        memcpy(header.riff_chunk_id, "RIFF", 4);
        // RIFF spec: chunk_size is the size of everything after this field,
        // i.e. file_size - 8 (RIFF marker + size field itself).
        header.chunk_size = swapbyte32_on_be(file_size - 8);
        memcpy(header.riff_format, "WAVE", 4);

        memcpy(header.subchunk1_id, "fmt ", 4);
        // fmt chunk ends where the next chunk ("fact") begins.
        header.subchunk1_size = swapbyte32_on_be(offsetof(struct At3WaveHeader, fact_id) -
                                                 offsetof(struct At3WaveHeader, audio_format));

        // libnetmd: #define NETMD_RIFF_FORMAT_TAG_ATRAC3 0x270
        // mmreg.h (mingw-w64): WAVE_FORMAT_SONY_SCX 0x270
        // riff.c (ffmpeg): AV_CODEC_ID_ATRAC3 0x0270
        header.audio_format = swapbyte16_on_be(0x270);
        header.num_channels = swapbyte16_on_be(numChannels);
        header.sample_rate = swapbyte32_on_be(44100);
        header.byte_rate = swapbyte32_on_be(frameSize * header.sample_rate / 1024);
        header.block_align = swapbyte16_on_be(frameSize);
        header.bits_per_sample = swapbyte16_on_be(0);
        header.extradata_size = swapbyte16_on_be(offsetof(struct At3WaveHeader, fact_id) -
                                                 offsetof(struct At3WaveHeader, unknown0));

        header.unknown0 = swapbyte16_on_be(1);
        // 1024 samples × 2 channels × 2 bytes = 4096 (0x1000).  Sony's encoder
        // writes this value; PSP tool and ffmpeg rely on it for frame sizing.
        header.bytes_per_frame = swapbyte32_on_be(0x1000);
        header.coding_mode = swapbyte16_on_be(jointStereo ? 0x0001 : 0x0000);
        header.coding_mode2 = header.coding_mode; // already byte-swapped (if needed)
        header.unknown1 = swapbyte16_on_be(1);
        header.unknown2 = swapbyte16_on_be(0);

        memcpy(header.fact_id, "fact", 4);
        header.fact_size = swapbyte32_on_be(8);
        header.total_samples = swapbyte32_on_be(uint32_t(numFrames) * 1024);
        header.samples_per_frame = swapbyte32_on_be(1024);

        memcpy(header.subchunk2_id, "data", 4);
        header.subchunk2_size = swapbyte32_on_be(numFrames * frameSize); // TODO

        if (fwrite(&header, 1, sizeof(header), fp) != sizeof(header)) {
            throw std::runtime_error("Cannot write WAV header to file");
        }
    }

    virtual ~TAt3() override {
        // The PCM engine can flush more frames than initially estimated
        // (encoder look-ahead tail).  Backfill the length fields so
        // RIFF chunk_size, fact total_samples, and data subchunk_size
        // reflect the actual frame count on disk.
        if (FramesWritten > 0) {
            const uint64_t actualFileSize = sizeof(struct At3WaveHeader) +
                                            uint64_t(FramesWritten) * uint64_t(FrameSize);
            if (actualFileSize < UINT32_MAX) {
                const uint32_t chunkSize = uint32_t(actualFileSize - 8);
                const uint32_t totalSamples = uint32_t(FramesWritten) * 1024u;
                const uint32_t dataSize = uint32_t(FramesWritten) * FrameSize;
                const uint32_t chunkSizeLE = swapbyte32_on_be(chunkSize);
                const uint32_t totalSamplesLE = swapbyte32_on_be(totalSamples);
                const uint32_t dataSizeLE = swapbyte32_on_be(dataSize);
                fseek(fp, offsetof(struct At3WaveHeader, chunk_size), SEEK_SET);
                fwrite(&chunkSizeLE, sizeof(uint32_t), 1, fp);
                fseek(fp, offsetof(struct At3WaveHeader, total_samples), SEEK_SET);
                fwrite(&totalSamplesLE, sizeof(uint32_t), 1, fp);
                fseek(fp, offsetof(struct At3WaveHeader, subchunk2_size), SEEK_SET);
                fwrite(&dataSizeLE, sizeof(uint32_t), 1, fp);
            }
        }
        fclose(fp);
    }

    virtual void WriteFrame(std::vector<char> data) override {
        if (fwrite(data.data(), 1, data.size(), fp) != data.size()) {
            throw std::runtime_error("Cannot write AT3 data to file");
        }
        ++FramesWritten;
    }

    std::string GetName() const override {
        return {};
    }

    size_t GetChannelNum() const override {
        return 2;
    }

private:
    FILE *fp;
    uint32_t FrameSize;
    uint64_t FramesWritten;
};

} //namespace

TCompressedOutputPtr
CreateAt3Output(const std::string& filename, size_t numChannel,
        uint32_t numFrames, uint32_t framesize, bool jointStereo)
{
    return std::unique_ptr<TAt3>(new TAt3(filename, numChannel, numFrames, framesize, jointStereo));
}
