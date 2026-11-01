#include <gtest/gtest.h>

#include "pes_match_constants.hpp"

#include <algorithm>
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
#include "onthepitch/pessetpiecetaker.hpp"
#include "onthepitch/player/player.hpp"
#include "onthepitch/team.hpp"
#include "onthepitch/teamAIcontroller.hpp"
#include "pes/vfs.hpp"
#include "rig/rig.hpp"
#include "simcontext.hpp"

#ifndef GF_SOURCE_DIR
#define GF_SOURCE_DIR "."
#endif

namespace {

constexpr int HOME_TEAM = 1, AWAY_TEAM = 2;
constexpr unsigned int kSeed = 3;
constexpr int kMinutes = 2;
// Seed 7's two minutes release no throw-in (its first is past tick 15000: docs/ports/pes-port-StoppageFlow5.md §3), so the
// tests search matches in turn until one has a throw-in. Seed 3's first walker is at tick 2461 on the path-A census.
constexpr unsigned int kThrowInSeeds[] = {kSeed, 3u, 13u, 11u};
// Ceremony takers (path B) need a match with a throw-in no seamless candidate can take: seeds 2, 15 and 16 each have one in
// their first two minutes on the translated judge (a 1..40 seed census, docs/ports/pes-port-CardLevel7.md); 3, 13 and 11 have none.
constexpr unsigned int kCeremonySeeds[] = {2u, 15u, 16u};
// The release key's horizontal length is checked to this tolerance (m): the key is measured against the root the hold used.
constexpr float kKeyTolerance_m = 0.05f;
// The ball's radius (m): Ball::ResetSituation seats its centre this far above the point it is given (ball.cpp).
constexpr float kBallRadius_m = 0.11f;

// The install's set-piece taker constants, read as the match reads them. The caller has checked PES21_ROOT.
void LoadTakerConstants(pes::SetPieceTakerConstants* out) {
  const char* root = std::getenv("PES21_ROOT");
  ASSERT_TRUE(root != nullptr) << "PES21_ROOT not set";
  pes::PesVfs vfs;
  std::string err;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;
  auto rig = pesrig::BuildRig(vfs, &err);
  ASSERT_TRUE(rig) << err;
  auto setup = std::make_unique<MatchSetup>();
  setup->rig = &*rig;
  ASSERT_TRUE(pestest::FillMissingPesConstants(setup.get())) << "install constants unavailable";
  *out = *setup->pesSetPieceTakerConstants;
}

// Runs a PES-loop match for two minutes of `seed`, calling `onTick` after every tick.
void RunMatch(unsigned int seed, const std::function<void(Match&)>& onTick) {
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
    if (onTick) onTick(match);
  }
}

}  // namespace

// The match reads the install's player database (common/etc/pesdb/Player.bin), as gfsim_run does. Without it the match
// runs on stand-in form, stronger-foot and injury values, so this harness would not play the match gfsim plays.
TEST(PesThrowInRelease, TheMatchIsBuiltWithTheInstallsPlayerDatabase) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  MatchSetup setup;
  pestest::FillMissingPesConstants(&setup);
  EXPECT_NE(setup.pesPlayerDatabase, nullptr) << "the install's Player.bin is not loaded into the match";
}

// PES's carry release (FUN_140792E10, carry branch, docs/re/pes-re-throwin-choreography.md 4.2): the short run row while the
// taker is within the install's release distance of the throw's target, the mid row beyond it. The threshold is read from the
// install (0x1425c5948), so the test asserts the rule against it, on either side.
TEST(PesThrowInRelease, ACarriedThrowUsesTheShortRowUpToThePesDistanceAndTheMidRowBeyond) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  pes::SetPieceTakerConstants k;
  LoadTakerConstants(&k);
  const float reach_m = k.throwInReleaseShort_m;
  ASSERT_GT(reach_m, 0.0f);
  EXPECT_EQ(pes::ThrowInCarryReleaseRow(reach_m - 0.01f, reach_m), pes::kThrowInReleaseRunShort);
  EXPECT_EQ(pes::ThrowInCarryReleaseRow(reach_m + 0.01f, reach_m), pes::kThrowInReleaseRunMid);
}

