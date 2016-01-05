#pragma once

#include <vector>
#include <memory>
#include <exception>
#include <functional>

#include <assert.h>

class TPCMBufferTooSmall : public std::exception {
    virtual const char* what() const throw() {
        return "PCM buffer too small";
    }
};

class TWrongReadBuffer : public std::exception {
    virtual const char* what() const throw() {
        return "PCM buffer too small";
    }
};


class TEndOfRead : public std::exception {
    virtual const char* what() const throw() {
        return "End of reader";
    }
};

template <class T>
class IPCMWriter {
    public:
        virtual void Write(const std::vector<std::vector<T>>& data , const uint32_t size) const = 0;
        IPCMWriter() {};
        virtual ~IPCMWriter() {};
};

template <class T>
class IPCMReader {
    public:
        virtual void Read(std::vector<std::vector<T>>& data , const uint32_t size) const = 0;
        IPCMReader() {};
        virtual ~IPCMReader() {};
};



template<class T>
class TPCMEngine {
    std::vector<std::vector<T>> Buffers;
public:
    typedef std::unique_ptr<IPCMWriter<T>> TWriterPtr;
    typedef std::unique_ptr<IPCMReader<T>> TReaderPtr;
private:
    TWriterPtr Writer;
    TReaderPtr Reader;
    uint64_t Processed = 0;
public:
        TPCMEngine(const int32_t bufSize, const int32_t numChannels) {
            InitBuffers(bufSize, numChannels);
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TWriterPtr&& writer)
            : Writer(std::move(writer)) {
            InitBuffers(bufSize, numChannels);
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TReaderPtr&& reader)
            : Reader(std::move(reader)) {
            InitBuffers(bufSize, numChannels);
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TWriterPtr&& writer, TReaderPtr&& reader)
            : Writer(std::move(writer))
            , Reader(std::move(reader)) {
            InitBuffers(bufSize, numChannels);
        }
        typedef std::function<void(std::vector<T>* data)> TProcessLambda; 

        uint64_t ApplyProcess(int step, TProcessLambda lambda) {
            if (step > Buffers.size()) {
                throw TPCMBufferTooSmall();
            }
            if (Reader) {
                const uint32_t sizeToRead = Buffers.size();
                Reader->Read(Buffers, sizeToRead);
            }
            int32_t lastPos = 0;
            for (int i = 0; i + step <= Buffers.size(); i+=step) {
                lambda(&Buffers[i]);
                lastPos = i + step;
            }
            assert(lastPos == Buffers.size());
            if (Writer) {
                Writer->Write(Buffers, lastPos);
            }
            Processed += lastPos;
            return Processed;

        }


private:
    void InitBuffers(const int32_t bufSize, const int32_t numChannels) {
        Buffers.resize(bufSize);
        for (std::vector<T>& channel : Buffers) {
            channel.resize(numChannels);
        }
    }
};

template<class T>
class IProcessor {
public:
    virtual typename TPCMEngine<T>::TProcessLambda GetDecodeLambda() = 0;
    virtual typename TPCMEngine<T>::TProcessLambda GetEncodeLambda() = 0;
    virtual ~IProcessor() {}
};
