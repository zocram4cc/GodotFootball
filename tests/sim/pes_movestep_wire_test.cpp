// MOVESTEP on the PES path: the install constants come from the match, the machine runs only
// with pes_loop on, the rate ceiling and speed class are stored each tick, an issued clip plays
// on the humanoid, and CurrentMotion is the PES motion the body plays. Each test is a separate
// process under gtest_discover_tests, so the constants test sees a cold cache.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/properties.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesclips.hpp"
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
constexpr int kTicks = 900;  // about 17 s of play

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

// PES21_ROOT is restored when the test ends, whatever the test did to it.
struct EnvRestore {
  std::optional<std::string> value;
  EnvRestore() {
    if (const char* v = std::getenv("PES21_ROOT")) value = v;
  }
  ~EnvRestore() {
    if (value) setenv("PES21_ROOT", value->c_str(), 1);
  }
};

Player* FirstPlayer(Match& m) { return m.GetTeam(0)->GetAllPlayers().at(0).get(); }

std::vector<Player*> ActivePlayers(Match& m) {
  std::vector<Player*> out;
  m.GetActiveTeamPlayers(0, out);
  m.GetActiveTeamPlayers(1, out);
  return out;
}

// The clip a humanoid plays, read from the clip's own PES variables. family -1 is the AI's clip.
struct Played {
  int family = -1;
  int motion = -1;
};

Played PlayedOf(Player* p) {
  Played out;
  const Anim* a = p->GetCurrentAnim();
  if (!a || !a->anim) return out;
  const std::string family = a->anim->GetVariable("specialvar1");
  if (family.empty()) return out;
  out.family = std::atoi(family.c_str());
  out.motion = std::atoi(a->anim->GetVariable("specialvar2").c_str());
  return out;
}

int LocomotionFamily() { return static_cast<int>(pes::PesClipFamily::kLocomotion); }

bool IsPesFamily(int family) {
  return family == static_cast<int>(pes::PesClipFamily::kDodge) ||
         family == static_cast<int>(pes::PesClipFamily::kTackle) || family == LocomotionFamily();
}

}  // namespace

// The install constants come from the match (MatchSetup), never the process environment. The
// test removes PES21_ROOT before the first MOVESTEP tick of its process: the adapter's constant
// cache fills on that first tick. Built on main, the adapter reads PES21_ROOT, finds nothing, and
// the FSM never ticks, so the record's rate stays at zero.
TEST(MoveStepWire, ConstantsComeFromTheMatchNotTheEnvironment) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  EnvRestore restore;
  unsetenv("PES21_ROOT");
  Player* p = FirstPlayer(*match);
  pes::MoveStepRecord& rec = p->GetMoveStep();
  rec.state = pes::MoveState::kInit;
  rec.rate = 0.0f;
  pesmovestep::ProcessMoveStep(*p, *match);
  EXPECT_GT(rec.rate, 0.0f) << "the FSM must tick from the match's own constants";
}

// With pes_loop off MOVESTEP does not run: the player's FSM, clips and playback are untouched.
// Built on main, the FSM ticks and overwrites the playback rate with the record's 1.0.
TEST(MoveStepWire, PesLoopOffLeavesThePlayerAlone) {
  REQUIRE_PES_MATCH(Properties());
  match->StartPlay();
  Player* p = FirstPlayer(*match);
  p->SetPlaybackRate(0.5f);
  const pes::MoveStepRecord before = p->GetMoveStep();
  const uint32_t serial = p->GetIssuedClip().serial;
  pesmovestep::ProcessMoveStep(*p, *match);
  EXPECT_EQ(p->GetIssuedClip().serial, serial);
  EXPECT_EQ(p->GetMoveStep().state, before.state);
  EXPECT_EQ(p->GetMoveStep().rate, before.rate);
  EXPECT_FLOAT_EQ(p->GetCurrentAnim()->playbackRate, 0.5f);
}

// On the PES path the playback rate is written where PES writes it, at a SetMotion issue. A tick
// that issues nothing leaves a contact's rate alone (match.cpp:1925,1941 write those rates).
// Built on main, every tick overwrites the rate with the record's 1.0.
TEST(MoveStepWire, PesLoopWritesPlaybackOnlyWhenItIssues) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  Player* p = FirstPlayer(*match);
  int quiet = 0;
  for (int tick = 0; tick < 300; ++tick) {
    match->Process();
    p->SetPlaybackRate(0.5f);
    const uint32_t serial = p->GetIssuedClip().serial;
    pesmovestep::ProcessMoveStep(*p, *match);
    if (p->GetIssuedClip().serial == serial) {
      ++quiet;
      EXPECT_FLOAT_EQ(p->GetCurrentAnim()->playbackRate, 0.5f) << "tick " << tick;
    }
  }
  printf("[movestep-playback] quiet ticks %d of 300\n", quiet);
  EXPECT_GT(quiet, 0);
}

