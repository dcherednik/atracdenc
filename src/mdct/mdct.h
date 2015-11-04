#pragma once

#include "vorbis_impl/mdct.h"
#include <vector>

namespace NMDCT {

class TMDCTBase {
protected:
    MDCTContext Ctx;
    TMDCTBase(int n, double scale) {
        mdct_ctx_init(&Ctx, n, scale);
    };
    virtual ~TMDCTBase() {
        mdct_ctx_close(&Ctx);
    };
};


template<int N>
class TMDCT : public TMDCTBase {
    std::vector<double> Buf;
public:
    TMDCT(float scale = 1.0)
        : Buf(N/2)
        , TMDCTBase(N, scale)
    {}
    const std::vector<double>& operator()(double* in) {
        mdct(&Ctx, &Buf[0], in);
        return Buf;
    }

};
} //namespace NMDCT
