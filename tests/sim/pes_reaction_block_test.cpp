// PES 2021's step 9 block B factor writer (FUN_140503430) and its helpers: the
// sine and angle helpers, the motion vector, the closest point, and ComputeBlock9's
// branches over synthetic facts. The constants are the install's stock values; the
// install test below reads them from the exe and the sine table from the image.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "onthepitch/pesreactionblock.hpp"
#include "pes/exe.hpp"

namespace {

using pes::reactionblock::Block9Inputs;
using pes::reactionblock::CardFacts;
using pes::reactionblock::ComputeBlock9;
using pes::reactionblock::Constants;
using pes::reactionblock::PlayerFacts;
using pes::reactionblock::Tables;
using pes::reactionblock::Vec3;

// The install's values (stock) and a sine table built from its formula. Class 1
// has PES bit 4 (flag80), class 2 has bit 5 (R13 and the card-zero test).
Tables StockTables() {
  Tables t;
  Constants& k = t.k;
  k.turnPeriod = 360.0f;
  k.turnPeriodNeg = -360.0f;
  k.halfTurn = 180.0f;
  k.halfTurnNeg = -180.0f;
  k.quarterTurn = 90.0f;
  k.tableScale = 512.0f;
  k.degPerRad = 57.295776f;
  k.twoPiRad = 6.2831855f;
  k.one = 1.0f;
  k.minusOne = -1.0f;
  k.half = 0.5f;
  k.atanCubic = 0.33333334f;
  k.piOver4 = 0.7853982f;
  k.atanMidBase = 0.98279375f;
  k.piOver2 = 1.5707964f;
  k.minusPiOver2 = -1.5707964f;
  k.pi = 3.1415927f;
  k.minusPi = -3.1415927f;
  k.atanSlope = 1.5f;
  k.closestEps = 1.1920929e-07f;
  k.drawFloat = 54.0f;
  k.drawInt = 54;
  k.speedScale = 0.6944444f;
  k.speedK1 = 60.0f;
  k.speedK2 = 1000.0f;
  k.ramp1Low = 15.0f;
  k.ramp1Top = 20.0f;
  k.ramp1Step = 5.0f;
  k.ramp2Low = 10.0f;
  k.ramp2High = 22.0f;
  k.ramp2Div = 12.0f;
  k.forcedScale = 3.5f;
  k.forcedLow = 7.0f;
  k.p5Bound = 50.0f;
  k.plainBound = 45.0f;
  k.block0Low = 0.3f;
  k.block0Div = 0.90000004f;
  k.block0High = 1.2f;
  k.tailFull = 70.0f;
  k.tailZero = 140.0f;
  k.tailDiv = -70.0f;
  k.tailGate = 0.2f;
  k.dlScale = 0.4f;
  k.pFull = 25.0f;
  k.pDiv = -20.0f;
  const double quarter = std::acos(-1.0) / 2.0;
  for (size_t i = 0; i < t.sine.size(); ++i) {
    t.sine[i] = static_cast<float>(std::sin(static_cast<double>(i) * quarter / 512.0));
  }
  t.sine[512] = 1.0f;
  t.classes.words[0] = 0u;
  t.classes.words[1] = 1u << 4;
  t.classes.words[2] = 1u << 5;
  return t;
}

// The inputs of the default case: slot 3 on the pitch, no holder (99), no card,
// the player at the origin at rest, the anchor at (0.6, 0, 0.3), zero ball vectors.
Block9Inputs Base() {
  Block9Inputs in;
  in.slot = 3;
  in.holder = 99;
  in.teamSubSlot = 99;
  in.live.pos = Vec3{0.0f, 0.0f, 0.0f};
  in.anchor = Vec3{0.6f, 0.0f, 0.3f};
  in.reach = 10.0f;

  return in;
}

// A player who moves with velocity (0, 0, z) in mode 0 (the velocity is the motion).
Block9Inputs Moving(float z) {
  Block9Inputs in = Base();
  in.live.mode = 0;
  in.live.vel = Vec3{0.0f, 0.0f, z};
  return in;
}

// The velocity that makes Lv = 194.4 * z equal to `lv` (Lv = 54*60*60/1000 * |Q|).
float ZForLv(float lv) { return lv / 194.4f; }

// The ground-plane distance band the block-B test cases use for the ramp: the
// anchor at dd = 0.6 from the closest point of a player moving straight along +z.
constexpr float kSpeedUnit = 0.1f;

// The ball records the block reads: a unit vector at `deg` degrees (atan2 convention:
// the angle from +z toward +x).
Vec3 AtDegrees(float deg) {
  const double rad = static_cast<double>(deg) * std::acos(-1.0) / 180.0;
  return Vec3{static_cast<float>(std::sin(rad)), 0.0f, static_cast<float>(std::cos(rad))};
}

void ExpectBlock(const pes::reactionblock::Block9Result& r, float b0, float b1, float b2,
                 float b3, float b4, float tol = 1e-4f) {
  EXPECT_NEAR(r.block[0], b0, tol) << "block[0]";
  EXPECT_NEAR(r.block[1], b1, tol) << "block[1]";
  EXPECT_NEAR(r.block[2], b2, tol) << "block[2]";
  EXPECT_NEAR(r.block[3], b3, tol) << "block[3]";
  EXPECT_NEAR(r.block[4], b4, tol) << "block[4]";
}

// ---- the helpers ----------------------------------------------------------

TEST(PesReactionBlockHelpers, SineIsTheQuarterWaveTableWithItsWraps) {
  const Tables t = StockTables();
  using pes::reactionblock::Sin;
  EXPECT_NEAR(Sin(0.0f, 1.0f, t), 0.0f, 1e-6f);
  EXPECT_NEAR(Sin(90.0f, 1.0f, t), 1.0f, 1e-6f);
  EXPECT_NEAR(Sin(180.0f, 1.0f, t), 0.0f, 1e-6f);
  EXPECT_NEAR(Sin(270.0f, 1.0f, t), -1.0f, 1e-6f);
  EXPECT_NEAR(Sin(-90.0f, 1.0f, t), -1.0f, 1e-6f);
  EXPECT_NEAR(Sin(30.0f, 1.0f, t), 0.5f, 2e-5f);
  EXPECT_NEAR(Sin(100.0f, 1.0f, t), 0.98480775f, 2e-5f);  // reflected about 90
  EXPECT_NEAR(Sin(45.0f, 2.0f, t), 2.0f * 0.70710678f, 3e-5f);
  EXPECT_FLOAT_EQ(Sin(30.0f, 2.0f, t), 2.0f * Sin(30.0f, 1.0f, t));  // linear in s
}

TEST(PesReactionBlockHelpers, AtanCoreMatchesTheSeriesAndBranches) {
  const Tables t = StockTables();
  using pes::reactionblock::AtanCore;
  EXPECT_NEAR(AtanCore(0.2f, t), std::atan(0.2f), 1e-4f);     // small: r - r^3/3
  EXPECT_NEAR(AtanCore(1.0f, t), 0.78539816f, 1e-6f);         // pi/4 branch
  EXPECT_NEAR(AtanCore(2.0f, t), std::atan(2.0f), 1e-4f);     // atan(1.5) branch
  EXPECT_NEAR(AtanCore(5.0f, t), std::atan(5.0f), 1e-4f);     // pi/2 branch
  EXPECT_FLOAT_EQ(AtanCore(-0.2f, t), -AtanCore(0.2f, t));    // odd
  EXPECT_FLOAT_EQ(AtanCore(-5.0f, t), -AtanCore(5.0f, t));
}

TEST(PesReactionBlockHelpers, Atan2AndAngleOfPickQuadrantsAndWraps) {
  const Tables t = StockTables();
  using pes::reactionblock::Atan2Rad;
  using pes::reactionblock::AngleOf;
  EXPECT_FLOAT_EQ(Atan2Rad(1.0f, 0.0f, t), 1.5707964f);
  EXPECT_FLOAT_EQ(Atan2Rad(-1.0f, 0.0f, t), -1.5707964f);
  EXPECT_NEAR(Atan2Rad(0.0f, -1.0f, t), 3.1415927f, 1e-6f);   // z < 0, x >= 0: +pi
  EXPECT_NEAR(Atan2Rad(-1.0f, -1.0f, t), -2.3561945f, 1e-5f);  // z < 0, x < 0: -pi
  EXPECT_NEAR(AngleOf(1.0f, 0.0f, t), 90.0f, 1e-3f);
  EXPECT_NEAR(AngleOf(0.0f, -1.0f, t), 180.0f, 1e-3f);
  EXPECT_NEAR(AngleOf(-1.0f, 0.0f, t), 270.0f, 1e-3f);        // negative radians wrap by 2pi
  EXPECT_FLOAT_EQ(AngleOf(0.0f, 1.0f, t), 0.0f);
  EXPECT_FLOAT_EQ(AngleOf(0.0f, 0.0f, t), 0.0f);              // zero vector: 0
}

TEST(PesReactionBlockHelpers, MotionIsVelocityUnlessDisplaced) {
  const Tables t = StockTables();
  using pes::reactionblock::Motion;
  PlayerFacts rest;
  rest.mode = 0;
  rest.vel = Vec3{0.1f, 0.2f, 0.3f};
  const Vec3 v = Motion(rest, 0.5f, t);
  EXPECT_FLOAT_EQ(v.x, 0.1f);
  EXPECT_FLOAT_EQ(v.y, 0.2f);
  EXPECT_FLOAT_EQ(v.z, 0.3f);

  PlayerFacts walk = rest;
  walk.mode = 1;  // the displacement along the facing (0 degrees: +z)
  const Vec3 d = Motion(walk, 0.5f, t);
  EXPECT_NEAR(d.x, 0.0f, 1e-6f);
  EXPECT_NEAR(d.y, 0.0f, 1e-6f);
  EXPECT_NEAR(d.z, 0.5f, 1e-6f);

  PlayerFacts fast = rest;
  fast.mode = 5;
  fast.vel = Vec3{0.1f, 0.0f, 0.1f};  // speed^2 = 0.02 < s^2 = 0.25: displaced
  const Vec3 fd = Motion(fast, 0.5f, t);
  EXPECT_NEAR(fd.z, 0.5f, 1e-6f);
  fast.vel = Vec3{1.0f, 0.0f, 1.0f};  // speed^2 = 2 >= 0.25: the velocity
  const Vec3 fv = Motion(fast, 0.5f, t);
  EXPECT_FLOAT_EQ(fv.x, 1.0f);
  EXPECT_FLOAT_EQ(fv.z, 1.0f);
}

TEST(PesReactionBlockHelpers, ClosestPointParameterAndDegenerateGuard) {
  float t = -1.0f;
  EXPECT_TRUE(pes::reactionblock::ClosestT(Vec3{0, 0, 0}, Vec3{10, 0, 0}, Vec3{5, 0, 3},
                                           1.1920929e-07f, &t));
  EXPECT_FLOAT_EQ(t, 0.5f);
  EXPECT_FALSE(pes::reactionblock::ClosestT(Vec3{1, 0, 1}, Vec3{1, 0, 1}, Vec3{5, 0, 3},
                                            1.1920929e-07f, &t));
}

// ---- ComputeBlock9 branches ----------------------------------------------

TEST(PesReactionBlock, DegenerateSegmentGivesBlockZeroOneAndRestIsZero) {
  const Tables t = StockTables();
  // At rest the displaced segment is a point: block[0] = 1, and every ramp is 0.
  ExpectBlock(ComputeBlock9(Base(), t), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

TEST(PesReactionBlock, DistanceRampFromTheAnchorToTheClosestPoint) {
  const Tables t = StockTables();
  // A player walking +z at 0.1: the segment is (0,0,5.4); the anchor (0.6, 0, 0.3)
  // projects to (0, 0, 0.3), dd = 0.6, so block[0] = (0.6 - 0.3)/0.9 = 1/3.
  const auto r = ComputeBlock9(Moving(kSpeedUnit), t);
  EXPECT_NEAR(r.block[0], 1.0f / 3.0f, 1e-5f);
  // Lv = 194.4 * 0.1 = 19.44: block[1] = (19.44 - 15)/5 = 0.888.
  EXPECT_NEAR(r.block[1], 0.888f, 1e-4f);
}

TEST(PesReactionBlock, DistanceRampFloorsAtThreeTenthsAndCapsAtOneD2) {
  const Tables t = StockTables();
  Block9Inputs far = Moving(kSpeedUnit);
  far.anchor = Vec3{1.5f, 0.0f, 0.3f};  // dd = 1.5 >= 1.2
  EXPECT_FLOAT_EQ(ComputeBlock9(far, t).block[0], 1.0f);
  Block9Inputs near = Moving(kSpeedUnit);
  near.anchor = Vec3{0.2f, 0.0f, 0.3f};  // dd = 0.2 <= 0.3
  EXPECT_FLOAT_EQ(ComputeBlock9(near, t).block[0], 0.0f);
  Block9Inputs edge = Moving(kSpeedUnit);
  edge.anchor = Vec3{0.305f, 0.0f, 0.3f};  // dd = 0.305: (0.305 - 0.3)/0.9
  EXPECT_NEAR(ComputeBlock9(edge, t).block[0], 0.005f / 0.9f, 1e-4f);
}

TEST(PesReactionBlock, ForcedActionsGiveBlockZeroOne) {
  const Tables t = StockTables();
  Block9Inputs e = Moving(kSpeedUnit);
  e.live.action = 0x33;  // action 0x33 always gives 1
  EXPECT_FLOAT_EQ(ComputeBlock9(e, t).block[0], 1.0f);
  Block9Inputs forced = Moving(kSpeedUnit);
  forced.live.action = 0x35;  // action 0x35 forces the flag
  EXPECT_FLOAT_EQ(ComputeBlock9(forced, t).block[0], 1.0f);
}

TEST(PesReactionBlock, VariantOneSecondRampAndForcedBranch) {
  const Tables t = StockTables();
  // Lv = 16 -> block[1] = 0.2. Reach 3: tbl < s2 = 2*0.2 + 5 = 5.4, so
  // block[2] = (3 - 5.4)/(0.2 - 5.4) = 0.4615385.
  Block9Inputs in = Moving(ZForLv(16.0f));
  in.reach = 3.0f;
  const auto r = ComputeBlock9(in, t);
  EXPECT_NEAR(r.block[1], 0.2f, 1e-4f);
  EXPECT_NEAR(r.block[2], 2.4f / 5.2f, 1e-4f);

  // The forced flag (action 0x35) takes the other branch: rp = 0.2*3.5 + 1.5 = 2.2,
  // tbl < 7: block[2] = (3 - 7)/(2.2 - 7) = 0.8333333.
  Block9Inputs forced = in;
  forced.live.action = 0x35;
  const auto f = ComputeBlock9(forced, t);
  EXPECT_NEAR(f.block[1], 0.2f, 1e-4f);
  EXPECT_NEAR(f.block[2], 4.0f / 4.8f, 1e-4f);
}

TEST(PesReactionBlock, VariantTwoRampsBetweenTheTwoBands) {
  const Tables t = StockTables();
  // p5: Lv = 12.4 -> block[1] = (12.4 - 10)/12 = 0.2; s2 = 5.4, s1 = 21; tbl = 10
  // lies between them: block[2] = (10 - 21)/(5.4 - 21) = 0.7051282.
  Block9Inputs in = Moving(ZForLv(12.4f));
  in.p5 = true;
  in.reach = 10.0f;
  in.s4 = 20.0f;  // the p5 gate: s4 >= tbl gives g = 1
  const auto r = ComputeBlock9(in, t);
  EXPECT_NEAR(r.block[1], 0.2f, 1e-4f);
  EXPECT_NEAR(r.block[2], 11.0f / 15.6f, 1e-4f);
  EXPECT_NEAR(r.block[4], 16.0f, 1e-4f);  // (int)(54 * 0.3) = 16, times g = 1
}

TEST(PesReactionBlock, P5GateScalesTheSixteenByTheReachRatio) {
  const Tables t = StockTables();
  // s4 = 1 < tbl = 1.5 < s5 = 2: g = (1.5 - 2)/(1 - 2) = 0.5, block[4] = 16 * 0.5 = 8.
  Block9Inputs in = Moving(ZForLv(12.4f));
  in.p5 = true;
  in.reach = 1.5f;
  in.s4 = 1.0f;
  in.s5 = 2.0f;
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 8.0f, 1e-4f);
}

TEST(PesReactionBlock, DlOverridesBlockFourOnlyUnderP5) {
  const Tables t = StockTables();
  // Kind 2 with payload +0x45 set: DL holds, so under p5 block[4] = (int)(54*0.4) = 21.
  Block9Inputs in = Moving(ZForLv(12.4f));
  in.p5 = true;
  in.reach = 10.0f;
  in.s4 = 20.0f;
  in.kind = 2u;
  in.byte45 = 1;
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 21.0f, 1e-4f);
  // Without p5 the DL override is not reached: block[4] stays 0.
  in.p5 = false;
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 0.0f, 1e-4f);
}

