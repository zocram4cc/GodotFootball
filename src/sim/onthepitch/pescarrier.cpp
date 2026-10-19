#include "pescarrier.hpp"

#include "onthepitch/pesball.hpp"

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

// 0x140439EF0: bearing(a -> b) in [0, 360), atan2(dx, dz), a negative result plus 2*pi.
// ponytail: std::atan2 stands in for PES's atan2 polynomial 0x140A92DC0, undecoded (lane-core O3).
inline float BearingDeg(CarrierPoint a, CarrierPoint b) {
  if (a.x == b.x && a.z == b.z) return 0.0f;
  float rad = std::atan2(b.x - a.x, b.z - a.z);
  if (rad < 0.0f) rad += 2.0f * kPi_deg;
  return rad * (180.0f / kPi_deg);
}

// 0x140A8F7C0: the point r metres from p along theta (dir = (sin, cos) over (x, z)).
// ponytail: std::sin/cos stand in for PES's sine table 0x140A92C80; the lane core's Dir uses the
// table, but the units' advance calls below never run through it.
inline CarrierPoint AdvancePoint(CarrierPoint p, float theta_deg, float r_m) {
  const float rad = theta_deg * (kPi_deg / 180.0f);
  return {p.x + r_m * std::sin(rad), p.z + r_m * std::cos(rad)};
}

// PES's frame counts: int(fps * gain + 0.5) with fps = 54 (0x141E532D0/E0).
inline int PesFrames(float gain) {
  return static_cast<int>(kPesTickHz * gain + 0.5f);
}

// P + frames * V, the predicted point every unit builds from the record's per-frame velocity.
inline CarrierPoint PredictPoint(CarrierPoint p, CarrierPoint vPerFrame, int frames) {
  const float h = static_cast<float>(frames);
  return {p.x + h * vPerFrame.x, p.z + h * vPerFrame.z};
}

inline float Dist2(CarrierPoint a, CarrierPoint b) {
  const float dx = a.x - b.x;
  const float dz = a.z - b.z;
  return dx * dx + dz * dz;
}

inline offball::Vec2f ToLaneVec(CarrierPoint p) { return {p.x, p.z}; }

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
  // The Dribble chooser's gates (0x14212f1c0): the 30, 60, 40, 20, 10, 15 and 12 immediates' floats.
  const std::pair<uint64_t, float*> chooser[] = {
      {0x142591140, &k.chooserGate_deg},      {0x14259a764, &k.chooserTurn_deg},
      {0x14259be2c, &k.chooserSideAngle_deg}, {0x14259be1c, &k.chooserPressureScale_deg},
      {0x142588210, &k.chooserSpeedLow_kmh},  {0x14259be0c, &k.chooserSpeedMid_kmh},
      {0x14259be04, &k.chooserRoom_m},
  };
  for (const auto& r : chooser) ok &= ReadVa(exe, r.first, r.second, error);

  // The lane core the unit gates call (0x14063CD90 with its packer 0x14063DE20 and the test
  // 0x14063DC10), loaded into this bundle so a unit gets it from the one constants object the match
  // carries.
  if (std::optional<lane::LaneConstants> l = lane::LoadLaneConstants(exe, error); l.has_value()) {
    k.lane = *l;
  } else {
    ok = false;
  }
  // The GoalFront unit: its lane request (0x14212ABA0) and its picker's 0x1421265C0 load sites.
  const std::pair<uint64_t, float*> goalFront[] = {
      {0x14259be1c, &k.unitLaneLen_m},    {0x142596e6c, &k.unitLaneCone_deg},
      {0x14256ae1c, &k.unitLaneRad_m},    {0x14256ae1c, &k.gfDifficultyMax},
      {0x142588210, &k.gfDistMin_m},      {0x142591140, &k.gfDistMax_m},
      {0x14259222c, &k.gfDistMinBand_m},  {0x14259be0c, &k.gfClass2Dist_m},
      {0x14259be5c, &k.gfAngle_deg},      {0x142bf07e0, &k.gfStatAngle_deg},
      {0x14259be64, &k.gfFlagAngle_deg},  {0x14259be20, &k.gfFlagDistSq_m2},
      {0x142565600, &k.gfPredictGain},    {0x14259be0c, &k.gfZoneDist_m},
  };
  for (const auto& r : goalFront) ok &= ReadVa(exe, r.first, r.second, error);
  // The picker's two byte immediates: `cmp byte ptr [rsp+0x34],0x46` (the stat floor) and
  // `add bpl,0x14` (the bonus 0x14063E960 pays on it).
  uint8_t gateByte = 0;
  ok &= ReadImm(exe, Imm{0x1421267ff, {0x80, 0x7c, 0x24, 0x34}, 4, 4}, &gateByte, error);
  k.gfStatFloor = static_cast<float>(gateByte);
  ok &= ReadImm(exe, Imm{0x1421266ad, {0x40, 0x80, 0xc5}, 3, 3}, &gateByte, error);
  k.gfStatBonus = static_cast<float>(gateByte);
  // The BackSpace unit: its gate 0x142129110, validator 0x1421226C0 and cut-back picker
  // 0x142125140 load sites. The three lane-1 immediates are `mov [rbp+disp8],imm32` stores
  // (opcode C7 45), the disp byte naming the lane slot.
  const std::pair<uint64_t, float*> backSpace[] = {
      {0x1425a9464, &k.bsOppDistSq_m2},   {0x14256ae1c, &k.bsBallHeight_m},
      {0x14259be70, &k.bsCornerDistSq_m2}, {0x142590d98, &k.bsPredictGain},
      {0x14259222c, &k.bsBoxSkipMargin_m}, {0x14259bdec, &k.bsAngleLimitGap_m},
      {0x14259bdec, &k.bsAlignDist_m},     {0x14259bdf4, &k.bsAlignNeed_m},
      {0x14259be30, &k.bsAlignAngle_deg},  {0x14259be1c, &k.bsNearBox_m},
      {0x14259be2c, &k.bsNearSum_m},       {0x14259be1c, &k.bsFarDist_m},
      {0x14259be5c, &k.bsFacingLimit_deg}, {0x14256ae30, &k.bsHumanFacingLimit_deg},
      {0x14259be64, &k.bsLane2Cone_deg},   {0x14259be1c, &k.bsLane2Cap_deg},
      {0x1425a946c, &k.bsLane2Floor_deg},  {0x142565604, &k.bsLane2Gain},
      {0x14258c264, &k.bsLane2XNear_m},    {0x14259be0c, &k.bsLane2XFar_m},
      {0x14259be04, &k.bsLane2XSpan_m},    {0x1425656c0, &k.bsLane2ZGain},
      {0x14259bdec, &k.bsLane2Base_m},     {0x142565604, &k.bsLane2Pad_m},
      {0x14258c264, &k.bsLane2LenMin_m},   {0x14253ee30, &k.bsTailDistSq_m2},
      {0x142565600, &k.bsTailHalf},        {0x14259be5c, &k.bsTailFacing_deg},
      {0x142591140, &k.bsTailHeading_deg}, {0x14256ae1c, &k.bsTailPad_m},
      {0x14259be0c, &k.cbDist_m},          {0x142588210, &k.cbSpeedBranch_kmh},
      {0x14259be4c, &k.cbAlign_deg},       {0x1425c48b8, &k.cbFastAlign_kmh},
      {0x14258c264, &k.cbCrowd_m},         {0x14253ee30, &k.cbBoxAngle_deg},
      {0x14259222c, &k.cbSpeedGate_kmh},   {0x14259be1c, &k.cbAdvance_m},
  };
  for (const auto& r : backSpace) ok &= ReadVa(exe, r.first, r.second, error);
  const std::pair<uint64_t, float*> s1Rows[] = {{0x14259be40, &k.s1RadToDeg}, {0x1425795e4, &k.s1TwoPi}};
  for (const auto& r : s1Rows) ok &= ReadVa(exe, r.first, r.second, error);
  k.bsBoxSkipCount = 2.0f;  // `cmp [lVar15+0xb48],2`: an instruction shape, not a float
  k.bsAngleClass = 4.0f;    // `cmp [recB],4`: an instruction shape, not a float
  uint32_t lane1 = 0;
  ok &= ReadImm(exe, Imm{0x142122cc1, {0xc7, 0x45, 0xd0}, 3, 3}, &lane1, error);
  std::memcpy(&k.bsLane1Len_m, &lane1, sizeof lane1);
  ok &= ReadImm(exe, Imm{0x142122cc8, {0xc7, 0x45, 0xd4}, 3, 3}, &lane1, error);
  std::memcpy(&k.bsLane1Cone_deg, &lane1, sizeof lane1);
  ok &= ReadImm(exe, Imm{0x142122cd8, {0xc7, 0x45, 0xd8}, 3, 3}, &lane1, error);
  std::memcpy(&k.bsLane1Rad_m, &lane1, sizeof lane1);
  // The Safety unit: its gate 0x14212B520, short picker 0x142126B70 and through picker
  // 0x142128490 load sites.
  const std::pair<uint64_t, float*> safety[] = {
      {0x14259be60, &k.sfFacingTurn_deg}, {0x14258c264, &k.sfLaneRad_m},
      {0x14259be0c, &k.sfKeeperDist_m},   {0x14259be2c, &k.sfKeeperFar_m},
      {0x14259bddc, &k.sfKeeperScalar},   {0x14259a764, &k.sfKeeperSpeedCap},
      {0x14259be70, &k.sfOppD2Min},       {0x142588210, &k.sfScoreDist_m},
      {0x14259d628, &k.sfTurnOver_deg},   {0x142596e6c, &k.sfTurnUnder_deg},
      {0x14259be1c, &k.sfClass4Bonus},    {0x14256ae30, &k.sfNeedDanger},
      {0x14259be60, &k.sfNeedCap},        {0x14259be30, &k.sfNeedFloor},
      {0x14259222c, &k.sfSpeedFloor_kmh}, {0x14259be20, &k.sfSpeedCeil_kmh},
      {0x1425a946c, &k.sfSpeedSpan_kmh},  {0x142591140, &k.sfDistCeil_m},
      {0x14259a764, &k.sfNeedBase},       {0x142591140, &k.sfNeedGain},
      {0x142591140, &k.sfClass5Bonus},    {0x14259be5c, &k.sfTurnAttack_deg},
      {0x14259be30, &k.sfTurnFace_deg},   {0x14259be2c, &k.sfCeilDefault_m},
      {0x14259be0c, &k.sfCeilNear_m},     {0x14259a764, &k.sfCeilFar_m},
      {0x14253ee30, &k.sfCeilOpp_m},      {0x14259222c, &k.sfMateNear_m},
      {0x14259be1c, &k.sfMateSpeed_kmh},  {0x142596e6c, &k.sfFrameSpeed},
      {0x14259a760, &k.sfOppGate_m},      {0x14259b89c, &k.sfLimGain},
      {0x142588210, &k.sfLimNear_m},      {0x14259be1c, &k.sfLimFar_m},
      {0x14258c264, &k.sfLimMin_m},       {0x142565604, &k.sfOppPad_m},
      {0x14259222c, &k.sfKeepPad_m},      {0x14259be34, &k.sfOuterTurn_deg},
      {0x14259be0c, &k.sfPressDist_m},    {0x14259222c, &k.sfLaneDivisor},
      {0x1425656c0, &k.sfLaneLenMin_m},   {0x14259be04, &k.sfLaneLenMax_m},
      {0x1425ca310, &k.sfLaneCone_deg},   {0x1425a9bf0, &k.sfTieDiff_deg},
      {0x14259d60c, &k.sfTiePress},       {0x14259ce40, &k.sfTieCornerSq},
      {0x14259222c, &k.sfTieGap_m},       {0x14258c264, &k.sfThruIdle},
      {0x14259d60c, &k.sfThruPress},      {0x14259ce40, &k.sfThruCarrierSq},
      {0x1425c65f4, &k.sfThruRowSq},      {0x14259be4c, &k.sfThruAlign_deg},
      {0x14259be60, &k.sfThruAim_deg},    {0x14259a764, &k.sfThruFacing_deg},
      {0x14253ee30, &k.sfThruNearSq},     {0x1425c65f4, &k.sfThruFarSq},
      {0x142588210, &k.sfThruLaneLen_m},  {0x14259a764, &k.sfThruLaneCone_deg},
      {0x14259222c, &k.sfThruLaneRad_m},
      {0x14259be5c, &k.sfKeeperCone_deg}, {0x14256ae1c, &k.sfKeeperRad_m},
      {0x14258c264, &k.sfPadBack_m},
  };
  for (const auto& r : safety) ok &= ReadVa(exe, r.first, r.second, error);
  // The OneTwo and DirectHighBall units and the one-touch and receiver predicates they share: each VA
  // is the one lane-units2 §0 audits, read from the install.
  const std::pair<uint64_t, float*> touch[] = {
      {0x142590d98, &k.oneTwoFrameGain},          {0x142598580, &k.oneTwoClosingHalf_deg},
      {0x14259bdec, &k.oneTwoOuter_m},            {0x14259be5c, &k.oneTwoAngle_deg},
      {0x142565604, &k.touchPad_m},               {0x14259ce40, &k.touchScore_m2},
      {0x14259be0c, &k.touchSpeed_kmh},           {0x14253ee30, &k.touchTurn_deg},
      {0x14259be84, &k.touchBand_m},              {0x14259be0c, &k.touchLaneLen_m},
      {0x14259a764, &k.touchLaneCone_deg},        {0x142565604, &k.touchLaneRad_m},
      {0x142565604, &k.rcvDifficultyNear},        {0x14256ae1c, &k.rcvDifficultyFar},
      {0x14259be0c, &k.rcvDistFar_m},             {0x14259d628, &k.rcvTurnFar_deg},
      {0x14259be0c, &k.rcvSpeedGate_kmh},         {0x142596e6c, &k.rcvAngleFast_deg},
      {0x14256ae30, &k.rcvAngleSlow_deg},         {0x14256ae30, &k.rcvBaseSplit_deg},
      {0x142588210, &k.rcvBaseNear_m},            {0x14259222c, &k.rcvBaseFar_m},
      {0x14259a764, &k.rcvRampHalf_deg},          {0x1425a9bf0, &k.rcvRampTop_deg},
      {0x1425c4900, &k.rcvRampSpan},              {0x142588210, &k.rcvRampGain_m},
      {0x14258c264, &k.rcvWindowFloor_m},         {0x14259be1c, &k.rcvM2_deg},
      {0x1425a9464, &k.rcvNearSq_m2},             {0x142588200, &k.dhbHeightPad_m},
      {0x14259be20, &k.dhbNearOpp_m2},            {0x14253ee30, &k.dhbMateNear_m2},
      {0x14259d60c, &k.dhbPredNear_m2},           {0x14259be70, &k.dhbScoreFloor},
      {0x1425c65f4, &k.dhbScoreCeil},             {0x142565600, &k.dhbFarGain},
      {0x14259be0c, &k.dhbSpeedGate_kmh},         {0x14259be0c, &k.dhbLaneLen_m},
      {0x14259a764, &k.dhbLaneCone_deg},          {0x14259222c, &k.dhbLaneRad_m},
      {0x1425656c0, &k.dhbAdvance_m},             {0x142596e6c, &k.dhbFacing_deg},
  };
  for (const auto& r : touch) ok &= ReadVa(exe, r.first, r.second, error);
  // The curve builder's constants (0x142138220) and the G12 band's curve reader (0x142139650).
  const std::pair<uint64_t, float*> curve[] = {
      {0x142598580, &k.curveHalfTurn_deg}, {0x14259be34, &k.curveArmAngle_deg}, {0x14256ae34, &k.curveArm_m},
      {0x14253ee30, &k.curveWide_deg},     {0x14256ae1c, &k.curveStep_m},       {0x14259bdc4, &k.curveEps},
  };
  for (const auto& r : curve) ok &= ReadVa(exe, r.first, r.second, error);
  // G3's team level (reader 0x140A40BD0, comparator 0x140A60360). The writer 0x1B5D8340 stores
  // clamp(class x 100, 0, 700), where the class is `*(int*)(matchSetting+0x18)` through its 7-entry
  // switch, whose entries are all the same arm, so an out-of-range selector takes the masked global.
  // GF has no match-setting selector, so the stock default class and the scale/cap come from the
  // install's own bytes (no transcription).
  {
    uint32_t maskedClass = 0, xorKey = 0, cap = 0;
    uint8_t scale = 0;
    ok &= exe.ReadVa(0x14810F4E0, sizeof maskedClass, reinterpret_cast<uint8_t*>(&maskedClass));
    ok &= ReadImm(exe, Imm{0x15B5D8378, {0x35}, 1, 1}, &xorKey, error);
    ok &= ReadImm(exe, Imm{0x140A602B0, {0x6b, 0xc2}, 2, 2}, &scale, error);
    ok &= ReadImm(exe, Imm{0x140A602B5, {0x3d}, 1, 1}, &cap, error);
    const uint32_t cls = (maskedClass ^ xorKey) & 0xff;
    const int32_t level = static_cast<int32_t>(cls) * static_cast<int32_t>(scale);
    k.teamLevelRaw = level < 0 ? 0 : (static_cast<uint32_t>(level) > cap ? static_cast<int>(cap) : level);
  }
  k.sfThruClass = 3.0f;  // `cmp [recB],2; jbe`: class above 2, an instruction shape
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
    // Case B (pf[2] >= |K.z|): `+0x170 = |pf[0] - sign*K.x|`. The 0x142565D30 absolute-value mask
    // (`andps` at 0x140698EE1) is real: past the goal line the signed form would go negative.
    z.d_m = std::fabs(halfLen_m - attackSign * ballX);
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

