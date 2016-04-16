#include "atrac3denc.h"
#include "atrac/atrac3_bitstream.h"
#include "util.h"
#include <assert.h>

#include <iostream>

namespace NAtracDEnc {

using namespace NMDCT;
using std::vector;

void TAtrac3MDCT::Mdct(double specs[1024], double* bands[4], TGainModulatorArray gainModulators) {
    for (int band = 0; band < 4; ++band) {
        double* srcBuff = bands[band];
        double* const curSpec = &specs[band*256];
        TGainModulator modFn = gainModulators[band];
        vector<double> tmp(512);
        memcpy(&tmp[0], &srcBuff[256], 256 * sizeof(double));
        if (modFn) {
            modFn(tmp.data(), srcBuff);
        }
        for (int i = 0; i < 256; i++) {
            srcBuff[256+i] = TAtrac3Data::EncodeWindow[i] * srcBuff[i];
            srcBuff[i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[i];
        }
        memcpy(&tmp[256], &srcBuff[0], 256 * sizeof(double));
        const vector<double>& sp = Mdct512(&tmp[0]);
        assert(sp.size() == 256);
        memcpy(curSpec, sp.data(), 256 * sizeof(double));
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
    }
}

void TAtrac3MDCT::Midct(double specs[1024], double* bands[4], TGainDemodulatorArray gainDemodulators) {
    for (int band = 0; band < 4; ++band) {
        double* dstBuff = bands[band];
        double* curSpec = &specs[band*256];
        double* prevBuff = dstBuff + 256;
        TAtrac3GainProcessor::TGainDemodulator demodFn = gainDemodulators[band];
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
        vector<double> inv  = Midct512(curSpec);
        assert(inv.size()/2 == 256);
        for (int j = 0; j < 256; ++j) {
            inv[j] *= 2 * DecodeWindow[j];
            inv[511 - j] *= 2 * DecodeWindow[j];
        }
        if (demodFn) {
            demodFn(dstBuff, inv.data(), prevBuff);
        } else {
            for (uint32_t j = 0; j < 256; ++j) {
                dstBuff[j] = inv[j] + prevBuff[j];
            }
        }
        memcpy(prevBuff, &inv[256], sizeof(double)*256);
    }
}

TAtrac3Processor::TAtrac3Processor(TAeaPtr&& oma, const TContainerParams& params)
    : Oma(std::move(oma))
    , Params(params)
{}

TAtrac3Processor::~TAtrac3Processor()
{}

TAtrac3MDCT::TGainModulatorArray TAtrac3MDCT::MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si) {
    switch (si.GetQmfNum()) {
        case 1:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), TAtrac3MDCT::TGainModulator(),
                TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator() }};
        }
        case 2:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator() }};
        }
        case 3:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                GainProcessor.Modulate(si.GetGainPoints(2)), TAtrac3MDCT::TGainModulator() }};
        }
        case 4:
        {
            return {{ GainProcessor.Modulate(si.GetGainPoints(0)), GainProcessor.Modulate(si.GetGainPoints(1)),
                GainProcessor.Modulate(si.GetGainPoints(2)), GainProcessor.Modulate(si.GetGainPoints(3)) }};
        }
        default:
            assert(false);
            return {};

    }
}

TPCMEngine<double>::TProcessLambda TAtrac3Processor::GetEncodeLambda() {
    TOma* omaptr = dynamic_cast<TOma*>(Oma.get());
    if (!omaptr) {
        std::cerr << "Wrong container" << std::endl;
        abort();
    }

    TAtrac3BitStreamWriter* bitStreamWriter = new TAtrac3BitStreamWriter(omaptr, Params);
    return [this, bitStreamWriter](double* data, const TPCMEngine<double>::ProcessMeta& meta) {
        for (uint32_t channel=0; channel < 2; channel++) {
            vector<double> specs(1024);
            double src[NumSamples];
            for (int i = 0; i < NumSamples; ++i) {
                src[i] = data[meta.Channels == 1 ? i : (i * 2 + channel)]; //no mono mode in atrac3. //TODO we can double frame after encoding
            }

            double* p[4] = {&PcmBuffer[channel][0][0], &PcmBuffer[channel][1][0], &PcmBuffer[channel][2][0], &PcmBuffer[channel][3][0]};
            SplitFilterBank[channel].Split(&src[0], p);

            TAtrac3Data::SubbandInfo siCur;

            Mdct(specs.data(), p, MakeGainModulatorArray(siCur));
            const TBlockSize blockSize(false, false, false);
            bitStreamWriter->WriteSoundUnit(siCur, Scaler.Scale(specs, blockSize)); 
        }
    };
}

TPCMEngine<double>::TProcessLambda TAtrac3Processor::GetDecodeLambda() {
    abort();
    return {};
}

}//namespace NAtracDEnc