TEST(PesReactionBlock, P6TermIsFiveTimesTheReachRampAndKeepsTheLarger) {
  const Tables t = StockTables();
  Block9Inputs in = Base();
  in.p6 = true;
  in.reach = 3.0f;  // h = 1 for reach <= 5: 5
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 5.0f, 1e-4f);
  in.reach = 22.0f;  // h = (22 - 25)/(-20) = 0.15: 0.75
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 0.75f, 1e-4f);
  in.reach = 30.0f;  // h = 0 above 25
  EXPECT_NEAR(ComputeBlock9(in, t).block[4], 0.0f, 1e-4f);
}

TEST(PesReactionBlock, BlockThreeRampFromTheVelocityAngleGap) {
  const Tables t = StockTables();
  // The gap |x6 - a1| = 30 (V1 at 30 degrees, V2 at 0): with r1 = 0, hh = 20 and
  // tt2 = 45, block[3] = (30 - 20)/(45 - 20) = 0.4.
  Block9Inputs in = Base();
  in.ballV1 = AtDegrees(30.0f);
  in.ballV2 = AtDegrees(0.0f);
  // The atan core is PES's three-term series, about 0.015 degrees off in this band
  // (block[3] moves by 0.0006): the tolerance covers that and nothing else.
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.4f, 2e-3f);

  // With r1 = 0.2 (Lv = 16): hh = 20 - 0.2*5 = 19, tt2 = 45 - 0.2*15 = 42.
  Block9Inputs moving = Moving(ZForLv(16.0f));
  moving.ballV1 = AtDegrees(30.0f);
  moving.ballV2 = AtDegrees(0.0f);
  EXPECT_NEAR(ComputeBlock9(moving, t).block[3], 11.0f / 23.0f, 2e-3f);

  // A gap above tt2 gives 1; a gap below hh gives 0.
  in.ballV1 = AtDegrees(60.0f);
  EXPECT_FLOAT_EQ(ComputeBlock9(in, t).block[3], 1.0f);
  in.ballV1 = AtDegrees(10.0f);
  EXPECT_FLOAT_EQ(ComputeBlock9(in, t).block[3], 0.0f);
}