float AttackAngleDeg(float attackSign) {
  // 0x14045DF70 writes MA+0x228D4 = bearing(-halfLen, 0) = 270 and MA+0x228D8 = bearing(+halfLen, 0) = 90
  // (atan2(dx, dz) x 180/pi, 0x140A92D40 x 0x14259BE40); 0x140A71520 picks by the sign byte
  // `T+0x254`, and 0x1404FEBD0 stores it at T+0x250. `attackSign` is that sign, so this is its bearing.
  return attackSign < 0.0f ? 270.0f : 90.0f;
}

float AimVsFacingDeg(float attackSign, float facingX, float facingZ) {
  const float facing = BearingDeg({0.0f, 0.0f}, {facingX, facingZ});
  return std::fabs(WrapDeg(AttackAngleDeg(attackSign) - facing));
}
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
      // Base 50 with the Crosser style, the Early Cross COM or Pinpoint
      // Crossing (0x14211AB10; the style arrives folded into `skill`).
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

// PES's list order is the table order the list builder writes (0x1421112c0); the walk reads it.
namespace {

constexpr int kAnswerClear = 5;    // type 5: the clear (0x142109eb0)
constexpr int kAnswerRequest = 7;  // type 7: the pass to a requester (RespondRequest)

const UnitSlot* FindSlot(const std::vector<UnitSlot>& table, int kind) {
  for (const UnitSlot& s : table) {
    if (s.kind == kind) return &s;
  }
  return nullptr;
}

// Rule 3, the first-time gate (report 5): off unless the tier gate is on.
bool FirstTimeAccept(const UnitSlot& s, const WalkContext& c) {
  if (!c.firstTimeGate) return true;
  const int t = s.answer.type;
  return (c.ballHeightOk || ((t != kAnswerShoot || c.skill11) && t != kAnswerClear)) &&
         (t != kAnswerPass || c.skill12);
}

// Rule 5, the restart vetoes: MI+0x1775 in {3, 9, 10} skips passes and clears, and the stopped mode
// (MI+0x116C == 2) accepts only passes.
bool RestartAccept(const UnitSlot& s, const WalkContext& c) {
  const int t = s.answer.type;
  const bool vetoState = c.restartState == 3 || c.restartState == 9 || c.restartState == 10;
  if (vetoState && (t == kAnswerPass || t == kAnswerClear)) return false;
  return c.restartMode != 2 || t == kAnswerPass;
}

// The set-piece tail (0x14210f110 L141-157): a carrier who is not the designated kicker is free;
// the kicker takes shots and clears, and passes only where the pass gate allows.
bool SetPieceAccept(const UnitSlot& s, const WalkContext& c) {
  if (!c.carrierIsKicker) return true;
  const int t = s.answer.type;
  if (t == kAnswerShoot || t == kAnswerClear) return true;
  return (t == kAnswerPass || t == kAnswerRequest) && c.passAllowedInSetPiece;
}

// Rule 10, the final ownership check (0x14210F110 L641-645): the ball's owner unknown, a teammate,
// or the answer is a dribble.
bool OwnerAccept(const UnitSlot& s, const WalkContext& c) {
  if (c.ballOwnerTeam < 0 || c.ballOwnerTeam == c.ownTeam) return true;
  return s.answer.type == kAnswerDribble;
}

// Rule 7's Safety proviso: a back-pass to the keeper stands when owned or the
// keeper is within 90 degrees of the body direction.
bool SafetyAccept(const WalkContext& c) {
  return !c.safetyToKeeper || c.owned || c.keeperWithinNinety;
}

// Rule 8's pass-kind vetoes (carunits5.c:393,421): Safety dies in the close
// band, Forward dies on an instant return to the last passer, and a dribble
// answer survives only from the Dribble kind itself.
bool PassVeto(const UnitSlot& s, const WalkContext& c) {
  if (s.answer.type != kAnswerPass && s.answer.type != kAnswerDribble) return false;
  if (s.kind == 6 && s.answer.type == kAnswerPass && c.closeBand) return true;
  if (s.kind == 4 && s.answer.type == kAnswerPass && c.forwardIsInstantReturn) return true;
  if (s.answer.type == kAnswerDribble && s.kind != 16) return true;
  return false;
}

}  // namespace

UnitSlot WalkCarrierUnits(const std::vector<UnitSlot>& table, const WalkContext& c) {
  for (const UnitSlot& s : table) {
    const int t = s.answer.type;
    if (t == kAnswerNone) continue;
    if (c.restartMode != 3) {  // the open-play body (set pieces skip it, L122)
      if (s.kind == 22) return s;                                    // rule 1
      if (!FirstTimeAccept(s, c) || !RestartAccept(s, c)) continue;  // rules 3 and 5
      if (t == kAnswerShoot && !c.closeBand) {                       // rule 6
        const UnitSlot* back = FindSlot(table, 3);
        if (back != nullptr && back->answer.type != kAnswerNone) return *back;
      }
      if (t == kAnswerClear && !c.forcedClear) {  // rule 7: the first listed
        for (const int kind : {3, 4, 9, 6}) {     // 3, 4, 9, 6 that answers
          const UnitSlot* pass = FindSlot(table, kind);  // replaces the clear
          if (pass == nullptr || pass->answer.type == kAnswerNone) continue;
          if (kind == 6 && !SafetyAccept(c)) continue;
          return *pass;
        }
      }
      if (PassVeto(s, c)) continue;  // rule 8
      // Rule 9: a cross override for a pass or dribble from a kind other than 7, 8 and 23.
      if ((t == kAnswerPass || t == kAnswerDribble) && c.skill14 && s.kind != 7 && s.kind != 8 &&
          s.kind != 23) {
        const UnitSlot* cross = FindSlot(table, 7);
        if (cross != nullptr && cross->answer.type != kAnswerNone) return *cross;
      }
    }
    const bool accepted = c.restartMode == 1 ? OwnerAccept(s, c) : SetPieceAccept(s, c);
    if (accepted) return s;
  }
  return UnitSlot{};
}