// PES releases a carried throw-in from behind the line: the release holds the ball at the hand's key offset, inside the
// touchline, until the release (pes-re-throwin-choreography.md 4.2, 5.3). So when the throw's touch ends the set piece, the
// ball lies inside the line. Counted over the first searched two-minute match with a carried throw-in. A ceremony taker
// (path B) is not counted: PES places him so the release key's ball is over the restart spot, 0.25 m outside the line
// (3.3, 4.1), so his ball is outside by design (ACeremonyThrowInIsReleasedFromTheHandOverTheSpot covers him). Counting him
// failed seed 7 once the kick scatter took PES's situation factor Q and its two minutes reached a ceremony throw-in.
TEST(PesThrowInRelease, AThrowInIsReleasedFromInsideTheLine) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  int releases = 0, outside = 0;
  for (const unsigned int seed : kThrowInSeeds) {
    if (releases > 0) break;
    bool throwInTaking = false, carrying = false;
    RunMatch(seed, [&](Match& match) {
      const RefereeBuffer& b = match.GetRefereeBuffer();
      if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) {
        if (!throwInTaking) carrying = match.GetTeam(b.teamID)->GetController()->IsCarryingBall();
        throwInTaking = true;
      }
      if (throwInTaking && !match.IsInSetPiece()) {
        throwInTaking = false;
        if (!carrying) return;
        releases++;
        if (std::fabs(match.GetBall()->Predict(0).coords[1]) > pesFieldHalfWidth) outside++;
      }
    });
  }
  EXPECT_GT(releases, 0) << "no carried throw-in was released in any searched two-minute match";
  EXPECT_EQ(outside, 0) << outside << " of " << releases << " released throw-ins lay outside the touchline at the touch";
}

// A ceremony taker's ball is in his hand when he releases (pes-re-throwin-choreography.md 3.1, 3.3: PES places him with the
// ball so the release key's ball is over the spot; 4.2: the throw is released from the hand, 1.8 m up, never from the
// ground). The hold of a tick places the ball from the root the previous tick ended with, so the check pairs the last held
// ball with that root: its height is the key's height and its horizontal distance from the root the key's (clip 0xcfa's
// second record, the one SetKick reads for the stand point).
TEST(PesThrowInRelease, ACeremonyThrowInIsReleasedFromTheHandOverTheSpot) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  constexpr int kThrowInKeyMotion = 0xcfa;
  int ceremony = 0, mismatched = 0;
  float worstHeight_m = 0.0f, worstReach_m = 0.0f;
  for (const unsigned int seed : kCeremonySeeds) {
    if (ceremony > 0) break;
    bool throwInTaking = false, placed = false;
    Vector3 heldBall, heldRoot, rootEndingLastTick;
    RunMatch(seed, [&](Match& match) {
      const RefereeBuffer& b = match.GetRefereeBuffer();
      if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && !throwInTaking) {
        throwInTaking = true;
        placed = match.GetTeam(b.teamID)->GetController()->IsPieceTakerHeld(b.taker);
        rootEndingLastTick = b.taker->GetPosition();
      }
      if (throwInTaking && match.IsInSetPiece()) {
        heldBall = match.GetBall()->Predict(0);
        heldRoot = rootEndingLastTick;
        rootEndingLastTick = b.taker->GetPosition();
        return;
      }
      if (!throwInTaking) return;
      throwInTaking = false;
      if (!placed) return;
      ceremony++;
      const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(kThrowInKeyMotion);
      ASSERT_GT(hits.size(), 1u);
      const pes::GoalKickConstants g = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
      const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hits[1].ballX, hits[1].ballY, hits[1].ballZ), 1.0f, false, g);
      const float dx = heldBall.coords[0] - heldRoot.coords[0], dy = heldBall.coords[1] - heldRoot.coords[1];
      // Ball::ResetSituation seats the ball's centre one radius above the point it is given, so the held centre is the key's
      // height plus that radius; a ball left on the ground is 1.7 m from it either way.
      const float heightGap_m = std::fabs(heldBall.coords[2] - key.y - kBallRadius_m);
      const float reachGap_m = std::fabs(std::sqrt(dx * dx + dy * dy) - std::hypot(key.x, key.z));
      worstHeight_m = std::max(worstHeight_m, heightGap_m);
      worstReach_m = std::max(worstReach_m, reachGap_m);
      if (heightGap_m > kKeyTolerance_m || reachGap_m > kKeyTolerance_m) mismatched++;
    });
  }
  EXPECT_GT(ceremony, 0) << "no ceremony throw-in was released in any searched two-minute match";
  EXPECT_EQ(mismatched, 0) << mismatched << " of " << ceremony << " ceremony releases were not at the key (worst height gap "
                           << worstHeight_m << " m, reach gap " << worstReach_m << " m)";
}

