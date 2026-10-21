// Pure tests of the presser picker (src/sim/onthepitch/pespress.hpp) against
// synthetic constants, plus one install relation test. The synthetic values
// are arbitrary on purpose: the tests check the arithmetic, not PES's numbers.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <vector>


#include "onthepitch/pespress.hpp"
#include "onthepitch/teamAIcontroller.hpp"

namespace {

pes::PressConstants Synthetic() {
  pes::PressConstants c{};
  c.rampStart_deg = 20.0f;
  c.rampEnd_deg = 90.0f;
  c.rampSpan_deg = 70.0f;
  c.radiusBase_m = 4.0f;
  c.halfTurn_deg = 180.0f;
  c.fullTurn_deg = 360.0f;
  c.radToDeg = 57.2957763671875f;
  c.notSlotPenalty = 1000.0f;
  c.notRolePenalty = 5000.0f;
  c.carrierRampStart_m = 10.0f;
  c.carrierRampEnd_m = 20.0f;
  c.carrierRampSpan_m = 10.0f;
  c.pass2Lo = 2.0f;
  c.pass2Hi = 5.0f;
  c.pass2Div = -3.0f;
  c.offsetBase_deg = 7.0f;
  c.offsetSpan_deg = 3.0f;
  c.slot1Lo_deg = 30.0f;
  c.slot1Hi_deg = 120.0f;
  c.lowBand_deg = 40.0f;
  c.highBand_deg = 100.0f;
  c.slot2Band_deg = 80.0f;
  c.slot3Lo_deg = -30.0f;
  c.slot3Hi_deg = -120.0f;
  c.pass2DelayDistSq = 16.0f;
  c.pass2ZeroRoleDistSq = 81.0f;
  c.pass2SameCodeAdd = 8.0f;
  c.pass2ActionAdd = 4.0f;
  c.speedCut0_kmh = 0.05f;
  c.speedCut1_kmh = 8.0f;
  c.speedCut2_kmh = 13.5f;
  c.speedCut3_kmh = 22.0f;
  c.targetTickRate = 0.1f;
  c.targetTickBias = 0.5f;
  c.evalThetaBase_m = 10.0f;
  c.evalThetaFar_m = 20.0f;
  c.evalThetaZ_m = 16.0f;
  c.evalThetaB29_m = 14.0f;
  c.evalIncumbentTheta_m = 3.0f;
  c.evalIncumbentStep_m = 4.0f;
  c.evalBlockASpeedKmh = 5.0f;
  c.evalBlockAHeading_deg = 60.0f;
  c.evalBlockAAdd_m = 10.0f;
  c.evalWindowSec_s = 5.0f;
  c.evalThetaTwoLow_m = 6.0f;
  c.evalThetaTwoHigh_m = 12.0f;
  c.evalMode7Angle_deg = 50.0f;
  c.evalMode7IncAngle_deg = 40.0f;
  c.evalMode4Angle_deg = 100.0f;
  c.evalMode4Margin_m = 10.0f;
  c.coverNear_m = 2.5f;
  c.coverFar_m = 14.0f;
  c.coverOuter_m = 11.0f;
  c.coverCone_deg = 140.0f;
  c.gateFacing_deg = 60.0f;
  c.gateBallHeight_m = 1.5f;
  c.classFiveDist_m = 3.0f;
  c.classFiveKindTen_m = 5.0f;
  c.maintainReason5DistSq_m2 = 9.0f;
  c.maintainBearing_deg = 45.0f;
  return c;
}

constexpr float kEps = 1e-3f;

TEST(PressSort, StableAscendingByScore) {
  std::vector<pes::PressCandidate> b = {{3, 5.0f}, {7, 1.0f}, {9, 5.0f}, {2, 0.5f}};
  pes::SortPressBucket(b);
  ASSERT_EQ(b.size(), 4u);
  EXPECT_EQ(b[0].man, 2);
  EXPECT_EQ(b[1].man, 7);
  // Equal scores keep their input order.
  EXPECT_EQ(b[2].man, 3);
  EXPECT_EQ(b[3].man, 9);
}

TEST(PressBearing, CardinalDirectionsAtAtan2DxDz) {
  auto c = Synthetic();
  EXPECT_NEAR(pes::PressBearingDeg(c, 0, 0, 0, 5), 0.0f, kEps);     // +z
  EXPECT_NEAR(pes::PressBearingDeg(c, 0, 0, 5, 0), 90.0f, kEps);    // +x
  EXPECT_NEAR(pes::PressBearingDeg(c, 0, 0, 0, -5), 180.0f, kEps);  // -z
  EXPECT_NEAR(pes::PressBearingDeg(c, 0, 0, -5, 0), 270.0f, kEps);  // -x
}

TEST(PressBearing, CoincidentPointsGiveZero) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressBearingDeg(c, 3, 4, 3, 4), 0.0f);
}

TEST(PressWrap, IntoZeroToFullTurn) {
  auto c = Synthetic();
  EXPECT_NEAR(pes::PressWrapDeg(c, -10.0f), 350.0f, kEps);
  EXPECT_NEAR(pes::PressWrapDeg(c, 370.0f), 10.0f, kEps);
  EXPECT_NEAR(pes::PressWrapDeg(c, 360.0f), 0.0f, kEps);
}

TEST(PressSideAngle, FoldsAcrossTheSeamWithSign) {
  auto c = Synthetic();
  // 10 against 350 is +20 (the short way round); 350 against 10 is -20.
  EXPECT_NEAR(pes::PressSideAngleDeg(c, 10.0f, 350.0f), 20.0f, kEps);
  EXPECT_NEAR(pes::PressSideAngleDeg(c, 350.0f, 10.0f), -20.0f, kEps);
  EXPECT_NEAR(pes::PressSideAngleDeg(c, 90.0f, 0.0f), 90.0f, kEps);
}

TEST(PressAngleBand, WrappedBoundaryTest) {
  pes::PressConstants c = Synthetic();
  EXPECT_FALSE(pes::PressAngleOutsideBand(c, 170.0f, 10.0f));
  EXPECT_TRUE(pes::PressAngleOutsideBand(c, 175.0f, 10.0f));
  EXPECT_TRUE(pes::PressAngleOutsideBand(c, -175.0f, 10.0f));
  EXPECT_FALSE(pes::PressAngleOutsideBand(c, 190.0f, 10.0f));
  EXPECT_TRUE(pes::PressAngleOutsideBand(c, 5.0f, 10.0f));
}

