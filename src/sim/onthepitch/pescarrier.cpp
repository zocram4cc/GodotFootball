#include "pescarrier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace pes {
namespace {

// {VA, expected opcode bytes, immediate offset}: the opcode is checked before
// the immediate is trusted, so another build fails loudly.
struct Imm {
  uint64_t va;
  uint8_t opcode[8] = {};
  size_t opcodeLen = 0;
  size_t offset = 0;
};

// .rdata float.
struct Va {
  uint64_t va;
};

template <typename T>
bool ReadImm(const PeImage& exe, const Imm& imm, T* out, std::string* error) {
  uint8_t code[8] = {};
  if (!exe.ReadVa(imm.va, imm.opcodeLen, code) ||
      !std::equal(imm.opcode, imm.opcode + imm.opcodeLen, code) ||
      !exe.ReadVa(imm.va + imm.offset, sizeof *out, reinterpret_cast<uint8_t*>(out))) {
    *error = "carrier constant: unexpected instruction at " +
             [&] {
               char va[32];
               std::snprintf(va, sizeof va, "%#llx",
                             static_cast<unsigned long long>(imm.va));
               return std::string(va);
             }();
    return false;
  }
  return true;
}

bool ReadVa(const PeImage& exe, uint64_t va, float* out, std::string* error) {
  if (!exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out))) {
    *error = "carrier constant: .rdata unreadable";
    return false;
  }
  return true;
}

// The shoot-zone builder's init: ten `mov dword ptr [rcx + off], imm32`, the
// immediate being the float 0x140699610 stores. The opcode is
// `c7 81 <off32> <imm32>` so the immediate sits 6 bytes in.
constexpr uint8_t kMovDwordRcx[] = {0xc7, 0x81};
Imm ZoneInit(uint64_t va, uint32_t off) {
  Imm imm;
  imm.va = va;
  imm.opcodeLen = 6;
  imm.opcode[0] = kMovDwordRcx[0];
  imm.opcode[1] = kMovDwordRcx[1];
  imm.opcode[2] = static_cast<uint8_t>(off);
  imm.opcode[3] = static_cast<uint8_t>(off >> 8);
  imm.opcode[4] = static_cast<uint8_t>(off >> 16);
  imm.opcode[5] = static_cast<uint8_t>(off >> 24);
  imm.offset = 6;
  return imm;
}

// `mov r32, imm32` at `va`, with the immediate `n` immediately behind the
// opcode byte(s) of `opcodeLen` length.
Imm MovImm(uint64_t va, const uint8_t (&opcode)[8], size_t opcodeLen, uint32_t n) {
  Imm imm;
  imm.va = va;
  imm.opcodeLen = opcodeLen;
  for (size_t i = 0; i < opcodeLen; ++i) imm.opcode[i] = opcode[i];
  imm.offset = opcodeLen;
  (void)n;
  return imm;
}

// The zone's ten range/angle fields, in the order 0x140699610 writes them.
inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// The angle wrap helpers (0x140481b00/0x14043a020/0x1404583b0): wrap at 360,
// fold at 180.
inline float WrapDeg(float a) {
  while (a > 180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

inline float Wrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a < 0.0f) a += 360.0f;
  return a;
}

// 180/pi, the radian -> degree factor the bearing helper 0x1404583b0 applies.
constexpr float kPi_deg = 3.14159265358979f;

}  // namespace

namespace {

// Literals of the search the loader does not read. ponytail: the exponent curve
// 3 + 3u is the prior report's (INF(med)); its raw call was not re-verified.
constexpr int kDribbleSteps = 15;     // the radial steps per heading (report 1)
constexpr float kWeightFloor = 0.5f;  // u = clamp01((W - 0.5) * 2), 0x142565600
constexpr float kExpFloor = 3.0f;
constexpr float kExpSpan = 3.0f;
constexpr float kDegPerTurn = 360.0f;
constexpr float kRadPerDeg = 3.14159265358979f / 180.0f;

float Dist(CarrierPoint a, CarrierPoint b) { return std::hypot(a.x - b.x, a.z - b.z); }

// The nearest point on the segment a..b to p.
CarrierPoint ClosestOnSegment(CarrierPoint p, CarrierPoint a, CarrierPoint b) {
  const float dx = b.x - a.x, dz = b.z - a.z;
  const float len2 = dx * dx + dz * dz;
  const float t = len2 == 0.0f ? 0.0f : Clamp01(((p.x - a.x) * dx + (p.z - a.z) * dz) / len2);
  return {a.x + t * dx, a.z + t * dz};
}

}  // namespace

float DribbleThreat(CarrierPoint ball, CarrierPoint p, const std::vector<DribbleOpponent>& opponents,
                    const CarrierConstants& k) {
  float total = 0.0f;
  for (const DribbleOpponent& o : opponents) {
    if (o.reachRo_m <= 0.0f) continue;
    const float reach = std::max(k.threatReachFloor_m, k.threatReachScale * o.reachRo_m);
    const float dx = p.x - o.pos.x, dz = p.z - o.pos.z;
    if (dx * dx + dz * dz > reach * reach) continue;
    // ponytail: o* is the nearest point on his path (pos to pred). The report's
    // path parameter is not in GF; a static man's pred equals his pos.
    CarrierPoint star = ClosestOnSegment(p, o.pos, o.pred);
    // A man already on the ball is pushed out to the push radius along his bearing.
    const float bx = star.x - ball.x, bz = star.z - ball.z;
    const float bd2 = bx * bx + bz * bz;
    if (bd2 < k.threatPushSq_m2) {
      const float n = std::sqrt(bd2);
      if (n > 0.0f) {
        star = {ball.x + bx / n * k.dribbleStep_m, ball.z + bz / n * k.dribbleStep_m};
      } else {
        star = {ball.x + k.dribbleStep_m, ball.z};
      }
    }
    // w = clamp01((d - 5) * -0.25): a man far from the carrier counts for less.
    const float w = Clamp01((Dist(ball, o.pos) - k.threatFullDist_m) * k.threatFalloff);
    float term = std::pow(std::max(o.reachRo_m - Dist(p, star), 0.0f) / o.reachRo_m, 1.0f + w);
    // ponytail: the report's owned-opponent angle factor a_o is taken as 1.
    if (o.keeper) term *= k.threatKeeperScale;
    total += term;
  }
  return total;
}