TEST(PesReactionBlock, HolderCardOfClassFiveZeroesBlockThree) {
  const Tables t = StockTables();
  Block9Inputs in = Base();
  in.holder = 4;  // valid: the holder's card is the card test's subject
  in.ballV1 = AtDegrees(30.0f);  // would give 0.4
  CardFacts card;
  card.cls = 2;   // class bit 5
  card.b40 = 5;
  in.cardHolder = card;
  EXPECT_FLOAT_EQ(ComputeBlock9(in, t).block[3], 0.0f);
  card.b40 = 4;   // not the zero case
  in.cardHolder = card;
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.4f, 2e-3f);
}

TEST(PesReactionBlock, FlagEightyTakesTheSixtyWordOnlyWhenInRange) {
  const Tables t = StockTables();
  Block9Inputs in = Base();
  in.holder = 4;
  in.ballV1 = AtDegrees(0.0f);
  CardFacts card;
  card.cls = 1;  // class bit 4: flag80
  card.f60 = 30.0f;
  card.f11c = 35.0f;
  card.w1a = 0;
  card.w1c = 0;
  in.cardSub = card;
  // f60 = 30 is in range: x6 = 30, gap 30, block[3] = (30 - 20)/25 = 0.4.
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.4f, 1e-4f);
  // Bit 11 at +8 takes the eleven word: x6 = 35, block[3] = 0.6.
  card.bit11AtEight = true;
  in.cardSub = card;
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.6f, 1e-4f);
  // w1a > w1c also takes the eleven word.
  card.bit11AtEight = false;
  card.w1a = 1;
  in.cardSub = card;
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.6f, 1e-4f);
  // f60 beyond 180 takes the eleven word.
  card.w1a = 0;
  card.f60 = 200.0f;
  in.cardSub = card;
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.6f, 1e-4f);
}

