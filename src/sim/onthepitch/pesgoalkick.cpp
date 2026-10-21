#include "pesgoalkick.hpp"

#include <algorithm>
#include <cmath>

#include "pesballtrack.hpp"

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

// The placer's turn (sub_140732D40): an offset in the heading's frame, turned in the ground plane with its height kept.
// The sine and cosine are the install's table (FUN_140A92C80), here the library's.
Vec3 TurnByHeading(const Vec3& local, float headingDeg) {
  const float rad = headingDeg * kPi / kHalfTurn_deg;
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  return Vec3(local.z * s + local.x * c, local.y, local.z * c - local.x * s);
}

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
