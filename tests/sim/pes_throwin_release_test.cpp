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
#include "onthepitch/pesworldrecord.hpp"
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
// Census on the OneWorld r18 merge (19-11-26; seeds 1..30, two-minute pes-loop matches, one process per seed, the census probe of
// docs/ports/pes-merge-BI.md section 3; `first` is Match::GetPesFrame() at the throw-in's start). The in-play changes of r18 (PES's
// holder for hasIt, the unquantised presser, the held throw-in taker) and the merged boot moved every earlier list's matches.
// Carried throw-ins (path A) release first on seeds 2 (frame 3827), 29 (4966), 28 (5901), 4 (6044), 30 (6923) and 1 (7592); the four
// fastest are searched in that order. The tests search matches in turn until one has a throw-in.
constexpr unsigned int kThrowInSeeds[] = {2u, 29u, 28u, 4u};
// Ceremony takers (path B) need a match with a throw-in no seamless candidate can take (a taker counted when IsPieceTakerHeld at the
// throw-in's start and its release ends the throw-in): releases on seeds 14 (2062), 25 (7275), 3 (9225), 26 (9320), 20 (11874), 24
// (12167, two), 9 (24574), 8 (31163) and 28 (44657); none on the other seeds. Seed 11 did not finish within the census's 40 minutes.
constexpr unsigned int kCeremonySeeds[] = {14u, 25u, 3u, 26u, 20u, 24u};
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

// ACarriedThrowInIsReleased / TheThrowInTakersAnklesStayBehindTheLine: one searched seed's two-minute match. Counts the carried
// releases, and on every tick a throw-in is being taken measures PES's own rule (FUN_140792E10, before the release frame): of the
// taker's two ankles (PES joints 3 and 6 = rig left_ankle / right_ankle, docs/ports/pes-port-KickJoints.md 1; read from the window's
// joint block P+0x538 + 0xC * joint, as FUN_141EB51A0 reads them) the one nearer the centre line is at least F[1] + DAT_142588200
// (0.1 m) from it.
class BehindLineProbe : public pestest::Probe {
 public:
  explicit BehindLineProbe(unsigned int seed)
      : Probe("TheThrowInTakersAnklesStayBehindTheLine", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int releases = 0, takingTicks = 0, persistentShort = 0;
  float worstShortfall_m = 0.0f;
  float ankleMargin_m = 0.0f;  // DAT_142588200, read from the image
  static constexpr uint64_t kAnkleMarginVa = 0x142588200;
  static constexpr std::size_t kJointPositionsOffset = 0x538, kJointPositionBytes = 0xC, kJointZOffset = 8;
  static constexpr int kLeftAnkleJoint = 3, kRightAnkleJoint = 6;
  void OnStart(Match& match) override {
    const pes::PesTrapExternals& ext = match.PesTrapExternalsOf();
    std::memcpy(&ankleMargin_m, ext.image + (kAnkleMarginVa - ext.imageBase), sizeof ankleMargin_m);
  }
  // Each taking tick reads the taker's nearer ankle from the joint block the tick wrote (PES's rule reads that block). A short reading
  // is corrected by the push of the same tick, so the next tick's reading is the test: a short reading that persists into the next
  // taking tick of the same throw is a push that did not happen.
  void OnTick(Match& match) override {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) {
      if (!throwInTaking_) {
        carrying_ = match.GetTeam(b.teamID)->GetController()->IsCarryingBall();
        prevShort_ = false;
      }
      throwInTaking_ = true;
      TeamAIController* c = match.GetTeam(b.teamID)->GetController();
      if (carrying_ && b.taker != nullptr && c->IsCarryingBall() && c->PieceTakerArrived()) {
        const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(b.taker));
        if (p[pes::kPAnimeReadyOffset] != 0) {
          float z[2];
          std::memcpy(&z[0], p + kJointPositionsOffset + kJointPositionBytes * kLeftAnkleJoint + kJointZOffset, sizeof(float));
          std::memcpy(&z[1], p + kJointPositionsOffset + kJointPositionBytes * kRightAnkleJoint + kJointZOffset, sizeof(float));
          const float nearer = std::min(std::fabs(z[0]), std::fabs(z[1]));
          const float shortfall = pesFieldHalfWidth + ankleMargin_m - nearer;
          ++takingTicks;
          const bool short_ = shortfall > kAnkleTolerance_m;
          if (short_) worstShortfall_m = std::max(worstShortfall_m, shortfall);
          if (short_ && prevShort_) ++persistentShort;
          prevShort_ = short_;
        }
      }
    }
    if (throwInTaking_ && !match.IsInSetPiece()) {
      throwInTaking_ = false;
      if (carrying_) releases++;
    }
  }

 private:
  static constexpr float kAnkleTolerance_m = 1e-3f;  // float noise of the push's own arithmetic, not a PES value
  bool throwInTaking_ = false, carrying_ = false, prevShort_ = false;
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