TEST(PesReactionBlock, ClassFiveBitFourClearTakesTheElevenWordAndVariantTwo) {
  const Tables t = StockTables();
  Block9Inputs in = Base();
  in.holder = 4;
  in.reach = 10.0f;
  in.ballV1 = AtDegrees(0.0f);
  CardFacts card;
  card.cls = 2;   // bit 5 only: R13 with byte +0x38 in {3, 4}
  card.b38 = 3;
  card.f11c = 30.0f;
  in.cardSub = card;
  const auto r = ComputeBlock9(in, t);
  // Variant two: r2 = 0 (Lv = 0); s2 = 5 < 10 and 10 < s1 = 20: block[2] = (10 - 20)/(5 - 20).
  EXPECT_NEAR(r.block[1], 0.0f, 1e-4f);
  EXPECT_NEAR(r.block[2], 2.0f / 3.0f, 1e-4f);
  EXPECT_NEAR(r.block[3], 0.4f, 1e-4f);  // x6 = 30: gap 30 over (45 - 20)
}

TEST(PesReactionBlock, HolderSubstitutesOnlyForAValidSlot) {
  const Tables t = StockTables();
  Block9Inputs in = Base();
  in.holder = 99;        // invalid: the side's team slot stands in when the slot is valid
  in.teamSubSlot = 4;
  in.ballV1 = AtDegrees(0.0f);
  CardFacts card;
  card.cls = 1;
  card.f60 = 30.0f;
  in.cardSub = card;
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.4f, 1e-4f);

  // An invalid slot too: no substitute, so the card is ignored and x6 is V2's angle.
  Block9Inputs off = in;
  off.slot = 99;
  off.ballV1 = AtDegrees(30.0f);
  off.ballV2 = AtDegrees(0.0f);
  EXPECT_NEAR(ComputeBlock9(off, t).block[3], 0.4f, 2e-3f);
}

