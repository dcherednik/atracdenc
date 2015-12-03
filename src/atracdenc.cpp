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
TAtrac1Processor::TAtrac1Processor(TAeaPtr&& aea, bool mono)
    : MixChannel(mono)
    , Aea(std::move(aea))
    , Mdct512(2)
    , Mdct256(1)
{
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

void TAtrac1Processor::Mdct(double Specs[512], double* low, double* mid, double* hi) {
    uint32_t pos = 0;
    for (uint32_t band = 0; band < QMF_BANDS; band++) {
        double* srcBuf = (band == 0) ? low : (band == 1) ? mid : hi;
        uint32_t bufSz = (band == 2) ? 256 : 128; 
        uint32_t winStart = (band == 2) ? 112 : 48; 
        vector<double> tmp(512);

        memcpy(&tmp[winStart], &srcBuf[bufSz], 32 * sizeof(double));
        for (int i = 0; i < 32; i++) {
            srcBuf[bufSz + i] = TAtrac1Data::SineWindow[i] * srcBuf[bufSz - 32 + i];
            srcBuf[bufSz - 32 + i] = TAtrac1Data::SineWindow[31 - i] * srcBuf[bufSz - 32 + i];
        }
        memcpy(&tmp[winStart+32], &srcBuf[0], bufSz * sizeof(double));
        const vector<double>&  sp = (band == 2) ? Mdct512(&tmp[0]) : Mdct256(&tmp[0]);
        if (band) {
            for (uint32_t j = 0; j < sp.size()/2; j++) {
                Specs[pos+j] = sp[bufSz - 1 -j];
                Specs[pos + bufSz - 1 -j] = sp[j];
            }
        } else {
            for (uint32_t i = 0; i < sp.size(); i++) {
                Specs[pos + i] = sp[i];
            }
        }
        /*
        if (band == 2) {
            for (int i = 0; i < bufSz * 2; i++) {
                cout << "tmp " << i << " " << tmp[i] << endl;
            }
            for (int i = 0; i < sp.size(); ++i ) {
                cout << "band2 " << i << "  " << Specs[pos + i] << " " << endl;
            }
        }
        */
        pos += bufSz;
    } 
}
void TAtrac1Processor::IMdct(double Specs[512], const TBlockSize& mode, double* low, double* mid, double* hi) {
    uint32_t pos = 0;
    for (uint32_t band = 0; band < QMF_BANDS; band++) {
        const uint32_t numMdctBlocks = 1 << mode.LogCount[band];
        const uint32_t blockSize = (numMdctBlocks == 1) ? ((band == 2) ? 256 : 128) : 32;
        //if (blockSize == 32) {
        //    cout << "SHORT: " << numMdctBlocks << " band: " << band << endl;
        //}
        uint32_t start = 0;

        double* dstBuf = (band == 0) ? low : (band == 1) ? mid : hi;


        vector<double> invBuf;
        invBuf.resize(512);
        double* prevBuf = &dstBuf[blockSize*2  - 16];
        static uint32_t fc;
        for (uint32_t block = 0; block < numMdctBlocks; block++) {

    //        cout << "fc counter: " << fc++ << endl;
            if (band) {
                for (uint32_t j = 0; j < blockSize/2; j++) {
                    double tmp = Specs[pos+j];
                    Specs[pos+j] = Specs[pos + blockSize - 1 -j];
                    Specs[pos + blockSize - 1 -j] = tmp;
                }
            }
            vector<double> inv = (blockSize == 32) ? midct(&Specs[pos], blockSize) : (blockSize == 128) ? Midct256(&Specs[pos]) : Midct512(&Specs[pos]);
            for (int i = 0; i < (inv.size()/2); i++) {

                invBuf[start+i] = inv[i + inv.size()/4];
            }

            vector_fmul_window(dstBuf + start, prevBuf, &invBuf[start], &TAtrac1Data::SineWindow[0], 16);

            prevBuf = &invBuf[start+16];
            start += blockSize;
            pos += blockSize;
        }
        if (numMdctBlocks == 1)
            memcpy(dstBuf + 32, &invBuf[16], ((band == 2) ? 240 : 112) * sizeof(double));

        for (int j = 0; j < 16; j++) {
            dstBuf[blockSize*2 - 16  + j] = invBuf[blockSize - 16 + j];
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
            const int numChannel = data[0].size();
            for (int i = 0; i < NumSamples; ++i) {
                data[i][srcChannels - 1-channel] = sum[i];
            }
        }

    };
}


TPCMEngine<double>::TProcessLambda TAtrac1Processor::GetEncodeLambda(const TAtrac1EncodeSettings& settings) {
    const uint32_t srcChannels = Aea->GetChannelNum();
    //TODO: should not be here
    //vector<char> dummy;
    //dummy.resize(212 * srcChannels);
    //Aea->WriteFrame(dummy);
    //cout << "Encode, channels: " << srcChannels << endl;
    vector<IAtrac1BitAlloc*> bitAlloc;
    for (int i = 0; i < srcChannels; i++)
        bitAlloc.push_back(new TAtrac1SimpleBitAlloc(Aea.get(), settings.GetBfuIdxConst(), settings.GetFastBfuNumSearch()));
        //bitAlloc.push_back(new TAtrac1PsyBitAlloc(Aea.get()));

    return [this, srcChannels, bitAlloc](vector<double>* data) {
        for (uint32_t channel = 0; channel < srcChannels; channel++) {
            double src[NumSamples];
            double sum[512];
            vector<double> specs;
            specs.resize(512);
            for (int i = 0; i < NumSamples; ++i) {
                src[i] = data[i][channel];
            }
            SplitFilterBank[channel].Split(&src[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);

            Mdct(&specs[0], &PcmBufLow[channel][0], &PcmBufMid[channel][0], &PcmBufHi[channel][0]);
            bitAlloc[channel]->Write(Scaler.Scale(specs));
        }
    };
}


}
