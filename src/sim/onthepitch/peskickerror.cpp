#include "peskickerror.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>

namespace pes {
namespace {

float Clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }

// `lo >= x -> 0, x >= hi -> 1, else (x - lo) / span`: the ramp idiom every
// input of FUN_14083cc30 is built from.
float Ramp(float x, float lo, float hi, float span) {
  if (lo >= x) return 0.0f;
  if (x >= hi) return 1.0f;
  return (x - lo) / span;
}

// 0x1406ebf30: Range(slot 2) mod 100 < threshold, threshold = int(pSign*100)
// (FUN_1408398d5 passes the record's pSign that way). +1 on success.
constexpr int kPercent = 100;
float SignRoll(Random& rng, float pSign) {
  const int threshold = static_cast<int>(pSign * static_cast<float>(kPercent));
  return rng.Range(kKickRngSlot) % kPercent < threshold ? 1.0f : -1.0f;
}

// Normal's 0..99 back to the |N - 50| / 50 every caller computes.
float HalfNormal(const KickErrorConstants& k, int n) {
  return std::fabs(static_cast<float>(n) - k.normalCentre) / k.normalCentre;
}

// The situation product Q and the motion-descriptor factors F1/F2
// (scatter 6) keys on the PES kick kind (ap+0x2752, 151-row table 0x14344e380: class byte 13, subclass byte 12),
// the first-time/volley flags (ap+0x2756, ap+0x2764), the body turn (ap+0xbe4 - ap+0x247c), the AI bit (ap+0x2460
// bit 29) and the motion descriptor F1/F2 (0x1406b0250 over the motion table, a runtime singleton at 0x1436f23c0).
// ponytail: GF has none of these fields (the kind bridge is open, docs/re/pes-re-kick-scatter-inputs.md Unresolved 1):
// Q = 1 until the bridge lands. Remove when GF kicks carry a PES kind and motion id.
constexpr float kSituationNotPorted = 1.0f;

// The install's quarter-wave sine table (513 floats at 0x142655950), read at load and mapped into the translated sine.
constexpr uint64_t kSinTableVa = 0x142655950;

}  // namespace

std::optional<KickErrorConstants> LoadKickErrorConstants(const PeImage& exe, std::string* error) {
  KickErrorConstants k{};
  bool ok = true;
  const std::pair<uint64_t, float*> rdata[] = {
      {0x14259be2c, &k.qFloorStat},     {0x14259a764, &k.qMidStat},
      {0x1425c594c, &k.qTopStat},       {0x142588200, &k.qLowGain},
      {0x14259be1c, &k.qLowSpan},       {0x1425ab120, &k.qMidGain},
      {0x14259a760, &k.qMidSpan},       {0x1425a8a28, &k.qTopGain},
      {0x142592228, &k.qTopQuarter},    {0x1425ab124, &k.qTopBase},
      {0x14259222c, &k.skillBonus},     {0x14259d618, &k.angleScale_deg},
      {0x142590d98, &k.speedScale},     {0x1425ef9f8, &k.ampGain},
      {0x14259bdd4, &k.ampBase},        {0x142565600, &k.half},
      {0x142592228, &k.aBase},          {0x14259a764, &k.minute_s},
      {0x14256ae34, &k.km_m},           {0x142588210, &k.rampPerQuality},
      {0x14259a764, &k.rampLo_kmh},     {0x14259be58, &k.groundHi_kmh},
      {0x142596e6c, &k.otherHi_kmh},    {0x1425ab124, &k.wQuality},
      {0x142590d98, &k.wGround},        {0x1425e4a30, &k.wOther},
      {0x14259be1c, &k.pitchLo_deg},    {0x142591140, &k.pitchHi_deg},
      {0x142598578, &k.powerLo},        {0x142572348, &k.powerSpan},
      {0x142592228, &k.shootPowerWeight}, {0x14259b89c, &k.rWeight},
      {0x142572348, &k.dBase},          {0x1425a946c, &k.contactLo},
      {0x14259be2c, &k.contactSpan},    {0x14259be1c, &k.contactHi},
      {0x14256ae1c, &k.nearOpp_m},      {0x142565604, &k.farOpp_m},
      {0x14259bddc, &k.dWeight},        {0x1426550e4, &k.normalDiv},
      {0x14259be34, &k.normalCentre},   {0x1426550dc, &k.normalTabScale},
      {0x14259a768, &k.normalNegTwo},   {0x1426550e8, &k.normalTabSteps},
      {0x14256ae34, &k.normalDraw},     {0x14259858c, &k.fullTurn_deg},
      {0x14259be5c, &k.quarterTurn_deg}};
  for (auto [va, out] : rdata)
    ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  constexpr uint64_t LOG_TABLE_VA = 0x1426548c0;
  ok &= exe.ReadVa(LOG_TABLE_VA, sizeof k.logTable, reinterpret_cast<uint8_t*>(k.logTable.data()));
  ok &= exe.ReadVa(kSinTableVa, sizeof k.sinTable, reinterpret_cast<uint8_t*>(k.sinTable.data()));
  if (!ok) {
    *error = "kick error constant: .rdata unreadable";
    return std::nullopt;
  }
  auto penalty = LoadPenaltyScatterConstants(exe, error);
  if (!penalty) return std::nullopt;
  k.penaltyScatter = *penalty;
  return k;
}

