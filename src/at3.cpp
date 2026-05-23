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

#include "file.h"
#include "lib/endian_tools.h"
#include "utf8_file.h"
#include <cstring>
#include <iostream>
#include <cmath>
#include <assert.h>
#include <memory>
#include <stdexcept>

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
#define ATRACDENC_PACKED
#else
#define ATRACDENC_PACKED __attribute__((packed))
#endif
struct ATRACDENC_PACKED At3WaveHeader {
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
    uint16_t extradata_size;

    struct ATRACDENC_PACKED TAt3Data {
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

    struct ATRACDENC_PACKED TAt3pData {
        // WAVEFORMATEXTENSIBLE
        uint16_t valid_bits_per_sample;
        uint32_t channel_mask;
        uint8_t subformat_guid[16];

        // "fact" subchunk
        char fact_id[4];
        uint32_t fact_size;
        uint32_t total_samples;

        // "data" subchunk
        char subchunk2_id[4];
        uint32_t subchunk2_size;
    };

    union {
        TAt3Data at3;
        TAt3pData at3p;
    } codec;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef ATRACDENC_PACKED

static constexpr uint32_t WaveSampleRate = 44100;
static constexpr uint32_t At3SamplesPerFrame = 1024;
static constexpr uint32_t At3pSamplesPerFrame = 2048;
static constexpr size_t WaveFormatBaseSize = offsetof(At3WaveHeader, codec) -
                                             offsetof(At3WaveHeader, audio_format);
static constexpr size_t At3ExtraSize = offsetof(At3WaveHeader::TAt3Data, fact_id);
static constexpr size_t At3pExtraSize = offsetof(At3WaveHeader::TAt3pData, fact_id);
static constexpr size_t At3HeaderSize = offsetof(At3WaveHeader, codec) +
                                        sizeof(At3WaveHeader::TAt3Data);
static constexpr size_t At3pHeaderSize = offsetof(At3WaveHeader, codec) +
                                         sizeof(At3WaveHeader::TAt3pData);

static_assert(At3ExtraSize == 14, "unexpected ATRAC3 WAV extradata size");
static_assert(At3pExtraSize == 22, "unexpected ATRAC3plus WAV extension size");
static_assert(At3HeaderSize == 76, "unexpected ATRAC3 WAV header size");
static_assert(At3pHeaderSize == 80, "unexpected ATRAC3plus WAV header size");

static void BackfillWaveHeader(FILE* Fp, size_t headerSize, uint64_t framesWritten,
                               uint32_t frameSize, uint32_t samplesPerFrame,
                               size_t totalSamplesOffset, size_t dataSizeOffset)
{
    const uint64_t actualFileSize = headerSize + framesWritten * uint64_t(frameSize);
    if (actualFileSize >= UINT32_MAX) {
        return;
    }

    const uint32_t chunkSize = uint32_t(actualFileSize - 8);
    const uint32_t totalSamples = uint32_t(framesWritten) * samplesPerFrame;
    const uint32_t dataSize = uint32_t(framesWritten) * frameSize;
    const uint32_t chunkSizeLE = swapbyte32_on_be(chunkSize);
    const uint32_t totalSamplesLE = swapbyte32_on_be(totalSamples);
    const uint32_t dataSizeLE = swapbyte32_on_be(dataSize);

    fseek(Fp, offsetof(At3WaveHeader, chunk_size), SEEK_SET);
    fwrite(&chunkSizeLE, sizeof(uint32_t), 1, Fp);
    fseek(Fp, static_cast<long>(totalSamplesOffset), SEEK_SET);
    fwrite(&totalSamplesLE, sizeof(uint32_t), 1, Fp);
    fseek(Fp, static_cast<long>(dataSizeOffset), SEEK_SET);
    fwrite(&dataSizeLE, sizeof(uint32_t), 1, Fp);
}

class TAt3 : public ICompressedOutput {
public:
    TAt3(const std::string &filename, size_t numChannels,
        uint32_t numFrames, uint32_t frameSize, bool jointStereo)
        : Fp(NAtracDEnc::FOpenUtf8(filename, "wb"))
        , FrameSize(frameSize)
        , FramesWritten(0)
    {
        if (!Fp) {
            throw std::runtime_error("unable to open output file '" + filename + "'");
        }

        At3WaveHeader header;
        memset(&header, 0, sizeof(header));

        uint64_t file_size = At3HeaderSize + uint64_t(numFrames) * uint64_t(frameSize);

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
        header.subchunk1_size = swapbyte32_on_be(WaveFormatBaseSize + At3ExtraSize);

        // libnetmd: #define NETMD_RIFF_FORMAT_TAG_ATRAC3 0x270
        // mmreg.h (mingw-w64): WAVE_FORMAT_SONY_SCX 0x270
        // riff.c (ffmpeg): AV_CODEC_ID_ATRAC3 0x0270
        header.audio_format = swapbyte16_on_be(0x270);
        header.num_channels = swapbyte16_on_be(numChannels);
        header.sample_rate = swapbyte32_on_be(WaveSampleRate);
        header.byte_rate = swapbyte32_on_be(frameSize * WaveSampleRate / At3SamplesPerFrame);
        header.block_align = swapbyte16_on_be(frameSize);
        header.bits_per_sample = swapbyte16_on_be(0);
        header.extradata_size = swapbyte16_on_be(At3ExtraSize);

        header.codec.at3.unknown0 = swapbyte16_on_be(1);
        // 1024 samples × 2 channels × 2 bytes = 4096 (0x1000).  Sony's encoder
        // writes this value; PSP tool and ffmpeg rely on it for frame sizing.
        header.codec.at3.bytes_per_frame = swapbyte32_on_be(0x1000);
        header.codec.at3.coding_mode = swapbyte16_on_be(jointStereo ? 0x0001 : 0x0000);
        header.codec.at3.coding_mode2 = header.codec.at3.coding_mode; // already byte-swapped (if needed)
        header.codec.at3.unknown1 = swapbyte16_on_be(1);
        header.codec.at3.unknown2 = swapbyte16_on_be(0);

        memcpy(header.codec.at3.fact_id, "fact", 4);
        header.codec.at3.fact_size = swapbyte32_on_be(8);
        header.codec.at3.total_samples = swapbyte32_on_be(uint32_t(numFrames) * At3SamplesPerFrame);
        header.codec.at3.samples_per_frame = swapbyte32_on_be(At3SamplesPerFrame);

        memcpy(header.codec.at3.subchunk2_id, "data", 4);
        header.codec.at3.subchunk2_size = swapbyte32_on_be(numFrames * frameSize);

        if (fwrite(&header, 1, At3HeaderSize, Fp.get()) != At3HeaderSize) {
            throw std::runtime_error("Cannot write WAV header to file");
        }
    }

