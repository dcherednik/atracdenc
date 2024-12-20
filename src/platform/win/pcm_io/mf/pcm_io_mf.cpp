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

#include "../../../wav.h"
#include "../../../env.h"

#include "pcm_io_mf.h"
#include "../pcm_io_impl.h"

#include <comdef.h>

#include <initguid.h>
#include <cguid.h>
#include <atlbase.h>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <mferror.h>

#include <propvarutil.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#include <string>

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "Propsys.lib")

class THException : public std::exception {
    static std::string hrToText(HRESULT hr) {
        _com_error err(hr);
        return err.ErrorMessage();
    }
public:
    THException(HRESULT hr, const std::string& msg)
        : std::exception((msg + ", " + hrToText(hr)).c_str())
    {}
};

static std::wstring Utf8ToMultiByte(const std::string& in) {
    std::vector<wchar_t> buf;
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), in.size(), buf.data(), 0);
    if (!len) {
        throw std::exception("unable to convert utf8 to multiByte");
    }
    buf.resize((size_t)len);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in.data(), in.size(), buf.data(), buf.size());
    return std::wstring(buf.data(), buf.size());
}

// TODO: add dither, noise shape?
static inline int16_t FloatToInt16(float in) {
    return std::min((int)INT16_MAX, std::max((int)INT16_MIN, (int)lrint(in * (float)INT16_MAX)));
}

static HRESULT WriteToFile(HANDLE hFile, void* p, DWORD cb) {
    DWORD cbWritten = 0;
    HRESULT hr = S_OK;

    BOOL bResult = WriteFile(hFile, p, cb, &cbWritten, NULL);
    if (!bResult) {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    return hr;
}

static HRESULT ConfigureAudioStream(IMFSourceReader *pReader, IMFMediaType **ppPCMAudio) {
    CComPtr<IMFMediaType> pUncompressedAudioType;
    CComPtr<IMFMediaType> pPartialType;

    // Select the first audio stream, and deselect all other streams.
    HRESULT hr = pReader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);

    if (SUCCEEDED(hr)) {
        hr = pReader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    }

    // Create a partial media type that specifies uncompressed PCM audio.
    hr = MFCreateMediaType(&pPartialType);

    if (SUCCEEDED(hr)) {
        hr = pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    }

    if (SUCCEEDED(hr)) {
        hr = pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    }

    // Set this type on the source reader. The source reader will
    // load the necessary decoder.
    if (SUCCEEDED(hr)) {
        hr = pReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            NULL, pPartialType);
    }

    // Get the complete uncompressed format.
    if (SUCCEEDED(hr)) {
        hr = pReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            &pUncompressedAudioType);
    }

    // Ensure the stream is selected.
    if (SUCCEEDED(hr)) {
        hr = pReader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            TRUE);
    }

    // Return the PCM format to the caller.
    if (SUCCEEDED(hr)) {
        *ppPCMAudio = pUncompressedAudioType;
        (*ppPCMAudio)->AddRef();
    }

    return hr;
}

static HRESULT CreatePCMAudioType(UINT32 sampleRate, UINT32 bitsPerSample, UINT32 cChannels, IMFMediaType **ppType) {
    HRESULT hr = S_OK;

    CComPtr<IMFMediaType> pType;

    // Calculate derived values.
    UINT32 blockAlign = cChannels * (bitsPerSample / 8);
    UINT32 bytesPerSecond = blockAlign * sampleRate;

    // Create the empty media type.
    hr = MFCreateMediaType(&pType);

    // Set attributes on the type.
    if (SUCCEEDED(hr)) {
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, cChannels);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecond);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitsPerSample);
    }

    if (SUCCEEDED(hr)) {
        hr = pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    }

    if (SUCCEEDED(hr)) {
        // Return the type to the caller.
        *ppType = pType;
        (*ppType)->AddRef();
    }

	return hr;
}