PassAnswer PassLongUnit(const LongUnitInput& in, const CarrierConstants& k) {
  (void)k;
  // The head gate: owned, or the danger flag with the ball below 1.5 m.
  if (!in.owned && !(in.danger && in.ballHeight_m < 1.5f)) return PassAnswer{};
  int target = -1;
  if (in.planTarget >= 0 && (in.plan == kPlanLong || in.plan == kPlanSideChange) &&
      in.planTargetValid) {
    target = in.planTarget;  // 0x1421240B0 keeps the plan target
  } else {
    // The picker 0x142128A20: the most advanced valid mate; within 10 m of
    // the best, taller by more than 5 cm takes it.
    float bestAdvance = -1.0e30f;
    float bestHeight = 0.0f;
    for (const LongMate& m : in.mates) {
      if (m.index < 0) continue;
      if (target < 0) {
        target = m.index;
        bestAdvance = m.advance_m;
        bestHeight = m.height_cm;
        continue;
      }
      // Raw (0x142128A20): lead = s*(new-best); past 10 m ahead the new mate
      // takes it; inside (-10, 10) the taller one does when over 5 cm up.
      const float lead = m.advance_m - bestAdvance;
      if (lead > 10.0f || ((lead > -10.0f && lead < 10.0f) && m.height_cm - bestHeight > 5.0f)) {
        target = m.index;
        bestAdvance = m.advance_m;
        bestHeight = m.height_cm;
      }
    }
  }
  if (target < 0) return PassAnswer{};
  float openness = 0.0f;
  for (const LongMate& m : in.mates) {
    if (m.index == target) openness = m.opennessClass;
  }
  // The tail: short at class 4 (3 with 0x14063A3A0), else long with f14.
  PassAnswer answer;
  answer.index = target;
  answer.kind = openness >= (in.favourShort ? 3.0f : 4.0f) ? kPassShort : kPassLong;
  return answer;
}

PassAnswer CrossUnit(const CrossInput& in, const CarrierConstants& k) {
  (void)k;
  // The carry gate: within 130 degrees of the facing.
  if (std::fabs(WrapDeg(in.carryVsFacing_deg)) > 130.0f) return PassAnswer{};
  if (!in.laneOpen) return PassAnswer{};
  // Unowned: the predicted receiver's lead must be inside 16 m.
  if (!in.owned && in.receiverLead_m >= 16.0f) return PassAnswer{};
  // The picker 0x142125AB0: among valid mates the tallest, last max wins.
  int target = -1;
  float bestHeight = -1.0e30f;
  for (const LongMate& m : in.mates) {
    if (m.index < 0) continue;
    if (target < 0 || m.height_cm >= bestHeight) {
      target = m.index;
      bestHeight = m.height_cm;
    }
  }
  if (target < 0) return PassAnswer{};
  PassAnswer answer;
  answer.index = target;
  answer.kind = kPassCentering;
  return answer;
}

PassAnswer GoalFrontUnit(const GoalFrontInput& in, const CarrierConstants& k) {
  PassAnswer answer;
  // The gate (0x14212ABA0): only a carrier outside the central zone, with the lane about the
  // per-slot heading + 180 blocked, runs the picker at all.
  if (in.central) return answer;
  const lane::LaneRequest lane{{in.carrierPos.x, in.carrierPos.z}, in.laneHeading_deg,
                               k.unitLaneLen_m, k.unitLaneCone_deg, k.unitLaneRad_m};
  if (lane::LaneFree(k.lane, in.players, in.carrierSlot, lane, /*keepKeeper*/ false,
                     /*horizonFrames*/ 0)) {
    return answer;
  }
  // The picker (0x1421265C0). The stat floor is settled once, before the mate loop.
  const float stat = in.carrierStat + (in.statBonus ? k.gfStatBonus : 0.0f);
  int best = -1;
  float bestOpenness = 0.0f;
  for (const GoalFrontMate& m : in.mates) {
    if (m.index < 0 || !m.statusValid) continue;
    if (!(m.difficulty <= k.gfDifficultyMax)) continue;
    const float d = m.distance_m;
    // The angle gate holds only from 10 m out; the distance band below is its own, separate gate,
    // so a mate inside (5, 10) is never measured against the angle at all.
    if (k.gfDistMin_m <= d) {
      float limit = k.gfAngle_deg;
      if (!in.humanCarrier) {
        if (stat > k.gfStatFloor) limit = k.gfStatAngle_deg;
      } else if (in.flagSet && in.flagDistFar) {
        limit = k.gfFlagAngle_deg;
      }
      if (limit <= m.angleToFacing_deg) continue;
    }
    if (!(k.gfDistMinBand_m < d && d < k.gfDistMax_m)) continue;
    if (!(m.opennessClass < 0 ||
          (m.opennessClass > 1 && (m.opennessClass != 2 || d < k.gfClass2Dist_m)))) {
      continue;
    }
    if (!(IsCentral(m.zone) && m.zone.d_m < k.gfZoneDist_m && m.pressure != 0.0f)) continue;
    // The first candidate is taken; only a strictly larger openness replaces it.
    if (best < 0 || (bestOpenness <= m.openness && m.openness != bestOpenness)) {
      best = m.index;
      bestOpenness = m.openness;
    }
  }
  if (best < 0) return answer;
  answer.index = best;
  answer.kind = kPassShort;
  return answer;
}

namespace {

// 0x140A4EE40/60: the nearest slot of side `side` to `point` (rank 0), kNoSlot when none is
// active. PES scans the side's eleven in order and keeps the strictly nearer; GF's 22-slot array
// is in the same layout.
int NearestSideSlot(const std::array<lane::LanePlayer, lane::kSlots>& players, int side,
                     CarrierPoint point) {
  int best = lane::kNoSlot;
  float bestD2 = 0.0f;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int q = lane::kSideSize * side + j;
    if (!players[q].active) continue;
    const float dx = players[q].pos.x - point.x;
    const float dz = players[q].pos.z - point.z;
    const float d2 = dx * dx + dz * dz;
    if (best == lane::kNoSlot || d2 < bestD2) {
      best = q;
      bestD2 = d2;
    }
  }
  return best;
}

// 0x140A4F720: the crowd count behind the cut-back picker - opponents (side `side`) whose
// 13-frame predicted position is within `radius_m` of `point`. Pure from the records.
int CrowdCount(const std::array<lane::LanePlayer, lane::kSlots>& players, int side,
               CarrierPoint point, float radius_m) {
  const int horizon = static_cast<int>(kPesTickHz) / 4;  // fps >> 2 = 13
  int count = 0;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int q = lane::kSideSize * side + j;
    if (!players[q].active) continue;
    const CarrierPoint pred = PredictPoint({players[q].pos.x, players[q].pos.z},
                                           {players[q].vel.x, players[q].vel.z}, horizon);
    if (Dist2(pred, point) < radius_m * radius_m) ++count;
  }
  return count;
}

// 0x140A4FBC0: the annulus count behind the keeper lane (0x140A4FEA0). Players of `side` whose
// current point sits between rIn and rIn + len of `point`, and whose bearing from it is within half
// of `cone_deg` of `facing_deg`; `keepKeeper` decides whether the side's keeper is counted (Safety
// passes true). The decode reads no horizon for this call, so the current positions are used (INF).
int AnnulusCount(const std::array<lane::LanePlayer, lane::kSlots>& players, int side,
                 CarrierPoint point, float facing_deg, float cone_deg, float rIn_m, float len_m,
                 bool keepKeeper) {
  const int keeper = lane::KeeperSlot(side, players);
  const float rOut_m = rIn_m + len_m;
  int count = 0;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int q = lane::kSideSize * side + j;
    if (!players[q].active || (!keepKeeper && q == keeper)) continue;
    const CarrierPoint pos{players[q].pos.x, players[q].pos.z};
    const float d2 = Dist2(pos, point);
    if (rIn_m * rIn_m < d2 && d2 < rOut_m * rOut_m &&
        std::fabs(WrapDeg(BearingDeg(point, pos) - facing_deg)) < cone_deg * 0.5f)
      ++count;
  }
  return count;
}

// The PassThrough validator 0x1421226C0: the target mate's record-B target must survive the side,
// difficulty, slot-stat, mode-1 zone, distance, align, near-box, facing and two lane gates.
// `mateIdx` is the mate's index into in.mates; `nearestRank0` answers the tail loop's
// NearestEntry rank-0 read (T18+0x22E4C+q*200+side*0x58 == target).
bool PassThroughValidator(const BackSpaceInput& in, size_t mateIdx, const CarrierConstants& k,
                          const std::function<int(int q)>& nearestRank0) {
  const BackSpaceMate& m = in.mates[mateIdx];
  if (m.index < 0 || m.index > 21) return false;
  // Gate 1: b6c status 0xB.
  if (!m.statusValid) return false;
  // Gate 2: k*mate.x <= k*teamLine.
  if (in.attackSign * m.pos.x > in.attackSign * in.teamLineOpp) return false;
  // Gate 3: k*recB.x <= F[8], and (danger or difficulty <= 1).
  if (in.attackSign * m.recBTarget.x > in.pitchBound8) return false;
  if (!in.danger && !(m.recBDifficulty <= k.bsBallHeight_m)) return false;
  // Gate 4: the carrier->recB-target bearing, kept for gate 10. (The 0x140A8F890 corner call's
  // result is discarded - a dead call.)
  const float carrierBearing = BearingDeg(in.carrierPos, m.recBTarget);
  // Gate 5: the carrier's per-slot float at or below the target's.
  if (in.carrierSlotStat > m.slotStat) return false;
  // Gate 6: the mode-1 zone arm.
  if (in.planMode == 1) {
    const bool skip = in.boxCount >= k.bsBoxSkipCount &&
                      in.attackSign * in.ball.x >= in.pitchF6 - k.bsBoxSkipMargin_m;
    if (!skip) {
      // The zone struct 0x140698D10 builds at the mate's 16-frame prediction; GF's ShootZone
      // carries its sector, margin and angle in the same fields the gates read (+0x178/+0x170/
      // +0x174).
      if (in.boxSector < static_cast<float>(m.zoneAtPred16.sector)) return false;
      if (in.goalMouth) return false;
      if (static_cast<float>(m.zoneAtPred16.sector) == in.boxSector) {
        const float gap = m.zoneAtPred16.d_m - in.boxMargin;
        if (gap > 0.0f) return false;  // mode is 1 here, so a positive gap rejects
        if (k.bsAngleLimitGap_m <= gap && in.boxAngle <= m.zoneAtPred16.angle_deg &&
            m.zoneAtPred16.angle_deg != in.boxAngle)
          return false;
      }
    }
  }
  // Gate 7/8: past 6 m, a need under 8 m with headings agreeing within 45 degrees rejects.
  const float d = std::sqrt(Dist2(in.ball, m.recBTarget));
  if (k.bsAlignDist_m <= d && m.teamNeed_m < k.bsAlignNeed_m &&
      std::fabs(WrapDeg(m.heading_deg - in.carrierHeading_deg)) < k.bsAlignAngle_deg)
    return false;
  // Gate 9: the near box (|dx| <= 20, |dx|+|dz| <= 40) and the difficulty re-read.
  const float dx = std::fabs(in.ball.x - m.pos.x);
  const float dz = std::fabs(in.ball.z - m.pos.z);
  if (!(dx <= k.bsNearBox_m && dx + dz <= k.bsNearSum_m)) return false;
  if (!(m.recBDifficulty <= k.bsBallHeight_m)) return false;
  // Gate 10: past 20 m, or a record-B class below 4, the facing gate holds: the carrier's facing
  // against the gate-4 bearing, 90 degrees (120 for a human carrier inside 20 m).
  if (k.bsFarDist_m <= d || static_cast<float>(m.recBClass) < k.bsAngleClass) {
    float limit = k.bsFacingLimit_deg;
    if (in.humanCarrier && d < k.bsFarDist_m) limit = k.bsHumanFacingLimit_deg;
    if (limit <= std::fabs(WrapDeg(in.carrierFacing_deg - carrierBearing))) return false;
  }
  // Gate 11: lane 1, the direct core call 0x14063CD90 - origin the ball advanced 1 m along the
  // record angle, len 5, cone 30, rad 2, h = the target slot (verbatim PES: `MOV [RSP+0x30],ESI`
  // at 0x142122D00 passes the slot where a frame count sits; likely a register mix-up, ported
  // as is). Any entry rejects.
  const lane::LaneRequest lane1{ToLaneVec(AdvancePoint(in.ball, m.recBAngle_deg, k.bsBallHeight_m)),
                                m.recBAngle_deg, k.bsLane1Len_m, k.bsLane1Cone_deg,
                                k.bsLane1Rad_m};
  const int oppSide = lane::IsAwaySlot(in.carrierSlot) ? 0 : 1;
  if (lane::ScanLane(k.lane, in.players, in.carrierSlot, oppSide, lane1, /*keepKeeper*/ false,
                     /*horizonFrames*/ m.index)
          .count != 0)
    return false;
  // Gate 12: lane 2, packer B 0x14063D2F0 - origin the mate's 16-frame prediction, theta the
  // side angle clamped, length from the carrier-mate separations, cone 140, h = 16. Empty
  // accepts; otherwise the tail loop runs.
  const int pred16 = PesFrames(k.bsPredictGain);
  const CarrierPoint matePred = PredictPoint(m.pos, m.velPerFrame, pred16);
  const float theta = Wrap360(
      in.sideAngle_deg +
      std::clamp(WrapDeg(BearingDeg(m.pos, m.recBTarget) - in.sideAngle_deg) * k.bsLane2Gain,
                 k.bsLane2Floor_deg, k.bsLane2Cap_deg));
  const float wx = std::fabs(in.carrierPos.x - m.pos.x);
  const float fx = wx < k.bsLane2XNear_m
                       ? 0.0f
                       : (k.bsLane2XFar_m < wx ? 1.0f : (wx - k.bsLane2XNear_m) / k.bsLane2XSpan_m);
  const float wz = std::fabs(in.carrierPos.z - m.pos.z);
  const float len = std::max(k.bsLane2LenMin_m,
                             std::min(wz, k.bsLane2ZGain * fx + k.bsLane2Base_m) + k.bsLane2Pad_m);
  const lane::LaneRequest lane2{ToLaneVec(matePred), theta, len, k.bsLane2Cone_deg, 0.0f};
  // The tail loop needs the entries, not just the count: re-scan per entry would repeat the
  // core, so collect the recorded slots once. The core records the same set for the same inputs.
  const int horizon9 = static_cast<int>(kPesTickHz) / 6;  // fps / 6 = 9
  const int horizon18 = static_cast<int>(kPesTickHz) / 3;  // fps / 3 = 18
  // ScanLane reports nearest/count only; walk the side and re-apply the record test per slot by
  // scanning each candidate alone is O(11) core calls - instead, probe membership via the count
  // delta is wrong; so run one scan per side slot (11 scans, same shape as PES's per-entry loop).
  std::vector<int> entries;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int q = lane::kSideSize * oppSide + j;
    if (!in.players[q].active || q == in.carrierSlot) continue;
    std::array<lane::LanePlayer, lane::kSlots> solo{};
    solo[q] = in.players[q];
    solo[in.carrierSlot] = in.players[in.carrierSlot];
    if (lane::ScanLane(k.lane, solo, in.carrierSlot, oppSide, lane2, /*keepKeeper*/ false,
                       /*horizonFrames*/ pred16)
            .count != 0)
      entries.push_back(q);
  }
  if (entries.empty()) return true;
  for (const int q : entries) {
    if (q > 21) continue;
    const lane::LanePlayer& qp = in.players[q];
    const CarrierPoint qPos{qp.pos.x, qp.pos.z};
    const CarrierPoint qVel{qp.vel.x, qp.vel.z};
    const CarrierPoint qPred = PredictPoint(qPos, qVel, horizon9);
    // Exit A: the predicted entry past 10 m from the ball and nearer in x than half the
    // carrier-mate gap.
    if (k.bsTailDistSq_m2 < Dist2(in.ball, qPred) &&
        std::fabs(m.pos.x - qPos.x) < std::fabs(in.carrierPos.x - m.pos.x) * k.bsTailHalf)
      return false;
    // Exit B: z-agreement, facing agreement (folded once past 180) and heading agreement.
    const float zM = std::fabs(in.ball.z - m.pos.z);
    const float zQ = std::fabs(in.ball.z - qPos.z);
    if (zQ <= zM + k.bsTailPad_m) {
      float facingDiff = 0.0f;  // the entry's facing is not in the lane record; see below
      (void)facingDiff;
      // ponytail: exit B's facing agreement reads the lane entry's record facing (+0x554),
      // which the caller's lane records do not carry; the z gate and the heading gate below
      // stand in. The PES writer of the facing is the player record itself (0x140A48A10).
      const float headQ = 0.0f;  // ditto for the entry's vel_eff heading
      if (std::fabs(WrapDeg(headQ - m.heading_deg)) < k.bsTailHeading_deg) {
        // The facing arm is skipped (stand-in above); the heading arm alone decides.
        return false;
      }
    }
    // Exit C: the entry's rank-0 nearest is the target and the 18-frame x race loses.
    if (nearestRank0(q) == m.index) {
      const float bit = (zM < zQ) ? k.bsTailPad_m : 0.0f;
      const float mx = (static_cast<float>(horizon18) * m.velPerFrame.x + m.pos.x);
      const float qx = (static_cast<float>(horizon18) * qVel.x + qPos.x);
      if (bit + in.attackSign * mx < in.attackSign * qx) return false;
    }
  }
  return true;
}

}  // namespace