float DribbleDirection(const DribbleInput& in, const CarrierConstants& k) {
  const float u = Clamp01((in.weight - kWeightFloor) * 2.0f);
  const float expo = kExpFloor + kExpSpan * u;
  const int directions = static_cast<int>(k.dribbleDirections);
  // ponytail: 72 headings and 15 radial steps per think, the cost's ceiling;
  // a coarser step is the upgrade if the smoke runs slow.
  float best = std::numeric_limits<float>::infinity();
  float answer = 0.0f;
  for (int i = 0; i < directions; ++i) {
    const float heading = i * kDegPerTurn / k.dribbleDirections;
    const float rad = heading * kRadPerDeg;
    float cost = 0.0f;
    for (int j = 1; j <= kDribbleSteps; ++j) {
      const float r = j * k.dribbleStep_m;
      const CarrierPoint p{in.ball.x + r * std::cos(rad), in.ball.z + r * std::sin(rad)};
      const float margin = std::min(in.halfLen_m - std::fabs(p.x), in.halfWid_m - std::fabs(p.z));
      const float edge = margin <= k.dribbleEdgeMargin_m ? k.dribbleEdgeCost : 0.0f;
      float pref = 0.0f;
      if (in.weight != 0.0f) {
        const float delta = WrapDeg(heading - in.desired_deg) * kRadPerDeg;
        pref = k.dribblePrefGain * in.weight * (1.0f - r * std::cos(delta) / k.dribbleRange_m);
      }
      const float threat = DribbleThreat(in.ball, p, in.opponents, k);
      const float base = (k.dribbleRange_m - (r - k.dribbleStep_m)) / k.dribbleRange_m;
      cost += std::max(edge, threat + pref) * std::pow(base, expo);
    }
    if (cost < best) {
      best = cost;
      answer = heading;
    }
  }
  return answer;
}

