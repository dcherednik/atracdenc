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

#pragma once

#include "pcmengin.h"
#include <cstdint>
#include <memory>
#include <cstddef>

/**
 * Resample a TPCMBuffer stream from srcRate to dstRate using windowed-sinc
 * (Blackman) polyphase interpolation.
 *
 * @param input       Reader providing samples at srcRate
 * @param srcRate     Source sample rate in Hz
 * @param dstRate     Target sample rate in Hz (default 44100)
 * @param channels    Number of audio channels
 * @return            A new IPCMReader that delivers resampled audio at dstRate
 */
std::unique_ptr<IPCMReader> CreateResamplingReader(
    std::unique_ptr<IPCMReader> input,
    uint32_t srcRate,
    uint32_t dstRate,
    uint16_t channels);

/**
 * Compute the total number of output samples after resampling.
 */
uint64_t ResampledLength(uint64_t srcSamples, uint32_t srcRate, uint32_t dstRate);