// 0x1421225E0, the curve gate: the mate's own point is on or past the curve, k*x >= k*curve(z) (frames 0).
inline bool CurveGateOk(const BackSpaceInput& in, CarrierPoint pos) {
  return in.curve.has_value() && in.attackSign * pos.x >= in.attackSign * CurveEval(*in.curve, pos.z);
}

PassAnswer BackSpaceUnit(const BackSpaceInput& in, const CarrierConstants& k) {
  PassAnswer answer;
  // The lane branch (0x142129110): a Mid-zone carrier with a FREE gate lane checks the nearest
  // opponent's 9-frame prediction against the ball point - past 6 m, no answer.
  if (in.mid) {
    const lane::LaneRequest gate{{in.carrierPos.x, in.carrierPos.z}, in.sideAngle_deg,
                                 k.unitLaneLen_m, k.unitLaneCone_deg, k.unitLaneRad_m};
    if (lane::LaneFree(k.lane, in.players, in.carrierSlot, gate, /*keepKeeper*/ false,
                       /*horizonFrames*/ 0)) {
      const int oppSide = lane::IsAwaySlot(in.carrierSlot) ? 0 : 1;
      const int opp = NearestSideSlot(in.players, oppSide, in.ball);
      if (opp < lane::kSlots) {
        const lane::LanePlayer& op = in.players[opp];
        const CarrierPoint pred =
            PredictPoint({op.pos.x, op.pos.z}, {op.vel.x, op.vel.z},
                         static_cast<int>(kPesTickHz) / 6);
        if (k.bsOppDistSq_m2 < Dist2(pred, in.ball)) return answer;
      }
    }
  }
  // Block 2: owned, or the ball low.
  if (in.receiving) return answer;
  if (!in.owned && !(in.ballHeight_m <= k.bsBallHeight_m)) return answer;
  // Plan 4: the plan-record target through the validator and the corner/curve gate.
  if (in.plan == kPlanPassThrough) {
    if (in.planRecordTarget < 0 || in.planRecordTarget > 21) return answer;
    if (!in.planRecordBytesOk) return answer;
    size_t mateIdx = in.mates.size();
    for (size_t i = 0; i < in.mates.size(); ++i)
      if (in.mates[i].index == in.planRecordTarget) mateIdx = i;
    if (mateIdx >= in.mates.size()) return answer;
    const BackSpaceMate& m = in.mates[mateIdx];
    // The NearestList rank 0 of the side opposite the entry (row q of the carrier's lane snapshot).
    auto rank0 = [&](int q) { return in.nearestOpponent[q]; };
    if (!PassThroughValidator(in, mateIdx, k, rank0)) return answer;
    const CarrierPoint corner{in.attackSign == 1.0f ? in.pitchHalfLen_m : -in.pitchHalfLen_m, 0.0f};
    if (!(Dist2(m.recBTarget, corner) <= k.bsCornerDistSq_m2) && !CurveGateOk(in, m.pos)) return answer;
    answer.index = m.index;
    answer.kind = kPassThrough;
    return answer;
  }
  // Other plans: the cut-back picker 0x142125140.
  int best = -1;
  const BackSpaceMate* bestMate = nullptr;
  // The picker's head lane (0x142125311) and skill call (0x142125249) are both dead - results
  // discarded - so the port starts at the loop.
  for (const BackSpaceMate& m : in.mates) {
    if (m.index < 0 || m.index > 21 || m.index == in.carrierSlot) continue;
    auto rank0 = [&](int q) { return in.nearestOpponent[q]; };
    size_t mateIdx = 0;
    for (size_t i = 0; i < in.mates.size(); ++i)
      if (in.mates[i].index == m.index) mateIdx = i;
    if (!PassThroughValidator(in, mateIdx, k, rank0)) continue;
    const CarrierPoint corner{in.attackSign == 1.0f ? in.pitchHalfLen_m : -in.pitchHalfLen_m, 0.0f};
    if (!(Dist2(m.recBTarget, corner) <= k.bsCornerDistSq_m2) && !CurveGateOk(in, m.pos)) continue;
    const float dMate = std::sqrt(Dist2(in.carrierPos, m.pos));
    const float dTarg = std::sqrt(Dist2(in.carrierPos, m.recBTarget));
    const int horizon13 = static_cast<int>(kPesTickHz) >> 2;  // fps >> 2 = 13
    const CarrierPoint qPred13 = PredictPoint(m.pos, m.velPerFrame, horizon13);
    const int oppSide = lane::IsAwaySlot(in.carrierSlot) ? 0 : 1;
    const bool free13 = CrowdCount(in.players, oppSide, qPred13, k.cbCrowd_m) == 0;
    // Lane A: origin the 13-frame prediction, theta the side angle, 20/90/3, h = 13.
    const lane::LaneRequest laneA{ToLaneVec(qPred13), Wrap360(in.sideAngle_deg), k.unitLaneLen_m,
                                  k.bsFacingLimit_deg, k.cbCrowd_m};
    const bool laneAFree = lane::LaneFree(k.lane, in.players, m.index, laneA,
                                          /*keepKeeper*/ false, /*horizonFrames*/ horizon13);
    // Lane B: origin the 13-frame prediction, theta the team angle, len = need, 90/3, h = 13.
    const lane::LaneRequest laneB{ToLaneVec(qPred13), Wrap360(m.teamAngle_deg), m.teamNeed_m,
                                  k.bsFacingLimit_deg, k.cbCrowd_m};
    const bool laneBFree = lane::LaneFree(k.lane, in.players, m.index, laneB,
                                          /*keepKeeper*/ false, /*horizonFrames*/ horizon13);
    bool b = (m.action == 0x25 || m.action == 0x1E);
    // Gate 8: the distance gate, then the speed branch (spd > 10) arming the align arm.
    bool runSpeedBranch = false;
    if (!free13) {
      if (dTarg < k.cbDist_m) {
        runSpeedBranch = true;
      } else if (!(dMate < k.cbDist_m)) {
        continue;
      }
    } else if (dTarg < k.cbDist_m) {
      runSpeedBranch = true;
    }
    if (runSpeedBranch && k.cbSpeedBranch_kmh < m.speed_kmh) {
      if (std::fabs(WrapDeg(m.heading_deg - in.sideAngle_deg)) < k.cbAlign_deg) b = true;
    }
    // Gate 9: the fast arm and lane B.
    if (k.cbFastAlign_kmh < m.speed_kmh &&
        std::fabs(WrapDeg(m.heading_deg - in.sideAngle_deg)) < k.cbAlign_deg)
      b = true;
    if (laneBFree) b = true;
    // Gate 10: the box branch.
    if (!in.goalMouth && std::fabs(in.ball.z) <= std::fabs(m.recBTarget.z) &&
        (m.speed_kmh <= 0.0f ||
         std::fabs(WrapDeg(m.heading_deg - in.sideAngle_deg)) <= k.cbBoxAngle_deg) &&
        in.attackSign * m.recBTarget.x < in.pitchF6 && in.pitchF5 <= std::fabs(m.recBTarget.z) &&
        std::fabs(m.recBTarget.z) != in.pitchF5 && laneAFree)
      b = true;
    // Gate 11: the advance branch.
    if (in.carrierNeed_m <= m.teamNeed_m || in.central) {
      if (!b) continue;
    } else if (!m.zoneBuilt && (m.teamNeed_m < k.cbAdvance_m || b)) {
      if (!b) continue;
    } else if (!(!m.zoneBuilt || m.teamNeed_m < k.cbAdvance_m || b)) {
      continue;
    } else if (!b) {
      continue;
    }
    // Gate 12: the candidate check - first taken, then the zone-dependent tiebreak.
    if (best < 0) {
      best = m.index;
      bestMate = &m;
      continue;
    }
    if (in.central && in.mid) {
      if (m.lineValue < bestMate->lineValue) {
        best = m.index;
        bestMate = &m;
      }
    } else {
      // ponytail: the ctx[0]+0x40 flag has no GF state; GF takes the min-|z| (flag != 0 arm).
      // The flag's writer is undecoded (helpers §5).
      if (std::fabs(m.pos.z) < std::fabs(bestMate->pos.z)) {
        best = m.index;
        bestMate = &m;
      }
    }
  }
  if (best < 0) return answer;
  answer.index = best;
  answer.kind = kPassThrough;
  // The b-drop: close test, pressure class and record class all exceed 1.
  for (const BackSpaceMate& m : in.mates) {
    if (m.index == best && m.closeTest && 1.0f < m.pressureClass && 1.0f < m.recAClass) {
      answer.kind = kPassShort;
      break;
    }
  }
  return answer;
}