std::optional<CarrierConstants> LoadCarrierConstants(const PeImage& exe, std::string* error) {
  CarrierConstants k{};
  bool ok = true;
  // The zone init's ten immediates.
  const struct { uint64_t va; uint32_t off; float* out; } zone[] = {
      {0x14069964c, 0x180, &k.shootR0_m},        {0x140699656, 0x184, &k.shootR1_m},
      {0x140699660, 0x188, &k.nearCentral_m},    {0x14069966a, 0x18c, &k.farCentral_m},
      {0x140699674, 0x190, &k.nearSide_m},       {0x14069967e, 0x194, &k.farSide_m},
      {0x140699688, 0x198, &k.centralHalfAngle_deg}, {0x140699692, 0x19c, &k.sideAngle_deg},
  };
  for (const auto& z : zone) {
    uint32_t raw = 0;
    ok &= ReadImm(exe, ZoneInit(z.va, z.off), &raw, error);
    std::memcpy(z.out, &raw, sizeof(float));
  }
  // The zone's .rdata floats.
  const std::pair<uint64_t, float*> rdata[] = {
      {0x14258c264, &k.finishingBoost_m},   {0x14258c264, &k.longShotBoost_m},
      {0x14259be4c, &k.finishingFloor},     {0x1425c48cc, &k.finishingTop},
      {0x1425a9460, &k.finishingSpan},      {0x14259be1c, &k.justShootDist_m},
      {0x14259be2c, &k.justShootAngle_deg}, {0x1425a2670, &k.firstTimeDistSq},
      {0x14259be70, &k.firstTimeNearDistSq}, {0x14256ae30, &k.firstTimeIncoming_deg},
      {0x14259be4c, &k.firstTimeBody_deg},   {0x14259bdf0, &k.firstTimePathGate_m},
      {0x142572348, &k.aboveHeadFactor},    {0x14253ee30, &k.statDivisor},
      {0x142588210, &k.movingBallFloor_kmh}, {0x14259be34, &k.movingBallSpan_kmh},
      {0x14259ce38, &k.kmhPerMs},           {0x142588210, &k.distFloor_m},
      {0x14259be1c, &k.distSpan_m},         {0x14259be68, &k.ballAngleHigh_deg},
      {0x1425c48fc, &k.ballAngleSpan_deg},   {0x142565600, &k.oneTouchGain},
      {0x14259be10, &k.runFloor_kmh},        {0x14259be04, &k.runSpan_kmh},
      {0x14259be4c, &k.turnFloor_deg},       {0x1425a9bf0, &k.turnTop_deg},
      {0x142598958, &k.highFloor_m},         {0x142bef908, &k.highSpan_m},
      {0x142565604, &k.diffMovingBallWeight}, {0x14259bddc, &k.diffRunWeight},
      {0x14259bddc, &k.diffTurnWeight},       {0x142596e6c, &k.lowPassFloor},
      {0x1425c48cc, &k.lowPassTop},           {0x1425c80e0, &k.lowPassSpan},
      {0x14257a194, &k.lowPassBase},          {0x142590d98, &k.lowPassGain},
      {0x14259a764, &k.forwardAngle_deg},     {0x14259d628, &k.squareAngle_deg},
      {0x14256ae1c, &k.openNear_m},           {0x1425a9460, &k.openSpan_m},
      {0x14259be5c, &k.openInFront_deg},      {0x14259bddc, &k.openBlockMargin_m},
      {0x14256ae1c, &k.openLaneMargin_m},     {0x142598578, &k.openAlongMin_m},
      {0x14257a190, &k.openAlongGain},        {0x142590d98, &k.openAlongPenalty},
      {0x14253ee30, &k.openMarginScale},      {0x14259be30, &k.openKLowAngle_deg},
      {0x14259be5c, &k.openKHighAngle_deg},   {0x142588210, &k.openClass1},
      {0x14259be2c, &k.openClass2},           {0x14259a764, &k.openClass3},
      {0x14253ee30, &k.openClass4},           {0x1425a9bf0, &k.openClass5},
      {0x14259a764, &k.fwdPlanAngleGate_deg}, {0x1425a9464, &k.fwdTeamFlagGate},
      {0x14256ae30, &k.fwdCloseRange_deg},    {0x1425c65f4, &k.fwdShortPassDistSq},  {0x142591140, &k.fwdGoalCentreDist_m},
      {0x142592228, &k.fwdLaneWidthGain},     {0x14259bddc, &k.fwdLaneWidthMin_m},
      {0x14259bdec, &k.fwdLaneWidthMax_m},    {0x1425ca310, &k.fwdLaneBudget},
      {0x1425c5258, &k.fwdLineValueSq},       {0x14259ce40, &k.fwdLineValueFarSq},
      {0x14259bdf4, &k.fwdNearestOppFar_m},   {0x14259222c, &k.fwdNearestOppNear_m},
      {0x14259bddc, &k.fwdNearestOppMin_m},   {0x14259a760, &k.fwdTargetAdvance_m},
      {0x14259bdf0, &k.fwdOppNearDist_m},     {0x14258c264, &k.fwdOppNearFreeDist_m},
      {0x142596e6c, &k.fwdTurnFacing_deg},    {0x14259be60, &k.fwdFacingBack_deg},
      {0x14259be1c, &k.fwdMateSpeedCap_kmh},  {0x14259be0c, &k.loftMinDist_m},
      {0x14259be20, &k.loftMaxDist_m},        {0x142588210, &k.loftDistBase_m},
      {0x14259222c, &k.loftDistGain_m},       {0x14253ee30, &k.loftKeeperNearSq},
      {0x14259be70, &k.loftKeeperFarSq},      {0x14259222c, &k.throughMinDist_m},
      {0x14259be1c, &k.throughCap_m},         {0x14259be0c, &k.throughOpenCap_m},
      {0x14253ee30, &k.throughOpennessOpen},  {0x14259a764, &k.throughMateSpeedCap_kmh},
      {0x14259be60, &k.throughFacing_deg},    {0x142bf02b4, &k.breakthroughSeedK},
      {0x14260ba7c, &k.middleShotSeedK},      {0x14260b388, &k.curveShotSeedK},
      {0x1425e9094, &k.crossSeedK},           {0x14259ce40, &k.longSeedK},
      {0x14259be4c, &k.statFloor},            {0x1425a9460, &k.statSpan},
      {0x14259be0c, &k.breakthroughStatGain}, {0x14259be1c, &k.breakthroughStatSpan},
      {0x14259be1c, &k.crossStatGain},
      {0x14253ee30, &k.dribbleEdgeCost},     {0x142565600, &k.dribbleEdgeMargin_m},
      {0x14257a190, &k.dribblePrefGain},     {0x142588210, &k.dribbleRange_m},
      {0x14258d10c, &k.dribbleStep_m},       {0x1425cb47c, &k.dribbleDirections},
      {0x14259222c, &k.threatReachFloor_m},  {0x142565604, &k.threatReachScale},
      {0x142565604, &k.threatKeeperScale},   {0x14259222c, &k.threatFullDist_m},
      {0x1425c48ec, &k.threatFalloff},       {0x142bf0a88, &k.threatPushSq_m2},
      // The curve shot's stat gain is the same 10.0 the dribble search uses;
      // the ratchet's window and lead and the sticky mode's half line are
      // 0x14253ee30, 0x142588210 and 0x142565600.
      {0x142588210, &k.curveShotStatGain},   {0x14253ee30, &k.intentionClose_deg},
      {0x142588210, &k.intentionLead_m},     {0x142565600, &k.imageHalfLine},
  };
  for (const auto& r : rdata) ok &= ReadVa(exe, r.first, r.second, error);
  // The turn term's span, from the same instruction that loads its 130 top.
  ok &= ReadVa(exe, 0x14259be4c, &k.turnSpan_deg, error);

  // The rolled plans' base probabilities, from the immediates of the plan
  // functions 0x14211a800 (Breakthrough), 0x14211b460 (MiddleShot),
  // 0x14211ab10 (Cross) and 0x14211b1d0 (Long).
  // The plan bases are the `mov r32, imm32` immediates of the plan functions
  // (0x14211a800 Breakthrough, 0x14211b460 MiddleShot, 0x14211ab10 Cross,
  // 0x14211b1d0 Long). For a `mov r32, imm32` the immediate sits directly
  // behind the opcode - one byte for the b8+r form, two for the 41 b8 REX
  // form - so the address plus the opcode length pins the instruction, and the
  // value is read from behind it.
  uint32_t u = 0;
  auto imm32 = [&](uint64_t va, size_t opcodeLen, float* out) {
    if (!exe.ReadVa(va + opcodeLen, sizeof u, reinterpret_cast<uint8_t*>(&u))) {
      *error = "carrier constant: immediate unreadable";
      ok = false;
      return;
    }
    *out = static_cast<float>(u);
  };
  imm32(0x14211a956, 1, &k.breakthroughBase);
  imm32(0x14211a991, 1, &k.breakthroughComBase);
  imm32(0x14211b628, 1, &k.middleShotBase);
  imm32(0x14211bcf9, 2, &k.middleShotStateGain);
  imm32(0x14211b716, 1, &k.middleShotCap);
  imm32(0x14211ac86, 1, &k.crossBase);
  imm32(0x14211ad57, 1, &k.crossCap);
  imm32(0x14211ad5e, 1, &k.crossFloor);
  imm32(0x14211b289, 1, &k.longBasePressed);
  imm32(0x14211b2a9, 1, &k.longBaseFree);
  imm32(0x14211b2c5, 1, &k.longBaseMid);
  // The curve shot's two 30s, its floor and its cap are byte immediates: the
  // `and ecx, 0x1e` at 0x14211b052, the `lea ebx, [rcx + 0x1e]` displacement at
  // 0x14211b05e, the `mov eax, 5` at 0x14211b106 and the `cmp ebx, 0x46` at
  // 0x14211b0fa.
  uint8_t immByte = 0;
  auto imm8 = [&](uint64_t va, size_t opcodeLen, float* out) {
    if (!exe.ReadVa(va + opcodeLen, sizeof immByte, &immByte)) {
      *error = "carrier constant: immediate unreadable";
      ok = false;
      return;
    }
    *out = static_cast<float>(immByte);
  };
  imm8(0x14211b052, 2, &k.curveShotComBase);
  imm8(0x14211b05e, 2, &k.curveShotSkillBase);
  imm8(0x14211b106, 1, &k.curveShotFloor);
  imm8(0x14211b0fa, 2, &k.curveShotCap);
  // The Long plan's four bonuses are byte immediates as well: the `lea`
  // displacements of COM 6 (0x14211b2d9) and skill 13 (0x14211b2ec), the level
  // step of `add edi, 0x14` (0x14211b30b) and Path B's `and ecx, 0xa`
  // (0x14211b36b).
  imm8(0x14211b2d9, 2, &k.longComBonus);
  imm8(0x14211b2ec, 2, &k.longSkillBonus);
  imm8(0x14211b30b, 2, &k.longLevelStep);
  imm8(0x14211b36b, 2, &k.longPathBSkillBonus);
  // The caps and floors that are not immediates: Breakthrough's is the
  // `cmp ebx, 0x46; cmova ebx, edx(0)` of 0x14211aa7e, MiddleShot's floor the
  // -5 comparison at 0x14259be7c, and its stat gain the 15 of 0x14259be0c.
  k.breakthroughCap = 70.0f;      // ponytail: 0x14211aa7e, the clamp operand
  k.middleShotFloor = 5.0f;       // ponytail: 0x14259be7c (-5, negated)
  k.middleShotStatGain = 15.0f;   // ponytail: 0x14259be0c (15)
  k.longStatGain = 10.0f;         // ponytail: report 4.4's P table
  k.throughClassOpen = 4.0f;      // ponytail: report 2.6, `rec.class < 4`

  if (!ok) return std::nullopt;
  return k;
}

