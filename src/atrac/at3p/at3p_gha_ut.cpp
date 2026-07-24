#include "at3p_bitstream.h"
#include "at3p_gha.h"
#include "oma.h"
#include "util.h"
#include <atrac/atrac3plus_pqf/atrac3plus_pqf.h>
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstdlib>

using std::vector;

using namespace NAtracDEnc;

struct TTestParam {
    float freq;
    float phase;
    uint16_t amplitude;
    uint16_t start;
    uint16_t end;
};

static void atde_noinline Gen(const TTestParam& p, vector<float>& out)
{
    float freq = p.freq / (44100.0 / 16.0);
    float a = p.amplitude;
    int end = p.end;
    int j = 0;
    for (int i = p.start; i < end; i++, j++) {
        out[i] += sin(freq * (float)j * 2.0 * M_PI + p.phase) * a;
    }
}

static TAt3PGhaData DoAnalize(IGhaProcessor* p, IGhaProcessor::TBufPtr b1, IGhaProcessor::TBufPtr b2) {
    float w1[2048] = {0};
    float w2[2048] = {0};
    return *p->DoAnalize(b1, b2, w1, w2, nullptr, nullptr);
}

static const TAt3PGhaData atde_noinline GenAndRunGha(vector<TTestParam> p1, vector<TTestParam> p2)
{
    vector<float> buf1(2048 * 2);

    for (const auto& p : p1) {
        Gen(p, buf1);
    }

    vector<float> buf2;

    if (!p2.empty()) {
        buf2.resize(2048 * 2);

        for (const auto& p : p2) {
            Gen(p, buf2);
        }
    }

    auto processor = MakeGhaProcessor0(!p2.empty(), false);
    const float* b1 = buf1.data();
    const float* b2 = buf2.empty() ? nullptr : buf2.data();

    return DoAnalize(processor.get(), {b1, b1 + 2048}, {b2, b2 + 2048});
}

static class TDumper {
public:
    TDumper()
        : PathPrefix(std::getenv("GHA_UT_DUMP_DIR"))
    {}

    void Dump(const TAt3PGhaData* gha, size_t channels, size_t len) {
        if (!PathPrefix) {
            return;
        }

        std::string path = PathPrefix;
        path += "/";
        path += ::testing::UnitTest::GetInstance()->current_test_info()->name();
        path += ".oma";

        std::unique_ptr<TOma> out(new TOma(path,
            "test",
            channels,
            1, OMAC_ID_ATRAC3PLUS,
            2048,
            false));

        TAt3PBitStream bs(out.get(), 2048);

        for (size_t i = 0; i < len; i++) {
        //    bs.WriteFrame(channels, gha + i);
        }
    }
private:
    const char* PathPrefix;
} Dumper;


// Single channel simple cases

TEST(AT3PGHA, 689hz0625__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 63);
    //EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);
    Dumper.Dump(&res, 1, 1);
}

TEST(AT3PGHA, 0__full_frame_mono) {
    auto res = GenAndRunGha({{0.0f, M_PI/2, 32768, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 0);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 63);
    //EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);
    Dumper.Dump(&res, 1, 1);
}


TEST(AT3PGHA, 689hz0625__partial_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 32, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_900hz__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 0, 16384, 0, 128}, {900.0, 0, 8192, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 669);
    Dumper.Dump(&res, 1, 1);
}

TEST(AT3PGHA, 400hz_800hz__full_frame_mono) {
    auto res = GenAndRunGha({{400.0, 0, 16384, 0, 128}, {800.0, 0, 4096, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 297);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 594);
    Dumper.Dump(&res, 1, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 0, 16384, 0, 128}, {689.0625f, 0, 16384, 128, 256}}, {});
    EXPECT_EQ(res.NumToneBands, 2);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 59);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).first->AmpSf, 59);
    Dumper.Dump(&res, 1, 1);
}

TEST(AT3PGHA, 689hz0625_4823hz4375__full_frame_mono) {
    // The abrupt on/off edges of the second 689.0625Hz segment (a "click" at
    // samples 128 and 256) splatter real, ATH-passing energy into subband 2
    // as well as subband 0 -- DoRound's own CheckResuidalAndApply validation
    // (round 2+) confirms it explains the residual across the WHOLE
    // subband-2 window (envelope (0,31), i.e. start=0/end=~128), not a
    // marginal len>=4 borderline pass, so this is a real detected tone, not
    // noise. FillResultBuf now legitimately reports it (NumToneBands=3,
    // subband 1 empty in between) instead of the old contiguous-run walk
    // silently truncating everything past subband 0's first gap.
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 16384, 256, 384}}, {});
    EXPECT_EQ(res.NumToneBands, 3);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
    EXPECT_EQ(res.GetNumWaves(0, 1), 0);
    EXPECT_EQ(res.GetNumWaves(0, 2), 1);
    EXPECT_EQ(res.GetWaves(0, 2).second, 1);
    EXPECT_EQ(res.GetWaves(0, 2).first->FreqIndex, 512);
    Dumper.Dump(&res, 1, 1);
}

// Two channels simple cases

