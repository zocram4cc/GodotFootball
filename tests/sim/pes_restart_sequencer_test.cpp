#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

#include "gametypes.hpp"
#include "onthepitch/foulsequence.hpp"
#include "onthepitch/pessequencer.hpp"
#include "onthepitch/pesstoppage.hpp"
#include "pes/exe.hpp"

// The restart sequencer (pessequencer.*): which restart a ball out becomes, the side and the
// spot it is taken from, the programme between the whistle and the armed restart, and the
// frame at which the taker may release it. The install's constants come from PES2021.exe; the
// install-backed tests skip without PES21_ROOT.
namespace {

struct Install {
  std::optional<pes::StoppageConstants> stoppage;
  std::optional<pes::RestartSpotConstants> spots;
};

const Install& InstallConstants() {
  static Install install;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err)) {
        install.stoppage = pes::LoadStoppageConstants(*exe, &err);
        install.spots = pes::LoadRestartSpotConstants(*exe, &err);
      }
    }
  }
  return install;
}

// GF's Team::GetSide(): team 0 is -1 and team 1 is +1. A side points at its own goal line.
constexpr int kHomeSide = -1;
constexpr int kAwaySide = 1;
constexpr unsigned long kStop_ms = 100000;

unsigned long Ms(float seconds) { return static_cast<unsigned long>(std::lround(seconds * 1000.0f)); }

}  // namespace

// A ball out over an end line. The last toucher's side points at his own goal line, so the
// attacking team's touch over the far line is a goal kick and the defender's touch a corner.
TEST(PesRestartSequencer, AnEndLineBallLastTouchedByTheAttackerIsAGoalKick) {
  EXPECT_EQ(pes::EndLineRestart(kAwaySide, -53.25f), e_SetPiece_GoalKick);
  EXPECT_EQ(pes::EndLineRestart(kHomeSide, 52.8f), e_SetPiece_GoalKick);
}

TEST(PesRestartSequencer, AnEndLineBallLastTouchedByTheDefenderIsACornerOnHisOwnLine) {
  EXPECT_EQ(pes::EndLineRestart(kHomeSide, -52.83f), e_SetPiece_Corner);
  EXPECT_EQ(pes::EndLineRestart(kAwaySide, 52.8f), e_SetPiece_Corner);
}

TEST(PesRestartSequencer, TheTeamThatDidNotTouchTheBallLastIsAwardedTheRestart) {
  EXPECT_EQ(pes::AwardedTeamAfterBallOut(0), 1);
  EXPECT_EQ(pes::AwardedTeamAfterBallOut(1), 0);
}

TEST(PesRestartSequencer, ACornerIsTakenFromTheFlagPulledInByTheInstalledInsets) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;
  EXPECT_GT(k.cornerInsetX_m, 0.0f);
  EXPECT_LT(k.cornerInsetX_m, 1.0f);
  EXPECT_GT(k.cornerInsetZ_m, 0.0f);
  EXPECT_LT(k.cornerInsetZ_m, 1.0f);

  const pes::Spot spot = pes::CornerSpot(-52.83f, 34.3f, k);
  EXPECT_NEAR(spot.x, -(pesFieldHalfLength - k.cornerInsetX_m), 1e-4);
  EXPECT_NEAR(spot.z, pesFieldHalfWidth - k.cornerInsetZ_m, 1e-4);
}

TEST(PesRestartSequencer, AGoalKickIsTakenOnTheGoalAreaCornerLineOnTheSideOfTheBall) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;

  pes::Spot spot = pes::GoalKickSpot(-53.2f, -12.0f, k);
  EXPECT_NEAR(spot.x, -(pesGoalAreaLineX + k.goalAreaPushOut_m), 1e-4);
  EXPECT_NEAR(spot.z, -k.goalAreaPushZ_m, 1e-4);

  spot = pes::GoalKickSpot(52.9f, 3.0f, k);
  EXPECT_NEAR(spot.x, pesGoalAreaLineX + k.goalAreaPushOut_m, 1e-4);
  EXPECT_NEAR(spot.z, k.goalAreaPushZ_m, 1e-4);
}