ShootZone ComputeShootZone(float ballX, float ballZ, float attackSign, float finishing,
                           bool longShotSkill, float halfLen_m, float goalHalfWidth_m,
                           const CarrierConstants& k) {
  ShootZone z;
  // The near central range ramps with Finishing (0x140698e0d: below 70 it
  // stays, at 99 or above it is the full gain, in between (x-70)/29), and the
  // far one with the long-shot skills (0x140698e5a, `edx` the 39|33|8 mask).
  const float near = k.nearCentral_m + k.finishingBoost_m * Clamp01((finishing - k.finishingFloor) /
                                                                   k.finishingSpan);
  const float far = k.farCentral_m + (longShotSkill ? k.longShotBoost_m : 0.0f);

  // d (+0x170) and phi (+0x174), and the sector (+0x178): inside the posts the
  // distance is to the goal line and the bearing is 0; outside them the point
  // is the near post (0x140698ef6).
  const float postZ = (ballZ < 0.0f ? -1.0f : 1.0f) * goalHalfWidth_m;
  const bool outsidePosts = std::fabs(ballZ) > goalHalfWidth_m;
  if (outsidePosts) {
    const float dx = ballX - attackSign * halfLen_m;
    const float dz = ballZ - postZ;
    z.d_m = std::sqrt(dx * dx + dz * dz);
    const float bearing = Wrap360(std::atan2(dz, dx) * 180.0f / kPi_deg + 180.0f);
    z.angle_deg = WrapDeg(bearing);
  } else {
    z.d_m = halfLen_m - attackSign * ballX;
    z.angle_deg = 0.0f;
  }
  if (outsidePosts) {
    const float absAngle = std::fabs(z.angle_deg);
    z.sector = absAngle <= k.centralHalfAngle_deg
                   ? kSectorCentral
                   : (absAngle <= k.sideAngle_deg
                          ? (z.angle_deg < 0.0f ? kSectorSideLow : kSectorSideHigh)
                          : kSectorWide);
  } else {
    z.sector = kSectorCentral;
  }
  // The band (+0x17C): behind the goal line both band and sector go wide
  // (0x140698fbf), otherwise the central range for the central sector and the
  // side range for the side sectors (0x140698fd5..0x140699029).
  if (attackSign * ballX < 0.0f) {
    z.band = kBandOut;
    z.sector = kSectorWide;
  } else if (z.sector == kSectorCentral) {
    z.band = z.d_m <= near ? kBandClose : (z.d_m > far ? kBandFar : kBandMid);
  } else {
    z.band = z.d_m <= k.nearSide_m ? kBandClose : (z.d_m > k.farSide_m ? kBandFar : kBandMid);
  }
  // bit0 (+0x16C): the ball inside a 180 degree half-disc of radius r0 whose
  // centre is the goal centre (0x1406990a3 writes box 0 as a kind-2 disc with
  // the attack direction at +0xC and 180 degrees at +0x10). Inside 90 degrees
  // of the attack direction and within r0 of the centre.
  const float centreX = attackSign * halfLen_m;
  const float dx = ballX - centreX;
  const float distSq = dx * dx + ballZ * ballZ;
  // The cone's own direction points from the goal into the pitch, so the ball
  // must be on the pitch side of the goal centre (`attackSign * dx <= 0`) and
  // within r0 of it; a ball exactly level with the goal line is on the
  // boundary, which PES's `setbe` includes.
  z.inBox = distSq <= k.shootR0_m * k.shootR0_m && attackSign * dx <= 0.0f;
  return z;
}

