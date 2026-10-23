// PES places a restart's taker on his SetKick stand point when the restart is prepared (docs/re/pes-re-kicker-standby.md
// section 4): the spot plus the clip's contact offset, negated and turned by the heading (the placer FUN_140732D40). The goal
// kick, the corner, the free kick and the penalty share that placer. The free kick and the penalty run-ups are the anime cells
// (0x509 and 0x8EB, kind 9) walked through the player remap over the derivation table (pes/derivation.hpp). GF has no signature
// card, style stat or second kicker, so every taker walks with the defaults. The tests read the record from the install.
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
#include "pes/derivation.hpp"
#include "pes/exe.hpp"
#include "pes/mbinfo.hpp"
#include "pes/movestep.hpp"
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
  // Two and a half seconds: the ladder releases the kick about 240 frames after the restart is ready (pes-re-A5Judge.md 9.3),
  // and the strike's own motion is not part of the wait.
  constexpr int kWaitTicks = 150;
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  const Vector3 spot(-47.0f, 3.0f, 0.0f);
  const Vector3 pitch(1.0f, 0.0f, 0.0f);
  // The match's first tick prepares its opening kick-off, which would take the keeper's placement back: the restart is
  // prepared after that tick.
  match->Process();
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
    if (match->IsInPlay()) break;  // released: the kick is in play
    EXPECT_TRUE(controller->IsPieceTakerHeld(taker)) << "the keeper lost his hold at tick " << t;
    const float turned = AngleBetweenDeg(placed, taker->GetDirectionVec());
    if (turned > worst) worst = turned;
  }
  EXPECT_LE(worst, kHeadingTolerance_deg) << "the placed keeper turned " << worst << " degrees while he waited";
}

// A held set-piece taker is not steered by the default locomotion. Every standby but the two kick-off loops returns 1 from
// its slot 10, and FUN_1406F0C20 (the default locomotion driver) returns on that answer before any steering
// (docs/re/pes-re-kicker-standby.md 1-2: GoalKickStandby 0x140775730, FreeKickLoop 0x140742D90, CornerKickLoop 0x1407343A0,
// ThrowinLoop 0x1407952D0, PenaltyKickLoop 0x140742BC0). Built on main, MOVESTEP took the placed keeper into TURN_STOP on
// the first tick and turned him toward the ball.
TEST(SetKickStand, AHeldTakerIsNotSteeredByMoveStep) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  constexpr int kWaitTicks = 150;
  match->Process();  // the opening kick-off's preparation, as the heading test does
  Player* taker = PrepareRestart(*match, e_SetPiece_GoalKick, 0, Vector3(-47.0f, 3.0f, 0.0f));
  ASSERT_NE(taker, nullptr) << "no goal kick taker was chosen";
  const TeamAIController* controller = match->GetTeam(0)->GetController();
  const pes::MoveState before = taker->GetMoveStep().state;
  int heldTicks = 0, steered = 0;
  for (int t = 0; t < kWaitTicks; ++t) {
    match->Process();
    if (match->IsInPlay() || !controller->IsPieceTakerHeld(taker)) break;
    ++heldTicks;
    if (taker->GetMoveStep().state != before) ++steered;
  }
  ASSERT_GT(heldTicks, 0) << "the keeper was never held";
  EXPECT_EQ(steered, 0) << "MOVESTEP left its state on " << steered << " of " << heldTicks << " held ticks";
}

