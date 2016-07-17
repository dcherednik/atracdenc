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


static void hpFilter(const TFloat* in, TFloat* out, uint32_t n)
{
    TFloat t0 = 0;
    TFloat t1 = 0;
    for (uint32_t i = 0; i < n; ++i) {
        TFloat x = in[i] / 4.0f;
        TFloat y = t0 + x;
        t0 = t1 + y - 2.0f * x;
        t1 = x - .5f * y;
        out[i] = y;
    }
}

void TAtrac3MDCT::Mdct(TFloat specs[1024], TFloat* bands[4], TFloat maxLevels[4], TGainModulatorArray gainModulators)
{
    for (int band = 0; band < 4; ++band) {
        TFloat* srcBuff = bands[band];
        TFloat* const curSpec = &specs[band*256];
        TGainModulator modFn = gainModulators[band];
        vector<TFloat> tmp(512);
        TFloat maxOverlapGain = 0.0;
        memcpy(&tmp[0], &srcBuff[256], 256 * sizeof(TFloat));
        if (modFn) {
            modFn(tmp.data(), srcBuff);
        }
        for (int i = 0; i < 256; i++) {
            srcBuff[256+i] = TAtrac3Data::EncodeWindow[i] * srcBuff[i];
            maxOverlapGain = std::max(maxOverlapGain, std::abs(srcBuff[256+i]));
            srcBuff[i] = TAtrac3Data::EncodeWindow[255-i] * srcBuff[i];
        }
        memcpy(&tmp[256], &srcBuff[0], 256 * sizeof(TFloat));
        const vector<TFloat>& sp = Mdct512(&tmp[0]);
        assert(sp.size() == 256);
        memcpy(curSpec, sp.data(), 256 * sizeof(TFloat));
        if (band & 1) {
            SwapArray(curSpec, 256);
        }
        maxLevels[band] = maxOverlapGain;
    }
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
std::vector<TTonalComponent> TAtrac3Processor::MapTonalComponents(const TTonalComponents& tonalComponents)
{
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


TFloat TAtrac3Processor::LimitRel(TFloat x)
{
    return std::min(std::max(x, GainLevel[15]), GainLevel[0]);
}

uint32_t TAtrac3Processor::CheckLevelOverflow(const TFloat probe, uint32_t levelIdx)
{
    //std::cout << "CheckLevelOverflow: " << probe << " start idx: " << levelIdx << std::endl;
    while (probe / GainLevel[levelIdx] > 65535) {
        if (levelIdx == 0) {
            std::cerr << "level too hi" << std::endl;
            break;
        }
        levelIdx--;
    }
    return levelIdx;
}

vector<TAtrac3Data::SubbandInfo::TGainPoint> TAtrac3Processor::FilterCurve(const vector<SubbandInfo::TGainPoint>& curve,
                                                                           const int threshold)
{
    if (curve.empty())
        return curve;

#ifndef NDEBUG
    int prev = -1;
    for (auto v : curve) {
        assert((int)v.Location > prev);
//        std::cout << "in: " << v.Level << " " << v.Location << " threshold: " << threshold << std::endl;
        prev = v.Location;
    }
#endif

    std::vector<TAtrac3Data::SubbandInfo::TGainPoint> res;
    res.push_back(curve[curve.size() - 1]);
    for (int32_t i = curve.size() - 1; i >=0;) {
        uint32_t minSeenVal = curve[i].Level;
        uint32_t maxSeenVal = curve[i].Level;

        int32_t j = i;
        for (;;) {
            minSeenVal = std::min(curve[j].Level, minSeenVal);
            maxSeenVal = std::max(curve[j].Level, maxSeenVal);

            uint32_t curVal = curve[j].Level;
/*
            std::cout << "i: " << i
                << " j: " << j
                << " minSeenVal: " << minSeenVal
                << " maxSeenVal: " << maxSeenVal
                << " curVal: " << curVal
                << std::endl;
*/
            if ((j == 0 && (curve[0].Level != curve[1].Level)) ||
                (curVal - minSeenVal > threshold) ||
                (maxSeenVal - curVal > threshold) )
            {
                res.push_back(curve[j]);
                break;
            }
            if (j == 0)
                break;
            j--;
        }
        i = j;
        if (i == 0)
            break;
    }
    std::reverse(res.begin(), res.end());

//    for (auto v : res)
//        std::cout << "out: " << v.Level << " " << v.Location << std::endl;

    if (res.size() < TAtrac3Data::SubbandInfo::MaxGainPointsNum) {
        return res;
    }
    return FilterCurve(res, threshold + 1);
}

//TODO: implement real transient detector
bool checkTransient(TFloat cur, TFloat prev)
{
    TFloat x = (cur > prev) ? cur / prev : prev / cur;
    if (x > 6)
        return true;

    return false;
}

std::vector<TFloat> TAtrac3Processor::CalcBaseLevel(const TFloat prev, const std::vector<TFloat>& gain) {

    TFloat maxRel = 1.0;
    bool done = false;
    //TODO: recheck it. It looks like we realy need to compare only prev and last point
    for (int i = gain.size() - 1; i < gain.size(); ++i) {
        if (prev > gain[i] && prev / gain[i] > maxRel) {
            maxRel = prev / gain[i];
            done = true;
        }
    }

    TFloat val0 = gain[gain.size() - 1];
    if (done) {
        const TFloat rel = LimitRel(maxRel);
        uint32_t relIdx = 15 - Log2FloatToIdx(rel, 2048);
        val0 = prev / GainLevel[relIdx];
    }

    TFloat val1 = gain[gain.size() - 1];
    std::vector<TFloat> baseLine(gain.size());

    baseLine[0] = val0;
    baseLine[baseLine.size() - 1] = val1;
    TFloat a = (baseLine[baseLine.size() - 1] - baseLine[0]) / baseLine.size();

    for (int i = 1; i < baseLine.size() - 1; i++) {
        baseLine[i] = i * a + baseLine[0];
    }
    return baseLine;
}

TAtrac3Data::SubbandInfo TAtrac3Processor::CreateSubbandInfo(TFloat* in[4],
                                                             uint32_t channel,
                                                             TTransientDetector* transientDetector)
{
    TAtrac3Data::SubbandInfo siCur;
    for (int band = 0; band < 4; ++band) {

        const TFloat* srcBuff = in[band];
        TFloat* const lastLevel = &LastLevels[channel][band];
        TFloat* const lastHPLevel = &LastHPLevels[channel][band];
        TFloat* const lastMax = &PrevPeak[channel][band];

        std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve;
        //RMS gain
        std::vector<TFloat> gain = AnalyzeGain(srcBuff, 256, 32, true);
        //std::cout << "gain prev: " << *lastLevel << std::endl;
        //for ( auto vvv : gain ) {
        //    std::cout << " gain: " << vvv << std::endl;
        //}
        int32_t gainPos = gain.size() - 2;
        bool hasTransient = false;

        std::vector<TFloat> base = CalcBaseLevel(*lastLevel, gain);

        TFloat hpSig[256];
        hpFilter(srcBuff, &hpSig[0], 256);
        //Peak gain
        std::vector<TFloat> hpGain = AnalyzeGain(&hpSig[0], 256, 32, false);

        for (; gainPos >= 0; --gainPos) {
            const TFloat val = (gainPos == 0) ? *lastLevel : gain[gainPos];

            const TFloat hpval = (gainPos == 0) ? *lastHPLevel : hpGain[gainPos];
            if (!hasTransient && checkTransient(hpval, hpGain[gainPos + 1])) {
                //std::cout << "hasTransient true at: " << gainPos << " base: " << base[gainPos] << std::endl;
                hasTransient = true;
            }

            const TFloat rel = LimitRel(val / base[gainPos]);
            uint32_t scaleIdx = 15 - Log2FloatToIdx(rel, 2048);

            curve.push_back({scaleIdx, (uint32_t)gainPos /*+ !!gainPos*/});
        }


        *lastLevel = gain[gain.size() -1];
        *lastHPLevel = hpGain[gain.size() -1];
        if (hasTransient) {
            std::reverse(curve.begin(), curve.end());
            auto t = CheckLevelOverflow(*lastMax, curve[0].Level);
            //std::cout << "overflow: " << curve[0].Level << " new: " << t << " max: " << *lastMax << std::endl;
            curve[0].Level = t;
            siCur.AddSubbandCurve(band, std::move(FilterCurve(curve, 0)));
        }

    }
    return siCur;
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

            TFloat* maxOverlapLevels = PrevPeak[channel];

            Mdct(specs.data(), p, maxOverlapLevels, MakeGainModulatorArray(siCur));
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

TPCMEngine<TFloat>::TProcessLambda TAtrac3Processor::GetDecodeLambda()
{
    abort();
    return {};
}

}//namespace NAtracDEnc
