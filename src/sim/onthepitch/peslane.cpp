#include "onthepitch/peslane.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pes::lane {

namespace {
using offball::Vec2f;

// The scalars the lane core reads, by VA (see LaneConstants).
struct LaneScalar {
  uint64_t va;
  float LaneConstants::*field;
};
constexpr LaneScalar LANE_SCALARS[] = {
    {0x14259be40, &LaneConstants::radToDeg},
    {0x1425795e4, &LaneConstants::twoPi_rad},
    {0x142565600, &LaneConstants::coneScale},
    {0x142588210, &LaneConstants::coneOffset_deg},
    {0x14259be5c, &LaneConstants::coneFloor_deg},
    {0x14259bdc4, &LaneConstants::crossEps},
    {0x14256ae1c, &LaneConstants::crossAdvance_m},
    {0x14259222c, &LaneConstants::crossLen_m},
    {0x142591140, &LaneConstants::crossCone_deg},
    {0x142565604, &LaneConstants::crossRad_m},
};

// Position category 0 is the keeper (spec O1: selector confirmed, meaning INF from the docs).
constexpr int kKeeperCategory = 0;

Vec2f Add(Vec2f a, Vec2f b) { return {a.x + b.x, a.z + b.z}; }
Vec2f Sub(Vec2f a, Vec2f b) { return {a.x - b.x, a.z - b.z}; }
Vec2f Scale(float s, Vec2f v) { return {s * v.x, s * v.z}; }
float Cross2(Vec2f u, Vec2f v) { return u.x * v.z - u.z * v.x; }
float Dist2(Vec2f a, Vec2f b) {
  const Vec2f d = Sub(a, b);
  return d.x * d.x + d.z * d.z;
}

// 0x140A92C80 is sin in degrees; cos(x) is sin(x + 90), as PES takes it. dir(theta) = (sin, -cos): PES z = -GF y.
Vec2f Dir(const LaneConstants& k, float deg) {
  return {TableSinDeg(deg, k.trig.one, k.trig),
          -TableSinDeg(deg + k.trig.quarter, k.trig.one, k.trig)};
}

// The fold into [0, 360).
float Wrap360(float deg, const SpeedClassConstants& t) {
  while (deg >= t.full) deg -= t.full;
  while (deg < 0.0f) deg += t.full;
  return deg;
}

// 0x140439EF0: bearing(a -> b) in [0, 360], atan2(dx, dz), a negative result plus 2*pi.
// ponytail: std::atan2 stands in for PES's atan2 polynomial 0x140A92DC0, undecoded (spec O3).
float Bearing(const LaneConstants& k, Vec2f a, Vec2f b) {
  if (a.x == b.x && a.z == b.z) return 0.0f;
  float rad = std::atan2(b.x - a.x, a.z - b.z);  // PES z = -GF y: the lane's z is the GF y coordinate
  if (rad < 0.0f) rad += k.twoPi_rad;
  return rad * k.radToDeg;
}

// 0x140A900E0: segment a-b and segment c-d cross with each segment's ends on opposite sides.
bool ProperlyCross(Vec2f a, Vec2f b, Vec2f c, Vec2f d) {
  return Cross2(Sub(c, a), Sub(b, a)) * Cross2(Sub(d, a), Sub(b, a)) < 0.0f &&
         Cross2(Sub(a, c), Sub(d, c)) * Cross2(Sub(b, c), Sub(d, c)) < 0.0f;
}
}  // namespace

std::optional<LaneConstants> LoadLaneConstants(const PeImage& exe, std::string* error) {
  auto trig = LoadSpeedClassConstants(exe, error);
  if (!trig) return std::nullopt;
  LaneConstants k{};
  k.trig = *trig;
  for (const LaneScalar& s : LANE_SCALARS)
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*s.field)))) {
      *error = "lane constants: .rdata unreadable";
      return std::nullopt;
    }
  return k;
}

int KeeperSlot(int side, const std::array<LanePlayer, kSlots>& players) {
  for (int j = 0; j < kSideSize; ++j) {
    const int q = kSideSize * side + j;
    if (players[q].active && players[q].positionCategory == kKeeperCategory) return q;
  }
  return kNoSlot;
}