// A carried throw-in's ball is held at its release row's key offset from the taker's root until the touch. The hold of a
// tick places the ball from the root the previous tick ended with, so the check pairs the last held ball with that root.
// The key's horizontal length is independent of the heading (the Fox X and Z of the key), so the check is the hold's
// distance from the root. It checks the hold against the row's first hit record, which the hold reads; it does not check
// that this record is PES's release key (OPEN, pes-re-throwin-choreography.md 5). Ceremony takers hold the ball at their
// own stand-point key and are not counted.
TEST(PesThrowInRelease, ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  pes::SetPieceTakerConstants k;
  LoadTakerConstants(&k);
  int carried = 0, mismatched = 0;
  float worst_m = 0.0f;
  for (const unsigned int seed : kThrowInSeeds) {
    if (carried > 0) break;
    bool throwInTaking = false, carrying = false;
    int takeRow = 0;
    Vector3 heldBall, heldRoot, rootEndingLastTick;
    RunMatch(seed, [&](Match& match) {
      const RefereeBuffer& b = match.GetRefereeBuffer();
      if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && !throwInTaking) {
        throwInTaking = true;
        carrying = match.GetTeam(b.teamID)->GetController()->IsCarryingBall();
        const Player* mate = match.GetTeam(b.teamID)->GetController()->ThrowInTargetMate(b.taker);
        const Vector3 to = mate ? mate->GetPosition() : b.taker->GetPosition();
        const float d = (b.taker->GetPosition().Get2D() - to.Get2D()).GetLength();
        takeRow = pes::ThrowInCarryReleaseRow(d, k.throwInReleaseShort_m);
        rootEndingLastTick = b.taker->GetPosition();
      }
      if (throwInTaking && match.IsInSetPiece()) {
        heldBall = match.GetBall()->Predict(0);
        heldRoot = rootEndingLastTick;
        rootEndingLastTick = b.taker->GetPosition();
        return;
      }
      if (!throwInTaking) return;
      throwInTaking = false;
      if (!carrying) return;
      carried++;
      const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(takeRow);
      const pes::GoalKickConstants g = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
      const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
      const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), 1.0f, false, g);
      const float keyHorizontal_m = std::sqrt(key.x * key.x + key.z * key.z);
      const float dx = heldBall.coords[0] - heldRoot.coords[0], dy = heldBall.coords[1] - heldRoot.coords[1];
      const float gap_m = std::fabs(std::sqrt(dx * dx + dy * dy) - keyHorizontal_m);
      worst_m = std::max(worst_m, gap_m);
      if (gap_m > kKeyTolerance_m) mismatched++;
    });
  }
  EXPECT_GT(carried, 0) << "no carried throw-in was released in any searched match";
  EXPECT_EQ(mismatched, 0) << mismatched << " of " << carried << " carried releases were not at the key's horizontal offset (worst "
                           << worst_m << " m)";
}

// PES's lift turns a carried throw-in's taker to face the pitch: SeamlessThrowin phase 4 (FUN_140791EC0 case 4,
// docs/re/pes-re-throwin-choreography.md 3.2) plays throwin_seamless_liftupball and yaw-warps the root to 180 degrees, or to
// 0 when the spot's PES z is negative, over the clip. In GF's frame (PES z = -y) that is -y from the +y touchline and +y from
// the -y one. Built on main, the lift warped only the position, so a carrier who walked in facing the line ended the lift
// with his root facing out, and the release hold put the ball outside it. At the end of every carried throw-in's lift in a
// two-minute match of a searched seed, the root must face the pitch.
// The lift's warp ends on PES's heading. On seed 7's first carried throw-in, the retain clip the carrier starts on the lift's last
// tick (SelectRetainAnim) took its start angle from facing the pitch centre, and the root landed 54 degrees off. From the lift on,
// the taker's clips start from the root yaw (HumanoidBase::RootStartAngle, Humanoid::HoldsRootYaw); on the merged tree the MOVESTEP
// clip that follows the lift started from the travel direction and faced him out.
TEST(PesThrowInRelease, TheLiftTurnsACarriedThrowInTakerToFaceThePitch) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  constexpr float kFacingTolerance_deg = 5.0f;  // a GF tolerance, not a PES value: the warp ends on the heading exactly
  const float cosTolerance = std::cos(kFacingTolerance_deg * std::acos(-1.0f) / 180.0f);
  int lifts = 0, facingOut = 0;
  float worstDot = 1.0f;
  for (const unsigned int seed : kThrowInSeeds) {
    if (lifts > 0) break;
    bool wasArrived[2] = {true, true};
    RunMatch(seed, [&](Match& match) {
      for (int t = 0; t < 2; ++t) {
        TeamAIController* c = match.GetTeam(t)->GetController();
        Player* taker = c->GetPieceTaker();
        const bool walking = taker && c->IsPieceTakerWalking(taker);
        const bool arrived = !walking || c->PieceTakerArrived();
        if (walking && arrived && !wasArrived[t]) {  // the tick the lift ends
          ++lifts;
          const Vector3 pitchward(0.0f, c->GetPieceWalkTarget().coords[1] > 0.0f ? -1.0f : 1.0f, 0.0f);
          const float dot = taker->GetRootDirectionVec().GetDotProduct(pitchward);
          worstDot = std::min(worstDot, dot);
          if (dot < cosTolerance) ++facingOut;
        }
        wasArrived[t] = arrived;
      }
    });
  }
  ASSERT_GT(lifts, 0) << "no carried throw-in was lifted in any searched match";
  EXPECT_EQ(facingOut, 0) << facingOut << " of " << lifts << " lifted takers did not face the pitch (worst cos " << worstDot << ")";
}