TEST(PressSpeed, FramesPerSecondTimesSixtyTimesSixtyOverKm) {
  pes::PressConstants c = Synthetic();
  c.framesPerSec = 54.0f;
  c.secPerMinute = 60.0f;
  c.metresPerKm = 1000.0f;
  EXPECT_NEAR(pes::PressSpeedKmh(c, 1.0f), 194.4f, kEps);
  EXPECT_NEAR(pes::PressSpeedKmh(c, 0.0f), 0.0f, kEps);
}

TEST(PressCarrierRamp, ZeroInsideStartOneAtEnd) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressCarrierRamp(c, 5.0f), 0.0f);
  EXPECT_NEAR(pes::PressCarrierRamp(c, 15.0f), 0.5f, kEps);
  EXPECT_EQ(pes::PressCarrierRamp(c, 30.0f), 1.0f);
}

TEST(PressOffset, RampFromRootScore) {
  auto c = Synthetic();
  // At or below the low edge the offset is base plus full span.
  EXPECT_NEAR(pes::PressOffsetDeg(c, 1.0f), 7.0f + 3.0f, kEps);
  // At or above the high edge it is the base alone.
  EXPECT_NEAR(pes::PressOffsetDeg(c, 6.0f), 7.0f, kEps);
  // Between the edges it is linear: (root - hi) / div, here 0.5.
  EXPECT_NEAR(pes::PressOffsetDeg(c, 3.5f), 7.0f + 3.0f * 0.5f, kEps);
}

TEST(PressManScore, InConeAddsTheConeTermOutsideAddsTheRampStart) {
  auto c = Synthetic();
  EXPECT_NEAR(pes::PressManScore(c, 4.0f, 0.0f, pes::kPresserRole, 0), 4.0f + 16.0f, kEps);
  EXPECT_NEAR(pes::PressManScore(c, 100.0f, 0.0f, pes::kPresserRole, 0), 100.0f + 20.0f, kEps);
}

TEST(PressManScore, RoleAndSlotPenaltiesStack) {
  auto c = Synthetic();
  float base = pes::PressManScore(c, 100.0f, 0.0f, pes::kPresserRole, 0);
  EXPECT_NEAR(pes::PressManScore(c, 100.0f, 0.0f, 0, 0) - base, 5000.0f, kEps);
  EXPECT_NEAR(pes::PressManScore(c, 100.0f, 0.0f, pes::kPresserRole, -1) - base, 1000.0f, kEps);
}

TEST(PressAngleCode, SlotZeroIsAlwaysZero) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressAngleCode(c, 0.0f, 0), 0);
  EXPECT_EQ(pes::PressAngleCode(c, 170.0f, 0), 0);
}

TEST(PressAngleCode, SlotOneTakesTheForwardBandOnly) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressAngleCode(c, 60.0f, 1), 1);   // inside (30, 120)
  EXPECT_EQ(pes::PressAngleCode(c, 20.0f, 1), 0);   // outside: falls through, |a| < 40
  EXPECT_EQ(pes::PressAngleCode(c, -60.0f, 1), 3);  // outside: falls through, a <= 0
}

TEST(PressAngleCode, SlotTwoAndThreeHaveTheirOwnBands) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressAngleCode(c, 90.0f, 2), 2);   // |a| > 80
  EXPECT_EQ(pes::PressAngleCode(c, 60.0f, 2), 1);   // |a| <= 80 falls through to the band code
  EXPECT_EQ(pes::PressAngleCode(c, -60.0f, 3), 3);  // inside (-120, -30)
}

TEST(PressAngleCode, NoSlotFallsThroughToTheBands) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressAngleCode(c, 10.0f, -1), 0);
  EXPECT_EQ(pes::PressAngleCode(c, 60.0f, -1), 1);
  EXPECT_EQ(pes::PressAngleCode(c, -60.0f, -1), 3);
  EXPECT_EQ(pes::PressAngleCode(c, 170.0f, -1), 2);
}
// Pass-2 fixture: the ball is at the origin, the carrier reference 15 m away, so
// the candidate radius is 15 m. Bearings are measured from +z.
struct Pass2Fixture {
  pes::PressConstants c = Synthetic();
  pes::PressPickInput in;
  pes::PressPickState st;
  Pass2Fixture() {
    in.ballX = 0;
    in.ballZ = 0;
    in.refX = 0;
    in.refZ = -15;
    in.referenceDeg = 0;
    in.onField.fill(true);
  }
  // Places man `m` at `bearingDeg` from the ball, `dist` metres away.
  void Place(int m, float bearingDeg, float dist) {
    const float r = bearingDeg * 3.14159265358979f / 180.0f;
    in.posX[m] = dist * std::sin(r);
    in.posZ[m] = dist * std::cos(r);
  }
};

