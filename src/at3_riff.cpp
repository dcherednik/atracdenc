/*
 * AT3 RIFF container for ATRAC3Plus encoded audio.
 *
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "at3_riff.h"
#include "utf8_file.h"

#include <stdexcept>
#include <cstring>
#include <vector>

static const uint8_t ATRAC3PLUS_SUBFORMAT_GUID[16] = {
    0xBF, 0xAA, 0x23, 0xE9,
    0x58, 0xCB,
    0x71, 0x44,
    0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62
};

void TAt3Riff::WriteLE32(FILE* f, uint32_t v) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
    fwrite(buf, 1, 4, f);
}

void TAt3Riff::WriteLE16(FILE* f, uint16_t v) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    fwrite(buf, 1, 2, f);
}

void TAt3Riff::WriteFourCC(FILE* f, const char* cc) {
    fwrite(cc, 1, 4, f);
}

void TAt3Riff::WriteHeader() {
    const uint16_t wFormatTag = 0xFFFE;
    const uint32_t dwSampleRate = 44100;
    const uint16_t nBlockAlign = (uint16_t)BytesPerFrame;
    const uint32_t dwAvgBytesPerSec = dwSampleRate * BytesPerFrame / SamplesPerFrame;
    const uint16_t wBitsPerSample = 16;
    const uint16_t cbSizeActual = 34;
    const uint16_t wValidBitsPerSample = 0;
    const uint32_t dwChannelMask = (NumChannels == 2) ? 0x3 : 0x4;
    const uint16_t atrac3pVersion = 1;
    const uint32_t atrac3pSamplesPerFrame = SamplesPerFrame;
    uint8_t padding[6] = { 0 };

    WriteFourCC(File, "RIFF");
    WriteLE32(File, 0);
    WriteFourCC(File, "WAVE");
    WriteFourCC(File, "fmt ");
    WriteLE32(File, 52);
    WriteLE16(File, wFormatTag);
    WriteLE16(File, NumChannels);
    WriteLE32(File, dwSampleRate);
    WriteLE32(File, dwAvgBytesPerSec);
    WriteLE16(File, nBlockAlign);
    WriteLE16(File, wBitsPerSample);
    WriteLE16(File, cbSizeActual);
    WriteLE16(File, wValidBitsPerSample);
    WriteLE32(File, dwChannelMask);
    fwrite(ATRAC3PLUS_SUBFORMAT_GUID, 1, 16, File);
    WriteLE16(File, atrac3pVersion);
    WriteLE32(File, atrac3pSamplesPerFrame);
    fwrite(padding, 1, 6, File);
    WriteFourCC(File, "fact");
    WriteLE32(File, 4);
    WriteLE32(File, 0);
    WriteFourCC(File, "data");
    WriteLE32(File, 0);

    DataChunkSize = 0;
    NumFrames = 0;
}

void TAt3Riff::Finalize() {
    const long dataSizeOffset = 88;
    const long factDataOffset = 80;
    const long riffSizeOffset = 4;

    fseek(File, dataSizeOffset, SEEK_SET);
    WriteLE32(File, DataChunkSize);

    uint32_t totalSamples = NumFrames * SamplesPerFrame;
    fseek(File, factDataOffset, SEEK_SET);
    WriteLE32(File, totalSamples);

    fseek(File, 0, SEEK_END);
    long fileSize = ftell(File);
    fseek(File, riffSizeOffset, SEEK_SET);
    WriteLE32(File, (uint32_t)(fileSize - 8));

    fseek(File, 0, SEEK_END);
}

TAt3Riff::TAt3Riff(const std::string& filename, uint16_t channels, uint32_t framesize)
    : File(nullptr)
    , DataChunkSize(0)
    , NumFrames(0)
    , NumChannels(channels)
    , SamplesPerFrame(2048)
    , BytesPerFrame(framesize)
{
    File = NAtracDEnc::FOpenUtf8(filename, "wb");
    if (!File) {
        throw std::runtime_error("unable to open output file '" + filename + "'");
    }
    WriteHeader();
}

#ifdef _WIN32
TAt3Riff::TAt3Riff(const std::wstring& filename, uint16_t channels, uint32_t framesize)
    : File(nullptr)
    , DataChunkSize(0)
    , NumFrames(0)
    , NumChannels(channels)
    , SamplesPerFrame(2048)
    , BytesPerFrame(framesize)
{
    File = _wfopen(filename.c_str(), L"wb");
    if (!File) {
        throw std::runtime_error("unable to open output file");
    }
    WriteHeader();
}
#endif

TAt3Riff::~TAt3Riff() {
    if (File) {
        Finalize();
        fclose(File);
    }
}

void TAt3Riff::WriteFrame(std::vector<char> data) {
    if (!File) return;

    size_t written = fwrite(data.data(), 1, data.size(), File);
    if (written != data.size()) {
        fprintf(stderr, "write error in AT3 RIFF container\n");
        abort();
    }

    DataChunkSize += (uint32_t)data.size();
    NumFrames++;
}

std::string TAt3Riff::GetName() const {
    return "AT3 RIFF";
}

size_t TAt3Riff::GetChannelNum() const {
    return NumChannels;
}