float KickQuality(const KickErrorConstants& k, float s) {
  float q;
  if (k.qMidStat > s)
    q = (s - k.qFloorStat) * k.qLowGain / k.qLowSpan;
  else if (k.qTopStat > s)
    q = (s - k.qMidStat) * k.qMidGain / k.qMidSpan + k.qLowGain;
  else
    q = (s - k.qTopStat) * k.qTopGain * k.qTopQuarter + k.qTopBase;
  return std::max(0.0f, q);
}

float SoftMin4(float a, float b, float c, float d) {
  // FUN_1408413c0: qsort descending (comparator 0x14083cc10 is -1 when a > b),
  // then fold from x = 1.0 once per factor: x = (max*min + min) * 0.5 with
  // min/max of (x, v). The 1.0 is 0x14256ae1c, the 0.5 is 0x142565600.
  constexpr float kFoldStart = 1.0f;
  constexpr float kFoldHalf = 0.5f;
  float v[4] = {a, b, c, d};
  std::sort(v, v + 4, std::greater<float>());
  float x = kFoldStart;
  for (const float f : v) {
    const float lo = std::min(x, f);
    const float hi = std::max(x, f);
    x = (hi * lo + lo) * kFoldHalf;
  }
  return x;
}

// 0x140a92c80: sin of `deg` in degrees, times `amp`. Runs PES's own translated function
// (ball/pes_fn_140A92C80.cpp: xmm0 = deg, xmm1 = amp, result in xmm0). The translated code reads
// the quarter-wave table through its memory view, so the install's table is mapped as that window.
float SinTableDeg(const KickErrorConstants& k, float deg, float amp) {
  PesState s;
  s.mem.window = {kSinTableVa, sizeof k.sinTable, reinterpret_cast<const uint8_t*>(k.sinTable.data()),
                  nullptr};
  s.set_x_f32(0, deg);
  s.set_x_f32(1, amp);
  ::pes_fn_140A92C80(s);
  return s.x_f32(0);
}

int Normal(Random& rng, const KickErrorConstants& k, float amp) {
  constexpr uint32_t kDrawModulus = 1000;  // 0x10624dd3 >> 6 and imul 0x3e8
  constexpr int kLast = 99;                // cmp eax, 0x63 ; jbe
  // Both offered angles rejected: PES jumps back to redraw the pair
  // (0x140a9275f), so this loops until one lands.
  for (;;) {
    const uint32_t s1 = rng.Step(kKickRngSlot);
    const uint32_t s2 = rng.Step(kKickRngSlot);
    const float angle = static_cast<float>(s2 % kDrawModulus + 1) / k.normalDraw * k.fullTurn_deg;
    const float t = static_cast<float>(s1 % kDrawModulus + 1) / k.normalDraw * k.normalTabSteps;
    const int idx = static_cast<int>(t);
    const float v = idx >= kNormalTableSize - 1
                        ? 0.0f
                        : k.logTable[idx] + (k.logTable[idx + 1] - k.logTable[idx]) *
                                                (t - static_cast<float>(idx));
    // sqrt(-2 ln u) * amp / sqrt(-2 ln (1/512)): Box-Muller scaled so the
    // table's far end is `amp` (scatter 2).
    const float g = std::sqrt(v * k.normalTabScale * k.normalNegTwo) * amp / k.normalDiv;
    for (int attempt = 0; attempt < 2; ++attempt) {
      const float a = SinTableDeg(k, angle + (attempt != 1 ? k.quarterTurn_deg : 0.0f), 1.0f);
      const float x = (1.0f + a * g) * k.normalCentre;
      const int res = static_cast<int>((x < 0.0f ? -k.half : k.half) + x);
      if (res >= 0 && res <= kLast) return res;
    }
  }
}

float PassLaunchPenalty(const KickErrorConstants& k, float a, float speed_mps, float pitch_deg,
                        bool groundPass) {
  const float kmh = speed_mps * k.minute_s * k.minute_s / k.km_m;
  const float lo = a * k.rampPerQuality + k.rampLo_kmh;
  const float hi = a * k.rampPerQuality + (groundPass ? k.groundHi_kmh : k.otherHi_kmh);
  float ramp = Ramp(kmh, lo, hi, hi - lo);
  const float w = std::max(0.0f, 1.0f - a * k.wQuality) * (groundPass ? k.wGround : k.wOther);
  // The elevation ramp (ap+0x2754 == 0 branch).
  ramp = std::max(ramp, Ramp(pitch_deg, k.pitchLo_deg, k.pitchHi_deg, k.rampPerQuality));
  return 1.0f - ramp * w;
}

