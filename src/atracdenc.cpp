#include <vector>

#include "atracdenc.h"
#include "bitstream/bitstream.h"
#include "atrac/atrac1.h"
#include "atrac/atrac1_dequantiser.h"
#include "atrac/atrac1_qmf.h"
#include "atrac/atrac1_bitalloc.h"

namespace NAtracDEnc {
using namespace std;
using namespace NBitStream;
using namespace NAtrac1;
using namespace NMDCT;

template<int N>
static vector<double> invertSpectr(double* in) {
    vector<double> buf(N);
    memcpy(&buf[0], in, N * sizeof(double));
    for (int i = 0; i < N; i+=2)
        buf[i] *= -1;
    return buf;
}

TAtrac1Processor::TAtrac1Processor(TAeaPtr&& aea, bool mono)
    : MixChannel(mono)
    , Aea(std::move(aea))
{
    (void)MixChannel; //TODO
}

static void vector_fmul_window(double *dst, const double *src0,
                                const double *src1, const double *win, int len)
{
    int i, j;

    dst  += len;
    win  += len;
    src0 += len;

    for (i = -len, j = len - 1; i < 0; i++, j--) {
        double s0 = src0[i];
        double s1 = src1[j];
        double wi = win[i];
        double wj = win[j];
        dst[i] = s0 * wj - s1 * wi;
        dst[j] = s0 * wi + s1 * wj;
    }
}

vector<double> midct(double* x, int N) {
    vector<double> res;
    for (int n = 0; n < 2 * N; n++) {
        double sum = 0;
        for (int k = 0; k < N; k++) {
            sum += (x[k] * cos((M_PI/N) * ((double)n + 0.5 + N/2) * ((double)k + 0.5)));
        }

        res.push_back(sum);
    }
    return res;
}

void TAtrac1MDCT::Mdct(double Specs[512], double* low, double* mid, double* hi, const TBlockSize& blockSize) {
    uint32_t pos = 0;
    for (uint32_t band = 0; band < QMF_BANDS; band++) {
        const uint32_t numMdctBlocks = 1 << blockSize.LogCount[band];
        double* srcBuf = (band == 0) ? low : (band == 1) ? mid : hi;
        uint32_t bufSz = (band == 2) ? 256 : 128; 
        const uint32_t blockSz = (numMdctBlocks == 1) ? bufSz : 32;
        uint32_t winStart = (numMdctBlocks == 1) ? ((band == 2) ? 112 : 48) : 0;
        //compensate level for 3rd band in case of short window
        const double multiple = (numMdctBlocks != 1 && band == 2) ? 2.0 : 1.0;
        vector<double> tmp(512);
        uint32_t blockPos = 0;

        for (int k = 0; k < numMdctBlocks; ++k) {
            memcpy(&tmp[winStart], &srcBuf[bufSz], 32 * sizeof(double));
            for (int i = 0; i < 32; i++) {
                srcBuf[bufSz + i] = TAtrac1Data::SineWindow[i] * srcBuf[blockPos + blockSz - 32 + i];
                srcBuf[blockPos + blockSz - 32 + i] = TAtrac1Data::SineWindow[31 - i] * srcBuf[blockPos + blockSz - 32 + i];
            }
            memcpy(&tmp[winStart+32], &srcBuf[blockPos], blockSz * sizeof(double));
            const vector<double>&  sp = (numMdctBlocks == 1) ? ((band == 2) ? Mdct512(&tmp[0]) : Mdct256(&tmp[0])) : Mdct64(&tmp[0]);
            for (uint32_t i = 0; i < sp.size(); i++) {
                Specs[blockPos + pos + i] = sp[i] * multiple;
            }
            if (band) {
                for (uint32_t j = 0; j < sp.size() / 2; j++) {
                    double tmp = Specs[blockPos + pos +j];
                    Specs[blockPos + pos + j] = Specs[blockPos + pos + sp.size() - 1 -j];
                    Specs[blockPos + pos + sp.size() - 1 -j] = tmp;
                }
            }

            blockPos += 32;
        }
        pos += bufSz;
    } 
}
void TAtrac1MDCT::IMdct(double Specs[512], const TBlockSize& mode, double* low, double* mid, double* hi) {
    uint32_t pos = 0;
    for (uint32_t band = 0; band < QMF_BANDS; band++) {
        const uint32_t numMdctBlocks = 1 << mode.LogCount[band];
        const uint32_t bufSz = (band == 2) ? 256 : 128;
        const uint32_t blockSz = (numMdctBlocks == 1) ? bufSz : 32;
        uint32_t start = 0;

        double* dstBuf = (band == 0) ? low : (band == 1) ? mid : hi;

        vector<double> invBuf(512);
        double* prevBuf = &dstBuf[bufSz * 2  - 16];
        for (uint32_t block = 0; block < numMdctBlocks; block++) {

            if (band) {
                for (uint32_t j = 0; j < blockSz/2; j++) {
                    double tmp = Specs[pos+j];
                    Specs[pos+j] = Specs[pos + blockSz - 1 -j];
                    Specs[pos + blockSz - 1 -j] = tmp;
                }
            }

            vector<double> inv = (numMdctBlocks != 1) ? midct(&Specs[pos], blockSz) : (bufSz == 128) ? Midct256(&Specs[pos]) : Midct512(&Specs[pos]);
            for (int i = 0; i < (inv.size()/2); i++) {
                invBuf[start+i] = inv[i + inv.size()/4];
            }

            vector_fmul_window(dstBuf + start, prevBuf, &invBuf[start], &TAtrac1Data::SineWindow[0], 16);

            prevBuf = &invBuf[start+16];
            start += blockSz;
            pos += blockSz;
        }
        if (numMdctBlocks == 1)
            memcpy(dstBuf + 32, &invBuf[16], ((band == 2) ? 240 : 112) * sizeof(double));

        for (int j = 0; j < 16; j++) {
            dstBuf[bufSz*2 - 16  + j] = invBuf[bufSz - 16 + j];
        }
    }
}

TPCMEngine<double>::TProcessLambda TAtrac1Processor::GetDecodeLambda() {
    return [this](vector<double>* data) {
        double sum[512];
        const uint32_t srcChannels = Aea->GetChannelNum();
        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            std::unique_ptr<TAea::TFrame> frame(Aea->ReadFrame());
            TBitStream bitstream(&(*frame.get())[0], frame->size());
      //      cout << "frame size: " << bitstream.GetBufSize() << endl;

            TBlockSize mode(&bitstream);
            TAtrac1Dequantiser dequantiser;
            vector<double> specs;
            specs.resize(512);;
            dequantiser.Dequant(&bitstream, mode, &specs[0]);

            IMdct(&specs[0], mode, &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);
            SynthesisFilterBank[channel].Synthesis(&sum[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);
            for (int i = 0; i < NumSamples; ++i) {
                if (sum[i] > PcmValueMax)
                    sum[i] = PcmValueMax;
                if (sum[i] < PcmValueMin)
                    sum[i] = PcmValueMin;

                data[i][channel] = sum[i];
            }
        }

    };
}


TPCMEngine<double>::TProcessLambda TAtrac1Processor::GetEncodeLambda(const TAtrac1EncodeSettings& settings) {
    const uint32_t srcChannels = Aea->GetChannelNum();
    vector<IAtrac1BitAlloc*> bitAlloc;
    for (int i = 0; i < srcChannels; i++)
        bitAlloc.push_back(new TAtrac1SimpleBitAlloc(Aea.get(), settings.GetBfuIdxConst(), settings.GetFastBfuNumSearch()));

    return [this, srcChannels, bitAlloc, settings](vector<double>* data) {
        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            double src[NumSamples];
            vector<double> specs(512);
            for (int i = 0; i < NumSamples; ++i) {
                src[i] = data[i][channel];
            }

            SplitFilterBank[channel].Split(&src[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);

            uint32_t windowMask = 0;
            if (settings.GetWindowMode() == TAtrac1EncodeSettings::EWindowMode::EWM_AUTO) {
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 0).Detect(&PcmBufLow[channel][0]);

                const vector<double>& invMid = invertSpectr<128>(&PcmBufMid[channel][0]);
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 1).Detect(&invMid[0]) << 1;

                const vector<double>& invHi = invertSpectr<256>(&PcmBufHi[channel][0]);
                windowMask |= (uint32_t)TransientDetectors.GetDetector(channel, 2).Detect(&invHi[0]) << 2;

                //std::cout << "trans: " << windowMask << std::endl;
            } else {
                //no transient detection, use given mask
                windowMask = settings.GetWindowMask();
            }
            const TBlockSize blockSize(windowMask & 0x1, windowMask & 0x2, windowMask & 0x4); //low, mid, hi

            //for (int i = 0; i < 256; ++i) {
            //    std::cout << PcmBufHi[channel][i] << std::endl;
            //}
            //std::cout<< "============" << std::endl;
            Mdct(&specs[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0], blockSize);
            bitAlloc[channel]->Write(Scaler.Scale(specs, blockSize), blockSize);
        }
    };
}


}
