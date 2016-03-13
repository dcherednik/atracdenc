#include "atrac3.h"
#include <algorithm>
constexpr uint32_t TAtrac3Data::BlockSizeTab[33];
constexpr uint32_t TAtrac3Data::ClcLengthTab[8];
constexpr double TAtrac3Data::MaxQuant[8];
constexpr uint32_t TAtrac3Data::BlocksPerBand[4 + 1];
constexpr uint32_t TAtrac3Data::SpecsPerBlock[33];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable1[HuffTable1Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable2[HuffTable2Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable3[HuffTable3Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable5[HuffTable5Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable6[HuffTable6Sz];
constexpr TAtrac3Data::THuffEntry TAtrac3Data::HuffTable7[HuffTable7Sz];
constexpr TAtrac3Data::THuffTablePair TAtrac3Data::HuffTables[7];

constexpr TContainerParams TAtrac3Data::ContainerParams[8];
double TAtrac3Data::EncodeWindow[256] = {0};
double TAtrac3Data::ScaleTable[64] = {0};

const TContainerParams* TAtrac3Data::GetContainerParamsForBitrate(uint32_t bitrate) {
    std::cout << bitrate << std::endl;
    return std::lower_bound(ContainerParams, ContainerParams+8, bitrate);
}
