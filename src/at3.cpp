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

#include "utf8_file.h"
#include <cstring>
#include <stdexcept>

/*
 * ATRAC3-in-WAV RIFF container.
 *
 * Compatible with ffmpeg's ATRAC3 decoder (libavcodec/atrac3.c).
 *
 * RIFF structure:
 *   RIFF header (12 bytes)
 *   fmt  chunk: 32 bytes total = 8 header + 18 WaveFormatEx + 14 extradata
 *   fact chunk: 16 bytes total = 8 header + 4 frame_count + 4 samples_per_frame
 *   data chunk: compressed ATRAC3 frames
 *
 * Extradata layout (14 bytes, parsed by ffmpeg):
 *   [0-1]   mode              (uint16 LE) = 1
 *   [2-5]   reserved          (uint32 LE) = 0
 *   [6-7]   coding_mode       (uint16 LE) = 0 (stereo) or 1 (joint stereo)
 *   [8-9]   coding_mode_dup   (uint16 LE) = same as [6-7]
 *   [10-11] frame_factor      (uint16 LE) = 1
 *   [12-13] reserved          (uint16 LE) = 0
 */

static void WriteLE16(FILE* f, uint16_t v) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    fwrite(buf, 1, 2, f);
}

static void WriteLE32(FILE* f, uint32_t v) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
    fwrite(buf, 1, 4, f);
}

static void WriteFourCC(FILE* f, const char* cc) {
    fwrite(cc, 1, 4, f);
}

/*
 * RIFF file layout (all offsets from start of file):
 *   0:  "RIFF" (4)
 *   4:  file size - 8 (4)
 *   8:  "WAVE" (4)
 *  12:  "fmt " (4)
 *  16:  fmt chunk size = 32 (4)
 *  20:  WaveFormatEx (18 bytes)
 *  38:  extradata (14 bytes)
 *  52:  "fact" (4)
 *  56:  fact chunk size = 8 (4)
 *  60:  total samples (4)
 *  64:  samples per frame = 1024 (4)
 *  68:  "data" (4)
 *  72:  data chunk size (4)
 *  76:  compressed data begins
 */
static const long OFF_RIFF_SIZE  = 4;
static const long OFF_FACT_COUNT = 60;
static const long OFF_FACT_SPF   = 64;
static const long OFF_DATA_SIZE  = 72;

class TAt3 : public ICompressedOutput {
public:
    TAt3(const std::string& filename, size_t numChannels,
        uint32_t /*numFrames*/, uint32_t frameSize, bool jointStereo)
        : Fp(NAtracDEnc::FOpenUtf8(filename, "wb"))
        , FrameSize(frameSize)
        , FramesWritten(0)
        , NumChannels((uint16_t)numChannels)
    {
        if (!Fp) {
            throw std::runtime_error("unable to open output file '" + filename + "'");
        }

        const uint16_t blockAlign = (uint16_t)frameSize;
        const uint32_t avgBytesPerSec = ((uint32_t)blockAlign * 44100u + 512u) / 1024u;

        // RIFF header
        WriteFourCC(Fp, "RIFF");
        WriteLE32(Fp, 0);  // placeholder
        WriteFourCC(Fp, "WAVE");

        // fmt chunk (32 bytes = 8 header + 18 WaveFormatEx + 14 extradata)
        WriteFourCC(Fp, "fmt ");
        WriteLE32(Fp, 32);

        // WaveFormatEx (18 bytes)
        WriteLE16(Fp, 0x0270);           // wFormatTag = WAVE_FORMAT_ATRAC3
        WriteLE16(Fp, (uint16_t)numChannels);
        WriteLE32(Fp, 44100);            // nSamplesPerSec
        WriteLE32(Fp, avgBytesPerSec);   // nAvgBytesPerSec
        WriteLE16(Fp, blockAlign);       // nBlockAlign
        WriteLE16(Fp, 0);                // wBitsPerSample
        WriteLE16(Fp, 14);               // cbSize

        // ATRAC3 extradata (14 bytes) — ffmpeg-compatible layout
        WriteLE16(Fp, 1);                            // [0-1]  mode = 1
        WriteLE32(Fp, 0);                            // [2-5]  reserved = 0
        WriteLE16(Fp, jointStereo ? 1 : 0);          // [6-7]  coding_mode
        WriteLE16(Fp, jointStereo ? 1 : 0);          // [8-9]  coding_mode duplicate
        WriteLE16(Fp, 1);                            // [10-11] frame_factor = 1
        WriteLE16(Fp, 0);                            // [12-13] reserved = 0

        // fact chunk (16 bytes = 8 header + 8 data)
        WriteFourCC(Fp, "fact");
        WriteLE32(Fp, 8);
        WriteLE32(Fp, 0);  // placeholder: total samples
        WriteLE32(Fp, 1024);  // samples per frame

        // data chunk header
        WriteFourCC(Fp, "data");
        WriteLE32(Fp, 0);  // placeholder
    }

    ~TAt3() override {
        if (Fp && FramesWritten > 0) {
            Finalize();
        }
        if (Fp) {
            fclose(Fp);
        }
    }

    void WriteFrame(std::vector<char> data) override {
        if (!Fp) return;
        if (fwrite(data.data(), 1, data.size(), Fp) != data.size()) {
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
    void Finalize() {
        // Backfill data chunk size
        fseek(Fp, OFF_DATA_SIZE, SEEK_SET);
        WriteLE32(Fp, (uint32_t)FramesWritten * FrameSize);

        // Backfill fact chunk: total samples = frames * 1024
        fseek(Fp, OFF_FACT_COUNT, SEEK_SET);
        WriteLE32(Fp, (uint32_t)FramesWritten * 1024u);

        // Backfill RIFF chunk size
        fseek(Fp, 0, SEEK_END);
        long fileSize = ftell(Fp);
        fseek(Fp, OFF_RIFF_SIZE, SEEK_SET);
        WriteLE32(Fp, (uint32_t)(fileSize - 8));

        fseek(Fp, 0, SEEK_END);
    }

    FILE* Fp;
    uint32_t FrameSize;
    uint64_t FramesWritten;
    uint16_t NumChannels;
};

TCompressedOutputPtr
CreateAt3Output(const std::string& filename, size_t numChannel,
        uint32_t numFrames, uint32_t framesize, bool jointStereo)
{
    return std::unique_ptr<TAt3>(new TAt3(filename, numChannel, numFrames, framesize, jointStereo));
}
