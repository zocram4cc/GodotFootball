// PES 2021's period-end rules in the referee: the half length comes from the
// phase table, added time is the once-per-half linear formula, and the period
// changes only when the restart state has completed (report 2.1, 3.2, 5.4).

#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <memory>
#include <string>

#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/matchprogression.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesjudge.hpp"
#include "onthepitch/peskeeper.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesstoppage.hpp"
#include "onthepitch/pesrandom.hpp"
#include "onthepitch/pesclock.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int SHORT_MATCH_MINUTES = 2;
// The menu's floor of 5 minutes: a game-second is 54 frames x 5 / 90, three logic frames.
constexpr uint32_t kFramesPerGameSecond = 3;
uint32_t FramesIn(float gameSeconds) { return static_cast<uint32_t>(gameSeconds) * kFramesPerGameSecond; }

// The player skeleton comes from the user's PES install.
const pesrig::Rig* SharedRig() {
  static std::optional<pesrig::Rig> rig;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      pes::PesVfs vfs;
      std::string err;
      if (vfs.Mount(root, &err)) rig = pesrig::BuildRig(vfs, &err);
    }
  }
  return rig ? &*rig : nullptr;
}

// PES 2021's ball physics constants, from the user's install (docs/re/
// pes-re-ball-contact.md 6). The Match requires them like the clock's.
const pes::PesBallConstants* SharedBallConstants() {
  static std::optional<pes::PesBallConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      pes::PesVfs vfs;
      std::string err;
      if (vfs.Mount(root, &err)) {
        if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
          constants = pes::LoadPesBallConstants(*exe, vfs, &err);
      }
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's foul-decision constants (docs/re/pes-re-duels-fouls.md 2).
const pes::JudgeConstants* SharedJudgeConstants() {
  static std::optional<pes::JudgeConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadJudgeConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's generator constants (docs/re/pes-re-rng.md 1).
const pes::RngConstants* SharedRngConstants() {
  static std::optional<pes::RngConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadRngConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's goalkeeper deflect point and post-catch distribution target
// (docs/re/pes-re-keeper-port.md).
// PES 2021's restart timings (docs/re/pes-re-foul-choreography.md 9).
const pes::CarrierConstants* SharedCarrierConstants() {
  static std::optional<pes::CarrierConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadCarrierConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

const pes::StoppageConstants* SharedStoppageConstants() {
  static std::optional<pes::StoppageConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadStoppageConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

const pes::KeeperConstants* SharedKeeperConstants() {
  static std::optional<pes::KeeperConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadKeeperConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's stamina model (docs/re/pes-re-stamina-drain.md): the tank, the
// drain, the breath, the break bonus and the injury roll's constants.
const pes::StaminaConstants* SharedStaminaConstants() {
  static std::optional<pes::StaminaConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadStaminaConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's clock constants, read from the user's install (the time-up gate and
// the stoppage rule are PES's own: docs/re/pes-re-clock-live.md 8).
const pes::PesClockConstants* SharedClockConstants() {
  static std::optional<pes::PesClockConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      std::string err;
      if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
        constants = pes::LoadPesClockConstants(*exe, &err);
    }
  }
  return constants ? &*constants : nullptr;
}

#define REQUIRE_RIG()                                                                    \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  if (!SharedBallConstants())                                                            \
    GTEST_SKIP() << "constant_match.bin/exe: ball physics constants unavailable"; \
  if (!SharedStoppageConstants())                                               \
    GTEST_SKIP() << "PES2021.exe: stoppage constants unavailable"; \
  if (!SharedCarrierConstants())                                                 \
    GTEST_SKIP() << "PES2021.exe: carrier constants unavailable";

// A match in the given phase with PES's clock constants installed, and the
// simulation clock driven to `seconds` of elapsed game time.
std::unique_ptr<Match> MatchAtPhase(e_MatchPhase phase, float seconds, bool pesLoop = false) {
  blunted::Database db;
  static std::unique_ptr<blunted::Database> held;
  // The engine reads a process-wide database; keep it alive for the match.
  held = std::make_unique<blunted::Database>();
  if (!held->LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  SetDB(held.get());
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = SHORT_MATCH_MINUTES;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  setup.pesClockConstants = SharedClockConstants();
  if (!setup.pesClockConstants) return nullptr;
  setup.pesBallConstants = SharedBallConstants();
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  if (!setup.pesBallConstants) return nullptr;
  pestest::FillMissingPesConstants(&setup);
  Properties config;
  config.SetBool("pes_loop", pesLoop);
  auto match = std::make_unique<Match>(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config);
  match->SetMatchPhase(phase);
  match->StartPlay();
  match->AdvanceMatchFrames(FramesIn(seconds));
  return match;
}

}  // namespace

// PES 2021's period lifecycle in the match (docs/re/pes-re-clock-live.md 8): each
// period starts its own record, the referee blows when the time-up gate opens,
// and a stoppage moves the clock only through F.

TEST(PesPeriodLifecycle, EachTransitionStartsItsOwnPESPeriod) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2400.0f);
  ASSERT_TRUE(match);
  EXPECT_EQ(match->GetPesClock().Record().period, 1u);
  EXPECT_EQ(MatchProgression::GetNext(e_MatchPhase_1stHalf, false).nextPhase, e_MatchPhase_2ndHalf);
  match->SetMatchPhase(e_MatchPhase_2ndHalf);
  EXPECT_EQ(match->GetPesClock().Record().period, 3u);
  EXPECT_FLOAT_EQ(match->GetPesClock().PeriodClock(), 0.0f) << "the second half's record starts at zero";
  EXPECT_EQ(MatchProgression::GetNext(e_MatchPhase_2ndHalf, true).nextPhase, e_MatchPhase_1stExtraTime);
  match->SetMatchPhase(e_MatchPhase_1stExtraTime);
  EXPECT_EQ(match->GetPesClock().Record().period, 5u);
  match->SetMatchPhase(e_MatchPhase_2ndExtraTime);
  EXPECT_EQ(match->GetPesClock().Record().period, 7u);
  EXPECT_EQ(MatchProgression::GetNext(e_MatchPhase_2ndExtraTime, true).nextPhase, e_MatchPhase_Penalties);
  match->SetMatchPhase(e_MatchPhase_Penalties);
  EXPECT_EQ(match->GetPesClock().Record().period, 9u);
}

// The referee blows when the gate opens: 44:59 holds and 45:00 blows. Tier 0 earns
// no added time, so the length is the whistle, as it was before this port.
TEST(PesPeriodLifecycle, TheRefereeWhistlesWhenTheGateOpens) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2699.0f);
  ASSERT_TRUE(match);
  match->GetReferee()->Process();
  EXPECT_TRUE(match->IsInPlay()) << "44:59 blows nothing";
  match->AdvanceMatchFrames(FramesIn(1.0f));
  match->GetReferee()->Process();
  EXPECT_FALSE(match->IsInPlay()) << "45:00 is the time-up";
}

// A stoppage holds the stamp still and adds half a unit per frame to F, so the
// clock moves only through F while play is stopped.
TEST(PesPeriodLifecycle, AStoppageMovesTheClockOnlyThroughF) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 100.0f);
  ASSERT_TRUE(match);
  const float stamp = pes::PeriodStamp(match->GetPesClock().Record());
  match->StopPlay();
  match->AdvanceMatchFrames(FramesIn(10.0f));
  EXPECT_FLOAT_EQ(pes::PeriodStamp(match->GetPesClock().Record()), stamp);
  EXPECT_FLOAT_EQ(match->GetPesClock().Record().f,
                  pes::kStoppageUnitsPerFrame * FramesIn(10.0f));
}