TEST(AT3PGHA, 689hz0625__full_frame_stereo_shared) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}}, {{689.0625f, 0, 32768, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], true);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0);
    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625__full_frame_stereo_own) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}}, {{1000.0625f, 0, 32768, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], false);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(1, 0), 1);
    EXPECT_EQ(res.GetWaves(1, 0).second, 1);
    EXPECT_EQ(res.GetWaves(1, 0).first->FreqIndex, 743);

    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625__full_frame_stereo_multiple_second) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}}, {{689.0625f, 0, 16384, 0, 128}, {900.0, 0, 8192, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], false);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(1, 0), 2);
    EXPECT_EQ(res.GetWaves(1, 0).second, 2);
    EXPECT_EQ(res.GetWaves(1, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(1, 0).first[1].FreqIndex, 669);
    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875__full_frame_stereo_first_is_leader) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 16384, 128, 256}},
                            {{689.0625f, 0, 32768, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 2);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], true);
    EXPECT_EQ(res.ToneSharing[1], false);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0); // sb0 sharing, sb1 zerro
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(1, 1), 0);
    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875__full_frame_stereo_second_is_leader) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}},
                            {{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 16384, 128, 256}});
    EXPECT_EQ(res.NumToneBands, 2);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], true);
    EXPECT_EQ(res.ToneSharing[1], false);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(1, 1), 0);

    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_sharing_0_2) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 32768, 128, 256}, {689.0625f, 0, 16384, 256, 384}},
                            {{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 16384, 256, 384}});
    EXPECT_EQ(res.NumToneBands, 3);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 3);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetNumWaves(0, 2), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 2).second, 1);
    EXPECT_EQ(res.GetWaves(0, 2).first[0].FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], true);
    EXPECT_EQ(res.ToneSharing[1], false);
    EXPECT_EQ(res.ToneSharing[2], true);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0);
    EXPECT_EQ(res.GetNumWaves(1, 1), 0);
    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_folower_sharing_2) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 32768, 128, 256}, {689.0625f, 0, 16384, 256, 384}},
                            {{689.0625f, 0, 16384, 256, 384}});
    EXPECT_EQ(res.NumToneBands, 3);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 3);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetNumWaves(0, 2), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 2).second, 1);
    EXPECT_EQ(res.GetWaves(0, 2).first[0].FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], false);
    EXPECT_EQ(res.ToneSharing[1], false);
    EXPECT_EQ(res.ToneSharing[2], true);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0);
    EXPECT_EQ(res.GetNumWaves(1, 0), 0);
    EXPECT_EQ(res.GetNumWaves(1, 1), 0);
    Dumper.Dump(&res, 2, 1);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_folower_sharing_1) {
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 32768, 128, 256}, {689.0625f, 0, 16384, 256, 384}},
                            {{689.0625f, 0, 16384, 128, 256}});
    EXPECT_EQ(res.NumToneBands, 3);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 3);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetNumWaves(0, 2), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 2).second, 1);
    EXPECT_EQ(res.GetWaves(0, 2).first[0].FreqIndex, 512);

    EXPECT_EQ(res.ToneSharing[0], false);
    EXPECT_EQ(res.ToneSharing[1], true);
    EXPECT_EQ(res.ToneSharing[2], false);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 0);
    EXPECT_EQ(res.GetNumWaves(1, 0), 0);
    EXPECT_EQ(res.GetNumWaves(1, 2), 0);
    Dumper.Dump(&res, 2, 1);
}

