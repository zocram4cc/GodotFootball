#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchreport.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesball.hpp"
#include "onthepitch/pesjudge.hpp"
#include "onthepitch/peskeeper.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pesstoppage.hpp"
#include "onthepitch/pesrandom.hpp"
#include "onthepitch/pesclock.hpp"
#include "onthepitch/pesteamtick.hpp"
#include "onthepitch/pesrunslots.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

// Headless AI-vs-AI matches on the synthetic fixture, with the player skeleton
// built from the user's PES install (skipped without one).
namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int SEED = 1000;
constexpr int SHORT_MATCH_MINUTES = 2;  // long enough for set pieces and shots
constexpr int FULL_MATCH_MINUTES = 5;
// A match must end: the clock covers the duration plus generous added time.
constexpr unsigned long kMaxSimTime_ms = 30ul * 60ul * 1000ul;
// A tripwire, not a PES rate: no live offside rate has been measured (the one live sample is a single whistled offside
// in about 10 minutes, docs/re/pes-re-defence-leftovers.md 7.4). It catches the gross defect: with the line read in the
// wrong frame the 5-minute fixture calls more than ten.
constexpr int kOffsideTripwire_5min = 5;

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

// PES 2021's match-clock constants, read from the install's own exe by the same
// rule the rig is (docs/re/pes-re-tactics-clock.md 7.2). Required by every Match.
const pes::PesClockConstants* SharedClockConstants() {
  static std::optional<pes::PesClockConstants> constants;
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (const char* root = std::getenv("PES21_ROOT")) {
      pes::PesVfs vfs;
      std::string err;
      if (vfs.Mount(root, &err)) {
        if (auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err))
          constants = pes::LoadPesClockConstants(*exe, &err);
      }
    }
  }
  return constants ? &*constants : nullptr;
}

// PES 2021's ball physics constants: the `ball.o` record of
// constant_match.bin plus the image values (docs/re/pes-re-ball-contact.md 6).
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


#define REQUIRE_PES_INSTALL()                                          \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  if (!SharedClockConstants())                                                          \
    GTEST_SKIP() << "PES2021.exe: match-clock constants unavailable" \
  if (!SharedStoppageConstants()) GTEST_SKIP() << "PES2021.exe: stoppage constants unavailable"; \
  if (!SharedCarrierConstants()) GTEST_SKIP() << "PES2021.exe: carrier constants unavailable"

struct Outcome {
  bool finished = false;
  uint64_t hash = 0;
  std::string report;
  int goals[2] = {0, 0};
  int offsides[2] = {0, 0};
};

MatchSetup MakeSetup(unsigned int seed, int minutes) {
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = seed;
  setup.durationMinutes = minutes;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  setup.pesClockConstants = SharedClockConstants();
  setup.pesBallConstants = SharedBallConstants();
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesSetPieceTakerConstants = SharedSetPieceTakerConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  return setup;
}

Outcome Play(unsigned int seed, int minutes) {
  blunted::Database db;
  EXPECT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(seed, minutes);

  Outcome out;
  {
    // Match owns and deletes its MatchData.
    pestest::FillMissingPesConstants(&setup);
    Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
    while (!match.IsGameOver() && match.GetSimTime_ms() < kMaxSimTime_ms) match.Process();
    out.finished = match.IsGameOver();
    out.hash = match.GetDeterminismHash();
    out.goals[0] = match.GetMatchData()->GetGoalCount(0);
    out.goals[1] = match.GetMatchData()->GetGoalCount(1);
    out.offsides[0] = match.GetMatchData()->GetOffsides(0);
    out.offsides[1] = match.GetMatchData()->GetOffsides(1);
    char* buf = nullptr;
    size_t len = 0;
    FILE* mem = open_memstream(&buf, &len);
    PrintMatchReport(match, mem);
    std::fclose(mem);
    out.report.assign(buf, len);
    std::free(buf);
  }
  SetDB(nullptr);
  return out;
}

#define REQUIRE_RIG() \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES"; \
  if (!SharedClockConstants())                                                  \
    GTEST_SKIP() << "PES2021.exe: match-clock constants unavailable"; \
  if (!SharedBallConstants())                                                   \
    GTEST_SKIP() << "constant_match.bin/exe: ball physics constants unavailable"; \
  if (!SharedStoppageConstants())                                               \
    GTEST_SKIP() << "PES2021.exe: stoppage constants unavailable"; \
  if (!SharedSetPieceTakerConstants())                                         \
    GTEST_SKIP() << "PES2021.exe: set-piece taker constants unavailable"

}  // namespace

