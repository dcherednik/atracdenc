/*
 * High-quality polyphase resampler for PCM audio.
 * Uses a windowed-sinc (Blackman) lowpass filter for clean conversion.
 *
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "resampler.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --------------------------------------------------------------------------
// Windowed-sinc resampler (polyphase)
// --------------------------------------------------------------------------

class CResamplerReader : public IPCMReader {
public:
    CResamplerReader(std::unique_ptr<IPCMReader> src,
                     uint32_t srcRate, uint32_t dstRate,
                     uint16_t channels)
        : Src(std::move(src))
        , Channels(channels)
        , SrcRate(srcRate)
        , DstRate(dstRate)
        , Ratio((double)srcRate / (double)dstRate)
        , PhaseCount(0)
        , FilterLen(0)
        , SrcBufPos(0)
        , SrcBufUsed(0)
        , FracPos(0.0)
        , SrcExhausted(false)
    {
        if (!Src)
            throw std::invalid_argument("resampler: null source reader");
        if (srcRate == 0 || dstRate == 0)
            throw std::invalid_argument("resampler: sample rate must be > 0");
        if (srcRate == dstRate)
            throw std::invalid_argument("resampler: rates are equal, no resampling needed");

        // Choose filter parameters based on the conversion ratio
        // More phases = better quality, but more memory
        PhaseCount = 256;
        // Filter half-length (in input samples); longer = sharper cutoff
        HalfLen = 32;
        FilterLen = 2 * HalfLen;

        BuildFilter();

        // Internal source buffer: must hold enough input for a batch of output samples
        // We read in chunks to avoid excessive per-sample reads
        SrcBuf.resize(SrcBufSize * Channels, 0.0f);
    }

    bool Read(TPCMBuffer& outBuf, const uint32_t outFrames) const override {
        const uint16_t ch = Channels;
        uint32_t written = 0;

        while (written < outFrames) {
            // Refill source buffer if running low and source not exhausted
            if (!SrcExhausted && SrcBufPos + (size_t)FilterLen > SrcBufUsed) {
                // Shift remaining samples to front
                size_t remain = (SrcBufPos < SrcBufUsed) ? (SrcBufUsed - SrcBufPos) : 0;
                if (remain > 0 && SrcBufPos > 0) {
                    memmove(SrcBuf.data(),
                            SrcBuf.data() + SrcBufPos * ch,
                            remain * ch * sizeof(float));
                }
                SrcBufUsed = remain;
                SrcBufPos = 0;

                // Read more data from source
                uint32_t toRead = (uint32_t)(SrcBufSize - SrcBufUsed);
                if (toRead > 0) {
                    TPCMBuffer tmpBuf((uint16_t)toRead, ch);
                    bool ok = Src->Read(tmpBuf, toRead);
                    if (!ok) {
                        SrcExhausted = true;
                        // Zero-pad the remaining source buffer so the filter can still run
                        if (SrcBufUsed < (size_t)FilterLen) {
                            memset(SrcBuf.data() + SrcBufUsed * ch, 0,
                                   (FilterLen - SrcBufUsed) * ch * sizeof(float));
                            SrcBufUsed = FilterLen;
                        }
                    } else {
                        for (uint32_t i = 0; i < toRead; i++) {
                            for (uint16_t c = 0; c < ch; c++) {
                                SrcBuf[(SrcBufUsed + i) * ch + c] = tmpBuf[i][c];
                            }
                        }
                        SrcBufUsed += toRead;
                    }
                }
            }

            // Check if we have enough data to produce an output sample
            if (SrcBufPos + (size_t)FilterLen > SrcBufUsed) {
                // Not enough source data even after refill attempt
                if (SrcExhausted) {
                    // Pad output with zeros to let PQF drain
                    for (uint32_t i = written; i < outFrames; i++) {
                        for (uint16_t c = 0; c < ch; c++) {
                            outBuf[i][c] = 0.0f;
                        }
                    }
                    return true;
                }
                break;
            }

            // Resample one output sample using polyphase sinc interpolation
            size_t intPos = SrcBufPos;
            double frac = FracPos;

            int phase = (int)(frac * PhaseCount + 0.5);
            if (phase >= PhaseCount) phase = PhaseCount - 1;

            for (uint16_t c = 0; c < ch; c++) {
                double sum = 0.0;
                for (int k = 0; k < FilterLen; k++) {
                    size_t srcIdx = intPos + k;
                    if (srcIdx < SrcBufUsed) {
                        sum += SrcBuf[srcIdx * ch + c] * FilterBank[phase * FilterLen + k];
                    }
                }
                outBuf[written][c] = (float)sum;
            }
            written++;

            // Advance source position
            FracPos += Ratio;
            size_t advance = (size_t)FracPos;
            FracPos -= advance;
            SrcBufPos += advance;
        }

        return written > 0;
    }

private:
    void BuildFilter() {
        FilterBank.resize((size_t)PhaseCount * FilterLen);

        for (int p = 0; p < PhaseCount; p++) {
            double phaseOffset = (double)p / PhaseCount;
            double sum = 0.0;

            for (int k = 0; k < FilterLen; k++) {
                // Position relative to the interpolation point
                double x = (k - HalfLen + 1) + phaseOffset;

                // Windowed sinc
                double w;
                double sincVal;

                if (fabs(x) < 1e-10) {
                    sincVal = 1.0;
                } else {
                    sincVal = sin(M_PI * x) / (M_PI * x);
                }

                // Blackman window
                double n = (double)k / (FilterLen - 1);
                w = 0.42 - 0.5 * cos(2.0 * M_PI * n) + 0.08 * cos(4.0 * M_PI * n);

                double val = sincVal * w;
                FilterBank[p * FilterLen + k] = (float)val;
                sum += val;
            }

            // Normalize so the filter has unity gain
            if (fabs(sum) > 1e-10) {
                float scale = (float)(1.0 / sum);
                for (int k = 0; k < FilterLen; k++) {
                    FilterBank[p * FilterLen + k] *= scale;
                }
            }
        }
    }

    std::unique_ptr<IPCMReader> Src;
    uint16_t Channels;
    uint32_t SrcRate;
    uint32_t DstRate;
    double Ratio;

    int PhaseCount;
    int HalfLen;
    int FilterLen;
    std::vector<float> FilterBank;

    // Internal interleaved source buffer
    static const size_t SrcBufSize = 4096;
    mutable std::vector<float> SrcBuf;
    mutable size_t SrcBufPos;
    mutable size_t SrcBufUsed;
    mutable double FracPos;
    mutable bool SrcExhausted;
};

// --------------------------------------------------------------------------
// Pass-through reader (same rate, no resampling needed)
// --------------------------------------------------------------------------

class CPassThroughReader : public IPCMReader {
public:
    CPassThroughReader(std::unique_ptr<IPCMReader> src)
        : Src(std::move(src)) {}

    bool Read(TPCMBuffer& buf, const uint32_t size) const override {
        return Src->Read(buf, size);
    }

private:
    std::unique_ptr<IPCMReader> Src;
};

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

std::unique_ptr<IPCMReader> CreateResamplingReader(
    std::unique_ptr<IPCMReader> input,
    uint32_t srcRate,
    uint32_t dstRate,
    uint16_t channels)
{
    if (srcRate == dstRate) {
        return std::make_unique<CPassThroughReader>(std::move(input));
    }
    return std::make_unique<CResamplerReader>(std::move(input), srcRate, dstRate, channels);
}

uint64_t ResampledLength(uint64_t srcSamples, uint32_t srcRate, uint32_t dstRate) {
    if (srcRate == dstRate) return srcSamples;
    return (uint64_t)((double)srcSamples * dstRate / srcRate + 0.5);
}
