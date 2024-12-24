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

#define ATRAC_UT_PUBLIC

#include "atrac3denc.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>
using std::vector;
using namespace NAtracDEnc;
using namespace NAtrac3;

static void GenerateSignal(float* buf, size_t n, float f, float a) {
    for (size_t i = 0; i < n; ++i) {
        buf[i] = a * sin((M_PI/2) * i * f);
    }
}

static void GenerateSignalWithTransient(float* buf, size_t n, float f, float a,
        size_t transientPos, size_t transientLen, float transientLev) {
    assert(transientPos + transientLen < n);
    GenerateSignal(buf, n, f, a);
    GenerateSignal(buf+transientPos, transientLen, f, transientLev);
//    for (size_t i = transientPos; i < transientPos + transientLen; ++i) {
//        buf[i] += (i & 1) ? transientLev : - transientLev;
//    }
}

class TWindowTest {
public:
    void RunTest() {
        for (size_t i = 0; i < 256; i++) {
            const float ha1 = TAtrac3Data::EncodeWindow[i] / 2.0; //compensation
            const double hs1 = TAtrac3Data::DecodeWindow[i];
            const double hs2 = TAtrac3Data::DecodeWindow[255-i];
            const float res = hs1 / (hs1 * hs1 + hs2 * hs2);
            EXPECT_NEAR(ha1, res, 1.0 / (1 << 24));
        }
    }
};

template<class T>
class TAtrac3MDCTWorkBuff {
    T* Buffer;
public:
    static const size_t BandBuffSz = 256;
    static const size_t BandBuffAndOverlapSz = BandBuffSz * 2;
    static const size_t BuffSz = BandBuffAndOverlapSz * (4 + 4); 
    T* const Band0;
    T* const Band1;
    T* const Band2;
    T* const Band3;
    T* const Band0Res;
    T* const Band1Res;
    T* const Band2Res;
    T* const Band3Res;
    TAtrac3MDCTWorkBuff()
        : Buffer(new T[BuffSz])
        , Band0(Buffer)
        , Band1(Band0 + BandBuffAndOverlapSz)
        , Band2(Band1 + BandBuffAndOverlapSz)
        , Band3(Band2 + BandBuffAndOverlapSz)
        , Band0Res(Band3 + BandBuffAndOverlapSz)
        , Band1Res(Band0Res + BandBuffAndOverlapSz)
        , Band2Res(Band1Res + BandBuffAndOverlapSz)
        , Band3Res(Band2Res + BandBuffAndOverlapSz)
    {
        memset(Buffer, 0, sizeof(T)*BuffSz);
    }
    ~TAtrac3MDCTWorkBuff()
    {
        delete[] Buffer;
    }
};


