// PES 2021's carrier image layer: the Incisive Run (COM 4) curve shot's
// evaluator, the Intention ratchet and the image manager's selection
// persistence (docs/re/pes-re-com-styles.md 4-6.4). Synthetic constants
// exercise the score's bases, the per-stat truncation, the floor and cap, the
// ratchet's branches and the dispatcher's order; one install test checks the
// loader against the user's own PES2021.exe through relations, not pinned
// numbers.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pescarrier.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

namespace {

// The install's own values for the fields these tests read (report 6.4 for the
// curve shot, 5 for the ratchet, 4 for the manager). Every other field of
// CarrierConstants is irrelevant here and stays zero.
pes::CarrierConstants ImageConstants() {
  pes::CarrierConstants k{};
  k.statFloor = 70.0f;
  k.statSpan = 29.0f;
  k.curveShotComBase = 30.0f;
  k.curveShotSkillBase = 30.0f;
  k.curveShotStatGain = 10.0f;
  k.curveShotFloor = 5.0f;
  k.curveShotCap = 70.0f;
  k.curveShotSeedK = 1300.0f;
  k.planRollSlot = 2;
  k.planRollModulus = 100;
  k.intentionClose_deg = 100.0f;
  k.intentionLead_m = 10.0f;
  k.touchPad_m = 2.0f;   // 0x142565604, mode 2's goal-line offset
  k.sfLimMin_m = 3.0f;   // 0x14258C264, mode 2's a-point divisor
  k.imageHalfLine = 0.5f;
  return k;
}

// A curve shot that clears every head gate, scored off the ramp's floor (so
// the stat term is zero) with neither the COM card nor the skill.
pes::CurveShotInput ClearedCurveShot(const pes::CarrierConstants& k) {
  pes::CurveShotInput in;
  in.intention = 3;
  in.trajectory_m = 0.0f;
  in.kickingPower = k.statFloor;
  in.finishing = k.statFloor;
  return in;
}

TEST(PesImageCurveShot, BasesAreTheComAndSkillLadder) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  // Neither trigger: the base is 0, which the floor lifts to 5.
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotFloor);
  // The COM 4 card adds 30 on its own, the skills byte's bit 7 another 30,
  // both 60.
  in.incisiveRun = true;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotComBase);
  in.incisiveRun = false;
  in.skill7 = true;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotSkillBase);
  in.incisiveRun = true;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotComBase + k.curveShotSkillBase);
}

TEST(PesImageCurveShot, EachStatAddsItsOwnTruncatedStep) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  // Both stats at the top: the ramp is 1, so 10 each, and the two are added to
  // the running integer, not to one float.
  in.kickingPower = in.finishing = 99.0f;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), 2.0f * k.curveShotStatGain);
  // A stat 2 over the floor adds (2/29)*10 = 0.6896..., which truncates to 0.
  // Two of those added to the base 60 (COM and skill) stay 60, where a single
  // term truncated once would give 61: the truncation is per stat.
  in = ClearedCurveShot(k);
  in.incisiveRun = true;
  in.skill7 = true;
  in.kickingPower = in.finishing = k.statFloor + 2.0f;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotComBase + k.curveShotSkillBase);
}

TEST(PesImageCurveShot, TheFloorAndTheCap) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  // The floor lifts the empty score to 5, and never lowers a real one.
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotFloor);
  // Both triggers with both stats at the top is 80 before the cap.
  in.incisiveRun = true;
  in.skill7 = true;
  in.kickingPower = in.finishing = 99.0f;
  EXPECT_FLOAT_EQ(pes::CurveShotScore(in, k), k.curveShotCap);
}

TEST(PesImageCurveShot, TheHeadGates) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  ASSERT_GE(pes::CurveShotScore(in, k), 0.0f);
  // The Intention must be at least 3 (0x14211afa1).
  in.intention = 2;
  EXPECT_LT(pes::CurveShotScore(in, k), 0.0f);
  in.intention = 3;
  // a2[3]+0xB40 must not be positive (0x14211afb9).
  in.trajectory_m = 0.001f;
  EXPECT_LT(pes::CurveShotScore(in, k), 0.0f);
  in.trajectory_m = 0.0f;
  // The team's full-attack flag must be clear (0x14211afc6).
  in.fullAttack = true;
  EXPECT_LT(pes::CurveShotScore(in, k), 0.0f);
  in.fullAttack = false;
  EXPECT_GE(pes::CurveShotScore(in, k), 0.0f);
}

