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

#include <vector>

#include "atrac1denc.h"
#include "bitstream/bitstream.h"
#include "atrac/atrac1.h"
#include "atrac/atrac1_dequantiser.h"
#include "atrac/atrac1_qmf.h"
#include "atrac/atrac1_bitalloc.h"
#include "atrac/atrac_psy_common.h"
#include "util.h"

namespace NAtracDEnc {
using namespace NBitStream;
using namespace NAtrac1;
using namespace NMDCT;
using std::vector;

TAtrac1Encoder::TAtrac1Encoder(TCompressedOutputPtr&& aea, TAtrac1EncodeSettings&& settings)
    : Aea(std::move(aea))
    , Settings(std::move(settings))
    , LoudnessCurve(CreateLoudnessCurve(TAtrac1Data::NumSamples))
{
}

TAtrac1Decoder::TAtrac1Decoder(TCompressedInputPtr&& aea)
    : Aea(std::move(aea))
{
}

static void vector_fmul_window(float *dst, const float *src0,
                                const float *src1, const float *win, int len)
{
    int i, j;

    dst  += len;
    win  += len;
    src0 += len;

    for (i = -len, j = len - 1; i < 0; i++, j--) {
        float s0 = src0[i];
        float s1 = src1[j];
        float wi = win[i];
        float wj = win[j];
        dst[i] = s0 * wj - s1 * wi;
        dst[j] = s0 * wi + s1 * wj;
    }
}

void TAtrac1MDCT::Mdct(float Specs[512], float* low, float* mid, float* hi, const TAtrac1Data::TBlockSizeMod& blockSize) {
    uint32_t pos = 0;
    for (uint32_t band = 0; band < TAtrac1Data::NumQMF; band++) {
        const uint32_t numMdctBlocks = 1 << blockSize.LogCount[band];
        float* srcBuf = (band == 0) ? low : (band == 1) ? mid : hi;
        uint32_t bufSz = (band == 2) ? 256 : 128;
        const uint32_t blockSz = (numMdctBlocks == 1) ? bufSz : 32;
        uint32_t winStart = (numMdctBlocks == 1) ? ((band == 2) ? 112 : 48) : 0;
        //compensate level for 3rd band in case of short window
        const float multiple = (numMdctBlocks != 1 && band == 2) ? 2.0 : 1.0;
        vector<float> tmp(512);
        uint32_t blockPos = 0;

        for (size_t k = 0; k < numMdctBlocks; ++k) {
            memcpy(&tmp[winStart], &srcBuf[bufSz], 32 * sizeof(float));
            for (size_t i = 0; i < 32; i++) {
                srcBuf[bufSz + i] = TAtrac1Data::SineWindow[i] * srcBuf[blockPos + blockSz - 32 + i];
                srcBuf[blockPos + blockSz - 32 + i] = TAtrac1Data::SineWindow[31 - i] * srcBuf[blockPos + blockSz - 32 + i];
            }
            memcpy(&tmp[winStart+32], &srcBuf[blockPos], blockSz * sizeof(float));
            const vector<float>&  sp = (numMdctBlocks == 1) ? ((band == 2) ? Mdct512(&tmp[0]) : Mdct256(&tmp[0])) : Mdct64(&tmp[0]);
            for (size_t i = 0; i < sp.size(); i++) {
                Specs[blockPos + pos + i] = sp[i] * multiple;
            }
            if (band) {
                SwapArray(&Specs[blockPos + pos], sp.size());
            }

            blockPos += 32;
        }
        pos += bufSz;
    }
}
void TAtrac1MDCT::IMdct(float Specs[512], const TAtrac1Data::TBlockSizeMod& mode, float* low, float* mid, float* hi) {
    uint32_t pos = 0;
    for (size_t band = 0; band < TAtrac1Data::NumQMF; band++) {
        const uint32_t numMdctBlocks = 1 << mode.LogCount[band];
        const uint32_t bufSz = (band == 2) ? 256 : 128;
        const uint32_t blockSz = (numMdctBlocks == 1) ? bufSz : 32;
        uint32_t start = 0;

        float* dstBuf = (band == 0) ? low : (band == 1) ? mid : hi;

        vector<float> invBuf(512);
        float* prevBuf = &dstBuf[bufSz * 2  - 16];
        for (uint32_t block = 0; block < numMdctBlocks; block++) {
            if (band) {
                SwapArray(&Specs[pos], blockSz);
            }
            vector<float> inv = (numMdctBlocks != 1) ? Midct64(&Specs[pos]) : (bufSz == 128) ? Midct256(&Specs[pos]) : Midct512(&Specs[pos]);
            for (size_t i = 0; i < (inv.size()/2); i++) {
                invBuf[start+i] = inv[i + inv.size()/4];
            }

            vector_fmul_window(dstBuf + start, prevBuf, &invBuf[start], &TAtrac1Data::SineWindow[0], 16);

            prevBuf = &invBuf[start+16];
            start += blockSz;
            pos += blockSz;
        }
        if (numMdctBlocks == 1)
            memcpy(dstBuf + 32, &invBuf[16], ((band == 2) ? 240 : 112) * sizeof(float));

        for (size_t j = 0; j < 16; j++) {
            dstBuf[bufSz*2 - 16  + j] = invBuf[bufSz - 16 + j];
        }
    }
}

TPCMEngine::TProcessLambda TAtrac1Decoder::GetLambda() {
    return [this](float* data, const TPCMEngine::ProcessMeta& /*meta*/) {
        float sum[512];
        const uint32_t srcChannels = Aea->GetChannelNum();
        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            std::unique_ptr<ICompressedIO::TFrame> frame(Aea->ReadFrame());

            TBitStream bitstream(frame->Get(), frame->Size());

            TAtrac1Data::TBlockSizeMod mode(&bitstream);
            TAtrac1Dequantiser dequantiser;
            vector<float> specs;
            specs.resize(512);;
            dequantiser.Dequant(&bitstream, mode, &specs[0]);

            IMdct(&specs[0], mode, &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);
            SynthesisFilterBank[channel].Synthesis(&sum[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);
            for (size_t i = 0; i < TAtrac1Data::NumSamples; ++i) {
                if (sum[i] > PcmValueMax)
                    sum[i] = PcmValueMax;
                if (sum[i] < PcmValueMin)
                    sum[i] = PcmValueMin;

                data[i * srcChannels + channel] = sum[i];
            }
        }
        return TPCMEngine::EProcessResult::PROCESSED;
    };
}


TPCMEngine::TProcessLambda TAtrac1Encoder::GetLambda() {
    const uint32_t srcChannels = Aea->GetChannelNum();
    vector<IAtrac1BitAlloc*> bitAlloc(srcChannels);

    for (auto& x : bitAlloc) {
        x = new TAtrac1SimpleBitAlloc(Aea.get(), Settings.GetBfuIdxConst(), Settings.GetFastBfuNumSearch());
    }

    struct TChannelData {
        TChannelData()
            : Specs(TAtrac1Data::NumSamples)
            , Loudness(0.0)
        {}

        vector<float> Specs;
        float Loudness;
    };

    using TData = vector<TChannelData>;
    auto buf = std::make_shared<TData>(srcChannels);

    return [this, srcChannels, bitAlloc, buf](float* data, const TPCMEngine::ProcessMeta& /*meta*/) {
        TAtrac1Data::TBlockSizeMod blockSz[2];

        uint32_t windowMasks[2] = {0};
        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            float src[TAtrac1Data::NumSamples];
            for (size_t i = 0; i < TAtrac1Data::NumSamples; ++i) {
                src[i] = data[i * srcChannels + channel];
            }

            AnalysisFilterBank[channel].Analysis(&src[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);

            uint32_t& windowMask = windowMasks[channel];
            if (Settings.GetWindowMode() == TAtrac1EncodeSettings::EWindowMode::EWM_AUTO) {
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 0).Detect(&PcmBufLow[channel][0]);

                const vector<float>& invMid = InvertSpectr<128>(&PcmBufMid[channel][0]);
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 1).Detect(&invMid[0]) << 1;

                const vector<float>& invHi = InvertSpectr<256>(&PcmBufHi[channel][0]);
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 2).Detect(&invHi[0]) << 2;

                //std::cout << "trans: " << windowMask << std::endl;
            } else {
                //no transient detection, use given mask
                windowMask = Settings.GetWindowMask();
            }

            blockSz[channel]  = TAtrac1Data::TBlockSizeMod(windowMask & 0x1, windowMask & 0x2, windowMask & 0x4); //low, mid, hi

            auto& specs = (*buf)[channel].Specs;

            Mdct(&specs[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0], blockSz[channel]);

            float l = 0.0;
            for (size_t i = 0; i < specs.size(); i++) {
                float e = specs[i] * specs[i];
                l += e * LoudnessCurve[i];
            }
            (*buf)[channel].Loudness = l;
        }

        if (srcChannels == 2 && windowMasks[0] == 0 && windowMasks[1] == 0) {
            Loudness = TrackLoudness(Loudness, (*buf)[0].Loudness, (*buf)[1].Loudness);
        } else if (windowMasks[0] == 0) {
            Loudness = TrackLoudness(Loudness, (*buf)[0].Loudness);
        }

        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            bitAlloc[channel]->Write(Scaler.ScaleFrame((*buf)[channel].Specs, blockSz[channel]), blockSz[channel], Loudness / LoudFactor);
        }

        return TPCMEngine::EProcessResult::PROCESSED;
    };
}

} //namespace NAtracDEnc
