#pragma once
#include <string.h>

template<class TPCM, int nIn>
class TQmf {
    static const float TapHalf[24];
    double QmfWindow[48];
    TPCM PcmBuffer[nIn + 46];
    double PcmBufferMerge[nIn + 46];
    double DelayBuff[46];
public:
    TQmf() {
        const int sz = sizeof(QmfWindow)/sizeof(QmfWindow[0]);

        for (int i = 0 ; i < sz/2; i++) {
            QmfWindow[i] = QmfWindow[ sz - 1 - i] = TapHalf[i] * 2.0;
        }
        for (int i = 0; i < sizeof(PcmBuffer)/sizeof(PcmBuffer[0]); i++) {
            PcmBuffer[i] = 0;
            PcmBufferMerge[i] = 0;
        }
    }

    void Split(TPCM* in, double* lower, double* upper) {
        double temp;
        static double last;
        for (int i = 0; i < 46; i++)
            PcmBuffer[i] = PcmBuffer[nIn + i];

        for (int i = 0; i < nIn; i++)
            PcmBuffer[46+i] = in[i];

        for (int j = 0; j<nIn; j+=2) {
            lower[j/2] = upper[j/2] = 0.0;
            for (int i = 0; i < 24; i++)  {
                lower[j/2] += QmfWindow[2*i] * PcmBuffer[48-1+j-(2*i)];
                upper[j/2] += QmfWindow[(2*i)+1] * PcmBuffer[48-1+j-(2*i)-1];
            }
            temp = upper[j/2];
            upper[j/2] = lower[j/2] - upper[j/2];
            lower[j/2] += temp;
        }
    }

    void Merge(TPCM* out, double* lower, double* upper) {
        memcpy(&PcmBufferMerge[0], &DelayBuff[0], 46*sizeof(double));
        double* newPart = &PcmBufferMerge[46];
        for (int i = 0; i < nIn; i+=4) {
            newPart[i+0] = lower[i/2] + upper[i/2];
            newPart[i+1] = lower[i/2] - upper[i/2];
            newPart[i+2] = lower[i/2 + 1] + upper[i/2 + 1];
            newPart[i+3] = lower[i/2 + 1] - upper[i/2 + 1];
        }

        double* winP = &PcmBufferMerge[0];
        for (int j = nIn/2; j != 0; j--) {
            double s1 = 0;
            double s2 = 0;
            for (int i = 0; i < 48; i+=2) {
                s1 += winP[i] * QmfWindow[i];
                s2 += winP[i+1] * QmfWindow[i+1];
            }
            out[0] = s2;
            out[1] = s1;
            winP += 2;
            out += 2;
        }
        memcpy(&DelayBuff[0], &PcmBufferMerge[nIn], 46*sizeof(double));
    }
};

template<class TPCM, int nIn>
const float TQmf<TPCM, nIn>::TapHalf[24] = {
    -0.00001461907,  -0.00009205479, -0.000056157569,  0.00030117269,
    0.0002422519,    -0.00085293897, -0.0005205574,    0.0020340169,
    0.00078333891,   -0.0042153862,  -0.00075614988,   0.0078402944,
    -0.000061169922, -0.01344162,    0.0024626821,     0.021736089,
    -0.007801671,    -0.034090221,   0.01880949,       0.054326009,
    -0.043596379,    -0.099384367,   0.13207909,       0.46424159
};