bool InShootRange(const ShootZone& z) { return z.d_m >= 0.0f && (z.inBox || (z.band < 3 && z.sector < 3)); }
bool IsCentral(const ShootZone& z) { return InShootRange(z) && z.sector == kSectorCentral; }
bool IsMid(const ShootZone& z) { return InShootRange(z) && z.band < 2; }
bool IsClose(const ShootZone& z) { return InShootRange(z) && z.band == kBandClose; }
bool InBox(const ShootZone& z) { return z.inBox; }
bool InHeaderRange(const ShootZone& z, const CarrierConstants& k) {
  return InShootRange(z) && z.d_m <= k.shootR1_m;
}

// The header-range gate PES adds to it: the ball must be over the shooter's
// head, which is `ballY > 0.8 * height/100` (0x142121a5e).
inline bool BallAboveHead(float ballHeight_m, float height_m, const CarrierConstants& k) {
  return ballHeight_m > k.aboveHeadFactor * height_m / k.statDivisor;
}

int ShootSubKind(const ShootInput& in, const CarrierConstants& k) {
  const ShootZone& z = in.zone;
  // No-shot exits, in the order 0x142121920 takes them: outside the shoot
  // range with no usable keeper, and the first-time gates.
  if (!InShootRange(z) && !in.keeperPresent) return 0;
  if (!in.owned && !in.volatileShot) {
    if (std::fabs(WrapDeg(in.incomingVsFacing_deg)) > k.firstTimeIncoming_deg) return 0;
    if (in.ballToGoalDistSq > k.firstTimeDistSq) return 0;
    if (in.ballToGoalDistSq > k.firstTimeNearDistSq &&
        std::fabs(WrapDeg(in.aimVsFacing_deg)) > k.firstTimeBody_deg &&
        in.bodyPathGate_m > k.firstTimePathGate_m)
      return 0;
  }
  // "just shoot": inside 20 m and inside 40 degrees, with the keeper settled.
  if (z.d_m < k.justShootDist_m && std::fabs(z.angle_deg) < k.justShootAngle_deg &&
      in.keeperPresent && in.keeperSettled)
    return 1;
  // The box, or a header inside the header range, or the goal-shot-pass plan.
  if (InBox(z)) return 1;
  if (BallAboveHead(in.ballHeight_m, in.height_m, k) && InHeaderRange(z, k)) return 1;
  if (in.planId == kPlanShotPassFromGoal) return 1;
  // The remaining branches price the lane and the aim; without the lane test
  // and the capability rows this engine does not carry, a shot in range with
  // the goal in front of him is the answer.
  // ponytail: the wide-shot evaluator 0x142120e60 and central_eval 0x14211fe20
  // are not ported (report 9.1); a central, in-range carrier shoots.
  if (IsCentral(z) && std::fabs(WrapDeg(in.aimVsFacing_deg)) < k.openInFront_deg) return 1;
  return 0;
}

int DirectionClass(float fwdAngle_deg, const CarrierConstants& k) {
  const float a = std::fabs(WrapDeg(fwdAngle_deg));
  if (a <= k.forwardAngle_deg) return 0;
  if (a <= k.squareAngle_deg) return 2;
  return 1;
}

int TurnClass(float turnAngle_deg, const CarrierConstants& k) {
  const float a = std::fabs(WrapDeg(turnAngle_deg));
  if (a <= k.forwardAngle_deg) return 3;
  if (a <= k.squareAngle_deg) return 4;
  return 5;
}

float ReceiverDifficulty(float ballSpeed_mps, float dist_m, float incomingAngle_deg,
                         bool oneTouchPass, float carrierSpeed_kmh, bool running,
                         float turnAngle_deg, float ballHeight_m, float lowPass,
                         const CarrierConstants& k) {
  // The moving-ball term (a first-time pass of a ball on the move).
  float movingBall = 0.0f;
  if (ballSpeed_mps > 0.0f) {
    const float speed = Clamp01((ballSpeed_mps * k.kmhPerMs - k.movingBallFloor_kmh) /
                               k.movingBallSpan_kmh);
    const float distance = Clamp01((dist_m - k.distFloor_m) / k.distSpan_m);
    float angle = 1.0f;
    const float a = std::fabs(WrapDeg(incomingAngle_deg));
    if (a > k.ballAngleHigh_deg) angle = 0.0f;
    else if (a > 90.0f) angle = (k.ballAngleHigh_deg - a) / k.ballAngleSpan_deg;
    movingBall = (angle + distance) * speed * speed;
    if (oneTouchPass) movingBall *= k.oneTouchGain;
  }
  const float run = running ? Clamp01((carrierSpeed_kmh - k.runFloor_kmh) / k.runSpan_kmh) : 0.0f;
  const float turn = Clamp01((std::fabs(WrapDeg(turnAngle_deg)) - k.turnFloor_deg) / k.turnSpan_deg);
  const float high = Clamp01((ballHeight_m - k.highFloor_m) / k.highSpan_m);
  float diff = k.diffMovingBallWeight * movingBall + k.diffRunWeight * run +
               k.diffTurnWeight * turn + high;
  // The per-player Low Pass multiplier, 0.7 + 0.3 * g(LowPass).
  float g = 0.0f;
  if (lowPass <= k.lowPassFloor) g = 1.0f;
  else if (lowPass < k.lowPassTop) g = (k.lowPassTop - lowPass) / k.lowPassSpan;
  diff *= k.lowPassBase + k.lowPassGain * g;
  return diff;
}

int OpennessClassFromMargin(float margin, const CarrierConstants& k) {
  if (k.openClass1 > margin) return 0;
  if (k.openClass2 > margin) return 1;
  if (k.openClass3 > margin) return 2;
  if (k.openClass4 > margin) return 3;
  if (k.openClass5 >= margin) return 4;
  return 5;
}

