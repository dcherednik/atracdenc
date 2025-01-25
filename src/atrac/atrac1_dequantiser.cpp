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

#include "atrac1_dequantiser.h"
#include <string.h>
namespace NAtracDEnc {
namespace NAtrac1 {

using namespace NBitStream;

TAtrac1Dequantiser::TAtrac1Dequantiser() {
}

void TAtrac1Dequantiser::Dequant(TBitStream* stream, const TAtrac1Data::TBlockSizeMod& bs, float specs[512]) {
    uint32_t wordLens[TAtrac1Data::MaxBfus];
    uint32_t idScaleFactors[TAtrac1Data::MaxBfus];
    const uint32_t numBFUs = TAtrac1Data::BfuAmountTab[stream->Read(3)];
    stream->Read(2);
    stream->Read(3);

    for (uint32_t i = 0; i < numBFUs; i++) {
        wordLens[i] = stream->Read(4);
    }

    for (uint32_t i = 0; i < numBFUs; i++) {
        idScaleFactors[i] = stream->Read(6);
    }
    for (uint32_t i = numBFUs; i < TAtrac1Data::MaxBfus; i++) {
        wordLens[i] = idScaleFactors[i] = 0;
    }

    for (uint32_t bandNum = 0; bandNum < TAtrac1Data::NumQMF; bandNum++) {
        for (uint32_t bfuNum = TAtrac1Data::BlocksPerBand[bandNum]; bfuNum < TAtrac1Data::BlocksPerBand[bandNum + 1]; bfuNum++) {
            const uint32_t numSpecs = TAtrac1Data::SpecsPerBlock[bfuNum];
            const uint32_t wordLen = !!wordLens[bfuNum] + wordLens[bfuNum];
            const float scaleFactor = TAtrac1Data::ScaleTable[idScaleFactors[bfuNum]];
            const uint32_t startPos = bs.LogCount[bandNum] ?
                TAtrac1Data::SpecsStartShort[bfuNum] : TAtrac1Data::SpecsStartLong[bfuNum];
            if (wordLen) {
                float maxQuant = 1.0 / (float)((1 << (wordLen - 1)) - 1);
                //cout << "BFU ("<< bfuNum << ") :" <<  "wordLen " << wordLen << " maxQuant " << maxQuant << " scaleFactor " << scaleFactor << " id " << idScaleFactors[bfuNum] << " num Specs " << numSpecs << " short: "<< (int)bs.LogCount[bandNum] << endl;
                for (uint32_t i = 0; i < numSpecs; i++ ) {
                    specs[startPos + i] = scaleFactor * maxQuant * MakeSign(stream->Read(wordLen), wordLen);
                }
            } else {
                memset(&specs[startPos], 0, numSpecs * sizeof(float));
            }
        }

    }
}

} //namespace NAtrac1
} //namespace NAtracDEnc
