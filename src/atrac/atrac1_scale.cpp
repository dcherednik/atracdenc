#include "atrac1_scale.h"
#include <cmath>
#include <iostream>
#include <algorithm>
namespace NAtrac1 {
using std::vector;
using std::map;

using namespace std;
map<uint32_t, uint8_t> TScaler::ScaleIndex;
static const uint32_t MAX_SCALE = 65536;

static bool absComp(double a, double b) {
    return abs(a) < abs(b);
}

TScaler::TScaler() {
    if (ScaleIndex.empty()) {
        for (int i = 0; i < 64; i++) {
            ScaleIndex[ScaleTable[i] * 256] = i;
        }
    }
}

vector<TScaledBlock> TScaler::Scale(const vector<double>& specs) {
    vector<TScaledBlock> scaledBlocks;
    for (uint8_t bandNum = 0; bandNum < QMF_BANDS; ++bandNum) {
        for (uint8_t blockNum = BlocksPerBand[bandNum]; blockNum < BlocksPerBand[bandNum + 1]; ++blockNum) {
            const uint16_t specNumStart = SpecsStartLong[blockNum]; 
            const uint16_t specNumEnd = SpecsStartLong[blockNum] + SpecsPerBlock[blockNum];
            double maxAbsSpec = 0;
            for (uint16_t curSpec = specNumStart; curSpec < specNumEnd; ++curSpec) {
                const double absSpec = abs(specs[curSpec]);
                if (absSpec > maxAbsSpec) {
                    if (absSpec > MAX_SCALE) {
                        cerr << "got " << absSpec << " value - overflow" << endl;
                        maxAbsSpec = MAX_SCALE;
                    } else {
                        maxAbsSpec = absSpec;
                    }
                }
            }
            const map<uint32_t, uint8_t>::const_iterator scaleIter = ScaleIndex.lower_bound(maxAbsSpec * 256);
            const double scaleFactor = scaleIter->first / 256.0;
            const uint8_t scaleFactorIndex = scaleIter->second;
            scaledBlocks.push_back(TScaledBlock(scaleFactorIndex));
            for (uint16_t specNum = specNumStart; specNum < specNumEnd; ++specNum) {
            const double scaledValue = specs[specNum] / scaleFactor;
                scaledBlocks.back().Values.push_back(scaledValue);
			}
		}
	}
    return scaledBlocks;
}
}
