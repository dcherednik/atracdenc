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

#include <functional>

#include "config.h"

template<class T>
class TGainProcessor : public T {

public:
    typedef std::function<void(float* out, float* cur, float* prev)> TGainDemodulator;
    /*
     * example GainModulation:
     * PCMinput:
     *     N   b    N        N
     * --------|--------|--------|--------
     * |       | - mdct #1
     *     |        | - mdct #2
     *     a
     *         |        | - mdct #3
     *         ^^^^^ - modulated by previous step
     * lets consider a case we want to modulate mdct #2.
     *     bufCur - is a buffer of first half of mdct transformation (a)
     *     bufNext - is a buffer of second half of mdct transformation and overlaping
     *               (i.e the input buffer started at b point)
     * so next transformation (mdct #3) gets modulated first part
     */
    typedef std::function<void(float* bufCur, float* bufNext)> TGainModulator;
    static float GetGainInc(uint32_t levelIdxCur)
    {
        const int incPos = T::ExponentOffset - levelIdxCur + T::GainInterpolationPosShift;
        return T::GainInterpolation[incPos];
    }
    static float GetGainInc(uint32_t levelIdxCur, uint32_t levelIdxNext)
    {
        const int incPos = levelIdxNext - levelIdxCur + T::GainInterpolationPosShift;
        return T::GainInterpolation[incPos];
    }


    TGainDemodulator Demodulate(const std::vector<typename T::SubbandInfo::TGainPoint>& giNow,
                                const std::vector<typename T::SubbandInfo::TGainPoint>& giNext)
    {
        return [=](float* out, float* cur, float* prev) {
            uint32_t pos = 0;
            const float scale = giNext.size() ? T::GainLevel[giNext[0].Level] : 1;
            for (uint32_t i = 0; i < giNow.size(); ++i) {
                uint32_t lastPos = giNow[i].Location << T::LocScale;
                const uint32_t levelPos = giNow[i].Level;
                assert(levelPos < sizeof(T::GainLevel)/sizeof(T::GainLevel[0]));
                float level = T::GainLevel[levelPos];
                const int incPos = ((i + 1) < giNow.size() ? giNow[i + 1].Level : T::ExponentOffset)
                                   - giNow[i].Level + T::GainInterpolationPosShift;
                float gainInc = T::GainInterpolation[incPos];
                for (; pos < lastPos; pos++) {
                    //std::cout << "pos: " << pos << " scale: " << scale << " level: " << level << std::endl;
                    out[pos] = (cur[pos] * scale + prev[pos]) * level;
                }
                for (; pos < lastPos + T::LocSz; pos++) {
                    //std::cout << "pos: " << pos << " scale: " << scale << " level: " << level << " gainInc: " << gainInc << std::endl;
                    out[pos] = (cur[pos] * scale + prev[pos]) * level;
                    level *= gainInc;
                }
            }
            for (; pos < T::MDCTSz/2; pos++) {
                //std::cout << "pos: " << pos << " scale: " << scale << std::endl;
                out[pos] = cur[pos] * scale + prev[pos];
            }
        };
    }
    TGainModulator Modulate(const std::vector<typename T::SubbandInfo::TGainPoint>& giCur) {
        if (giCur.empty())
            return {};
        return [=](float* bufCur, float* bufNext) {
            uint32_t pos = 0;
            const float scale = T::GainLevel[giCur[0].Level];
            for (uint32_t i = 0; i < giCur.size(); ++i) {
                uint32_t lastPos = giCur[i].Location << T::LocScale;
                const uint32_t levelPos = giCur[i].Level;
                assert(levelPos < sizeof(T::GainLevel)/sizeof(T::GainLevel[0]));
                float level = T::GainLevel[levelPos];
                const int incPos = ((i + 1) < giCur.size() ? giCur[i + 1].Level : T::ExponentOffset)
                                   - giCur[i].Level + T::GainInterpolationPosShift;
                float gainInc = T::GainInterpolation[incPos];
                for (; pos < lastPos; pos++) {
                    //std::cout << "mod pos: " << pos << " scale: " << scale << " bufCur: " <<  bufCur[pos]  << " level: " << level << " bufNext: " << bufNext[pos] << std::endl;
                    bufCur[pos] /= scale;
                    bufNext[pos] /= level;
                }
                for (; pos < lastPos + T::LocSz; pos++) {

                    //std::cout << "mod pos: " << pos << " scale: " << scale << " bufCur: " <<  bufCur[pos]  << " level: " << level << " (gainInc) " << gainInc << " bufNext: " << bufNext[pos] << std::endl;
                    bufCur[pos] /= scale;
                    bufNext[pos] /= level;
                    //std::cout << "mod pos: " << pos << " scale: " << scale << " level: " << level << " gainInc: " << gainInc << std::endl;
                    level *= gainInc;
                }
            }
            for (; pos < T::MDCTSz/2; pos++) {

                //std::cout << "mod pos: " << pos << " scale: " << scale << " bufCur: " << bufCur[pos] << " new value: " << bufCur[pos] / scale<<std::endl;
                bufCur[pos] /= scale;
            }
        };
    }
};