float ShotLaunchPenalty(const KickErrorConstants& k, float power) {
  // ponytail: the -0.02*g pitch term (0x14083dd17..dd27; 0.02 at 0x1425c4894, 0.25 at 0x142592228)
  // is left out. g = SinTableDeg(90*x, xmm12) with x = 0.25*(xmm14-xmm1)
  // when xmm14 < xmm7, else xmm12. xmm14, xmm7, xmm1 are set before 0x14083da40 and are
  // not traced. Moves I by at most 0.02.
  return 1.0f - k.shootPowerWeight * Ramp(power, k.powerLo, 1.0f, k.powerSpan);
}

float PressureFactor(const KickErrorConstants& k, float contactSelf, float contactOpp,
                     float oppDistance_m) {
  if (oppDistance_m >= k.farOpp_m) return 1.0f;
  // 0x14043b8b0's distance ramp: 1 inside a metre, 0 at two.
  const float distRamp = oppDistance_m <= k.nearOpp_m ? 1.0f : k.farOpp_m - oppDistance_m;
  const float edge = Ramp(contactSelf - contactOpp, k.contactLo, k.contactHi, k.contactSpan);
  // ponytail: xmm8 (contact-byte factor, 0x14083ecc9..0x14083ed32; KickDecode (d)): obj = 0x140a48910(FUN_1406ea340(kicker));
  // byte[obj+1]==0x48 gives 1.0 (0x14083ecd4); else B = byte[obj+0x2c] maps to t = 1 for B <= 9, 0 for B >= 54, else
  // (54-B)/45 (9 = 54/6 by the magic divide, 54 = int 0x14351AE50 set to 0x36 by 0x141e532f0). The angRamp is xmm7:
  // clamp((err+20)/40)*0.4, err = byte[obj2+0x28] - byte[r13+0x28] (0x14083ed40..ed87). GF has no anime or player
  // record for either: xmm8 is 1.0 here and the xmm7 angRamp is omitted.
  const float d = (k.dBase - k.rWeight * edge) * distRamp;
  // ponytail: FUN_140723320 (run term, 3-pass running max of 0.1*sum; weights 0x14344ef78..0x14344ef94 and
  // 0x14344ef98/9c/a0 = -0.0029828, 2.7572925, 8.7184591; case affine from byte[r15+1]-8 via table 0x14072394c)
  // needs the kicker's contact vector and the state id of PES's player record. GF has neither: run = 0.
  return 1.0f - k.rWeight * (k.dWeight * d);
}

KickScatter BuildScatter(const KickErrorConstants& k, float a, float launchPenalty,
                         float pressure) {
  const float q = kSituationNotPorted;
  const float qk = k.half * q + (1.0f - k.half);       // Qk, k = 0.5 by default
  const float qs = k.half * q + k.half;                // 0.5Q + 0.5
  const float stat = a * k.ampBase + k.aBase;          // A = 0.75a + 0.25
  const float f = kSituationNotPorted;
  auto soft = [&](float x) { return SoftMin4(launchPenalty, x, f, pressure); };
  auto record = [&](float scale, float qScale, float qAmp) {
    const float m = 1.0f - qAmp;
    return ScatterRecord{(1.0f - qScale) * scale, k.half, m * m * k.ampGain + k.ampBase};
  };
  return {record(k.angleScale_deg, soft(q), soft(stat * q)),
          record(k.angleScale_deg, soft(qk), soft(stat * qk)),
          record(k.speedScale, soft(qs), soft(stat * qs))};
}

KickError DrawKickError(const KickErrorConstants& k, Random& rng, const KickScatter& s,
                        bool halveNegativePitch) {
  KickError e;
  // ponytail: the yaw redraw loop (0x14083a550, <=6 draws) is skipped; it
  // only re-rolls a yaw that matches a sign test on untraced fields.
  float h = HalfNormal(k, Normal(rng, k, s.yaw.amp));
  e.yaw_deg = SignRoll(rng, s.yaw.pSign) * s.yaw.scale * h;
  h = HalfNormal(k, Normal(rng, k, s.pitch.amp));
  const float pitchSign = SignRoll(rng, s.pitch.pSign);
  e.pitch_deg = pitchSign * s.pitch.scale * h;
  if (pitchSign < 0.0f && halveNegativePitch) e.pitch_deg *= k.half;
  h = HalfNormal(k, Normal(rng, k, s.speed.amp));
  e.speedFactor = 1.0f + SignRoll(rng, s.speed.pSign) * s.speed.scale * h;
  return e;
}

void ApplyKickError(const KickError& e, float v[3]) {
  constexpr float kDegToRad = 3.14159265f / 180.0f;
  const float horizontal = std::sqrt(v[0] * v[0] + v[1] * v[1]);
  const float speed = std::sqrt(horizontal * horizontal + v[2] * v[2]) * e.speedFactor;
  const float heading = std::atan2(v[1], v[0]) + e.yaw_deg * kDegToRad;
  const float pitch = std::atan2(v[2], horizontal) + e.pitch_deg * kDegToRad;
  v[0] = speed * std::cos(pitch) * std::cos(heading);
  v[1] = speed * std::cos(pitch) * std::sin(heading);
  v[2] = speed * std::sin(pitch);
}

}  // namespace pes
