/*
 * Built-in PCM I/O backend - reads standard WAV files without external libraries.
 * Supports 16-bit and 24-bit PCM WAV files.
 *
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "wav.h"
#include "utf8_file.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

// WAV header structures
#pragma pack(push, 1)
struct WavRiffHeader {
    char riffId[4];      // "RIFF"
    uint32_t fileSize;
    char waveId[4];      // "WAVE"
};

struct WavFmtChunk {
    char fmtId[4];       // "fmt "
    uint32_t chunkSize;
    uint16_t audioFormat; // 1 = PCM, 3 = IEEE float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

static uint32_t ReadLE32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t ReadLE16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

class TPCMIOWavBuiltin : public IPCMProviderImpl {
public:
    TPCMIOWavBuiltin(const std::string& filename)
        : Fp(nullptr)
        , Channels(0)
        , SampleRate(0)
        , TotalSamples(0)
        , BitsPerSample(0)
        , DataOffset(0)
        , DataSize(0)
        , BytesRead(0)
    {
        Fp = NAtracDEnc::FOpenUtf8(filename, "rb");
        if (!Fp) {
            throw std::runtime_error("unable to open input file '" + filename + "'");
        }
        ParseHeader();
    }

    ~TPCMIOWavBuiltin() {
        if (Fp) fclose(Fp);
    }

    size_t GetChannelsNum() const override {
        return Channels;
    }

    size_t GetSampleRate() const override {
        return SampleRate;
    }

    size_t GetTotalSamples() const override {
        return TotalSamples;
    }

    size_t Read(TPCMBuffer& buf, size_t sz) override {
        if (!Fp) return 0;

        const size_t bytesPerSample = BitsPerSample / 8;
        const size_t frameBytes = Channels * bytesPerSample;
        const size_t maxFrames = DataSize / frameBytes;
        const size_t framesDone = BytesRead / frameBytes;
        const size_t framesRemaining = maxFrames - framesDone;

        if (framesRemaining == 0) return 0;

        size_t framesToRead = std::min(sz, framesRemaining);
        std::vector<uint8_t> rawBuf(framesToRead * frameBytes);

        size_t nRead = fread(rawBuf.data(), frameBytes, framesToRead, Fp);
        if (nRead == 0) return 0;

        BytesRead += nRead * frameBytes;

        // Convert to float
        for (size_t i = 0; i < nRead; i++) {
            float* out = buf[i];
            const uint8_t* frameData = rawBuf.data() + i * frameBytes;
            for (size_t c = 0; c < Channels; c++) {
                const uint8_t* sampleData = frameData + c * bytesPerSample;
                if (BitsPerSample == 16) {
                    int16_t val = (int16_t)ReadLE16(sampleData);
                    out[c] = (float)val / 32768.0f;
                } else if (BitsPerSample == 24) {
                    int32_t val = (int32_t)(sampleData[0] | (sampleData[1] << 8) | (sampleData[2] << 16));
                    if (val & 0x800000) val |= 0xFF000000; // sign extend
                    out[c] = (float)val / 8388608.0f;
                } else if (BitsPerSample == 32) {
                    int32_t val = (int32_t)ReadLE32(sampleData);
                    out[c] = (float)val / 2147483648.0f;
                } else {
                    out[c] = 0.0f;
                }
            }
        }

        return nRead;
    }

    size_t Write(const TPCMBuffer& /*buf*/, size_t /*sz*/) override {
        // Read-only backend
        return 0;
    }

private:
    void ParseHeader() {
        uint8_t header[12];
        if (fread(header, 1, 12, Fp) != 12) {
            throw std::runtime_error("invalid WAV file: too short");
        }

        if (memcmp(header, "RIFF", 4) != 0) {
            throw std::runtime_error("invalid WAV file: missing RIFF marker");
        }
        if (memcmp(header + 8, "WAVE", 4) != 0) {
            throw std::runtime_error("invalid WAV file: missing WAVE marker");
        }

        bool foundFmt = false;
        bool foundData = false;

        while (!foundData) {
            uint8_t chunkHeader[8];
            if (fread(chunkHeader, 1, 8, Fp) != 8) break;

            uint32_t chunkSize = ReadLE32(chunkHeader + 4);

            if (memcmp(chunkHeader, "fmt ", 4) == 0) {
                // Parse format chunk
                std::vector<uint8_t> fmtData(chunkSize);
                if (fread(fmtData.data(), 1, chunkSize, Fp) != chunkSize) {
                    throw std::runtime_error("invalid WAV file: truncated fmt chunk");
                }

                uint16_t audioFormat = ReadLE16(fmtData.data());
                if (audioFormat != 1 && audioFormat != 3) { // PCM or IEEE float
                    throw std::runtime_error("unsupported WAV format: only PCM (1) and IEEE float (3) are supported");
                }

                Channels = ReadLE16(fmtData.data() + 2);
                SampleRate = ReadLE32(fmtData.data() + 4);
                BitsPerSample = ReadLE16(fmtData.data() + 14);

                if (Channels == 0 || SampleRate == 0 || BitsPerSample == 0) {
                    throw std::runtime_error("invalid WAV format parameters");
                }

                foundFmt = true;
            } else if (memcmp(chunkHeader, "data", 4) == 0) {
                if (!foundFmt) {
                    throw std::runtime_error("invalid WAV file: data chunk before fmt chunk");
                }

                DataOffset = ftell(Fp);
                DataSize = chunkSize;
                TotalSamples = DataSize / (Channels * (BitsPerSample / 8));

                foundData = true;
            } else {
                // Skip unknown chunk
                if (chunkSize > 0) {
                    fseek(Fp, chunkSize, SEEK_CUR);
                }
            }
        }

        if (!foundFmt || !foundData) {
            throw std::runtime_error("invalid WAV file: missing fmt or data chunk");
        }
    }

    FILE* Fp;
    size_t Channels;
    size_t SampleRate;
    size_t TotalSamples;
    size_t BitsPerSample;
    long DataOffset;
    size_t DataSize;
    size_t BytesRead;
};

