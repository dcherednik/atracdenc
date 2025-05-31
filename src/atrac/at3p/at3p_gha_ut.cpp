#include "at3p_bitstream.h"
#include "at3p_gha.h"
#include "oma.h"
#include "util.h"
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
    return *p->DoAnalize(b1, b2, w1, w2);
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

    auto processor = MakeGhaProcessor0(!p2.empty());
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
    auto res = GenAndRunGha({{689.0625f, 0, 32768, 0, 128}, {689.0625f, 0, 16384, 256, 384}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
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
    auto processor = MakeGhaProcessor0(false);

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
    auto processor = MakeGhaProcessor0(false);

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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
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

    auto processor = MakeGhaProcessor0(false);
    float w1[2048] = {0};
    float w2[2048] = {0};
    {
        const auto res = processor->DoAnalize({&buf[0], &buf[2048]}, {nullptr, nullptr}, w1, w2);
        EXPECT_EQ(res->NumToneBands, 1);
        EXPECT_EQ(res->Waves[0].WaveParams.size(), 1);
        EXPECT_EQ(res->GetWaves(0, 0).first[0].FreqIndex, expFreqIndex);
    }
    {
        memcpy(&w1[0], &buf[0], sizeof(float) * 2048);
        const auto res = processor->DoAnalize({&buf[2048], &buf[4096]}, {nullptr, nullptr}, w1, w2);
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
