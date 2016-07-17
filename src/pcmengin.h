#pragma once

#include <vector>
#include <memory>
#include <exception>
#include <functional>

#include <assert.h>
#include <string.h>

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
class TPCMBuffer {
    std::vector<T> Buf_;
    int32_t NumChannels;
public:
    TPCMBuffer(const int32_t bufSize, const int32_t numChannels)
       : NumChannels(numChannels)
    {
        Buf_.resize(bufSize*numChannels);
    }
    size_t Size() {
        return Buf_.size() / NumChannels;
    }
    T* operator[](size_t pos) {
        size_t rpos = pos * NumChannels;
        if (rpos >= Buf_.size())
            abort();
        return &Buf_[rpos];
    }
    const T* operator[](size_t pos) const {
        size_t rpos = pos * NumChannels;
        if (rpos >= Buf_.size())
            abort();
        return &Buf_[rpos];
    }
    size_t Channels() const {
        return NumChannels;
    }
    void Zero(size_t pos, size_t len) {
        assert((pos + len) * NumChannels <= Buf_.size());
        memset(&Buf_[pos*NumChannels], 0, len*NumChannels);
    }
};

template <class T>
class IPCMWriter {
    public:
        virtual void Write(const TPCMBuffer<T>& data , const uint32_t size) const = 0;
        IPCMWriter() {};
        virtual ~IPCMWriter() {};
};

template <class T>
class IPCMReader {
    public:
        virtual void Read(TPCMBuffer<T>& data , const uint32_t size) const = 0;
        IPCMReader() {};
        virtual ~IPCMReader() {};
};

template<class T>
class TPCMEngine {
public:
    typedef std::unique_ptr<IPCMWriter<T>> TWriterPtr;
    typedef std::unique_ptr<IPCMReader<T>> TReaderPtr;
private:
    TPCMBuffer<T> Buffer;
    TWriterPtr Writer;
    TReaderPtr Reader;
    uint64_t Processed = 0;
public:
        TPCMEngine(const int32_t bufSize, const int32_t numChannels)
           : Buffer(bufSize, numChannels) {
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TWriterPtr&& writer)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer)) {
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Reader(std::move(reader)) {
        }
        TPCMEngine(const int32_t bufSize, const int32_t numChannels, TWriterPtr&& writer, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer))
            , Reader(std::move(reader)) {
        }
        typedef std::function<void(T* data)> TProcessLambda; 

        uint64_t ApplyProcess(int step, TProcessLambda lambda) {
            if (step > Buffer.Size()) {
                throw TPCMBufferTooSmall();
            }
            if (Reader) {
                const uint32_t sizeToRead = Buffer.Size();
                Reader->Read(Buffer, sizeToRead);
            }
            int32_t lastPos = 0;
            for (int i = 0; i + step <= Buffer.Size(); i+=step) {
                lambda(Buffer[i]);
                lastPos = i + step;
            }
            assert(lastPos == Buffer.Size());
            if (Writer) {
                Writer->Write(Buffer, lastPos);
            }
            Processed += lastPos;
            return Processed;

        }
};

template<class T>
class IProcessor {
public:
    virtual typename TPCMEngine<T>::TProcessLambda GetDecodeLambda() = 0;
    virtual typename TPCMEngine<T>::TProcessLambda GetEncodeLambda() = 0;
    virtual ~IProcessor() {}
};