// Player IDs phase per-player work ((time + id * 10) % 100 in Player::Process):
// a process-wide counter shifted them in a second match and the same seed then
// played a different game.
TEST(SimMatch, PlayerIdsStartAtZeroInEveryMatch) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = SHORT_MATCH_MINUTES;
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  setup.pesClockConstants = SharedClockConstants();
  setup.pesBallConstants = SharedBallConstants();
  setup.pesJudgeConstants = SharedJudgeConstants();
  setup.pesRngConstants = SharedRngConstants();
  setup.pesKeeperConstants = SharedKeeperConstants();
  setup.pesSetPieceTakerConstants = SharedSetPieceTakerConstants();
  setup.pesStaminaConstants = SharedStaminaConstants();
  setup.pesStoppageConstants = SharedStoppageConstants();
  setup.pesCarrierConstants = SharedCarrierConstants();
  for (int match = 0; match < 2; ++match) {
    pestest::FillMissingPesConstants(&setup);
    Match m(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
    std::vector<Player*> players;
    m.GetAllTeamPlayers(0, players);
    EXPECT_EQ(players.at(0)->GetID(), 0) << "match " << match;
  }
  SetDB(nullptr);
}

// A squad member who never takes the field has no dynamic formation slot dealt to him. His
// dynamic entry is read by the set-piece suitability (through GetDeployedSlot), so it must start
// as his own static entry, as Player::Activate sets it for the starting eleven. Uninitialised,
// it held garbage (valgrind: an uninitialised FormationEntry read in FormState::SlotFor).
TEST(SimMatch, BenchPlayersStartWithTheirOwnFormationEntry) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  // The second match is built over the first one's freed memory, where an uninitialised field
  // still holds whatever the last match left there.
  for (int match = 0; match < 2; ++match) {
    MatchSetup setup = MakeSetup(SEED, SHORT_MATCH_MINUTES);
    pestest::FillMissingPesConstants(&setup);
    Match m(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
    for (int side = 0; side < 2; ++side) {
      std::vector<Player*> players;
      m.GetAllTeamPlayers(side, players);
      for (Player* player : players) {
        const FormationEntry dynamic = player->GetDynamicFormationEntry();
        const FormationEntry own = player->GetFormationEntry();
        EXPECT_EQ(dynamic.role, own.role)
            << "match " << match << " side " << side << " player " << player->GetID();
        EXPECT_EQ(dynamic.databasePosition.coords[1], own.databasePosition.coords[1])
            << "match " << match << " side " << side << " player " << player->GetID();
      }
    }
  }
  SetDB(nullptr);
}

// Trailing by three, PES's CPU strategy decider (pesstrategy.hpp) must take
// the team to an attacking level at the latest from the second half's switch
// minute; the level is GF's mentality rung. With the CPU manager switched off
// for that team, the sheet's own mentality stands all match. The positive run
// keeps the negative one from passing vacuously.
// The strategy's input is the scoreline, so the test holds it at 0-3 the way
// the PES-loop sibling (PesLoopStepsTheTeamTickFromTheStrategy) does: without
// the hold the home side scores twice in play by minute 8 and trails by one,
// where Balanced - not Attacking - is the decider's right answer
// (pesstrategy.cpp, the second half's pre-switch step machine: g < -1 attacks,
// g = -1 holds). The hold keeps the deficit the test is about; it is scenery
// for "trailing", not a claim about the score.
bool EverAttacksWhileTrailing(bool aiManagerRunsHome) {
  blunted::Database db;
  EXPECT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = 3;  // 3, not 2: on the kick-off-125 trajectory (b0a9387) the trailing side's switch lands between minutes 2 and 3 (measured: 2 fails, 3 passes).
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  setup.aiManagerRuns[0] = aiManagerRunsHome;
  bool attacking = false;
  {
    MatchData* data = new MatchData(HOME_TEAM, AWAY_TEAM);
    pestest::FillMissingPesConstants(&setup);
    Match match(data, setup, Properties());
    const TeamInstructions::e_Mentality sheet =
        match.GetTeam(0)->GetController()->GetInstructions().mentality;
    constexpr int kAwayLead = 3;
    data->SetGoalCount(1, kAwayLead);
    // The decider's switch minute (pesstrategy.cpp, the second half's minute-28
    // hold) is the deadline: past it a trailing side has had every chance the
    // decider gives it.
    constexpr int kSwitchDeadlineMinute = 70;
    while (!match.IsGameOver() && !attacking &&
           match.GetMatchMinute() < kSwitchDeadlineMinute) {
      match.Process();
      // Hold the scoreline the strategy reads: a goal in play must not shrink
      // the deficit the test is about.
      data->SetGoalCount(0, 0);
      data->SetGoalCount(1, kAwayLead);
      const TeamInstructions::e_Mentality now =
          match.GetTeam(0)->GetController()->GetInstructions().mentality;
      attacking = now >= TeamInstructions::e_Mentality_Attacking &&
                  now != sheet;
    }
  }
  SetDB(nullptr);
  return attacking;
}