namespace {

// The opponents of a carrier slot are the other side: away carriers face the home side (0).
int OpponentSide(int carrierSlot) { return lane::IsAwaySlot(carrierSlot) ? 0 : 1; }

// dist / (speed * 1000 / (fps * 3600)), truncated: the frames a mate needs at his speed (0x1404C8DC0).
int ShortFrames(float dist_m, float speed_kmh) {
  return static_cast<int>(dist_m / (speed_kmh * 1000.0f / (kPesTickHz * 3600.0f)));
}

// The mate's scored limit (0x142126B70 L183-232): the keeper's is the floor; a field mate's is
// max(need, floor) while the need is within the cap, and the cap otherwise. The need weighs the
// carrier's speed (the 5/25 km/h ramp) and the mate's distance (the 10/30 m ramp) over the shared
// -20 span; danger fixes it at 120.
float ShortLimit(const SafetyInput& in, const SafetyMate& m, bool keeper, const CarrierConstants& k) {
  if (keeper) return k.sfNeedFloor;
  float wS = 1.0f;
  if (k.sfSpeedFloor_kmh < in.carrierSpeed_kmh)
    wS = in.carrierSpeed_kmh < k.sfSpeedCeil_kmh
             ? (in.carrierSpeed_kmh - k.sfSpeedCeil_kmh) / k.sfSpeedSpan_kmh
             : 0.0f;
  float wD = 1.0f;
  if (k.sfScoreDist_m < m.recADist_m)
    wD = m.recADist_m < k.sfDistCeil_m ? (m.recADist_m - k.sfDistCeil_m) / k.sfSpeedSpan_kmh : 0.0f;
  float need = wS * k.sfNeedGain + k.sfNeedBase + wD * k.sfNeedGain;
  if (m.recAClass == 4) need += k.sfClass4Bonus;
  if (m.recAClass == 5) need += k.sfClass5Bonus;
  if (in.danger) need = k.sfNeedDanger;
  return need <= k.sfNeedCap ? std::max(need, k.sfNeedFloor) : k.sfNeedCap;
}

// The carrier's inputs to the short picker's head: b28 (free-ish: press10 == 0, or press10 == 1
// with press34 > 1) and dCarrierOpp2, the squared gap between his 27-frame prediction and the
// nearest opponent's 27-frame prediction. No opponent at all answers no one (0xFF).
struct ShortCarrier {
  bool b28 = false;
  float oppD2_27 = 0.0f;
};

std::optional<ShortCarrier> ShortCarrierState(const SafetyInput& in, const CarrierConstants& k) {
  ShortCarrier c;
  c.b28 = in.carrierPressClass == 0 || (in.carrierPressClass == 1 && in.pressCount2 > 1);
  const int frames = PesFrames(k.gfPredictGain);  // int(54 * 0.5 + 0.5) = 27
  const CarrierPoint carrierPred = PredictPoint(in.carrierPos, in.carrierVelPerFrame, frames);
  const int opp = NearestSideSlot(in.players, OpponentSide(in.carrierSlot), carrierPred);
  if (opp == lane::kNoSlot) return std::nullopt;
  const lane::LanePlayer& op = in.players[opp];
  c.oppD2_27 =
      Dist2(carrierPred, PredictPoint({op.pos.x, op.pos.z}, {op.vel.x, op.vel.z}, frames));
  return c;
}

// The short picker's per-mate gates in the decompile's order (0x142126B70). False when a gate
// rejects the mate; a survivor is a candidate for the tiebreak.
bool ShortMateSurvives(const SafetyInput& in, const SafetyMate& m, const ShortCarrier& c,
                       const CarrierConstants& k) {
  if (!m.statusValid) return false;
  const bool keeper = m.roleClass == 0;
  const bool over110 = k.sfTurnOver_deg < m.recATurnAngle_deg;
  const bool under80 = m.recATurnAngle_deg < k.sfTurnUnder_deg;
  const int oppSide = OpponentSide(in.carrierSlot);
  // The pre-gate: a keeper needs danger or a pressed carrier, the ball-speed arm and the side arm;
  // a field mate needs a human carrier or an opponent-table distance of at least 15 m.
  if (keeper) {
    if (!(in.danger || in.pressCount1 != 0)) return false;
    const bool speedArm = in.humanCarrier || m.recADist_m >= k.sfKeeperDist_m ||
                          (!in.ballTrackSet && in.ballSpeed_kmh <= k.sfKeeperSpeedCap);
    const bool carrierBack = in.attackSign * in.carrierPos.x <=
                             in.attackSign * in.teamLineOwn + k.sfMateNear_m;
    const bool sideArm = in.humanCarrier ||
                         ((m.recADist_m <= k.sfKeeperFar_m || carrierBack) &&
                          (!in.receivingFlag || in.ctx44 < k.sfKeeperScalar) &&
                          in.carrierPressClass == 0);
    if (!(speedArm && sideArm)) return false;
  } else if (!(in.humanCarrier || k.sfOppD2Min <= m.nearestOppD2)) {
    return false;
  }
  // The scored limit and the turn arm, when the scoring gate runs (press30 < 2, class < 5, dist >= 10).
  if (in.pressCount1 < 2 || m.recAClass < 5 || k.sfScoreDist_m <= m.recADist_m) {
    if (ShortLimit(in, m, keeper, k) < m.recAVsFacing_deg) return false;
    if (over110) {
      const float aDiff = std::fabs(WrapDeg(in.sideAngle_deg - m.facing_deg));
      const float fDiff = std::fabs(WrapDeg(in.carrierFacing_deg - m.facing_deg));
      if (aDiff < k.sfTurnAttack_deg && k.sfTurnFace_deg < fDiff) return false;
    }
  }
  // The distance window: 10 m for a keeper, 5 m otherwise; then the ceiling cascade.
  if (m.recADist_m < (keeper ? k.sfScoreDist_m : k.sfMateNear_m)) return false;
  float ceiling = k.sfCeilDefault_m;
  if (over110) {
    bool x = true;
    if (k.sfCeilOpp_m < m.recAVsFacing_deg) {
      ceiling = k.sfCeilNear_m;
      x = m.recAClass > 3;
    }
    if (x) {
      ceiling = k.sfCeilNear_m;
      if (in.attackSign * in.ball.x <= 0.0f) {
        ceiling = k.sfCeilDefault_m;
        if (keeper && m.recAClass > 4) ceiling = k.sfCeilFar_m;
      }
    }
  }
  if (2 < in.teamMode) ceiling = k.sfCeilNear_m;
  if (ceiling < m.recADist_m || k.gfDifficultyMax < m.recADifficulty) return false;
  // The mate-speed arm: a mate under 5 m away and moving over 20 km/h is no receiver.
  if (m.recADist_m < k.sfMateNear_m && k.sfMateSpeed_kmh < m.speed_kmh) return false;
  const int fr = ShortFrames(m.recADist_m, k.sfFrameSpeed);
  const CarrierPoint matePred = PredictPoint(m.pos, m.velPerFrame, fr);
  const int mateOpp = NearestSideSlot(in.players, oppSide, matePred);
  // G1: a carrier not b28, a turn of 50 or more, or a mate within 15 m: the opponent tests.
  const bool g1 =
      !c.b28 || k.sfOuterTurn_deg <= m.recATurnAngle_deg || m.recADist_m <= k.sfPressDist_m;
  if (g1) {
    if (mateOpp != lane::kNoSlot) {
      const lane::LanePlayer& op = in.players[mateOpp];
      const CarrierPoint oppPred = PredictPoint({op.pos.x, op.pos.z}, {op.vel.x, op.vel.z}, fr);
      const float gap2 = Dist2(matePred, oppPred);
      const float reach = k.sfLimGain * m.recADist_m;
      if (keeper || std::sqrt(m.nearestOppD2) < k.sfOppGate_m) {
        const float lim = std::clamp(reach, k.sfLimNear_m, k.sfLimFar_m);
        if (gap2 <= lim * lim) return false;
      } else if (!under80 && !m.zoneMid) {
        const float lim = std::clamp(reach, k.sfLimMin_m, k.sfLimNear_m);
        if (gap2 <= lim * lim) return false;
      }
      // The second test: the mate's predicted point stays clear of his opponent's reach.
      const float pad = in.attackSign * m.pos.x <= 0.0f
                            ? (keeper ? k.sfKeepPad_m : k.sfPadBack_m)
                            : (over110 ? k.sfOppPad_m : 0.0f);
      const float reach2 = std::sqrt(c.oppD2_27) + pad;
      if (gap2 < reach2 * reach2) return false;
    }
    if (!under80) {  // the pressure gate: the mate's class above the carrier's, and the close-in rules
      const float pc = m.pressureClass;
      const float cc = static_cast<float>(in.carrierPressClass);
      if (pc <= cc) return false;
      if (pc < 2.0f && k.sfPressDist_m < m.recADist_m) return false;
      if (!c.b28 && over110 && pc < 2.0f) return false;
    }
  }
  const int classNeed = keeper ? 4 : (c.b28 && under80 ? 2 : 3);
  if (m.recAClass < classNeed) return false;
  const int horizonFourth = static_cast<int>(kPesTickHz) >> 2;  // fps >> 2 = 13
  if (!under80) {
    // The one-touch lane (wrapper D): the ball advanced by the mate's distance along his heading,
    // aimed back at the ball; length clamp(dist / 5, 4, 12), cone 220, no radius, horizon 13.
    const CarrierPoint origin = AdvancePoint(in.ball, m.heading_deg, m.recADist_m);
    const float len =
        std::clamp(m.recADist_m / k.sfLaneDivisor, k.sfLaneLenMin_m, k.sfLaneLenMax_m);
    const lane::LaneRequest lane{ToLaneVec(origin), BearingDeg(origin, in.ball), len,
                                 k.sfLaneCone_deg, 0.0f};
    if (!lane::LaneFree(k.lane, in.players, m.index, lane, /*keepKeeper*/ false, horizonFourth))
      return false;
  }
  if (keeper) {
    // The keeper lane: the ball toward the keeper's 13-frame prediction, cone 90, radius 1, with
    // the opponents' keeper counted in the scan (keep = 1).
    const CarrierPoint keeperPred = PredictPoint(m.pos, m.velPerFrame, horizonFourth);
    const float len = std::sqrt(Dist2(in.ball, keeperPred));
    if (AnnulusCount(in.players, oppSide, in.ball, BearingDeg(in.ball, keeperPred),
                     k.sfKeeperCone_deg, k.sfKeeperRad_m, len, /*keepKeeper*/ true) != 0)
      return false;
  }
  return true;
}

// The tiebreak between a surviving candidate `q` and the best so far (0x142126B70's tail): true
// when q replaces the best. The own-half and facing branch hands the keeper's place to a mate whose
// turn is no bigger; otherwise a field mate replaces a keeper, and two field mates are ranked by the
// corner distance, then by class and nearest-opponent distance.
bool ShortReplaces(const SafetyInput& in, const SafetyMate& best, const SafetyMate& q,
                   const CarrierConstants& k) {
  const bool bestKeeper = best.roleClass == 0;
  const bool ownHalf = in.attackSign == 1.0f    ? in.carrierPos.x <= 0.0f
                       : in.attackSign == -1.0f ? in.carrierPos.x >= 0.0f
                                                : false;
  const bool facingAway =
      k.sfTieDiff_deg < std::fabs(WrapDeg(in.sideAngle_deg - in.carrierFacing_deg));
  if (ownHalf && facingAway && in.oppPressPair <= k.sfTiePress)
    return bestKeeper && best.recAVsFacing_deg <= q.recAVsFacing_deg;
  if (q.roleClass == 0) return false;
  if (bestKeeper) return true;
  const CarrierPoint corner{-in.attackSign * in.pitchHalfLen_m, 0.0f};
  if (k.sfTieCornerSq <= Dist2(in.ball, corner))
    return in.attackSign * best.pos.x < in.attackSign * q.pos.x;
  if (best.recAClass != q.recAClass) return false;
  const float dBest = std::sqrt(best.nearestOppD2);
  const float dQ = std::sqrt(q.nearestOppD2);
  if (!(dBest <= dQ)) return true;  // the chain's comma assignment leaves q in place
  return std::fabs(dBest - dQ) < k.sfTieGap_m &&
         in.attackSign * best.pos.x < in.attackSign * q.pos.x;
}

// 0x142126B70, the Safety short picker: the first survivor is taken and a later one replaces it
// under ShortReplaces.
int SafetyShortPicker(const SafetyInput& in, const CarrierConstants& k) {
  const std::optional<ShortCarrier> carrier = ShortCarrierState(in, k);
  if (!carrier) return lane::kNoSlot;
  const SafetyMate* best = nullptr;
  for (const SafetyMate& m : in.mates) {
    if (m.index < 0 || m.index > 21 || m.index == in.carrierSlot) continue;
    if (!ShortMateSurvives(in, m, *carrier, k)) continue;
    if (best == nullptr || ShortReplaces(in, *best, m, k)) best = &m;
  }
  return best != nullptr ? best->index : lane::kNoSlot;
}

// The PassThrough validator 0x1421226C0 for a Safety mate: the same gates on his record-B fields,
// his team need as the nearest-opponent distance (0x1404C8E00), and the carrier as the BackSpace
// view of him.
bool ThroughValidator(const SafetyInput& in, const SafetyMate& m, const CarrierConstants& k) {
  BackSpaceInput bs;
  bs.players = in.players;
  bs.carrierSlot = in.carrierSlot;
  bs.carrierPos = in.carrierPos;
  bs.carrierVelPerFrame = in.carrierVelPerFrame;
  bs.carrierFacing_deg = in.carrierFacing_deg;
  bs.carrierHeading_deg = in.carrierHeading_deg;
  bs.carrierSlotStat = 0.0f;  // ponytail: the per-slot stat (imageA+0x898) is undecoded (helpers §5)
  bs.attackSign = in.attackSign;
  bs.teamLineOpp = in.teamLineOpp;
  bs.pitchBound8 = in.pitchBound8;
  bs.pitchF6 = in.pitchF6;
  bs.ball = in.ball;
  bs.humanCarrier = in.humanCarrier;
  bs.danger = in.danger;
  bs.goalMouth = in.goalMouth;
  bs.planMode = in.planMode;
  bs.boxSector = in.boxSector;
  bs.boxCount = in.boxCount;
  bs.boxMargin = in.boxMargin;
  bs.boxAngle = in.boxAngle;
  BackSpaceMate bm;
  bm.index = m.index;
  bm.pos = m.pos;
  bm.velPerFrame = m.velPerFrame;
  bm.facing_deg = m.facing_deg;
  bm.heading_deg = m.heading_deg;
  bm.speed_kmh = m.speed_kmh;
  bm.statusValid = m.statusValid;
  bm.teamNeed_m = std::sqrt(m.nearestOppD2);
  bm.recBTarget = m.recBTarget;
  bm.recBAngle_deg = m.recBAngle_deg;
  bm.recBDist_m = m.recBDist_m;
  bm.recBDifficulty = m.recBDifficulty;
  bm.recBClass = m.recBClass;
  bm.zoneAtPred16 = m.zoneAtPred16;
  bs.mates.push_back(bm);
  const auto nearestRank0 = [&](int q) {
    const lane::LanePlayer& qp = in.players[q];
    return NearestSideSlot(in.players, lane::IsAwaySlot(q) ? 0 : 1, {qp.pos.x, qp.pos.z});
  };
  return PassThroughValidator(bs, 0, k, nearestRank0);
}

// 0x142128490, the Safety through picker. Unless the fast path holds (the carrier's count over 162
// frames and the opponent pair under 9), the carrier must be 40 m clear of his nearest opponent and
// not behind his own line. Then the teammates nearest the carrier, in order: the first whose aim,
// band, validator and final lane all pass takes the through ball.
int SafetyThroughPicker(const SafetyInput& in, const CarrierConstants& k) {
  const bool fastPath = PesFrames(k.sfThruIdle) < in.pressCount4 && in.oppPressPair < k.sfThruPress;
  if (!fastPath) {
    const int nearest = NearestSideSlot(in.players, OpponentSide(in.carrierSlot), in.carrierPos);
    if (nearest == lane::kNoSlot) return lane::kNoSlot;
    const lane::LanePlayer& np = in.players[nearest];
    if (!(k.sfThruCarrierSq < Dist2({np.pos.x, np.pos.z}, in.carrierPos))) return lane::kNoSlot;
    if (in.attackSign * in.carrierPos.x < 0.0f &&
        in.attackSign * in.carrierPos.x < in.attackSign * in.teamLineB)
      return lane::kNoSlot;
  }
  // The carrier's NearestEntry row: his teammates in the NearestList order the caller gives (the
  // caller sorts them), eleven at most.
  std::vector<const SafetyMate*> row;
  for (const SafetyMate& m : in.mates)
    if (0 <= m.index && m.index <= 21 && m.index != in.carrierSlot) row.push_back(&m);
  for (size_t j = 0; j < row.size() && j < static_cast<size_t>(lane::kSideSize); ++j) {
    const SafetyMate& m = *row[j];
    if (k.sfThruRowSq < Dist2(m.pos, in.carrierPos)) break;  // the row is sorted: the rest are farther
    if (m.roleClass == 0 || !m.statusValid) continue;
    if (k.sfSpeedFloor_kmh <= m.speed_kmh &&
        k.sfThruAlign_deg < std::fabs(WrapDeg(m.heading_deg - in.sideAngle_deg)))
      continue;
    const CarrierPoint aim = AdvancePoint(m.recBTarget, Wrap360(in.sideAngle_deg),
                                          m.recBDist_m * k.imageHalfLine);
    if (!(std::fabs(WrapDeg(BearingDeg(in.ball, aim) - in.sideAngle_deg)) < k.sfThruAim_deg))
      continue;
    if (m.recBClass < k.sfThruClass || k.gfDifficultyMax < m.recBDifficulty ||
        k.sfThruFacing_deg < m.recBVsFacing_deg)
      continue;
    if (in.attackSign * aim.x < in.attackSign * in.ball.x) continue;
    const float bandD2 = Dist2(in.ball, aim);
    if (bandD2 < k.sfThruNearSq || k.sfThruFarSq < bandD2) continue;
    if (!ThroughValidator(in, m, k)) continue;
    // The final lane: origin the record-B target, theta the side angle, 10 m / 60 deg / 5 m, h = 54.
    const lane::LaneRequest lane{ToLaneVec(m.recBTarget), Wrap360(in.sideAngle_deg),
                                 k.sfThruLaneLen_m, k.sfThruLaneCone_deg, k.sfThruLaneRad_m};
    if (lane::LaneFree(k.lane, in.players, m.index, lane, /*keepKeeper*/ false, PesFrames(1.0f)))
      return m.index;
  }
  return lane::kNoSlot;
}

}  // namespace

