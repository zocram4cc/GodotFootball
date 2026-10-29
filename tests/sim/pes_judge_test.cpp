// PES 2021's advantage timing and the chief referee's restart marks (docs/re/pes-re-duels-fouls.md 3,
// docs/re/pes-re-protection.md 3.3). Synthetic constants exercise the formulas; the install tests check the loader
// against the user's own PES2021.exe through relations, not pinned numbers. The foul decision itself is PES's
// translated judge: pes_judge_decide_test.
#include <gtest/gtest.h>

#include <cstdlib>

#include "gametypes.hpp"
#include "onthepitch/pesjudge.hpp"

namespace {

pes::JudgeConstants Synthetic() {
  pes::JudgeConstants k{};
  k.whistleHold_s = 0.9f;
  k.advantageWindow_s = 3.0f;
  return k;
}

}  // namespace

TEST(PesJudgeAdvantage, DecisionOnlyInTheLastThirdOfTheHold) {
  const auto k = Synthetic();
  EXPECT_FALSE(pes::InAdvantageDecisionWindow(0.5f, k));
  EXPECT_TRUE(pes::InAdvantageDecisionWindow(0.6f, k));
  EXPECT_FALSE(pes::InAdvantageDecisionWindow(0.9f, k)) << "the hold is over: whistle";
}

TEST(PesJudgeAdvantage, NeverOnAPenaltyOrARedAndNotForABookedMansYellow) {
  EXPECT_TRUE(pes::ShouldPlayAdvantage(1, false, false, 3.0f, 5.0f));
  EXPECT_FALSE(pes::ShouldPlayAdvantage(1, true, false, 3.0f, 5.0f));
  EXPECT_FALSE(pes::ShouldPlayAdvantage(4, false, false, 3.0f, 5.0f));
  EXPECT_TRUE(pes::ShouldPlayAdvantage(3, false, false, 3.0f, 5.0f));
  EXPECT_FALSE(pes::ShouldPlayAdvantage(3, false, true, 3.0f, 5.0f));
  EXPECT_FALSE(pes::ShouldPlayAdvantage(1, false, false, 5.0f, 3.0f))
      << "the fouling team is nearer the ball";
}

TEST(PesJudgeAdvantage, ClockRunsDoubleWithTheBallAndPassesBurnASecond) {
  EXPECT_FLOAT_EQ(pes::AdvanceAdvantage(3.0f, 0.01f, false, false), 2.99f);
  EXPECT_FLOAT_EQ(pes::AdvanceAdvantage(3.0f, 0.01f, true, false), 2.98f);
  EXPECT_FLOAT_EQ(pes::AdvanceAdvantage(3.0f, 0.01f, true, true), 1.98f);
  EXPECT_FLOAT_EQ(pes::AdvanceAdvantage(0.5f, 0.01f, false, true), 0.0f);
}

// The chief referee's restart marks (docs/re/pes-re-protection.md 3.3). Every
// constant distinct, so a constant used in the wrong place moves a mark.
pes::RefereeSpotConstants SyntheticSpot() {
  pes::RefereeSpotConstants k{};
  k.kickOffBehind_m = 6.0f;
  k.kickOffAcross_m = 11.0f;
  k.throwInUpfieldGate_m = 2.0f;
  k.throwInTrail_m = 7.0f;
  k.throwInLeadDivisor = 4.0f;
  k.throwInLeadBase_m = 9.0f;
  k.throwInInset_m = 5.0f;
  k.goalKickX_m = -3.0f;
  k.goalKickDrop_m = 1.5f;
  k.cornerInset_m = 0.75f;
  k.cornerOut_m = 2.5f;
  k.penaltyAhead_m = 4.25f;
  k.penaltyOut_m = 5.25f;
  k.fkHalf = 0.5f;
  k.fkDeepX_m = -13.0f;
  k.fkOwnHalfOut_m = 1.25f;
  k.fkOwnBoxX_m = 2.5f;
  k.fkOwnBoxIn_m = 3.25f;
  k.fkWideDivisor = 4.0f;
  k.fkBoxCornerInset_m = 4.5f;
  k.fkBoxCornerOut_m = 2.25f;
  k.fkQuarter = 0.2f;
  k.fkEdgeInset_m = 3.75f;
  k.fkEdgeOut_m = 3.5f;
  k.fkWideOut_m = 1.75f;
  k.fkFarAcross_m = 9.5f;
  k.fkCentreAhead_m = 10.5f;
  k.fkCentreAcross_m = 8.5f;
  k.fkFlankAhead_m = 5.5f;
  k.fkFlankAcross_m = 14.5f;
  return k;
}

