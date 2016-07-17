#include "atrac_psy_common.h"

namespace NAtracDEnc {

//returns 1 for tone-like, 0 - noise-like
TFloat AnalizeScaleFactorSpread(const std::vector<TScaledBlock>& scaledBlocks) {
    TFloat s = 0.0;
    for (size_t i = 0; i < scaledBlocks.size(); ++i) {
        s += scaledBlocks[i].ScaleFactorIndex;
    }
    s /= scaledBlocks.size();
    TFloat sigma = 0.0;
    TFloat t = 0.0;
    for (size_t i = 0; i < scaledBlocks.size(); ++i) {
        t = (scaledBlocks[i].ScaleFactorIndex - s);
        t *= t;
        sigma += t;
    }
    sigma /= scaledBlocks.size();
    sigma = sqrt(sigma);
    if (sigma > 14.0)
        sigma = 14.0;
    return sigma/14.0;
}

} //namespace NAtracDEnc
