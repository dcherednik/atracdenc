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
#include <cstdint>

#include <assert.h>
#include <string.h>

class TNoDataToRead : public std::exception {
};

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

class TPCMBuffer {
    std::vector<float> Buf_;
    size_t NumChannels;

public:
    TPCMBuffer(uint16_t bufSize, size_t numChannels)
       : NumChannels(numChannels)
    {
        Buf_.resize((size_t)bufSize * numChannels);
    }

    size_t Size() {
        return Buf_.size() / NumChannels;
    }

    float* operator[](size_t pos) {
        size_t rpos = pos * NumChannels;
        if (rpos >= Buf_.size()) {
            std::cerr << "attempt to access out of buffer pos: " << pos << std::endl;
            std::abort();
        }
        return &Buf_[rpos];
    }

    const float* operator[](size_t pos) const {
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

class IPCMWriter {
    public:
        virtual void Write(const TPCMBuffer& data , const uint32_t size) const = 0;
        IPCMWriter() {};
        virtual ~IPCMWriter() {};
};

class IPCMReader {
    public:
        virtual bool Read(TPCMBuffer& data , const uint32_t size) const = 0;
        IPCMReader() {};
        virtual ~IPCMReader() {};
};

class TPCMEngine {
public:
    typedef std::unique_ptr<IPCMWriter> TWriterPtr;
    typedef std::unique_ptr<IPCMReader> TReaderPtr;
    struct ProcessMeta {
        const uint16_t Channels;
    };
private:
    TPCMBuffer Buffer;
    TWriterPtr Writer;
    TReaderPtr Reader;
    uint64_t Processed = 0;
    uint64_t ToDrain = 0;
public:
        TPCMEngine(uint16_t bufSize, size_t numChannels)
           : Buffer(bufSize, numChannels) {
        }

        TPCMEngine(uint16_t bufSize, size_t numChannels, TWriterPtr&& writer)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer)) {
        }

        TPCMEngine(uint16_t bufSize, size_t numChannels, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Reader(std::move(reader)) {
        }

        TPCMEngine(uint16_t bufSize, size_t numChannels, TWriterPtr&& writer, TReaderPtr&& reader)
            : Buffer(bufSize, numChannels)
            , Writer(std::move(writer))
            , Reader(std::move(reader)) {
        }

        enum class EProcessResult {
            LOOK_AHEAD, // need more data to process
            PROCESSED,
        };

        typedef std::function<EProcessResult(float* data, const ProcessMeta& meta)> TProcessLambda;

        uint64_t ApplyProcess(size_t step, TProcessLambda lambda) {
            if (step > Buffer.Size()) {
                throw TPCMBufferTooSmall();
            }

            bool drain = false;
            if (Reader) {
                const uint32_t sizeToRead = Buffer.Size();
                const bool ok = Reader->Read(Buffer, sizeToRead);
                if (!ok) {
                    if (ToDrain) {
                        drain = true;
                    } else {
                        throw TNoDataToRead();
                    }
                }
            }

            size_t lastPos = 0;
            ProcessMeta meta = {Buffer.Channels()};

            for (size_t i = 0; i + step <= Buffer.Size(); i+=step) {
                auto res = lambda(Buffer[i], meta);
                if (res == EProcessResult::PROCESSED) {
                    lastPos += step;
                    if (drain && ToDrain--) {
                        break;
                    }
                } else {
                    assert(!drain);
                    ToDrain++;
                }
            }

            if (Writer) {
                Writer->Write(Buffer, lastPos);
            }

            Processed += lastPos;
            return Processed;
        }
};

class IProcessor {
public:
    virtual typename TPCMEngine::TProcessLambda GetLambda() = 0;
    virtual ~IProcessor() {}
};