TEST(TAtrac3MDCT, TAtrac3MDCTZeroOneBlock) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<float> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<float>::BandBuffSz;

    vector<float> specs(1024);

    float* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };

    mdct.Mdct(specs.data(), p);
    for(auto s: specs)
        EXPECT_NEAR(s, 0.0, 0.0000000001);

    float* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };
    mdct.Midct(specs.data(), p);

    for(size_t i = 0; i < workSz; ++i)
        EXPECT_NEAR(buff.Band0Res[i], 0.0, 0.0000000001);

   for(size_t i = 0; i < workSz; ++i)
        EXPECT_NEAR(buff.Band1Res[i], 0.0, 0.0000000001);

   for(size_t i = 0; i < workSz; ++i)
        EXPECT_NEAR(buff.Band2Res[i], 0.0, 0.0000000001);

   for(size_t i = 0; i < workSz; ++i)
        EXPECT_NEAR(buff.Band3Res[i], 0.0, 0.0000000001);


}
/*
TEST(TAtrac3MDCT, TAtrac3MDCTSignal) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<float> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<float>::BandBuffSz;

    const size_t len = 1024;
    vector<float> signal(len);
    vector<float> signalRes(len);
    GenerateSignal(signal.data(), signal.size(), 0.25, 32768);
    
    for (size_t pos = 0; pos < len; pos += workSz) {
        vector<float> specs(1024);
        memcpy(buff.Band0 + workSz, signal.data() + pos, workSz * sizeof(float));

        float* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };
        mdct.Mdct(specs.data(), p);

        float* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };
        mdct.Midct(specs.data(), t);

        memcpy(signalRes.data() + pos, buff.Band0Res, workSz * sizeof(float));
    }

    for (int i = workSz; i < len; ++i)
        EXPECT_NEAR(signal[i - workSz], signalRes[i], 0.00000001);
}

TEST(TAtrac3MDCT, TAtrac3MDCTSignalWithGainCompensation) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<float> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<float>::BandBuffSz;

    const size_t len = 4096;
    vector<float> signal(len, 8000);
    vector<float> signalRes(len);
    GenerateSignal(signal.data() + 1024, signal.size()-1024, 0.25, 32768);
    
    for (size_t pos = 0; pos < len; pos += workSz) {
        vector<float> specs(1024);
        memcpy(buff.Band0 + workSz, signal.data() + pos, workSz * sizeof(float));

        float* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };

        if (pos == 256) { //apply gain modulation
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{3, 2}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 1024) {
            TAtrac3Data::SubbandInfo siCur;
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{3, 2}, {2, 5}};
            siCur.AddSubbandCurve(0, std::move(curve));

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 1024 + 256) {
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{1, 0}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 2048) {
            TAtrac3Data::SubbandInfo siCur;
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{4, 2}, {1, 5}};
            siCur.AddSubbandCurve(0, std::move(curve));

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else {
            mdct.Mdct(specs.data(), p);
        }

        float* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };

        if (pos == 256) { //restore gain modulation
            TAtrac3Data::SubbandInfo siCur;
            TAtrac3Data::SubbandInfo siNext;
            siNext.AddSubbandCurve(0, {{3, 2}});
 
            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 512) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{3, 2}});
 
            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 1024) {
            TAtrac3Data::SubbandInfo siCur;
            TAtrac3Data::SubbandInfo siNext;
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{3, 2}, {2, 5}};
            siNext.AddSubbandCurve(0, std::move(curve));

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 1024 + 256) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siNext.AddSubbandCurve(0, {{1, 0}});
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{3, 2}, {2, 5}};
            siCur.AddSubbandCurve(0, std::move(curve));

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 1024 + 256 + 256) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{1, 0}});

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 2048) {
            TAtrac3Data::SubbandInfo siCur;
            TAtrac3Data::SubbandInfo siNext;
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{4, 2}, {1, 5}};
            siNext.AddSubbandCurve(0, std::move(curve));

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 2048 + 256) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve = {{4, 2}, {1, 5}};
            siCur.AddSubbandCurve(0, std::move(curve));

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else {
            mdct.Midct(specs.data(), t);
        }
        memcpy(signalRes.data() + pos, buff.Band0Res,  workSz * sizeof(float));
    }
    for (int i = workSz; i < len; ++i) {
        //std::cout << "res: " << i << " " << signalRes[i] << std::endl;
        EXPECT_NEAR(signal[i - workSz], signalRes[i], 0.00000001);
    }
}

TEST(TAtrac3MDCT, TAtrac3MDCTSignalWithGainCompensationAndManualTransient) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<float> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<float>::BandBuffSz;

    const size_t len = 1024;
    vector<float> signal(len);
    vector<float> signalRes(len);
    GenerateSignalWithTransient(signal.data(), signal.size(), 0.03125, 512.0,
                    640, 64, 32768.0);
    const std::vector<TAtrac3Data::SubbandInfo::TGainPoint> curve1 = {{6, 13}, {4, 14}};
 
    for (size_t pos = 0; pos < len; pos += workSz) {
        vector<float> specs(1024);
        memcpy(buff.Band0 + workSz, signal.data() + pos, workSz * sizeof(float));

        float* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };
        //for (int i = 0; i < 256; i++) {
        //    std::cout << i + pos << " " << buff.Band0[i] << std::endl;
        //}

        if (pos == 512) { //apply gain modulation
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, std::vector<TAtrac3Data::SubbandInfo::TGainPoint>(curve1));

            for (int i = 0; i < 256; i++) {
                std::cout << i << " " << buff.Band0[i] << std::endl;
            }

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else { 
            mdct.Mdct(specs.data(), p);
        }

        for (int i = 0; i < specs.size(); ++i) {
            if (i > 240 && i < 256)
                specs[i] /= 1.9;
        }
        float* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };
        if (pos == 512) { //restore gain modulation
            TAtrac3Data::SubbandInfo siCur;
            TAtrac3Data::SubbandInfo siNext;
            siNext.AddSubbandCurve(0, std::vector<TAtrac3Data::SubbandInfo::TGainPoint>(curve1));
            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 768) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, std::vector<TAtrac3Data::SubbandInfo::TGainPoint>(curve1));
 
            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else { 
            mdct.Midct(specs.data(), t);
        }

        memcpy(signalRes.data() + pos, buff.Band0Res,  workSz * sizeof(float));
    }
    for (int i = workSz; i < len; ++i) {
        //std::cout << "res: " << i << " " << signalRes[i] << std::endl;
        EXPECT_NEAR(signal[i - workSz], signalRes[i], 10);
    }
}
*/
TEST(AtracGainControl, RelToIdxTest) {

    EXPECT_EQ(4, RelationToIdx(1));
    EXPECT_EQ(4, RelationToIdx(1.8));
    EXPECT_EQ(3, RelationToIdx(2));
    EXPECT_EQ(3, RelationToIdx(3));
    EXPECT_EQ(3, RelationToIdx(3.5));
    EXPECT_EQ(2, RelationToIdx(4));
    EXPECT_EQ(2, RelationToIdx(7));
    EXPECT_EQ(1, RelationToIdx(8));
    EXPECT_EQ(1, RelationToIdx(15));
    EXPECT_EQ(0, RelationToIdx(16));
    EXPECT_EQ(0, RelationToIdx(9999));

    EXPECT_EQ(4, RelationToIdx(0.8));
    EXPECT_EQ(5, RelationToIdx(0.5));
    EXPECT_EQ(5, RelationToIdx(0.4));
    EXPECT_EQ(5, RelationToIdx(0.3));
    EXPECT_EQ(6, RelationToIdx(0.25));
    EXPECT_EQ(6, RelationToIdx(0.126));
    EXPECT_EQ(7, RelationToIdx(0.125));
    EXPECT_EQ(7, RelationToIdx(0.0626));
    EXPECT_EQ(8, RelationToIdx(0.0625));
    EXPECT_EQ(8, RelationToIdx(0.03126));
    EXPECT_EQ(9, RelationToIdx(0.03125));
    EXPECT_EQ(9, RelationToIdx(0.015626));
    EXPECT_EQ(10, RelationToIdx(0.015625));
    EXPECT_EQ(13, RelationToIdx(0.001953125));
    EXPECT_EQ(15, RelationToIdx(0.00048828125));
    EXPECT_EQ(15, RelationToIdx(0.00000048828125));
}

TEST(TAtrac3MDCT, TAtrac3MDCTWindow) {
    TWindowTest test;
    test.RunTest();
}


