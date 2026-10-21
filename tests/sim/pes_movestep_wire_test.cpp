// MOVESTEP on the PES path: the install constants come from the match, the machine runs only
// with pes_loop on, the rate ceiling and speed class are stored each tick, an issued clip plays
// on the humanoid, and CurrentMotion is the PES motion the body plays. Each test is a separate
// process under gtest_discover_tests, so the constants test sees a cold cache.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
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
// The gfsim configuration of the trace that found a STOP held for 72 ticks (player 4 at tick 1258).
constexpr unsigned int kTraceSeed = 7;
constexpr int kTraceMinutes = 2;

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

std::unique_ptr<Match> MakeMatch(const Properties& config, unsigned int seed = kSeed, int minutes = 5) {
  MatchSetup setup;
  setup.homeTeamDbId = kHomeTeam;
  setup.awayTeamDbId = kAwayTeam;
  setup.seed = seed;
  setup.durationMinutes = minutes;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  if (!pestest::FillMissingPesConstants(&setup)) return nullptr;
  return std::unique_ptr<Match>(new Match(new MatchData(kHomeTeam, kAwayTeam), setup, config));
}

#define REQUIRE_PES_MATCH_AT(config, seed, minutes)                                        \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  blunted::Database* db = SharedDb();                                                    \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                        \
  SetDB(db);                                                                             \
  std::unique_ptr<Match> match = MakeMatch(config, seed, minutes);                       \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