PassAnswer SafetyUnit(const SafetyInput& in, const CarrierConstants& k) {
  PassAnswer answer;
  // The gate (0x14212B520): the pressure counts and the facing-vs-plan turn set k; a FREE gate lane
  // (origin the carrier, the nearest opponent's distance as the length) answers no one.
  int kk = 0;
  if (in.danger) {
    kk = 3;
  } else {
    if (in.pressCount1 > 0) {
      kk = 2;
    } else if (in.pressCount2 > 1) {
      kk = 1;
    }
    if (in.pressCount3 != 0 &&
        k.sfFacingTurn_deg < std::fabs(WrapDeg(in.carrierFacing_deg - in.planHeading_deg)))
      ++kk;
    // PES's gate length is the root of the carrier's own d^2 in the score row of the goal he defends
    // (0x14212B71D..0x14212B76F, the row for the sign !k): the S1 writer's row, pesscorerows.hpp.
    const float len = std::sqrt(in.carrierOwnGoalD2_m2);
    const lane::LaneRequest gate{ToLaneVec(in.carrierPos), in.laneHeading_deg, len,
                                 k.unitLaneCone_deg, k.sfLaneRad_m};
    if (lane::LaneFree(k.lane, in.players, in.carrierSlot, gate, /*keepKeeper*/ false, 0))
      return answer;
  }
  if (in.goalMouth) return answer;
  // The short picker always runs; the through picker only when the gate set k.
  const int shortSlot = SafetyShortPicker(in, k);
  if (kk != 0) {
    const int thru = SafetyThroughPicker(in, k);
    if (thru != lane::kNoSlot) {
      answer.index = thru;
      answer.kind = kPassThrough;
      return answer;
    }
  }
  if (shortSlot == lane::kNoSlot) return answer;
  answer.index = shortSlot;
  answer.kind = kPassShort;
  return answer;
}

namespace {

// 0x140A4E3F0, the closing sweep behind OneTwo's gate: opponents of `ref` whose frames-ahead point lies
// strictly within `outer_m` of the reference's own frames-ahead point (0x140A4E720 passes the inner
// radius as 0.0, so only d2 > 0 is required), and whose bearing from it is within `thr_deg` of the
// attack angle.
// ponytail: PES's flag-1 path skips the per-side active check; GF keeps it, so a slot GF leaves
// empty (no player) never counts at the origin.
int ClosingCount(const std::array<lane::LanePlayer, lane::kSlots>& players, int ref,
                 float attack_deg, int frames, float outer_m, float thr_deg) {
  if (ref < 0 || ref > 21) return 0;
  const int opp = lane::IsAwaySlot(ref) ? 0 : 1;
  const lane::LanePlayer& r = players[ref];
  const CarrierPoint pr = PredictPoint({r.pos.x, r.pos.z}, {r.vel.x, r.vel.z}, frames);
  const float outerSq = outer_m * outer_m;
  int count = 0;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int q = lane::kSideSize * opp + j;
    if (!players[q].active) continue;
    const lane::LanePlayer& o = players[q];
    const CarrierPoint pq = PredictPoint({o.pos.x, o.pos.z}, {o.vel.x, o.vel.z}, frames);
    const float d2 = Dist2(pr, pq);
    if (!(0.0f < d2 && d2 < outerSq)) continue;
    if (std::fabs(WrapDeg(BearingDeg(pr, pq) - attack_deg)) <= thr_deg) ++count;
  }
  return count;
}

