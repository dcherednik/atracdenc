#pragma once
#include <vector>
#include <array>
#include <string>
#include <memory>

class ICompressedIO {
public:
    class TFrame {
        uint64_t Sz;
        char* Data;
        TFrame(const TFrame& src) = delete;
        TFrame() = delete;
    public:
        TFrame(uint64_t sz)
            : Sz(sz)
        {
            Data = new char[Sz];
        }
        ~TFrame() {
            delete[] Data;
        }
        uint64_t Size() const { return Sz; }
        char* Get() { return Data; }
    };
    virtual void WriteFrame(std::vector<char> data) = 0;
    virtual std::unique_ptr<TFrame> ReadFrame() = 0;
    virtual std::string GetName() const = 0;
    virtual int GetChannelNum() const = 0;
    virtual long long GetLengthInSamples() const = 0;
    virtual ~ICompressedIO() {}
};

typedef std::unique_ptr<ICompressedIO> TCompressedIOPtr;
