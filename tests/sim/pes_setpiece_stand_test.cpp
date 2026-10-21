// PES places a restart's taker on his SetKick stand point when the restart is prepared (docs/re/pes-re-kicker-standby.md
// section 4): the spot plus the clip's contact offset, negated and turned by the heading (the placer FUN_140732D40). The
// goal kick's offset is the clip's first ball hit record (FUN_1406E8CC0 at index 0), so the keeper stands that record's
// horizontal length from the spot, whatever the heading. The test reads the record from the install and compares the body.
//
// Only the goal kick is tested here. The free kick, corner and penalty run-ups are remapped by the player's style table
// (FUN_1406B3560/3780 over the object at _DAT_1436F2600), which GF does not load yet.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/properties.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "onthepitch/matchsetup.hpp"
#include "onthepitch/pesgoalkick.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"
#include "pes/vfs.hpp"
#include "pes_match_constants.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"
#include "utils/database.hpp"

namespace {

constexpr int kHomeTeam = 1, kAwayTeam = 2;
constexpr unsigned int kSeed = 1000;
// The goal kick's strike clip (gkgoalkick_instep_0_0): its first hit record gives the keeper's stand point.
constexpr int kGoalKickStrikeMotion = 0x6b1;
// The keeper's distance from the spot is the record's horizontal length; the placed body reaches it to this tolerance.
constexpr float kPlacementTolerance_m = 0.05f;

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

#define REQUIRE_PES_MATCH(config)                                                                  \
  if (!SharedRig()) GTEST_SKIP() << "PES21_ROOT not set: the player skeleton comes from PES";     \
  blunted::Database* db = SharedDb();                                                              \
  ASSERT_TRUE(db) << "assets/fixtures/teams.sql";                                                  \
  SetDB(db);                                                                                       \
  std::unique_ptr<Match> match = MakeMatch(config);                                                \
  if (!match) GTEST_SKIP() << "PES2021.exe: the match constants are unavailable"

// The horizontal length of a clip's contact offset in metres: the first ball hit record's place (cm) times the install's
// centimetre scale. Every stand point is this far from its spot, whatever the heading turns it to.
float ContactOffsetLengthM(Match& match, int motionNo) {
  const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(motionNo);
  EXPECT_FALSE(hits.empty()) << "motion " << motionNo << " has no hit record in the install";
  if (hits.empty()) return 0.0f;
  const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
  return std::hypot(hits.front().ballX, hits.front().ballZ) * k.centimetre_m;
}

// Prepares a restart the way the referee does (Referee::PrepareSetPiece): the ball and the restart at the spot, then both
// teams' controllers. Returns the taker of the restarting team.
Player* PrepareRestart(Match& match, e_SetPiece kind, int teamID, const Vector3& spot) {
  match.ResetSituation(spot);
  match.GetTeam(0)->GetController()->PrepareSetPiece(kind, teamID);
  match.GetTeam(1)->GetController()->PrepareSetPiece(kind, teamID);
  return match.GetTeam(teamID)->GetController()->GetPieceTaker();
}

float DistanceFromSpotM(const Player& taker, const Vector3& spot) {
  return (taker.GetPosition() - spot).Get2D().GetLength();
}

}  // namespace

// The keeper of the restarting side stands on the goal kick's stand point at every spot he is prepared for. His formation
// position happens to lie at the offset's length from one of the two spots; the second spot tells the two apart.
TEST(SetKickStand, TheGoalKeeperIsPlacedOnTheStandPointAtEverySpot) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const float expected = ContactOffsetLengthM(*match, kGoalKickStrikeMotion);
  for (const Vector3 spot : {Vector3(-47.0f, 0.0f, 0.0f), Vector3(-47.0f, 3.0f, 0.0f)}) {
    Player* taker = PrepareRestart(*match, e_SetPiece_GoalKick, 0, spot);
    ASSERT_NE(taker, nullptr) << "no goal kick taker was chosen";
    EXPECT_NEAR(DistanceFromSpotM(*taker, spot), expected, kPlacementTolerance_m)
        << "the keeper is not on the SetKick stand point for the spot (" << spot.coords[0] << ", " << spot.coords[1]
        << ")";
  }
}

// PES's throw-in stand (ThrowinLoop's SetKick, docs/re/pes-re-throwin-choreography.md 4.1) is the release key's ball offset
// from clip 0xcfa's second record. The live path-B placement was 0.773 m outside the restart point at a body scale of 0.972
// (the doc's four observations); the same record at that scale must reproduce it.
TEST(SetKickStand, TheThrowInStandIsTheReleaseKeyOffsetAtTheLiveBodyScale) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  constexpr float kLiveBodyScale = 0.972f;
  constexpr float kLiveStandIn_m = 0.773f;
  const std::vector<pes::HitRecord>& hits = match->GetPesMbinfo().Hits(0xcfa);
  ASSERT_GT(hits.size(), 1u) << "clip 0xcfa has no second record";
  const pes::GoalKickConstants k = pes::GoalKickConstantsFrom(match->GetPesSetPieceTakerConstants());
  const pes::Vec3 offset =
      pes::ClipOffsetMetres(pes::Vec3(hits[1].ballX, hits[1].ballY, hits[1].ballZ), kLiveBodyScale, false, k);
  EXPECT_NEAR(std::hypot(offset.x, offset.z), kLiveStandIn_m, 0.01f);
}

// The angle between two ground directions, in degrees.
float AngleBetweenDeg(const Vector3& a, const Vector3& b) {
  const float cross = a.coords[0] * b.coords[1] - a.coords[1] * b.coords[0];
  const float dot = a.coords[0] * b.coords[0] + a.coords[1] * b.coords[1];
  const float radToDeg = 180.0f / std::acos(-1.0f);
  return std::fabs(std::atan2(cross, dot)) * radToDeg;
}

// A placed keeper faces into the pitch and keeps that heading while he waits for the kick: PES's standby plays with the
// SetKick yaw, not the ball (docs/re/pes-re-kicker-standby.md section 4). The spot is off the keeper's axis, so a heading
// toward the ball is not the pitch. The keeper at the -X end faces +X.
TEST(SetKickStand, ThePlacedKeeperKeepsHisHeadingWhileHeWaits) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  constexpr int kWaitTicks = 600;                // eleven seconds: past the restart's hold and the goal kick ladder's windows
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  const Vector3 spot(-47.0f, 3.0f, 0.0f);
  const Vector3 pitch(1.0f, 0.0f, 0.0f);
  Player* taker = PrepareRestart(*match, e_SetPiece_GoalKick, 0, spot);
  ASSERT_NE(taker, nullptr) << "no goal kick taker was chosen";
  const TeamAIController* controller = match->GetTeam(0)->GetController();
  ASSERT_TRUE(controller->IsPieceTakerHeld(taker)) << "the keeper is not held after the placement";
  const Vector3 placed = taker->GetDirectionVec();
  EXPECT_LE(AngleBetweenDeg(pitch, placed), kHeadingTolerance_deg)
      << "the placed keeper does not face the pitch (" << placed.coords[0] << ", " << placed.coords[1] << ")";
  float worst = 0.0f;
  for (int t = 0; t < kWaitTicks; ++t) {
    match->Process();
    if (!controller->IsPieceTakerHeld(taker)) break;  // released: the wait is over
    const float turned = AngleBetweenDeg(placed, taker->GetDirectionVec());
    if (turned > worst) worst = turned;
  }
  EXPECT_LE(worst, kHeadingTolerance_deg) << "the placed keeper turned " << worst << " degrees while he waited";
}