class TPCMIOMediaFoundationFile : public IPCMProviderImpl {
public:
    TPCMIOMediaFoundationFile(const std::string& path) {

        HRESULT hr = S_OK;

        hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		
        if (!SUCCEEDED(hr)) {
            throw THException(hr, "unable to initialize COM library");
        }

        hr = MFStartup(MF_VERSION);

        if (!SUCCEEDED(hr)) {
            throw THException(hr, "unable to initialize Media Foundation platform");
        }

        std::wstring wpath = Utf8ToMultiByte(path);

        hr = MFCreateSourceReaderFromURL(wpath.c_str(), NULL, &Reader_);

        if (FAILED(hr)) {
            throw THException(hr, "qqq unable to open input file");
        }

        hr = ConfigureAudioStream(Reader_, &MediaType_);

        if (FAILED(hr)) {
            throw THException(hr, "unable to get media type");
        }

        if (FAILED(MediaType_->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ChannelsNum_))) {
            throw THException(hr, "unable to get channels number");
        }

        if (FAILED(MediaType_->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &SampleRate_))) {
            throw THException(hr, "unable to get sample rate");
        }

        if (FAILED(MediaType_->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &BytesPerSample_))) {
            throw THException(hr, "unable to get sample size");
        }

        if (!(ChannelsNum_ == 1 && BytesPerSample_ == 2 || ChannelsNum_ == 2 && BytesPerSample_ == 4)) {
            throw THException(hr, "unsupported samaple format");
        }

        NEnv::SetRoundFloat();
	}

    TPCMIOMediaFoundationFile(const std::string& path, int channels, int sampleRate) {

        HRESULT hr = S_OK;
        ChannelsNum_ = channels;
        SampleRate_ = sampleRate;

        hr = CreatePCMAudioType(sampleRate, 16, channels, &MediaType_);

        if (FAILED(hr)) {
            throw THException(hr, "unable to create PCM audio type");
        }

        UINT32 format = 0;

        WAVEFORMATEX *wav = NULL;

        hr = MFCreateWaveFormatExFromMFMediaType(MediaType_, &wav, &format);

        if (FAILED(hr)) {
            throw THException(hr, "unable to create wave format");
        }

        DWORD header[] = {
            // RIFF header
            FCC('RIFF'),
            0,
            FCC('WAVE'),
            // Start of 'fmt ' chunk
            FCC('fmt '),
            format
        };

        DWORD dataHeader[] = { FCC('data'), 0 };

        std::wstring wpath = Utf8ToMultiByte(path);

        OutFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, 0, NULL);

        if (OutFile == INVALID_HANDLE_VALUE) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            throw THException(hr, "qqq2 unable to open output file");
        }

        hr = WriteToFile(OutFile, header, sizeof(header));

        // Write the WAVEFORMATEX structure.
        if (SUCCEEDED(hr)) {
            hr = WriteToFile(OutFile, wav, format);
        }

        // Write the start of the 'data' chunk

        if (SUCCEEDED(hr)) {
            hr = WriteToFile(OutFile, dataHeader, sizeof(dataHeader));
        }

        if (FAILED(hr)) {
            throw THException(hr, "unable to write headers");
        }

        NEnv::SetRoundFloat();
    }

    ~TPCMIOMediaFoundationFile() {
        if (OutFile) {
            if (!CloseHandle(OutFile)) {
                std::cerr << "unable to close handle" << std::endl;
            }
        }
    }

