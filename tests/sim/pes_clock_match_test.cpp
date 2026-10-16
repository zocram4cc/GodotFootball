// The PES clock inside a live match: half length by phase, the once-per-half
// stoppage formula, the freeze during a stoppage and the state-completion
// transition - the divergences docs/re/pes-re-tactics-clock.md 8 lists.

#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cmath>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <optional>
#include <string>

#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesjudge.hpp"
#include "onthepitch/peskeeper.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesstoppage.hpp"
#include "onthepitch/pesrandom.hpp"
#include "onthepitch/pesclock.hpp"
#include "rig/rig.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "simcontext.hpp"

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int SHORT_MATCH_MINUTES = 2;

// The player skeleton comes from the user's PES install, without which a Match
// cannot be built at all.
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
// pes-re-ball-contact.md 6). Required by every Match, like the clock's.
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

// PES 2021's set-piece taker wait (docs/re/pes-re-setpiece-choreography.md 9
// item 2): the corner/free-kick/penalty gates read from the exe.
const pes::SetPieceTakerConstants* SharedSetPieceTakerConstants() {
  static std::optional<pes::SetPieceTakerConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      pes::PesVfs vfs;
      std::string err;
      if (vfs.Mount(root, &err)) {
        if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
          constants = pes::LoadSetPieceTakerConstants(vfs, *exe, &err);
      }
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
    GTEST_SKIP() << "PES2021.exe: carrier constants unavailable"; \
  if (!SharedSetPieceTakerConstants())                                         \
    GTEST_SKIP() << "PES2021.exe: set-piece taker constants unavailable"


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

}  // namespace

// The clock's exe-derived constants come from the install, not from code.
TEST(PesClockInMatch, ClockConstantsAreExeDerived) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = SHORT_MATCH_MINUTES;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  // The clock constants are required, like the rig: a match built without them
  // is a caller error, and there is no fallback rig either.
  setup.pesClockConstants = nullptr;
  EXPECT_THROW(Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties()),
               std::runtime_error);

  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto c = pes::LoadPesClockConstants(*exe, &err);
  ASSERT_TRUE(c) << err;
  setup.pesClockConstants = &*c;
  auto b = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(b) << err;
  setup.pesBallConstants = &*b;
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesSetPieceTakerConstants = SharedSetPieceTakerConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  pestest::FillMissingPesConstants(&setup);
  Match withExe(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
  EXPECT_FLOAT_EQ(withExe.GetPesClockConstants().gameSpeed, 54.0f);
  EXPECT_FLOAT_EQ(withExe.GetPesBallConstants().image.tickHz, 54.0f);
  SetDB(nullptr);
}

// The ball's constants are required the same way, and they come from the
// install's constant_match.bin `ball.o` rather than from code.
TEST(PesBallInMatch, BallConstantsAreRequiredAndInstallDerived) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = SHORT_MATCH_MINUTES;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  const pes::PesClockConstants* clockConstants = SharedClockConstants();
  ASSERT_NE(clockConstants, nullptr) << "PES2021.exe: the match clock constants are unavailable";
  setup.pesClockConstants = clockConstants;
  setup.pesBallConstants = SharedBallConstants();
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesSetPieceTakerConstants = SharedSetPieceTakerConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  // Without them, the Match refuses to build, like the clock's and the rig.
  setup.pesBallConstants = nullptr;
  EXPECT_THROW(Match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties()),
               std::runtime_error);

  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto b = pes::LoadPesBallConstants(*exe, vfs, &err);
  ASSERT_TRUE(b) << err;
  setup.pesBallConstants = &*b;
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesSetPieceTakerConstants = SharedSetPieceTakerConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  pestest::FillMissingPesConstants(&setup);
  Match withBall(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
  // docs/re/pes-re-ball-constants.md 3.1/6.1: the mode-1 column.
  EXPECT_FLOAT_EQ(withBall.GetPesBallConstants()
                      .record.At(pes::BallRecordConstants::kFrictionRollSpeedMin, 1),
                  8.0f);
  EXPECT_FLOAT_EQ(withBall.GetPesBallConstants().record.boundRollMax, 7.5f);
  SetDB(nullptr);
}
