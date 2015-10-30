#pragma once

#include "../qmf/qmf.h"

template<class TIn>
class Atrac1SplitFilterBank {
    const static int nInSamples = 512;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    std::vector<double> MidLowTmp;
    std::vector<double> DelayBuf;
public:
    Atrac1SplitFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(23 + 512);
    }
    void Split(TIn* pcm, double* low, double* mid, double* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(double) *  23);
        Qmf1.Split(pcm, &MidLowTmp[0], &DelayBuf[23]);
        Qmf2.Split(&MidLowTmp[0], low, mid);
        memcpy(hi, &DelayBuf[0], sizeof(double) * 256);

    }
};
template<class TOut>
class Atrac1SynthesisFilterBank {
    const static int nInSamples = 512;
    TQmf<TOut, nInSamples> Qmf1;
    TQmf<TOut, nInSamples / 2> Qmf2;
    std::vector<double> MidLowTmp;
    std::vector<double> DelayBuf;
public:
    Atrac1SynthesisFilterBank() {
        MidLowTmp.resize(512);
        DelayBuf.resize(23 + 512);
    }
    void Synthesis(TOut* pcm, double* low, double* mid, double* hi) {
        memcpy(&DelayBuf[0], &DelayBuf[256], sizeof(double) *  23);
        memcpy(&DelayBuf[23], hi, sizeof(double) * 256);
        Qmf2.Merge(&MidLowTmp[0], &low[0], &mid[0]);
        Qmf1.Merge(&pcm[0], &MidLowTmp[0], &DelayBuf[0]);
    }
};