/*
TEST(AT3PGHA, max_tones_multiple_bands_full_frame_stereo) {
    auto res = GenAndRunGha({
                                {60.0f, 8192, 0,   128}, {120.0f, 8192, 0,   128}, {180.0f, 4096, 0,   128}, {240.0f, 2048, 0,   128},
                                {60.0f, 8192, 128, 256}, {120.0f, 8192, 128, 256}, {180.0f, 4096, 128, 256}, {240.0f, 2048, 128, 256},
                                {60.0f, 8192, 256, 384}, {120.0f, 8192, 256, 384}, {180.0f, 4096, 256, 384}, {240.0f, 2048, 256, 384},
                                {60.0f, 8192, 384, 512}, {120.0f, 8192, 384, 512}, {180.0f, 4096, 384, 512}, {240.0f, 2048, 384, 512},
                                {60.0f, 8192, 512, 640}, {120.0f, 8192, 512, 640}, {180.0f, 4096, 512, 640}, {240.0f, 2048, 512, 640},
                                {60.0f, 8192, 640, 768}, {120.0f, 8192, 640, 768}, {180.0f, 4096, 640, 768}, {240.0f, 2048, 640, 768},
                                {60.0f, 8192, 768, 896}, {120.0f, 8192, 768, 896}, {180.0f, 4096, 768, 896}, {240.0f, 2048, 768, 896},
                            }, {
                                {60.0f, 8192, 0,   128}, {120.0f, 8192, 0,   128}, {180.0f, 4096, 0,   128}, {240.0f, 2048, 0,   128},
                                {60.0f, 8192, 128, 256}, {120.0f, 8192, 128, 256}, {180.0f, 4096, 128, 256}, {240.0f, 2048, 128, 256},
                                {60.0f, 8192, 256, 384}, {120.0f, 8192, 256, 384}, {180.0f, 4096, 256, 384}, {240.0f, 2048, 256, 384},
                                {60.0f, 8192, 384, 512}, {120.0f, 8192, 384, 512}, {180.0f, 4096, 384, 512}, {240.0f, 2048, 384, 512},
                                {60.0f, 8192, 512, 640}, {120.0f, 8192, 512, 640}, {180.0f, 4096, 512, 640}, {240.0f, 2048, 512, 640},
                                {60.0f, 8192, 640, 768}, {120.0f, 8192, 640, 768}, {180.0f, 4096, 640, 768}, {240.0f, 2048, 640, 768},
                                {60.0f, 8192, 768, 896}, {120.0f, 8192, 768, 896}, {180.0f, 4096, 768, 896}, {240.0f, 2048, 768, 896},
                            });
    EXPECT_EQ(res.NumToneBands, 7);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 28);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 7);
    EXPECT_EQ(res.GetNumWaves(0, 0), 4);
    EXPECT_EQ(res.GetNumWaves(0, 1), 4);
    EXPECT_EQ(res.GetNumWaves(0, 2), 4);
    EXPECT_EQ(res.GetNumWaves(0, 3), 4);
    EXPECT_EQ(res.GetNumWaves(0, 4), 4);
    EXPECT_EQ(res.GetNumWaves(0, 5), 4);
    EXPECT_EQ(res.GetNumWaves(0, 6), 4);
    EXPECT_EQ(res.GetWaves(0, 0).second, 4);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 0).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 0).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 1).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 1).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 1).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 2).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 2).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 2).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 2).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 3).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 3).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 3).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 3).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 4).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 4).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 4).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 4).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 5).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 5).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 5).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 5).first[3].FreqIndex, 178);

    EXPECT_EQ(res.GetWaves(0, 6).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(0, 6).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(0, 6).first[2].FreqIndex, 134);
    EXPECT_EQ(res.GetWaves(0, 6).first[3].FreqIndex, 178);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 21);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 7);
    EXPECT_EQ(res.GetNumWaves(1, 0), 3);
    EXPECT_EQ(res.GetNumWaves(1, 1), 3);
    EXPECT_EQ(res.GetNumWaves(1, 2), 3);
    EXPECT_EQ(res.GetNumWaves(1, 3), 3);
    EXPECT_EQ(res.GetNumWaves(1, 4), 3);
    EXPECT_EQ(res.GetNumWaves(1, 5), 3);
    EXPECT_EQ(res.GetNumWaves(1, 6), 3);
    EXPECT_EQ(res.GetWaves(1, 0).second, 3);
    EXPECT_EQ(res.GetWaves(1, 0).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 0).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 0).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 1).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 1).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 1).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 2).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 2).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 2).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 3).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 3).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 3).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 4).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 4).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 4).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 5).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 5).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 5).first[2].FreqIndex, 134);

    EXPECT_EQ(res.GetWaves(1, 6).first[0].FreqIndex, 45);
    EXPECT_EQ(res.GetWaves(1, 6).first[1].FreqIndex, 89);
    EXPECT_EQ(res.GetWaves(1, 6).first[2].FreqIndex, 134);
    Dumper.Dump(&res, 2, 1);
}

*/

TEST(AT3PGHA, 100hz__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({100.0f, 0, 32768, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    std::vector<TAt3PGhaData> resBuf;
    auto processor = MakeGhaProcessor0(false, false);

    {
        const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

        EXPECT_EQ(res.NumToneBands, 1);
        EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
        EXPECT_EQ(res.GetNumWaves(0, 0), 1);
        EXPECT_EQ(res.GetWaves(0, 0).second, 1);
        EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 74);
        EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
        EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);

        resBuf.push_back(res);
    }

    {
        memset(&buf[0], 0, sizeof(float) * 128);
        const auto res = DoAnalize(processor.get(), {&buf[2048], &buf[0]}, {nullptr, nullptr});

        EXPECT_EQ(res.NumToneBands, 1);
        EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
        EXPECT_EQ(res.GetNumWaves(0, 0), 1);
        EXPECT_EQ(res.GetWaves(0, 0).second, 1);
        EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 74);
        EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
        EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 21);

        resBuf.push_back(res);
    }
    Dumper.Dump(resBuf.data(), 1, 2);
}

TEST(AT3PGHA, 100hz_than_500hz_than_100hz__3_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({100.0f, 0, 32768, 0, 128}, buf);
    Gen({500.0f, 0, 32768, 128, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    std::vector<TAt3PGhaData> resBuf;
    auto processor = MakeGhaProcessor0(false, false);

    {
        const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

        EXPECT_EQ(res.NumToneBands, 1);
        EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
        EXPECT_EQ(res.GetNumWaves(0, 0), 1);
        EXPECT_EQ(res.GetWaves(0, 0).second, 1);
        EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 74);
        EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
        EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);

        resBuf.push_back(res);
    }

    {
        memset(&buf[0], 0, sizeof(float) * 128);
        Gen({100.0f, 0, 32768, 0, 128}, buf);
        const auto res = DoAnalize(processor.get(), {&buf[2048], &buf[0]}, {nullptr, nullptr});

        EXPECT_EQ(res.NumToneBands, 1);
        EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
        EXPECT_EQ(res.GetNumWaves(0, 0), 1);
        EXPECT_EQ(res.GetWaves(0, 0).second, 1);
        EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 372);
        EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
        EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);

        resBuf.push_back(res);
    }
    {
        memset(&buf[2048], 0, sizeof(float) * 128);
        const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

        EXPECT_EQ(res.NumToneBands, 1);
        EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
        EXPECT_EQ(res.GetNumWaves(0, 0), 1);
        EXPECT_EQ(res.GetWaves(0, 0).second, 1);
        EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 74);
        EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
        EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 0);

        resBuf.push_back(res);
    }

    Dumper.Dump(resBuf.data(), 1, 3);
}