TEST(PesReactionBlock, TailScalesByTheAngleFactorInTheSixtyToOneFortyBand) {
  const Tables t = StockTables();
  // Holder facing 0 with table 200: the gap wraps to -160, rb = 160 >= 140, so the
  // tail runs. V2 at 100 degrees gives Y = 100: q = (100 - 140)/(-70) = 4/7.
  Block9Inputs in = Base();
  in.liveHolder = PlayerFacts{};
  in.holderSlotTable = 200.0f;
  in.ballV1 = AtDegrees(30.0f);   // base block[3] = 1 (gap 70 > tt2)
  in.ballV2 = AtDegrees(100.0f);
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 4.0f / 7.0f, 1e-4f);
}

TEST(PesReactionBlock, TailSignMatchedFactorIsTheTailGate) {
  const Tables t = StockTables();
  // Y = 150: q = 0 and rb = 160 >= 140, so the sign test runs. e1 = -160 (sign -1).
  // The anchor-to-sample direction at 200 degrees gives e2 = -160 (sign -1): equal,
  // so q = 0.2 and block[3] = 0.2 * 1.
  Block9Inputs in = Base();
  in.liveHolder = PlayerFacts{};
  in.holderSlotTable = 200.0f;
  in.ballV1 = AtDegrees(30.0f);
  in.ballV2 = AtDegrees(150.0f);
  const Vec3 dir = AtDegrees(200.0f);  // anchor - sample0 = dir
  in.ballSample0 = Vec3{in.anchor.x - dir.x, 0.0f, in.anchor.z - dir.z};
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.2f, 1e-4f);

  // Direction 0 gives e2 = 0 (sign +1): the signs differ, so q stays 0.
  const Vec3 zero = AtDegrees(0.0f);
  in.ballSample0 = Vec3{in.anchor.x - zero.x, 0.0f, in.anchor.z - zero.z};
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.0f, 1e-4f);
}

