#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>

#include "data/matchdata.hpp"
#include "gametypes.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesgoalkick.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 7;
constexpr int kMinutes = 2;
// PES's throwin_release_run_000_short (throwin 4.2, the carry's short row). The installed clip supplies its key.
constexpr int kReleaseRunShortMotion = 0x9bb;
// The release key's horizontal length is checked to this tolerance (m): one tick of the taker's root motion.
constexpr float kKeyTolerance_m = 0.05f;

// Runs a PES-loop match for two minutes, calling `onTick` after every tick.
void RunMatch(const std::function<void(Match&)>& onTick) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
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
  setup->seed = kSeed;
  setup->durationMinutes = kMinutes;
  setup->animationDir = std::string(GF_SOURCE_DIR) + "/assets/gf_anims";
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";

  Properties config;
  config.SetBool("pes_loop", true);
  Match match(new MatchData(HOME_TEAM, AWAY_TEAM), *setup, config);
  while (!match.IsGameOver()) {
    match.Process();
    if (onTick) onTick(match);
  }
}

}  // namespace

// PES releases a throw-in from behind the line: the release holds the ball at the hand's key offset, inside the touchline,
// until the release (pes-re-throwin-choreography.md 4.2, 5.3). So when the throw's touch ends the set piece, the ball lies
// inside the line. Counted over a two-minute seed-7 match.
TEST(PesThrowInRelease, AThrowInIsReleasedFromInsideTheLine) {
  int releases = 0, outside = 0;
  bool throwInTaking = false;
  RunMatch([&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) throwInTaking = true;
    if (throwInTaking && !match.IsInSetPiece()) {
      throwInTaking = false;
      releases++;
      if (std::fabs(match.GetBall()->Predict(0).coords[1]) > pesFieldHalfWidth) outside++;
    }
  });
  EXPECT_GT(releases, 0) << "no throw-in was released within two minutes";
  EXPECT_EQ(outside, 0) << outside << " of " << releases << " released throw-ins lay outside the touchline at the touch";
}

// The released ball is held at the release clip's key offset from the taker's root (PES's hand, not his root). The
// horizontal length of that offset is independent of the heading, so the check needs no axis convention beyond ToFox:
// the horizontal length of the installed key (its Fox X and Z) equals the ball's horizontal distance from the root.
TEST(PesThrowInRelease, AThrowInIsReleasedAtThePesReleaseKey) {
  int releases = 0, mismatched = 0;
  float worst_m = 0.0f;
  bool throwInTaking = false;
  RunMatch([&](Match& match) {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) throwInTaking = true;
    if (!(throwInTaking && !match.IsInSetPiece())) return;
    throwInTaking = false;
    releases++;
    const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(kReleaseRunShortMotion);
    const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
    const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
    const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), 1.0f, false, k);
    const float keyHorizontal_m = std::sqrt(key.x * key.x + key.z * key.z);
    const Vector3 ball = match.GetBall()->Predict(0);
    const Vector3 root = b.taker->GetPosition();
    const float dx = ball.coords[0] - root.coords[0], dy = ball.coords[1] - root.coords[1];
    const float gap_m = std::fabs(std::sqrt(dx * dx + dy * dy) - keyHorizontal_m);
    worst_m = std::max(worst_m, gap_m);
    if (gap_m > kKeyTolerance_m) mismatched++;
  });
  EXPECT_GT(releases, 0) << "no throw-in was released within two minutes";
  EXPECT_EQ(mismatched, 0) << mismatched << " of " << releases << " releases were not at the key's horizontal offset (worst "
                           << worst_m << " m)";
}