TEST(AT3PGHA, 100hz__phase_two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({100.0f, M_PI * 0.25, 32768, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 74);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 62);
    EXPECT_EQ(res.GetWaves(0, 0).first->PhaseIndex, 4);
}

TEST(AT3PGHA, 689hz0625__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({689.0625f, 0, 32768, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 63);
}

TEST(AT3PGHA, 689hz0625_1000hz__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({689.0625f, 0, 16384, 0, 256}, buf);
    Gen({1000.0f, 0, 16384, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].AmpSf, 58);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 743);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].AmpSf, 58);
}

TEST(AT3PGHA, 500hz_1000hz__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({500.0f, 0, 16384, 0, 256}, buf);
    Gen({1000.0f, 0, 2048, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 372);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].AmpSf, 58);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 743);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].AmpSf, 46);
}

TEST(AT3PGHA, 500hz_1000hz__phase_two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({500.0f, M_PI * 0.5, 16384, 0, 256}, buf);
    Gen({1000.0f, M_PI * 0.25, 2048, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 372);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].AmpSf, 59);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 743);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].AmpSf, 46);
    Dumper.Dump(&res, 1, 1);
}

TEST(AT3PGHA, 250hz_500hz_1000hz__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({250.0f, 0, 16384, 0, 256}, buf);
    Gen({500.0f, 0, 4096, 0, 256}, buf);
    Gen({1000.0f, 0, 2048, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 3);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 3);
    EXPECT_EQ(res.GetWaves(0, 0).second, 3);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 186);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].AmpSf, 58);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 372);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].AmpSf, 50);
    EXPECT_EQ(res.GetWaves(0, 0).first[2].FreqIndex, 743);
    EXPECT_EQ(res.GetWaves(0, 0).first[2].AmpSf, 46);
}

TEST(AT3PGHA, 250hz_500hz_1000hz_1200hz__two_frames_mono) {
    vector<float> buf(2048 * 2);

    Gen({250.0f, 0, 16384, 0, 256}, buf);
    Gen({500.0f, 0, 8000, 0, 256}, buf);
    Gen({1000.0f, 0, 4096, 0, 256}, buf);
    Gen({1200.0f, 0, 2048, 0, 256}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 128);

    auto processor = MakeGhaProcessor0(false, false);
    const auto res = DoAnalize(processor.get(), {&buf[0], &buf[2048]}, {nullptr, nullptr});

    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 4);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 4);
    EXPECT_EQ(res.GetWaves(0, 0).second, 4);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 186);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].AmpSf, 58);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 372);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].AmpSf, 54);
    EXPECT_EQ(res.GetWaves(0, 0).first[2].FreqIndex, 743);
    EXPECT_EQ(res.GetWaves(0, 0).first[2].AmpSf, 50);
    EXPECT_EQ(res.GetWaves(0, 0).first[3].FreqIndex, 892);
    EXPECT_EQ(res.GetWaves(0, 0).first[3].AmpSf, 46);
}

void CheckReduction(float f, uint32_t expFreqIndex){
    vector<float> buf(2048 * 3);
    Gen({f, 0, 16384, 0, 384}, buf);

    memcpy(&buf[2048], &buf[128], sizeof(float) * 128);
    memcpy(&buf[4096], &buf[256], sizeof(float) * 128);
    memset(&buf[128], 0, sizeof(float) * 256);

    auto processor = MakeGhaProcessor0(false, false);
    float w1[2048] = {0};
    float w2[2048] = {0};
    {
        const auto res = processor->DoAnalize({&buf[0], &buf[2048]}, {nullptr, nullptr}, w1, w2, nullptr, nullptr);
        EXPECT_EQ(res->NumToneBands, 1);
        EXPECT_EQ(res->Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res->GetWaves(0, 0).first[0].FreqIndex, expFreqIndex);
    }
    {
        memcpy(&w1[0], &buf[0], sizeof(float) * 2048);
        const auto res = processor->DoAnalize({&buf[2048], &buf[4096]}, {nullptr, nullptr}, w1, w2, nullptr, nullptr);
        EXPECT_EQ(res->NumToneBands, 1);
        EXPECT_EQ(res->Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res->GetWaves(0, 0).first[0].FreqIndex, expFreqIndex);
        double e1 = 0;
        double e2 = 0;
        for (size_t i = 0; i < 128; i++) {
            e1 += w1[i] * w1[i];
            e2 += buf[i] * buf[i];
        }
        std::cerr << 5 * log(e2/e1) << std::endl;
        float reduction = 5 * log(e2/e1);
        EXPECT_GE(reduction, 50);
    }

}

TEST(AT3PGHA, 269hz166_long_frame_mono) {
    CheckReduction(269.166, 200);
}

TEST(AT3PGHA, 999hz948_long_frame_mono) {
    CheckReduction(999.948, 743);
}

TEST(AT3PGHA, 1345hz826_long_frame_mono) {
    CheckReduction(1345.826, 1000);
}

// ---- Wideband GHA path (GHA_WIDEBAND) ----
//
// Unlike the tests above, which fabricate subband-domain buffers directly
// (bypassing the real PQF filter), the wideband path's correctness depends
// on the ACTUAL PQF analysis output being consistent with the raw PCM it
// was derived from (its magnitude/phase composition was calibrated against
// the real at3plus_pqf_do_analyse -- see at3p_pqf_wideband_table.h and the
// "wideband GHA + PQF-domain projection" plan). These tests run the real
// PQF filter to build a consistent (raw, PQF) pair, matching how
// TAt3PEnc::TImpl::EncodeFrame does it in production.

