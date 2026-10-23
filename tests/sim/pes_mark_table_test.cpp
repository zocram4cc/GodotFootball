// PES 2021's mark table (team-tick step 35, FUN_1404F61E0): the 11-byte opponent mask
// T+0xB7EC[11]. Pure behaviour tests with synthetic candidates; no install needed except
// the constants check. MarkGateRE's four open inputs arrive as Facts booleans.
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "onthepitch/pesmarktable.hpp"
#include "pes/exe.hpp"

namespace {

using pes::marktable::Candidate;
using pes::marktable::Constants;
using pes::marktable::Facts;
using pes::marktable::MarkState;

Constants TestConstants() {
  Constants k;
  k.rampBase = 7.0f;
  k.rampTop = 3.0f;
  k.coneOffset = 15.0f;
  k.ceilD2 = 1369.0f;
  k.wrapCap = 105.0f;
  k.wrapSide = 80.0f;
  k.coneDeg = 90.0f;
  k.coneLen = 12.0f;
  k.gateRadius = 64.0f;
  k.half = 0.5f;
  return k;
}

// A passing scene: side 0 marks slot 3, holder is slot 5, one candidate at slot 7 with
// bearings that clear the 105/80 wrap gates. The ramp wind puts the cone at 0.5 - 15.
Facts PassingFacts() {
  Facts f;
  f.side = 0;
  f.holder = 5;
  f.sign = 1.0f;
  f.holderX = 0.0f;
  f.rampWind = -7.5f;  // lo = -14.5, ramp clamps: (0 + 14.5)/3 > 1 -> cone = 1 - 15
  Candidate c;
  c.id = 7;
  c.d2 = 100.0f;
  c.pairToTarget = 10.0f;
  c.pairToHolder = 70.0f;  // cap |70| < 105, side |70| < 80, floor |70 + 14| = 84 >= 80
  c.head = 0.0f;
  f.candidates[0] = c;
  return f;
}

TEST(MarkTable, ZeroedOnEntry) {
  MarkState s;
  s.markSlot = 3;
  s.mark.fill(1);
  Facts f = PassingFacts();
  f.holder = 0xFF;  // gated out: mask keeps only the zero-init
  pes::marktable::BuildMarkTable(&s, f, TestConstants());
  for (uint8_t b : s.mark) EXPECT_EQ(b, 0);
}

TEST(MarkTable, EmptySlotMarksNothing) {
  MarkState s;  // markSlot = 0xFF
  Facts f = PassingFacts();
  pes::marktable::BuildMarkTable(&s, f, TestConstants());
  for (uint8_t b : s.mark) EXPECT_EQ(b, 0);
}

TEST(MarkTable, HolderKeeperOrTargetMarksNothing) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.holderIsKeeper = true;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
  f = PassingFacts();
  f.holder = 3;  // == tgt (side 0 * 11 + 3)
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

TEST(MarkTable, MarksIdModElevenUpToThree) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;  // tgt 9: none of the ids 1..5 is skipped
  Facts f = PassingFacts();
  for (int i = 0; i < 5; ++i) {
    f.candidates[i].id = i + 1;
    f.candidates[i].d2 = 100.0f;
    f.candidates[i].pairToTarget = 10.0f;
    f.candidates[i].pairToHolder = 70.0f;
    f.candidates[i].head = 0.0f;
  }
  pes::marktable::BuildMarkTable(&s, f, k);
  int total = 0;
  for (uint8_t b : s.mark) total += b;
  EXPECT_EQ(total, 3);
  EXPECT_EQ(s.mark[1], 1);
  EXPECT_EQ(s.mark[2], 1);
  EXPECT_EQ(s.mark[3], 1);
  EXPECT_EQ(s.mark[4], 0) << "the fourth passing candidate stops at the cap";
  EXPECT_EQ(s.mark[5], 0);
}

TEST(MarkTable, GlobalIdFoldsModEleven) {
  // The commit index is id % 11 (0x1404F65AA MUL 0xBA2E8BA3): id 12 commits slot 1.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;
  Facts f = PassingFacts();
  f.candidates[0].id = 12;
  f.candidates[0].d2 = 100.0f;
  f.candidates[0].pairToTarget = 10.0f;
  f.candidates[0].pairToHolder = 70.0f;
  f.candidates[0].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[1], 1);
  EXPECT_EQ(s.mark[0], 0);
}

TEST(MarkTable, FarCandidateHardExits) {
  // d^2 > 1369 returns (0x1404F643D JA), so a later passing candidate is never reached.
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].id = 1;
  f.candidates[0].d2 = 2000.0f;
  f.candidates[1].id = 2;
  f.candidates[1].d2 = 100.0f;
  f.candidates[1].pairToTarget = 10.0f;
  f.candidates[1].pairToHolder = 70.0f;
  f.candidates[1].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[2], 0);
}

TEST(MarkTable, WrapBoundaryPasses) {
  // 0x1404F64D9 JA: d = 105.0 exactly does not skip (strictly-greater exit).
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 9;
  Facts f = PassingFacts();
  f.candidates[0].pairToTarget = 105.0f;
  f.candidates[0].pairToHolder = 70.0f;
  f.candidates[0].head = 0.0f;
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 1);
}

TEST(MarkTable, WrapGatesReject) {
  const Constants k = TestConstants();
  MarkState s;
  s.markSlot = 3;
  Facts f = PassingFacts();
  f.candidates[0].pairToTarget = 150.0f;  // |150 - 0| = 150 > 105
  pes::marktable::BuildMarkTable(&s, f, k);
  EXPECT_EQ(s.mark[7], 0);
}

// The install's constants, read from the exe's .rdata at the VAs the header cites.
TEST(MarkTableInstall, LoadsTheInstallConstants) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the mark constants come from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto loaded = pes::marktable::LoadMarkTableConstants(*exe, &err);
  ASSERT_TRUE(loaded.has_value()) << err;
  const Constants want = TestConstants();
  EXPECT_FLOAT_EQ(loaded->rampBase, want.rampBase);
  EXPECT_FLOAT_EQ(loaded->rampTop, want.rampTop);
  EXPECT_FLOAT_EQ(loaded->coneOffset, want.coneOffset);
  EXPECT_FLOAT_EQ(loaded->ceilD2, want.ceilD2);
  EXPECT_FLOAT_EQ(loaded->wrapCap, want.wrapCap);
  EXPECT_FLOAT_EQ(loaded->wrapSide, want.wrapSide);
  EXPECT_FLOAT_EQ(loaded->coneDeg, want.coneDeg);
  EXPECT_FLOAT_EQ(loaded->coneLen, want.coneLen);
  EXPECT_FLOAT_EQ(loaded->gateRadius, want.gateRadius);
  EXPECT_FLOAT_EQ(loaded->half, want.half);
}

}  // namespace
