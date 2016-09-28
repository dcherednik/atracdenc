#pragma once

#include <memory>
#include <string>

#include <sndfile.hh>

#include "pcmengin.h"

class TFileAlreadyExists : public std::exception {

};

class TWavIOError : public std::exception {
    const int ErrNum = 0;
    const char* Text;
public:
    TWavIOError(const char* str, int err)
        : ErrNum(err)
        , Text(str)
    {
        (void)ErrNum;
    }
    virtual const char* what() const throw() {
        return Text;
    }
};

template<class T>
class TWavPcmReader : public IPCMReader<T> {
public:
    typedef std::function<void(TPCMBuffer<T>& data, const uint32_t size)> TLambda;
    TLambda Lambda;
    TWavPcmReader(TLambda lambda)
        : Lambda(lambda)
    {}
    void Read(TPCMBuffer<T>& data , const uint32_t size) const override {
        Lambda(data, size);
    }
};

template<class T>
class TWavPcmWriter : public IPCMWriter<T> {
public:
    typedef std::function<void(const TPCMBuffer<T>& data, const uint32_t size)> TLambda;
    TLambda Lambda;
    TWavPcmWriter(TLambda lambda)
        : Lambda(lambda)
    {}
    void Write(const TPCMBuffer<T>& data , const uint32_t size) const override {
        Lambda(data, size);
    }
};

class TWav {
    mutable SndfileHandle File;
public:
    enum Mode {
        E_READ,
        E_WRITE
    };
    TWav(const std::string& filename); // reading
    TWav(const std::string& filename, int channels, int sampleRate); //writing
    ~TWav();
    uint32_t GetChannelNum() const;
    uint32_t GetSampleRate() const;
    uint64_t GetTotalSamples() const;
    bool IsFormatSupported() const;
    template<class T>
    IPCMReader<T>* GetPCMReader() const;

    template<class T>
    IPCMWriter<T>* GetPCMWriter();
};

typedef std::unique_ptr<TWav> TWavPtr;

template<class T>
IPCMReader<T>* TWav::GetPCMReader() const {
    return new TWavPcmReader<T>([this](TPCMBuffer<T>& data, const uint32_t size) {
        if (data.Channels() != (size_t)File.channels())
            throw TWrongReadBuffer(); 
        if (size_t read = File.readf(data[0], size) != size) {
            assert(read < size);
            //fprintf(stderr, "to zero: %d\n", size-read);
            data.Zero(read, size - read);
        }
    });
}

template<class T>
IPCMWriter<T>* TWav::GetPCMWriter() {
    return new TWavPcmWriter<T>([this](const TPCMBuffer<T>& data, const uint32_t size) {
        if (data.Channels() != (size_t)File.channels())
            throw TWrongReadBuffer();
        if (File.writef(data[0], size) != size) {
            fprintf(stderr, "can't write block\n");
        }
    });
}


