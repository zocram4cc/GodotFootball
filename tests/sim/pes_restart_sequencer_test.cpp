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
  pes::Spot spot = pes::ThrowInSpot(40.0f, -36.0f);
  EXPECT_NEAR(spot.x, 40.0f, 1e-4);
  EXPECT_LT(spot.z, -pesFieldHalfWidth);
  EXPECT_GT(spot.z, -pesFieldHalfWidth - 1.0f);

  spot = pes::ThrowInSpot(pesFieldHalfLength + 2.0f, 34.0f);
  EXPECT_NEAR(spot.x, pesFieldHalfLength - 1.0f, 1e-4);
}
// The throw-in's writer form (FUN_14050e770, VA 0x14050e770): the spot sits F1 + 0.25 outside the
// touchline and its x stays inside F0 - 1.0. Both offsets are .rdata floats read from the
// install, so the loader must expose the same bits the image holds at their VAs.
TEST(PesRestartSequencer, AThrowInSpotComesFromTheInstalledOffsets) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "spot constants unavailable";
  const pes::RestartSpotConstants& k = *install.spots;
  float outset = 0.0f, inset = 0.0f;
  ASSERT_TRUE(exe->ReadVa(0x142592228, sizeof outset, reinterpret_cast<uint8_t*>(&outset)));
  ASSERT_TRUE(exe->ReadVa(0x14256ae1c, sizeof inset, reinterpret_cast<uint8_t*>(&inset)));
  EXPECT_EQ(k.throwInLineOutset_m, outset);
  EXPECT_EQ(k.throwInEndInset_m, inset);
}

// The throw-in is taken from outside the touchline (F1 + the installed outset), with its x kept
// off the goal lines by the installed inset - not from on the line.
TEST(PesRestartSequencer, AThrowInIsTakenOutsideTheTouchlineByTheInstalledOutset) {
  const Install& install = InstallConstants();
  if (!install.spots) GTEST_SKIP() << "PES21_ROOT not set";
  const pes::RestartSpotConstants& k = *install.spots;

  const pes::Spot spot = pes::ThrowInSpot(40.0f, -36.0f, k);
  EXPECT_NEAR(spot.x, 40.0f, 1e-4);
  EXPECT_NEAR(spot.z, -(pesFieldHalfWidth + k.throwInLineOutset_m), 1e-4);

  const pes::Spot clamped = pes::ThrowInSpot(pesFieldHalfLength + 2.0f, 34.0f, k);
  EXPECT_NEAR(clamped.x, pesFieldHalfLength - k.throwInEndInset_m, 1e-4);
  EXPECT_NEAR(clamped.z, pesFieldHalfWidth + k.throwInLineOutset_m, 1e-4);
}

// The release gate, in frames from arming, is the live-measured wait (A5Live 9.3): each restart
// is released on the first frame at or past its gate, and a kick-off is not timed at all.
TEST(PesRestartSequencer, EachRestartIsReleasedOnItsMeasuredFrame) {
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_Corner, 364));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_Corner, 365));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_FreeKick, 354));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_FreeKick, 355));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_GoalKick, 237));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_GoalKick, 238));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_ThrowIn, 117));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_ThrowIn, 118));
  EXPECT_FALSE(pes::MayRelease(e_SetPiece_Penalty, 53));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_Penalty, 54));
  EXPECT_TRUE(pes::MayRelease(e_SetPiece_KickOff, 0));
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