TEST(PesImageCurveShot, TheSideCompare) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  // Both arms compare the self scalar against the two fields (0x14211afe6 and
  // 0x14211aff4) and accept only when the same one of the -1/+1 pair is
  // selected twice (0x14211b000). With +0x48 != 1 the second field is negated
  // (0x14211b028) to -eval+0x14.
  in.sideSelf = 0.0f;
  in.sideA = 1.0f;   // eval+0x14
  in.sideB = 2.0f;   // eval+0x20
  // flag 0: self > 2 is false while self > -1 is true, so the pair differs.
  EXPECT_LT(pes::CurveShotScore(in, k), 0.0f);
  in.sideSelf = 3.0f;
  EXPECT_GE(pes::CurveShotScore(in, k), 0.0f);
  // flag 1: both thresholds positive, so the same value rejects.
  in.sideFlag = true;
  in.sideSelf = 1.5f;
  EXPECT_LT(pes::CurveShotScore(in, k), 0.0f);
  in.sideSelf = 3.0f;
  EXPECT_GE(pes::CurveShotScore(in, k), 0.0f);
}

TEST(PesImageCurveShot, AdmissionIsTheRollAndTheFinalGate) {
  const pes::CarrierConstants k = ImageConstants();
  pes::CurveShotInput in = ClearedCurveShot(k);
  in.incisiveRun = true;
  in.skill7 = true;  // the score is 60
  // `CMP EBX,EAX; JNC` accepts while the draw is at or below the score.
  EXPECT_TRUE(pes::CurveShotAdmitted(in, 0, k));
  EXPECT_TRUE(pes::CurveShotAdmitted(in, 60, k));
  EXPECT_FALSE(pes::CurveShotAdmitted(in, 61, k));
  // The final gate: the skill alone admits, the COM card alone admits, and
  // with neither - the empty score is only the floor - nothing is adopted.
  in.skill7 = false;
  EXPECT_TRUE(pes::CurveShotAdmitted(in, 30, k));
  in.incisiveRun = false;
  EXPECT_FALSE(pes::CurveShotAdmitted(in, 0, k));
  // A head gate still rejects whatever the draw and the gate say.
  in = ClearedCurveShot(k);
  in.intention = 2;
  in.incisiveRun = true;
  EXPECT_FALSE(pes::CurveShotAdmitted(in, 0, k));
}

// --- the Intention ratchet (report 5, 0x1421197b0) ---

// An input that reaches the tail of the ratchet: no shoot band, no penalty
// line, no ball lead, no corridor. The lead term is
// `ballLead + k.intentionLead_m > ballLimit_m`, so with no lead the limit is
// itself the bar and the term cannot fire.
pes::IntentionInput Quiet(const pes::CarrierConstants& k) {
  pes::IntentionInput in;
  in.ballLimit_m = k.intentionLead_m;
  return in;
}

TEST(PesImageIntention, StateFourAbsorbsAndTheCloseWindowIsAStrictHundred) {
  const pes::CarrierConstants k = ImageConstants();
  EXPECT_EQ(pes::NextIntention(4, Quiet(k), k), 4);
  pes::IntentionInput in = Quiet(k);
  in.closeBand = true;
  in.headingDiff_deg = 99.9f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 4);
  // Wrapped: -99.9 is inside the window, -100 is not.
  in.headingDiff_deg = -99.9f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 4);
  in.headingDiff_deg = -100.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.headingDiff_deg = 350.0f;  // wraps to -10
  EXPECT_EQ(pes::NextIntention(1, in, k), 4);
  // The window only counts inside the close band.
  in = Quiet(k);
  in.headingDiff_deg = 10.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
}

TEST(PesImageIntention, ThePenaltyLineAndTheBallLeadRaiseItToThree) {
  const pes::CarrierConstants k = ImageConstants();
  pes::IntentionInput in = Quiet(k);
  in.penaltyLine_m = 36.0f;
  in.attackDot = 36.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);  // strictly greater
  in.attackDot = 36.1f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 3);
  // The ball lead: a5+0xb58 + 10 against a5+0xb3c.
  in = Quiet(k);
  in.ballLimit_m = 9.9f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 3);
  in.ballLimit_m = 10.0f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  in.ballLead_m = 0.1f;
  EXPECT_EQ(pes::NextIntention(1, in, k), 3);
  // Three and four absorb, so no lower state is reachable from them.
  EXPECT_EQ(pes::NextIntention(3, Quiet(k), k), 3);
  EXPECT_EQ(pes::NextIntention(3, in, k), 3);
}

