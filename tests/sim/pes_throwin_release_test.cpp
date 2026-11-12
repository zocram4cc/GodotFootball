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
// Re-checked on the RestingBall3 merge (the same census): carried releases on 6 (first at frame 6358), 11 (10223), 10
// (10797) and 5 (30381), in that order, so the list stands.
constexpr unsigned int kThrowInSeeds[] = {6u, 11u, 10u, 5u};
// Ceremony takers (path B) need a match with a throw-in no seamless candidate can take. Census on the RestingBall3 merge
// (MergeBatchBI; seeds 1..24, two-minute pes-loop matches, one process per seed; a taker is counted when IsPieceTakerHeld
// at the throw-in's start and its release ends the throw-in): ceremony releases on seeds 18 (first at frame 7237, two
// releases), 12 (7525), 11 (11290), 23 (27409), 22 (31775) and 10 (42655); none on 1-9, 13-15, 17, 19-21 and 24. Seed 16
// aborts in PES_DISPATCH_UNTRANSLATED 0x14079E0B0, not counted. No release missed the key. Fastest first; the table is in
// docs/ports/pes-merge-BI.md section 3.
constexpr unsigned int kCeremonySeeds[] = {18u, 12u, 11u, 23u, 22u, 10u};
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

// Runs a PES-loop match for two minutes of `seed`, calling `onTick` after every tick. Played alone, as the ceremony test
// always played it; the carried-throw-in tests play the same matches through their probes below.
void RunMatch(unsigned int seed, const std::function<void(Match&)>& onTick) {
  pestest::CallbackProbe probe(::testing::UnitTest::GetInstance()->current_test_info()->name(), {true, kMinutes, seed},
                               pestest::kNoTickCap, /*stallStops=*/false, /*judgeEnd=*/false, onTick, nullptr);
  pestest::Run(probe);
  ASSERT_FALSE(probe.skipped) << "PES21_ROOT not set";
  ASSERT_TRUE(probe.setupError.empty()) << probe.setupError;
  pestest::Settle(probe);
}

// The physique PES scales a clip's ball offset by (FUN_1406E8CC0): P+0x3B18, written by FUN_1406EB210 as height_cm / 180 from the
// setup record, x and z clamped to 0.9166..1.0833. Read from the window's P for the taker's slot, as the sim's own key reader does.
float TakerPhysique(Match& match, const Player* taker) {
  if (taker == nullptr) return 1.0f;
  const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(taker));
  float scale = 0.0f;
  std::memcpy(&scale, p + pes::kPF3B18Offset, sizeof scale);
  return scale > 0.0f ? scale : 1.0f;
}

// AThrowInIsReleasedFromInsideTheLine: one searched seed's two-minute match, counting its carried releases over the whole match.
class BehindLineProbe : public pestest::Probe {
 public:
  explicit BehindLineProbe(unsigned int seed)
      : Probe("AThrowInIsReleasedFromBehindTheLine", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int releases = 0, notBehind = 0;
  // PES's pre-release rule's margin, DAT_142588200 (0.1 m), read from the image the way the sim reads its constants.
  float handOutside_m = 0.0f;
  void OnStart(Match& match) override {
    const pes::PesTrapExternals& ext = match.PesTrapExternalsOf();
    std::memcpy(&handOutside_m, ext.image + (kHandOutsideVa - ext.imageBase), sizeof handOutside_m);
  }
  static constexpr uint64_t kHandOutsideVa = 0x142588200;  // DAT_142588200: 0.1 m (FUN_141EB51A0's push bound)
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
      if (std::fabs(match.GetBall()->Predict(0).coords[1]) - pesFieldHalfWidth < handOutside_m) notBehind++;
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
    const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hit.ballX, hit.ballY, hit.ballZ), TakerPhysique(match, b.taker), false, g);
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

// One probe per entry of kThrowInSeeds, in the order the tests search them. The list's repeated seed is a second probe on
// the same match: the shared run feeds it the same ticks, as the old second run of that seed saw them.
BehindLineProbe behindLine[] = {BehindLineProbe(kThrowInSeeds[0]), BehindLineProbe(kThrowInSeeds[1]),
                                 BehindLineProbe(kThrowInSeeds[2]), BehindLineProbe(kThrowInSeeds[3])};
CarriedHoldProbe carriedHold[] = {CarriedHoldProbe(kThrowInSeeds[0]), CarriedHoldProbe(kThrowInSeeds[1]),
                                  CarriedHoldProbe(kThrowInSeeds[2]), CarriedHoldProbe(kThrowInSeeds[3])};
LiftProbe lift[] = {LiftProbe(kThrowInSeeds[0]), LiftProbe(kThrowInSeeds[1]), LiftProbe(kThrowInSeeds[2]),
                    LiftProbe(kThrowInSeeds[3])};

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

// PES releases a carried throw-in from BEHIND the line (pes-re-throwin-choreography.md 4.2): before the release the taker is pushed
// so the hand further outside stays at least F[1] + 0.1 m (DAT_142588200) from the centre line (FUN_141EB51A0, bones 3/6): "the ball
// is always released from behind the line". Seen live: live2/pid1482329/seq002, seq006, seq009 (hook throwin_1407951F0, one throw-in,
// action 0x1B phase 1): the taker stands at z 35.05..35.08 m (0.99..1.02 m outside the 34.06 m touchline) and the ball in his hand is at
// z 34.88 m, 0.82 m outside, 0.74 m up. So when the throw's touch ends the set piece, the ball is outside the line by at least the 0.1 m.
// This test REPLACES the earlier AThrowInIsReleasedFromInsideTheLine, whose premise (the ball inside the line at the touch) was GF's
// own and is the opposite of PES's rule; the capture above overturned it. A ceremony taker (path B) is not counted here
// (ACeremonyThrowInIsReleasedFromTheHandOverTheSpot covers him).
TEST(PesThrowInRelease, AThrowInIsReleasedFromBehindTheLine) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  int releases = 0, notBehind = 0;
  for (BehindLineProbe& p : behindLine) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    releases += p.releases;
    notBehind += p.notBehind;
    if (releases > 0) break;
  }
  EXPECT_GT(releases, 0) << "no carried throw-in was released in any searched two-minute match";
  EXPECT_EQ(notBehind, 0) << notBehind << " of " << releases
                          << " released throw-ins were not behind the touchline by PES's 0.1 m (DAT_142588200) at the touch";
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
      const pes::Vec3 key = pes::ClipOffsetMetres(pes::Vec3(hits[1].ballX, hits[1].ballY, hits[1].ballZ), TakerPhysique(match, b.taker), false, g);
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
