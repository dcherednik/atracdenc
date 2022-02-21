/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <vector>
#include <memory>
#include <exception>
#include <functional>
#include <cstdlib>
#include <iostream>

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
    uint16_t NumChannels;

public:
    TPCMBuffer(uint16_t bufSize, uint8_t numChannels)
       : NumChannels(numChannels)
    {
        Buf_.resize((size_t)bufSize * numChannels);
    }

    size_t Size() {
        return Buf_.size() / NumChannels;
    }

    T* operator[](size_t pos) {
        size_t rpos = pos * NumChannels;
        if (rpos >= Buf_.size()) {
            std::cerr << "attempt to access out of buffer pos: " << pos << std::endl;
            std::abort();
        }
        return &Buf_[rpos];
    }

    const T* operator[](size_t pos) const {
        size_t rpos = pos * NumChannels;
        if (rpos >= Buf_.size()) {
            std::cerr << "attempt to access out of buffer pos: " << pos << std::endl;
            std::abort();
        }
        return &Buf_[rpos];
    }

    uint16_t Channels() const {
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
    struct ProcessMeta {
        const uint16_t Channels;
    };
private:
    TPCMBuffer<T> Buffer;
    TWriterPtr Writer;
    TReaderPtr Reader;
    uint64_t Processed = 0;
public:
        TPCMEngine(uint16_t bufSize, uint8_t numChannels)
           : Buffer(bufSize, numChannels) {
        }

        TPCMEngine(uint16_t bufSize, uint8_t numChannels, TWriterPtr&& writer)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer)) {
        }

        TPCMEngine(uint16_t bufSize, uint8_t numChannels, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Reader(std::move(reader)) {
        }

        TPCMEngine(uint16_t bufSize, uint8_t numChannels, TWriterPtr&& writer, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer))
            , Reader(std::move(reader)) {
        }

        typedef std::function<void(T* data, const ProcessMeta& meta)> TProcessLambda; 

        uint64_t ApplyProcess(size_t step, TProcessLambda lambda) {
            if (step > Buffer.Size()) {
                throw TPCMBufferTooSmall();
            }

            if (Reader) {
                const uint32_t sizeToRead = Buffer.Size();
                Reader->Read(Buffer, sizeToRead);
            }

            size_t lastPos = 0;
            ProcessMeta meta = {Buffer.Channels()};
            for (size_t i = 0; i + step <= Buffer.Size(); i+=step) {
                lambda(Buffer[i], meta);
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
    virtual typename TPCMEngine<T>::TProcessLambda GetLambda() = 0;
    virtual ~IProcessor() {}
};