// 0x142124950, the OneTwo receiver predicate (lane-core §8): teammate r takes a one-two ball off the
// carrier. The gates in decoded order.
bool TouchReceiverOk(const TouchInput& in, const TouchReceiver& r, const CarrierConstants& k) {
  const int c = in.carrierSlot;
  if (r.index < 0 || r.index > 21 || r.index == c) return false;  // P0
  if (!r.statusValid) return false;                                // P1
  if (r.roleClass == 0) return false;                              // P2: 0x142124A83 exits on snapshot[0xD] == 0
  if (r.oneTwoRun) return false;                                   // P3: player(r)+0x5D0 != 0x25
  // Band 1: beyond 15 m, or past 110 degrees of facing, the difficulty is held to 1.0; else to 2.0.
  const bool farOrTurned = r.dist_m >= k.rcvDistFar_m || r.turnFacing_deg >= k.rcvTurnFar_deg;
  if (r.difficulty > (farOrTurned ? k.rcvDifficultyFar : k.rcvDifficultyNear)) return false;
  // The angle block runs unless the carrier is human, mode 3 or 4 holds with C[0xF] set, and r is
  // within 6 m (NL(c, r) under 36). It tests the carrier's facing against the bearing to r.
  // ponytail: the bearing stands in for the A_tab table T18+0x22F7C (lane-units2 O9, writer OPEN).
  const float nl = Dist2(in.carrierPos, r.pos);
  const bool modeThreeOrFour = in.teamMode == 3 || in.teamMode == 4;
  if (in.humanCarrier || !modeThreeOrFour || !in.flag15 || nl >= k.rcvNearSq_m2) {
    const float limit =
        in.carrierSpeed_kmh > k.rcvSpeedGate_kmh ? k.rcvAngleFast_deg : k.rcvAngleSlow_deg;
    if (std::fabs(WrapDeg(in.carrierFacing_deg - BearingDeg(in.carrierPos, r.pos))) > limit)
      return false;
  }
  // The window: the base by the pass angle against the attack, the ramp by the angle against the facing.
  const float base = r.attackAngle_deg < k.rcvBaseSplit_deg ? k.rcvBaseNear_m : k.rcvBaseFar_m;
  float ramp = 0.0f;
  if (r.turnFacing_deg <= k.rcvRampHalf_deg) {
    ramp = 1.0f;
  } else if (r.turnFacing_deg < k.rcvRampTop_deg) {
    ramp = (r.turnFacing_deg - k.rcvRampTop_deg) / k.rcvRampSpan;
  }
  const float reach = base + k.rcvRampGain_m * ramp;
  if (r.dist_m < k.rcvWindowFloor_m || std::max(reach, k.rcvWindowFloor_m) < r.dist_m) return false;
  // M[r][c]: the receiver faces the carrier within 120 degrees.
  // ponytail: the bearing r -> c stands in for the M table T18+0x23F7C (lane-units2 O9, writer OPEN).
  if (!(std::fabs(WrapDeg(r.facing_deg - BearingDeg(r.pos, in.carrierPos))) < k.rcvAngleSlow_deg))
    return false;
  // M[c][r]: the pass is off the attack's line by more than 20 degrees, and the openness class is over 2.
  // ponytail: the bearing c -> r stands in for the M table (lane-units2 O9, writer OPEN).
  if (!(std::fabs(WrapDeg(in.attackAngle_deg - BearingDeg(in.carrierPos, r.pos))) > k.rcvM2_deg))
    return false;
  return r.opennessClass > 2;
}

}  // namespace

float CurveEval(const Curve& c, float z_m) {
  // 0x142139650: u = clamp(z*scale + 35, 0, 70); u == 70 reads the end sample, else the lerp of two samples.
  const float u = std::clamp(z_m * c.scale + kCurveCentre, 0.0f, static_cast<float>(kCurveLast));
  const int i = static_cast<int>(u);
  if (i >= kCurveLast) return c.x[kCurveLast];
  // The decode multiplies and divides by 0x14253EE30 around the lerp; that factor cancels to one rounding.
  return c.x[i] + (c.x[i + 1] - c.x[i]) * (u - static_cast<float>(i));
}

namespace {

// 0x142565600: the decode's one half (the dedupe cone is half of K).
constexpr float kHalf = 0.5f;

// 0x140A90C20: the crossing of the lines a->b and c->d, t along a->b and u along c->d. False when the lines
// are within eps of parallel.
bool CrossLines(CarrierPoint a, CarrierPoint b, CarrierPoint c, CarrierPoint d, float eps, float& t, float& u) {
  const float dx = d.x - c.x;
  const float dz = d.z - c.z;
  const float den = (b.x - a.x) * dz - (b.z - a.z) * dx;
  if (std::fabs(den) < eps) return false;
  t = ((c.x - a.x) * dz - (c.z - a.z) * dx) / den;
  u = ((b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z)) / den;
  return true;
}

}  // namespace

// 0x142138220: the 71-sample curve from the carrier's opponents (see the header for the contract).
std::optional<Curve> BuildCurve(const std::array<lane::LanePlayer, lane::kSlots>& players, int carrierSide,
                                float attackSign, float attackAngle_deg, float halfLen_m,
                                const CarrierConstants& k) {
  // The listed men are the opponents (flag = side 0 lists slots 11..21), in play and not their keeper
  // (0x140A638C0), keyed by s*x and taken most advanced first (the comparator 0x14063EA50 sorts descending).
  // ponytail: 0x140A48910's record filter (record +1 == 0x11 or 'W', +8 bit 2) also drops men; GF carries no
  // such bytes, so none is dropped. ponytail: equal keys keep slot order; the tie rule of 0x141E53540 is open.
  const int oppSide = 1 - carrierSide;
  const int keeper = lane::KeeperSlot(oppSide, players);
  struct Listed {
    int slot;
    float key;
  };
  std::array<Listed, lane::kSideSize> listed{};
  int nListed = 0;
  for (int j = 0; j < lane::kSideSize; ++j) {
    const int slot = lane::kSideSize * oppSide + j;
    if (!players[slot].active || slot == keeper) continue;
    listed[nListed++] = Listed{slot, attackSign * players[slot].pos.x};
  }
  // 0x142138220 keeps its old rows when nobody is listed; GF keeps no earlier table, so there is none.
  if (nListed == 0) return std::nullopt;
  std::stable_sort(listed.begin(), listed.begin() + nListed,
                   [](const Listed& a, const Listed& b) { return a.key > b.key; });

  // The lane angle a = psi - 180 (psi = T+0x250). Each listed man's segment point q sits curveStep_m along psi,
  // and is dropped when an earlier point shadows it (0x140A53DF0: 0 < d2 < arm^2, and the bearing from the
  // earlier point is within half of K of a).
  const float a = Wrap360(attackAngle_deg - k.curveHalfTurn_deg);
  const float along = Wrap360(a - k.curveHalfTurn_deg);
  std::array<CarrierPoint, lane::kSideSize> segments{};
  int nSegments = 0;
  for (int n = 0; n < nListed; ++n) {
    const CarrierPoint p{players[listed[n].slot].pos.x, players[listed[n].slot].pos.z};
    const CarrierPoint q = AdvancePoint(p, along, k.curveStep_m);
    bool shadowed = false;
    for (int s = 0; s < nSegments && !shadowed; ++s) {
      const float d2 = Dist2(segments[s], q);
      shadowed = d2 > 0.0f && d2 < k.curveArm_m * k.curveArm_m &&
                 std::fabs(WrapDeg(BearingDeg(segments[s], q) - a)) < k.curveWide_deg * kHalf;
    }
    if (!shadowed) segments[nSegments++] = q;
  }

  // Sample i is the lateral line z = (i - 35) * s across the pitch, from -s*F.x to s*F.x. Its x is the most
  // advanced crossing of any segment's two arms (rays from the segment point at a +- 50 degrees, 1000 m long;
  // a crossing counts when u >= 0), and starts at the back line -s*F.x.
  Curve curve;
  curve.scale = attackSign;
  for (int i = 0; i < kCurveSamples; ++i) {
    const float z = (static_cast<float>(i) - kCurveCentre) * attackSign;
    const CarrierPoint from{-attackSign * halfLen_m, z};
    const CarrierPoint to{attackSign * halfLen_m, z};
    float best = -attackSign * halfLen_m;
    for (int s = 0; s < nSegments; ++s) {
      for (const float turn : {Wrap360(a + k.curveArmAngle_deg), Wrap360(a - k.curveArmAngle_deg)}) {
        const CarrierPoint armEnd = AdvancePoint(segments[s], turn, k.curveArm_m);
        float t = 0.0f;
        float u = 0.0f;
        if (!CrossLines(from, to, segments[s], armEnd, k.curveEps, t, u) || u < 0.0f) continue;
        const float x = from.x + t * (to.x - from.x);
        if (attackSign * x > attackSign * best) best = x;
      }
    }
    curve.x[i] = best;
  }
  return curve;
}

bool OneTouch(const TouchInput& in, const CarrierConstants& k) {
  const float s = in.attackSign;
  // G1: the carrier is not behind his team line (k*x >= k*line).
  if (s * in.carrierPos.x < s * in.ownLine_m) return false;
  // G2: nor past the pitch bound and its pad (k*x <= F+0x18 + 2).
  if (s * in.carrierPos.x > in.pitchBound_m + k.touchPad_m) return false;
  // G3: the team level, level/100, is at least 2. 0x140A40BD0 reads the level (zero when +0xA29C is set);
  // 0x140A60360 rejects level/100 < 2.
  const int level = in.teamLevelZero ? 0 : in.teamLevelRaw;
  const int levelHundreds = level / 100;
  if (levelHundreds < 2) return false;
  // G4: the team mode is 3 or 4, or the carrier's category is 4 or more (0 is the keeper).
  const bool modeThreeOrFour = in.teamMode == 3 || in.teamMode == 4;
  if (!modeThreeOrFour && in.carrierCategory < 4) return false;
  // G5: the score row reads 1600 (40 m squared) or more.
  if (in.score < k.touchScore_m2) return false;
  // G6: the carrier is not in the close shooting band.
  if (in.closeZone) return false;
  // G7: a human carrier, or no one-two run on the carrier's side.
  if (!in.humanCarrier && in.oneTwoRunEvent) return false;
  // G8: the runs in progress stay under min(2, base + inc): base from the level (5 or 6: 2, 4: 1),
  // inc from the attack level (3 or more) or the team mode (2 or 3).
  const int base = (levelHundreds == 5 || levelHundreds == 6) ? 2 : (levelHundreds == 4 ? 1 : 0);
  const int inc = (in.attackLevel >= 3 || in.teamMode == 2 || in.teamMode == 3) ? 1 : 0;
  if (in.runCount >= std::min(2, base + inc)) return false;
  // G9: the plan is 1, 2, 3, 5 or 6.
  const int m = in.planMode;
  if (!(m == 1 || m == 2 || m == 3 || m == 5 || m == 6)) return false;
  // G10: not a fast, marked carrier turning more than 100 degrees from the plan heading.
  if (in.carrierSpeed_kmh >= k.touchSpeed_kmh && in.markCount > 3.0f &&
      std::fabs(WrapDeg(in.carrierHeading_deg - in.planHeading_deg)) > k.touchTurn_deg)
    return false;
  // G11: the lane 15 m along the plan heading from the carrier holds an opponent (count != 0).
  const lane::LaneRequest lane{ToLaneVec(in.carrierPos), in.planHeading_deg, k.touchLaneLen_m,
                               k.touchLaneCone_deg, k.touchLaneRad_m};
  if (lane::LaneFree(k.lane, in.players, in.carrierSlot, lane, /*keepKeeper*/ false,
                     /*horizonFrames*/ 0))
    return false;
  // G12: the carrier is 0 to 10 m behind the curve at his lateral position: -10 <= k*(x - curve) < 0.
  if (!in.curve.has_value()) return false;
  const float band = s * (in.carrierPos.x - CurveEval(*in.curve, in.carrierPos.z));
  return k.touchBand_m <= band && band < 0.0f;
}

PassAnswer OneTwoUnit(const OneTwoInput& in, const CarrierConstants& k) {
  PassAnswer answer;
  const int c = in.touch.carrierSlot;
  // The closing sweep (0x140A4E720): frames int(54*0.3+0.5) = 16, the 6 m outer radius, and the attack
  // bearing within half of the 180 degree half turn.
  const int closing = ClosingCount(in.touch.players, c, in.touch.attackAngle_deg,
                                   PesFrames(k.oneTwoFrameGain), k.oneTwoOuter_m,
                                   k.oneTwoClosingHalf_deg * 0.5f);
  // The carrier's angle against the attack, folded once past 180 (0x14212AF40-AF60).
  float d = in.carrierAngle_deg - in.touch.attackAngle_deg;
  if (std::fabs(d) > k.oneTwoClosingHalf_deg) d -= (d >= 0.0f ? 1.0f : -1.0f) * 360.0f;
  if (!(std::fabs(d) <= k.oneTwoAngle_deg || closing != 0)) return answer;
  if (!OneTouch(in.touch, k)) return answer;
  // The picker (0x142126A80): the first listed teammate the receiver predicate takes.
  for (const TouchReceiver& r : in.mates) {
    if (!TouchReceiverOk(in.touch, r, k)) continue;
    answer.index = r.index;
    answer.kind = kPassShort;  // b = 0
    answer.oneTouch = true;    // f10
    return answer;
  }
  return answer;
}


