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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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

void TAtrac3MDCT::Mdct(TFloat specs[1024], TFloat* bands[4], TFloat maxLevels[4], TGainModulatorArray gainModulators)
{
    for (int band = 0; band < 4; ++band) {
        TFloat* srcBuff = bands[band];
        TFloat* const curSpec = &specs[band*256];
        TGainModulator modFn = gainModulators[band];
        vector<TFloat> tmp(512);
        memcpy(&tmp[0], &srcBuff[256], 256 * sizeof(TFloat));
        if (modFn) {
            modFn(tmp.data(), srcBuff);
        }
        TFloat max = 0.0;
        for (int i = 0; i < 256; i++) {
            max = std::max(max, std::abs(srcBuff[i]));
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
        maxLevels[band] = max;
    }
}

void TAtrac3MDCT::Mdct(TFloat specs[1024], TFloat* bands[4], TGainModulatorArray gainModulators)
{
    static TFloat dummy[4];
    Mdct(specs, bands, dummy, gainModulators);
}

void TAtrac3MDCT::Midct(TFloat specs[1024], TFloat* bands[4], TGainDemodulatorArray gainDemodulators)
{
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
    , SingleChannelElements(Params.SourceChannels)
{}

TAtrac3Processor::~TAtrac3Processor()
{}

TAtrac3MDCT::TGainModulatorArray TAtrac3MDCT::MakeGainModulatorArray(const TAtrac3Data::SubbandInfo& si)
{
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
TAtrac3Data::TTonalComponents TAtrac3Processor::ExtractTonalComponents(TFloat* specs, TTonalDetector fn)
{
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
                        std::cerr << "overflow: " << specs[n] << " at: " << n << std::endl;
                        //res.push_back({n, specs[n] - shift});
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

void TAtrac3Processor::MapTonalComponents(const TTonalComponents& tonalComponents, vector<TTonalBlock>* componentMap)
{
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
        componentMap->push_back({&tonalComponents[startPos], 7, scaledBlock});
    }
}


TFloat TAtrac3Processor::LimitRel(TFloat x)
{
    return std::min(std::max(x, GainLevel[15]), GainLevel[0]);
}

TAtrac3Processor::TTransientParam TAtrac3Processor::CalcTransientParam(const std::vector<TFloat>& gain, const TFloat lastMax)
{
    int32_t attackLocation = 0;
    TFloat attackRelation = 1;

    const TFloat attackThreshold = 4;
    //pre-echo searching
    TFloat tmp;
    TFloat q = lastMax; //std::max(lastMax, gain[0]);
    tmp = gain[0] / q;
    if (tmp > attackThreshold) {
        attackRelation = tmp;
    } else {
        for (uint32_t i = 0; i < gain.size() -1; ++i) {
            q =  std::max(q, gain[i]);
            tmp = gain[i+1] / q;
            if (tmp > attackThreshold) {
                attackRelation = tmp;
                attackLocation = i;
                break;
            }
        }
    }

    int32_t releaseLocation = 0;
    TFloat releaseRelation = 1;

    const TFloat releaseTreshold = 4;
    //post-echo searching
    q = 0;
    for (uint32_t i = gain.size() - 1; i > 0; --i) {
        q = std::max(q, gain[i]);
        tmp = gain[i-1] / q;
        if (tmp > releaseTreshold) {
            releaseRelation = tmp;
            releaseLocation = i;
            break;
        }
    }
    if (releaseLocation == 0) {
        q = std::max(q, gain[0]);
        tmp = lastMax / q;
        if (tmp > releaseTreshold) {
            releaseRelation = tmp;
        }
    }

    return {attackLocation, attackRelation, releaseLocation, releaseRelation};
}

void TAtrac3Processor::CreateSubbandInfo(TFloat* in[4],
                                         uint32_t channel,
                                         TTransientDetector* transientDetector,
                                         TAtrac3Data::SubbandInfo* subbandInfo)
{
    for (int band = 0; band < 4; ++band) {

        TFloat invBuf[256];
        if (band & 1) {
            memcpy(invBuf, in[band], 256*sizeof(TFloat));
            InvertSpectrInPlase<256>(invBuf);
        }
        const TFloat* srcBuff = (band & 1) ? invBuf : in[band];

        const TFloat* const lastMax = &PrevPeak[channel][band];

        std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve;
        std::vector<TFloat> gain = AnalyzeGain(srcBuff, 256, 32, false);

        auto transientParam = CalcTransientParam(gain, *lastMax);
        bool hasTransient = (transientParam.AttackRelation != 1.0 || transientParam.ReleaseRelation != 1.0);

        //combine attack and release
        TFloat relA = 1;
        TFloat relB = 1;
        TFloat relC = 1;
        uint32_t loc1 = 0;
        uint32_t loc2 = 0;
        if (transientParam.AttackLocation < transientParam.ReleaseLocation) {
            //Peak like transient
            relA = transientParam.AttackRelation;
            loc1 = transientParam.AttackLocation;
            relB = 1;
            loc2 = transientParam.ReleaseLocation;
            relC = transientParam.ReleaseRelation;
        } else if (transientParam.AttackLocation > transientParam.ReleaseLocation) {
            //Hole like transient
            relA = transientParam.AttackRelation;
            loc1 = transientParam.ReleaseLocation;
            relB = transientParam.AttackRelation * transientParam.ReleaseRelation;
            loc2 = transientParam.AttackLocation;
            relC = transientParam.ReleaseRelation;
        } else {
            //???
            //relA = relB = relC = transientParam.AttackRelation * transientParam.ReleaseRelation;
            //loc1 = loc2 = transientParam.ReleaseLocation;
            hasTransient = false;
        }
        //std::cout << "loc: " << loc1 << " " << loc2 << " rel: " << relA << " " << relB << " " << relC <<  std::endl;

        if (relC != 1) {
            relA /= relC;
            relB /= relC;
            relC = 1.0;
        }
        auto relToIdx = [this](TFloat rel) {
            rel = LimitRel(1/rel);
            return (uint32_t)(15 - Log2FloatToIdx(rel, 2048));
        };
        curve.push_back({relToIdx(relA), loc1});
        if (loc1 != loc2) {
            curve.push_back({relToIdx(relB), loc2});
        }
        if (loc2 != 31) {
            curve.push_back({relToIdx(relC), 31});
        }

        if (hasTransient) {
            subbandInfo->AddSubbandCurve(band, std::move(curve));
        }

    }
}


TPCMEngine<TFloat>::TProcessLambda TAtrac3Processor::GetEncodeLambda()
{
    TOma* omaptr = dynamic_cast<TOma*>(Oma.get());
    if (!omaptr) {
        std::cerr << "Wrong container" << std::endl;
        abort();
    }

    TAtrac3BitStreamWriter* bitStreamWriter = new TAtrac3BitStreamWriter(omaptr, *Params.ConteinerParams);
    return [this, bitStreamWriter](TFloat* data, const TPCMEngine<TFloat>::ProcessMeta& meta) {
        using TSce = TAtrac3BitStreamWriter::TSingleChannelElement;

        // TTonalBlock has pointer to the TTonalVal so TTonalComponents must be avaliable
        // TODO: this code should be rewritten
        TTonalComponents tonals[2];

        assert(SingleChannelElements.size() == meta.Channels);
        for (uint32_t channel = 0; channel < SingleChannelElements.size(); channel++) {
            vector<TFloat> specs(1024);
            TFloat src[NumSamples];

            for (size_t i = 0; i < NumSamples; ++i) {
                src[i] = data[i * meta.Channels  + channel] / 4.0;
            }

            TFloat* p[4] = {&PcmBuffer[channel][0][0], &PcmBuffer[channel][1][0], &PcmBuffer[channel][2][0], &PcmBuffer[channel][3][0]};
            SplitFilterBank[channel].Split(&src[0], p);
            
            TSce* sce = &SingleChannelElements[channel];

            sce->SubbandInfo.Reset();
            if (!Params.NoGainControll) {
                CreateSubbandInfo(p, channel, &TransientDetectors[channel*4], &sce->SubbandInfo); //4 detectors per band
            }

            TFloat* maxOverlapLevels = PrevPeak[channel];

            Mdct(specs.data(), p, maxOverlapLevels, MakeGainModulatorArray(sce->SubbandInfo));
            tonals[channel] = Params.NoTonalComponents ?
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

            sce->TonalBlocks.clear();
            MapTonalComponents(tonals[channel], &sce->TonalBlocks);

            //TBlockSize for ATRAC3 - 4 subband, all are long (no short window)
            sce->ScaledBlocks = std::move(Scaler.ScaleFrame(specs, TBlockSize()));

        }

        bitStreamWriter->WriteSoundUnit(SingleChannelElements);
    };
}

TPCMEngine<TFloat>::TProcessLambda TAtrac3Processor::GetDecodeLambda()
{
    abort();
    return {};
}

}//namespace NAtracDEnc