TEST(SimMatch, AIManagerSwitchGovernsTactics) {
  REQUIRE_RIG();
  EXPECT_TRUE(EverAttacksWhileTrailing(true));
  EXPECT_FALSE(EverAttacksWhileTrailing(false));
}
// On the PES path (pes_loop) the team's attack level is the per-tick struct
// (pesteamtick.hpp), stepped every team tick from the strategy: trailing by
// three, the CPU side must reach level 3+ from the second half's switch
// minute, and WriteAttackFlags must have run (level 4 sets B8A2).
TEST(SimMatch, PesLoopStepsTheTeamTickFromTheStrategy) {
  REQUIRE_RIG();
  blunted::Database db;
  EXPECT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup;
  setup.homeTeamDbId = HOME_TEAM;
  setup.awayTeamDbId = AWAY_TEAM;
  setup.seed = SEED;
  setup.durationMinutes = 1;
  setup.animationDir = GF_SOURCE_DIR "/assets/gf_anims";
  setup.rig = SharedRig();
  bool attacking = false;
  bool flagsRan = false;
  {
    MatchData* data = new MatchData(HOME_TEAM, AWAY_TEAM);
    pestest::FillMissingPesConstants(&setup);
    Properties config;
    config.SetBool("pes_loop", true);
    Match match(data, setup, config);
    constexpr int kAwayLead = 3;
    data->SetGoalCount(1, kAwayLead);
    constexpr int kSwitchDeadlineMinute = 70;
    while (!match.IsGameOver() && (!attacking || !flagsRan) &&
           match.GetMatchMinute() < kSwitchDeadlineMinute) {
      match.Process();
      // The strategy's input is the scoreline: hold it at 0-3 so a goal in play cannot shrink the deficit the test is about
      // (on the role-array trajectory the home side scores twice and trails by one, where level 2 is the right answer).
      data->SetGoalCount(0, 0);
      data->SetGoalCount(1, kAwayLead);
      const pes::TeamTick tick = match.GetTeam(0)->GetController()->GetTeamTick();
      if (tick.b8c0Frame > 0) flagsRan = (tick.b8a4 >= 4) == tick.b8a2;
      attacking = tick.b8a4 >= 3;
    }
    EXPECT_TRUE(attacking) << "level never left 2 while trailing 0-3";
    EXPECT_TRUE(flagsRan) << "B8A2 never matched level >= 4 after a drift";
  }
  SetDB(nullptr);
}

// Step 55 (FUN_140A42A70 at 0x1404F7A8F; restart arm 26c at 0x1404F6F0E) is the
// team tick's tail copy, not part of the strategy decider: it runs once per
// team tick on the PES path, after the tail write, and touches every slot head
// to the tick's frame stamp. The AI is off for this side, so the decider
// (UpdateStrategy) early-returns: only the tail step can touch the slots.
TEST(SimMatch, PesStep55TouchesEverySlotOncePerTeamTick) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(SEED, SHORT_MATCH_MINUTES);
  setup.aiManagerRuns[0] = false;
  pestest::FillMissingPesConstants(&setup);
  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config);
  auto* ctl = match.GetTeam(0)->GetController();
  match.Process();
  match.Process();
  match.Process();
  ASSERT_FALSE(match.IsInPlay()) << "takes would stamp the heads with the same frame";
  // The stamp the tail step writes during the next tick: actualTime only
  // advances at the tick's end, so this is the next tick's in-tick frame.
  const uint32_t stamp = static_cast<uint32_t>(match.GetPesFrame());
  ASSERT_GT(stamp, 0u) << "the frame must advance past the fresh rings' stamp";
  match.Process();
  for (int s = 0; s < pes::run::kSlots; ++s) {
    const pes::run::Slot& slot = ctl->GetRunSlot(s);
    EXPECT_EQ(slot.ring.entry[slot.ring.index].seenAt, stamp) << "slot " << s;
  }
  SetDB(nullptr);
}

