#include "bitstream.h"
#include <gtest/gtest.h>

using namespace NBitStream;

TEST(TBitStream, DefaultConstructor) {
    TBitStream bs;
    EXPECT_EQ(0u, bs.GetSizeInBits());
}

TEST(TBitStream, SimpleWriteRead) {
    TBitStream bs;
    bs.Write(5, 3);
    EXPECT_EQ(3, bs.GetSizeInBits());
    EXPECT_EQ(5, bs.Read(3));
}

TEST(TBisStream, OverlapWriteRead) {
    TBitStream bs;
    bs.Write(101, 22);
    EXPECT_EQ(22, bs.GetSizeInBits());

    bs.Write(212, 22);
    EXPECT_EQ(44, bs.GetSizeInBits());

    bs.Write(323, 22);
    EXPECT_EQ(66, bs.GetSizeInBits());

    EXPECT_EQ(101, bs.Read(22));
    EXPECT_EQ(212, bs.Read(22));
    EXPECT_EQ(323, bs.Read(22));
}
TEST(TBisStream, OverlapWriteRead2) {
    TBitStream bs;
    bs.Write(2, 2);
    bs.Write(7, 4);
    bs.Write(10003, 16);

    EXPECT_EQ(2, bs.Read(2));
    EXPECT_EQ(7, bs.Read(4));
    EXPECT_EQ(10003, bs.Read(16));
}

TEST(TBisStream, SignWriteRead) {
    TBitStream bs;
    bs.Write(MakeSign(-2, 3), 3);
    bs.Write(MakeSign(-1, 3), 3);
    bs.Write(MakeSign(1, 2), 2);
    bs.Write(MakeSign(-7, 4), 4);
    EXPECT_EQ(-2, MakeSign(bs.Read(3), 3));
    EXPECT_EQ(-1, MakeSign(bs.Read(3), 3));
    EXPECT_EQ(1, MakeSign(bs.Read(2), 2));
    EXPECT_EQ(-7, MakeSign(bs.Read(4), 4));
}