// The FSM leaves GOTO_RUN: the run states are reached in a match. Built on main, the speed class
// has no writer, so every player stays in GOTO_RUN.
TEST(MoveStepWire, TheFsmLeavesGotoRunAndReachesRunLoop) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  std::map<int, long> states;
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) ++states[static_cast<int>(p->GetMoveStep().state)];
  }
  for (const auto& [state, ticks] : states)
    printf("[movestep-states] state %d ticks %ld\n", state, ticks);
  EXPECT_GT(states[static_cast<int>(pes::MoveState::kRunLoop)], 0L);
}

// The FSM reaches STOP and ACCELERATE in a match: the run loop's pre-empt entries fire once the
// request carries the look distance PES's gates read. Built before the fill, the look distance
// is always zero, so both entries refuse and states 8 and 9 never run.
TEST(MoveStepWire, TheFsmReachesStopAndAccelerate) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  std::map<int, long> states;
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) ++states[static_cast<int>(p->GetMoveStep().state)];
  }
  for (const auto& [state, ticks] : states)
    printf("[movestep-states] state %d ticks %ld\n", state, ticks);
  EXPECT_GT(states[static_cast<int>(pes::MoveState::kStop)], 0L);
  EXPECT_GT(states[static_cast<int>(pes::MoveState::kAccelerate)], 0L);
}

// The kick-off taker reaches the ball: during the opening set piece his distance to the ball
// shrinks (PES's 0.65 m reach). Built with the mirrored heading, the FSM runs him away and the
// restart stalls.
TEST(MoveStepWire, TheKickoffTakerClosesToTheBall) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  float first = -1.0f, closest = 1e9f;
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    float best = 1e9f;
    for (Player* p : ActivePlayers(*match)) {
      const float d = (p->GetPosition() - match->GetBall()->GetPositionBuffer()).GetLength();
      if (d < best) best = d;
    }
    if (t == 0) first = best;
    if (best < closest) closest = best;
  }
  printf("[movestep-approach] first %.2f closest %.2f\n", first, closest);
  EXPECT_GT(first, 0.0f);
  EXPECT_LT(closest, 0.65f) << "someone must reach the ball (PES's 0.65 m reach)";
}

// PES's bearing is taken on PES's z, which is -coords[1] (referee.cpp:197). A GF point on the +coords[1]
// side is on PES's -z side, so the bearing to it is 180 degrees, not 0. An AI command toward the ball
// must give the heading PES would take toward it; the mirrored one walks the player away.
TEST(MoveStepWire, ABearingToAGroundPointIsTakenOnPesZ) {
  pes::RunConstants k{};
  k.full = 360.0f;
  k.half_turn = 180.0f;
  k.wrapFull = -360.0f;
  EXPECT_NEAR(pesmovestep::PesBearingDeg(k, 0.0f, 10.0f), 180.0f, 1e-3f)
      << "a point at GF +coords[1] is PES -z: bearing 180";
  EXPECT_NEAR(pesmovestep::PesBearingDeg(k, 10.0f, 0.0f), 90.0f, 1e-3f)
      << "a point at GF +coords[0] is PES +x: bearing 90";
}

// StoreRateCeiling has a caller: the record holds the TopRate ceiling once the FSM has ticked.
// Built on main, nothing stores it, so the ceiling reads zero.
TEST(MoveStepWire, TheRateCeilingIsStoredEachTick) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  for (int t = 0; t < 5; ++t) match->Process();
  const float ceiling = FirstPlayer(*match)->GetMoveStep().rateCeiling;
  printf("[movestep-ceiling] rate ceiling %.4f\n", ceiling);
  EXPECT_GT(ceiling, 0.5f);
  EXPECT_LE(ceiling, 1.0f);
}

// An issued clip drives the humanoid: on each tick where the FSM issues, the body plays a PES
// locomotion clip. Built on main, Issue only records the clip for the viewer, so the body keeps
// the AI's own clip.
TEST(MoveStepWire, AnIssuedClipDrivesTheHumanoid) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  int issued = 0, driven = 0;
  for (int t = 0; t < kTicks; ++t) {
    std::vector<Player*> ps = ActivePlayers(*match);
    std::vector<uint32_t> serial;
    for (Player* p : ps) serial.push_back(p->GetIssuedClip().serial);
    match->Process();
    for (size_t i = 0; i < ps.size(); ++i) {
      if (ps[i]->GetIssuedClip().serial == serial[i]) continue;
      ++issued;
      if (PlayedOf(ps[i]).family == LocomotionFamily()) ++driven;
    }
  }
  printf("[movestep-link] clips issued %d, played by the humanoid %d\n", issued, driven);
  EXPECT_GT(issued, 0);
  EXPECT_GT(driven, 0);
}