TEST(PesReactionBlock, TailNeedsTheHundredFortyBoundOnTheFacingGap) {
  const Tables t = StockTables();
  // rb = 139.5 is under 140, so the sign test is skipped even though the signs
  // would match (direction 100 degrees gives sign +1, as does e1 = 139.5).
  Block9Inputs in = Base();
  in.liveHolder = PlayerFacts{};
  in.holderSlotTable = 139.5f;
  in.ballV1 = AtDegrees(30.0f);
  in.ballV2 = AtDegrees(150.0f);
  const Vec3 dir = AtDegrees(100.0f);
  in.ballSample0 = Vec3{in.anchor.x - dir.x, 0.0f, in.anchor.z - dir.z};
  EXPECT_NEAR(ComputeBlock9(in, t).block[3], 0.0f, 1e-4f);
}

TEST(PesReactionBlock, ReadsTheInstallConstantsAndQuarterWaveTable) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set: the block-B constants come from the install";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto loaded = pes::reactionblock::LoadTables(*exe, &err);
  ASSERT_TRUE(loaded.has_value()) << err;
  const Tables want = StockTables();
  EXPECT_FLOAT_EQ(loaded->k.turnPeriod, want.k.turnPeriod);
  EXPECT_FLOAT_EQ(loaded->k.halfTurn, want.k.halfTurn);
  EXPECT_FLOAT_EQ(loaded->k.quarterTurn, want.k.quarterTurn);
  EXPECT_FLOAT_EQ(loaded->k.tableScale, want.k.tableScale);
  EXPECT_FLOAT_EQ(loaded->k.drawFloat, 54.0f);
  EXPECT_EQ(loaded->k.drawInt, 54);
  EXPECT_FLOAT_EQ(loaded->k.speedK2, want.k.speedK2);
  EXPECT_FLOAT_EQ(loaded->k.tailGate, want.k.tailGate);
  EXPECT_NEAR(loaded->sine[256], 0.70710678f, 1e-6f);  // sin(45 degrees)
  EXPECT_TRUE(std::any_of(loaded->classes.words.begin(), loaded->classes.words.end(),
                          [](uint32_t w) { return w != 0u; }));
}