// PES's corner SetKick turns the stand point by the corner heading: the side yaw plus the bearing from the spot to the
// penalty spot on the spot's own end (FUN_1408B4560, restart kind 4). The bearing points at that penalty spot from either
// end and either touchline.
TEST(SetKickStand, TheCornerRestartYawPointsAtThePenaltySpotFromEitherEnd) {
  constexpr float kAimTolerance_deg = 0.5f;  // a GF tolerance, not a PES value
  const float radPerDeg = std::acos(-1.0f) / 180.0f;
  for (const Vector3 spot : {Vector3(52.0f, 33.5f, 0.0f), Vector3(52.0f, -33.5f, 0.0f), Vector3(-52.0f, 33.5f, 0.0f),
                             Vector3(-52.0f, -33.5f, 0.0f)}) {
    const pes::Vec3 spotPes(spot.coords[0], spot.coords[1], spot.coords[2]);
    const float yaw = pes::CornerRestartYawDeg(spotPes, pesPenaltySpotX) * radPerDeg;
    // Both vectors are in PES's ground plane (X along the goal axis, Z across). GF's y is -Z, so the penalty spot (sign * X, 0)
    // lies at (targetX - x, y) from the spot.
    const float targetX = spot.coords[0] < 0.0f ? -pesPenaltySpotX : pesPenaltySpotX;
    const Vector3 toTarget(targetX - spot.coords[0], spot.coords[1], 0.0f);
    const Vector3 aim(std::sin(yaw), std::cos(yaw), 0.0f);
    EXPECT_LE(AngleBetweenDeg(aim, toTarget), kAimTolerance_deg)
        << "the corner restart yaw does not point at the penalty spot from (" << spot.coords[0] << ", " << spot.coords[1]
        << ")";
  }
}

// The corner placer turns the side yaw (315 for the straight run-up, 45 for the mirrored one) by the restart bearing, and
// each is wrapped to [0, 360).
TEST(SetKickStand, TheCornerHeadingIsTheSideYawPlusTheRestartBearing) {
  constexpr float kHeadingTolerance_deg = 0.01f;  // float arithmetic, not a PES value
  const auto facing = [](float deg) {
    const float rad = deg * std::acos(-1.0f) / 180.0f;
    return Vector3(std::sin(rad), std::cos(rad), 0.0f);
  };
  for (const float restart_deg : {0.0f, 197.1f, 350.0f}) {
    EXPECT_LE(AngleBetweenDeg(facing(pes::CornerSetKickHeadingDeg(restart_deg, false)), facing(315.0f + restart_deg)),
              kHeadingTolerance_deg)
        << "the straight run-up does not turn the side yaw by the restart bearing " << restart_deg;
    EXPECT_LE(AngleBetweenDeg(facing(pes::CornerSetKickHeadingDeg(restart_deg, true)), facing(45.0f + restart_deg)),
              kHeadingTolerance_deg)
        << "the mirrored run-up does not turn the side yaw by the restart bearing " << restart_deg;
  }
}

// PES's corner taker stands on his SetKick stand point when the corner is prepared and holds it: the run-up clip's contact
// offset is the stand's length from the corner spot (as for the goal kick), and his body faces the corner heading.
TEST(SetKickStand, TheCornerTakerIsPlacedOnTheStandPointFacingTheCornerHeading) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  const float radPerDeg = std::acos(-1.0f) / 180.0f;
  // The run-up is the anime cell at row 0x509, column 9: the cell the placer reads.
  const int runUpMotion = match->GetPesAnimetable().Cell(0x509, 9).motionNo;
  ASSERT_GE(runUpMotion, 0) << "the corner run-up cell (0x509, kind 9) is empty in the install";
  const float expected = ContactOffsetLengthM(*match, runUpMotion);
  const TeamAIController* controller = match->GetTeam(0)->GetController();
  for (const Vector3 spot : {Vector3(52.0f, 33.5f, 0.0f), Vector3(-52.0f, -33.5f, 0.0f)}) {
    Player* taker = PrepareRestart(*match, e_SetPiece_Corner, 0, spot);
    ASSERT_NE(taker, nullptr) << "no corner taker was chosen";
    EXPECT_TRUE(controller->IsPieceTakerHeld(taker)) << "the corner taker is not held on his stand point";
    EXPECT_NEAR(DistanceFromSpotM(*taker, spot), expected, kPlacementTolerance_m)
        << "the corner taker is not on the SetKick stand point for the spot (" << spot.coords[0] << ", " << spot.coords[1]
        << ")";
    const pes::Vec3 spotPes(spot.coords[0], spot.coords[1], spot.coords[2]);
    const float heading = pes::CornerSetKickHeadingDeg(pes::CornerRestartYawDeg(spotPes, pesPenaltySpotX), false);
    // A body facing heading h faces (sin h, cos h) in PES's ground plane, which is (sin h, -cos h) in GF's (y is -Z).
    const float rad = heading * radPerDeg;
    const Vector3 facing(std::sin(rad), -std::cos(rad), 0.0f);
    EXPECT_LE(AngleBetweenDeg(taker->GetDirectionVec(), facing), kHeadingTolerance_deg)
        << "the corner taker does not face the corner heading (" << taker->GetDirectionVec().coords[0] << ", "
        << taker->GetDirectionVec().coords[1] << ")";
  }
}