public:
    size_t GetChannelsNum() const override {
        return ChannelsNum_;
    }

    size_t GetSampleRate() const override {
        return SampleRate_;
    }

    size_t GetTotalSamples() const override {
        PROPVARIANT var;
        LONGLONG duration; // duration in 100 nanosecond units
        HRESULT hr = Reader_->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
        if (SUCCEEDED(hr)) {
            hr = PropVariantToInt64(var, &duration);
            PropVariantClear(&var);
        }
        if (!SUCCEEDED(hr)) {
            throw THException(hr, "unable to extract duration from source");
        }

        const UINT32 bytesPerSecond = MFGetAttributeUINT32(MediaType_, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0);
        const UINT32 blockSize = MFGetAttributeUINT32(MediaType_, MF_MT_AUDIO_BLOCK_ALIGNMENT, 0);

        if (!blockSize) {
            throw THException(hr, "got zero block size");
        }

        const LONGLONG samplesPerSecond = bytesPerSecond / blockSize;
        const LONGLONG totalSamples = samplesPerSecond * duration / 10000000;

        return static_cast<size_t>(totalSamples);
    }

    size_t Read(TPCMBuffer& buf, size_t sz) override {
        HRESULT hr = S_OK;

        const size_t sizeBytes = sz * BytesPerSample_;

        size_t curPos = 0; // position in outpuf buffer (TPCMBuffer)

        if (!Buf_.empty()) {
            if (sizeBytes > (Buf_.size() - ConsummerPos_)) {
                if (Buf_.size() & 0x3) {
                    std::cerr << "buffer misconfiguration" << std::endl;
                    abort();
                }
                curPos = (Buf_.size() - ConsummerPos_) / BytesPerSample_;
                ConvertToPcmBufferFromLE(Buf_.data() + ConsummerPos_, buf, curPos, 0, ChannelsNum_);
                ConsummerPos_ = 0;
            } else {
                // We have all data in our buffer, just convert it and shift consumer position
                ConvertToPcmBufferFromLE(Buf_.data() + ConsummerPos_, buf, sz, 0, ChannelsNum_);
                ConsummerPos_ += sizeBytes;
                return sz;
            }
        }

        bool cont = true;
        while (cont) {
            DWORD flags = 0;
            DWORD readyToRead = 0;
            CComPtr<IMFSample> sample;

            hr = Reader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                0, NULL, &flags, NULL, &sample);

            if (FAILED(hr)) {
                break;
            }

            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
				std::cerr << "Type change - not supported by WAVE file format.\n" << std::endl;
                break;
            }

            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                std::cerr << "End of input file.\n" << std::endl;
                break;
            }

            if (sample == NULL) {
                continue;
            }
            LONGLONG duration;
            sample->GetSampleDuration(&duration);

            CComPtr<IMFMediaBuffer> buffer;
            BYTE *audioData = NULL;

            hr = sample->ConvertToContiguousBuffer(&buffer);
			
            if (FAILED(hr)) {
                break;
            }

            hr = buffer->Lock(&audioData, NULL, &readyToRead);

            if (FAILED(hr)) {
                break;
            }

            if (sizeBytes > (readyToRead + (curPos * BytesPerSample_))) {
                const size_t ready = readyToRead / BytesPerSample_;
                ConvertToPcmBufferFromLE(audioData, buf, ready, curPos, ChannelsNum_);
                curPos += ready;
            } else {
                ConvertToPcmBufferFromLE(audioData, buf, sz - curPos, curPos, ChannelsNum_);
                size_t leftBytes = readyToRead - (sz - curPos) * BytesPerSample_;
                Buf_.resize(leftBytes);
                std::memcpy(Buf_.data(), audioData + (sz - curPos) * BytesPerSample_, leftBytes);
                // out buffer writen completely
                curPos = sz;
                cont = false;
            }

            hr = buffer->Unlock();
            audioData = NULL;

            if (FAILED(hr)) {
                break;
            }
        }

        if (FAILED(hr)) {
            throw THException(hr, "unable to read from PCM stream");
        }

        return curPos;
    }

    size_t Write(const TPCMBuffer& buf, size_t sz) override {
        const size_t samples = ChannelsNum_ * sz;
        Buf_.resize(samples * 2);
        for (size_t i = 0; i < samples; i++) {
            *(int16_t*)(Buf_.data() + i * 2) = FloatToInt16(*(buf[0] + i));
        }
        if (FAILED(WriteToFile(OutFile, Buf_.data(), Buf_.size()))) {
            throw std::exception("unable to write PCM buffer to file");
        }
        return sz;
    }

private:
    CComPtr<IMFSourceReader> Reader_;
    CComPtr<IMFMediaType> MediaType_;

    uint32_t ChannelsNum_;
    uint32_t SampleRate_;
    uint32_t BytesPerSample_;

    // Temporial buffer and consumer position
    std::vector<BYTE> Buf_;
    size_t ConsummerPos_ = 0;

    HANDLE OutFile = NULL;
};

IPCMProviderImpl* CreatePCMIOMFReadImpl(const std::string& path) {
    return new TPCMIOMediaFoundationFile(path);
}

IPCMProviderImpl* CreatePCMIOMFWriteImpl(const std::string& path, int channels, int sampleRate) {
    return new TPCMIOMediaFoundationFile(path, channels, sampleRate);
}