constexpr pes::FieldMarks SYNTHETIC_FIELD{50.0f, 10.0f, 30.0f, 20.0f};

void ExpectSpot(e_SetPiece kind, pes::PitchSpot restart, int dir, pes::PitchSpot want) {
  const auto got = pes::ChiefRefereeSpot(kind, restart, dir, SYNTHETIC_FIELD, SyntheticSpot());
  ASSERT_TRUE(got) << "restart (" << restart.x << ", " << restart.z << ")";
  EXPECT_FLOAT_EQ(got->x, want.x) << "restart (" << restart.x << ", " << restart.z << ")";
  EXPECT_FLOAT_EQ(got->z, want.z) << "restart (" << restart.x << ", " << restart.z << ")";
}

TEST(PesRefereeSpot, KickOffBehindAndAcrossFromTheKickers) {
  ExpectSpot(e_SetPiece_KickOff, {0, 0}, 1, {-6.0f, -11.0f});
  ExpectSpot(e_SetPiece_KickOff, {0, 0}, -1, {6.0f, 11.0f});
}

TEST(PesRefereeSpot, GoalKickDropIsNotMirrored) {
  ExpectSpot(e_SetPiece_GoalKick, {-45.0f, 3.0f}, 1, {3.0f, 8.5f});
  ExpectSpot(e_SetPiece_GoalKick, {-45.0f, 0.0f}, 1, {3.0f, 8.5f});  // PES's zero is positive
  ExpectSpot(e_SetPiece_GoalKick, {-45.0f, -3.0f}, 1, {3.0f, -11.5f});
}

TEST(PesRefereeSpot, CornerAndPenaltyTakeBothSignsFromTheSpotX) {
  ExpectSpot(e_SetPiece_Corner, {50.0f, -30.0f}, 1, {29.25f, -12.5f});
  ExpectSpot(e_SetPiece_Corner, {-50.0f, -30.0f}, -1, {-29.25f, 12.5f});
  ExpectSpot(e_SetPiece_Penalty, {39.0f, 0.0f}, 1, {34.25f, -15.25f});
}

TEST(PesRefereeSpot, ThrowInTrailsOrLeadsThenClampsAtTheBox) {
  ExpectSpot(e_SetPiece_ThrowIn, {20.0f, 34.0f}, 1, {13.0f, 15.0f});
  ExpectSpot(e_SetPiece_ThrowIn, {-20.0f, -34.0f}, 1, {-6.0f, -15.0f});
  ExpectSpot(e_SetPiece_ThrowIn, {-40.0f, -34.0f}, -1, {-25.0f, -10.0f});
  ExpectSpot(e_SetPiece_ThrowIn, {40.0f, 34.0f}, 1, {25.0f, 10.0f});
}

TEST(PesRefereeSpot, FreeKickInTheKickersOwnHalf) {
  ExpectSpot(e_SetPiece_FreeKick, {-10.0f, 5.0f}, 1, {13.0f, 11.25f});
  ExpectSpot(e_SetPiece_FreeKick, {-20.0f, -5.0f}, 1, {-2.0f, -11.25f});
  ExpectSpot(e_SetPiece_FreeKick, {-35.0f, 5.0f}, 1, {-2.5f, 6.75f});
}

