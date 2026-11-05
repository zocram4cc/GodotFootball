#include <gtest/gtest.h>

#include "pes_match_constants.hpp"
#include "pes_shared_match.hpp"

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

constexpr int kMinutes = 2;
// The two lists below are results of a census on the MotionClips tree (TurnStop's stop test, idle exit and class word read PES's
// clip records; batch BB merged): seeds 1..12, two-minute pes-loop matches, each throw-in whose release ends counted as a
// ceremony taker (IsPieceTakerHeld, path B) or a carried one (IsCarryingBall, path A) (docs/ports/pes-port-MotionClips2.md
// section 8). Every cutover that moves the matches needs a new census.
// Carried throw-ins (path A): seeds 5, 6, 7, 10 and 11 release one (first at ticks 22628, 4573, 25018, 14048 and 10588), fastest
// first. The tests search matches in turn until one has a throw-in.
constexpr unsigned int kThrowInSeeds[] = {6u, 11u, 10u, 5u};
// Ceremony takers (path B) need a match with a throw-in no seamless candidate can take: only seeds 3 and 8 place one (first at
// ticks 32745 and 26044), fastest first.
constexpr unsigned int kCeremonySeeds[] = {8u, 3u};
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


// AThrowInIsReleasedFromInsideTheLine: one searched seed's two-minute match, counting its carried releases over the whole match.
class InsideLineProbe : public pestest::Probe {
 public:
  explicit InsideLineProbe(unsigned int seed)
      : Probe("AThrowInIsReleasedFromInsideTheLine", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int releases = 0, outside = 0;
  void OnTick(Match& match) override {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) {
      if (!throwInTaking_) carrying_ = match.GetTeam(b.teamID)->GetController()->IsCarryingBall();
      throwInTaking_ = true;
    }
    if (throwInTaking_ && !match.IsInSetPiece()) {
      throwInTaking_ = false;
      if (!carrying_) return;
      releases++;
      if (std::fabs(match.GetBall()->Predict(0).coords[1]) > pesFieldHalfWidth) outside++;
    }
  }

 private:
  bool throwInTaking_ = false, carrying_ = false;
};

// ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch: one searched seed's two-minute match, the holds of its carried releases.
class CarriedHoldProbe : public pestest::Probe {
 public:
  explicit CarriedHoldProbe(unsigned int seed)
      : Probe("ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int carried = 0, mismatched = 0;
  float worst_m = 0.0f;
  // The install's short release reach, read from the match the probe plays (the same install values LoadTakerConstants reads).
  void OnStart(Match& match) override { throwInReleaseShort_m_ = match.GetPesSetPieceTakerConstants().throwInReleaseShort_m; }
  void OnTick(Match& match) override {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && !throwInTaking_) {
      throwInTaking_ = true;
      carrying_ = match.GetTeam(b.teamID)->GetController()->IsCarryingBall();
      const Player* mate = match.GetTeam(b.teamID)->GetController()->ThrowInTargetMate(b.taker);
      const Vector3 to = mate ? mate->GetPosition() : b.taker->GetPosition();
      const float d = (b.taker->GetPosition().Get2D() - to.Get2D()).GetLength();
      takeRow_ = pes::ThrowInCarryReleaseRow(d, throwInReleaseShort_m_);
      rootEndingLastTick_ = b.taker->GetPosition();
    }
    if (throwInTaking_ && match.IsInSetPiece()) {
      heldBall_ = match.GetBall()->Predict(0);
      heldRoot_ = rootEndingLastTick_;
      rootEndingLastTick_ = b.taker->GetPosition();
      return;
    }
    if (!throwInTaking_) return;
    throwInTaking_ = false;
    if (!carrying_) return;
    carried++;
    const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(takeRow_);
    const pes::GoalKickConstants g = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
    const pes::HitRecord hit = hits.empty() ? pes::HitRecord() : hits.front();
    const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), 1.0f, false, g);
    const float keyHorizontal_m = std::sqrt(key.x * key.x + key.z * key.z);
    const float dx = heldBall_.coords[0] - heldRoot_.coords[0], dy = heldBall_.coords[1] - heldRoot_.coords[1];
    const float gap_m = std::fabs(std::sqrt(dx * dx + dy * dy) - keyHorizontal_m);
    worst_m = std::max(worst_m, gap_m);
    if (gap_m > kKeyTolerance_m) mismatched++;
  }

 private:
  bool throwInTaking_ = false, carrying_ = false;
  int takeRow_ = 0;
  float throwInReleaseShort_m_ = 0.0f;
  Vector3 heldBall_, heldRoot_, rootEndingLastTick_;
};

