#include "atrac1.h"

constexpr uint32_t TAtrac1Data::BlocksPerBand[QMF_BANDS + 1];
constexpr uint32_t TAtrac1Data::SpecsPerBlock[MAX_BFUS];
constexpr uint32_t TAtrac1Data::SpecsStartLong[MAX_BFUS];
constexpr uint32_t TAtrac1Data::SpecsStartShort[MAX_BFUS];
constexpr uint32_t TAtrac1Data::BfuAmountTab[8];
double TAtrac1Data::ScaleTable[64] = {0};
double TAtrac1Data::SineWindow[32] = {0};