TEST(PesImageIntention, TheLatchAndTheCorridor) {
  const pes::CarrierConstants k = ImageConstants();
  pes::IntentionInput in = Quiet(k);
  in.corridorGuard = true;
  in.corridorAnswered = true;
  in.corridorState1 = true;
  EXPECT_EQ(pes::NextIntention(1, in, k), 1);
  // The block's own gate failing leaves 0 (0x142119cbd).
  in.corridorState1 = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 0);
  // A query that does not answer latches at 2 (0x142119ad9, 0x142119c94).
  in.corridorState1 = true;
  in.corridorAnswered = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  // Without the guard (a6+4 >= 2) the block does not run at all.
  in.corridorAnswered = true;
  in.corridorGuard = false;
  EXPECT_EQ(pes::NextIntention(1, in, k), 2);
  // State 2 latches: the corridor cannot take it back down to 1.
  in.corridorGuard = true;
  EXPECT_EQ(pes::NextIntention(2, in, k), 2);
}

// --- the image manager (report 4, 0x14211e2d0) ---

TEST(PesImageSelection, CancelRunsBeforeAdmission) {
  pes::CarrierImageSelection sel;
  sel.id = pes::kPlanBreakthrough;
  std::vector<int> calls;
  pes::AdvanceImageSelection(
      sel, [&](int plan) { calls.push_back(-plan); return true; },
      [&](int plan) { calls.push_back(plan); return true; },
      [](int) { return pes::PlanImage{1, 0.0f}; });
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0], -pes::kPlanBreakthrough);     // the cancel test first
  EXPECT_EQ(calls[1], pes::kPlanPriorityOrder[0]);  // then the first admission
  EXPECT_EQ(sel.id, pes::kPlanPriorityOrder[0]);
}

TEST(PesImageSelection, AdmissionRunsOnlyWhenNothingIsSelected) {
  pes::CarrierImageSelection sel;
  sel.id = pes::kPlanLong;
  bool admitted = false;
  pes::AdvanceImageSelection(
      sel, [](int) { return false; }, [&](int) { admitted = true; return true; },
      [](int) { return pes::PlanImage{2, 0.0f}; });
  EXPECT_FALSE(admitted);
  EXPECT_EQ(sel.id, pes::kPlanLong);  // the selection persists
  // A cancel clears it, and then the admission gets its turn.
  pes::AdvanceImageSelection(
      sel, [](int) { return true; }, [&](int plan) { return plan == pes::kPlanCurveShot; },
      [](int) { return pes::PlanImage{1, 0.0f}; });
  EXPECT_EQ(sel.id, pes::kPlanCurveShot);
}

TEST(PesImageSelection, ThePriorityOrderDecides) {
  // The dispatcher walks 9, 8, 2, 3, 1, 4, 5, 6, 7: the cross shot is reached
  // after the plans in front of it have refused.
  pes::CarrierImageSelection sel;
  std::vector<int> tried;
  pes::AdvanceImageSelection(
      sel, [](int) { return false; },
      [&](int plan) {
        tried.push_back(plan);
        return plan == pes::kPlanCross;
      },
      [](int) { return pes::PlanImage{4, 0.0f}; });
  const std::vector<int> expected(pes::kPlanPriorityOrder,
                                  pes::kPlanPriorityOrder +
                                      sizeof(pes::kPlanPriorityOrder) / sizeof(int));
  EXPECT_EQ(tried, std::vector<int>(expected.begin(), expected.begin() + 7));
  EXPECT_EQ(sel.id, pes::kPlanCross);
  EXPECT_EQ(sel.mode, 4);
}

TEST(PesImageSelection, TheModeChangeRestartsTheFrameCounter) {
  pes::CarrierImageSelection sel;
  sel.id = pes::kPlanBreakthrough;
  sel.mode = 1;
  // The same mode keeps counting frames.
  pes::AdvanceImageSelection(
      sel, [](int) { return false; }, [](int) { return false; },
      [](int) { return pes::PlanImage{1, 0.0f}; });
  pes::AdvanceImageSelection(
      sel, [](int) { return false; }, [](int) { return false; },
      [](int) { return pes::PlanImage{1, 0.0f}; });
  EXPECT_EQ(sel.framesSinceChange, 2);
  // A mode change restarts it, and the new mode and heading are stored.
  pes::AdvanceImageSelection(
      sel, [](int) { return false; }, [](int) { return false; },
      [](int) { return pes::PlanImage{8, 45.0f}; });
  EXPECT_EQ(sel.framesSinceChange, 0);
  EXPECT_EQ(sel.mode, 8);
  EXPECT_FLOAT_EQ(sel.heading_deg, 45.0f);
}