// TheLiftTurnsACarriedThrowInTakerToFaceThePitch: one searched seed's two-minute match, the facing of each lifted taker.
class LiftProbe : public pestest::Probe {
 public:
  explicit LiftProbe(unsigned int seed)
      : Probe("TheLiftTurnsACarriedThrowInTakerToFaceThePitch", {true, kMinutes, seed}, pestest::kNoTickCap, false, false),
        cosTolerance_(std::cos(kFacingTolerance_deg * std::acos(-1.0f) / 180.0f)) {}
  int lifts = 0, facingOut = 0;
  float worstDot = 1.0f;
  void OnTick(Match& match) override {
    for (int t = 0; t < 2; ++t) {
      TeamAIController* c = match.GetTeam(t)->GetController();
      Player* taker = c->GetPieceTaker();
      const bool walking = taker && c->IsPieceTakerWalking(taker);
      const bool arrived = !walking || c->PieceTakerArrived();
      if (walking && arrived && !wasArrived_[t]) {  // the tick the lift ends
        ++lifts;
        const Vector3 pitchward(0.0f, c->GetPieceWalkTarget().coords[1] > 0.0f ? -1.0f : 1.0f, 0.0f);
        const float dot = taker->GetRootDirectionVec().GetDotProduct(pitchward);
        worstDot = std::min(worstDot, dot);
        if (dot < cosTolerance_) ++facingOut;
      }
      wasArrived_[t] = arrived;
    }
  }

 private:
  // A GF tolerance, not a PES value: the warp ends on the heading exactly.
  static constexpr float kFacingTolerance_deg = 5.0f;
  const float cosTolerance_;
  bool wasArrived_[2] = {true, true};
};

// ACeremonyThrowInIsReleasedFromTheHandOverTheSpot's probe: one seed's two-minute match, the hand's key at each ceremony release.
class CeremonyHandProbe : public pestest::Probe {
 public:
  explicit CeremonyHandProbe(unsigned int seed)
      : Probe("ACeremonyThrowInIsReleasedFromTheHandOverTheSpot", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int ceremony = 0, mismatched = 0;
  float worstHeight_m = 0.0f, worstReach_m = 0.0f;
  void OnTick(Match& match) override {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn && !throwInTaking_) {
      throwInTaking_ = true;
      placed_ = match.GetTeam(b.teamID)->GetController()->IsPieceTakerHeld(b.taker);
      rootEndingLastTick_ = b.taker->GetPosition();
    }
    if (throwInTaking_ && match.IsInSetPiece()) {
      heldBall_ = match.GetBall()->Predict(0);
      heldRoot_ = rootEndingLastTick_;
      rootEndingLastTick_ = b.taker->GetPosition();
      return;
    }
    if (!throwInTaking_) return;
    throwInTaking_ = false;
    if (!placed_) return;
    ceremony++;
    const std::vector<pes::HitRecord>& hits = match.GetPesMbinfo().Hits(kThrowInKeyMotion);
    ASSERT_GT(hits.size(), 1u);
    const pes::GoalKickConstants g = pes::GoalKickConstantsFrom(match.GetPesSetPieceTakerConstants());
    const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hits[1].ballX, hits[1].ballY, hits[1].ballZ), 1.0f, false, g);
    const float dx = heldBall_.coords[0] - heldRoot_.coords[0], dy = heldBall_.coords[1] - heldRoot_.coords[1];
    // Ball::ResetSituation seats the ball's centre one radius above the point it is given, so the held centre is the key's
    // height plus that radius; a ball left on the ground is 1.7 m from it either way.
    const float heightGap_m = std::fabs(heldBall_.coords[2] - key.y - kBallRadius_m);
    const float reachGap_m = std::fabs(std::sqrt(dx * dx + dy * dy) - std::hypot(key.x, key.z));
    worstHeight_m = std::max(worstHeight_m, heightGap_m);
    worstReach_m = std::max(worstReach_m, reachGap_m);
    if (heightGap_m > kKeyTolerance_m || reachGap_m > kKeyTolerance_m) mismatched++;
  }

 private:
  static constexpr int kThrowInKeyMotion = 0xcfa;  // the throw-in's SetKick and release clip (pes-re-throwin-choreography.md 4.1)
  bool throwInTaking_ = false, placed_ = false;
  Vector3 heldBall_, heldRoot_, rootEndingLastTick_;
};