bool IsAwaySlot(int slot) { return kSideSize <= slot && slot < kSlots; }

float HeadingAdd(const LaneConstants& k, float heading_deg, float add_deg) {
  return Wrap360(Wrap360(heading_deg, k.trig) + add_deg, k.trig);
}

LaneScan ScanLane(const LaneConstants& k, const std::array<LanePlayer, kSlots>& players, int reference,
                  int side, const LaneRequest& lane, bool keepKeeper, int horizonFrames) {
  LaneScan out;
  const int keeper = KeeperSlot(side, players);
  // The origin cone applies to opponents only; teammates of the reference skip it.
  const bool opponent = side != (IsAwaySlot(reference) ? 1 : 0);
  const float horizon = static_cast<float>(horizonFrames);
  const float halfCone_deg = k.coneScale * lane.cone_deg;
  const float opponentLimit_deg = std::max(halfCone_deg + k.coneOffset_deg, k.coneFloor_deg);
  const Vec2f origin = lane.origin;
  const Vec2f apex =
      Add(origin, Scale(lane.rad_m, Dir(k, HeadingAdd(k, lane.heading_deg, k.trig.half))));
  const Vec2f end = Add(origin, Scale(lane.len_m, Dir(k, lane.heading_deg)));
  const float innerSq = lane.rad_m * lane.rad_m;
  const float outer_m = lane.rad_m + lane.len_m;
  const float outerSq = outer_m * outer_m;
  float bestD2 = 0.0f;

  for (int j = 0; j < kSideSize; ++j) {
    const int q = kSideSize * side + j;
    if (q == reference || !players[q].active) continue;
    if (!keepKeeper && q == keeper) continue;
    const LanePlayer& m = players[q];
    const Vec2f predicted = Add(m.pos, Scale(horizon, m.vel));
    if (opponent && std::fabs(WrapHalfTurn(Bearing(k, origin, predicted) - lane.heading_deg, k.trig)) >
                        opponentLimit_deg)
      continue;

    const float apexSq = Dist2(predicted, apex);
    const float apexBearing = WrapHalfTurn(Bearing(k, apex, predicted) - lane.heading_deg, k.trig);
    const bool inCone = innerSq < apexSq && apexSq < outerSq && std::fabs(apexBearing) < halfCone_deg;
    const bool crossesLane =
        !inCone && ProperlyCross(origin, end, m.pos, predicted) &&
        std::fabs(Cross2(Sub(end, origin), Sub(predicted, m.pos))) >= k.crossEps;
    if (!inCone && !crossesLane) continue;

    const float d2 = Dist2(predicted, origin);
    ++out.count;
    if (out.nearest == kNoSlot || d2 < bestD2) {
      out.nearest = q;
      bestD2 = d2;
    }
  }
  return out;
}

bool LaneFree(const LaneConstants& k, const std::array<LanePlayer, kSlots>& players, int reference,
              const LaneRequest& lane, bool keepKeeper, int horizonFrames) {
  const int opposing = IsAwaySlot(reference) ? 0 : 1;
  return ScanLane(k, players, reference, opposing, lane, keepKeeper, horizonFrames).count == 0;
}

LaneRequest CrossLane(const LaneConstants& k, offball::Vec2f ball, float aimX_m) {
  // The aim sits on the pitch's centre line at the goal-area line, so the bearing from the ball
  // to it is the lane's heading (0x140474C40 over the aim point, 0x14212972F..0x14212974A).
  const offball::Vec2f aim{aimX_m, 0.0f};
  const float heading_deg = Bearing(k, ball, aim);
  // The origin is the ball point advanced one metre along that bearing (0x140A8F7C0 with r = 1.0,
  // 0x14212974A). dir(theta) = (sin, -cos) in this engine's frame (PES z = -GF y).
  const offball::Vec2f dir = Dir(k, heading_deg);
  const offball::Vec2f origin{ball.x + k.crossAdvance_m * dir.x, ball.z + k.crossAdvance_m * dir.z};
  return {origin, heading_deg, k.crossLen_m, k.crossCone_deg, k.crossRad_m};
}

}  // namespace pes::lane
