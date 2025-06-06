#include "at3p_bitstream.h"
#include "at3p_bitstream_impl.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace NAtracDEnc;

TEST(AT3PBitstream, ToneFreqBitPack__1) {
    std::vector<TAt3PGhaData::TWaveParam> params;

    params.push_back(TAt3PGhaData::TWaveParam{1, 0, 0, 0});

    auto r = CreateFreqBitPack(params.data(), params.size());
    EXPECT_EQ(r.UsedBits, 10);
    EXPECT_EQ(r.Order, ETonePackOrder::ASC);
    EXPECT_EQ(r.Data.size(), 1);
    EXPECT_EQ(r.Data[0].Code, 1);
    EXPECT_EQ(r.Data[0].Bits, 10);
}

TEST(AT3PBitstream, ToneFreqBitPack__512_1020_1023) {
    std::vector<TAt3PGhaData::TWaveParam> params;

    params.push_back(TAt3PGhaData::TWaveParam{512, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1020, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1023, 0, 0, 0});

    auto r = CreateFreqBitPack(params.data(), params.size());
    EXPECT_EQ(r.UsedBits, 21);
    EXPECT_EQ(r.Order, ETonePackOrder::ASC);
    EXPECT_EQ(r.Data.size(), 3);
    EXPECT_EQ(r.Data[0].Code, 512);
    EXPECT_EQ(r.Data[0].Bits, 10);
    EXPECT_EQ(r.Data[1].Code, 508);
    EXPECT_EQ(r.Data[1].Bits, 9);
    EXPECT_EQ(r.Data[2].Code, 3);
    EXPECT_EQ(r.Data[2].Bits, 2);
}

TEST(AT3PBitstream, ToneFreqBitPack__1_2_3) {
    std::vector<TAt3PGhaData::TWaveParam> params;

    params.push_back(TAt3PGhaData::TWaveParam{1, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{2, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{3, 0, 0, 0});

    auto r = CreateFreqBitPack(params.data(), params.size());
    EXPECT_EQ(r.UsedBits, 14);
    EXPECT_EQ(r.Order, ETonePackOrder::DESC);
    EXPECT_EQ(r.Data.size(), 3);
    EXPECT_EQ(r.Data[0].Code, 3);
    EXPECT_EQ(r.Data[0].Bits, 10);
    EXPECT_EQ(r.Data[1].Code, 2);
    EXPECT_EQ(r.Data[1].Bits, 2);
    EXPECT_EQ(r.Data[2].Code, 1);
    EXPECT_EQ(r.Data[2].Bits, 2);
}

TEST(AT3PBitstream, ToneFreqBitPack__1_2_3_1020_1021_1022) {
    std::vector<TAt3PGhaData::TWaveParam> params;

    params.push_back(TAt3PGhaData::TWaveParam{1, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{2, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{3, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1020, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1021, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1022, 0, 0, 0});

    auto r = CreateFreqBitPack(params.data(), params.size());
    EXPECT_EQ(r.UsedBits, 44);
    EXPECT_EQ(r.Order, ETonePackOrder::DESC);
    EXPECT_EQ(r.Data.size(), 6);
    EXPECT_EQ(r.Data[0].Code, 1022);
    EXPECT_EQ(r.Data[0].Bits, 10);
    EXPECT_EQ(r.Data[1].Code, 1021);
    EXPECT_EQ(r.Data[1].Bits, 10);
    EXPECT_EQ(r.Data[2].Code, 1020);
    EXPECT_EQ(r.Data[2].Bits, 10);
    EXPECT_EQ(r.Data[3].Code, 3);
    EXPECT_EQ(r.Data[3].Bits, 10);
    EXPECT_EQ(r.Data[4].Code, 2);
    EXPECT_EQ(r.Data[4].Bits, 2);
    EXPECT_EQ(r.Data[5].Code, 1);
    EXPECT_EQ(r.Data[5].Bits, 2);
}

TEST(AT3PBitstream, ToneFreqBitPack__1_2_1020_1021_1022) {
    std::vector<TAt3PGhaData::TWaveParam> params;

    params.push_back(TAt3PGhaData::TWaveParam{1, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{2, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1020, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1021, 0, 0, 0});
    params.push_back(TAt3PGhaData::TWaveParam{1022, 0, 0, 0});

    auto r = CreateFreqBitPack(params.data(), params.size());
    EXPECT_EQ(r.UsedBits, 34);
    EXPECT_EQ(r.Order, ETonePackOrder::ASC);
    EXPECT_EQ(r.Data.size(), 5);
    EXPECT_EQ(r.Data[0].Code, 1);
    EXPECT_EQ(r.Data[0].Bits, 10);
    EXPECT_EQ(r.Data[1].Code, 2);
    EXPECT_EQ(r.Data[1].Bits, 10);
    EXPECT_EQ(r.Data[2].Code, 1020);
    EXPECT_EQ(r.Data[2].Bits, 10);
    EXPECT_EQ(r.Data[3].Code, 1);
    EXPECT_EQ(r.Data[3].Bits, 2);
    EXPECT_EQ(r.Data[4].Code, 2);
    EXPECT_EQ(r.Data[4].Bits, 2);
}

void FillFrameData(TSpecFrame& frame) {
    frame.NumQuantUnits = 6;

    for (size_t i = 0; i < frame.NumQuantUnits; i++) {
        frame.WordLen.push_back({6, 6});
    }
}

TEST(AT3PBitstream, Wordlen) {
    std::vector<IBitStreamPartEncoder::TPtr> encoders;

    encoders.emplace_back(new TWordLenEncoder());

    NBitStream::TBitStream bs;
    TBitStreamEncoder encoder(std::move(encoders));

    std::vector<std::vector<TScaledBlock>> scaledBlocks;
    scaledBlocks.resize(2);

    TSpecFrame frame(444, 28, 2, nullptr, scaledBlocks);

    FillFrameData(frame);

    encoder.Do(&frame, bs);

    EXPECT_EQ(bs.GetSizeInBits(), 28);
}