// A played clip advances, and a re-issue of the same motion does not restart it: while one PES
// clip stays on the body, its frame moves on and never moves back. Built on main, no PES
// locomotion clip ever plays, so there is nothing to advance.
TEST(MoveStepWire, APlayedClipAdvancesAndIsNotRestartedByItsOwnMotion) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  int sameClip = 0, advanced = 0, restarted = 0;
  for (int t = 0; t < kTicks; ++t) {
    std::vector<Player*> ps = ActivePlayers(*match);
    // The clip is the Animation the humanoid's Anim struct holds: the struct is one object per player.
    std::vector<const Animation*> before;
    std::vector<int> frameBefore;
    for (Player* p : ps) {
      const Anim* a = p->GetCurrentAnim();
      before.push_back(a ? a->anim : nullptr);
      frameBefore.push_back(a ? a->frameNum : 0);
    }
    match->Process();
    for (size_t i = 0; i < ps.size(); ++i) {
      const Anim* a = ps[i]->GetCurrentAnim();
      if (!a || !a->anim || a->anim != before[i] || PlayedOf(ps[i]).family != LocomotionFamily()) continue;
      ++sameClip;
      if (a->frameNum > frameBefore[i]) ++advanced;
      if (a->frameNum < frameBefore[i]) ++restarted;
    }
  }
  printf("[movestep-advance] same clip ticks %d, advanced %d, restarted %d\n", sameClip, advanced,
         restarted);
  EXPECT_GT(sameClip, 0);
  EXPECT_GT(advanced, 0);
  EXPECT_EQ(restarted, 0);
}

// A re-issue of the motion the body already plays changes its rate only (humanoidbase.cpp's hold). Built
// without the hold, the re-issue selects the clip again and restarts it from frame 0.
TEST(MoveStepWire, AReIssueOfThePlayedMotionDoesNotRestartTheClip) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) {
      const Played played = PlayedOf(p);
      const Anim* a = p->GetCurrentAnim();
      if (played.family != LocomotionFamily() || a->frameNum < 3) continue;
      const Animation* clipBefore = a->anim;
      const int frameBefore = a->frameNum;
      p->CastHumanoid()->PlayIssuedClip(played.motion, 1.0f, false);
      printf("[movestep-hold] frame %d re-issued, now %d\n", frameBefore, a->frameNum);
      EXPECT_EQ(a->anim, clipBefore);
      EXPECT_GE(a->frameNum, frameBefore);
      return;
    }
  }
  ADD_FAILURE() << "no locomotion clip reached frame 3";
}

// The FSM issues past its entry clip: the due requests the tail issues (FUN_1406b8340) are clips too. Built
// without the tail, each player gets its one goto entry clip and no more.
TEST(MoveStepWire, TheFsmKeepsIssuingAfterItsEntryClip) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  std::map<const Player*, uint32_t> lastSerial;
  std::map<const Player*, int> issues;
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) {
      const uint32_t serial = p->GetIssuedClip().serial;
      if (serial == lastSerial[p]) continue;
      lastSerial[p] = serial;
      ++issues[p];
    }
  }
  int most = 0, multi = 0;
  for (const auto& [player, count] : issues) {
    most = std::max(most, count);
    if (count > 1) ++multi;
  }
  printf("[movestep-reissue] players with more than one issue %d, most issues %d\n", multi, most);
  EXPECT_GT(multi, 0);
}

// The contact stage (match.cpp step 6) writes a tripped body's rate after MOVESTEP's tick (PES's order:
// the player's MOVESTEP before the contact reaction). MOVESTEP must leave that rate alone on a trip body.
TEST(MoveStepWire, MoveStepLeavesATrippedBodysContactRateAlone) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  int tripped = 0, changed = 0;
  for (int t = 0; t < kTicks && tripped < 200; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) {
      if (p->GetCurrentFunctionType() != e_FunctionType_Trip) continue;
      const float before = p->GetCurrentAnim()->playbackRate;
      pesmovestep::ProcessMoveStep(*p, *match);
      ++tripped;
      if (p->GetCurrentAnim()->playbackRate != before) ++changed;
    }
  }
  printf("[movestep-contact] tripped body ticks %d, rate changed %d\n", tripped, changed);
  EXPECT_GT(tripped, 0);
  EXPECT_EQ(changed, 0);
}

// CurrentMotion is the PES motion number the body plays (movestep.hpp: PES's motion id), and -1
// when the AI's own clip plays. Built on main it returns the GF anim id, which is another space.
TEST(MoveStepWire, CurrentMotionIsThePesMotionTheBodyPlays) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  int checks = 0;
  for (int t = 0; t < kTicks; ++t) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) {
      const Played played = PlayedOf(p);
      const int expected = IsPesFamily(played.family) ? played.motion : -1;
      EXPECT_EQ(pesmovestep::CurrentMotionOf(*p), expected) << "tick " << t;
      ++checks;
    }
  }
  printf("[movestep-motion] checks %d\n", checks);
  EXPECT_GT(checks, 0);
}
