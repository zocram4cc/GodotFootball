// A played match's foul stop, end to end: the referee's whistle runs PES's commit 0x143F546F0 (PesWorld::CommitJudge), whose
// writes (0x14050E770 into JR = MI+0x1554) are the stoppage words the match-info block holds, and whose card level (JR+0x38)
// is the booking. The match is a PES-loop match of the first searched seed that reaches a foul stop and ends it within the
// two minutes (each seed is one gfsim-sized run; the search stops at the first hit, so a hit on seed 1 costs one run).
// docs/ports/pes-port-CardWhistle.md.

#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr int kMinutes = 2;
// The searched seeds, in order. A seed counts when a direct foul is whistled and its stop is taken inside the match.
constexpr unsigned int kSeeds[] = {7u, 3u, 5u, 9u, 13u, 11u, 2u, 15u, 16u, 18u, 19u, 25u};
constexpr uint32_t kRestartFreeKick = 5, kRestartPenalty = 6;  // MI+0x1554 (pes-re-ref-record.md 1.2)
constexpr uint32_t kFoulKindDirect = 2;
constexpr uint32_t kLevelYellow = 3, kLevelRed = 4;
constexpr std::size_t kResultStampOffset = pes::kMiRestartKindOffset + 0x4C;  // JR+0x4C

// Runs a PES-loop match of `seed`, calling `onTick` after every tick until it returns true or the match ends.
void RunMatch(unsigned int seed, const std::function<bool(Match&)>& onTick) {
  const char* root = std::getenv("PES21_ROOT");
  ASSERT_TRUE(root != nullptr) << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  blunted::Database db;
  ASSERT_TRUE(db.LoadSqlScript(std::string(GF_SOURCE_DIR) + "/assets/fixtures/teams.sql"));
  SetDB(&db);

  auto setup = std::make_unique<MatchSetup>();
  setup->homeTeamDbId = HOME_TEAM;
  setup->awayTeamDbId = AWAY_TEAM;
  setup->seed = seed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  while (!match.IsGameOver()) {
    match.Process();
    if (onTick(match)) return;
  }
}

struct FoulStop {
  unsigned int seed = 0;
  // The stoppage words on the tick of the whistle.
  uint32_t restart = 0, side = 0, reason = 0, kind = 0, level = 0, fouler = 0, victim = 0;
  // GF's own state on that tick.
  e_SetPiece setPiece = e_SetPiece_None;
  int buffered_team = -1;
  int foulerTeam = -1;
  int yellowsAdded = 0, redsAdded = 0;  // the fouler's side's cards shown on that tick
  // The words once the stop was taken.
  uint32_t restartAfter = 0, levelAfter = 0, foulerAfter = 0, stampAfter = 0;
  uint32_t stamp = 0;  // JR+0x4C on the whistle's tick: the pending record's frame stamp the commit copies
};

uint32_t MiWord(Match& match, std::size_t offset) { return pes::LoadU32(match.PesWorldOf().MatchInfo(), offset); }

// The first direct-foul stop of `seed`, followed until its restart is taken; nullopt when the seed has none.
std::optional<FoulStop> FoulStopOf(unsigned int seed) {
  FoulStop stop;
  stop.seed = seed;
  bool whistled = false, taken = false;
  int yellow[2] = {0, 0}, red[2] = {0, 0};
  RunMatch(seed, [&](Match& match) {
    const int nowYellow[2] = {match.GetMatchData()->GetYellowCards(0), match.GetMatchData()->GetYellowCards(1)};
    const int nowRed[2] = {match.GetMatchData()->GetRedCards(0), match.GetMatchData()->GetRedCards(1)};
    const RefereeBuffer& buffer = match.GetRefereeBuffer();
    if (!whistled && buffer.active && MiWord(match, pes::kMiFoulKindOffset) == kFoulKindDirect) {
      whistled = true;
      stop.restart = MiWord(match, pes::kMiRestartKindOffset);
      stop.side = MiWord(match, pes::kMiRestartSideOffset);
      stop.reason = MiWord(match, pes::kMiStopReasonOffset);
      stop.kind = MiWord(match, pes::kMiFoulKindOffset);
      stop.level = MiWord(match, pes::kMiCardLevelOffset);
      stop.fouler = MiWord(match, pes::kMiFoulerOffset);
      stop.victim = MiWord(match, pes::kMiVictimOffset);
      stop.stamp = MiWord(match, kResultStampOffset);
      stop.setPiece = buffer.desiredSetPiece;
      stop.buffered_team = buffer.teamID;
      for (int team = 0; team < 2; ++team) {
        std::vector<Player*> onPitch;
        match.GetTeam(team)->GetActivePlayers(onPitch);
        for (Player* p : onPitch)
          if (static_cast<uint32_t>(match.PesSlotOf(p)) == stop.fouler) stop.foulerTeam = p->GetTeamID();
      }
      if (stop.foulerTeam >= 0) {
        stop.yellowsAdded = nowYellow[stop.foulerTeam] - yellow[stop.foulerTeam];
        stop.redsAdded = nowRed[stop.foulerTeam] - red[stop.foulerTeam];
      }
    } else if (whistled && !buffer.active) {
      taken = true;
      stop.restartAfter = MiWord(match, pes::kMiRestartKindOffset);
      stop.levelAfter = MiWord(match, pes::kMiCardLevelOffset);
      stop.foulerAfter = MiWord(match, pes::kMiFoulerOffset);
      stop.stampAfter = MiWord(match, kResultStampOffset);
    }
    for (int team = 0; team < 2; ++team) {
      yellow[team] = nowYellow[team];
      red[team] = nowRed[team];
    }
    return taken;
  });
  if (!taken) return std::nullopt;
  return stop;
}