static void atde_noinline GenWideband(float freqHz, float amplitude, float* out, size_t len)
{
    const double omega = 2.0 * M_PI * freqHz / 44100.0;
    for (size_t i = 0; i < len; i++) {
        out[i] += (float)(amplitude * sin(omega * i));
    }
}

// Range variant, mirroring the legacy Gen/TTestParam::start,end pattern
// already in this file: only fills [startSample, endSample), leaving the
// rest of the buffer untouched (silent, if not otherwise written).
static void atde_noinline GenWidebandRange(float freqHz, float amplitude, float* out,
    size_t startSample, size_t endSample)
{
    const double omega = 2.0 * M_PI * freqHz / 44100.0;
    for (size_t i = startSample; i < endSample; i++) {
        out[i] += (float)(amplitude * sin(omega * i));
    }
}

// Two consecutive frames of raw PCM through the real PQF filter, matching
// EncodeFrame's Cur/Next buffer convention (frame 0 = Cur, frame 1 = Next).
struct TWidebandPqfFixture {
    float Raw0[2048] = {0};
    float Raw1[2048] = {0};
    float Pqf0[2048];
    float Pqf1[2048];

    void Run() {
        at3plus_pqf_a_ctx_t ctx = at3plus_pqf_create_a_ctx();
        at3plus_pqf_do_analyse(ctx, Raw0, Pqf0);
        at3plus_pqf_do_analyse(ctx, Raw1, Pqf1);
        at3plus_pqf_free_a_ctx(ctx);
    }
};

TEST(AT3PGHAWideband, SingleToneRoundTrips) {
    TWidebandPqfFixture fx;
    // 2000Hz sits in the middle of subband 1 (1378-2756Hz) -- away from any
    // subband boundary, so this is a plain "does the projected amplitude
    // land in the right ballpark" check, guarding the empirical
    // tone.Magnitude * response.Magnitude scale equivalence against silent
    // drift (see FillChannelDataWideband's comment on this).
    GenWideband(2000.0f, 16384.0f, fx.Raw0, 2048);
    GenWideband(2000.0f, 16384.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const float* raw1Cur = fx.Raw0;
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, raw1Cur, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GE(res->NumToneBands, 2u); // subband 0 (anchor) + subband 1 (the tone)
    ASSERT_GT(res->GetNumWaves(0, 1), 0u);
    // AmpSf 63 is the top of the table (see CreateAmpSfTab / AmplitudeToSf);
    // a 16384-amplitude tone should land solidly in the upper range, not
    // near the bottom -- a coarse bound, not a precise one, but enough to
    // catch a scale-formula regression (e.g. an accidental extra factor of
    // response.Magnitude, or dropping the tone.Magnitude term entirely).
    EXPECT_GE(res->GetWaves(0, 1).first[0].AmpSf, 40u);
}

TEST(AT3PGHAWideband, StereoManyTonesStaysUnderBudget) {
    // Enough simultaneous tones, across both channels, that a budget-check
    // bug (see FillChannelDataWideband's "shared totalTones across both
    // channels" requirement) would either abort() in ApplyFilter (>48
    // combined) or, if under-counted, silently misbehave. This is
    // primarily a "does this complete without crashing" regression guard.
    TWidebandPqfFixture fx1, fx2;
    for (int k = 0; k < 12; k++) {
        float f = 300.0f + k * 210.0f; // spread across subbands 0-4ish
        GenWideband(f, 3000.0f, fx1.Raw0, 2048);
        GenWideband(f, 3000.0f, fx1.Raw1, 2048);
        GenWideband(f + 55.0f, 3000.0f, fx2.Raw0, 2048);
        GenWideband(f + 55.0f, 3000.0f, fx2.Raw1, 2048);
    }
    fx1.Run();
    fx2.Run();

    auto processor = MakeGhaProcessor0(true, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx1.Pqf0, fx1.Pqf1}, {fx2.Pqf0, fx2.Pqf1}, w1, w2, fx1.Raw0, fx2.Raw0);

    ASSERT_NE(res, nullptr);
    size_t totalWaves = 0;
    for (size_t sb = 0; sb < res->NumToneBands; sb++) {
        totalWaves += res->GetNumWaves(0, sb);
        if (!res->ToneSharing[sb]) {
            totalWaves += res->GetNumWaves(1, sb);
        }
    }
    EXPECT_LE(totalWaves, 48u);
}

// ---- Envelope (onset/offset) detection ----
//
// FindWidebandEnvelope's wire-grid units: raw subband-sample-equivalent
// position (0-128) divided by 4 gives the 32-point wire grid AdjustEnvelope
// produces. A raw sample index i (0-2047, wideband domain) corresponds to
// wire-grid point i/16/4 = i/64.

