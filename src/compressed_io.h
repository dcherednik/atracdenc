#pragma once
#include <vector>
#include <array>
#include <string>

template<int FRAME_SZ>
class ICompressedIO {
public:
    typedef std::array<char, FRAME_SZ> TFrame;
    virtual void WriteFrame(std::vector<char> data) = 0;
    virtual std::unique_ptr<TFrame> ReadFrame() = 0;
    virtual std::string GetName() const = 0;
    virtual int GetChannelNum() const = 0;
    virtual long long GetLengthInSamples() const = 0;
    virtual ~ICompressedIO() {}
};