// Step 48 (FUN_14057F600 at 0x1404F79A8) is wired at its step on the PES path.
// With the record tables OPEN (helpers sect 1-3) the arm verdicts stand in as
// off, so the dispatcher exits on a live tick and takes nothing.
TEST(SimMatch, PesStep48DispatchExitsWithRecordTablesOpen) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(SEED, SHORT_MATCH_MINUTES);
  pestest::FillMissingPesConstants(&setup);
  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, config);
  auto* ctl = match.GetTeam(0)->GetController();
  match.Process();
  EXPECT_EQ(ctl->PesStep48(), pes::run::ReleaseRoute::kExit);
  for (int s = 0; s < pes::run::kSlots; ++s) {
    EXPECT_EQ(ctl->GetRunSlot(s).state, 0) << "slot " << s;
  }
  SetDB(nullptr);
}

// The offside port's end-to-end effect (docs/re/pes-re-offside-port.md 4): a
// PES-faithful line, test and category plus the AI's offside readers. The pin
// is the frame the line is read in: with this engine's own-goal side passed to
// `OffsideLineX` instead of the team's attack direction the line lands on the
// ball, and every attacker behind the ball at his team's touch is offside - the
// same match then whistles 23 (10 for one side, 13 for the other). With the
// frame right the count sits inside PES's band, which for this side's tempo
// means single digits over the whole five minutes.
TEST(SimMatch, OffsideCountStaysUnderTheWrongFrameTripwire) {
  REQUIRE_RIG();
  const Outcome o = Play(SEED, FULL_MATCH_MINUTES);
  const int offsides = o.offsides[0] + o.offsides[1];
  EXPECT_LE(offsides, kOffsideTripwire_5min) << o.report;
  // Both sides are treated by the same rule, so neither side can run away with
  // the whole of it.
  EXPECT_LE(o.offsides[0], kOffsideTripwire_5min);
  EXPECT_LE(o.offsides[1], kOffsideTripwire_5min);
}

TEST(SimMatch, SameSeedPlaysTheSameMatch) {
  REQUIRE_RIG();
  const Outcome a = Play(SEED, SHORT_MATCH_MINUTES);
  const Outcome b = Play(SEED, SHORT_MATCH_MINUTES);
  ASSERT_TRUE(a.finished);
  EXPECT_EQ(a.hash, b.hash);
  EXPECT_EQ(a.report, b.report);
}

TEST(SimMatch, AnotherSeedPlaysAnotherMatch) {
  REQUIRE_RIG();
  EXPECT_NE(Play(SEED, SHORT_MATCH_MINUTES).hash, Play(SEED + 1, SHORT_MATCH_MINUTES).hash);
}

// The seed-7 stalls (20-minute setup): from 107 s and from 174 s a throw-in restart
// is started but never taken. The ball lies beyond the line (pushed off its spot in
// the hold), and the taker's touch is refused as out of bounds. The window covers both.
// A 3-minute setup does not reproduce them, so the test uses 20.
constexpr unsigned int kThrowInSeed = 7;
constexpr int kThrowInMatchMinutes = 20;
constexpr unsigned long kThrowInWindow_ms = 200ul * 1000ul;
// ponytail: a GF bound, not a PES value. PES's CPU taker throws about 1.5 s
// after he is ready (pes-re-throwin-choreography.md EV-TI-0015).
constexpr unsigned long kMaxThrowInWait_ms = 10ul * 1000ul;

TEST(SimMatch, AThrowInTakerTakesTheThrow) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(kThrowInSeed, kThrowInMatchMinutes);
  double longestWait_ms = 0;
  {
    pestest::FillMissingPesConstants(&setup);
    Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
    double waitStart_ms = 0;
    bool waiting = false;
    while (match.GetSimTime_ms() < kThrowInWindow_ms) {
      match.Process();
      const RefereeBuffer& buffer = match.GetRefereeBuffer();
      const bool throwInStarted =
          buffer.active && buffer.started && buffer.desiredSetPiece == e_SetPiece_ThrowIn;
      if (throwInStarted && !waiting) {
        waiting = true;
        waitStart_ms = match.GetSimTime_ms();
      }
      if (!throwInStarted) waiting = false;
      if (waiting)
        longestWait_ms = std::max(longestWait_ms, match.GetSimTime_ms() - waitStart_ms);
    }
  }
  SetDB(nullptr);
  EXPECT_LE(longestWait_ms, kMaxThrowInWait_ms);
}