    virtual ~TAt3() override {
        // The PCM engine can flush more frames than initially estimated
        // (encoder look-ahead tail).  Backfill the length fields so
        // RIFF chunk_size, fact total_samples, and data subchunk_size
        // reflect the actual frame count on disk.
        if (FramesWritten > 0) {
            BackfillWaveHeader(Fp.get(), At3HeaderSize, FramesWritten, FrameSize, At3SamplesPerFrame,
                               offsetof(At3WaveHeader, codec) +
                                   offsetof(At3WaveHeader::TAt3Data, total_samples),
                               offsetof(At3WaveHeader, codec) +
                                   offsetof(At3WaveHeader::TAt3Data, subchunk2_size));
        }
    }

    virtual void WriteFrame(std::vector<char> data) override {
        if (fwrite(data.data(), 1, data.size(), Fp.get()) != data.size()) {
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
    TFilePtr Fp;
    uint32_t FrameSize;
    uint64_t FramesWritten;
};

static const uint8_t Atrac3plusSubformatGuid[16] = {
    0xBF, 0xAA, 0x23, 0xE9, 0x58, 0xCB, 0x71, 0x44,
    0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62
};

static uint32_t GetWaveChannelMask(size_t numChannels) {
    switch (numChannels) {
        case 1:
            return 0x00000004; // front center
        case 2:
            return 0x00000003; // front left | front right
        default:
            return 0;
    }
}

class TAt3p : public ICompressedOutput {
public:
    TAt3p(const std::string &filename, size_t numChannels,
        uint32_t numFrames, uint32_t frameSize)
        : Fp(NAtracDEnc::FOpenUtf8(filename, "wb"))
        , FrameSize(frameSize)
        , FramesWritten(0)
        , NumChannels(numChannels)
    {
        if (!Fp) {
            throw std::runtime_error("unable to open output file '" + filename + "'");
        }
        if (frameSize > UINT16_MAX) {
            throw std::runtime_error("ATRAC3plus frame size is too large for WAV block_align");
        }
        if (numChannels > UINT16_MAX) {
            throw std::runtime_error("Too many channels for WAV output");
        }

        At3WaveHeader header;
        memset(&header, 0, sizeof(header));

        const uint64_t file_size = At3pHeaderSize + uint64_t(numFrames) * uint64_t(frameSize);
        if (file_size >= UINT32_MAX) {
            throw std::runtime_error("File size is too big for this file format");
        }

        memcpy(header.riff_chunk_id, "RIFF", 4);
        header.chunk_size = swapbyte32_on_be(uint32_t(file_size - 8));
        memcpy(header.riff_format, "WAVE", 4);

        memcpy(header.subchunk1_id, "fmt ", 4);
        header.subchunk1_size = swapbyte32_on_be(WaveFormatBaseSize + At3pExtraSize);
        header.audio_format = swapbyte16_on_be(0xFFFE); // WAVE_FORMAT_EXTENSIBLE
        header.num_channels = swapbyte16_on_be(uint16_t(numChannels));
        header.sample_rate = swapbyte32_on_be(WaveSampleRate);
        header.byte_rate = swapbyte32_on_be(frameSize * WaveSampleRate / At3pSamplesPerFrame);
        header.block_align = swapbyte16_on_be(uint16_t(frameSize));
        header.bits_per_sample = swapbyte16_on_be(16);
        header.extradata_size = swapbyte16_on_be(At3pExtraSize);
        header.codec.at3p.valid_bits_per_sample = swapbyte16_on_be(16);
        header.codec.at3p.channel_mask = swapbyte32_on_be(GetWaveChannelMask(numChannels));
        memcpy(header.codec.at3p.subformat_guid, Atrac3plusSubformatGuid, sizeof(header.codec.at3p.subformat_guid));

        memcpy(header.codec.at3p.fact_id, "fact", 4);
        header.codec.at3p.fact_size = swapbyte32_on_be(sizeof(uint32_t));
        header.codec.at3p.total_samples = swapbyte32_on_be(uint32_t(numFrames) * At3pSamplesPerFrame);

        memcpy(header.codec.at3p.subchunk2_id, "data", 4);
        header.codec.at3p.subchunk2_size = swapbyte32_on_be(numFrames * frameSize);

        if (fwrite(&header, 1, At3pHeaderSize, Fp.get()) != At3pHeaderSize) {
            throw std::runtime_error("Cannot write WAV header to file");
        }
    }

    virtual ~TAt3p() override {
        if (FramesWritten > 0) {
            BackfillWaveHeader(Fp.get(), At3pHeaderSize, FramesWritten, FrameSize, At3pSamplesPerFrame,
                               offsetof(At3WaveHeader, codec) +
                                   offsetof(At3WaveHeader::TAt3pData, total_samples),
                               offsetof(At3WaveHeader, codec) +
                                   offsetof(At3WaveHeader::TAt3pData, subchunk2_size));
        }
    }

    virtual void WriteFrame(std::vector<char> data) override {
        if (data.size() != FrameSize) {
            throw std::runtime_error("Unexpected ATRAC3plus frame size");
        }
        if (fwrite(data.data(), 1, data.size(), Fp.get()) != data.size()) {
            throw std::runtime_error("Cannot write AT3 data to file");
        }
        ++FramesWritten;
    }

    std::string GetName() const override {
        return {};
    }

    size_t GetChannelNum() const override {
        return NumChannels;
    }

private:
    TFilePtr Fp;
    uint32_t FrameSize;
    uint64_t FramesWritten;
    size_t NumChannels;
};

} //namespace

TCompressedOutputPtr
CreateAt3Output(const std::string& filename, size_t numChannel,
        uint32_t numFrames, uint32_t framesize, bool jointStereo)
{
    return std::unique_ptr<TAt3>(new TAt3(filename, numChannel, numFrames, framesize, jointStereo));
}

TCompressedOutputPtr
CreateAt3POutput(const std::string& filename, size_t numChannel,
        uint32_t numFrames, uint32_t framesize)
{
    return std::unique_ptr<TAt3p>(new TAt3p(filename, numChannel, numFrames, framesize));
}
