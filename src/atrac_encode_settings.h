#pragma once

namespace NAtracDEnc {

class TAtrac1EncodeSettings {
public:
    enum class EWindowMode {
        EWM_LONG_ONLY,
        EWM_SHORT_ONLY,
        EWM_AUTO
    };
private:
    const uint32_t BfuIdxConst = 0;
    const bool FastBfuNumSearch = false;
    EWindowMode WindowMode = EWindowMode::EWM_LONG_ONLY;
public:
    TAtrac1EncodeSettings();
    TAtrac1EncodeSettings(uint32_t bfuIdxConst, bool fastBfuNumSearch, EWindowMode windowMode)
        : BfuIdxConst(bfuIdxConst)
        , FastBfuNumSearch(fastBfuNumSearch)
        , WindowMode(windowMode)
    {}
    uint32_t GetBfuIdxConst() const { return BfuIdxConst; }
    bool GetFastBfuNumSearch() const { return FastBfuNumSearch; }

};

}