PassAnswer DirectHighBallUnit(const DirectHighBallInput& in, const CarrierConstants& k) {
  PassAnswer answer;
  const int c = in.touch.carrierSlot;
  const std::array<lane::LanePlayer, lane::kSlots>& players = in.touch.players;
  const int side = lane::IsAwaySlot(c) ? 1 : 0;  // IsAway(c): the carrier's side, the teammate scan
  if (in.firstTimeWhileReceiving) return answer;  // AI rec+1 state bit 5 (0x140A6E5E0)
  if (!in.volatileBall) return answer;            // K+0x6E == 0 (0x142129CC8)
  // The low-ball block: a ball at or under the carrier's head height (+0.1 m) needs an opponent within
  // 5 m of it, and the carrier outside the mid zone.
  if (in.ballHeight_m <= in.carrierHeight_m + k.dhbHeightPad_m) {
    const int opp = NearestSideSlot(players, 1 - side, in.ball);
    if (opp == lane::kNoSlot) return answer;
    // ponytail: the history position (0x140A76240, S17) stands in with the current one, so the
    // decode's two d2 tests collapse into the one on the current position.
    const lane::LanePlayer& op = players[opp];
    if (k.dhbNearOpp_m2 < Dist2({op.pos.x, op.pos.z}, in.ball)) return answer;
    if (in.mid) return answer;
  }
  // The mates, nearest first: the candidates the score, branch A, and the 9- and 27-frame tests pick.
  const int frames9 = static_cast<int>(kPesTickHz) / 6;
  const int frames27 = PesFrames(k.dhbFarGain);
  const DirectHighBallMate* best = nullptr;
  float bestD2 = 0.0f;
  for (const DirectHighBallMate& m : in.mates) {
    const TouchReceiver& r = m.receiver;
    if (r.index < 0 || r.index > 21 || r.index == c) continue;
    if (m.keeper || !r.statusValid || m.recAClass < 1) continue;
    if (m.score < k.dhbScoreFloor) continue;
    if (m.score < k.dhbScoreCeil) {
      // Branch A: a mate whose nearest opponent is under 10 m away is skipped (exactly 10 m is kept).
      const int nearOpp = NearestSideSlot(players, 1 - side, r.pos);
      if (nearOpp != lane::kNoSlot) {
        const lane::LanePlayer& op = players[nearOpp];
        if (Dist2(r.pos, {op.pos.x, op.pos.z}) < k.dhbMateNear_m2) continue;
      }
    }
    if (Dist2(in.ball, PredictPoint(r.pos, m.velPerFrame, frames9)) < k.dhbPredNear_m2) continue;
    const float d = Dist2(in.ball, PredictPoint(r.pos, m.velPerFrame, frames27));
    if (best == nullptr || d < bestD2) {
      best = &m;
      bestD2 = d;
    }
  }
  if (best == nullptr) return answer;
  answer.index = best->receiver.index;
  answer.kind = kPassShort;  // b = 0 unless the lane below is free
  // X = wrap180(heading - attack): past 80 degrees the ball stays short and f10 is not written.
  if (std::fabs(WrapDeg(best->heading_deg - in.touch.attackAngle_deg)) > k.dhbFacing_deg)
    return answer;
  // Free when the mark count is 4, or it is off 0 and 2 with him over 15 km/h. The count is read
  // only below 5, else 0 (0x140A58320).
  const int mc = best->markCount < 5 ? best->markCount : 0;
  bool isFree = mc == 4 || (mc != 0 && mc != 2 && best->speed_kmh > k.dhbSpeedGate_kmh);
  if (!isFree) {
    // The teammate lane from the mate's position advanced 4 m along theta = table + 180: 15 m, cone 60,
    // radius 5, horizon fps >> 2 = 13, keeper excluded.
    // ponytail: the angle table 0x140A71260 (S10, writer OPEN) stands in with his facing.
    const float theta = Wrap360(best->tableAngle_deg + 180.0f);
    const CarrierPoint origin = AdvancePoint(best->receiver.pos, theta, k.dhbAdvance_m);
    const lane::LaneRequest lane{ToLaneVec(origin), theta, k.dhbLaneLen_m, k.dhbLaneCone_deg,
                                 k.dhbLaneRad_m};
    isFree = lane::ScanLane(k.lane, players, c, side, lane, /*keepKeeper*/ false,
                            /*horizonFrames*/ static_cast<int>(kPesTickHz) >> 2)
                 .count == 0;
  }
  // f10: the one-touch predicate and the mate's receiver predicate.
  answer.oneTouch = OneTouch(in.touch, k) && TouchReceiverOk(in.touch, best->receiver, k);
  answer.kind = isFree ? kPassThrough : kPassShort;
  return answer;
}

bool UnitArmed(int kind, bool capFeint, bool capBackSpace, bool capSafety, float ballDistSq_m2,
               const CarrierConstants& k) {
  (void)k;
  // The team caps first: 17 needs 0x1E, 3 needs 0x1C, 6 needs 0x1D.
  if (kind == 17 && !capFeint) return false;
  if (kind == 3 && !capBackSpace) return false;
  if (kind == 6 && !capSafety) return false;
  // The 5 m ball-distance gate for shoot/clear/pass kinds (dist^2 <= 25).
  // ponytail: the touch-window frames, the turn veto and the restart-taker
  // veto read state GF does not carry; the distance gate is the portable part.
  if ((kind == 0 || kind == 1 || (kind >= 2 && kind <= 13)) && ballDistSq_m2 > 25.0f) return false;
  return true;
}
std::vector<int> BuildUnitList(const UnitListInput& in) {
  std::vector<int> l = {23, 22, 0, 1, 17, 14, 11, 12};
  const auto has = [&](int kind) { return std::find(l.begin(), l.end(), kind) != l.end(); };
  switch (in.plan) {
    case kPlanBreakthrough:
    case kPlanMiddleShot:
    case kPlanCurveShot:
      if (!(in.central && in.close)) {
        l.push_back(2);
        if (!in.stopAfterFirst) {
          if ((!in.central || !in.mid) && !in.close) l.push_back(3);
          if (in.wideSector) l.push_back(7);
          l.push_back(4);
          l.push_back(10);
        }
      }
      break;
    case kPlanPassThrough:
      l.push_back(3);
      l.push_back(2);
      if (!(in.oneTwoTarget && in.stopAfterFirst)) {
        l.push_back(4);
        l.push_back(10);
      }
      break;
    case kPlanCross:
      if (in.pastCrossLine) l.push_back(3);
      l.push_back(7);
      l.push_back(8);
      break;
    case kPlanLong:
      l.push_back(9);
      break;
    case kPlanSideChange:
      l.push_back(5);
      break;
    case kPlanShotPassFromGoal:
      break;
    case kPlanBackDFLine:
      l.push_back(13);
      break;
    default:  // Basic
      if (!in.flagE7 && !in.close) {
        l.push_back(2);
        l.push_back(3);
        l.push_back(10);
        if (!in.intentionMode4) l.push_back(4);
      }
      break;
  }
  // The tail (L82-95): the wide-side tail (7 and 6) and the team's human-side 9.
  if (in.teamModeThree && in.humanCarrier && !has(9)) l.push_back(9);
  if (in.wideSideLatch || in.planMode == 3) {
    if (!has(7)) l.push_back(7);
    if (!has(6)) l.push_back(6);
  }
  return l;
}

std::vector<UnitSlot> FillUnitTable(const std::vector<int>& kinds, bool shotAnswer,
                                    const PassAnswer& fwd, const PassAnswer& lng,
                                    const PassAnswer& cross, const PassAnswer& goalFront,
                                    const PassAnswer& backSpace, const PassAnswer& safety,
                                    const PassAnswer& oneTwo, const PassAnswer& directHigh) {
  std::vector<UnitSlot> table;
  for (const int kind : kinds) {
    UnitSlot slot;
    slot.kind = kind;
    if (kind == 0 && shotAnswer) slot.answer = {kAnswerShoot, 1};
    if (kind == 4 && fwd.index >= 0) slot.answer = {kAnswerPass, fwd.kind};
    if (kind == 9 && lng.index >= 0) slot.answer = {kAnswerPass, lng.kind};
    if (kind == 7 && cross.index >= 0) slot.answer = {kAnswerPass, cross.kind};
    if (kind == 2 && goalFront.index >= 0) slot.answer = {kAnswerPass, goalFront.kind};
    if (kind == 3 && backSpace.index >= 0) slot.answer = {kAnswerPass, backSpace.kind};
    if (kind == 6 && safety.index >= 0) slot.answer = {kAnswerPass, safety.kind};
    if (kind == 10 && oneTwo.index >= 0) slot.answer = {kAnswerPass, oneTwo.kind};
    if (kind == 11 && directHigh.index >= 0) slot.answer = {kAnswerPass, directHigh.kind};
    table.push_back(slot);
  }
  return table;
}

int DribbleChooser(const DribbleChooserInput& in, const CarrierConstants& k) {
  // The feint gate: the facing inside 30 degrees of the attack and of the plan heading, the carrier
  // past halfway, and the caller's feint test (0x14212e970).
  if (std::fabs(WrapDeg(in.teamAttack_deg - in.facing_deg)) < k.chooserGate_deg &&
      std::fabs(WrapDeg(in.planHeading_deg - in.facing_deg)) < k.chooserGate_deg &&
      in.forwardOfHalf && in.feintGate) {
    return kDribbleFeint;
  }
  if (in.teamMode == 1 && in.verticalVel < 0.0f && in.verticalVel < in.verticalLimit) {
    return kDribbleMode3;
  }
  // Plans 1-3 and a turning carrier are measured against the plan heading; the rest against the
  // team's attack, the side band widened by the pressure scalar.
  const bool planTurns = in.plan >= kPlanBreakthrough && in.plan <= kPlanCurveShot;
  if (!in.turning && !planTurns) {
    if (std::fabs(WrapDeg(in.facing_deg - in.teamAttack_deg)) >
        k.chooserSideAngle_deg + k.chooserPressureScale_deg * in.pressure) {
      return kDribbleMode3;
    }
  } else if (std::fabs(WrapDeg(in.facing_deg - in.planHeading_deg)) > k.chooserTurn_deg) {
    return kDribbleMode3;
  }
  if (in.boxPressed) return kDribbleMode3;
  const float vh = in.velZero ? 0.0f : in.velHeading_deg;
  if ((in.pressureClass < 0 || in.teamMode < 2) && in.speed_kmh > k.chooserSpeedLow_kmh &&
      std::fabs(WrapDeg(in.facing_deg - vh)) > k.chooserSideAngle_deg) {
    return kDribbleMode3;
  }
  // The free-lane test: at walking speed, or with the heading near his motion, and room to run.
  const bool steady = in.speed_kmh <= k.chooserSpeedMid_kmh ||
                      std::fabs(WrapDeg(in.facing_deg - vh)) <= k.chooserTurn_deg;
  if (steady && in.room_m >= k.chooserRoom_m) return in.laneFree ? kDribbleMode4 : kDribbleMode3;
  return kDribbleMode3;
}

int DribbleSubKind(const DribbleChooserInput& in, bool headingOverride, const CarrierConstants& k) {
  // The Long image runs no chooser (0x14212fe70 L433): the dispatch keeps its mode 3.
  if (in.plan == kPlanLong) return kDribbleMode3;
  const int sub = DribbleChooser(in, k);
  return headingOverride && sub == kDribbleMode3 ? kDribbleHeading : sub;
}

bool MiddleShotSkill(bool longRangeShooting, bool knuckleShot, bool droppingShots,
                     bool risingShots) {
  return longRangeShooting || knuckleShot || droppingShots || risingShots;
}

e_FunctionType PesPassFunctionType(int passKind) {
  switch (passKind) {
    case kPassThrough:
      return e_FunctionType_LongPass;  // GF's through ball
    case kPassLong:
    case kPassCentering:
      return e_FunctionType_HighPass;  // the lofted and the cross, GF's HighPass
    default:
      return e_FunctionType_ShortPass;
  }
}

}  // namespace pes
