#include <functional>

#include "config.h"

template<class T>
class TGainProcessor : public T {

public:
    typedef std::function<void(TFloat* out, TFloat* cur, TFloat* prev)> TGainDemodulator;
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
    typedef std::function<void(TFloat* bufCur, TFloat* bufNext)> TGainModulator;
    static TFloat GetGainInc(uint32_t levelIdxCur)
    {
        const int incPos = T::ExponentOffset - levelIdxCur + T::GainInterpolationPosShift;
        return T::GainInterpolation[incPos];
    }
    static TFloat GetGainInc(uint32_t levelIdxCur, uint32_t levelIdxNext)
    {
        const int incPos = levelIdxNext - levelIdxCur + T::GainInterpolationPosShift;
        return T::GainInterpolation[incPos];
    }


    TGainDemodulator Demodulate(const std::vector<typename T::SubbandInfo::TGainPoint>& giNow,
                                const std::vector<typename T::SubbandInfo::TGainPoint>& giNext)
    {
        return [=](TFloat* out, TFloat* cur, TFloat* prev) {
            uint32_t pos = 0;
            const TFloat scale = giNext.size() ? T::GainLevel[giNext[0].Level] : 1;
            for (uint32_t i = 0; i < giNow.size(); ++i) {
                uint32_t lastPos = giNow[i].Location << T::LocScale;
                const uint32_t levelPos = giNow[i].Level;
                assert(levelPos < sizeof(T::GainLevel)/sizeof(T::GainLevel[0]));
                TFloat level = T::GainLevel[levelPos];
                const int incPos = ((i + 1) < giNow.size() ? giNow[i + 1].Level : T::ExponentOffset)
                                   - giNow[i].Level + T::GainInterpolationPosShift;
                TFloat gainInc = T::GainInterpolation[incPos];
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
        return [=](TFloat* bufCur, TFloat* bufNext) {
            uint32_t pos = 0;
            const TFloat scale = T::GainLevel[giCur[0].Level];
            for (uint32_t i = 0; i < giCur.size(); ++i) {
                uint32_t lastPos = giCur[i].Location << T::LocScale;
                const uint32_t levelPos = giCur[i].Level;
                assert(levelPos < sizeof(T::GainLevel)/sizeof(T::GainLevel[0]));
                TFloat level = T::GainLevel[levelPos];
                const int incPos = ((i + 1) < giCur.size() ? giCur[i + 1].Level : T::ExponentOffset)
                                   - giCur[i].Level + T::GainInterpolationPosShift;
                TFloat gainInc = T::GainInterpolation[incPos];
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

                //std::cout << "mod pos: " << pos << " scale: " << scale << " bufCur: " << bufCur[pos] << std::endl;
                bufCur[pos] /= scale;
            }
        };
    }
};