// ---- the holder predicates (FUN_140a55d40, FUN_140479470) over the roster -----

// A roster whose entry 0 names `slot` with `value`; the other entries name slot 0.
pes::reactionblock::Roster OneEntry(int slot, int value, uint8_t b06 = 0, uint8_t b0d = 0,
                                    uint8_t b68 = 0) {
  pes::reactionblock::Roster r;
  r.entries[0].b06 = b06;
  r.entries[0].slot20 = slot;
  r.entries[0].b0d = b0d;
  r.entries[0].b68 = b68;
  r.values[0] = value;
  return r;
}

TEST(PesReactionBlockRoster, SideOfSlotIsTheSecondSideForElevenToTwentyOne) {
  using pes::reactionblock::SideOfSlot;
  EXPECT_EQ(SideOfSlot(0), 0);
  EXPECT_EQ(SideOfSlot(10), 0);
  EXPECT_EQ(SideOfSlot(11), 1);
  EXPECT_EQ(SideOfSlot(21), 1);
  EXPECT_EQ(SideOfSlot(22), 0);
  EXPECT_EQ(SideOfSlot(-1), 0);
}

TEST(PesReactionBlockRoster, SideEntryNeedsAValueAtMostTheLimitOnItsSide) {
  using pes::reactionblock::SideHasEntryAtMost;
  pes::reactionblock::Roster r = OneEntry(12, 1);
  EXPECT_TRUE(SideHasEntryAtMost(r, 1, 2));
  r.values[0] = 3;
  EXPECT_FALSE(SideHasEntryAtMost(r, 1, 2));
  r.values[0] = 1;
  for (int i = 1; i < pes::reactionblock::Roster::kEntries; ++i) r.entries[i].b06 = 1;  // only entry 0 counts
  EXPECT_FALSE(SideHasEntryAtMost(r, 0, 2));  // slot 12 is the second side
}