// The first searched seed with a direct foul stop, found once and shared by the tests (each seed is a full match run).
const FoulStop* TheFoulStop() {
  static const std::optional<FoulStop> found = [] {
    for (const unsigned int seed : kSeeds) {
      std::fprintf(stderr, "foul whistle search: seed %u\n", seed);
      if (const std::optional<FoulStop> stop = FoulStopOf(seed)) return stop;
    }
    return std::optional<FoulStop>();
  }();
  return found ? &*found : nullptr;
}

}  // namespace

// The stoppage words of a played foul are the whistle's: the restart kind and the side are the ones GF's referee armed
// (PES's box test and GF's penalty geometry agree on this foul), the reason is the commit's, the kind is a direct foul, and
// the fouler and victim are two players of opposite sides.
TEST(FoulWhistle, ThePlayedFoulStopCarriesTheWhistlesWords) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  const FoulStop* stop = TheFoulStop();
  ASSERT_TRUE(stop != nullptr) << "no searched seed reached a foul stop and took it";
  RecordProperty("seed", static_cast<int>(stop->seed));
  std::fprintf(stderr, "foul whistle: seed %u restart %u side %u level %u fouler %u victim %u yellow +%d red +%d\n", stop->seed,
               stop->restart, stop->side, stop->level, stop->fouler, stop->victim, stop->yellowsAdded, stop->redsAdded);
  EXPECT_EQ(stop->kind, kFoulKindDirect);
  EXPECT_EQ(stop->restart, stop->setPiece == e_SetPiece_Penalty ? kRestartPenalty : kRestartFreeKick);
  EXPECT_TRUE(stop->restart == kRestartFreeKick || stop->restart == kRestartPenalty);
  EXPECT_EQ(stop->side, static_cast<uint32_t>(stop->buffered_team));
  EXPECT_EQ(stop->reason, 5u);
  ASSERT_GE(stop->foulerTeam, 0) << "the whistle's fouler slot names no player on the pitch";
  EXPECT_NE(stop->side, static_cast<uint32_t>(stop->foulerTeam)) << "the free kick goes to the fouler's opponents";
  EXPECT_NE(stop->fouler / pes::kSlotsPerSide, stop->victim / pes::kSlotsPerSide);
}

// The booking is the commit's card level: 3 is a yellow, 4 a red, 1 and 2 give no card (2 is a warning).
TEST(FoulWhistle, TheBookingIsTheLevelTheCommitWrote) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  const FoulStop* stop = TheFoulStop();
  ASSERT_TRUE(stop != nullptr) << "no searched seed reached a foul stop and took it";
  ASSERT_GE(stop->level, 1u);
  ASSERT_LE(stop->level, kLevelRed);
  EXPECT_EQ(stop->yellowsAdded, stop->level == kLevelYellow ? 1 : 0) << "level " << stop->level;
  EXPECT_EQ(stop->redsAdded, stop->level == kLevelRed ? 1 : 0) << "level " << stop->level;
}

// PES clears JR when the stop ends: once the restart is taken the whistle's words are gone.
TEST(FoulWhistle, TheWhistlesWordsEndWithTheStop) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  const FoulStop* stop = TheFoulStop();
  ASSERT_TRUE(stop != nullptr) << "no searched seed reached a foul stop and took it";
  EXPECT_EQ(stop->restartAfter, 0u);
  EXPECT_EQ(stop->levelAfter, 0u);
  EXPECT_EQ(stop->foulerAfter, static_cast<uint32_t>(pes::kMiNoSlot));
  // The words the commit copies beyond the stoppage ones (JR+0x4C: the record's frame stamp) are PES's too and end with the stop.
  EXPECT_NE(stop->stamp, 0u);
  EXPECT_EQ(stop->stampAfter, 0u);
}