TEST(SimMatch, FullMatchRunsToTheWhistle) {
  REQUIRE_RIG();
  const Outcome o = Play(SEED, FULL_MATCH_MINUTES);
  ASSERT_TRUE(o.finished);
  EXPECT_NE(o.report.find("Full match complete: HOM " + std::to_string(o.goals[0]) + " - " +
                          std::to_string(o.goals[1]) + " AWA"),
            std::string::npos)
      << o.report;
  EXPECT_NE(o.report.find("[determinism] "), std::string::npos);
}

// The strategy decider reads each side's rating from the match setup, which is
// written once at match load (pes::SetupStrength over the starting eleven). It
// is in place before the first tick, so the decider never sees a stale zero.
TEST(SimMatch, SetupStrengthIsWrittenAtMatchLoadFromTheStartingEleven) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(SEED, SHORT_MATCH_MINUTES);
  pestest::FillMissingPesConstants(&setup);
  {
    Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, Properties());
    for (int side = 0; side < 2; side++) {
      std::vector<Player*> starters;
      match.GetActiveTeamPlayers(side, starters);
      ASSERT_EQ(starters.size(), 11u);
      std::vector<float> overalls;
      for (Player* p : starters) overalls.push_back(p->GetPlayerData()->GetAverageStat());
      EXPECT_FLOAT_EQ(match.GetTeamSetupStrength(side), pes::SetupStrength(overalls));
      EXPECT_GT(match.GetTeamSetupStrength(side), 0.0f);
    }
  }
  SetDB(nullptr);
}

// The recorded verdict is what the ball physically did during the kick: the goal
// plane crossed inside the frame is a goal, a touch by the defending keeper is a
// save, anything else a miss. Both are read from the ball and the engine's touch
// record each tick, not from the controller's own classifier.
TEST(SimMatch, ShootoutVerdictIsWhatTheBallDid) {
  REQUIRE_RIG();
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(GF_SOURCE_DIR "/assets/fixtures/teams.sql"));
  SetDB(&db);
  MatchSetup setup = MakeSetup(7, SHORT_MATCH_MINUTES);
  pestest::FillMissingPesConstants(&setup);
  Properties shootoutOnly;
  shootoutOnly.SetBool("penalty_shootout_only", true);
  {
    Match match(new MatchData(HOME_TEAM, AWAY_TEAM), setup, shootoutOnly);
    PenaltyShootoutController* shootout = match.GetPenaltyShootout();
    const float wholeBallOverLine_m = lineHalfW + match.GetPesBallConstants().image.ballRadius;
    bool wasLive = false;
    bool crossedInFrame = false;
    bool keeperTouched = false;
    bool beyondLine = false;
    int kick = 0;
    while (!shootout->IsFinished() && match.GetSimTime_ms() < kMaxSimTime_ms) {
      match.Process();
      if (shootout->IsKickLive() && !wasLive) {
        crossedInFrame = false;
        keeperTouched = false;
        beyondLine = false;
      }
      if (wasLive || shootout->IsKickLive()) {
        const float goalX = shootout->GetGoalX();
        const float side = goalX > 0.0f ? 1.0f : -1.0f;
        const Vector3 ball = match.GetBall()->Predict(0);
        // A goal is the whole ball over the line: its centre the line's half-width
        // plus the ball's radius past it.
        const bool beyond = (ball.coords[0] - goalX) * side > wholeBallOverLine_m;
        if (beyond && !beyondLine && std::fabs(ball.coords[1]) <= goalHalfWidth &&
            ball.coords[2] >= 0.0f && ball.coords[2] <= goalHeight)
          crossedInFrame = true;
        beyondLine = beyond;
        Player* keeper = match.GetTeam(1 - shootout->GetState().shootingTeam)->GetGoalie();
        if (keeper && match.GetLastTouchPlayer() == keeper)
          keeperTouched = true;
      }
      if (wasLive && !shootout->IsKickLive()) {
        const PenaltyShootout::e_Outcome physical =
            crossedInFrame ? PenaltyShootout::e_Outcome_Goal
            : keeperTouched ? PenaltyShootout::e_Outcome_Save
                            : PenaltyShootout::e_Outcome_Miss;
        EXPECT_EQ(shootout->GetState().lastOutcome, physical) << "kick " << kick;
        kick++;
      }
      wasLive = shootout->IsKickLive();
    }
    EXPECT_TRUE(shootout->IsFinished());
    EXPECT_GT(kick, 0);
  }
  SetDB(nullptr);
}