TEST(PressPass2, FirstUseRecordsTheCodeAndAddsNothing) {
  Pass2Fixture f;
  f.Place(3, 60.0f, 5.0f);
  auto out = pes::RunPresserPicker(f.c, f.in, {{3, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].man, 3);
  EXPECT_NEAR(out[0].score, 1.0f, kEps);
  EXPECT_EQ(f.st.codeTaken[1], 1);
  EXPECT_EQ(f.st.pressCode[3], 1);
  EXPECT_FALSE(f.st.zeroCodeFlag);
}

TEST(PressPass2, ZeroCodeFarManIsSkippedWithoutAPresser) {
  Pass2Fixture f;
  f.Place(5, 0.0f, 11.0f);  // code 0, dist^2 121 > rampStart 20
  EXPECT_TRUE(pes::RunPresserPicker(f.c, f.in, {{5, 1.0f}}, f.st).empty());
}

TEST(PressPass2, ZeroCodeFarManKeptWhenTheTeamHasAPresser) {
  Pass2Fixture f;
  f.in.presserFlag = true;
  f.Place(5, 0.0f, 11.0f);
  auto out = pes::RunPresserPicker(f.c, f.in, {{5, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(f.st.pressCode[5], 0);
  EXPECT_TRUE(f.st.zeroCodeFlag);
}

TEST(PressPass2, PresserRoleIsExemptFromTheZeroCodeSkip) {
  Pass2Fixture f;
  f.Place(5, 0.0f, 11.0f);
  f.in.role[5] = pes::kPresserRole;
  EXPECT_EQ(pes::RunPresserPicker(f.c, f.in, {{5, 1.0f}}, f.st).size(), 1u);
}

TEST(PressPass2, SameCodeTakenAddsEight) {
  Pass2Fixture f;
  f.Place(3, 60.0f, 5.0f);
  f.Place(4, 60.0f, 6.0f);
  auto out = pes::RunPresserPicker(f.c, f.in, {{3, 1.0f}, {4, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_NEAR(out[0].score, 1.0f, kEps);
  EXPECT_NEAR(out[1].score, 9.0f, kEps);
  EXPECT_EQ(f.st.pressCode[4], -1);  // no reassignment
}

TEST(PressPass2, DifferentCodeReassignsAndAddsFour) {
  Pass2Fixture f;
  f.Place(3, 60.0f, 5.0f);
  f.Place(4, 95.0f, 5.0f);  // code 1; +offset moves it to code 2
  auto out = pes::RunPresserPicker(f.c, f.in, {{3, 1.0f}, {4, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_NEAR(out[1].score, 5.0f, kEps);
  EXPECT_EQ(f.st.pressCode[4], 2);
}

TEST(PressPass2, ZeroChosenCodeFarFromTheBallAddsEightAndKeepsCode) {
  Pass2Fixture f;
  f.Place(3, 60.0f, 5.0f);
  f.Place(6, 45.0f, 10.0f);  // code 1; -offset moves it to code 0
  auto out = pes::RunPresserPicker(f.c, f.in, {{3, 1.0f}, {6, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_NEAR(out[1].score, 9.0f, kEps);
  EXPECT_EQ(f.st.pressCode[6], -1);
}

TEST(PressPass2, DelayManTakesHisSlotAsCode) {
  Pass2Fixture f;
  f.in.presserFlag = true;
  f.Place(3, 0.0f, 5.0f);   // takes code 0
  f.Place(7, 0.0f, 3.0f);   // DELAY man in slot 2, dist^2 9 <= 16
  f.in.action[7] = 0x33;
  f.in.pressSlotMan[2] = 7;
  auto out = pes::RunPresserPicker(f.c, f.in, {{3, 1.0f}, {7, 1.0f}}, f.st);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_NEAR(out[1].score, 1.0f, kEps);  // DELAY: no action add
  EXPECT_EQ(f.st.pressCode[7], 2);
}

TEST(PressPass2, ExcludedOffPitchAndOutsideTheRadiusAreDropped) {
  Pass2Fixture f;
  f.Place(9, 60.0f, 5.0f);
  f.Place(10, 60.0f, 5.0f);
  f.Place(8, 0.0f, 16.0f);  // radius 15
  f.in.excludedMan = 9;
  f.in.onField[10] = false;
  EXPECT_TRUE(pes::RunPresserPicker(f.c, f.in, {{9, 1.0f}, {10, 1.0f}, {8, 1.0f}}, f.st).empty());
}

TEST(PressPass1, BucketsOnlyTheSidesOnFieldMenSortedByScore) {
  Pass2Fixture f;
  f.in.onField.fill(false);
  f.in.role.fill(pes::kPresserRole);
  f.in.pressSlotMan = {{0, 1, 11, 12}};
  f.Place(0, 0.0f, 3.0f);   // distSq 9 inside radius 4: score 9 + rampStart-cone 16 = 25
  f.Place(1, 0.0f, 5.0f);   // distSq 25 outside radius: score 25 + rampStart 20 = 45
  f.Place(11, 0.0f, 1.0f);  // the other side: not in this bucket
  f.in.onField[0] = f.in.onField[1] = f.in.onField[11] = true;
  auto b = pes::RunPresserPass1(f.c, f.in, 0);
  ASSERT_EQ(b.size(), 2u);
  EXPECT_EQ(b[0].man, 0);
  EXPECT_NEAR(b[0].score, 25.0f, kEps);
  EXPECT_EQ(b[1].man, 1);
  EXPECT_NEAR(b[1].score, 45.0f, kEps);
}

TEST(PressPass1, NonPresserRoleAndNoSlotAddTheirPenalties) {
  Pass2Fixture f;
  f.in.onField.fill(false);
  f.in.role.fill(pes::kPresserRole);
  f.in.role[1] = 0;  // not the presser role: +notRolePenalty (5000)
  f.Place(0, 0.0f, 3.0f);
  f.Place(1, 0.0f, 5.0f);
  f.in.onField[0] = f.in.onField[1] = true;
  f.in.pressSlotMan = {{0, -1, -1, -1}};  // man 1 has no slot: +notSlotPenalty (1000)
  auto b = pes::RunPresserPass1(f.c, f.in, 0);
  ASSERT_EQ(b.size(), 2u);
  EXPECT_NEAR(b[1].score, 45.0f + 5000.0f + 1000.0f, kEps);
}

TEST(PressExcludedMan, FirstOnFieldManWithAZeroFlagOfTheSide) {
  std::array<bool, 22> on{};
  // man 11 (slot 0) is off the field: its zero flag must not make it eligible.
  on[13] = true;  // slot 2, flag 0: returned
  on[15] = true;
  std::array<int, 11> flag{};
  flag.fill(1);
  flag[0] = 0;
  flag[2] = 0;
  EXPECT_EQ(pes::PressExcludedMan(1, on, flag), 13);
}

TEST(PressExcludedMan, NoEligibleManGivesTheSentinel) {
  std::array<bool, 22> on{};
  on[12] = true;
  std::array<int, 11> flag{};
  flag.fill(1);
  EXPECT_EQ(pes::PressExcludedMan(1, on, flag), pes::kPressNoMan);
}

TEST(PressWriterWord, TheSecondPresserSurvivesTheResetAndTheWinnerIsPressed) {
  EXPECT_EQ(pes::PressWriterWord(7, 3, 7), pes::kPressState);
  EXPECT_EQ(pes::PressWriterWord(3, 3, 7), pes::kPressState);
  EXPECT_EQ(pes::PressWriterWord(0, 3, 7), pes::kNothingState);
}

TEST(PressWriterWord, NoWinnerStillKeepsTheSecondPresser) {
  EXPECT_EQ(pes::PressWriterWord(7, pes::kPressNoMan, 7), pes::kPressState);
  EXPECT_EQ(pes::PressWriterWord(2, pes::kPressNoMan, 7), pes::kNothingState);
}

TEST(PressWriterWord, WithoutASecondPresserOnlyTheWinnerIsPressed) {
  EXPECT_EQ(pes::PressWriterWord(7, 3, pes::kPressNoMan), pes::kNothingState);
  EXPECT_EQ(pes::PressWriterWord(3, 3, pes::kPressNoMan), pes::kPressState);
}

TEST(PressTeamState, SetActionStoresRoleAndStampsRing) {
  pes::TeamPressState s{};
  s.SetAction(3, 0x34, 100);
  EXPECT_EQ(s.role[3], 0x34);
  EXPECT_EQ(s.actionRing[3].slots[1].id, 0x34);
  s.SetAction(3, 0x34, 200);  // same action as head: role rewritten, stamp unchanged
  EXPECT_EQ(s.role[3], 0x34);
  EXPECT_EQ(s.actionRing[3].slots[1].last, 100u);
}

TEST(PressStampRing, SameIdAsHeadIsANoOp) {
  pes::PressStampRing r{};
  pes::PressStampPush(r, 5, 10);  // slot 0 is empty (first 0 < 10): head -> 1
  pes::PressStampPush(r, 5, 20);  // same id as head: no-op
  EXPECT_EQ(r.head, 1);
  EXPECT_EQ(r.slots[1].last, 10u);
}

TEST(PressStampRing, NewerHeadAdvancesAndWraps) {
  pes::PressStampRing r{};
  for (int i = 0; i < 5; ++i) pes::PressStampPush(r, i + 1, 10u * (i + 1));
  EXPECT_EQ(r.head, 1);  // 5 pushes past an empty slot 0: wrapped, lands on slot 1
  EXPECT_EQ(r.slots[1].id, 5);
  EXPECT_EQ(r.slots[0].id, 4);
}

TEST(PressStampRing, EqualHeadTimeIsOverwrittenInPlace) {
  pes::PressStampRing r{};
  pes::PressStampPush(r, 1, 50);  // head -> 1
  pes::PressStampPush(r, 2, 50);  // old first 50 is not < 50: overwrite slot 1
  EXPECT_EQ(r.head, 1);
  EXPECT_EQ(r.slots[1].id, 2);
}


TEST(PressRole, BitTestAndPastTheTableGivesFalse) {
  pes::PressRoleTable t{};
  t.bits[2] = (1u << pes::kRoleBitA) | (1u << pes::kRoleBitJostle);
  EXPECT_TRUE(pes::PressRoleHas(t, 2, pes::kRoleBitA));
  EXPECT_FALSE(pes::PressRoleHas(t, 2, pes::kRoleBitB));
  EXPECT_TRUE(pes::PressRoleHas(t, 2, pes::kRoleBitJostle));
  EXPECT_FALSE(pes::PressRoleHas(t, pes::kRoleCount, pes::kRoleBitA));
}

// Install relation test: the role table loads and every entry is a 32-bit flag word.
TEST(PressRoleTableLoader, LoadsNinetyFourEntriesFromTheImage) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto t = pes::LoadPressRoleTable(*exe, &err);
  ASSERT_TRUE(t.has_value()) << err;
  EXPECT_EQ(t->bits.size(), static_cast<size_t>(pes::kRoleCount));
  EXPECT_FALSE(pes::PressRoleHas(*t, pes::kRoleCount, pes::kRoleBitA));
}
// Fill fixture: every man passes the gates unless a test breaks one.
static pes::PressFillMan Passer(int code, float distSq) {
  pes::PressFillMan m;
  m.onField = true;
  m.inDistance = true;
  m.carrierTagged = true;
  m.teamFlagZero = true;
  m.gate479470 = true;
  m.angleCode = code;
  m.tableDistSq = distSq;
  return m;
}

TEST(PressCandidateFill, CountsPassersAndSetsFlags) {
  pes::PressConstants c = Synthetic();
  std::array<pes::PressFillMan, 11> mans{};
  mans[2] = Passer(0, 81.0f);  // distance 9 m: no flagA; code 0: flagB
  mans[5] = Passer(2, 9.0f);   // distance 3 m: flagA; code 2 marks slot 2
  mans[6].onField = false;     // off field: rejected
  mans[7] = Passer(1, 81.0f);
  mans[7].excluded = true;     // the excluded man: rejected
  pes::PressFillResult r = pes::PressCandidateFill(c, mans);
  EXPECT_EQ(r.count, 2);
  EXPECT_TRUE(r.flagA);
  EXPECT_TRUE(r.flagB);
  EXPECT_TRUE(r.slotSet[0]);
  EXPECT_TRUE(r.slotSet[2]);
  EXPECT_FALSE(r.slotSet[1]);
}

// FUN_14044e0d0 is sqrtf (ucrtbase RVA 0x26620) of the table's float d^2, and
// FUN_140639830 tests that distance against 4.0. Squared 15.9 is 3.987 m: a passer.
TEST(PressCandidateFill, FlagAIsDistanceNotSquaredDistance) {
  pes::PressConstants c = Synthetic();
  std::array<pes::PressFillMan, 11> mans{};
  mans[1] = Passer(1, 15.9f);   // sqrt 3.987 < 4: flagA
  EXPECT_TRUE(pes::PressCandidateFill(c, mans).flagA);
  mans[1] = Passer(1, 16.1f);   // sqrt 4.012 >= 4: no flagA
  EXPECT_FALSE(pes::PressCandidateFill(c, mans).flagA);
}

TEST(PressCandidateFill, LowIndexNeedsCarrierTagOrStampedSet) {
  pes::PressConstants c = Synthetic();
  std::array<pes::PressFillMan, 11> mans{};
  mans[3] = Passer(1, 81.0f);
  mans[3].carrierTagged = false;  // no carrier tag, and not in the stamped set: rejected
  EXPECT_EQ(pes::PressCandidateFill(c, mans).count, 0);
  mans[3].inStampedSet = true;    // stamped set and not the A558 man: passes
  mans[3].notInA558 = true;
  EXPECT_EQ(pes::PressCandidateFill(c, mans).count, 1);
}

TEST(PressCandidateFill, HighIndexNeedsNoTeamFlagOnlyInCarrierBranch) {
  pes::PressConstants c = Synthetic();
  std::array<pes::PressFillMan, 11> mans{};
  // k = 10 is not > 10, so the carrier branch also needs the team flag to be zero.
  mans[10] = Passer(1, 81.0f);
  mans[10].teamFlagZero = false;
  EXPECT_EQ(pes::PressCandidateFill(c, mans).count, 0);
}

TEST(PressCandidateFill, NothingPassesGivesZero) {
  pes::PressConstants c = Synthetic();
  std::array<pes::PressFillMan, 11> mans{};
  pes::PressFillResult r = pes::PressCandidateFill(c, mans);
  EXPECT_EQ(r.count, 0);
  EXPECT_FALSE(r.flagA);
  EXPECT_FALSE(r.flagB);
}

TEST(PressConstantsLoader, ImageRelationsHold) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;
  auto c = pes::LoadPressConstants(*exe, &err);
  ASSERT_TRUE(c.has_value()) << err;
  EXPECT_FLOAT_EQ(c->fullTurn_deg, 2.0f * c->halfTurn_deg);
  EXPECT_FLOAT_EQ(c->rampSpan_deg, c->rampEnd_deg - c->rampStart_deg);
  EXPECT_FLOAT_EQ(c->carrierRampSpan_m, c->carrierRampEnd_m - c->carrierRampStart_m);
  EXPECT_FLOAT_EQ(c->pass2Div, c->pass2Lo - c->pass2Hi);
  EXPECT_FLOAT_EQ(c->slot3Hi_deg, -c->slot1Hi_deg);
  EXPECT_FLOAT_EQ(c->framesPerSec, 54.0f);
  EXPECT_FLOAT_EQ(c->secPerMinute, 60.0f);
  EXPECT_FLOAT_EQ(c->metresPerKm, 1000.0f);
  // The chase additions: ordered cuts, a 21.5 m table, the k = 5 tick rule.
  EXPECT_LT(c->speedCut0_kmh, c->speedCut1_kmh);
  EXPECT_LT(c->speedCut1_kmh, c->speedCut2_kmh);
  EXPECT_LT(c->speedCut2_kmh, c->speedCut3_kmh);
  // table radius is the (18+7r) pick; no separate field used by the chase.
  EXPECT_EQ(pes::PressTargetTick(*c), 5);
}

TEST(PressPickRadius, RampLegs) {
  pes::PressConstants c = Synthetic();
  // root <= pass2Lo (2): full span; root >= pass2Hi (5): base only.
  EXPECT_NEAR(pes::PressPickRadius_m(c, 1.0f), 20.0f, kEps);
  EXPECT_NEAR(pes::PressPickRadius_m(c, 2.0f), 20.0f, kEps);
  EXPECT_NEAR(pes::PressPickRadius_m(c, 5.0f), 10.0f, kEps);
  EXPECT_NEAR(pes::PressPickRadius_m(c, 9.0f), 10.0f, kEps);
  // Between: (root - hi) / div = (3.5 - 5) / -3 = 0.5 -> 10 + 10*0.5.
  EXPECT_NEAR(pes::PressPickRadius_m(c, 3.5f), 15.0f, kEps);
}

TEST(PressTarget, BallNowOrOwnBase) {
  // Sample nearer the ball now than the base: the ball.
  EXPECT_TRUE(pes::PressTargetIsBall({1.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
  // Sample nearer the base: the base.
  EXPECT_FALSE(pes::PressTargetIsBall({9.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
  // Tie goes to the ball (<=).
  EXPECT_TRUE(pes::PressTargetIsBall({5.0f, 0.0f}, {0.0f, 0.0f}, {10.0f, 0.0f}));
}

TEST(PressTargetTick, FpsTimesRatePlusBias) {
  pes::PressConstants c = Synthetic();
  c.framesPerSec = 54.0f;
  // int(54 * 0.1 + 0.5) = int(5.9) = 5.
  EXPECT_EQ(pes::PressTargetTick(c), 5);
}

TEST(PressSpeedClass, CutLegs) {
  pes::PressConstants c = Synthetic();
  EXPECT_EQ(pes::PressSpeedClass(c, 0.0f), 0);
  EXPECT_EQ(pes::PressSpeedClass(c, 0.05f), 0);
  EXPECT_EQ(pes::PressSpeedClass(c, 0.06f), 1);
  EXPECT_EQ(pes::PressSpeedClass(c, 8.0f), 1);
  EXPECT_EQ(pes::PressSpeedClass(c, 8.01f), 2);
  EXPECT_EQ(pes::PressSpeedClass(c, 13.5f), 2);
  EXPECT_EQ(pes::PressSpeedClass(c, 13.51f), 3);
  EXPECT_EQ(pes::PressSpeedClass(c, 22.0f), 3);
  EXPECT_EQ(pes::PressSpeedClass(c, 22.01f), 4);
}

TEST(PressCommandSpeed, DistanceSlopeCappedOneBandAbove) {
  pes::PressConstants c = Synthetic();
  // Standing (class 0): capped at the class-1 top, 8 km/h = 2.22 m/s.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 11.0f, 0.0f, 2.6f), 8.0f / 3.6f, 0.01f);
  // Near the ball: the slope wins over the cap.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 0.5f, 0.0f, 2.6f), 0.5f * 2.6f, kEps);
  // Sprinting (class 4): no cap below the slope.
  EXPECT_NEAR(pes::PressCommandSpeed_ms(c, 11.0f, 25.0f, 2.6f), 11.0f * 2.6f, kEps);
}

}  // namespace

namespace {

// Eval fixture: the holder is man 11 (side 1) at the origin, and side 0 attacks +x (sign +1). Every man is
// active and far away unless a test moves him. Man 10 is the excluded man (X) at (0, -30): his bearing from the
// holder is 180 degrees, and his class 0 gives the mode-7 takeover LR of 1.
pes::PressEvalInput EvalFixture() {
  pes::PressEvalInput in;
  in.side = 0;
  in.sign = 1.0f;
  in.ref = 11;
  in.excluded = 10;
  in.mode = 7;
  in.men[11].active = true;
  for (int k = 0; k < 11; ++k) {
    in.men[k].active = true;
    in.men[k].x = 40.0f;
    in.men[k].z = 0.0f;
    in.slots[k].roleClass = 1;
  }
  in.men[10].x = 0.0f;
  in.men[10].z = -30.0f;
  in.slots[10].roleClass = 0;
  return in;
}

// Baseline maintain inputs: every step passes, and the stamp window keeps the presser. Each test changes one input.
pes::PressMaintainInput KeptPresser() {
  pes::PressMaintainInput in;
  in.inPlay = true;
  in.slot = 3;
  in.man = 3;
  in.tableWord = pes::kPressAction;
  in.holderNow = 7;
  in.holderRecorded = 7;
  in.holderFromTouch = true;
  in.classFiveNow = pes::kPressNoMan;
  in.xMan = 5;
  in.roleClass = 1;
  in.windowFrames = 108;
  in.manToXDistSq = 100.0f;
  return in;
}

}  // namespace

TEST(PressHolderKind, GoalkeeperKindsAreNotHolders) {
  for (int kind : {0x11, 0x14, 0x50, 0x52, 0x57}) EXPECT_TRUE(pes::PressHolderKindExcluded(kind)) << kind;
  for (int kind : {0x10, 0x12, 0x51, 0x53, 0x58}) EXPECT_FALSE(pes::PressHolderKindExcluded(kind)) << kind;
}

TEST(PressHolder, PossessionManBeatsTheOtherCarrier) {
  EXPECT_EQ(pes::PressHolder(3, 15, 0x20), 3);
}

TEST(PressHolder, OtherCarrierStandsInWithoutAPossessionMan) {
  EXPECT_EQ(pes::PressHolder(pes::kPressNoMan, 15, 0x20), 15);
}

TEST(PressHolder, AnExcludedKindOrNoCarrierGivesNoHolder) {
  EXPECT_EQ(pes::PressHolder(pes::kPressNoMan, 15, 0x52), pes::kPressNoMan);
  EXPECT_EQ(pes::PressHolder(pes::kPressNoMan, pes::kPressNoMan, 0x20), pes::kPressNoMan);
}

TEST(PressClass, FlagFiveManWinsOverEveryLaterTest) {
  pes::PressClassInput in;
  in.flagFiveMan = true;
  in.flagDNone = true;
  in.testFun632850 = true;
  in.teamB716 = true;
  in.testFun632320 = true;
  EXPECT_EQ(pes::PressClass(in), 5);
}

TEST(PressClass, FlagDNoneIsSevenBeforeTheFourTest) {
  pes::PressClassInput in;
  in.flagDNone = true;
  in.testFun632850 = true;
  EXPECT_EQ(pes::PressClass(in), 7);
}

TEST(PressClass, FourThenSixThenTheBandTestThenSeven) {
  pes::PressClassInput in;
  in.testFun632850 = true;
  EXPECT_EQ(pes::PressClass(in), 4);
  in = {};
  in.teamB716 = true;
  EXPECT_EQ(pes::PressClass(in), 6);
  in = {};
  in.testFun632320 = true;
  EXPECT_EQ(pes::PressClass(in), 1);
  in = {};
  EXPECT_EQ(pes::PressClass(in), 7);
}

TEST(PressMaintain, BaselineIsKeptByTheStampWindow) {
  auto c = Synthetic();
  EXPECT_TRUE(pes::PressPresserKept(c, KeptPresser()));
}

TEST(PressMaintain, NotInPlayReleases) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.inPlay = false;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, OnlyAPressAssignmentInASlotBelowElevenIsKept) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.tableWord = 0x3a;  // MATCH_UP in the table, not PRESS
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in = KeptPresser();
  in.slot = 11;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, KeeperWithTheBallReleases) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.keeperHolds = true;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, HolderChangeReleases) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.holderNow = 8;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, ReasonFiveNeedsTheClassFiveMan) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.reason = 5;
  in.classFiveNow = 4;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in.classFiveNow = in.man;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, LiftedBallReleasesWhenThisSideMadeTheLastKick) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.lifted = true;
  in.teamKickMan = 2;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in.teamKickMan = pes::kPressNoMan;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, GroundedKickerNeedsHimAsTheMatchUpMan) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.teamKickMan = 2;
  EXPECT_FALSE(pes::PressPresserKept(c, in));  // no match-up man
  in.matchUp = 5;
  EXPECT_FALSE(pes::PressPresserKept(c, in));  // another man
  in.matchUp = 2;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, ReasonFiveReleasesFromThreeMetresOfTheHolder) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.reason = 5;
  in.classFiveNow = in.man;
  in.manToHolderDistSq = 9.0f;  // 3 m: released at the boundary
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in.manToHolderDistSq = 8.0f;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, OpponentHolderReleasesWhenTheMarkerIsNearerTheBall) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.holderFromTouch = false;
  in.xCloserToTeamPoint = true;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in.xCloserToTeamPoint = false;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, ReasonFourOrSevenNeedsTheBearingBandAndAKickerRole) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.reason = 4;
  in.bearingVsCoverDeg = 30.0f;  // inside the 45 degree band with flag 13 off: released
  EXPECT_FALSE(pes::PressPresserKept(c, in));
  in.bearingVsCoverDeg = 60.0f;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
  in.bearingVsCoverDeg = 30.0f;
  in.flag13 = true;  // flag 13 switches the band off
  EXPECT_TRUE(pes::PressPresserKept(c, in));
  in.flag13 = false;
  in.bearingVsCoverDeg = 60.0f;
  in.roleClass = 0;  // no kicker role: released
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, StaleStampFallsThroughToTheMarkerDistanceAtTheKeepRadius) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.stampLast = 200;  // 200 frames > 108: the stamp window no longer keeps him
  in.manToXDistSq = c.carrierRampEnd_m;  // exactly the keep radius: kept
  EXPECT_TRUE(pes::PressPresserKept(c, in));
  in.manToXDistSq = c.carrierRampEnd_m + 0.5f;
  EXPECT_FALSE(pes::PressPresserKept(c, in));
}

TEST(PressMaintain, WithoutAMarkerAStalePresserIsKept) {
  auto c = Synthetic();
  pes::PressMaintainInput in = KeptPresser();
  in.stampLast = 200;
  in.xMan = pes::kPressNoMan;
  in.manToXDistSq = 1000.0f;
  EXPECT_TRUE(pes::PressPresserKept(c, in));
}

TEST(PressEval, TheManWithinTheRadiusIsPickedAndTheModeIsWritten) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 4.0f;  // 4 m ahead of the holder
  const pes::PressEvalResult r = pes::PressEvalPick(c, in);
  EXPECT_EQ(r.man, 0);
  EXPECT_EQ(r.mode, 7);
}

TEST(PressEval, ManBeyondTheDefaultRadiusIsRefused) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 12.0f;  // 12 m beyond the 10 m default radius
  EXPECT_EQ(pes::PressEvalPick(c, in).man, pes::kPressNoMan);
}

TEST(PressEval, FlagThirteenWidensTheRadiusAndTheAcceptTwo) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.flag13 = true;
  in.men[0].x = 18.0f;  // 18 m inside the widened 20 m radius
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 0);
}

TEST(PressEval, AcceptTwoNeedsSixMetresUnlessTheAttackLevelIsThree) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 8.0f;  // inside the 10 m radius, outside the 6 m accept-two radius
  EXPECT_EQ(pes::PressEvalPick(c, in).man, pes::kPressNoMan);
  in.attackLevel = 3;  // accept-two radius becomes max(12, radius) = 12
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 0);
}