class TPCMIOWavBuiltinWriter : public IPCMProviderImpl {
public:
    TPCMIOWavBuiltinWriter(const std::string& filename, int channels, int sampleRate)
        : Fp(nullptr)
        , Channels(channels)
        , SampleRate(sampleRate)
        , TotalSamples(0)
        , DataSize(0)
    {
        Fp = NAtracDEnc::FOpenUtf8(filename, "wb");
        if (!Fp) {
            throw std::runtime_error("unable to open output file '" + filename + "'");
        }
        WriteHeader();
    }

    ~TPCMIOWavBuiltinWriter() {
        if (Fp) {
            Finalize();
            fclose(Fp);
        }
    }

    size_t GetChannelsNum() const override { return Channels; }
    size_t GetSampleRate() const override { return SampleRate; }
    size_t GetTotalSamples() const override { return TotalSamples; }

    size_t Read(TPCMBuffer&, size_t) override { return 0; }

    size_t Write(const TPCMBuffer& buf, size_t sz) override {
        if (!Fp) return 0;

        const size_t frameBytes = Channels * 2; // 16-bit
        std::vector<uint8_t> rawBuf(sz * frameBytes);

        for (size_t i = 0; i < sz; i++) {
            const float* in = buf[i];
            uint8_t* frameData = rawBuf.data() + i * frameBytes;
            for (size_t c = 0; c < Channels; c++) {
                float sample = in[c];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                int16_t val = (int16_t)(sample * 32767.0f);
                frameData[c * 2] = (uint8_t)(val & 0xFF);
                frameData[c * 2 + 1] = (uint8_t)((val >> 8) & 0xFF);
            }
        }

        size_t written = fwrite(rawBuf.data(), frameBytes, sz, Fp);
        DataSize += written * frameBytes;
        TotalSamples += written;
        return written;
    }

private:
    void WriteHeader() {
        // Write placeholder header
        uint8_t header[44] = {};
        memcpy(header, "RIFF", 4);
        memcpy(header + 8, "WAVE", 4);
        memcpy(header + 12, "fmt ", 4);
        uint32_t fmtSize = 16;
        memcpy(header + 16, &fmtSize, 4);
        uint16_t audioFormat = 1; // PCM
        memcpy(header + 20, &audioFormat, 2);
        uint16_t ch = (uint16_t)Channels;
        memcpy(header + 22, &ch, 2);
        uint32_t sr = (uint32_t)SampleRate;
        memcpy(header + 24, &sr, 4);
        uint32_t byteRate = (uint32_t)(SampleRate * Channels * 2);
        memcpy(header + 28, &byteRate, 4);
        uint16_t blockAlign = (uint16_t)(Channels * 2);
        memcpy(header + 32, &blockAlign, 2);
        uint16_t bps = 16;
        memcpy(header + 34, &bps, 2);
        memcpy(header + 36, "data", 4);
        fwrite(header, 1, 44, Fp);
    }

    void Finalize() {
        uint32_t fileSize = (uint32_t)(DataSize + 36);
        fseek(Fp, 4, SEEK_SET);
        fwrite(&fileSize, 4, 1, Fp);
        fseek(Fp, 40, SEEK_SET);
        uint32_t ds = (uint32_t)DataSize;
        fwrite(&ds, 4, 1, Fp);
    }

    FILE* Fp;
    size_t Channels;
    size_t SampleRate;
    size_t TotalSamples;
    size_t DataSize;
};

IPCMProviderImpl* CreatePCMIOReadImpl(const std::string& path) {
    return new TPCMIOWavBuiltin(path);
}

IPCMProviderImpl* CreatePCMIOWriteImpl(const std::string& path, int channels, int sampleRate) {
    return new TPCMIOWavBuiltinWriter(path, channels, sampleRate);
}
