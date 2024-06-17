/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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
    bs.Write(true, 1);
    EXPECT_EQ(4, bs.GetSizeInBits());
    EXPECT_EQ(5, bs.Read(3));
    EXPECT_EQ(true, bs.Read(1));
}

TEST(TBitStream, OverlapWriteRead) {
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

TEST(TBitStream, OverlapWriteRead2) {
    TBitStream bs;
    bs.Write(2, 2);
    bs.Write(7, 4);
    bs.Write(10003, 16);

    EXPECT_EQ(2, bs.Read(2));
    EXPECT_EQ(7, bs.Read(4));
    EXPECT_EQ(10003, bs.Read(16));
}

TEST(TBitStream, OverlapWriteRead3) {
    TBitStream bs;
    bs.Write(40, 6);
    bs.Write(3, 2);
    bs.Write(0, 3);
    bs.Write(0, 3);
    bs.Write(0, 3);
    bs.Write(0, 3);

    bs.Write(3, 5);
    bs.Write(1, 2);
    bs.Write(1, 1);
    bs.Write(1, 1);
    bs.Write(1, 1);
    bs.Write(1, 1);

    bs.Write(0, 3);
    bs.Write(4, 3);
    bs.Write(35, 6);
    bs.Write(25, 6);
    bs.Write(3, 3);
    bs.Write(32, 6);
    bs.Write(29, 6);
    bs.Write(3, 3);
    bs.Write(36, 6);
    bs.Write(49, 6);




    EXPECT_EQ(40, bs.Read(6));
    EXPECT_EQ(3, bs.Read(2));
    EXPECT_EQ(0, bs.Read(3));
    EXPECT_EQ(0, bs.Read(3));
    EXPECT_EQ(0, bs.Read(3));
    EXPECT_EQ(0, bs.Read(3));
    EXPECT_EQ(3, bs.Read(5));

    EXPECT_EQ(1, bs.Read(2));
    EXPECT_EQ(1, bs.Read(1));
    EXPECT_EQ(1, bs.Read(1));
    EXPECT_EQ(1, bs.Read(1));
    EXPECT_EQ(1, bs.Read(1));

    EXPECT_EQ(0, bs.Read(3));
    EXPECT_EQ(4, bs.Read(3));
    EXPECT_EQ(35, bs.Read(6));
    EXPECT_EQ(25, bs.Read(6));
    EXPECT_EQ(3, bs.Read(3));
    EXPECT_EQ(32, bs.Read(6));
    EXPECT_EQ(29, bs.Read(6));
    EXPECT_EQ(3, bs.Read(3));
    EXPECT_EQ(36, bs.Read(6));
    EXPECT_EQ(49, bs.Read(6));

}


TEST(TBitStream, SignWriteRead) {
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