float LaneOpennessMargin(float laneX, float laneZ, float along, float perp, float kFactor,
                         float difficulty, const CarrierConstants& k) {
  if (along < k.openAlongMin_m) return 0.0f;
  float t = k.openAlongGain * along;
  const float penalty = k.openAlongPenalty * (std::max(along - 1.0f, 0.0f) *
                                             std::max(along - 1.0f, 0.0f));
  t = std::min(t, penalty);
  if (kFactor != 1.0f) t *= kFactor * kFactor;
  if (difficulty > 1.0f) t *= difficulty + 1.0f;
  (void)laneX;
  (void)laneZ;
  return k.openMarginScale * std::max(perp - k.openLaneMargin_m, 0.0f) / t;
}

PassAnswer PassForward(const std::vector<PassCandidate>& mates,
                       const std::vector<LaneOpponent>& opponents, const PassForwardInput& in,
                       const CarrierConstants& k) {
  PassAnswer answer;
  // Gate: (own or press.cls < 2) and not close (0x14212a700's head).
  if (!((in.owned || in.carrierPressureClass < 2) && !in.close)) return answer;
  (void)opponents;

  // The openness a mate must have to take a short forward ball: 0x142125f9b's
  // ladder over the image state and the pass's forward angle, then the short-
  // pass tightening (a pass shorter than 30 m needs a very open lane).
  auto needOpenness = [&](const ReceiverInput& r) {
    if (in.danger || r.difficulty > 1.0f) return -1.0f;  // not eligible at all
    float need = 3.0f;
    if (in.imageState == 0) {
      need = r.fwdAngle_deg > k.forwardAngle_deg ? 4.0f : 2.0f;
    } else if (in.imageState == 3 || in.imageState == 4) {
      need = 2.0f;
    } else if (r.fwdAngle_deg > k.fwdCloseRange_deg) {
      need = 4.0f;
    } else if (!in.mateLaneOk && r.fwdAngle_deg <= 30.0f) {
      need = 2.0f;
    }
    // A pass shorter than 30 m (900 = 30^2 @0x1425c65f4) needs openness 4.
    if (r.ballToTarget_m * r.ballToTarget_m < k.fwdShortPassDistSq) need = 4.0f;
    return need;
  };

  int shortIdx = -1, loftIdx = -1, throughIdx = -1;
  float shortAdvance = -1.0e30f, loftAdvance = -1.0e30f, throughAdvance = -1.0e30f;
  for (const PassCandidate& m : mates) {
    if (m.index < 0) continue;
    const ReceiverInput& r = m.record;
    const float advance = in.attackSign * r.target.x;

    // --- tier 1: short forward, 0x142125c80 ---
    const float need = needOpenness(r);
    if (need >= 0.0f &&
        std::fabs(WrapDeg(r.passAngle_deg - in.planAngle_deg)) <= k.fwdPlanAngleGate_deg &&
        r.opennessClass >= static_cast<int>(need) &&
        std::fabs(r.ballToTarget_m) <= k.fwdTargetAdvance_m &&
        m.nearestOppToMate_m >= (m.mateFree ? k.fwdOppNearFreeDist_m : k.fwdOppNearDist_m) &&
        std::fabs(WrapDeg(r.turnAngle_deg)) < k.fwdTurnFacing_deg && advance > shortAdvance) {
      shortIdx = m.index;
      shortAdvance = advance;
    }

    // --- tier 2: through, 0x142127ff0, only with free space ---
    if (in.freeSpace > k.fwdTeamFlagGate && r.ballToTarget_m >= k.throughMinDist_m &&
        r.ballToTarget_m <= (r.openness >= k.throughOpennessOpen && r.opennessClass < k.throughClassOpen
                                 ? k.throughOpenCap_m
                                 : k.throughCap_m) &&
        advance > throughAdvance) {
      throughIdx = m.index;
      throughAdvance = advance;
    }

    // --- tier 3: lofted forward, 0x14212aaa2's cascade ---
    if (r.ballToTarget_m >= k.loftMinDist_m && r.ballToTarget_m <= k.loftMaxDist_m &&
        r.turnClass == 3 && r.dirClass != 1 && r.opennessClass == 0 && m.pressureClass > 2) {
      const float ramp = Clamp01((r.ballToTarget_m - k.loftMinDist_m) /
                                 (k.loftMaxDist_m - k.loftMinDist_m));
      const float keeperDist = k.loftDistBase_m + k.loftDistGain_m * ramp;
      const bool wide = in.planWideSide != 3 || std::fabs(r.target.z) > in.goalAreaHalfWidth_m;
      if (keeperDist * keeperDist < m.pressureDistOpp0_m && wide && advance > loftAdvance) {
        loftIdx = m.index;
        loftAdvance = advance;
      }
    }
  }

  // The tiers are attempted in order (0x14212a700): short forward, then the
  // through ball when the carrier's side has free space (image states 1 and 2,
  // 0x14212a8c0), then the lofted forward (also states 1 and 2).
  if (shortIdx >= 0) {
    answer.index = shortIdx;
    answer.kind = kPassShort;
    return answer;
  }
  if (in.freeSpace > k.fwdTeamFlagGate && (in.imageState == 1 || in.imageState == 2) &&
      throughIdx >= 0) {
    answer.index = throughIdx;
    answer.kind = kPassThrough;
    return answer;
  }
  if (loftIdx >= 0 && (in.imageState == 1 || in.imageState == 2)) {
    answer.index = loftIdx;
    answer.kind = kPassLong;
  }
  return answer;
}

