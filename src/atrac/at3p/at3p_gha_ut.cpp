#include "at3p_gha.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

using std::vector;

using namespace NAtracDEnc;

struct TTestParam {
    float freq;
    uint16_t amplitude;
    uint16_t start;
    uint16_t end;
};

static void __attribute__ ((noinline)) Gen(const TTestParam& p, vector<float>& out)
{
    float freq = p.freq / (44100.0 / 16.0);
    float a = p.amplitude;
    int end = p.end;
    for (int i = p.start; i < end; i++) {
        out[i] += sin(freq * (float)(i % 128) * 2 * M_PI) * a;
    }
}

static const TAt3PGhaData __attribute__ ((noinline)) GenAndRunGha(vector<TTestParam> p1, vector<TTestParam> p2)
{
    vector<float> buf1(2048);

    for (const auto& p : p1) {
        Gen(p, buf1);
    }

    vector<float> buf2;

    if (!p2.empty()) {
        buf2.resize(2048);

        for (const auto& p : p2) {
            Gen(p, buf2);
        }
    }

    auto processor = MakeGhaProcessor0(buf1.data(), buf2.empty() ? nullptr : buf2.data());


    return *(processor->DoAnalize());
}

// Single channel simple cases

TEST(AT3PGHA, 689hz0625__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first->AmpSf, 63);
}


TEST(AT3PGHA, 689hz0625__partial_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 32768, 32, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_900hz__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 16384, 0, 128}, {900.0, 8192, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 669);
}

TEST(AT3PGHA, 400hz_800hz__full_frame_mono) {
    auto res = GenAndRunGha({{400.0, 16384, 0, 128}, {800.0, 4096, 0, 128}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 2);
    EXPECT_EQ(res.GetWaves(0, 0).second, 2);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 297);
    EXPECT_EQ(res.GetWaves(0, 0).first[1].FreqIndex, 594);
}

TEST(AT3PGHA, 689hz0625_2067hz1875__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}, {689.0625f, 16384, 128, 256}}, {});
    EXPECT_EQ(res.NumToneBands, 2);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_4823hz4375__full_frame_mono) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}, {689.0625f, 16384, 256, 384}}, {});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);
}

// Two channels simple cases

TEST(AT3PGHA, 689hz0625__full_frame_stereo) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}}, {{689.0625f, 32768, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(1, 0), 1);
    EXPECT_EQ(res.GetWaves(1, 0).second, 1);
    EXPECT_EQ(res.GetWaves(1, 0).first->FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625__full_frame_stereo_multiple_second) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}}, {{689.0625f, 16384, 0, 128}, {900.0, 8192, 0, 128}});
    EXPECT_EQ(res.NumToneBands, 1);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first->FreqIndex, 512);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 1);
    EXPECT_EQ(res.GetNumWaves(1, 0), 2);
    EXPECT_EQ(res.GetWaves(1, 0).second, 2);
    EXPECT_EQ(res.GetWaves(1, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(1, 0).first[1].FreqIndex, 669);
}

TEST(AT3PGHA, 689hz0625_2067hz1875__full_frame_stereo_second_is_leader) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}},
                            {{689.0625f, 32768, 0, 128}, {689.0625f, 16384, 128, 256}});
    EXPECT_EQ(res.NumToneBands, 2);
    EXPECT_EQ(res.Waves[0].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[0].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetNumWaves(0, 1), 1);
    EXPECT_EQ(res.GetWaves(0, 0).second, 1);
    EXPECT_EQ(res.GetWaves(0, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(0, 1).second, 1);
    EXPECT_EQ(res.GetWaves(0, 1).first[0].FreqIndex, 512);

    EXPECT_EQ(res.Waves[1].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 2);
    EXPECT_EQ(res.GetNumWaves(0, 0), 1);
    EXPECT_EQ(res.GetWaves(1, 0).second, 1);
    EXPECT_EQ(res.GetWaves(1, 0).first[0].FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_folower_has_0_2) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}, {689.0625f, 32768, 128, 256}, {689.0625f, 16384, 256, 384}},
                            {{689.0625f, 32768, 0, 128}, {689.0625f, 16384, 256, 384}});
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

    EXPECT_EQ(res.SecondChBands[0], true);
    EXPECT_EQ(res.SecondChBands[1], false);
    EXPECT_EQ(res.SecondChBands[2], true);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 2);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(1, 0), 1);
    EXPECT_EQ(res.GetNumWaves(1, 2), 1);
    EXPECT_EQ(res.GetWaves(1, 0).second, 1);
    EXPECT_EQ(res.GetWaves(1, 0).first[0].FreqIndex, 512);
    EXPECT_EQ(res.GetWaves(1, 2).second, 1);
    EXPECT_EQ(res.GetWaves(1, 2).first[0].FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_folower_has_2) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}, {689.0625f, 32768, 128, 256}, {689.0625f, 16384, 256, 384}},
                            {{689.0625f, 16384, 256, 384}});
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

    EXPECT_EQ(res.SecondChBands[0], false);
    EXPECT_EQ(res.SecondChBands[1], false);
    EXPECT_EQ(res.SecondChBands[2], true);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(1, 2), 1);
    EXPECT_EQ(res.GetWaves(1, 2).second, 1);
    EXPECT_EQ(res.GetWaves(1, 2).first[0].FreqIndex, 512);
}

TEST(AT3PGHA, 689hz0625_2067hz1875_3445hz3125__full_frame_stereo_folower_has_1) {
    auto res = GenAndRunGha({{689.0625f, 32768, 0, 128}, {689.0625f, 32768, 128, 256}, {689.0625f, 16384, 256, 384}},
                            {{689.0625f, 16384, 128, 256}});
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

    EXPECT_EQ(res.SecondChBands[0], false);
    EXPECT_EQ(res.SecondChBands[1], true);
    EXPECT_EQ(res.SecondChBands[2], false);
    EXPECT_EQ(res.Waves[1].WaveParams.size(), 1);
    EXPECT_EQ(res.Waves[1].WaveSbInfos.size(), 3);
    EXPECT_EQ(res.GetNumWaves(1, 1), 1);
    EXPECT_EQ(res.GetWaves(1, 1).second, 1);
    EXPECT_EQ(res.GetWaves(1, 1).first[0].FreqIndex, 512);
}

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
}