// One probe per entry of kThrowInSeeds, in the order the tests search them. The list's repeated seed is a second probe on
// the same match: the shared run feeds it the same ticks, as the old second run of that seed saw them.
InsideLineProbe insideLine[] = {InsideLineProbe(kThrowInSeeds[0]), InsideLineProbe(kThrowInSeeds[1]),
                                InsideLineProbe(kThrowInSeeds[2]), InsideLineProbe(kThrowInSeeds[3])};
CarriedHoldProbe carriedHold[] = {CarriedHoldProbe(kThrowInSeeds[0]), CarriedHoldProbe(kThrowInSeeds[1]),
                                  CarriedHoldProbe(kThrowInSeeds[2]), CarriedHoldProbe(kThrowInSeeds[3])};
LiftProbe lift[] = {LiftProbe(kThrowInSeeds[0]), LiftProbe(kThrowInSeeds[1]), LiftProbe(kThrowInSeeds[2]),
                    LiftProbe(kThrowInSeeds[3])};
CeremonyHandProbe ceremonyHand[] = {CeremonyHandProbe(kCeremonySeeds[0]), CeremonyHandProbe(kCeremonySeeds[1])};

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
  for (InsideLineProbe& p : insideLine) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    releases += p.releases;
    outside += p.outside;
    if (releases > 0) break;
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
  int ceremony = 0, mismatched = 0;
  float worstHeight_m = 0.0f, worstReach_m = 0.0f;
  for (CeremonyHandProbe& p : ceremonyHand) {
    if (ceremony > 0) break;
    pestest::Run(p);
    ASSERT_FALSE(p.skipped) << "PES21_ROOT not set";
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    ceremony += p.ceremony;
    mismatched += p.mismatched;
    worstHeight_m = std::max(worstHeight_m, p.worstHeight_m);
    worstReach_m = std::max(worstReach_m, p.worstReach_m);
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
  int carried = 0, mismatched = 0;
  float worst_m = 0.0f;
  for (CarriedHoldProbe& p : carriedHold) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    carried += p.carried;
    mismatched += p.mismatched;
    worst_m = std::max(worst_m, p.worst_m);
    if (carried > 0) break;
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
  int lifts = 0, facingOut = 0;
  float worstDot = 1.0f;
  for (LiftProbe& p : lift) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    lifts += p.lifts;
    facingOut += p.facingOut;
    worstDot = std::min(worstDot, p.worstDot);
    if (lifts > 0) break;
  }
  ASSERT_GT(lifts, 0) << "no carried throw-in was lifted in any searched match";
  EXPECT_EQ(facingOut, 0) << facingOut << " of " << lifts << " lifted takers did not face the pitch (worst cos " << worstDot << ")";
}