// A placed corner taker keeps his heading while he waits, as the placed keeper does (PES's standby plays with the SetKick yaw).
TEST(SetKickStand, ThePlacedCornerTakerKeepsHisHeadingWhileHeWaits) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  // Two and a half seconds: the ladder releases the kick about 240 frames after the restart is ready (pes-re-A5Judge.md 9.3),
  // and the strike's own motion is not part of the wait.
  constexpr int kWaitTicks = 150;
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  const Vector3 spot(52.0f, 33.5f, 0.0f);
  // The match's first tick prepares its opening kick-off, which would take the corner taker's placement back: the restart is
  // prepared after that tick.
  match->Process();
  Player* taker = PrepareRestart(*match, e_SetPiece_Corner, 0, spot);
  ASSERT_NE(taker, nullptr) << "no corner taker was chosen";
  const TeamAIController* controller = match->GetTeam(0)->GetController();
  ASSERT_TRUE(controller->IsPieceTakerHeld(taker)) << "the corner taker is not held after the placement";
  const Vector3 placed = taker->GetDirectionVec();
  float worst = 0.0f;
  for (int t = 0; t < kWaitTicks; ++t) {
    match->Process();
    if (match->IsInPlay()) break;  // released: the restart is in play and the taker runs to the ball
    EXPECT_TRUE(controller->IsPieceTakerHeld(taker)) << "the corner taker lost his hold at tick " << t;
    const float turned = AngleBetweenDeg(placed, taker->GetDirectionVec());
    if (turned > worst) worst = turned;
  }
  EXPECT_LE(worst, kHeadingTolerance_deg) << "the placed corner taker turned " << worst << " degrees while he waited";
}

// Two live corner kickers (docs/re/pes-re-setpiece-choreography.md section 4.1) faced the straight run-up's heading: the side
// yaw (315) plus the restart bearing from their corner to the penalty spot. The live readings are one decimal.
TEST(SetKickStand, TheCornerHeadingMatchesTheLiveKickerYaws) {
  constexpr float kLiveTolerance_deg = 0.5f;  // a GF tolerance, not a PES value
  struct LiveCorner {
    Vector3 spot;
    float yaw_deg;
  };
  // GF's y is -Z: the live corners are PES (x, z) = (-51.781, -33.61) and (52.252, -33.214).
  for (const LiveCorner& live : {LiveCorner{Vector3(-51.781f, 33.61f, 0.0f), 331.9f},
                                 LiveCorner{Vector3(52.252f, 33.214f, 0.0f), 297.2f}}) {
    const pes::Vec3 spotPes(live.spot.coords[0], live.spot.coords[1], live.spot.coords[2]);
    const float heading = pes::CornerSetKickHeadingDeg(pes::CornerRestartYawDeg(spotPes, pesPenaltySpotX), false);
    EXPECT_LE(std::fabs(std::remainder(heading - live.yaw_deg, 360.0f)), kLiveTolerance_deg)
        << "the corner heading at (" << live.spot.coords[0] << ", " << live.spot.coords[1] << ") is " << heading
        << " degrees";
  }
}

// PES's pickup root (FUN_140791EC0 phase 1, the clip's ball-contact root point): the taker's root stands the clip's ball offset
// behind the ball, so the hands meet the ball at the contact frame. A ball 0.8 m ahead of the root puts the root 0.8 m on the far
// side. Heading 0 is +Z in PES, which is -y in GF; heading 90 is +X.
TEST(SetKickStand, ThePickupRootStandsTheClipsBallOffsetBehindTheBall) {
  constexpr float kPointTolerance_m = 1e-4f;  // float arithmetic, not a PES value
  const pes::Vec3 ball(10.0f, 4.0f, 0.0f);
  const pes::Vec3 ahead(0.0f, 0.0f, 0.8f);  // the ball's place in the clip's frame: 0.8 m forward (+Z)
  const pes::Vec3 atZero = pes::PickupContactRoot(ball, 0.0f, ahead);
  EXPECT_NEAR(atZero.x, 10.0f, kPointTolerance_m);
  EXPECT_NEAR(atZero.y, 4.8f, kPointTolerance_m) << "heading 0 faces +Z, so the root is 0.8 m on the -Z side of the ball";
  const pes::Vec3 atNinety = pes::PickupContactRoot(ball, 90.0f, ahead);
  EXPECT_NEAR(atNinety.x, 9.2f, kPointTolerance_m) << "heading 90 faces +X, so the root is 0.8 m on the -X side of the ball";
  EXPECT_NEAR(atNinety.y, 4.0f, kPointTolerance_m);
}

