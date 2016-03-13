#include "atrac3denc.h"
#include "atrac/atrac3_bitstream.h"
#include <assert.h>

#include <iostream>

namespace NAtracDEnc {

using namespace NMDCT;
using std::vector;

void TAtrac3MDCT::Mdct(double specs[1024], double* bands[4]) {
    for (int band = 0; band < 4; ++band) {
        double* srcBuff = bands[band];
        vector<double> tmp(512);
        memcpy(&tmp[0], &srcBuff[256], 256 * sizeof(double));
        for (int i = 0; i < 256; i++) {
            srcBuff[256+i] = TAtrac3Data::EncodeWindow[i] * srcBuff[i];
            srcBuff[i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[i];
        }
        memcpy(&tmp[256], &srcBuff[0], 256 * sizeof(double));
        const vector<double>& sp = Mdct512(&tmp[0]);
        assert(sp.size() == 256);
        memcpy(&specs[band*256], sp.data(), 256*sizeof(double));
        if (band & 1) {
            for (uint32_t j = 0; j < sp.size() / 2; j++) {
                double tmp = specs[band*256 +j];
                specs[band*256 + j] = specs[band*256 + sp.size() - 1 -j];
                specs[band*256 + sp.size() - 1 -j] = tmp;
            }
        }
    }
}

TAtrac3Processor::TAtrac3Processor(TAeaPtr&& oma, const TContainerParams& params)
    : Oma(std::move(oma))
    , Params(params)
{}

TAtrac3Processor::~TAtrac3Processor()
{}

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
                src[i] = data[meta.Channels == 1 ? i : (i * 2 + channel)]; //no mono mode in atrac1. //TODO we can double frame after encoding
            }

            double* p[4] = {&PcmBuffer[channel][0][0], &PcmBuffer[channel][1][0], &PcmBuffer[channel][2][0], &PcmBuffer[channel][3][0]};
            SplitFilterBank[channel].Split(&src[0], p);
            Mdct(specs.data(), p);
            const TBlockSize blockSize(false, false, false);
            bitStreamWriter->WriteSoundUnit(TAtrac3SubbandInfo(), Scaler.Scale(specs, blockSize)); 
        }
    };
}

TPCMEngine<double>::TProcessLambda TAtrac3Processor::GetDecodeLambda() {
    abort();
    return {};
}

}//namespace NAtracDEnc
