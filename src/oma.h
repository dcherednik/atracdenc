#pragma once

#include "compressed_io.h"
#include "oma/liboma/include/oma.h"


class TOma : public ICompressedIO {
    OMAFILE* File;
public:
    TOma(const std::string& filename, const std::string& title, int numChannel, uint32_t numFrames, int cid, uint32_t framesize);
    ~TOma();
    std::unique_ptr<TFrame> ReadFrame() override;
    void WriteFrame(std::vector<char> data) override;
    std::string GetName() const override;
    int GetChannelNum() const override;
    long long GetLengthInSamples() const override;
};