// ThePlayStateIsSetPlayReadyUntilTheReleaseTouch: one searched seed's two-minute match. PES holds MI+0x116C at 3 (set-play ready)
// through a throw-in's take and writes 1 (in play) at the taker's release touch, never at the take's start
// (docs/re/pes-re-taker-release.md 1.6). Every tick of a take (restart awarded and started, not yet touched) must read 3 in the
// state PES's code reads (Match::PesPlayState), and the first tick after the touch must read 1.
class ReleaseTouchProbe : public pestest::Probe {
 public:
  explicit ReleaseTouchProbe(unsigned int seed)
      : Probe("ThePlayStateIsSetPlayReadyUntilTheReleaseTouch", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int takes = 0, takeTicksNotReady = 0, touchesNotLive = 0;
  void OnTick(Match& match) override {
    const RefereeBuffer& b = match.GetRefereeBuffer();
    if (match.IsInSetPiece() && b.started && b.desiredSetPiece == e_SetPiece_ThrowIn) {
      if (!taking_) {
        taking_ = true;
        takes++;
      }
      if (match.PesPlayState() != pes::kPlayStateSetPlayReady) takeTicksNotReady++;
      return;
    }
    if (!taking_) return;
    taking_ = false;
    if (match.PesPlayState() != pes::kPlayStateInPlay) touchesNotLive++;
  }

 private:
  bool taking_ = false;
};

ReleaseTouchProbe releaseTouch[] = {ReleaseTouchProbe(kThrowInSeeds[0]), ReleaseTouchProbe(kThrowInSeeds[1]),
                                    ReleaseTouchProbe(kThrowInSeeds[2]), ReleaseTouchProbe(kThrowInSeeds[3])};

TEST(PesThrowInRelease, ThePlayStateIsSetPlayReadyUntilTheReleaseTouch) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  int takes = 0, takeTicksNotReady = 0, touchesNotLive = 0;
  for (ReleaseTouchProbe& p : releaseTouch) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    takes += p.takes;
    takeTicksNotReady += p.takeTicksNotReady;
    touchesNotLive += p.touchesNotLive;
    if (takes > 0) break;
  }
  ASSERT_GT(takes, 0) << "no throw-in take ran in any searched match";
  EXPECT_EQ(takeTicksNotReady, 0) << takeTicksNotReady << " take ticks did not read set-play ready (3)";
  EXPECT_EQ(touchesNotLive, 0) << touchesNotLive << " takes did not read in play (1) on the tick after the release touch";
}

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

// EveryManHasHisDemoInfoRef: PES's actor setup builds each man's DemoInfoRef (P+0x3AC8) and PLAYER object (P+0x3AD0) once, FUN_1406EB250 from
// FUN_1406EB670. The throw-in's bookkeeping block (FUN_140792E10 after FUN_1406EC260) calls through that object, so a man without one is a
// block PES would run and GF skips (PesWorld::BookkeepingSkips). The probe reads every active man's two pointers on every tick after the
// first, and the process's skip count at the end of the match.
class DemoInfoProbe : public pestest::Probe {
 public:
  explicit DemoInfoProbe(unsigned int seed)
      : Probe("EveryManHasHisDemoInfoRef", {true, kMinutes, seed}, pestest::kNoTickCap, false, false) {}
  int menWithout = 0, menSeen = 0, aliasedObjects = 0;
  unsigned long skips = 0;
  void OnStart(Match&) override { skipsAtStart_ = pes::PesWorld::BookkeepingSkips(); }
  void OnTick(Match& match) override {
    skips = pes::PesWorld::BookkeepingSkips() - skipsAtStart_;
    if (++ticks_ < 2) return;  // the first tick builds the actors
    for (int t = 0; t < 2; ++t)
      for (const auto& player : match.GetTeam(t)->GetAllPlayers()) {
        if (!player->IsActive()) continue;
        const uint8_t* p = match.PesWorldOf().P(match.PesSlotOf(player.get()));
        uint64_t info = 0, object = 0;
        std::memcpy(&info, p + pes::kPBookkeepingObjectOffset, sizeof info);
        std::memcpy(&object, p + pes::kPBookkeepingObjectOffset + sizeof info, sizeof object);
        ++menSeen;
        if (info == 0 || object == 0) ++menWithout;
        if (info != 0 && info == object) ++aliasedObjects;
      }
  }

