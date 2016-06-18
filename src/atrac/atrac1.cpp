#include "atrac1.h"

namespace NAtracDEnc {
namespace NAtrac1 {

constexpr uint32_t TAtrac1Data::BlocksPerBand[NumQMF + 1];
constexpr uint32_t TAtrac1Data::SpecsPerBlock[MaxBfus];
constexpr uint32_t TAtrac1Data::SpecsStartLong[MaxBfus];
constexpr uint32_t TAtrac1Data::SpecsStartShort[MaxBfus];
constexpr uint32_t TAtrac1Data::BfuAmountTab[8];
double TAtrac1Data::ScaleTable[64] = {0};
double TAtrac1Data::SineWindow[32] = {0};

} //namespace NAtrac1
} //namespace NAtracDEnc