#define REQUIRE_PES_MATCH(config) REQUIRE_PES_MATCH_AT(config, kSeed, 5)

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
// DISABLED 20-10-26 (owner decision, lane-batch landing): with valid S1 rows the ball goes out
// early and the goal-kick restart path runs; GF has no PES goal-kick taker walk-in and release
// on reach (pessetpiecetaker.hpp:21, A5 §4.7), so the keeper stands 2.3 m off and no player
// reaches 0.65 m. The test passed before only because empty rows kept the ball in play (S1Port
// trace, docs/ports/pes-port-S1Port.md). Re-enable when branch GoalKickReach lands.
TEST(MoveStepWire, DISABLED_TheKickoffTakerClosesToTheBall) {
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
// clip stays on the body, its frame moves on and never moves back.
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
    std::vector<int> placementBefore;
    for (Player* p : ps) {
      const Anim* a = p->GetCurrentAnim();
      before.push_back(a ? a->anim : nullptr);
      frameBefore.push_back(a ? a->frameNum : 0);
      placementBefore.push_back(p->CastHumanoid()->GetPlacementCount());
    }
    match->Process();
    for (size_t i = 0; i < ps.size(); ++i) {
      const Anim* a = ps[i]->GetCurrentAnim();
      // GF's set-piece placement (Referee::PrepareSetPiece -> Match::ResetSituation -> PlayerBase::ResetSituation ->
      // HumanoidBase::ResetPosition) takes the body off its clip; the clip is issued again from frame 0. That tick says
      // nothing about a clip that stayed on the body, so it is skipped for that player only.
      if (ps[i]->CastHumanoid()->GetPlacementCount() != placementBefore[i]) continue;
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

// A STOP leaves the stop on a turned body. PES's STOP (FUN_1406c9e20) exits to IDLE_TURN through FUN_1406d1390 when the
// look bucket (word 0xd) is off straight and the facing error's magnitude is at least 33.75 degrees (VA 0x1425cc688); the
// call is at VA 0x1406ca51f. Built on main, that else arm is empty and the bucket it tests is the move bucket (word 8), so
// a stop with a turned body holds for the whole match: the trace held one for 72 ticks. Every active player is set into
// STOP, and none may still be in STOP after 60 ticks.
TEST(MoveStepWire, AStopOnATurnedBodyLeavesTheStop) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH_AT(config, kTraceSeed, kTraceMinutes);
  // Snapshots through play: at each one every active player enters STOP through PES's STOP entry (FUN_1406d5d60, as a handler
  // does), and the players the entry leaves in STOP step 60 ticks on the AI's commands of that moment. The geometry a stopping
  // body meets is a turn toward a target, looked past the turn.
  constexpr int kSnapshots = 6;
  constexpr int kTicksBetween = 100;
  constexpr int kStopTicks = 60;  // about 1.1 s: the second dwell (0.35 s) and the turn ramp's scale fit inside it
  int forced = 0;
  int held = 0;
  for (int s = 0; s < kSnapshots; ++s) {
    for (int t = 0; t < kTicksBetween; ++t) match->Process();
    std::vector<Player*> stopped;
    for (Player* p : ActivePlayers(*match)) {
      pesmovestep::RequestStop(*p, *match);
      if (p->GetMoveStep().state == pes::MoveState::kStop) stopped.push_back(p);
    }
    for (int t = 0; t < kStopTicks; ++t) {
      for (Player* p : stopped) pesmovestep::ProcessMoveStep(*p, *match);
    }
    for (Player* p : stopped) {
      ++forced;
      // A ball action that takes the body holds MOVESTEP's FSM (the same-tick precedence, pesmovestep.cpp), so a STOP it
      // interrupts leaves only when the action ends. Only a STOP no ball action holds must leave within the bound.
      if (p->GetMoveStep().state == pes::MoveState::kStop && !IsBallActionFunction(p->CastHumanoid()->GetCurrentFunctionType())) ++held;
    }
  }
  EXPECT_GT(forced, 0);
  EXPECT_EQ(held, 0) << held << " of " << forced << " forced STOPs still held after " << kStopTicks << " ticks";
}


// While the MOVESTEP FSM holds STOP, the AI's movement command does not pick the body's clip: PES's MOVE action plays the
// FSM's own clips and MOVESTEP is the only reader of the AI's command block (pes-re-anim-select.md 0 item 5 and 1). On main,
// a PES clip's last frame hands the body to Humanoid::Process's end-of-clip switch, which selects the AI's movement toward
// the target, so the body ran while the FSM held the stop. After the clip-end fix this trace holds no natural STOP in its
// first 1800 ticks, so the holds are forced through PES's STOP entry as AStopOnATurnedBodyLeavesTheStop does, and the match
// runs on while they hold.
TEST(MoveStepWire, AnAiMovementClipDoesNotTakeTheBodyWhileTheFsmHoldsAStop) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH_AT(config, kTraceSeed, kTraceMinutes);
  constexpr int kSnapshots = 6;
  constexpr int kTicksBetween = 100;
  constexpr int kHoldTicks = 60;  // about 1.1 s, as AStopOnATurnedBodyLeavesTheStop
  int holdTicks = 0;
  int violations = 0;
  int firstTick = -1;
  int tick = 0;
  for (int s = 0; s < kSnapshots; ++s) {
    for (int t = 0; t < kTicksBetween; ++t, ++tick) match->Process();
    std::vector<Player*> stopped;
    for (Player* p : ActivePlayers(*match)) {
      pesmovestep::RequestStop(*p, *match);
      if (p->GetMoveStep().state == pes::MoveState::kStop) stopped.push_back(p);
    }
    for (int t = 0; t < kHoldTicks; ++t, ++tick) {
      match->Process();
      for (Player* p : stopped) {
        if (p->GetMoveStep().state != pes::MoveState::kStop) continue;
        ++holdTicks;
        const Anim* a = p->GetCurrentAnim();
        if (a && a->functionType == e_FunctionType_Movement) {
          if (firstTick < 0) firstTick = tick;
          ++violations;
        }
      }
    }
  }
  EXPECT_GT(holdTicks, 0) << "no forced STOP held in the match: the check saw nothing";
  EXPECT_EQ(violations, 0) << "AI movement clips held the body on " << violations << " STOP player-ticks; the first at tick "
                           << firstTick;
}

// While the MOVESTEP FSM runs ACCELERATE, the AI's movement command does not pick the body's clip, the same rule as the STOP
// hold above. An AI movement clip on an ACCELERATE tick leaves the handler with no motion record (CurrentMotion -1), so
// every arm falls through and the state never leaves ACCELERATE.
TEST(MoveStepWire, AnAiMovementClipDoesNotTakeTheBodyWhileTheFsmRunsAccelerate) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH_AT(config, kTraceSeed, kTraceMinutes);
  constexpr int kAccelerateTicks = 1800;  // about 33 s of play
  int accelerateTicks = 0;
  int violations = 0;
  int firstTick = -1;
  for (int tick = 0; tick < kAccelerateTicks; ++tick) {
    match->Process();
    for (Player* p : ActivePlayers(*match)) {
      if (p->GetMoveStep().state != pes::MoveState::kAccelerate) continue;
      ++accelerateTicks;
      const Anim* a = p->GetCurrentAnim();
      if (a && a->functionType == e_FunctionType_Movement) {
        if (firstTick < 0) firstTick = tick;
        ++violations;
      }
    }
  }
  EXPECT_GT(accelerateTicks, 0) << "no ACCELERATE in " << kAccelerateTicks << " ticks: the check saw nothing";
  EXPECT_EQ(violations, 0) << "AI movement clips held the body on " << violations << " ACCELERATE player-ticks; the first at tick "
                           << firstTick;
}

