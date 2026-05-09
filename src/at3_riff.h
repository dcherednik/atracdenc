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

#pragma once

#include "compressed_io.h"
#include <string>
#include <cstdint>
#include <cstdio>

class TAt3Riff : public ICompressedOutput {
    FILE* File;
    uint32_t DataChunkSize;
    uint32_t NumFrames;
    uint16_t NumChannels;
    uint32_t SamplesPerFrame;
    uint32_t BytesPerFrame;

    static void WriteLE32(FILE* f, uint32_t v);
    static void WriteLE16(FILE* f, uint16_t v);
    static void WriteFourCC(FILE* f, const char* cc);
    void WriteHeader();
    void Finalize();

public:
    TAt3Riff(const std::string& filename, uint16_t channels, uint32_t framesize);
#ifdef _WIN32
    TAt3Riff(const std::wstring& filename, uint16_t channels, uint32_t framesize);
#endif
    ~TAt3Riff();

    void WriteFrame(std::vector<char> data) override;
    std::string GetName() const override;
    size_t GetChannelNum() const override;
};