// The rolled start probabilities of report 4.4's P table, for Breakthrough,
// MiddleShot and Cross. Each plan's base is its own instruction's immediate;
// every plan adds its own stat term, gain * clamp01((x - 70)/span), and clamps
// to its own window. `skill`/`com` are the plan's skill and COM triggers and
// `state2` MiddleShot's image-state-2 bonus.
float PlanStartProbability(int planId, const CarrierConstants& k, float stat, bool skill, bool com,
                           bool state2) {
  const float gain = [&]() {
    switch (planId) {
      case kPlanBreakthrough: return k.breakthroughStatGain;
      case kPlanMiddleShot: return k.middleShotStatGain;
      case kPlanCross: return k.crossStatGain;
      default: return 0.0f;
    }
  }();
  // The ramp's divisor is per plan (report 3.17): Breakthrough saturates at
  // stat 90 (divisor 20, 0x14259be1c), the other plans at 99 (29, 0x1425a9460).
  const float span = planId == kPlanBreakthrough ? k.breakthroughStatSpan : k.statSpan;
  const float statTerm = gain * Clamp01((stat - k.statFloor) / span);

  switch (planId) {
    case kPlanBreakthrough: {
      // Base 70 with the Double Touch skill or the +0x68 instruction, 40 with
      // the +0x68 == 1 instruction, 20 with a COM run style.
      float p = k.breakthroughComBase;
      if (skill) p = k.breakthroughBase;
      else if (com) p = std::max(p, k.breakthroughComBase);
      return std::min(p + statTerm, k.breakthroughCap);
    }
    case kPlanMiddleShot:
      // Base 70 with a long-shot skill or COM 5, 0 without.
      return std::max(std::min(((skill || com) ? k.middleShotBase : 0.0f) +
                                   (state2 ? k.middleShotStateGain : 0.0f) + statTerm,
                               k.middleShotCap),
                      k.middleShotFloor);
    case kPlanCross:
      // Base 50 with the Early Cross COM or Pinpoint Cross. ponytail: the Crosser style (bit 7,
      // report 4.4) is not read; the caller has no style input here.
      return std::max(std::min(((skill || com) ? k.crossBase : 0.0f) + statTerm, k.crossCap),
                      k.crossFloor);
    default:
      return -1.0f;
  }
}

int PlanRoll(const CarrierSeed& seed, float seedK, const RngConstants& rng,
             const CarrierConstants& k) {
  Random draw(rng);
  draw.SetSeed(k.planRollSlot, static_cast<uint32_t>(static_cast<int>(seed.snapshot * seedK)));
  return draw.RangeRaw(k.planRollSlot, k.planRollModulus);
}

void CarrierSeedTick(CarrierSeed& seed, bool predicate, float randomInfoUnit) {
  if (!predicate) {
    seed = CarrierSeed();  // 0x14063a3b0: count 0, snapshot -1.0
    return;
  }
  ++seed.updates;  // 0x14063a490: the counter at +0x8
  if (seed.snapshot < 0.0f || seed.updates == 1) seed.snapshot = randomInfoUnit;
}

float CurveShotScore(const CurveShotInput& in, const CarrierConstants& k) {
  // The head gates, in the order 0x14211af20 takes them.
  if (in.intention < 3) return -1.0f;        // 0x14211afa1, `cmp eax, 3; jl`
  if (in.trajectory_m > 0.0f) return -1.0f;  // 0x14211afb9, `comiss; ja`
  if (in.fullAttack) return -1.0f;           // 0x14211afc6, `cmp [rbp+0xb8a2], 0; jnz`
  // The two threshold ratios against the self scalar (0x14211afe6, 0x14211aff4):
  // the pair -1/+1 is selected twice and the score only continues when both
  // pick the same one (0x14211b000). With eval+0x48 != 1 the second threshold
  // is eval+0x14 negated (0x14211b028). (Report 6.4.2 reads the fields as a
  // stronger-foot test; the comparison itself is all that is decoded.)
  const float secondThreshold = in.sideFlag ? in.sideA : -in.sideA;
  if ((in.sideSelf > in.sideB) != (in.sideSelf > secondThreshold)) return -1.0f;

  // The base (0x14211b046): 30 for COM 4, 30 more for the skill bit 7.
  int base = 0;
  if (in.incisiveRun) base += static_cast<int>(k.curveShotComBase);   // 0x14211b052
  if (in.skill7) base += static_cast<int>(k.curveShotSkillBase);      // 0x14211b05e

  // Each stat adds gain * clamp01((stat - floor)/span) to the running integer
  // as a float and truncates that step (0x14211b0b8, 0x14211b0f5), so the
  // fractions are discarded between the two stats. Kicking Power (+0x2a) comes
  // first (0x14211b064), Finishing (+0x1a) second (0x14211b0bd).
  const auto statStep = [&](float stat) {
    return k.curveShotStatGain * Clamp01((stat - k.statFloor) / k.statSpan);
  };
  float acc = static_cast<float>(base);
  acc = std::trunc(acc + statStep(in.kickingPower));  // +0x2A first
  acc = std::trunc(acc + statStep(in.finishing));     // then +0x1A
  // Floor 5 then cap 70 (0x14211b0fa..0x14211b110).
  acc = std::min(acc, k.curveShotCap);
  acc = std::max(acc, k.curveShotFloor);
  return acc;
}

bool CurveShotAdmitted(const CurveShotInput& in, int draw, const CarrierConstants& k) {
  const float score = CurveShotScore(in, k);
  if (score < 0.0f) return false;  // a head gate
  // `cmp ebx, eax; jae` at 0x14211b159: the score against the draw.
  if (draw > static_cast<int>(score)) return false;
  // The final gate (0x14211b176, 0x14211b18e): the skill 7 card admits, the COM
  // 4 card admits, neither does not. The binary's third arm - team flag
  // 0xb8a2 set accepts (0x14211b169) - sits behind the head gate's reject at
  // 0x14211afc6, so it can never be reached.
  return in.skill7 || in.incisiveRun;
}

// The veto (0x14211a570): Intention 3 or 4 (`cmp eax, 3/4; jz`), or an engagement
// below 1 (`cmp [rax+0x10], 1; jl`).
bool LongVetoed(const LongInput& in) {
  return in.intention == 3 || in.intention == 4 || in.engagement < 1;
}