TEST(PesImageSelection, TheBreakthroughCancel) {
  EXPECT_TRUE(pes::ImagePlanCancelled(pes::kPlanBreakthrough, 2, true));
  EXPECT_FALSE(pes::ImagePlanCancelled(pes::kPlanBreakthrough, 3, true));
  // ponytail: the other plans' cancel tests are not decoded, so a plan whose
  // own start conditions have gone is dropped.
  EXPECT_TRUE(pes::ImagePlanCancelled(pes::kPlanMiddleShot, 3, false));
  EXPECT_FALSE(pes::ImagePlanCancelled(pes::kPlanMiddleShot, 3, true));
}

TEST(PesImageSelection, ASelectedPlanHoldsWhileItsOwnGatesPass) {
  const pes::CarrierConstants k = ImageConstants();
  const pes::CurveShotInput curve = ClearedCurveShot(k);
  pes::LongInput longIn;
  longIn.intention = 1;
  longIn.engagement = 2;
  // The curve shot holds while its head gates pass; Intention 2 drops it. This
  // is the stand-in for its own cancel test, which is not decoded.
  EXPECT_TRUE(pes::PlanStillHolds(pes::kPlanCurveShot, false, curve, longIn, k));
  pes::CurveShotInput lowIntention = curve;
  lowIntention.intention = 2;
  EXPECT_FALSE(pes::PlanStillHolds(pes::kPlanCurveShot, false, lowIntention, longIn, k));
  // Long holds until its veto fires; its choice eligibility does not enter.
  EXPECT_TRUE(pes::PlanStillHolds(pes::kPlanLong, false, curve, longIn, k));
  pes::LongInput vetoed = longIn;
  vetoed.intention = 3;
  EXPECT_FALSE(pes::PlanStillHolds(pes::kPlanLong, false, curve, vetoed, k));
  // The rolled plans hold by their choice eligibility.
  EXPECT_TRUE(pes::PlanStillHolds(pes::kPlanCross, true, curve, longIn, k));
  EXPECT_FALSE(pes::PlanStillHolds(pes::kPlanCross, false, curve, longIn, k));
}

TEST(PesImageMode, ThePlanProducers) {
  const pes::CarrierConstants k = ImageConstants();
  // Breakthrough, MiddleShot and ShotPassFromGoal are always mode 1 (4.6),
  // Cross always 4 (4.7).
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanBreakthrough, 2, false, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanMiddleShot, 3, false, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanShotPassFromGoal, 3, false, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanCross, 1, false, 0.0f, k), 4);
  // CurveShot's is sticky: 1 once the ball is coming at him or he is past the
  // half-line, otherwise whatever the manager already had (4.10).
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanCurveShot, 2, true, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanCurveShot, 2, false,
                               pesFieldHalfLength * k.imageHalfLine + 0.5f, k),
            1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanCurveShot, 2, false,
                               pesFieldHalfLength * k.imageHalfLine, k),
            2);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanCurveShot, 4, false, 0.0f, k), 4);
  // Long and SideChange (0x14211e140) and PassThrough (0x14211e160) answer from
  // unit fields GF does not carry, so the mode the plan already had stands
  // (4.9, 4.11); BackDFLine's is constant 2 (0x141f05c10).
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanLong, 2, false, 0.0f, k), 2);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanLong, 1, false, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanSideChange, 8, false, 0.0f, k), 8);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanPassThrough, 1, false, 0.0f, k), 1);
  EXPECT_EQ(pes::PlanImageMode(pes::kPlanBackDFLine, 8, false, 0.0f, k), 2);
}

