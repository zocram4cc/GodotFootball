// MOVESTEP wired onto the PES path: the install constants travel through
// MatchSetup like every other PES table (never the process env), the machine
// runs only with pes_loop on, and an issued clip drives the player's
// simulated motion (animation + playback) on that path.
#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "base/properties.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesmovestep.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/exe.hpp"
#include "pes/movestep.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1000;

blunted::Database* SharedDb() {
  static blunted::Database db;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    if (!db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql")) return nullptr;
  }
  return &db;
}

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

std::unique_ptr<Match> MakeMatch(const Properties& config) {
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = kSeed;
  setup.durationMinutes = 5;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  return std::unique_ptr<Match>(new Match(new MatchData(kHomeTeam, kAwayTeam), setup, config));
}

#define REQUIRE_PES_MATCH(config)                                                          \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch(config);                                      \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

void Place(Player* p, float x, float z, float fx, float fz) {
  p->ResetPosition(Vector3(x, z, 0.0f), Vector3(fx, fz, 0.0f));
}

}  // namespace

// The constants travel through MatchSetup: a match built without the
// MOVESTEP bundle refuses, the same way a match without the stamina or
// carrier tables refuses. (Red: MatchSetup has no such field yet.)
TEST(MoveStepWire, MatchWithoutMovestepConstantsRefuses) {
  REQUIRE_PES_MATCH(Properties());
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = kSeed;
  setup.durationMinutes = 5;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  ASSERT_TRUE(pestest::FillMissingPesConstants(&setup));
  setup.pesMoveStepConstants = nullptr;
  EXPECT_THROW(std::unique_ptr<Match>(new Match(new MatchData(kHomeTeam, kAwayTeam), setup, Properties())),
               std::runtime_error);
}

// Off path: with pes_loop off, MOVESTEP leaves the player alone entirely -
// no FSM step, no clip, no playback overwrite (the contact rates at
// match.cpp:1925,1941 keep theirs). (Red: ProcessMoveStep runs anyway.)
TEST(MoveStepWire, OffPathLeavesPlaybackAndClipAlone) {
  REQUIRE_PES_MATCH(Properties());
  Player* p = match->GetTeam(0)->GetAllPlayers().at(0).get();
  match->StartPlay();
  match->Process();
  p->SetPlaybackRate(0.5f);
  const uint32_t serialBefore = p->GetIssuedClip().serial;
  const pes::MoveState stateBefore = p->GetMoveStep().state;
  pesmovestep::ProcessMoveStep(*p, *match);
  EXPECT_FLOAT_EQ(p->GetCurrentAnim()->playbackRate, 0.5f);
  EXPECT_EQ(p->GetIssuedClip().serial, serialBefore);
  EXPECT_EQ(p->GetMoveStep().state, stateBefore);
}

// On path: an issued MOVESTEP clip changes the player's simulated motion -
// the animation the humanoid advances is the issued one. (Red: Issue only
// records the clip for the viewer.)
TEST(MoveStepWire, IssuedClipDrivesSimulatedMotion) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  Player* p = match->GetTeam(0)->GetAllPlayers().at(0).get();
  match->StartPlay();
  match->Process();
  pes::MoveStepRecord& rec = p->GetMoveStep();
  rec.state = pes::MoveState::kRunLoop;
  rec.style = 0;
  const int motionBefore = p->GetIssuedClip().request.motion;
  pesmovestep::ProcessMoveStep(*p, *match);
  EXPECT_GT(p->GetIssuedClip().serial, 0u);
  EXPECT_NE(p->GetIssuedClip().request.motion, motionBefore);
}

// CurrentMotion is PES's motion number (movestep.hpp:612-613), the motion
// MOVESTEP last issued - not GF's anim id. (Red: returns anim->id.)
TEST(MoveStepWire, CurrentMotionIsTheIssuedMotionNumber) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  Player* p = match->GetTeam(0)->GetAllPlayers().at(0).get();
  match->StartPlay();
  match->Process();
  pes::MoveStepRecord& rec = p->GetMoveStep();
  rec.state = pes::MoveState::kRunLoop;
  pesmovestep::ProcessMoveStep(*p, *match);
  // The adapter's CurrentMotion must answer the issued motion: GF anim ids
  // and PES motion numbers live in different spaces, so they must differ
  // here (the adapter returning anim->id would conflate them).
  EXPECT_NE(p->GetCurrentAnim()->id, p->GetIssuedClip().request.motion)
      << "GF anim ids and PES motion numbers live in different spaces";
}

// The FSM leaves GOTO_RUN once the action setter feeds the speed class
// (FUN_1406eebb0, P+0x245a): a running player reaches RUN_LOOP. (Red: the
// record's speed class has no writer, so GOTO_RUN never exits.)
TEST(MoveStepWire, FsmLeavesGotoRunForARunningPlayer) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  Team* home = match->GetTeam(0);
  Player* p = home->GetAllPlayers().at(0).get();
  Place(p, 0.0f, 0.0f, 0.0f, 10.0f);
  match->StartPlay();
  for (int i = 0; i < 4; ++i) match->Process();
  // Give the player ground speed: displace him, then tick MOVESTEP.
  p->GetMoveStep().state = pes::MoveState::kGotoRun;
  pesmovestep::ProcessMoveStep(*p, *match);
  const pes::MoveState after = p->GetMoveStep().state;
  EXPECT_TRUE(after == pes::MoveState::kRunLoop || after == pes::MoveState::kStepLoop ||
              after == pes::MoveState::kStop || after == pes::MoveState::kTurnStop)
      << "GOTO_RUN must exit, reached " << static_cast<int>(after);
}