TEST(PressEval, ManBehindTheHolderIsRefusedUnlessFlagThirteen) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = -4.0f;  // behind the holder on the attack side
  EXPECT_EQ(pes::PressEvalPick(c, in).man, pes::kPressNoMan);
  in.flag13 = true;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 0);
}

TEST(PressEval, IncumbentGetsTheRadiusBonusAtAttackLevelThree) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.attackLevel = 3;
  in.men[0].x = 11.5f;  // beyond 10 m, inside 10 + 3 for the incumbent
  EXPECT_EQ(pes::PressEvalPick(c, in).man, pes::kPressNoMan);
  in.incumbent = 0;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 0);
}

TEST(PressEval, ExcludedManIsSkipped) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[10].x = 4.0f;  // the excluded man is nearest and in front
  in.men[10].z = 0.0f;
  in.mode = 1;  // no mode test, so only the G3 skip keeps the excluded man out
  in.men[1].x = 1.0f;
  in.men[1].z = 5.0f;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
}

TEST(PressEval, InactiveAndExcludedKindMenAreSkipped) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 4.0f;
  in.men[0].active = false;
  in.men[1].x = 1.0f;
  in.men[1].z = 5.0f;
  in.men[2].x = 4.5f;  // in front, but a keeper kind
  in.men[2].excludedKind = true;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
}