// The plan heading (ctx7+0xC0): FUN_14211E650 writes it every frame from the
// plan's mode and the carrier record. Mode 2 (Basic, the case at 0x14211E7AC)
// projects the carrier onto the segment a -> b, where a = (F0-2-F0/3, cz),
// b = (F0-2, cz) and the blend target is the goal centre (F0-2, 0), then blends
// b toward that target by the carrier's own |z| past the goal area. So the aim
// runs across the pitch and leans with the carrier's depth: it is not 0, and it
// is not the attack bearing (90/270) that the old stand-in used.
TEST(PesPlanHeading, ModeTwoAimsAcrossThePitchAndLeansWithTheCarriersDepth) {
  const pes::CarrierConstants k = ImageConstants();
  // On the centre line the three points are level in z, so the aim is exactly
  // the pitch's cross axis; the sign mirrors it.
  EXPECT_FLOAT_EQ(pes::PlanHeadingDeg(2, 0.0f, 0.0f, 1.0f, k), 90.0f);
  EXPECT_FLOAT_EQ(pes::PlanHeadingDeg(2, 0.0f, 0.0f, -1.0f, k), 270.0f);
  // 5 m off the centre line: inside the goal-area half-width (F5 = 9.16), so the
  // blend gain is 1 and the aim point is the goal centre. p = a = (F0-2-F0/3, 5)
  // = (33.04, 5), q = (50.56, 0), so the heading is atan2(17.52, -5) = 105.93.
  EXPECT_NEAR(pes::PlanHeadingDeg(2, 0.0f, -5.0f, 1.0f, k), 105.93f, 0.02f);  // GF y is -PES z: the carrier is at GF y = -5
  // A carrier inside the segment at 20 m depth exercises the blend and the
  // overshoot reflection together: t = (40-33.04)/17.52 = 0.3972, the blend is
  // 2*(20-9.16)/(34.06-9.16) = 0.8707, the gain is
  // (1 - (1-0.3972)^(1/0.8707))^0.8707 = 0.4901, and the aim point is
  // (50.56, 20*(1-0.4901)) = (50.56, 10.20), giving atan2(10.56, -9.80) = 132.9.
  EXPECT_NEAR(pes::PlanHeadingDeg(2, 40.0f, -20.0f, 1.0f, k), 132.9f, 0.1f);  // the depth mirrored in GF y
  // Past the touchline the blend saturates at 2: the aim is b pulled halfway
  // toward the goal centre, and the projection clamps at 2 with the reflection
  // folding the parameter back, so the heading reads 100.73.
  EXPECT_NEAR(pes::PlanHeadingDeg(2, 40.0f, -40.0f, 1.0f, k), 100.73f, 0.02f);  // the depth mirrored in GF y
}

// --- the install: read the constants out of the user's own exe ---

TEST(PesImageInstall, TheCurveShotAndRatchetConstantsAreTheGamesOwn) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadCarrierConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // The two base terms are different instructions with the same immediate.
  EXPECT_FLOAT_EQ(k->curveShotComBase, k->curveShotSkillBase);
  EXPECT_GT(k->curveShotComBase, 0.0f);
  // The window is a floor-first window: 5 below 70.
  EXPECT_LT(k->curveShotFloor, k->curveShotCap);
  EXPECT_FLOAT_EQ(k->curveShotFloor, k->middleShotFloor);
  EXPECT_FLOAT_EQ(k->curveShotCap, k->statFloor);
  // The stat gain is the one Cross's ramp uses, and the seed sits between
  // MiddleShot's and Cross's.
  EXPECT_GT(k->curveShotStatGain, 0.0f);
  EXPECT_LT(k->curveShotStatGain, k->curveShotComBase);
  EXPECT_GT(k->curveShotSeedK, k->middleShotSeedK);
  EXPECT_LT(k->curveShotSeedK, k->crossSeedK);
  // The ratchet: a 100 degree window past a 10 m lead, and the sticky mode's
  // half-line at half the field.
  EXPECT_GT(k->intentionClose_deg, 90.0f);
  EXPECT_LT(k->intentionClose_deg, 180.0f);
  EXPECT_GT(k->intentionLead_m, 0.0f);
  EXPECT_GT(k->imageHalfLine, 0.0f);
  EXPECT_LT(k->imageHalfLine, 1.0f);
}

// The player skeleton and every constants block the generic setup requires;
// the install test above reads the loader directly, this one needs a Match.
const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return nullptr;
    pes::PesVfs vfs;
    std::string err;
    if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
  }
  return rig ? &*rig : nullptr;
}

// PES clears the member through 0x14210e5f0 (which writes 1) whenever a player
// is put into a new situation, so a stale 4 never survives a restart.
TEST(PesImageIntention, ASituationResetPutsEveryPlayerBackToOne) {
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = 1;
  setup.awayTeamDbId = 2;
  setup.seed = 7;
  setup.durationMinutes = 2;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  ASSERT_TRUE(pestest::FillMissingPesConstants(&setup));
  Match match(new MatchData(1, 2), setup, Properties());
  std::vector<Player*> players;
  match.GetAllTeamPlayers(0, players);
  ASSERT_FALSE(players.empty());
  players.at(0)->SetPesIntention(4);
  ASSERT_EQ(players.at(0)->GetPesIntention(), 4);
  match.ResetSituation(Vector3(0, 0, 0));
  EXPECT_EQ(players.at(0)->GetPesIntention(), 1);
  SetDB(nullptr);
}

}  // namespace
