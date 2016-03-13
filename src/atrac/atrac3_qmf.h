#pragma once
#include <vector>
#include "../qmf/qmf.h"

template<class TIn>
class Atrac3SplitFilterBank {
    const static int nInSamples = 1024;
    TQmf<TIn, nInSamples> Qmf1;
    TQmf<TIn, nInSamples / 2> Qmf2;
    TQmf<TIn, nInSamples / 2> Qmf3;
    std::vector<double> Buf1;
    std::vector<double> Buf2;
public:
    Atrac3SplitFilterBank() {
        Buf1.resize(nInSamples);
        Buf2.resize(nInSamples);
    }
    void Split(TIn* pcm, double* subs[4]) {
        Qmf1.Split(pcm, Buf1.data(), Buf2.data());
        Qmf2.Split(Buf1.data(), subs[0], subs[1]);
        Qmf3.Split(Buf2.data(), subs[3], subs[2]);
    }
};
