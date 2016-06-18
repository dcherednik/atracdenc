#include "atrac3denc.h"
#include "transient_detector.h"
#include "util.h"
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <cmath>
namespace NAtracDEnc {

using namespace NMDCT;
using namespace NAtrac3;
using std::vector;

void TAtrac3MDCT::Mdct(TFloat specs[1024], TFloat* bands[4], TGainModulatorArray gainModulators) {
    for (int band = 0; band < 4; ++band) {
        TFloat* srcBuff = bands[band];
        TFloat* const curSpec = &specs[band*256];
        TGainModulator modFn = gainModulators[band];
        vector<TFloat> tmp(512);
        memcpy(&tmp[0], &srcBuff[256], 256 * sizeof(TFloat));
        if (modFn) {
            modFn(tmp.data(), srcBuff);
        }
        for (int i = 0; i < 256; i++) {
            srcBuff[256+i] = TAtrac3Data::EncodeWindow[i] * srcBuff[i];
            srcBuff[i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[i];
        }
        memcpy(&tmp[256], &srcBuff[0], 256 * sizeof(TFloat));
        const vector<TFloat>& sp = Mdct512(&tmp[0]);
        assert(sp.size() == 256);
        memcpy(curSpec, sp.data(), 256 * sizeof(TFloat));
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
    }
}

void TAtrac3MDCT::Midct(TFloat specs[1024], TFloat* bands[4], TGainDemodulatorArray gainDemodulators) {
    for (int band = 0; band < 4; ++band) {
        TFloat* dstBuff = bands[band];
        TFloat* curSpec = &specs[band*256];
        TFloat* prevBuff = dstBuff + 256;
        TAtrac3GainProcessor::TGainDemodulator demodFn = gainDemodulators[band];
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
        vector<TFloat> inv  = Midct512(curSpec);
        assert(inv.size()/2 == 256);
        for (int j = 0; j < 256; ++j) {
            inv[j] *= /*2 */ DecodeWindow[j];
            inv[511 - j] *= /*2*/ DecodeWindow[j];
        }
        if (demodFn) {
            demodFn(dstBuff, inv.data(), prevBuff);
        } else {
            for (uint32_t j = 0; j < 256; ++j) {
                dstBuff[j] = inv[j] + prevBuff[j];
            }
        }
        memcpy(prevBuff, &inv[256], sizeof(TFloat)*256);
    }
}

TAtrac3Processor::TAtrac3Processor(TCompressedIOPtr&& oma, TAtrac3EncoderSettings&& encoderSettings)
    : Oma(std::move(oma))
    , Params(std::move(encoderSettings))
    , TransientDetectors(2 * 4, TTransientDetector(8, 256)) //2 - channels, 4 - bands
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

//TODO:
TAtrac3Data::TTonalComponents TAtrac3Processor::ExtractTonalComponents(TFloat* specs, TTonalDetector fn) {
    TAtrac3Data::TTonalComponents res;
    const float thresholds[TAtrac3Data::NumQMF] = { 0.9, 2.4, 2.8, 3.2 };
    for (uint8_t bandNum = 0; bandNum < this->NumQMF; ++bandNum) {
        //disable for frequence above 16KHz until we works without proper psy
        if (bandNum > 2)
            continue;
        for (uint8_t blockNum = BlocksPerBand[bandNum]; blockNum < BlocksPerBand[bandNum + 1]; ++blockNum) {
            const uint16_t specNumStart = SpecsStartLong[blockNum];
            const uint16_t specNumEnd = specNumStart + SpecsPerBlock[blockNum];
            float level = fn(specs + specNumStart, SpecsPerBlock[blockNum]);
            if (!isnan(level)) {
                for (uint16_t n = specNumStart; n < specNumEnd; ++n) {
                    //TODO:
                    TFloat absValue = std::abs(specs[n]);
                    if (absValue > 65535.0) {
                        TFloat shift = (specs[n] > 0) ? 65535.0 : -65535.0;

                        std::cerr << "shift overflowed value " << specs[n] << " " << specs[n] - shift << " " << shift << std::endl;
                        res.push_back({n, specs[n] - shift});
                        specs[n] = shift;
                    } else if (log10(std::abs(specs[n])) - log10(level) > thresholds[bandNum]) {
                        res.push_back({n, specs[n]/* - level*/});
                        specs[n] = 0;//level;
                    }
                    
                }

            }
        }
    }
    return res;
}
std::vector<TTonalComponent> TAtrac3Processor::MapTonalComponents(const TTonalComponents& tonalComponents) {
    vector<TTonalComponent> componentMap;
    for (uint16_t i = 0; i < tonalComponents.size();) {
        const uint16_t startPos = i;
        uint16_t curPos;
        do {
            curPos = tonalComponents[i].Pos;
            ++i;
        } while ( i < tonalComponents.size() && tonalComponents[i].Pos == curPos + 1 && i - startPos < 7);
        const uint16_t len = i - startPos;
        TFloat tmp[8];
        for (uint8_t j = 0; j < len; ++j)
            tmp[j] = tonalComponents[startPos + j].Val;
        const TScaledBlock& scaledBlock = Scaler.Scale(tmp, len);
        componentMap.push_back({&tonalComponents[startPos], 7, scaledBlock});
    }
    return componentMap;
}

TAtrac3Data::SubbandInfo TAtrac3Processor::CreateSubbandInfo(TFloat* in[4], uint32_t channel, TTransientDetector* transientDetector) {
    assert(false); //not implemented
    return {};
}

TPCMEngine<TFloat>::TProcessLambda TAtrac3Processor::GetEncodeLambda() {
    TOma* omaptr = dynamic_cast<TOma*>(Oma.get());
    if (!omaptr) {
        std::cerr << "Wrong container" << std::endl;
        abort();
    }

    TAtrac3BitStreamWriter* bitStreamWriter = new TAtrac3BitStreamWriter(omaptr, *Params.ConteinerParams);
    return [this, bitStreamWriter](TFloat* data, const TPCMEngine<TFloat>::ProcessMeta& meta) {
        for (uint32_t channel=0; channel < 2; channel++) {
            vector<TFloat> specs(1024);
            TFloat src[NumSamples];
            for (int i = 0; i < NumSamples; ++i) {
                src[i] = data[meta.Channels == 1 ? i : (i * 2 + channel)] / 4.0; //no mono mode in atrac3. //TODO we can TFloat frame after encoding
            }

            TFloat* p[4] = {&PcmBuffer[channel][0][0], &PcmBuffer[channel][1][0], &PcmBuffer[channel][2][0], &PcmBuffer[channel][3][0]};
            SplitFilterBank[channel].Split(&src[0], p);
            
            TAtrac3Data::SubbandInfo siCur = Params.NoGainControll ?
                TAtrac3Data::SubbandInfo() : CreateSubbandInfo(p, channel, &TransientDetectors[channel*4]); //4 detectors per band

            Mdct(specs.data(), p, MakeGainModulatorArray(siCur));
            TTonalComponents tonals = Params.NoTonalComponents ? 
                    TAtrac3Data::TTonalComponents() : ExtractTonalComponents(specs.data(), [](const TFloat* spec, uint16_t len) {
                std::vector<TFloat> magnitude(len);
                for (uint16_t i = 0; i < len; ++i) {
                    magnitude[i] = std::abs(spec[i]);
                }
                float median = CalcMedian(magnitude.data(), len);
                for (uint16_t i = 0; i < len; ++i) {
                    if (median > 0.001) {
                        return median;
                    }
                }
                return NAN;
            });

            const std::vector<TTonalComponent>& components = MapTonalComponents(tonals);

            //TBlockSize for ATRAC3 - 4 subband, all are long (no short window)
            bitStreamWriter->WriteSoundUnit(siCur, components, Scaler.ScaleFrame(specs, TBlockSize())); 
        }
    };
}

TPCMEngine<TFloat>::TProcessLambda TAtrac3Processor::GetDecodeLambda() {
    abort();
    return {};
}

}//namespace NAtracDEnc
