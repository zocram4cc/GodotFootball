#include "pesgoalkick.hpp"

#include <algorithm>
#include <cmath>

#include "pesballtrack.hpp"
#include "gametypes.hpp"

namespace pes {
namespace {

// Bisection steps: 2^-40 of the search range, well below the 0.25 m test tolerance.
constexpr int kBisectSteps = 40;

// The ball's flight in PES's space (Y up, metres): GF (x, y, z up) -> Fox (x, z, -y), the inverse
// of ball.cpp's Fox -> GF mapping.
Vec3 ToFox(const Vec3& gf) { return Vec3(gf.x, gf.z, -gf.y); }

// The inverse of ToFox: Fox (X, Y up, Z across) back to GF (x, y across, z up).
Vec3 FromFox(const Vec3& fox) { return Vec3(fox.x, -fox.z, fox.y); }

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfTurn_deg = 180.0f;

// The corner placer's side yaws: DAT_1425AA1C0 (315) for the straight run-up and DAT_14259BE30 (45) for the mirrored one.
constexpr float kCornerYawStraight_deg = 315.0f;
constexpr float kCornerYawMirrored_deg = 45.0f;

// A bearing wrapped to [0, 360).
float Wrap360Deg(float deg) {
  const float wrapped = std::fmod(deg, 360.0f);
  return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

// The placer's turn (sub_140732D40): an offset in the heading's frame, turned in the ground plane with its height kept.
// The sine and cosine are the install's table (FUN_140A92C80), here the library's.
Vec3 TurnByHeading(const Vec3& local, float headingDeg) {
  const float rad = headingDeg * kPi / kHalfTurn_deg;
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  return Vec3(local.z * s + local.x * c, local.y, local.z * c - local.x * s);
}

constexpr float kFullTurn_deg = 360.0f;  // the placer's fold, 0x14259858C
// The penalty's restart yaws (FUN_1408B4560, MODE 6): 90 (0x14259BE5C) from the +X side, 270 (0x1425A2008) from the -X side.
constexpr float kPenaltyRestartNear_deg = 90.0f;
constexpr float kPenaltyRestartFar_deg = 270.0f;
// The free kick's tail (FUN_1407411F0): the corner pull-in, 0.5 m (0x142565600), and the clamp's margin past the marks, 1.5 m
// (0x14259BDDC).
constexpr float kFreeKickCornerPullIn_m = 0.5f;
constexpr float kFreeKickClampMargin_m = 1.5f;

// The placer's fold of one turn (FUN_1404BEF90, and FUN_140782930's own at 0x140782A56-0x140782A76): one turn added below zero,
// one taken at or above 360. A single step, as PES does it.
float FoldOnce360(float deg) {
  if (deg < 0.0f) return deg + kFullTurn_deg;
  if (deg >= kFullTurn_deg) return deg - kFullTurn_deg;
  return deg;
}

// The sign the placer takes: negative below zero, positive otherwise (zero is +1, as its JBE branches go).
float Sign1(float v) { return v < 0.0f ? -1.0f : 1.0f; }

}  // namespace

GoalKickConstants GoalKickConstantsFrom(const SetPieceTakerConstants& set) {
  GoalKickConstants k;
  k.fps = kPesTickHz;
  k.half_s = set.halfSecond_s;
  // The ladder's long wait is the free kick's 3.0 (0x14258C264): the same word, read once by the loader.
  k.threeSeconds_s = set.freeKickGate_s;
  k.tacticsGate = set.goalKickTacticsGate;
  k.centimetre_m = set.centimetre_m;
  k.physiqueLow = set.physiqueLow;
  k.physiqueHigh = set.physiqueHigh;
  k.hitTickDivisor = set.hitTickDivisor;
  return k;
}

GoalKickOrder StepGoalKickLadder(GoalKickLadder& ladder, unsigned long frame, const GoalKickConstants& k,
                                 const GoalKickLadderInputs& in) {
  // sub_142136F00 counts (uint)(fps * seconds + rounding) frames for each wait and window.
  const auto frames = [&k](float seconds) { return static_cast<unsigned long>(k.fps * seconds + k.half_s); };
  const unsigned long elapsed = frame - ladder.stamp;
  const auto enter = [&ladder, frame](GoalKickPhase next) {
    ladder.phase = next;
    ladder.stamp = frame;
  };
  const GoalKickOrder none;
  switch (ladder.phase) {
    case GoalKickPhase::kTaker:
      enter(GoalKickPhase::kWaitOne);
      return none;
    case GoalKickPhase::kWaitOne:  // the 1 s wait: PES adds the rounding to fps itself, with no constant
      if (elapsed >= frames(1.0f)) enter(GoalKickPhase::kShortWindow);
      return none;
    case GoalKickPhase::kShortWindow:
      // The short-target window (FUN_142132660). GF has no short-target picker yet, so it passes without a hit.
      if (elapsed < frames(k.half_s)) return none;
      if (in.specialRestart && in.tactics < k.tacticsGate) {
        enter(GoalKickPhase::kWaitThree);
        return none;
      }
      enter(GoalKickPhase::kLoftedWindow);
      return none;
    case GoalKickPhase::kWaitThree:
      if (!in.specialObjectValid && elapsed < frames(k.threeSeconds_s)) return none;
      enter(GoalKickPhase::kLoftedWindow);
      return none;
    case GoalKickPhase::kLoftedWindow:
      if (elapsed >= frames(k.half_s)) {
        enter(GoalKickPhase::kForced);
        return none;
      }
      if (in.lofted >= 0) return {true, GoalKickCommand::kLoftedToReceiver, in.lofted};
      return none;
    case GoalKickPhase::kForced:
      return {true, GoalKickCommand::kLoftedIntoSpace, -1};
  }
  return none;
}

pes::Vec3 ClipOffsetMetres(const pes::Vec3& hitBallCm, float physique, bool mirror, const GoalKickConstants& k) {
  // FUN_1406E8CC0: x and z take the physique clamped to [low, high]; y takes the physique itself.
  const float clamped = std::clamp(physique, k.physiqueLow, k.physiqueHigh);
  Vec3 out(hitBallCm.x * k.centimetre_m * clamped, hitBallCm.y * k.centimetre_m * physique,
           hitBallCm.z * k.centimetre_m * clamped);
  if (mirror) out.x = -out.x;  // the sign flip on x (*param_2 = fVar5 * -1, 0x142588218)
  return out;
}

pes::Vec3 GoalKickStandPoint(const pes::Vec3& spot, float headingDeg, const pes::Vec3& clipOffsetFox_m) {
  // SetKick negates x and z (the sign mask 0x1425656D0) and keeps the height; the placer turns the rest.
  const Vec3 negated(-clipOffsetFox_m.x, clipOffsetFox_m.y, -clipOffsetFox_m.z);
  const Vec3 turned = TurnByHeading(negated, headingDeg);
  const Vec3 fox = ToFox(spot);
  return FromFox(Vec3(fox.x + turned.x, fox.y + turned.y, fox.z + turned.z));
}

float CornerRestartYawDeg(const Vec3& spot, float penaltySpotX_m) {
  // FUN_1408B4560 (restart kind 4) works in Fox space: X along the goal axis, Z across.
  const Vec3 fox = ToFox(spot);
  const float targetX = (fox.x < 0.0f ? -1.0f : 1.0f) * penaltySpotX_m;
  // FUN_14092E7E0 takes 0 when the spot is the target itself; otherwise the bearing (0 = +Z, 90 = +X) of target - spot.
  if (fox.z == 0.0f && targetX == fox.x) return 0.0f;
  return Wrap360Deg(std::atan2(targetX - fox.x, -fox.z) * kHalfTurn_deg / kPi);
}

float CornerSetKickHeadingDeg(float restartYawDeg, bool mirror) {
  const float side = mirror ? kCornerYawMirrored_deg : kCornerYawStraight_deg;
  return Wrap360Deg(Wrap360Deg(side) + restartYawDeg);
}

Vec3 PlacerLookPoint(const Vec3& stand, float headingDeg) {
  const Vec3 fox = ToFox(stand);
  const Vec3 facing = TurnByHeading(Vec3(0.0f, 0.0f, 1.0f), headingDeg);
  return FromFox(Vec3(fox.x + facing.x, fox.y + facing.y, fox.z + facing.z));
}

float FreeKickRestartYawDeg(const Vec3& spot, float attackSign) {
  // FUN_1408B4560's MODE 5 works in Fox space. The goal the side attacks is at (+F0 or -F0, 0): the sign is the attack sign's
  // (0x1408B45CA-0x1408B4615, `CMP R8B,1`: only +1 keeps F0 positive).
  const Vec3 fox = ToFox(spot);
  const float targetX = (attackSign > 0.0f ? 1.0f : -1.0f) * pesFieldHalfLength;
  // A spot on the goal's own axis has no bearing, and the placer's answer is 0 (0x1408B469B).
  if (fox.z == 0.0f && targetX == fox.x) return 0.0f;
  return Wrap360Deg(std::atan2(targetX - fox.x, -fox.z) * kHalfTurn_deg / kPi);
}

float PenaltyRestartYawDeg(const Vec3& spot) {
  // FUN_1408B4560's MODE 6 (0x1408B45AB-0x1408B45D5): the spot's x against zero.
  return ToFox(spot).x >= 0.0f ? kPenaltyRestartNear_deg : kPenaltyRestartFar_deg;
}

float ContactYawDeg(int rotOffset, bool mirror) {
  // FUN_1406B1C70 returns the record's offset; the mirror flag scales it by -1 (0x1406B1CC7-0x1406B1CDB).
  return FoldOnce360(static_cast<float>(mirror ? -rotOffset : rotOffset));
}

float SetKickHeadingDeg(float contactYawDeg, float restartYawDeg) {
  // FUN_1404BEF90 folds the restart yaw, adds the contact yaw, and folds the sum. The placer adds nothing after it.
  return FoldOnce360(FoldOnce360(restartYawDeg) + contactYawDeg);
}

Vec3 FreeKickFieldClamp(const Vec3& stand) {
  Vec3 fox = ToFox(stand);
  const float halfLength = pesFieldHalfLength, halfWidth = pesFieldHalfWidth;
  // The stand's overshoot past the corner (F0, F1) on each axis. Within 0.5 m of the corner, the axis that overshoots more is pulled
  // in to its mark less 0.5 m: x when |x| - F0 is the larger (0x74148B), else z (0x7414BE).
  const float overX = std::fabs(fox.x) - halfLength, overZ = std::fabs(fox.z) - halfWidth;
  if (std::sqrt(overX * overX + overZ * overZ) < kFreeKickCornerPullIn_m) {
    if (std::fabs(overX) > std::fabs(overZ))
      fox.x = Sign1(fox.x) * (halfLength - kFreeKickCornerPullIn_m);
    else
      fox.z = Sign1(fox.z) * (halfWidth - kFreeKickCornerPullIn_m);
  }
  // Each axis is clamped to its mark plus 1.5 m, keeping its sign (0x7414E9-0x741522).
  if (std::fabs(fox.x) > halfLength + kFreeKickClampMargin_m) fox.x = Sign1(fox.x) * (halfLength + kFreeKickClampMargin_m);
  if (std::fabs(fox.z) > halfWidth + kFreeKickClampMargin_m) fox.z = Sign1(fox.z) * (halfWidth + kFreeKickClampMargin_m);
  return FromFox(fox);
}

float FacingHeadingDeg(const Vec3& gfDirection) {
  // PES's facing yaw: 0 is +Z and 90 is +X in Fox space, where GF's y is -Z.
  const Vec3 fox = ToFox(gfDirection);
  return std::atan2(fox.x, fox.z) * kHalfTurn_deg / kPi;
}

Vec3 PickupContactRoot(const Vec3& ball, float headingDeg, const Vec3& offsetFox_m) {
  // The ball's place in the clip's frame, turned by the heading, is ahead of the root: the root stands that offset behind the
  // ball, so the hands meet the ball at the contact frame (the same turn the placer applies to the stand point).
  const Vec3 fox = ToFox(ball);
  const Vec3 turned = TurnByHeading(offsetFox_m, headingDeg);
  return FromFox(Vec3(fox.x - turned.x, fox.y - turned.y, fox.z - turned.z));
}

Vec3 HandBallFromRoot(const Vec3& root, float headingDeg, const Vec3& offsetFox_m) {
  const Vec3 fox = ToFox(root);
  const Vec3 turned = TurnByHeading(offsetFox_m, headingDeg);
  return FromFox(Vec3(fox.x + turned.x, fox.y + turned.y, fox.z + turned.z));
}

pes::Vec3 GoalKickContactRoot(const pes::Vec3& stand, float headingDeg, const pes::Vec3& hitRootCm, bool mirror,
                              const GoalKickConstants& k) {
  // The root's motion to the hit, in metres, mirrored like the offset and turned by the heading. The hit record
  // has no root height, so the height change is zero.
  Vec3 root(hitRootCm.x * k.centimetre_m, 0.0f, hitRootCm.z * k.centimetre_m);
  if (mirror) root.x = -root.x;
  const Vec3 turned = TurnByHeading(root, headingDeg);
  const Vec3 fox = ToFox(stand);
  return FromFox(Vec3(fox.x + turned.x, fox.y, fox.z + turned.z));
}

unsigned long GoalKickContactTicks(int hitFrame, const GoalKickConstants& k) {
  // FUN_1406B03D0, kind 0: twice the record's frame (a 30 Hz frame), times fps / 60, plus the rounding of the waits.
  return static_cast<unsigned long>(static_cast<float>(hitFrame + hitFrame) * k.fps / k.hitTickDivisor + k.half_s);
}

pes::Vec3 GoalKickStrikeRoot(const pes::Vec3& from, const pes::Vec3& contactRoot, unsigned long elapsed,
                             unsigned long ticks) {
  if (elapsed >= ticks) return contactRoot;
  const float t = static_cast<float>(elapsed) / static_cast<float>(ticks);
  return Vec3(from.x + (contactRoot.x - from.x) * t, from.y + (contactRoot.y - from.y) * t,
              from.z + (contactRoot.z - from.z) * t);
}

int PickGoalKickReceiver(const std::vector<float>& ratings, int takerIndex) {
  int best = -1;
  for (int i = 0; i < static_cast<int>(ratings.size()); ++i) {
    if (i == takerIndex) continue;
    if (best < 0 || ratings[i] > ratings[best]) best = i;
  }
  return best;
}

float PowerForLaunchSpeed(float speed_mps) {
  return speed_mps / kPassTouchSpeedPerPower_mps - kPassTouchPowerOffset;
}

float LoftedFlightDistance(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                           float speed_mps, float receiveHeight_m) {
  pesball::BallState s;
  s.position = ToFox(start);
  s.velocity = ToFox(dir * speed_mps);
  const Vec3 origin = s.position;
  const float radius_m = c.image.ballRadius;
  bool wasAbove = false;
  auto horizontal = [&]() { return std::hypot(s.position.x - origin.x, s.position.z - origin.z); };
  // The flight ends at PES's own horizon: one record per tick, 250 records (pesballtrack.hpp).
  for (unsigned tick = 1; tick < pesball::kTrackLength; ++tick) {
    pesball::BallTick(c, &s, 0, pesball::kPesTick_s);
    if (s.position.y > receiveHeight_m) wasAbove = true;
    if (wasAbove && s.velocity.y < 0.0f && s.position.y <= receiveHeight_m) return horizontal();
    if (s.position.y <= radius_m) return horizontal();
  }
  return horizontal();
}

float LoftedKickSpeed(const PesBallConstants& c, const Vec3& start, const Vec3& dir,
                      float distance_m, float receiveHeight_m) {
  if (LoftedFlightDistance(c, start, dir, kLoftedSearchTop_mps, receiveHeight_m) < distance_m)
    return kLoftedSearchTop_mps;
  float lo = 0.0f;
  float hi = kLoftedSearchTop_mps;
  for (int i = 0; i < kBisectSteps; ++i) {
    const float mid = 0.5f * (lo + hi);
    if (LoftedFlightDistance(c, start, dir, mid, receiveHeight_m) < distance_m) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return 0.5f * (lo + hi);
}

}  // namespace pes