// The adapter's PES frame (PesFrame): PES reads the height at P+0xc and the ground z at P+0x10, so out is (x, height, z)
// with height = GF z and ground z = -GF y (GF's coords[2] is the height: ball.cpp reads it as the height).
TEST(MoveStepWire, PesFrameIsXHeightGroundZ) {
  float out[3] = {0.0f, 0.0f, 0.0f};
  pesmovestep::PesFrame(1.0f, 2.0f, 3.0f, out);
  EXPECT_FLOAT_EQ(out[0], 1.0f) << "x is GF x";
  EXPECT_FLOAT_EQ(out[1], 3.0f) << "height is GF z";
  EXPECT_FLOAT_EQ(out[2], -2.0f) << "ground z is -GF y";
}

// PES's filler reads the AI's movement command, not a pending ball action (docs/re/pes-re-decision-read.md): a queue whose
// front is a pass hands MOVESTEP the walk behind it.
TEST(MoveStepWire, TheDecisionIsTheFirstMovementCommandNotAnAction) {
  PlayerCommand pass;
  pass.desiredFunctionType = e_FunctionType_ShortPass;
  PlayerCommand walk;
  walk.desiredFunctionType = e_FunctionType_Movement;
  walk.desiredVelocityFloat = sprintVelocity;
  const PlayerCommandQueue queue = {pass, walk};
  const std::optional<PlayerCommand> decision = pesmovestep::MovementDecision(queue);
  ASSERT_TRUE(decision.has_value()) << "no movement command was chosen from a pass and a walk";
  EXPECT_EQ(decision->desiredFunctionType, e_FunctionType_Movement);
  EXPECT_FLOAT_EQ(decision->desiredVelocityFloat, sprintVelocity);
  EXPECT_FALSE(pesmovestep::MovementDecision(PlayerCommandQueue{pass}).has_value()) << "a lone pass is not a movement";
}

// A move target within 0.2 m of the root steers by the ball's sample 0 (PES's filler: DAT_142598578, 0.2 in the install;
// pes-re-decision-read.md 2 says 0.4).
TEST(MoveStepWire, AMoveTargetWithin20CmOfTheRootSteersByTheBall) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  Player* p = FirstPlayer(*match);
  const Vector3 at = p->GetPosition();
  PlayerCommand c;
  c.useDesiredTarget = true;
  c.desiredTarget = at + Vector3(0.1f, 0.0f, 0.0f);
  c.desiredVelocityFloat = sprintVelocity;
  p->GetPesDecision() = PesDecision{match->GetPesFrame(), c};
  const Vector3 ball = match->GetBall()->Predict(0);
  const pes::MoveReq req = pesmovestep::TickRequest(*p, *match);
  EXPECT_NEAR(req.F(pes::moveword::kMoveTargetX), ball.coords[0], 1e-3f);
  EXPECT_NEAR(req.F(pes::moveword::kMoveTargetZ), -ball.coords[1], 1e-3f);
}

// The request steers by the AI's decision on the tick the decision changes, not by the clip the mover plays: a decision
// that turns him from east to north moves the request's move target north the same tick. Built on the clip's own command,
// the target keeps pointing where the clip's command did until the clip ends.
TEST(MoveStepWire, TheRequestFollowsTheDecisionOnTheTickItChanges) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  Player* p = FirstPlayer(*match);
  const Vector3 at = p->GetPosition();
  const auto decide = [&at](const Vector3& offset) {
    PlayerCommand c;
    c.useDesiredTarget = true;
    c.desiredTarget = at + offset;
    c.desiredVelocityFloat = sprintVelocity;
    return c;
  };
  p->GetPesDecision() = PesDecision{match->GetPesFrame(), decide(Vector3(10.0f, 0.0f, 0.0f))};
  const pes::MoveReq east = pesmovestep::TickRequest(*p, *match);
  EXPECT_NEAR(east.F(pes::moveword::kMoveTargetX), at.coords[0] + 10.0f, 1e-3f);
  EXPECT_NEAR(east.F(pes::moveword::kMoveTargetZ), -at.coords[1], 1e-3f);
  // GF +y is PES -z: the AI turns him north on its next decision.
  p->GetPesDecision() = PesDecision{match->GetPesFrame(), decide(Vector3(0.0f, 10.0f, 0.0f))};
  const pes::MoveReq north = pesmovestep::TickRequest(*p, *match);
  EXPECT_NEAR(north.F(pes::moveword::kMoveTargetX), at.coords[0], 1e-3f);
  EXPECT_NEAR(north.F(pes::moveword::kMoveTargetZ), -(at.coords[1] + 10.0f), 1e-3f);
}