TEST(PesRestartSequencer, AThrowInIsTakenJustOutsideTheTouchlineAndInsideTheEndLine) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;
  EXPECT_GT(k.throwInLineOutset_m, 0.0f);
  EXPECT_LT(k.throwInLineOutset_m, 1.0f);
  EXPECT_GT(k.throwInEndInset_m, 0.0f);

  pes::Spot spot = pes::ThrowInSpot(40.0f, -36.0f, k);
  EXPECT_NEAR(spot.x, 40.0f, 1e-4);
  EXPECT_NEAR(spot.z, -(pesFieldHalfWidth + k.throwInLineOutset_m), 1e-4);

  spot = pes::ThrowInSpot(pesFieldHalfLength + 2.0f, 34.0f, k);
  EXPECT_NEAR(spot.x, pesFieldHalfLength - k.throwInEndInset_m, 1e-4);
}

// The throw-in carry spot (FUN_140791ec0, phase 1; docs/re/pes-re-throwin-choreography.md 3.2): the
// taker lifts the ball where the line from the restart point to the centre of the goal the side
// attacks crosses the carry line, 0.7 m outside the touchline on the restart's own side.
TEST(PesRestartSequencer, TheCarrySpotStandsOutsideTheTouchlineOnTheRestartsSide) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;
  EXPECT_GT(k.throwInCarryOutset_m, 0.0f);
  EXPECT_LT(k.throwInCarryOutset_m, 1.0f);

  const float touchline = pesFieldHalfWidth + k.throwInLineOutset_m;
  pes::Spot spot = pes::ThrowInCarrySpot(20.0f, touchline, +1, k);
  EXPECT_NEAR(spot.z, pesFieldHalfWidth + k.throwInCarryOutset_m, 1e-4);
  spot = pes::ThrowInCarrySpot(20.0f, -touchline, +1, k);
  EXPECT_NEAR(spot.z, -(pesFieldHalfWidth + k.throwInCarryOutset_m), 1e-4);
}

// The carry runs along the line from the restart point to the centre of the goal the side attacks,
// so the spot lies on that line whichever way the side attacks.
TEST(PesRestartSequencer, TheCarrySpotLiesOnTheApproachLineToTheAttackedGoalCentre) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;
  const float restartZ = pesFieldHalfWidth + k.throwInLineOutset_m;
  for (const float restartX : {-30.0f, 0.0f, 20.0f}) {
    for (const int attack : {+1, -1}) {
      const pes::Spot spot = pes::ThrowInCarrySpot(restartX, restartZ, attack, k);
      const float goalX = static_cast<float>(attack) * pesFieldHalfLength;
      // The distance from the spot to the line through the restart point and the goal centre.
      const float dx = goalX - restartX;
      const float dz = -restartZ;
      const float cross = (spot.x - restartX) * dz - (spot.z - restartZ) * dx;
      EXPECT_NEAR(std::fabs(cross) / std::hypot(dx, dz), 0.0f, 1e-3f)
          << "restart x " << restartX << ", attacking " << attack;
    }
  }
}

// The spot's x stays within the install's clamp of the restart's x (FUN_140791ec0, the 2 m test at
// 0x1407921c2). At the stock outset the approach line never leans that far, so this widens the outset
// for the test to put the clamp on the line.
TEST(PesRestartSequencer, TheCarrySpotsXIsClampedToTheInstalledWidthOfTheRestart) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  pes::RestartSpotConstants wide = *install.spots;
  wide.throwInCarryOutset_m = 5.0f;
  const float restartX = 0.0f;
  const pes::Spot spot =
      pes::ThrowInCarrySpot(restartX, pesFieldHalfWidth + wide.throwInLineOutset_m, +1, wide);
  EXPECT_NEAR(spot.x, restartX - wide.throwInCarryClamp_m, 1e-4);
}

// Near the end line the spot's x falls back to the restart's x, so the carry never runs past the goal
// line (FUN_140791ec0, the F0 minus inset test at 0x1407921f3).
TEST(PesRestartSequencer, TheCarrySpotFallsBackToTheRestartsXNearTheEndLine) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;
  const float restartX = -(pesFieldHalfLength - 0.2f);
  const pes::Spot spot =
      pes::ThrowInCarrySpot(restartX, pesFieldHalfWidth + k.throwInLineOutset_m, +1, k);
  EXPECT_NEAR(spot.x, restartX, 1e-4);
}