TEST(PesRefereeSpot, FreeKickZonesInTheAttackingHalf) {
  ExpectSpot(e_SetPiece_FreeKick, {35.0f, 25.0f}, 1, {25.5f, -12.25f});   // 1
  ExpectSpot(e_SetPiece_FreeKick, {20.0f, -25.0f}, 1, {10.0f, -5.0f});    // 3
  ExpectSpot(e_SetPiece_FreeKick, {20.0f, 3.0f}, 1, {26.25f, -13.5f});    // 4
  ExpectSpot(e_SetPiece_FreeKick, {20.0f, -10.0f}, 1, {31.75f, -21.75f});  // 5
  ExpectSpot(e_SetPiece_FreeKick, {20.0f, 10.0f}, 1, {26.25f, -13.5f});   // 6
  ExpectSpot(e_SetPiece_FreeKick, {10.0f, 25.0f}, 1, {12.0f, 15.5f});     // 7
  ExpectSpot(e_SetPiece_FreeKick, {10.0f, -5.0f}, 1, {20.5f, -13.5f});    // 8
  ExpectSpot(e_SetPiece_FreeKick, {10.0f, 5.0f}, 1, {15.5f, -9.5f});      // 9
  ExpectSpot(e_SetPiece_FreeKick, {-20.0f, -10.0f}, -1, {-26.25f, 13.5f});  // 6, mirrored
}

TEST(PesRefereeSpot, FreeKickInsideTheBoxWidthLeavesTheRefereeWhereHeIs) {
  EXPECT_FALSE(pes::ChiefRefereeSpot(e_SetPiece_FreeKick, {35.0f, 5.0f}, 1, SYNTHETIC_FIELD,
                                     SyntheticSpot()));
}

TEST(PesJudgeInstall, LoadsFromTheExe) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadJudgeConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  EXPECT_GT(k->whistleHold_s, 0.0f);
  EXPECT_GT(k->advantageWindow_s, k->whistleHold_s);
}

TEST(PesRefereeSpotInstall, ReproducesTheLiveMarks) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadJudgeConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // PES's live FieldInfo (docs/re/pes-re-stadium-camera.md 2) and the live
  // restarts of docs/re/pes-re-setpiece-choreography.md 8.2 and
  // pes-re-protection.md 3.3, to the centimetre they were sampled at.
  constexpr pes::FieldMarks PES_FIELD{pesFieldHalfLength, pesGoalAreaHalfWidth,
                                      pesPenaltyAreaLineX, pesPenaltyAreaHalfWidth};
  constexpr float SAMPLE_TOLERANCE_M = 0.01f;
  const auto at = [&](e_SetPiece kind, pes::PitchSpot restart, int dir) {
    return *pes::ChiefRefereeSpot(kind, restart, dir, PES_FIELD, k->refereeSpot);
  };
  const auto kickOff = at(e_SetPiece_KickOff, {0, 0}, 1);
  EXPECT_NEAR(kickOff.x, -5.0f, SAMPLE_TOLERANCE_M);
  EXPECT_NEAR(kickOff.z, -10.0f, SAMPLE_TOLERANCE_M);
  const auto freeKick = at(e_SetPiece_FreeKick, {35.113f, -30.729f}, 1);
  EXPECT_NEAR(freeKick.x, 13.14f, SAMPLE_TOLERANCE_M);
  EXPECT_NEAR(freeKick.z, -6.71f, SAMPLE_TOLERANCE_M);
  const auto goalKick = at(e_SetPiece_GoalKick, {-47.06f, 5.0f}, 1);
  EXPECT_NEAR(goalKick.x, 4.0f, SAMPLE_TOLERANCE_M);
  EXPECT_NEAR(goalKick.z, 6.66f, SAMPLE_TOLERANCE_M);
  const auto corner = at(e_SetPiece_Corner, {-51.78f, 33.0f}, -1);
  EXPECT_NEAR(corner.x, -35.64f, SAMPLE_TOLERANCE_M);
  EXPECT_NEAR(corner.z, 11.16f, SAMPLE_TOLERANCE_M);
}
