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
    typedef std::function<void(std::vector<std::vector<T>>& data, const uint32_t size)> TLambda;
    TLambda Lambda;
    TWavPcmReader(TLambda lambda)
        : Lambda(lambda)
    {}
    void Read(std::vector<std::vector<T>>& data , const uint32_t size) const override {
        Lambda(data, size);
    }
};

template<class T>
class TWavPcmWriter : public IPCMWriter<T> {
public:
    typedef std::function<void(const std::vector<std::vector<T>>& data, const uint32_t size)> TLambda;
    TLambda Lambda;
    TWavPcmWriter(TLambda lambda)
        : Lambda(lambda)
    {}
    void Write(const std::vector<std::vector<T>>& data , const uint32_t size) const override {
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
    template<class T>
    IPCMReader<T>* GetPCMReader() const;

    template<class T>
    IPCMWriter<T>* GetPCMWriter();
};

typedef std::unique_ptr<TWav> TWavPtr;

template<class T>
IPCMReader<T>* TWav::GetPCMReader() const {
    return new TWavPcmReader<T>([this](std::vector<std::vector<T>>& data, const uint32_t size) {
        if (data[0].size() != File.channels())
            throw TWrongReadBuffer(); 
        uint32_t dataRead = 0;
        for (uint32_t i = 0; i < size; i++) {
            dataRead += File.readf(&data[i][0], 1);
        }
    });
}

template<class T>
IPCMWriter<T>* TWav::GetPCMWriter() {
    return new TWavPcmWriter<T>([this](const std::vector<std::vector<T>>& data, const uint32_t size) {
        if (data[0].size() != File.channels())
            throw TWrongReadBuffer();
        uint32_t dataWrite = 0;
        for (uint32_t i = 0; i < size; i++) {
            dataWrite += File.writef(&data[i][0], 1);
        }
    });
}


