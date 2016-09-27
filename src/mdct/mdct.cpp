#include "mdct.h"
#include <iostream>

namespace NMDCT {

static std::vector<TFloat> CalcSinCos(size_t n, TFloat scale)
{
    std::vector<TFloat> tmp(n >> 1);
    const TFloat alpha = 2.0 * M_PI / (8.0 * n);
    const TFloat omiga = 2.0 * M_PI / n;
    scale = sqrt(scale/n); 
    for (size_t i = 0; i < (n >> 2); ++i) {
        tmp[2 * i + 0] = scale * cos(omiga * i + alpha);
        tmp[2 * i + 1] = scale * sin(omiga * i + alpha);
    }
    return tmp;
}

TMDCTBase::TMDCTBase(size_t n, TFloat scale)
    : N(n)
    , SinCos(CalcSinCos(n, scale))
{
    FFTIn = (kiss_fft_cpx*) malloc(sizeof(kiss_fft_cpx) * N >> 2);
    FFTOut = (kiss_fft_cpx*) malloc(sizeof(kiss_fft_cpx) * N >> 2);
    FFTPlan = kiss_fft_alloc(N >> 2, false, nullptr, nullptr);
}

TMDCTBase::~TMDCTBase()
{
    kiss_fft_free(FFTPlan);
    free(FFTOut);
    free(FFTIn);
}

} // namespace NMDCT
