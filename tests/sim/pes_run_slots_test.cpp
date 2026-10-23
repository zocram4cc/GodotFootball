// PES's per-slot run table: the ring push, the state age, the block variants, the assignment and the release,
// each checked against the bytes the decoded functions write. Expectations are written out by hand.
// A fresh ring's head is entered at 0, so the first push always moves the index on (FUN_140A40200 compares the
// head's entered stamp with the new one).
#include <gtest/gtest.h>

#include <cstdint>

#include "onthepitch/pesrunslots.hpp"

namespace {

using pes::run::Age;
using pes::run::Assign;
using pes::run::Behind;
using pes::run::Point;
using pes::run::Push;
using pes::run::Release;
using pes::run::Ring;
using pes::run::Slot;
using pes::run::Touch;
using pes::run::ZeroTargetBlock;

constexpr int kChance = 0x1e;  // SPACE_RUN_CHANCE (team-tickets step 29)

TEST(PesRunRing, ANewStateIsRecordedWithItsStamps) {
  Ring r;
  Push(&r, kChance, 100);
  EXPECT_EQ(r.index, 1);
  EXPECT_EQ(r.entry[1].id, kChance);
  EXPECT_EQ(r.entry[1].enteredAt, 100u);
  EXPECT_EQ(r.entry[1].seenAt, 100u);
}

TEST(PesRunRing, TheSameStateAgainChangesNothing) {
  Ring r;
  Push(&r, kChance, 100);
  Push(&r, kChance, 150);
  EXPECT_EQ(r.index, 1);
  EXPECT_EQ(r.entry[1].enteredAt, 100u);
  EXPECT_EQ(r.entry[1].seenAt, 100u);
}

TEST(PesRunRing, TheIndexAdvancesOnlyWhenTheHeadIsOlder) {
  Ring r;
  Push(&r, 1, 100);  // the fresh head (entered at 0) is older: the index moves to 1
  Push(&r, 2, 100);  // the head was entered at 100, not before it: overwritten in place
  EXPECT_EQ(r.index, 1);
  EXPECT_EQ(r.entry[1].id, 2);
  Push(&r, 3, 101);  // the head was entered at 100, before 101: the index moves on
  EXPECT_EQ(r.index, 2);
  EXPECT_EQ(Behind(r, 0).id, 3);
  EXPECT_EQ(Behind(r, 1).id, 2);
}

TEST(PesRunRing, TheRingHoldsFourEntriesAndWraps) {
  Ring r;
  for (int i = 1; i <= 4; ++i) Push(&r, i, 10u * static_cast<uint32_t>(i));
  // Index moves: 1 -> 1, 2 -> 2, 3 -> 3, 4 -> wraps to 0.
  EXPECT_EQ(r.index, 0);
  EXPECT_EQ(Behind(r, 0).id, 4);
  EXPECT_EQ(Behind(r, 1).id, 3);
  EXPECT_EQ(Behind(r, 3).id, 1);
  Push(&r, 5, 50);  // moves on to entry 1, overwriting id 1
  EXPECT_EQ(r.index, 1);
  EXPECT_EQ(Behind(r, 0).id, 5);
  EXPECT_EQ(Behind(r, 1).id, 4);
  EXPECT_EQ(Behind(r, 3).id, 2);
}

TEST(PesRunRing, AgeIsTheHeadsSeenStampMinusItsEnteredStamp) {
  Ring r;
  Push(&r, kChance, 100);
  Touch(&r, 137);
  EXPECT_EQ(Age(r), 37u);
}

TEST(PesRunBlock, TheChanceVariantWritesItsBytesAndLeavesThePoint) {
  Slot s;
  pes::run::SetPoint(&s, {1.0f, 2.0f, 3.0f});
  Assign(&s, kChance, 200);
  EXPECT_EQ(s.state, kChance);
  EXPECT_EQ(s.ring.entry[s.ring.index].id, kChance);
  EXPECT_EQ(s.ring.entry[s.ring.index].enteredAt, 200u);
  // FUN_140A85AA0(block + 0x20, 0x1e): the dword 0x00010100 at +0x10, then the qword 0x0000000000010101 at +0x14.
  const uint8_t dword[4] = {0x00, 0x01, 0x01, 0x00};
  const uint8_t qword[8] = {0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < 4; ++i) EXPECT_EQ(s.block[0x30 + i], dword[i]) << "dword byte " << i;
  for (int i = 0; i < 8; ++i) EXPECT_EQ(s.block[0x34 + i], qword[i]) << "qword byte " << i;
  EXPECT_FLOAT_EQ(Point(s)[0], 1.0f);
  EXPECT_FLOAT_EQ(Point(s)[2], 3.0f);
}

TEST(PesRunBlock, ReleaseClearsTheStateAndThePointAndPushesZero) {
  Slot s;
  pes::run::SetPoint(&s, {1.0f, 2.0f, 3.0f});
  s.block[0x1d] = 0xab;  // the release does not write 0x1d..0x1f
  s.block[0x20] = 0x55;  // the 32-byte target block is zeroed
  Assign(&s, kChance, 200);
  Release(&s, 300);
  EXPECT_EQ(s.state, 0);
  EXPECT_EQ(s.ring.entry[s.ring.index].id, 0);
  EXPECT_EQ(s.ring.entry[s.ring.index].enteredAt, 300u);
  EXPECT_FLOAT_EQ(Point(s)[0], 0.0f);
  EXPECT_FLOAT_EQ(Point(s)[1], 0.0f);
  EXPECT_FLOAT_EQ(Point(s)[2], 0.0f);
  EXPECT_EQ(s.block[0x1d], 0xab);
  for (int i = 0x20; i < 0x40; ++i) EXPECT_EQ(s.block[i], 0) << "target byte " << i;
}

TEST(PesRunBlock, ReleasingAFreshSlotPushesNothing) {
  Slot s;
  Release(&s, 50);
  EXPECT_EQ(s.ring.index, 0);
  EXPECT_EQ(s.ring.entry[0].id, 0);
  EXPECT_EQ(s.ring.entry[0].enteredAt, 0u);
}

TEST(PesRunBlock, TheTargetVariantsWriteTheirOwnBytes) {
  uint8_t p[32];
  for (uint8_t& b : p) b = 0xff;
  ZeroTargetBlock(p, 0x22);  // the word 0x100 at +4, the byte 1 at +6
  EXPECT_EQ(p[4], 0x00);
  EXPECT_EQ(p[5], 0x01);
  EXPECT_EQ(p[6], 0x01);
  EXPECT_EQ(p[7], 0x00);
  ZeroTargetBlock(p, 0x27);  // only zeros
  for (uint8_t b : p) EXPECT_EQ(b, 0);
  ZeroTargetBlock(p, 0x1c);  // at or below 0x1c: zeros only
  for (uint8_t b : p) EXPECT_EQ(b, 0);
}

}  // namespace
