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
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "atrac_psy_common.h"

#include <cmath>

////////////////////////////////////////////////////////////////////////////////
namespace {

/*
 * Borrowed from Musepack.
 * https://www.musepack.net/
 */
static float
ATHformula_Frank ( float freq )
{
    /*
     * one value per 100 cent = 1
     * semitone = 1/4
     * third = 1/12
     * octave = 1/40 decade
     * rest is linear interpolated, values are currently in millibel rel. 20 µPa
     */
    static short tab [] = {
        /*    10.0 */  9669, 9669, 9626, 9512,
        /*    12.6 */  9353, 9113, 8882, 8676,
        /*    15.8 */  8469, 8243, 7997, 7748,
        /*    20.0 */  7492, 7239, 7000, 6762,
        /*    25.1 */  6529, 6302, 6084, 5900,
        /*    31.6 */  5717, 5534, 5351, 5167,
        /*    39.8 */  5004, 4812, 4638, 4466,
        /*    50.1 */  4310, 4173, 4050, 3922,
        /*    63.1 */  3723, 3577, 3451, 3281,
        /*    79.4 */  3132, 3036, 2902, 2760,
        /*   100.0 */  2658, 2591, 2441, 2301,
        /*   125.9 */  2212, 2125, 2018, 1900,
        /*   158.5 */  1770, 1682, 1594, 1512,
        /*   199.5 */  1430, 1341, 1260, 1198,
        /*   251.2 */  1136, 1057,  998,  943,
        /*   316.2 */   887,  846,  744,  712,
        /*   398.1 */   693,  668,  637,  606,
        /*   501.2 */   580,  555,  529,  502,
        /*   631.0 */   475,  448,  422,  398,
        /*   794.3 */   375,  351,  327,  322,
        /*  1000.0 */   312,  301,  291,  268,
        /*  1258.9 */   246,  215,  182,  146,
        /*  1584.9 */   107,   61,   13,  -35,
        /*  1995.3 */   -96, -156, -179, -235,
        /*  2511.9 */  -295, -350, -401, -421,
        /*  3162.3 */  -446, -499, -532, -535,
        /*  3981.1 */  -513, -476, -431, -313,
        /*  5011.9 */  -179,    8,  203,  403,
        /*  6309.6 */   580,  736,  881, 1022,
        /*  7943.3 */  1154, 1251, 1348, 1421,
        /* 10000.0 */  1479, 1399, 1285, 1193,
        /* 12589.3 */  1287, 1519, 1914, 2369,
#if 0
        /* 15848.9 */  3352, 4865, 5942, 6177,
        /* 19952.6 */  6385, 6604, 6833, 7009,
        /* 25118.9 */  7066, 7127, 7191, 7260,
#else
        /* 15848.9 */  3352, 4352, 5352, 6352,
        /* 19952.6 */  7352, 8352, 9352, 9999,
        /* 25118.9 */  9999, 9999, 9999, 9999,
#endif
    };
    double    freq_log;
    unsigned  index;

    if ( freq <    10. ) freq =    10.;
    if ( freq > 29853. ) freq = 29853.;

    freq_log = 40. * log10 (0.1 * freq);   /* 4 steps per third, starting at 10 Hz */
    index    = (unsigned) freq_log;
    return 0.01 * (tab [index] * (1 + index - freq_log) + tab [index+1] * (freq_log - index));
}

} // namespace
////////////////////////////////////////////////////////////////////////////////

namespace NAtracDEnc {

using std::vector;

//returns 1 for tone-like, 0 - noise-like
float AnalizeScaleFactorSpread(const vector<TScaledBlock>& scaledBlocks)
{
    float s = 0.0;
    for (size_t i = 0; i < scaledBlocks.size(); ++i) {
        s += scaledBlocks[i].ScaleFactorIndex;
    }
    s /= scaledBlocks.size();
    float sigma = 0.0;
    float t = 0.0;
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

vector<float> CalcATH(int len, int sampleRate)
{
    vector<float> res(len);
    float mf = (float)sampleRate / 2000.0;
    for (size_t i = 0; i < res.size(); i++) {
        const float f = (float)(i+1) * mf / len;   // Frequency in kHz
        float trh = ATHformula_Frank(1.e3 * f) - 100;

	// tmp -= f * f * (int)(EarModelFlag % 100 - 50) * 0.0015;  // 00: +30 dB, 100: -30 dB  @20 kHz
        trh -= f * f * 0.015;

	res[i] = trh;
    }
    return res;
}

vector<float> CreateLoudnessCurve(size_t sz)
{
    std::vector<float> res;
    res.resize(sz);

    for (size_t i = 0; i < sz; i++) {
        float f = (float)(i + 3) * 0.5 * 44100 / (float)sz;
        float t = std::log10(f) - 3.5;
        t = -10 * t * t + 3 - f / 3000;
        t = std::pow(10, (0.1 * t));
        //std::cerr << i << "  => " << f << "  "  << t <<std::endl;
        res[i] = t;
    }

    return res;
}

} // namespace NAtracDEnc