// PES's think cadence is not decoded. GF thinks every tick (pesmovestep::Think, after the controller's process), so the
// decision is re-taken on nearly every tick and its frame stamp takes nearly every frame of the run. The 25-frame gate
// that docs/re/pes-re-teamtick.md 5.1 named is computed and never read (raw asm 0x92578f, 0x8bfcf0).
TEST(MoveStepWire, TheDecisionIsTakenOnEveryTick) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  Player* p = FirstPlayer(*match);
  std::set<int> stamps;
  for (int tick = 0; tick < 300; ++tick) {
    match->Process();
    const int frame = p->GetPesDecision().frame;
    if (frame >= 0) stamps.insert(frame);
  }
  EXPECT_GT(stamps.size(), 250u) << "the decision was re-taken on only " << stamps.size() << " of 300 ticks";
}

// A PES locomotion clip that has reached its last frame starts again from its first frame when its own motion is issued
// again. PES's issue is a restart (IssuedClip, movestep.hpp), and the body's own Switch at the last frame is held in STOP
// and ACCELERATE (humanoid.cpp PesPoseHeld), so a re-issue is the only way such a clip moves on.
TEST(MoveStepWire, ASameMotionReissueRestartsAFinishedClip) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  Player* p = nullptr;
  Played played;
  for (int tick = 0; tick < 2000 && !p; ++tick) {
    match->Process();
    for (Player* q : ActivePlayers(*match)) {
      const Played now = PlayedOf(q);
      if (now.family == LocomotionFamily()) {
        p = q;
        played = now;
        break;
      }
    }
  }
  ASSERT_NE(p, nullptr) << "no player played a PES locomotion clip in 2000 ticks";
  Anim* anim = const_cast<Anim*>(p->GetCurrentAnim());  // the test sets the clip's frame, as the humanoid does while it plays
  anim->frameNum = anim->anim->GetFrameCount() - 1;
  p->CastHumanoid()->PlayIssuedClip(played.motion, 1.0f, false);
  EXPECT_EQ(p->GetCurrentAnim()->frameNum, 0) << "a finished clip did not start again on its own motion";
}

// A finished ball action gives the body back. A trap clip held at its last frame while MOVESTEP is in ACCELERATE kept the
// body: the FSM skipped its tick while the trap played, and its hold blocked the end-of-clip switch, so nothing handed the
// body back (FirstTouchPort2's seed-42 freeze on b1f958a).
TEST(MoveStepWire, AFinishedBallActionHandsTheBodyBack) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  match->StartPlay();
  for (int t = 0; t < 60; ++t) match->Process();
  Player* p = FirstPlayer(*match);
  Anim* anim = const_cast<Anim*>(p->GetCurrentAnim());  // the test sets the clip the humanoid plays, as the humanoid does
  anim->functionType = e_FunctionType_Trap;
  anim->frameNum = anim->anim->GetFrameCount() - 1;
  p->GetMoveStep().state = pes::MoveState::kAccelerate;
  // The trap lets go on the tick its last frame passes, and MOVESTEP hands the body back as IDLE (PES's state after a ball
  // action is not decoded, so IDLE stands in; pesmovestep.cpp). Later ticks may legitimately re-enter ACCELERATE.
  bool released = false;
  pes::MoveState atRelease = pes::MoveState::kAccelerate;
  for (int t = 0; t < 54 && !released; ++t) {
    match->Process();
    if (p->GetCurrentFunctionType() != e_FunctionType_Trap) {
      released = true;
      atRelease = p->GetMoveStep().state;
    }
  }
  EXPECT_TRUE(released) << "the finished trap still holds the body";
  EXPECT_EQ(atRelease, pes::MoveState::kIdle) << "the hand-back did not leave MOVESTEP idle";
}
