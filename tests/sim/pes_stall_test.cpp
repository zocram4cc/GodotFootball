#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
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
};

const BreakerCensus& RunSeedSevenPesMatch() {
  static const BreakerCensus census = [] {
    BreakerCensus c;
    Reality r;
    if (!r.Ready()) return c;
    c.ready = true;
    r.Start(kSeed, kMinutes);
    const int kMaxTicks = 30 * 60 * 100;  // the cap sim_match_test.cpp uses
    for (int ticks = 0; !r.match->IsGameOver() && ticks < kMaxTicks; ++ticks) {
      const unsigned long before = r.match->GetStalemateCount();
      r.match->Process();
      if (r.match->GetStalemateCount() == before) continue;
      if (r.match->GetRefereeBuffer().active)
        c.armedFirings++;
      else if (r.match->IsInPlay() && !r.match->IsInSetPiece())
        c.liveFirings++;
      else
        c.unarmedFirings++;
    }
    c.finished = r.match->IsGameOver();
    return c;
  }();
  return census;
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
// DISABLED 28-10-26 (owner decision, batch AM landing KickSoftMin): the PES kick-quality fold moved seed 7's match onto
// the known loose-ball pickup gap (80 firings): GF's trap takes its clip from the row picker 0x14081FF70, PES's reaction
// trap from the Trap action's kind-0 chain (0x14079A470 -> 0x14079AFD0), which is not ported yet
// (docs/ports/pes-port-OwnerPickup2.md, -TrapReplay.md). The same defect fires 147 times on branch StoppageFlow.
// Re-enable when the kind-0 trap chain lands.
TEST(PesStall, DISABLED_ALiveBallNeverLiesStillLongEnoughForTheBreaker) {
  const BreakerCensus& c = RunSeedSevenPesMatch();
  if (!c.ready) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(c.finished) << "the match did not reach full time";
  EXPECT_EQ(c.liveFirings, 0) << "the breaker forced a live ball for " << c.liveFirings << " ticks";
}

}  // namespace