// The free kick and the penalty (FreeKickLoop SetKick 0x1407411F0, PenaltyKickLoop SetKick 0x140782930; docs/re/pes-re-kicker-
// standby.md section 4, docs/re/pes-re-motion-remap.md section 5). The taker stands on the spot plus the final run-up's contact
// offset, turned by the heading: the run-up's contact yaw plus the restart yaw its restart record carries.

namespace {

// The run-up the placer walks to: the cell's motion through the player walk, for a taker with no card, stat or second kicker.
int FreeKickRunUp(const Match& match) {
  const int base = match.GetPesAnimetable().Cell(0x509, 9).motionNo;
  return base < 0 ? base : pes::DeriveFreeKickRunUp(match.GetPesDerivation(), pes::SetPieceKeys{}, base);
}
int PenaltyRunUp(const Match& match) {
  const int base = match.GetPesAnimetable().Cell(0x8eb, 9).motionNo;
  return base < 0 ? base : pes::DerivePenaltyRunUp(match.GetPesDerivation(), pes::SetPieceKeys{}, base);
}

// A body facing heading h, in GF: PES's ground plane is (sin h, cos h), and GF's y is -Z.
Vector3 FacingOf(float headingDeg) {
  const float rad = headingDeg * std::acos(-1.0f) / 180.0f;
  return Vector3(std::sin(rad), -std::cos(rad), 0.0f);
}

// A bearing folded to [0, 360).
float Fold360(float deg) {
  const float d = std::fmod(deg, 360.0f);
  return d < 0.0f ? d + 360.0f : d;
}

// The contact yaw of a run-up's first rotation record, unmirrored (FUN_1406B1C70 at index 0), folded to [0, 360).
float RecordYawDeg(const Match& match, int motion) {
  return Fold360(static_cast<float>(match.GetPesMbinfo().Motion(motion).Rot(0).offset));
}

}  // namespace

// PES places the free kick taker on his stand point when the free kick is prepared, and he holds it until the release.
TEST(SetKickStand, TheFreeKickTakerIsPlacedAndHeldOnTheRunUpsContactOffset) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const int runUp = FreeKickRunUp(*match);
  ASSERT_GE(runUp, 0) << "the free kick run-up cell (0x509, kind 9) is empty in the install";
  const float expected = ContactOffsetLengthM(*match, runUp);
  // On the centre line, away from the touchlines and the corners: the field clamp leaves the stand alone.
  const Vector3 spot(20.0f, 0.0f, 0.0f);
  Player* taker = PrepareRestart(*match, e_SetPiece_FreeKick, 0, spot);
  ASSERT_NE(taker, nullptr) << "no free kick taker was chosen";
  EXPECT_TRUE(match->GetTeam(0)->GetController()->IsPieceTakerHeld(taker)) << "the free kick taker is not held";
  EXPECT_NEAR(DistanceFromSpotM(*taker, spot), expected, kPlacementTolerance_m)
      << "the free kick taker is not on the SetKick stand point";
}