 private:
  int ticks_ = 0;
  unsigned long skipsAtStart_ = 0;
};

// One probe per entry of kThrowInSeeds, in the order the tests search them.
BehindLineProbe behindLine[] = {BehindLineProbe(kThrowInSeeds[0]), BehindLineProbe(kThrowInSeeds[1]),
                                 BehindLineProbe(kThrowInSeeds[2]), BehindLineProbe(kThrowInSeeds[3])};
CarriedHoldProbe carriedHold[] = {CarriedHoldProbe(kThrowInSeeds[0]), CarriedHoldProbe(kThrowInSeeds[1]),
                                  CarriedHoldProbe(kThrowInSeeds[2]), CarriedHoldProbe(kThrowInSeeds[3])};
LiftProbe lift[] = {LiftProbe(kThrowInSeeds[0]), LiftProbe(kThrowInSeeds[1]), LiftProbe(kThrowInSeeds[2]),
                    LiftProbe(kThrowInSeeds[3])};
DemoInfoProbe demoInfo(kThrowInSeeds[0]);  // the first searched match: the one the ankle test reads first

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

// A carried throw-in is released (the restart takes place) in the first searched two-minute match that has one.
TEST(PesThrowInRelease, ACarriedThrowInIsReleased) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  int releases = 0;
  for (BehindLineProbe& p : behindLine) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    releases += p.releases;
    if (releases > 0) break;
  }
  EXPECT_GT(releases, 0) << "no carried throw-in was released in any searched two-minute match";
}

// PES's throw-in rule is on the taker's FEET, not on the ball (FUN_140792E10, every tick before the release frame): the ankle nearer
// the centre line (joints 3 and 6 = the ankles, KickJoints 1) must be at least F[1] + 0.1 m (DAT_142588200) from it, else the taker is
// moved outward by the shortfall (FUN_141EB5BB0). Seen live as the hold: live2/pid1482329 seq002/006/009 (action 0x1B, phase 1): the
// taker at z 35.05..35.08 m, 0.99..1.02 m outside the 34.06 m line, the ball at his hip. PES has no rule on where the ball is when it
// leaves the hands. This replaces the ball-position assertions (AThrowInIsReleasedFromInsideTheLine, then ...FromBehindTheLine): both
// were GF's premises; the joint map and the hold captures overturned them.
TEST(PesThrowInRelease, TheThrowInTakersAnklesStayBehindTheLine) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  int ticks = 0, persistent = 0;
  float worst_m = 0.0f;
  for (BehindLineProbe& p : behindLine) {
    pestest::Run(p);
    ASSERT_TRUE(p.setupError.empty()) << p.setupError;
    pestest::Settle(p);
    ticks += p.takingTicks;
    persistent += p.persistentShort;
    worst_m = std::max(worst_m, p.worstShortfall_m);
    if (ticks > 0) break;
  }
  ASSERT_GT(ticks, 0) << "no carried throw-in taker stood ready in any searched match";
  EXPECT_EQ(persistent, 0) << persistent << " of " << ticks << " taking ticks kept the nearer ankle short of F[1] + 0.1 m past one tick (worst by "
                           << worst_m << " m)";
}

// Every man PES builds has his DemoInfoRef and PLAYER object (distinct heap objects), and no bookkeeping block of the match ran without one.
TEST(PesThrowInRelease, EveryManHasHisDemoInfoRef) {
  if (!std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  pestest::Run(demoInfo);
  ASSERT_TRUE(demoInfo.setupError.empty()) << demoInfo.setupError;
  pestest::Settle(demoInfo);
  ASSERT_GT(demoInfo.menSeen, 0);
  EXPECT_EQ(demoInfo.menWithout, 0) << demoInfo.menWithout << " of " << demoInfo.menSeen << " man-ticks had no DemoInfoRef or PLAYER object";
  EXPECT_EQ(demoInfo.aliasedObjects, 0) << "a man's DemoInfoRef and PLAYER object are the same object";
  EXPECT_EQ(demoInfo.skips, 0u) << demoInfo.skips << " bookkeeping blocks ran with no DemoInfoRef";
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
