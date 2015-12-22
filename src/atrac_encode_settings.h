#pragma once

namespace NAtracDEnc {

class TAtrac1EncodeSettings {
public:
    enum class EWindowMode {
        EWM_NOTRANSIENT,
        EWM_AUTO
    };
private:
    const uint32_t BfuIdxConst = 0;
    const bool FastBfuNumSearch = false;
    EWindowMode WindowMode = EWindowMode::EWM_AUTO;
    const uint32_t WindowMask = 0;
public:
    TAtrac1EncodeSettings();
    TAtrac1EncodeSettings(uint32_t bfuIdxConst, bool fastBfuNumSearch, EWindowMode windowMode, uint32_t windowMask)
        : BfuIdxConst(bfuIdxConst)
        , FastBfuNumSearch(fastBfuNumSearch)
        , WindowMode(windowMode)
        , WindowMask(windowMask)
    {}
    uint32_t GetBfuIdxConst() const { return BfuIdxConst; }
    bool GetFastBfuNumSearch() const { return FastBfuNumSearch; }
    EWindowMode GetWindowMode() const {return WindowMode; }
    uint32_t GetWindowMask() const {return WindowMask; }
};

}