// The release gate for a timed restart, in frames from arming, is the live-measured wait (A5Live 9.3): each is released
// on the first frame at or past its gate, and a kick-off is not timed at all. A goal kick has no clock: its ladder
// releases it (pesgoalkick.hpp), however long that takes.
TEST(PesRestartSequencer, EachRestartIsReleasedOnItsMeasuredFrame) {
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_Corner, 364, false));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_Corner, 365, false));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_FreeKick, 354, false));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_FreeKick, 355, false));
  // A goal kick is released by its ladder (pesgoalkick.hpp), whatever the clock and the keeper's arrival say.
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_GoalKick, 100000, false)) << "a goal kick has no clock";
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_GoalKick, 100000, true));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_Penalty, 53, false));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_Penalty, 54, false));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_KickOff, 0, false));
}

// A throw-in has no frame count: its taker's arrival releases it, however long he took. A timer
// would release him before he arrives, and PES releases on the taker's reach and kick action
// (docs/re/pes-re-A5Judge.md 9.3).
TEST(PesRestartSequencer, AThrowInIsReleasedOnItsTakersArrivalAndNotBefore) {
  constexpr int kLongAfterArming = 10000;  // far past any frame count a timer could hold
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_ThrowIn, kLongAfterArming, false));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_ThrowIn, 0, true));
}

// The programme runs the whole stoppage bracket for a corner: the hold, the wipe that
// prepares the restart, then the teleport, the fade-in and the tail to the armed restart.
TEST(PesRestartSequencer, ACornerRunsTheWholeStoppageBracketToItsArming) {
  const Install& install = InstallConstants();
  if (!install.stoppage) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::StoppageConstants& k = *install.stoppage;

  const pes::Timing corner = pes::RunProgramme(pes::BuildProgramme(e_SetPiece_Corner), kStop_ms,
                                               k, false, /*holdKind=*/5);
  EXPECT_EQ(corner.prepareAt_ms, FoulSequence::RestartPrepareAt_ms(kStop_ms, k, false, 5));
  EXPECT_EQ(corner.armedAt_ms, FoulSequence::RestartTakeAt_ms(kStop_ms, k, false, 5));
}

// The goal kick and the throw-in fade in on the half-second fade (0xd), the corner on the full
// second (0xe). Both prepare on the same wipe as the corner.
TEST(PesRestartSequencer, AGoalKickAndAThrowInArmOnTheHalfSecondFade) {
  const Install& install = InstallConstants();
  if (!install.stoppage) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::StoppageConstants& k = *install.stoppage;

  const auto run = [&](e_SetPiece kind) {
    return pes::RunProgramme(pes::BuildProgramme(kind), kStop_ms, k, false, 5);
  };
  const pes::Timing corner = run(e_SetPiece_Corner);
  const pes::Timing goalKick = run(e_SetPiece_GoalKick);
  const pes::Timing throwIn = run(e_SetPiece_ThrowIn);

  EXPECT_EQ(goalKick.prepareAt_ms, corner.prepareAt_ms);
  EXPECT_EQ(throwIn.prepareAt_ms, corner.prepareAt_ms);
  EXPECT_EQ(corner.armedAt_ms - goalKick.armedAt_ms, Ms(k.fadeIn_s) - Ms(k.fadeHalf_s));
  EXPECT_EQ(throwIn.armedAt_ms, goalKick.armedAt_ms);
}

// A player down adds PES's extra hold to the stoppage, and the armed restart moves with it.
TEST(PesRestartSequencer, ADownedPlayerAddsHisHoldToTheStoppage) {
  const Install& install = InstallConstants();
  if (!install.stoppage) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::StoppageConstants& k = *install.stoppage;

  const pes::Timing up = pes::RunProgramme(pes::BuildProgramme(e_SetPiece_FreeKick), kStop_ms, k,
                                           false, /*holdKind=*/1);
  const pes::Timing down = pes::RunProgramme(pes::BuildProgramme(e_SetPiece_FreeKick), kStop_ms,
                                             k, true, /*holdKind=*/1);
  EXPECT_NEAR(static_cast<double>(down.armedAt_ms) - static_cast<double>(up.armedAt_ms),
              static_cast<double>(Ms(k.downedPlayerExtra_s)), 1.0);
}