TEST(PressEval, FlagFifteenAndOrderTwentyFourAreSkipped) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 4.0f;
  in.men[0].flag15 = true;
  in.men[1].x = 1.0f;
  in.men[1].z = 5.0f;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
  in.men[0].flag15 = false;
  in.slots[0].orderTwentyFour = true;
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
}

TEST(PressEval, TheNearestPassingManWinsWhateverHisIndex) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 4.0f;  // 4 m
  in.men[1].x = 2.0f;  // 2 m: nearer, and later in the table
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
}

TEST(PressEval, ModeSevenNeedsABearingAwayFromTheExcludedMan) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.men[0].x = 1.0f;  // (1, -3): nearer, but only 18 degrees from the excluded man's bearing
  in.men[0].z = -3.0f;
  in.men[1].x = 4.0f;  // (4, 0): 90 degrees from the excluded man's bearing
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 1);
}

TEST(PressEval, BlockAWidensTheAcceptTwoWhenTheHolderRunsAtTheMan) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.phaseAggressive = true;  // radius 14, accept two 12
  in.levelClass = 6;
  in.blockAOppFree = true;
  in.refSpeedKmh = 6.0f;      // above 5
  in.men[0].x = 13.0f;        // 13 m: inside 14, outside 12
  in.refHeadingDeg = 0.0f;    // running 90 degrees off the man: no widening
  EXPECT_EQ(pes::PressEvalPick(c, in).man, pes::kPressNoMan);
  in.refHeadingDeg = 80.0f;   // running at the man: 10 degrees off, widened
  EXPECT_EQ(pes::PressEvalPick(c, in).man, 0);
}