TEST(PesReactionBlockRoster, IndirectValueFourReadsTheEntryByte) {
  using pes::reactionblock::SideHasEntryAtMost;
  pes::reactionblock::Roster r = OneEntry(12, 4, 0, 0, 2);  // byte +0x68 = 2 <= 2
  EXPECT_TRUE(SideHasEntryAtMost(r, 1, 2));
  r.entries[0].b68 = 3;
  EXPECT_FALSE(SideHasEntryAtMost(r, 1, 2));
}

TEST(PesReactionBlockRoster, InvalidEntriesAreSkipped) {
  using pes::reactionblock::SideHasEntryAtMost;
  EXPECT_FALSE(SideHasEntryAtMost(OneEntry(12, 0, /*b06=*/1), 1, 2));
  EXPECT_FALSE(SideHasEntryAtMost(OneEntry(12, 0, 0, /*b0d=*/1), 1, 2));
  EXPECT_FALSE(SideHasEntryAtMost(OneEntry(22, 0), 1, 2));  // slot 22 names no player; side 0 entries are not side 1
}

TEST(PesReactionBlockRoster, FirstEntryForTheSlotDecidesItsFlag) {
  using pes::reactionblock::FirstEntryFlagClear;
  pes::reactionblock::Roster r;
  r.entries[0].slot20 = 7;
  r.entries[0].b0d = 1;  // the first match is flagged
  r.entries[5].slot20 = 7;
  r.entries[5].b0d = 0;  // a later match is clear: it does not count
  EXPECT_FALSE(FirstEntryFlagClear(r, 7));
  r.entries[0].b0d = 0;
  r.entries[5].b0d = 1;
  EXPECT_TRUE(FirstEntryFlagClear(r, 7));
  EXPECT_FALSE(FirstEntryFlagClear(r, 30));  // above 21
  EXPECT_FALSE(FirstEntryFlagClear(r, 9));   // no entry names slot 9
}

TEST(PesReactionBlockRoster, Action34IsForcedWhenTheSideOrTheSlotEntryFails) {
  const Tables t = StockTables();
  // The default roster names slot 0 on every entry: a free side-0 entry with value 0.
  Block9Inputs side0 = Moving(kSpeedUnit);
  side0.live.action = 0x34;
  EXPECT_NEAR(ComputeBlock9(side0, t).block[0], 1.0f / 3.0f, 1e-5f);  // not forced

  Block9Inputs side1 = Moving(kSpeedUnit);
  side1.slot = 12;  // side 1 has no free entry in the default roster
  side1.live.action = 0x34;
  EXPECT_FLOAT_EQ(ComputeBlock9(side1, t).block[0], 1.0f);  // forced

  Block9Inputs flagged = Moving(kSpeedUnit);
  flagged.live.action = 0x34;
  flagged.roster.entries[0].slot20 = 3;
  flagged.roster.entries[0].b0d = 0;  // the first entry for slot 3 is clear
  EXPECT_FLOAT_EQ(ComputeBlock9(flagged, t).block[0], 1.0f);  // forced by FirstEntryFlagClear

  Block9Inputs taken = flagged;
  taken.roster.entries[0].b0d = 1;  // flagged: not forced
  EXPECT_NEAR(ComputeBlock9(taken, t).block[0], 1.0f / 3.0f, 1e-5f);
}

}  // namespace