// The team level adds one step per level, and the binary's compares
// (`cmp ecx, 2` at 0x14211b30e) stop at two.
constexpr int kLongLevelSteps = 2;

float LongScore(const LongInput& in, const CarrierConstants& k) {
  // The lofted ramp, 10 * clamp01((lofted - 70)/29) (0x14211b31a..b3cf), joins
  // the integer part and the sum truncates once.
  const float ramp = k.longStatGain * Clamp01((in.lofted - k.statFloor) / k.statSpan);
  if (in.teamSetting) {
    // Path B (0x14211b35a): 10 for each skill and the ramp; no base, COM or level.
    float bonus = 0.0f;
    if (in.skillWeightedPass) bonus += k.longPathBSkillBonus;
    if (in.skillLowLoftedPass) bonus += k.longPathBSkillBonus;
    return std::trunc(bonus + ramp);
  }
  // Path A (0x14211b289): the base by the pressure class (30 below 2, 60 at 2,
  // 80 at 3 and above), then 50 for COM 6 and for each skill, then the level.
  float base = k.longBaseFree;
  if (in.engagement < 2) base = k.longBasePressed;
  else if (in.engagement == 2) base = k.longBaseMid;
  if (in.comLongBallExpert) base += k.longComBonus;
  if (in.skillWeightedPass) base += k.longSkillBonus;
  if (in.skillLowLoftedPass) base += k.longSkillBonus;
  base += k.longLevelStep * static_cast<float>(std::clamp(in.level, 0, kLongLevelSteps));
  return std::trunc(base + ramp);
}

bool LongAdmitted(const LongInput& in, int draw, const CarrierConstants& k) {
  if (LongVetoed(in)) return false;
  // The accepts (0x14211b259..b279) answer before the roll: the team's mode 3,
  // the carrier as the designated player, and the full-attack flag.
  if (in.teamModeThree || in.designatedSelf || in.fullAttack) return true;
  // The draw is 0..99 and a plan starts at or below its score (`cmp ebx, eax`
  // at 0x14211b422 takes the reject when the score is below the draw).
  return draw <= static_cast<int>(LongScore(in, k));
}

int NextIntention(int state, const IntentionInput& in, const CarrierConstants& k) {
  if (state == 4) return 4;  // 0x142119862, absorbing
  if (in.closeBand &&
      std::fabs(WrapDeg(in.headingDiff_deg)) < k.intentionClose_deg) {
    return 4;  // 0x1421198f7, `comiss xmm3(100), |d|; jbe` skips
  }
  if (state == 3 || in.attackDot > in.penaltyLine_m ||
      in.ballLead_m + k.intentionLead_m > in.ballLimit_m) {
    return 3;  // 0x142119909..0x142119938
  }
  // The corridor block (0x142119960): behind its guard, and only after both of
  // its queries answer, the state is 1 or 0; every other outcome leaves it
  // latched at 2 (0x142119ad9, 0x142119c94).
  if (state == 2 || !in.corridorGuard || !in.corridorAnswered) return 2;
  return in.corridorState1 ? 1 : 0;
}

bool ImagePlanCancelled(int planId, int intention, bool stillEligible) {
  // Breakthrough's own cancel test (0x14211a550): the Intention below 3 stops
  // it, `cmp eax, 3; setl`.
  if (planId == kPlanBreakthrough && intention < 3) return true;
  // ponytail: a rolled plan's own cancel test (vtable[3]) is not decoded; the
  // stand-in is PlanStillHolds, which asks whether the plan's start still holds.
  return !stillEligible;
}

bool PlanStillHolds(int planId, bool choiceEligible, const CurveShotInput& curve,
                    const LongInput& longPlan, const CarrierConstants& k) {
  switch (planId) {
    case kPlanCurveShot:
      return CurveShotScore(curve, k) >= 0.0f;
    case kPlanLong:
      return !LongVetoed(longPlan);
    default:
      return choiceEligible;
  }
}

int PlanImageMode(int planId, int previousMode, bool receiving, float carrierX_m,
                  const CarrierConstants& k) {
  switch (planId) {
    case kPlanCross:
      return 4;  // 0x140e8cff0, always the cross mode
    case kPlanBreakthrough:
    case kPlanMiddleShot:
    case kPlanShotPassFromGoal:
      return 1;  // 0x140a4c0f0
    case kPlanCurveShot:
      // The sticky mode (0x14211e0f0): the ball coming at him, or a position
      // past half of the half line, pins 1; otherwise it keeps what it had.
      if (receiving) return 1;
      if (carrierX_m > k.imageHalfLine * pesFieldHalfLength) return 1;
      return previousMode;
    case kPlanBackDFLine:
      return 2;  // 0x141f05c10
    default:
      // ponytail: Long and SideChange answer 8 or 2 from their unit's own
      // pending-ball count (0x14211e140), PassThrough 1 or 2 (0x14211e160) and
      // Basic 1, 2 or 3 with its Intention-4 override (0x14211dc00); GF carries
      // none of those fields, so the manager's own mode stands in.
      return previousMode;
  }
}

namespace {

// PES's list order (report 3): the Basic list, OnePattern and GKCoaching first, then Shoot and the
// outfield heads, then PassForward, then the plan-dependent Cross and Long. The other plans'
// tails are not ported.
constexpr int kListOrder[] = {23, 22, 0, 1, 17, 14, 11, 12, 2, 3, 10, 4, 7, 6};

}  // namespace

CarrierAnswer WalkCarrierUnits(const std::vector<ListedAnswer>& listed,
                               const CarrierAnswer& dribbleDefault, const WalkVetoes& vetoes) {
  for (const int kind : kListOrder) {
    for (const ListedAnswer& l : listed) {
      if (l.kind != kind || l.answer.type == kAnswerNone) continue;
      if (vetoes.restartPasses && l.answer.type == kAnswerPass) continue;
      return l.answer;
    }
  }
  return dribbleDefault;
}

}  // namespace pes
