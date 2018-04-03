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
        TFloat tmp[512];
        memcpy(&tmp[0], srcBuff, 256 * sizeof(TFloat));
        if (modFn) {
            modFn(&tmp[0], &srcBuff[256]);
        }
        TFloat max = 0.0;
        for (int i = 0; i < 256; i++) {
            max = std::max(max, std::abs(srcBuff[256+i]));
            srcBuff[i] = TAtrac3Data::EncodeWindow[i] * srcBuff[256+i];
            tmp[256+i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[256+i];
        }
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
    , TransientParamsHistory(Params.SourceChannels, std::vector<TTransientParam>(4))
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
            if (!std::isnan(level)) {
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

void TAtrac3Processor::ResetTransientParamsHistory(int channel, int band)
{
    TransientParamsHistory[channel][band] = {-1 , 1, -1, 1, -1, 1};
}

void TAtrac3Processor::SetTransientParamsHistory(int channel, int band, const TTransientParam& params)
{
    TransientParamsHistory[channel][band] = params;
}

const TAtrac3Processor::TTransientParam& TAtrac3Processor::GetTransientParamsHistory(int channel, int band) const
{
    return TransientParamsHistory[channel][band];
}

TAtrac3Processor::TTransientParam TAtrac3Processor::CalcTransientParam(const std::vector<TFloat>& gain, const TFloat lastMax)
{
    int32_t attack0Location = -1; // position where gain is risen up, -1 - no attack
    TFloat attack0Relation = 1;

    const TFloat attackThreshold = 2;

    {
        // pre-echo searching
        // relative to previous half frame
        for (uint32_t i = 0; i < gain.size(); i++) {
            const TFloat tmp = gain[i] / lastMax;
            if (tmp > attackThreshold) {
                attack0Relation = tmp;
                attack0Location = i;
                break;
            }
         }
    }

    int32_t attack1Location = -1;
    TFloat attack1Relation = 1;
    {
        // pre-echo searching
        // relative to previous subsamples block
        TFloat q = gain[0];
        for (uint32_t i = 1; i < gain.size(); i++) {
            const TFloat tmp = gain[i] / q;
            if (tmp > attackThreshold) {
                attack1Relation = tmp;
                attack1Location = i;
            }
            q = std::max(q, gain[i]);
        }
    }

    int32_t releaseLocation = -1; // position where gain is fallen down, -1 - no release
    TFloat releaseRelation = 1;

    const TFloat releaseTreshold = 2;
    {
        // post-echo searching
        // relative to current frame
        TFloat q = gain.back();
        for (uint32_t i = gain.size() - 2; i > 0; --i) {
            const TFloat tmp = gain[i] / q;
            if (tmp > releaseTreshold) {
                releaseRelation = tmp;
                releaseLocation = i;
                break;
            }
            q = std::max(q, gain[i]);
        }
    }

    return {attack0Location, attack0Relation, attack1Location, attack1Relation, releaseLocation, releaseRelation};
}

void TAtrac3Processor::CreateSubbandInfo(TFloat* in[4],
                                         uint32_t channel,
                                         TTransientDetector* transientDetector,
                                         TAtrac3Data::SubbandInfo* subbandInfo)
{

    auto relToIdx = [this](TFloat rel) {
        rel = 1.0/rel;
        return (uint32_t)(RelationToIdx(rel));
    };

    for (int band = 0; band < 4; ++band) {

        const TFloat* srcBuff = in[band];

        const TFloat* const lastMax = &PrevPeak[channel][band];

        std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve;
        const std::vector<TFloat> gain = AnalyzeGain(srcBuff, 256, 32, false);

        auto transientParam = CalcTransientParam(gain, *lastMax);
        bool hasTransient = false;

        if (transientParam.Attack0Location == -1 && transientParam.Attack1Location == -1 && transientParam.ReleaseLocation == -1) {
            // No transient
            ResetTransientParamsHistory(channel, band);
            continue;
        }
        if (transientParam.Attack0Location == -1 && transientParam.Attack1Location == -1) {
            // Release only in current frame - PostEcho. Not implemented yet.
            // Note: "Hole like" transient also possible (if value is grather in next frame),
            // but we keep peak value of this frame, so in next frame we will use this peak value
            // for searching attack.
            // Handling "Hole like" transients also not implemented. But it should be masked
            SetTransientParamsHistory(channel, band, transientParam);
            continue;
        }

        auto transientParamHistory = GetTransientParamsHistory(channel, band);

        if (transientParamHistory.Attack0Location == -1 && transientParamHistory.Attack1Location == -1 && transientParamHistory.ReleaseLocation == -1 &&
            transientParam.Attack0Location != -1 /*&& transientParam.Attack1Location == -1*/ && transientParam.ReleaseLocation == -1) {
            // No transient at previous frame, but transient (attack) at border of first and second half - simplest way, just scale the first half.

            //std::cout << "CASE 1: " << transientParam.Attack0Location << " " << transientParam.Attack0Relation << std::endl;
            auto idx = relToIdx(transientParam.Attack0Relation);
            //std::cout << "PREV PEAK: " << *lastMax << " " << idx << std::endl;
            curve.push_back({idx, (uint32_t)transientParam.Attack0Location});
            hasTransient = true;
        }

        //std::cout << "transient params band: " << band <<  " atack0loc: " << transientParam.Attack0Location << " atack0rel: " << transientParam.Attack0Relation <<
        //                            " atack1loc: " << transientParam.Attack1Location << " atack1rel: " << transientParam.Attack1Relation <<
        //                            " releaseloc: " << transientParam.ReleaseLocation << " releaserel: "<< transientParam.ReleaseRelation << std::endl;

        //for (int i = 0; i < 256; i++) {
        //    std::cout << i << "   " << srcBuff[i] << "  |  " << srcBuff[i-256] << std::endl;
        //}
 

        SetTransientParamsHistory(channel, band, transientParam);
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

    TAtrac3BitStreamWriter* bitStreamWriter = new TAtrac3BitStreamWriter(omaptr, *Params.ConteinerParams, Params.BfuIdxConst);
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

            {
                TFloat* p[4] = {PcmBuffer.GetSecond(channel), PcmBuffer.GetSecond(channel+2), PcmBuffer.GetSecond(channel+4), PcmBuffer.GetSecond(channel+6)};
                SplitFilterBank[channel].Split(&src[0], p);
            }
            
            TSce* sce = &SingleChannelElements[channel];

            sce->SubbandInfo.Reset();
            if (!Params.NoGainControll) {
                TFloat* p[4] = {PcmBuffer.GetSecond(channel), PcmBuffer.GetSecond(channel+2), PcmBuffer.GetSecond(channel+4), PcmBuffer.GetSecond(channel+6)};
                CreateSubbandInfo(p, channel, &TransientDetectors[channel*4], &sce->SubbandInfo); //4 detectors per band
            }

            TFloat* maxOverlapLevels = PrevPeak[channel];

            {
                TFloat* p[4] = {PcmBuffer.GetFirst(channel), PcmBuffer.GetFirst(channel+2), PcmBuffer.GetFirst(channel+4), PcmBuffer.GetFirst(channel+6)};
                Mdct(specs.data(), p, maxOverlapLevels, MakeGainModulatorArray(sce->SubbandInfo));
            }

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