// The free kick heading is the run-up's contact yaw plus the bearing to the goal the side attacks (FUN_1408B4560, MODE 5).
// From a spot on the centre line that bearing is 90 toward +X and 270 toward -X.
TEST(SetKickStand, TheFreeKickTakerFacesTheContactYawPlusTheGoalBearing) {
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const int runUp = FreeKickRunUp(*match);
  ASSERT_GE(runUp, 0) << "the free kick run-up cell (0x509, kind 9) is empty in the install";
  const Vector3 spot(20.0f, 0.0f, 0.0f);
  Player* taker = PrepareRestart(*match, e_SetPiece_FreeKick, 0, spot);
  ASSERT_NE(taker, nullptr) << "no free kick taker was chosen";
  // PES's attack sign of the taker's team is the negated GF side (T+0x254).
  const float goalBearing = -static_cast<float>(match->GetTeam(0)->GetSide()) > 0.0f ? 90.0f : 270.0f;
  const float heading = Fold360(RecordYawDeg(*match, runUp) + goalBearing);
  EXPECT_LE(AngleBetweenDeg(taker->GetDirectionVec(), FacingOf(heading)), kHeadingTolerance_deg)
      << "the free kick taker does not face the contact yaw plus the goal bearing (" << heading << " degrees)";
}

// PES places the penalty taker on his stand point from the penalty run-up's contact offset, from the mark on either side.
TEST(SetKickStand, ThePenaltyTakerIsPlacedAndHeldOnTheRunUpsContactOffset) {
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const int runUp = PenaltyRunUp(*match);
  ASSERT_GE(runUp, 0) << "the penalty run-up cell (0x8eb, kind 9) is empty in the install";
  const float expected = ContactOffsetLengthM(*match, runUp);
  for (const Vector3 spot : {Vector3(pesPenaltySpotX, 0.0f, 0.0f), Vector3(-pesPenaltySpotX, 0.0f, 0.0f)}) {
    Player* taker = PrepareRestart(*match, e_SetPiece_Penalty, 0, spot);
    ASSERT_NE(taker, nullptr) << "no penalty taker was chosen";
    EXPECT_TRUE(match->GetTeam(0)->GetController()->IsPieceTakerHeld(taker))
        << "the penalty taker is not held at the mark x = " << spot.coords[0];
    EXPECT_NEAR(DistanceFromSpotM(*taker, spot), expected, kPlacementTolerance_m)
        << "the penalty taker is not on the SetKick stand point at the mark x = " << spot.coords[0];
  }
}

// The penalty heading is the run-up's contact yaw plus 90 from the +X mark and 270 from the -X mark (FUN_1408B4560, MODE 6).
TEST(SetKickStand, ThePenaltyTakerFacesTheContactYawPlusTheMarksSideBearing) {
  constexpr float kHeadingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value
  Properties config;
  config.SetBool("pes_loop", true);
  REQUIRE_PES_MATCH(config);
  const int runUp = PenaltyRunUp(*match);
  ASSERT_GE(runUp, 0) << "the penalty run-up cell (0x8eb, kind 9) is empty in the install";
  const float yaw = RecordYawDeg(*match, runUp);
  for (const float sign : {1.0f, -1.0f}) {
    const Vector3 spot(sign * pesPenaltySpotX, 0.0f, 0.0f);
    Player* taker = PrepareRestart(*match, e_SetPiece_Penalty, 0, spot);
    ASSERT_NE(taker, nullptr) << "no penalty taker was chosen";
    const float heading = Fold360(yaw + (sign > 0.0f ? 90.0f : 270.0f));
    EXPECT_LE(AngleBetweenDeg(taker->GetDirectionVec(), FacingOf(heading)), kHeadingTolerance_deg)
        << "the penalty taker at x = " << spot.coords[0] << " does not face " << heading << " degrees";
  }
}

// The free kick's restart yaw points down the goal the side attacks: from a spot on the centre line, 90 toward +X and 270
// toward -X (FUN_1408B4560, MODE 5; the sign is the taker's team's attack sign, T+0x254).
TEST(SetKickStand, TheFreeKickRestartYawPointsDownTheAttackedGoal) {
  constexpr float kBearingTolerance_deg = 0.01f;  // float arithmetic, not a PES value
  EXPECT_NEAR(pes::FreeKickRestartYawDeg(pes::Vec3(20.0f, 0.0f, 0.0f), 1.0f), 90.0f, kBearingTolerance_deg);
  EXPECT_NEAR(pes::FreeKickRestartYawDeg(pes::Vec3(20.0f, 0.0f, 0.0f), -1.0f), 270.0f, kBearingTolerance_deg);
}

