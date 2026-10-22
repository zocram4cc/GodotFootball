// PES's out-of-play zone code (docs/re/pes-re-out-side.md): the classifier on the ball pulled in by one
// radius, and K=10's event on a change of the code only. Synthetic positions on the install's field.
#include <gtest/gtest.h>

#include "gametypes.hpp"
#include "onthepitch/pesevents.hpp"

namespace {

// The install's ball radius (0x142598958, pes-re-out-side.md 3).
constexpr float kBallRadius_m = 0.1086859f;

constexpr int kInside = 0x1, kOut = 0x2, kTouchline = 0x4, kEndLine = 0x8;

}  // namespace

TEST(PesOutOfPlayCode, ABallWhollyInsideIsInside) {
  const int code = pes::OutOfPlayCode(0.0f, 0.0f, kBallRadius_m);
  EXPECT_TRUE(code & kInside);
  EXPECT_FALSE(code & kOut);
}

TEST(PesOutOfPlayCode, ABallLeftHalfAnRadiusOverTheTouchlineIsStillInside) {
  // The point is pulled in by one radius: a centre a tenth of a metre past F1 is still inside.
  const int code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.10f, kBallRadius_m);
  EXPECT_TRUE(code & kInside);
  EXPECT_FALSE(code & kOut);
}

TEST(PesOutOfPlayCode, ABallWhollyOverTheTouchlineIsOutByTouchline) {
  const int code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.20f, kBallRadius_m);
  EXPECT_TRUE(code & kOut);
  EXPECT_TRUE(code & kTouchline);
  EXPECT_FALSE(code & kInside);
}

TEST(PesOutOfPlayCode, ABallWhollyOverTheEndLineIsOutByEndLine) {
  const int inside = pes::OutOfPlayCode(pesFieldHalfLength + 0.10f, 0.0f, kBallRadius_m);
  EXPECT_FALSE(inside & kOut);
  const int code = pes::OutOfPlayCode(pesFieldHalfLength + 0.20f, 0.0f, kBallRadius_m);
  EXPECT_TRUE(code & kOut);
  EXPECT_TRUE(code & kEndLine);
  EXPECT_FALSE(code & kTouchline);
}

TEST(PesOutOfPlayCode, TheSideBitsFollowTheSignOfThePulledInPoint) {
  const int code = pes::OutOfPlayCode(10.0f, -20.0f, kBallRadius_m);
  EXPECT_TRUE(code & 0x20);  // x' > 0
  EXPECT_TRUE(code & 0x80);  // z' < 0
  EXPECT_FALSE(code & 0x10);
  EXPECT_FALSE(code & 0x40);
}

TEST(PesOutOfPlayDetector, FiresOnAChangeOfTheCodeAndNotWhileItStays) {
  pes::OutOfPlayInput in;
  pes::EventRecord rec;
  in.code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.20f, kBallRadius_m);  // out
  EXPECT_TRUE(pes::DetectOutOfPlay(&in, &rec));
  EXPECT_EQ(rec.kind, 0x0au);
  EXPECT_EQ(in.stored, in.code);
  EXPECT_FALSE(pes::DetectOutOfPlay(&in, &rec)) << "an unchanged code raises no event";
  in.code = pes::OutOfPlayCode(0.0f, 0.0f, kBallRadius_m);  // back inside: a change
  EXPECT_TRUE(pes::DetectOutOfPlay(&in, &rec));
}

// PES's K=10 record carries the zone code in its first payload word: FUN_140a17810 writes it at record+0x14.
// The judge reads that word's bit 0x2, the out-of-play bit (FUN_14050e4a0 case 10, pes-re-A5Judge.md 1.4).
TEST(PesOutOfPlayEvent, TheEventCarriesTheCodeInItsFirstPayloadWord) {
  pes::OutOfPlayInput in;
  pes::EventRecord rec;
  in.code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.20f, kBallRadius_m);  // out
  ASSERT_TRUE(pes::DetectOutOfPlay(&in, &rec));
  EXPECT_EQ(rec.U32(0x14), static_cast<uint32_t>(in.code));
}

// The judge's gate (FUN_14050e4a0 case 10): a K=10 event moves the judge only when its code is out of play (bit 0x2).
// The flip back below raises an event for a ball whose centre is past the touchline by 0.05 m but whose whole ball is
// not: the pulled-in point is inside (pes-re-out-side.md 3), so the event carries no out-of-play bit.
TEST(PesOutOfPlayEvent, TheJudgeActsOnlyOnAnEventWhoseCodeIsOutOfPlay) {
  pes::OutOfPlayInput in;
  pes::EventRecord outRec, inRec;
  in.code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.20f, kBallRadius_m);  // out
  ASSERT_TRUE(pes::DetectOutOfPlay(&in, &outRec));
  EXPECT_TRUE(pes::JudgeActsOnOutOfPlay(outRec));
  in.code = pes::OutOfPlayCode(0.0f, pesFieldHalfWidth + 0.05f, kBallRadius_m);  // back in play
  ASSERT_TRUE(pes::DetectOutOfPlay(&in, &inRec));
  EXPECT_FALSE(pes::JudgeActsOnOutOfPlay(inRec)) << "a flip back to in play must not move the judge";
}
