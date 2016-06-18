#pragma once

#include "../qmf/qmf.h"

namespace NAtracDEnc {

template<class TIn>
class Atrac1SplitFilterBank {
    const static int nInSamples = 512;
    const static int delayComp = 39;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    std::vector<TFloat> MidLowTmp;
    std::vector<TFloat> DelayBuf;
public:
    Atrac1SplitFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(delayComp + 512);
    }
    void Split(TIn* pcm, TFloat* low, TFloat* mid, TFloat* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(TFloat) *  delayComp);
        Qmf1.Split(pcm, &MidLowTmp[0], &DelayBuf[delayComp]);
        Qmf2.Split(&MidLowTmp[0], low, mid);
        memcpy(hi, &DelayBuf[0], sizeof(TFloat) * 256);

    }
};
template<class TOut>
class Atrac1SynthesisFilterBank {
    const static int nInSamples = 512;
    const static int delayComp = 39;
    TQmf<TOut, nInSamples> Qmf1;
    TQmf<TOut, nInSamples / 2> Qmf2;
    std::vector<TFloat> MidLowTmp;
    std::vector<TFloat> DelayBuf;
public:
    Atrac1SynthesisFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(delayComp + 512);
    }
    void Synthesis(TOut* pcm, TFloat* low, TFloat* mid, TFloat* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(TFloat) *  delayComp);
        memcpy(&DelayBuf[delayComp], hi, sizeof(TFloat) * 256);
        Qmf2.Merge(&MidLowTmp[0], &low[0], &mid[0]);
        Qmf1.Merge(&pcm[0], &MidLowTmp[0], &DelayBuf[0]);
    }
};

} //namespace NAtracDEnc