TEST(PressEval, ModeFourNeedsTheManOverAHundredDegreesFromTheExcludedMan) {
  auto c = Synthetic();
  pes::PressEvalInput in = EvalFixture();
  in.mode = 4;
  in.men[0].x = 1.0f;  // (1, -3): 18 degrees from the excluded man's bearing: refused
  in.men[0].z = -3.0f;
  in.men[1].x = 5.0f;  // (5, 1): about 101 degrees from it: accepted
  in.men[1].z = 1.0f;
  const pes::PressEvalResult r = pes::PressEvalPick(c, in);
  EXPECT_EQ(r.man, 1);
  EXPECT_EQ(r.mode, 4);
}

TEST(PressEval, NoPassingManWritesNothing) {
  auto c = Synthetic();
  EXPECT_EQ(pes::PressEvalPick(c, EvalFixture()).man, pes::kPressNoMan);
}

TEST(PressGate, AHighBallOrAKickByThisSideFailsWhateverElseHolds) {
  auto c = Synthetic();
  pes::PressGateInput in;
  in.flag13 = true;
  in.cover = true;
  in.ballHeight_m = 2.0f;  // above 1.5 m
  EXPECT_FALSE(pes::PressGatePasses(c, in));
  in.ballHeight_m = 0.0f;
  in.teamKickValid = true;
  EXPECT_FALSE(pes::PressGatePasses(c, in));
}