// The penalty's restart yaw is 90 from a mark on the +X side and 270 from the -X side (FUN_1408B4560, MODE 6).
TEST(SetKickStand, ThePenaltyRestartYawIsNinetyOrTwoSeventyBySpotSide) {
  constexpr float kBearingTolerance_deg = 0.01f;  // float arithmetic, not a PES value
  EXPECT_NEAR(pes::PenaltyRestartYawDeg(pes::Vec3(pesPenaltySpotX, 0.0f, 0.0f)), 90.0f, kBearingTolerance_deg);
  EXPECT_NEAR(pes::PenaltyRestartYawDeg(pes::Vec3(-pesPenaltySpotX, 0.0f, 0.0f)), 270.0f, kBearingTolerance_deg);
}

// The contact yaw is the record's rotation offset, negated when mirrored, and folded once to [0, 360) (FUN_1406B1C70, and the
// placer's fold at 0x140782A56-0x140782A76).
TEST(SetKickStand, TheContactYawIsTheRecordsOffsetMirroredAndFolded) {
  constexpr float kAngleTolerance_deg = 0.01f;  // float arithmetic, not a PES value
  EXPECT_NEAR(pes::ContactYawDeg(-30, false), 330.0f, kAngleTolerance_deg);
  EXPECT_NEAR(pes::ContactYawDeg(-30, true), 30.0f, kAngleTolerance_deg);
  EXPECT_NEAR(pes::ContactYawDeg(400, false), 40.0f, kAngleTolerance_deg);
}

// The placed heading is the contact yaw plus the restart yaw, each folded to [0, 360) (FUN_140732D40 through FUN_1404BEF90).
TEST(SetKickStand, TheSetKickHeadingIsTheContactYawPlusTheRestartYaw) {
  constexpr float kAngleTolerance_deg = 0.01f;  // float arithmetic, not a PES value
  EXPECT_NEAR(pes::SetKickHeadingDeg(350.0f, 30.0f), 20.0f, kAngleTolerance_deg);
  EXPECT_NEAR(pes::SetKickHeadingDeg(200.0f, 270.0f), 110.0f, kAngleTolerance_deg);
}

// The free kick's stand is pulled in when it lies within 0.5 m of the corner, on the axis it overshoots less, and then clamped
// to the marks plus 1.5 m (FUN_1407411F0's tail, 0x741473-0x741564; the constants 0x142565600 and 0x14259BDDC).
TEST(SetKickStand, TheFreeKickStandIsPulledInFromTheCornerAndClampedToTheMarks) {
  constexpr float kPointTolerance_m = 1e-3f;  // float arithmetic, not a PES value
  // 0.17 m from the corner: x overshoots by 0.06 m and z by 0.16 m, so z is pulled in to the touchline's inset.
  const pes::Vec3 nearZ = pes::FreeKickFieldClamp(pes::Vec3(pesFieldHalfLength - 0.06f, pesFieldHalfWidth - 0.16f, 0.0f));
  EXPECT_NEAR(std::fabs(nearZ.x), pesFieldHalfLength - 0.06f, kPointTolerance_m);
  EXPECT_NEAR(std::fabs(nearZ.y), pesFieldHalfWidth - 0.5f, kPointTolerance_m) << "z is pulled in by 0.5 m";
  // 0.11 m from the corner with x the larger overshoot: x is pulled in to the goal line's inset and z stays.
  const pes::Vec3 nearX = pes::FreeKickFieldClamp(pes::Vec3(pesFieldHalfLength - 0.1f, pesFieldHalfWidth - 0.05f, 0.0f));
  EXPECT_NEAR(std::fabs(nearX.x), pesFieldHalfLength - 0.5f, kPointTolerance_m) << "x is pulled in by 0.5 m";
  EXPECT_NEAR(std::fabs(nearX.y), pesFieldHalfWidth - 0.05f, kPointTolerance_m);
  // Beyond the marks, x is clamped to F0 + 1.5 m and z to F1 + 1.5 m, each keeping its sign.
  const pes::Vec3 far = pes::FreeKickFieldClamp(pes::Vec3(-70.0f, 60.0f, 0.0f));
  EXPECT_NEAR(far.x, -(pesFieldHalfLength + 1.5f), kPointTolerance_m);
  EXPECT_NEAR(far.y, pesFieldHalfWidth + 1.5f, kPointTolerance_m);
}