// Under pes_loop the gate opens the time-up, but the judge blows only after its 60 s count
// (FUN_14050B9B0): the gate alone does not whistle.
TEST(PesPeriodLifecycle, PesLoopTheRefereeWaitsSixtySecondsPastTheGate) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2700.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  ASSERT_TRUE(match->GetPesClock().TimeUpGateOpen());
  for (int tick = 1; tick <= 54 * 60; ++tick) {
    match->GetReferee()->Process();
    ASSERT_TRUE(match->IsInPlay()) << "the 60 s count has not run out at tick " << tick;
  }
  match->GetReferee()->Process();
  EXPECT_FALSE(match->IsInPlay()) << "the tick after 60 game-seconds blows";
}

// A stoppage adds half a unit per frame to LIMIT under pes_loop (FUN_14044CE80 step 4).
TEST(PesPeriodLifecycle, PesLoopAStoppageAddsToLimit) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 100.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  match->StopPlay();
  match->AdvanceMatchFrames(FramesIn(100.0f));
  EXPECT_FLOAT_EQ(match->GetPesClock().Record().limit, pes::kStoppageUnitsPerFrame * FramesIn(100.0f));
}

// Under pes_loop the period clock runs every frame, so a stoppage reaches Match::Process: F and
// LIMIT take half a unit a frame there (FUN_14044CE80 step 4) and the stamp holds.
TEST(PesPeriodLifecycle, PesLoopAStoppageThroughProcessGrowsFAndLimit) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 100.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  match->StopPlay();
  const pes::PeriodRecord before = match->GetPesClock().Record();
  const float stamp = pes::PeriodStamp(before);
  constexpr int kTicks = 54;  // one game-second of frames
  for (int tick = 0; tick < kTicks; ++tick) match->Process();
  ASSERT_FALSE(match->IsInPlay()) << "the stoppage held through the ticks";
  const pes::PeriodRecord& after = match->GetPesClock().Record();
  EXPECT_FLOAT_EQ(after.f, before.f + kTicks * pes::kStoppageUnitsPerFrame);
  EXPECT_FLOAT_EQ(after.limit, before.limit + kTicks * pes::kStoppageUnitsPerFrame);
  EXPECT_FLOAT_EQ(pes::PeriodStamp(after), stamp);
}