TEST(PressGate, AnyCoverFlagPassesWhateverTheFacing) {
  auto c = Synthetic();
  pes::PressGateInput in;
  in.facingToAttack_deg = 170.0f;  // would fail on facing alone
  EXPECT_FALSE(pes::PressGatePasses(c, in));
  in.pressTimer = true;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
  in.pressTimer = false;
  in.flag13 = true;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
  in.flag13 = false;
  in.attackLevel = 3;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
  in.attackLevel = 0;
  in.classFive = true;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
  in.classFive = false;
  in.cover = true;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
}

TEST(PressGate, FacingWithinSixtyDegreesPassesAndBeyondFails) {
  auto c = Synthetic();
  pes::PressGateInput in;
  in.facingToAttack_deg = 60.0f;
  EXPECT_TRUE(pes::PressGatePasses(c, in));
  in.facingToAttack_deg = 61.0f;
  EXPECT_FALSE(pes::PressGatePasses(c, in));
}

TEST(PressCover, HoldsInsideTheNearBand) {
  auto c = Synthetic();
  EXPECT_TRUE(pes::PressCoverByHolder(c, 2.0f, 170.0f, 0.0f));  // any bearing, under 2.5 m
}

TEST(PressCover, FailsBeyondTheFarBandAndOutsideTheOuterRadius) {
  auto c = Synthetic();
  EXPECT_FALSE(pes::PressCoverByHolder(c, 15.0f, 0.0f, 0.0f));
  EXPECT_FALSE(pes::PressCoverByHolder(c, 12.0f, 0.0f, 0.0f));  // beyond the 11 m cone
}

