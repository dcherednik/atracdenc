#pragma once

#include <memory>
#include <string>

#include "pcmengin.h"

typedef struct  WAV_HEADER {
    char                RIFF[4];        // RIFF Header      Magic header
    unsigned int        ChunkSize;      // RIFF Chunk Size
    char                WAVE[4];        // WAVE Header
    char                fmt[4];         // FMT header
    unsigned int        Subchunk1Size;  // Size of the fmt chunk
    unsigned short      AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
    unsigned short      NumOfChan;      // Number of channels 1=Mono 2=Sterio
    unsigned int        SamplesPerSec;  // Sampling Frequency in Hz
    unsigned int        bytesPerSec;    // bytes per second
    unsigned short      blockAlign;     // 2=16-bit mono, 4=16-bit stereo
    unsigned short      bitsPerSample;  // Number of bits per sample
    char                Subchunk2ID[4]; // "data"  string
    unsigned int        Subchunk2Size;  // Sampled data length
} __attribute__((packed)) TWavHeader;

class TFileAlreadyExists : public std::exception {

};

class TWavIOError : public std::exception {
    const int ErrNum = 0;
    const char* Text;
public:
    TWavIOError(const char* str, int err)
        : ErrNum(err)
        , Text(str)
    {}
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
    struct TMeta {
        FILE* WavFile;
        const TWavHeader Header;
    } Meta;
    static TWav::TMeta ReadWavHeader(const std::string& filename);
    static TWav::TMeta CreateFileAndHeader(const std::string& filename, const TWavHeader& header, bool overwrite);
public:
    static TWavHeader CreateHeader(int channels, uint32_t length);
    enum Mode {
        E_READ,
        E_WRITE
    };
    TWav(const std::string& filename); // reading
    TWav(const std::string& filename, const TWavHeader& header, bool overwrite = false); //writing
    ~TWav();
    uint32_t GetChannelNum() const;
    uint32_t GetSampleRate() const;
    const TWavHeader& GetWavHeader() const;
    template<class T>
    IPCMReader<T>* GetPCMReader() const;

    template<class T>
    IPCMWriter<T>* GetPCMWriter();
};

typedef std::unique_ptr<TWav> TWavPtr;

template<class T>
IPCMReader<T>* TWav::GetPCMReader() const {
    return new TWavPcmReader<T>([this](std::vector<std::vector<T>>& data, const uint32_t size) {
        if (data[0].size() != Meta.Header.NumOfChan)
            throw TWrongReadBuffer(); 
        uint32_t dataRead = 0;
        for (uint32_t i = 0; i < size; i++) {
            for (uint32_t c = 0; c < Meta.Header.NumOfChan; c++) {
                short buf;
                dataRead += fread(&buf, Meta.Header.bitsPerSample/8, 1, Meta.WavFile);
                data[i][c] = (T)buf;
            }
        }
    });
}

template<class T>
IPCMWriter<T>* TWav::GetPCMWriter() {
    return new TWavPcmWriter<T>([this](const std::vector<std::vector<T>>& data, const uint32_t size) {
        if (data[0].size() != Meta.Header.NumOfChan)
            throw TWrongReadBuffer();
        uint32_t dataWrite = 0;
        for (uint32_t i = 0; i < size; i++) {
            for (uint32_t c = 0; c < Meta.Header.NumOfChan; c++) {
                short buf = (short)data[i][c];
                dataWrite += fwrite(&buf, Meta.Header.bitsPerSample/8, 1, Meta.WavFile);
            }
        }
    });
}


