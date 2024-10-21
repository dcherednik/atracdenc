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

#include <memory>
#include <string>

#include "config.h"

#include "pcmengin.h"

class TFileAlreadyExists : public std::exception {
};

class TNoDataToRead : public std::exception {
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

class IPCMProviderImpl {
public:
    virtual ~IPCMProviderImpl() = default;
    virtual size_t GetChannelsNum() const = 0;
    virtual size_t GetSampleRate() const = 0;
    virtual size_t GetTotalSamples() const = 0;
    virtual size_t Read(TPCMBuffer<TFloat>& buf, size_t sz) = 0;
    virtual size_t Write(const TPCMBuffer<TFloat>& buf, size_t sz) = 0;
};

//TODO: split for reader/writer
class TWav {
    mutable std::unique_ptr<IPCMProviderImpl> Impl;
public:
    enum Mode {
        E_READ,
        E_WRITE
    };
    TWav(const std::string& filename); // reading
    TWav(const std::string& filename, size_t channels, size_t sampleRate); //writing
    ~TWav();
    size_t GetChannelNum() const;
    size_t GetSampleRate() const;
    uint64_t GetTotalSamples() const;

    template<class T>
    IPCMReader<T>* GetPCMReader() const;

    template<class T>
    IPCMWriter<T>* GetPCMWriter();
};

typedef std::unique_ptr<TWav> TWavPtr;

template<class T>
IPCMReader<T>* TWav::GetPCMReader() const {
    return new TWavPcmReader<T>([this](TPCMBuffer<T>& data, const uint32_t size) {
        if (data.Channels() != Impl->GetChannelsNum())
            throw TWrongReadBuffer(); 

        size_t read;
        if ((read = Impl->Read(data, size)) != size) {
            if (!read)
                throw TNoDataToRead();

            data.Zero(read, size - read);
        }
    });
}

template<class T>
IPCMWriter<T>* TWav::GetPCMWriter() {
    return new TWavPcmWriter<T>([this](const TPCMBuffer<T>& data, const uint32_t size) {
        if (data.Channels() != Impl->GetChannelsNum())
            throw TWrongReadBuffer();
        if (Impl->Write(data, size) != size) {
            fprintf(stderr, "can't write block\n");
        }
    });
}