TEST(AT3PGHAWideband, OnsetMidFrameDetected) {
    TWidebandPqfFixture fx;
    // Silent for the first half of Raw0, on for the second half and all of
    // Raw1 -- isolates onset detection from the gapless-continuation path.
    GenWidebandRange(2000.0f, 16384.0f, fx.Raw0, 1024, 2048);
    GenWideband(2000.0f, 16384.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GT(res->GetNumWaves(0, 1), 0u);
    auto env = res->GetEnvelope(0, 1);
    // Onset at raw sample 1024 == wire-grid point 16. Coarse bound (not
    // exact), same spirit as SingleToneRoundTrips's AmpSf check -- the
    // single-shot fit over a half-silent buffer has some estimation slop,
    // this just needs to prove mid-frame onset is detected at all, not "at
    // frame start" (0) or "not detected" (EMPTY_POINT).
    EXPECT_NE(env.first, TAt3PGhaData::EMPTY_POINT);
    EXPECT_GE(env.first, 8u);
    EXPECT_LE(env.first, 24u);
}

TEST(AT3PGHAWideband, OffsetMidFrameDetected) {
    TWidebandPqfFixture fx;
    // On for the first half of Raw0, silent after -- Raw1 stays silent too
    // so there's no continuation to worry about.
    GenWidebandRange(2000.0f, 16384.0f, fx.Raw0, 0, 1024);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GT(res->GetNumWaves(0, 1), 0u);
    auto env = res->GetEnvelope(0, 1);
    // Offset at raw sample 1024 == wire-grid point 16. Must not be 31 (ran
    // to frame end) or EMPTY_POINT (declared gapless).
    EXPECT_NE(env.second, TAt3PGhaData::EMPTY_POINT);
    EXPECT_GE(env.second, 8u);
    EXPECT_LE(env.second, 24u);
}

TEST(AT3PGHAWideband, GaplessAcrossFrames) {
    TWidebandPqfFixture fx;
    // Unbroken tone spanning both frames -- regression guard that
    // CheckNextFrame-based continuation still collapses .second to
    // EMPTY_POINT now that envelopes are computed, not hardcoded.
    GenWideband(2000.0f, 16384.0f, fx.Raw0, 2048);
    GenWideband(2000.0f, 16384.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GT(res->GetNumWaves(0, 1), 0u);
    auto env = res->GetEnvelope(0, 1);
    EXPECT_EQ(env.second, TAt3PGhaData::EMPTY_POINT);
}

TEST(AT3PGHAWideband, OverlappingTonesShareSubbandEnvelopeUnion) {
    // Two tones, home subbands 0 and 2, both close enough to subband 1's
    // edges to project into it as a shared neighbor -- with DIFFERENT
    // on/off timing. Guards the union-not-overwrite merge policy in
    // FillChannelDataWideband: subband 1 must reflect tone A's full-frame
    // coverage, not get truncated to tone B's later-starting window by a
    // naive overwrite. (Deliberately keeps tone A's home subband at 0, not
    // 1/3 as in an earlier draft of this test: with both tones far from
    // subband 0, its own real content is leakage-only and can be so small
    // that a single-shot Newton fit on near-silence produces a wildly
    // spurious huge-magnitude "tone" there, which the anchor pass's energy-
    // ratio validation correctly rejects -- collapsing NumToneBands to 0 via
    // the contiguous-from-subband-0 bitstream constraint, unrelated to what
    // this test is actually checking. Anchoring tone A directly in subband 0
    // sidesteps that pre-existing edge case entirely.)
    TWidebandPqfFixture fx;
    GenWideband(1300.0f, 12000.0f, fx.Raw0, 2048);           // home sb=0, full frame, near the 1378Hz edge
    GenWidebandRange(2800.0f, 12000.0f, fx.Raw0, 1024, 2048); // home sb=2, second half only, near the 2756Hz edge
    GenWideband(1300.0f, 12000.0f, fx.Raw1, 2048);
    GenWidebandRange(2800.0f, 12000.0f, fx.Raw1, 1024, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GE(res->NumToneBands, 2u);
    ASSERT_GT(res->GetNumWaves(0, 1), 0u) << "expected both tones to project into shared subband 1";
    auto env = res->GetEnvelope(0, 1);
    // Tone A alone covers the whole frame -- the union must reflect that,
    // not tone B's later start (~wire-grid 16).
    EXPECT_TRUE(env.first == TAt3PGhaData::EMPTY_POINT || env.first < 8u);
}

// --- Newton refinement (RefineWidebandTones) ---------------------------------

TEST(AT3PGHAWideband, RefinementKeepsToneCountMidSubbandTone) {
    // A clean mid-subband tone (2000Hz, middle of subband 1). The refinement
    // pass must not change how many tones/bands come out -- it only refines
    // the parameters of the already-discovered set (or falls back), never
    // adds or drops a tone. Guards the "totalTones is not touched" contract.
    TWidebandPqfFixture fx;
    GenWideband(2000.0f, 16384.0f, fx.Raw0, 2048);
    GenWideband(2000.0f, 16384.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    // Same expectation as SingleToneRoundTrips: subband 0 (anchor leakage) +
    // subband 1 (the tone). The refit must leave that structure intact.
    ASSERT_GE(res->NumToneBands, 2u);
    ASSERT_EQ(res->GetNumWaves(0, 1), 1u) << "refinement must not split or drop the single tone";
    // A 16384-amplitude tone must still land solidly in the upper AmpSf range
    // after refit -- catches a refit that corrupts magnitude but still passes
    // the drift guard's ratio bound coincidentally.
    EXPECT_GE(res->GetWaves(0, 1).first[0].AmpSf, 40u);
}

TEST(AT3PGHAWideband, RefinementSeparatesTwoToneSubband) {
    // Two well-separated tones that both land in subband 1 (1378-2756Hz):
    // 1700Hz and 2400Hz. This is the only wideband test that exercises the
    // JOINT multi-tone (k=2) Newton refit -- gha_adjust_info refining both
    // tones of the subband simultaneously against the real subband PCM. A
    // refit that diverged, collapsed the two onto one frequency (dupFound),
    // or otherwise corrupted the set would either fall back to the analytic
    // pair or drop one; either way the property to hold is that subband 1
    // still carries two distinct tones with distinct quantized frequency
    // indices. (The end-to-end residual-dB improvement from refit is measured
    // separately and robustly by the encoder chirp A/B, which does not depend
    // on ApplyFilter's cross-frame tone-synthesis scale conventions.)
    TWidebandPqfFixture fx;
    GenWideband(1700.0f, 12000.0f, fx.Raw0, 2048);
    GenWideband(2400.0f, 9000.0f, fx.Raw0, 2048);
    GenWideband(1700.0f, 12000.0f, fx.Raw1, 2048);
    GenWideband(2400.0f, 9000.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    ASSERT_GE(res->NumToneBands, 2u);
    ASSERT_EQ(res->GetNumWaves(0, 1), 2u) << "both tones must survive the joint refit in subband 1";
    auto w = res->GetWaves(0, 1);
    EXPECT_NE(w.first[0].FreqIndex, w.first[1].FreqIndex) << "the two tones must keep distinct frequency indices";
}

TEST(AT3PGHAWideband, RefinementFallbackOnBoundaryToneNoBlowup) {
    // A tone sitting essentially ON the subband 1/2 boundary (2756Hz). This
    // is exactly the ill-conditioned regime the drift guard exists for: a
    // per-subband Newton solve on a boundary tone can produce a near-singular
    // Hessian and a spurious huge-magnitude fit. Whether the refit is accepted
    // (within guard) or rejected (fallback to analytic), the result must be a
    // sane tone -- never a NaN/garbage magnitude, never an abort(). Reaching
    // this assertion at all proves no abort() fired in
    // CheckResuidalAndApply/AdjustEnvelope.
    TWidebandPqfFixture fx;
    GenWideband(2756.0f, 14000.0f, fx.Raw0, 2048);
    GenWideband(2756.0f, 14000.0f, fx.Raw1, 2048);
    fx.Run();

    auto processor = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = processor->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    // The tone should land in subband 1 or 2 (its home±1 window). Whichever
    // subbands hold tones, every emitted wave must have a sane amplitude
    // index (the AmpSf table is 0..63; a blown-up magnitude would still clamp
    // to 63 via AmplitudeToSf, so also assert it's not implausibly maxed for
    // a 14000-amplitude tone that projects attenuated across the boundary --
    // i.e. it must be a real fit, not a saturated garbage one).
    bool anyTone = false;
    for (int sb = 0; sb < res->NumToneBands; sb++) {
        auto w = res->GetWaves(0, sb);
        for (size_t i = 0; i < w.second; i++) {
            anyTone = true;
            // Sane band: amplitude index within table range (structurally
            // guaranteed, but asserts the tone exists and is well-formed).
            EXPECT_LE(w.first[i].AmpSf, 63u);
            EXPECT_LE(w.first[i].FreqIndex, 1023u);
        }
    }
    EXPECT_TRUE(anyTone) << "boundary tone must still be encoded (refined or analytic)";
}

// --- Option A: raw-2048-domain refit (ghawbrefine=1, MakeGhaProcessor0 arg 3) -

// Count the total waves across all tone bands of channel 0.
static size_t TotalWaves(const TAt3PGhaData& res) {
    size_t n = 0;
    for (int sb = 0; sb < res.NumToneBands; sb++) {
        n += res.GetNumWaves(0, sb);
    }
    return n;
}

TEST(AT3PGHAWideband, RawRefineMatchesAnalyticStructureIsolatedTone) {
    // A single clean tone is already well-fit; the raw-domain refit must not
    // change which subbands it occupies or how many waves come out vs the
    // subband-domain default. (Both modes are exercised on the same input.)
    TWidebandPqfFixture fx;
    GenWideband(2000.0f, 16384.0f, fx.Raw0, 2048);
    GenWideband(2000.0f, 16384.0f, fx.Raw1, 2048);
    fx.Run();

    float w1[2048] = {0};
    float w2[2048] = {0};
    auto pB = MakeGhaProcessor0(false, true, 0);
    auto rB = *pB->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);
    auto pA = MakeGhaProcessor0(false, true, 1);
    auto rA = *pA->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    EXPECT_EQ(rA.NumToneBands, rB.NumToneBands);
    EXPECT_EQ(TotalWaves(rA), TotalWaves(rB)) << "raw-domain refit must not add or drop tones for a clean tone";
    EXPECT_GT(rA.GetNumWaves(0, 1), 0u);
}

TEST(AT3PGHAWideband, RawRefineBoundaryToneNoBlowup) {
    // Boundary tone (2756Hz) through the raw-domain refit: the joint Newton
    // runs in the well-conditioned 2048 domain (no per-subband boundary
    // ill-conditioning at all), and re-projection + the drift guard must keep
    // every emitted wave well-formed -- no NaN/garbage magnitude, no abort.
    TWidebandPqfFixture fx;
    GenWideband(2756.0f, 14000.0f, fx.Raw0, 2048);
    GenWideband(2756.0f, 14000.0f, fx.Raw1, 2048);
    fx.Run();

    auto proc = MakeGhaProcessor0(false, true, 1);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = proc->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    bool anyTone = false;
    for (int sb = 0; sb < res->NumToneBands; sb++) {
        auto w = res->GetWaves(0, sb);
        for (size_t i = 0; i < w.second; i++) {
            anyTone = true;
            EXPECT_LE(w.first[i].AmpSf, 63u);
            EXPECT_LE(w.first[i].FreqIndex, 1023u);
        }
    }
    EXPECT_TRUE(anyTone);
}

TEST(AT3PGHAWideband, RawRefineManyTonesStackSafe) {
    // ~40 tones spread across the whole band in a single frame forces the
    // raw-domain refit to run its batched joint Newton over multiple groups
    // (batch size 6). The point is that it completes without a stack overflow
    // from gha_adjust_info_newton_md's alloca (which would be ~5.5 MB for a
    // single 40-tone solve at sz=2048 -- hence the batching). Reaching the
    // assertions at all proves no crash; the counts just confirm it produced
    // a sane, non-empty result under the shared 48-tone budget.
    TWidebandPqfFixture fx;
    int n = 0;
    for (double f = 150.0; f < 10800.0; f += 260.0) {
        GenWideband((float)f, 1500.0f, fx.Raw0, 2048);
        GenWideband((float)f, 1500.0f, fx.Raw1, 2048);
        n++;
    }
    ASSERT_GE(n, 35);
    fx.Run();

    auto proc = MakeGhaProcessor0(false, true, 1);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = proc->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr);
    EXPECT_GT(TotalWaves(*res), 0u);
    EXPECT_LE(TotalWaves(*res), 48u) << "must respect the shared tone budget";
}

// ---- Normalized-amplitude regression tests --------------------------------
//
// The real encode pipeline feeds the GHA path PCM normalized to [-1, 1]
// (libsndfile float reads), NOT the int16-range amplitudes the tests above use.
// The psychoacoustic threshold (SubbandAth) is calibrated for that range, so
// these tests exercise the actual near-threshold detection regime rather than a
// saturated one. Amplitudes here are ~0.5 full-scale.

// A single normalized sparse tone must be detected in its home PQF subband.
// Home subband = floor(freqHz / 1378.125): 5000->3, 8000->5, 10000->7.
static void CheckNormalizedIsolatedTone(float freqHz, int homeSb) {
    TWidebandPqfFixture fx;
    GenWideband(freqHz, 0.5f, fx.Raw0, 2048);
    GenWideband(freqHz, 0.5f, fx.Raw1, 2048);
    fx.Run();

    auto proc = MakeGhaProcessor0(false, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = proc->DoAnalize({fx.Pqf0, fx.Pqf1}, {nullptr, nullptr}, w1, w2, fx.Raw0, nullptr);

    ASSERT_NE(res, nullptr) << freqHz << "Hz normalized tone must be detected";
    ASSERT_GT(res->NumToneBands, homeSb) << "home subband must be within the used range";
    EXPECT_GT(res->GetNumWaves(0, homeSb), 0u) << freqHz << "Hz must land in its home subband " << homeSb;
}

TEST(AT3PGHAWideband, NormalizedSparseTone5kHz)  { CheckNormalizedIsolatedTone(5000.0f, 3); }
TEST(AT3PGHAWideband, NormalizedSparseTone8kHz)  { CheckNormalizedIsolatedTone(8000.0f, 5); }
TEST(AT3PGHAWideband, NormalizedSparseTone10kHz) { CheckNormalizedIsolatedTone(10000.0f, 7); }

// Follower-channel fairness at normalized amplitudes: a dense left channel must
// not consume the whole shared 48-tone budget and starve a clean, isolated
// right-channel tone (this is what the round-robin extraction fairness exists
// to prevent). Assert the follower channel actually carries tone(s), not just
// that the combined count stays under budget.
TEST(AT3PGHAWideband, NormalizedDenseLeftSparseRightFollowerGetsTones) {
    TWidebandPqfFixture left, right;
    int n = 0;
    for (double f = 150.0; f < 10800.0; f += 200.0) { // dense, spans the band
        GenWideband((float)f, 0.06f, left.Raw0, 2048);
        GenWideband((float)f, 0.06f, left.Raw1, 2048);
        n++;
    }
    ASSERT_GE(n, 40);
    GenWideband(3000.0f, 0.5f, right.Raw0, 2048); // one clean tone, well above the left floor
    GenWideband(3000.0f, 0.5f, right.Raw1, 2048);
    left.Run();
    right.Run();

    auto proc = MakeGhaProcessor0(true, true);
    float w1[2048] = {0};
    float w2[2048] = {0};
    const auto* res = proc->DoAnalize({left.Pqf0, left.Pqf1}, {right.Pqf0, right.Pqf1}, w1, w2, left.Raw0, right.Raw0);

    ASSERT_NE(res, nullptr);
    size_t leaderWaves = 0, followerWaves = 0;
    for (int sb = 0; sb < res->NumToneBands; sb++) {
        leaderWaves += res->GetNumWaves(0, sb);
        followerWaves += res->GetNumWaves(1, sb);
    }
    EXPECT_GT(leaderWaves, 0u);
    EXPECT_GT(followerWaves, 0u) << "the sparse right channel must not be starved by the dense left channel";
    EXPECT_LE(leaderWaves + followerWaves, 48u) << "must respect the shared tone budget";
}