// With pes_loop off the stoppage leaves LIMIT alone, as before.
TEST(PesPeriodLifecycle, WithoutPesLoopAStoppageLeavesLimitAlone) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 100.0f);
  ASSERT_TRUE(match);
  match->StopPlay();
  match->AdvanceMatchFrames(FramesIn(100.0f));
  EXPECT_FLOAT_EQ(match->GetPesClock().Record().limit, 0.0f);
}

// Under pes_loop the judge counts the ticks the ball is dead as well (FUN_14050C3E0 gates on the
// period only). 3239 dead ticks plus the first tick in play make 3240: the count is not past it.
TEST(PesPeriodLifecycle, PesLoopTheJudgeCountsDeadBallTicks) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2700.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  match->StopPlay();
  for (int tick = 0; tick < 54 * 60 - 1; ++tick) match->GetReferee()->Process();
  match->StartPlay();
  match->GetReferee()->Process();
  EXPECT_TRUE(match->IsInPlay()) << "3240 ticks of count is not past 60 game-seconds";
}

// 3241 dead ticks make the count past 60 game-seconds on a dead tick: the blow is held and the
// whistle follows on the first tick of play.
TEST(PesPeriodLifecycle, PesLoopADeadBallBlowWaitsForPlayThenWhistles) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2700.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  match->StopPlay();
  for (int tick = 0; tick < 54 * 60 + 1; ++tick) match->GetReferee()->Process();
  match->StartPlay();
  match->GetReferee()->Process();
  EXPECT_FALSE(match->IsInPlay()) << "the held blow whistles on the first tick of play";
}

// The whistle spends the held blow: the next period, started by the whistle, is not whistled again.
TEST(PesPeriodLifecycle, PesLoopAWhistleSpendsItsHeldBlow) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 2700.0f, /*pesLoop=*/true);
  ASSERT_TRUE(match);
  match->StopPlay();
  for (int tick = 0; tick < 54 * 60 + 1; ++tick) match->GetReferee()->Process();
  match->StartPlay();
  match->GetReferee()->Process();
  ASSERT_FALSE(match->IsInPlay());
  match->StartPlay();
  match->GetReferee()->Process();
  EXPECT_TRUE(match->IsInPlay()) << "the second half is not whistled by the first half's blow";
}

// The match speed is the match length as the menu clamps it: a 2-minute setting runs at the 5-minute
// floor, so a frame is 100 / 5 = 20 units.
TEST(PesPeriodLifecycle, TheMatchSpeedUsesTheClampedMatchLength) {
  REQUIRE_RIG();
  auto match = MatchAtPhase(e_MatchPhase_1stHalf, 0.0f);
  ASSERT_TRUE(match);
  match->AdvanceMatchFrames(1);
  EXPECT_FLOAT_EQ(match->GetPesClock().PeriodClock(), 20.0f);
}
