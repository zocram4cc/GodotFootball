#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/gameplaytuning.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/player/humanoid/humanoid.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/matchsetup.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

// The stalemate breaker is GF's band-aid: once nobody has moved the ball for five seconds it designates the
// nearest man, whatever the referee has armed. PES never needs it. So a firing is a defect in one of two places:
// the breaker counted idle time over a restart the referee had armed, or a live ball held by a man who never
// released it. Both are counted over one whole PES match (pes_loop on), tick by tick.
namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;

struct Reality {
  blunted::Database* db = nullptr;
  std::unique_ptr<blunted::Database> dbHolder;
  std::unique_ptr<pes::PesVfs> vfs;
  std::optional<pesrig::Rig> rig;
  std::unique_ptr<MatchSetup> setup;
  std::unique_ptr<Match> match;
  MatchData* data = nullptr;

  bool Ready() {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return false;
    std::string err;
    vfs = std::make_unique<pes::PesVfs>();
    if (!vfs->Mount(root, &err)) return false;
    rig = pesrig::BuildRig(*vfs, &err);
    if (!rig) return false;
    dbHolder = std::make_unique<blunted::Database>();
    if (!dbHolder->LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql")) return false;
    db = dbHolder.get();
    SetDB(db);
    return true;
  }

  void Start(unsigned int seed, int minutes) {
    setup = std::make_unique<MatchSetup>();
    setup->homeTeamDbId = HOME_TEAM;
    setup->awayTeamDbId = AWAY_TEAM;
    setup->seed = seed;
    setup->durationMinutes = minutes;
    setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
    setup->rig = &*rig;
    pestest::FillMissingPesConstants(setup.get());
    data = new MatchData(HOME_TEAM, AWAY_TEAM);
    Properties config;
    config.SetBool("pes_loop", true);  // the PES tick order; without it the match runs GF's
    match = std::make_unique<Match>(data, *setup, config);
  }
};

// One whole PES match, with each breaker firing filed by the state of play on its tick.
struct BreakerCensus {
  bool ready = false;
  bool finished = false;
  long armedFirings = 0;    // the referee had a restart armed (its buffer is active)
  long liveFirings = 0;     // play is live, nothing is armed: a man on a ball that nobody plays
  long unarmedFirings = 0;  // play is off and nothing is armed
  long restingEpisodes = 0;  // ticks of the longest run: the designated man holds the ball by distance, no retainer, ball at rest
  long restingLongest = 0;
};

BreakerCensus RunPesMatch(unsigned int seed) {
  BreakerCensus c;
  Reality r;
  if (!r.Ready()) return c;
  c.ready = true;
  r.Start(seed, kMinutes);
  const int kMaxTicks = 30 * 60 * 100;  // the cap sim_match_test.cpp uses
  long run = 0;
  for (int ticks = 0; !r.match->IsGameOver() && ticks < kMaxTicks; ++ticks) {
    const unsigned long before = r.match->GetStalemateCount();
    r.match->Process();
    Match& m = *r.match;
    Player* des = m.GetDesignatedPossessionPlayer();
    const bool resting = m.IsInPlay() && !m.IsInSetPiece() && des && des->HasPossession() && !m.GetBallRetainer() &&
                         m.GetBall()->GetMovement().GetLength() <= GameplayTuning::kStalemateBallSpeed_ms;
    run = resting ? run + 1 : 0;
    if (run > c.restingLongest) c.restingLongest = run;
    if (run == 1) c.restingEpisodes++;
    if (std::getenv("RESTING_TRACE") && resting && run % 50 == 1)
      printf("[rest] seed %u f=%lu run=%ld man=%s ball=(%.2f,%.2f) man=(%.2f,%.2f) clip=%s\n", seed, m.GetSimFrame(), run,
             des->GetPlayerData()->GetLastName().c_str(), m.GetBall()->Predict(0).coords[0],
             m.GetBall()->Predict(0).coords[1], des->GetPosition().coords[0], des->GetPosition().coords[1],
             des->CastHumanoid()->GetCurrentAnim()->anim ? des->CastHumanoid()->GetCurrentAnim()->anim->GetName().c_str() : "-");
    if (m.GetStalemateCount() == before) continue;
    if (m.GetRefereeBuffer().active)
      c.armedFirings++;
    else if (m.IsInPlay() && !m.IsInSetPiece())
      c.liveFirings++;
    else
      c.unarmedFirings++;
  }
  c.finished = r.match->IsGameOver();
  return c;
}

const BreakerCensus& RunSeedSevenPesMatch() {
  static const BreakerCensus census = RunPesMatch(kSeed);
  return census;
}

TEST(PesStall, ProbeResting) {
  for (unsigned int seed : {7u, 3u, 11u}) {
    const BreakerCensus c = RunPesMatch(seed);
    printf("[probe] seed %u episodes %ld longest %ld firings live %ld armed %ld unarmed %ld\n", seed, c.restingEpisodes,
           c.restingLongest, c.liveFirings, c.armedFirings, c.unarmedFirings);
  }
}

// The referee arms a restart and releases it when the taker is ready. While it is armed the referee owns the
// ball; a breaker that forces a man over it is counting a hold PES does not stall on.
TEST(PesStall, TheBreakerNeverForcesABallUnderAnArmedRestart) {
  const BreakerCensus& c = RunSeedSevenPesMatch();
  if (!c.ready) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(c.finished) << "the match did not reach full time";
  EXPECT_EQ(c.armedFirings, 0) << "the breaker forced a man over a restart the referee had armed, "
                                  "for " << c.armedFirings << " ticks";
}

// A live ball that lies still for five seconds is a man who holds it and does nothing with it. PES's holder keeps
// the ball until he releases it, so the breaker must not have to take it from him.
TEST(PesStall, ALiveBallNeverLiesStillLongEnoughForTheBreaker) {
  const BreakerCensus& c = RunSeedSevenPesMatch();
  if (!c.ready) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(c.finished) << "the match did not reach full time";
  EXPECT_EQ(c.liveFirings, 0) << "the breaker forced a live ball for " << c.liveFirings << " ticks";
}

}  // namespace