TEST(PressCover, TheConeDecidesTheMiddleBand) {
  auto c = Synthetic();
  EXPECT_TRUE(pes::PressCoverByHolder(c, 8.0f, 10.0f, 0.0f));   // 10 degrees off the axis: inside
  EXPECT_FALSE(pes::PressCoverByHolder(c, 8.0f, 90.0f, 0.0f));  // 90 degrees off: outside
}

TEST(PressClassFive, TheNearestSideManWithinThreeMetresIsTheClassFiveMan) {
  auto c = Synthetic();
  pes::PressClassFiveInput in;
  in.holder = 11;
  in.cand = 3;
  in.clauseOk = true;
  in.roleClass = 2;
  in.ballDist_m = 2.0f;
  EXPECT_EQ(pes::PressClassFiveMan(c, in), 3);
  in.ballDist_m = 3.0f;  // strictly inside 3 m
  EXPECT_EQ(pes::PressClassFiveMan(c, in), pes::kPressNoMan);
}

TEST(PressClassFive, MatchUpKeeperAndTheClauseExcludeTheMan) {
  auto c = Synthetic();
  pes::PressClassFiveInput in;
  in.holder = 11;
  in.cand = 3;
  in.clauseOk = true;
  in.roleClass = 2;
  in.ballDist_m = 1.0f;
  in.matchUp = 3;
  EXPECT_EQ(pes::PressClassFiveMan(c, in), pes::kPressNoMan);
  in.matchUp = pes::kPressNoMan;
  in.keeperKind = true;
  EXPECT_EQ(pes::PressClassFiveMan(c, in), pes::kPressNoMan);
  in.keeperKind = false;
  in.roleClass = 0;  // the keeper class, and no kind 10
  EXPECT_EQ(pes::PressClassFiveMan(c, in), pes::kPressNoMan);
  in.roleClass = 2;
  in.clauseOk = false;
  EXPECT_EQ(pes::PressClassFiveMan(c, in), pes::kPressNoMan);
}

// The writer's gate on the ball (FUN_140536FC0): with no carrier the ball sample must lie within kPressBallGate_m of the
// MA ball copy, or nobody is picked, fresh or renewed. The pick is the top man within the pick radius, the same test for a
// fresh pick and a renewal. Geometry: the copy at the origin, the ball sample 10 m out (inside the gate), the man 5 m from
// the copy and 15 m from the ball. The radius is the synthetic one (10 m at the top root).
TEST(PressGate, ThePresserIsPickedAndKeptWhileTheGateHolds) {
  auto c = Synthetic();
  pes::PressPickInput in;
  in.ballX = 0.0f;
  in.ballZ = 0.0f;
  in.sampleX = 10.0f;
  in.sampleZ = 0.0f;
  in.posX[0] = -5.0f;
  in.posZ[0] = 0.0f;
  in.onField[0] = true;
  in.role[0] = pes::kPresserRole;
  in.posX[1] = -40.0f;
  in.posZ[1] = 0.0f;
  in.onField[1] = true;
  in.role[1] = pes::kPresserRole;
  EXPECT_TRUE(pes::PressBallGatePasses(in));
  EXPECT_EQ(pes::PressWinner(c, in, pes::RunPresserPass1(c, in, 0)), 0);  // a fresh pick
  in.pressSlotMan[0] = 0;                                                 // a renewal: he holds the slot
  EXPECT_EQ(pes::PressWinner(c, in, pes::RunPresserPass1(c, in, 0)), 0);
}

TEST(PressGate, NobodyIsPickedOrRenewedWhileTheGateRefuses) {
  auto c = Synthetic();
  pes::PressPickInput in;
  in.ballX = 0.0f;
  in.ballZ = 0.0f;
  in.sampleX = pes::kPressBallGate_m + 0.1f;  // just outside the gate, though the man is 5 m from the copy
  in.sampleZ = 0.0f;
  in.posX[0] = -5.0f;
  in.posZ[0] = 0.0f;
  in.onField[0] = true;
  in.role[0] = pes::kPresserRole;
  EXPECT_FALSE(pes::PressBallGatePasses(in));
  EXPECT_EQ(pes::PressWinner(c, in, pes::RunPresserPass1(c, in, 0)), -1);
  in.pressSlotMan[0] = 0;  // a renewal is refused by the same gate
  EXPECT_EQ(pes::PressWinner(c, in, pes::RunPresserPass1(c, in, 0)), -1);
  in.sampleX = pes::kPressBallGate_m;  // exactly on the gate: 169 is not exceeded, so it passes
  EXPECT_TRUE(pes::PressBallGatePasses(in));
}
