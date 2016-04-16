#include "atrac3denc.h"
#include <gtest/gtest.h>

#include <vector>
#include <cmath>
using std::vector;
using namespace NAtracDEnc;

static void GenerateSignal(double* buf, size_t n, double f, double a) {
    for (size_t i = 0; i < n; ++i) {
        buf[i] = a * sin((M_PI/2) * i * f);
    }
}

class TWindowTest : public TAtrac3Data {
public:
    void RunTest() {
        for (size_t i = 0; i < 256; i++) {
            const double ha1 = EncodeWindow[i] / 2.0; //compensation
            const double hs1 = DecodeWindow[i];
            const double hs2 = DecodeWindow[255-i];
            const double res = hs1 / (hs1 * hs1 + hs2 * hs2);
            EXPECT_NEAR(ha1, res, 0.000000001);
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
    TAtrac3MDCTWorkBuff<double> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<double>::BandBuffSz;

    vector<double> specs(1024);

    double* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };

    mdct.Mdct(specs.data(), p);
    for(auto s: specs)
        EXPECT_NEAR(s, 0.0, 0.0000000001);

    double* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };
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

TEST(TAtrac3MDCT, TAtrac3MDCTSignal) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<double> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<double>::BandBuffSz;

    const size_t len = 1024;
    vector<double> signal(len);
    vector<double> signalRes(len);
    GenerateSignal(signal.data(), signal.size(), 0.25, 32768);
    
    for (size_t pos = 0; pos < len; pos += workSz) {
        vector<double> specs(1024);
        memcpy(buff.Band0, signal.data() + pos, workSz * sizeof(double));

        double* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };
        mdct.Mdct(specs.data(), p);

        double* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };
        mdct.Midct(specs.data(), t);

        memcpy(signalRes.data() + pos, buff.Band0Res, workSz * sizeof(double));
    }

    for (int i = workSz; i < len; ++i)
        EXPECT_NEAR(signal[i - workSz], signalRes[i], 0.00000001);
}

TEST(TAtrac3MDCT, TAtrac3MDCTSignalWithGainCompensation) {
    TAtrac3MDCT mdct;
    TAtrac3MDCTWorkBuff<double> buff;
    size_t workSz = TAtrac3MDCTWorkBuff<double>::BandBuffSz;

    const size_t len = 4096;
    vector<double> signal(len, 8000);
    vector<double> signalRes(len);
    GenerateSignal(signal.data() + 1024, signal.size()-1024, 0.25, 32768);
    
    for (size_t pos = 0; pos < len; pos += workSz) {
        vector<double> specs(1024);
        memcpy(buff.Band0, signal.data() + pos, workSz * sizeof(double));

        double* p[4] = { buff.Band0, buff.Band1, buff.Band2, buff.Band3 };

        if (pos == 256) { //apply gain modulation
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{3, 2}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 1024) {
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{3, 2}});
            siCur.AddSubbandCurve(0, {{2, 5}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 1024 + 256) {
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{1, 0}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else if (pos == 2048) {
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{4, 2}});
            siCur.AddSubbandCurve(0, {{1, 5}});

            mdct.Mdct(specs.data(), p, { mdct.GainProcessor.Modulate(siCur.GetGainPoints(0)),
                    TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator(), TAtrac3MDCT::TGainModulator()});
        } else {
            mdct.Mdct(specs.data(), p);
        }

        double* t[4] = { buff.Band0Res, buff.Band1Res, buff.Band2Res, buff.Band3Res };

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
            siNext.AddSubbandCurve(0, {{3, 2}});
            siNext.AddSubbandCurve(0, {{2, 5}});

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 1024 + 256) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siNext.AddSubbandCurve(0, {{1, 0}});
            siCur.AddSubbandCurve(0, {{3, 2}});
            siCur.AddSubbandCurve(0, {{2, 5}});

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
            siNext.AddSubbandCurve(0, {{4, 2}});
            siNext.AddSubbandCurve(0, {{1, 5}});

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else if (pos == 2048 + 256) {
            TAtrac3Data::SubbandInfo siNext;
            TAtrac3Data::SubbandInfo siCur;
            siCur.AddSubbandCurve(0, {{4, 2}});
            siCur.AddSubbandCurve(0, {{1, 5}});

            mdct.Midct(specs.data(), t, {mdct.GainProcessor.Demodulate(siCur.GetGainPoints(0), siNext.GetGainPoints(0)),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator(),
                    TAtrac3MDCT::TAtrac3GainProcessor::TGainDemodulator()});
        } else {
            mdct.Midct(specs.data(), t);
        }
        memcpy(signalRes.data() + pos, buff.Band0Res,  workSz * sizeof(double));
    }
    for (int i = workSz; i < len; ++i) {
        //std::cout << "res: " << i << " " << signalRes[i] << std::endl;
        EXPECT_NEAR(signal[i - workSz], signalRes[i], 0.00000001);
    }
}



TEST(TAtrac3MDCT, TAtrac3MDCTWindow) {
    TWindowTest test;
    test.RunTest();
}
