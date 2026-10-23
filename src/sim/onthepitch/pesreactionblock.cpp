#include "onthepitch/pesreactionblock.hpp"

#include <cmath>
#include <cstring>

#include "pes/exe.hpp"

namespace pes {
namespace reactionblock {
namespace {

constexpr uint64_t kVaSineTable = 0x142655950;
constexpr uint64_t kVaClassTable = 0x1426484f0;
constexpr size_t kSineCount = 513;  // quarter wave, 512 steps: index 0..512
constexpr size_t kClassCount = 94;  // FUN_140a6e1c0 & co test classes 0..0x5d
constexpr uint32_t kLastSlot = 21;  // slots 0..21: starters 0..10, bench 11..21
// The atan core's branch bounds, the bit patterns 0x3ee00000, 0x3f980000 and
// 0x401c0000 that FUN_140a92dc0 compares |r| against (0x140a92dd0, 0x140a92e01,
// 0x140a92e25).
constexpr float kAtanSmall = 0.4375f;
constexpr float kAtanMidLow = 1.1875f;
constexpr float kAtanMidHigh = 2.4375f;
// The sine table's last index passed to the lookup: idx > 0x1ff returns the
// signed amplitude (FUN_140a92c80 0x140a92cff).
constexpr uint32_t kSineLastIndex = 0x1ffu;

bool ReadFloat(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[sizeof(float)];
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  std::memcpy(out, bytes, sizeof *out);
  return std::isfinite(*out);
}

bool LoadConstants(const PeImage& exe, Constants& k, std::string* error) {
  const struct {
    uint64_t va;
    float* field;
  } table[] = {
      {0x14259858c, &k.turnPeriod},    {0x142598598, &k.turnPeriodNeg},
      {0x142598580, &k.halfTurn},      {0x1425ddffc, &k.halfTurnNeg},
      {0x14259be5c, &k.quarterTurn},   {0x1426550e8, &k.tableScale},
      {0x14259be40, &k.degPerRad},     {0x1425795e4, &k.twoPiRad},
      {0x14256ae1c, &k.one},           {0x142588218, &k.minusOne},
      {0x142565600, &k.half},          {0x1425903b4, &k.atanCubic},
      {0x142656158, &k.piOver4},       {0x14265615c, &k.atanMidBase},
      {0x1425a35a0, &k.piOver2},       {0x14260b394, &k.minusPiOver2},
      {0x142578480, &k.pi},            {0x142609f98, &k.minusPi},
      {0x14259bddc, &k.atanSlope},     {0x14259bdc4, &k.closestEps},
      {0x14259d624, &k.drawFloat},     {0x14259bdd0, &k.speedScale},
      {0x14259a764, &k.speedK1},       {0x14256ae34, &k.speedK2},
      {0x14259be0c, &k.ramp1Low},      {0x14259be1c, &k.ramp1Top},
      {0x14259222c, &k.ramp1Step},     {0x142588210, &k.ramp2Low},
      {0x1425c48c0, &k.ramp2High},     {0x14259be04, &k.ramp2Div},
      {0x14259bde8, &k.forcedScale},   {0x14259bdf0, &k.forcedLow},
      {0x14259be34, &k.p5Bound},       {0x14259be30, &k.plainBound},
      {0x142590d98, &k.block0Low},     {0x1425c48ac, &k.block0Div},
      {0x14259d5d4, &k.block0High},    {0x14259be4c, &k.tailFull},
      {0x14259be64, &k.tailZero},      {0x1425c4900, &k.tailDiv},
      {0x142598578, &k.tailGate},      {0x14259b89c, &k.dlScale},
      {0x14259be20, &k.pFull},         {0x1425a946c, &k.pDiv},
  };
  for (const auto& entry : table) {
    if (!ReadFloat(exe, entry.va, entry.field)) {
      if (error != nullptr) {
        *error = "block-B constant unreadable at VA 0x" + std::to_string(entry.va);
      }
      return false;
    }
  }
  // FUN_141e532f0 stores the same 54 in both cells (DAT_14351ae54 = 54.0f and
  // DAT_14351ae50 = 0x36), so the int cell is the float cell's value.
  k.drawInt = static_cast<int>(k.drawFloat);
  return true;
}

// The turn-angle wrap (FUN_1404a53e0, FUN_140481b00, FUN_140503430 0x140503555):
// x >= 0 keeps x or takes x - 360 when x >= 360; x < 0 takes x + 360.
float TurnWrap(float x, const Constants& k) {
  if (x >= 0.0f) {
    if (x >= k.turnPeriod) return x + k.turnPeriodNeg;
    return x;
  }
  return x + k.turnPeriod;
}

// The sine's wrap (FUN_140a92c80 0x140a92c86): x > 0 keeps x or takes x - 360
// when x >= 360; x <= 0 takes x + 360 (so 0 becomes 360).
float SineWrap(float x, const Constants& k) {
  if (0.0f < x) {
    if (x >= k.turnPeriod) return x + k.turnPeriodNeg;
    return x;
  }
  return x + k.turnPeriod;
}

// |d| > 180 moves d by sgn(d) * 360 (sgn(0) = +1), as FUN_140503430 0x140503d6b
// and FUN_140481b00 0x140481b39 do.
float WrapAdjust(float d, const Constants& k) {
  if (!(std::fabs(d) <= k.halfTurn)) {
    const float sgn = (0.0f <= d) ? k.one : k.minusOne;
    return d - sgn * k.turnPeriod;
  }
  return d;
}

}  // namespace

bool ClassTable::Bit(int cls, int bit) const {
  if (cls < 0 || static_cast<size_t>(cls) >= kClassCount) return false;
  return ((words[static_cast<size_t>(cls)] >> bit) & 1u) != 0u;
}

std::optional<Tables> LoadTables(const PeImage& exe, std::string* error) {
  Tables t;
  if (!LoadConstants(exe, t.k, error)) return std::nullopt;
  for (size_t i = 0; i < kSineCount; ++i) {
    if (!ReadFloat(exe, kVaSineTable + 4u * i, &t.sine[i])) {
      if (error != nullptr) *error = "sine table unreadable";
      return std::nullopt;
    }
  }
  // The shape check: a quarter wave from 0 to 1.
  if (t.sine[0] != 0.0f || std::fabs(t.sine[kSineCount - 1] - 1.0f) > 1e-6f) {
    if (error != nullptr) *error = "sine table has no quarter-wave shape";
    return std::nullopt;
  }
  uint8_t words[4 * kClassCount];
  if (!exe.ReadVa(kVaClassTable, sizeof words, words)) {
    if (error != nullptr) *error = "anime class table unreadable";
    return std::nullopt;
  }
  for (size_t c = 0; c < kClassCount; ++c) {
    std::memcpy(&t.classes.words[c], words + 4 * c, sizeof(uint32_t));
  }
  return t;
}

float Sin(float theta, float s, const Tables& t) {
  const Constants& k = t.k;
  float x = SineWrap(theta, k);
  float sign = k.one;
  if (!(x < k.halfTurn)) {  // 0x140a92cb1: x >= 180 flips the sign and drops 180
    sign = k.minusOne;
    x = x + k.halfTurnNeg;
  }
  if (!(x < k.quarterTurn)) x = k.halfTurn - x;  // 0x140a92cd9: reflect about 90
  const float idxF = (x / k.quarterTurn) * k.tableScale;
  const float signedS = sign * s;
  const int idx = static_cast<int>(idxF);
  if (static_cast<uint32_t>(idx) > kSineLastIndex) return signedS;
  const float frac = idxF - static_cast<float>(idx);
  const float lo = (k.one - frac) * t.sine[static_cast<size_t>(idx)];
  const float hi = frac * t.sine[static_cast<size_t>(idx) + 1];
  return (lo + hi) * signedS;
}

float AtanCore(float r, const Tables& t) {
  const Constants& k = t.k;
  const float a = std::fabs(r);
  if (a < kAtanSmall) return r - ((r * r) * k.atanCubic) * r;
  float base;
  float num;
  float den;
  if (a < kAtanMidLow) {
    base = k.piOver4;
    num = a - k.one;
    den = a + k.one;
  } else if (a < kAtanMidHigh) {
    base = k.atanMidBase;
    num = a - k.atanSlope;
    den = (a * k.atanSlope) + k.one;
  } else {
    base = k.piOver2;
    num = k.minusOne;
    den = a;
  }
  const float u = num / den;
  const float inner = (((u * u) * k.atanCubic) * u) - u;
  const float v = base - inner;
  return std::signbit(r) ? -v : v;
}

float Atan2Rad(float x, float z, const Tables& t) {
  const Constants& k = t.k;
  if (z == 0.0f) return x > 0.0f ? k.piOver2 : k.minusPiOver2;
  float r = AtanCore(x / z, t);
  if (z < 0.0f) r = r + ((0.0f <= x) ? k.pi : k.minusPi);
  return r;
}

float AngleOf(float x, float z, const Tables& t) {
  const Constants& k = t.k;
  if (z == 0.0f && x == 0.0f) return 0.0f;
  float rad = Atan2Rad(x, z, t);
  if (rad < 0.0f) rad = rad + k.twoPiRad;
  return TurnWrap(rad * k.degPerRad, k);
}

namespace {

// FUN_140a8f7c0: p.x += sin(theta) * s and p.z += sin(theta + 90) * s.
void Displace(Vec3& p, float theta, float s, const Tables& t) {
  p.x = Sin(theta, s, t) + p.x;
  p.z = Sin(theta + t.k.quarterTurn, s, t) + p.z;
}

// FUN_140503430 0x140503aee-0x140503b2a: the DL flag of the newest event.
bool EventDl(const Block9Inputs& in) {
  if (in.kind == 2u) return in.byte45 != 0;
  if (in.kind == 3u) {
    const uint32_t d = static_cast<uint32_t>(in.sub) - 3u;
    if (d > 1u) return true;
    return in.byte46 == 0;
  }
  return false;
}

}  // namespace

Vec3 Motion(const PlayerFacts& r, float s, const Tables& t) {
  bool disp = r.mode == 1;
  if (!disp && r.mode == 5) {
    const float speed2 = (r.vel.z * r.vel.z) + (r.vel.x * r.vel.x);
    if ((s * s) > speed2) disp = true;
  }
  if (!disp) return r.vel;
  Vec3 p;
  Displace(p, r.facing, s, t);
  return p;
}

bool ClosestT(const Vec3& a, const Vec3& b, const Vec3& c, float eps, float* t) {
  const float dx = b.x - a.x;
  const float dz = b.z - a.z;
  const float len2 = (dz * dz) + (dx * dx);
  if (!(eps <= len2)) return false;
  // The +0 terms of the asm (C.y * 0, the y of the reference) change no value.
  *t = ((dx * c.x) + (dz * c.z) - ((dx * a.x) + (dz * a.z))) / len2;
  return true;
}

Block9Result ComputeBlock9(const Block9Inputs& in, const Tables& t) {
  const Constants& k = t.k;
  Block9Result out;
  std::array<float, 5>& b = out.block;

  // 0x140503555-0x1405035c4: the forced flag, from the slot's action.
  bool forced = false;
  if (in.live.action == 0x34) {
    forced = !in.gate34SideOk || in.gate34SlotTaken;
  } else if (in.live.action == 0x35) {
    forced = true;
  }

  // The motion vector at c0 / 54 (0x1405035e3-0x1405035ff). The first and the
  // second projection are the same call, so they share one vector.
  const Vec3 q = Motion(in.live, k.speedScale / k.drawFloat, t);
  const float qLen = std::sqrt((q.z * q.z) + (q.x * q.x));
  const int r15 = static_cast<int>(k.drawFloat + k.half);
  const float sLen = static_cast<float>(r15) * qLen;

  // theta: the angle of the motion vector (0x140503651-0x14050368c), in degrees.
  float theta = 0.0f;
  if (q.z != 0.0f || q.x != 0.0f) {
    float rad = Atan2Rad(q.x, q.z, t);
    if (rad < 0.0f) rad = rad + k.twoPiRad;
    theta = TurnWrap(rad * k.degPerRad, k);
  }

  // The displaced point P' (0x1405036cf-0x1405036d4 via FUN_140a8f7c0), and the
  // closest point of the anchor on the segment live -> P'.
  Vec3 p2 = in.live.pos;
  Displace(p2, theta, sLen, t);
  float tc = 0.0f;
  const bool ok = ClosestT(in.live.pos, p2, in.anchor, k.closestEps, &tc);

  // block[0] (0x140503707-0x1405037aa): 1 when the segment is degenerate, the
  // action is forced or 0x33; else the distance ramp of the anchor to the closest point.
  if (!ok || forced || in.live.action == 0x33) {
    b[0] = k.one;
  } else {
    const float qx = ((p2.x - in.live.pos.x) * tc) + in.live.pos.x;
    const float qz = ((p2.z - in.live.pos.z) * tc) + in.live.pos.z;
    const float dx = in.anchor.x - qx;
    const float dz = in.anchor.z - qz;
    const float dd = std::sqrt((dz * dz) + (dx * dx));
    if (!(k.block0Low < dd)) {
      b[0] = 0.0f;
    } else if (!(dd < k.block0High)) {
      b[0] = k.one;
    } else {
      b[0] = (dd - k.block0Low) / k.block0Div;
    }
  }

  // The holder's substitute (0x140503812-0x14050383b): when the holder is not on
  // the pitch and the slot is, the side's team slot stands in for it.
  int sub = in.holder;
  if (static_cast<uint32_t>(sub) > kLastSlot && static_cast<uint32_t>(in.slot) <= kLastSlot) {
    sub = in.teamSubSlot;
  }
  const bool subValid = static_cast<uint32_t>(sub) <= kLastSlot;

  // The card's class flags (0x14050384f-0x140503888): flag80 is the class bit 4;
  // R13 is the class bit 5 with byte +0x38 in {3, 4} when bit 4 is clear.
  bool flag80 = false;
  bool r13 = false;
  if (subValid && in.cardSub) {
    const CardFacts& c = *in.cardSub;
    flag80 = t.classes.Bit(c.cls, 4);
    if (!flag80 && t.classes.Bit(c.cls, 5)) {
      r13 = static_cast<uint8_t>(c.b38 - 3) <= 1;
    }
  }

  // The variant: p5 or R13 takes the second ramp (0x14050388f-0x1405038a5).
  const bool variantTwo = in.p5 || r13;
  const float lv = (((k.drawFloat * k.speedK1) * k.speedK1) / k.speedK2) * qLen;
  const float tbl = in.reach;

  if (!variantTwo) {
    // Variant (i): the first ramp over Lv, then block[2] with F as the branch.
    float r1 = 0.0f;
    if (k.ramp1Low < lv) r1 = (lv < k.ramp1Top) ? (lv - k.ramp1Low) / k.ramp1Step : k.one;
    b[1] = r1;
    if (!forced) {
      if (!(r1 < tbl)) {
        b[2] = k.one;
      } else {
        const float s2 = (r1 + r1) + k.ramp1Step;
        b[2] = (tbl < s2) ? (tbl - s2) / (r1 - s2) : 0.0f;
      }
    } else {
      const float rp = (r1 * k.forcedScale) + k.atanSlope;
      if (!(rp < tbl)) {
        b[2] = k.one;
      } else {
        b[2] = (tbl < k.forcedLow) ? (tbl - k.forcedLow) / (rp - k.forcedLow) : 0.0f;
      }
    }
  } else {
    // Variant (ii): the second ramp, then block[2] over the band s2 .. s1.
    float r2 = 0.0f;
    if (k.ramp2Low < lv) r2 = (lv < k.ramp2High) ? (lv - k.ramp2Low) / k.ramp2Div : k.one;
    b[1] = r2;
    const float s2 = (r2 + r2) + k.ramp1Step;
    const float s1 = (r2 * k.ramp1Step) + k.ramp1Top;
    if (!(s2 < tbl)) {
      b[2] = k.one;
    } else {
      b[2] = (tbl < s1) ? (tbl - s1) / (s2 - s1) : 0.0f;
    }
  }

  // block[4], the p5 part (0x140503b2d-0x140503bb6): the reach gate times 54 * 0.3,
  // overridden by 54 * 0.4 when the event's DL holds.
  if (in.p5) {
    float g;
    if (!(in.s4 < tbl)) {
      g = k.one;
    } else {
      g = (tbl < in.s5) ? (tbl - in.s5) / (in.s4 - in.s5) : 0.0f;
    }
    const float scaled = static_cast<float>(static_cast<int>(static_cast<float>(k.drawInt) * k.block0Low));
    b[4] = scaled * g;
    if (EventDl(in)) {
      b[4] = static_cast<float>(static_cast<int>(static_cast<float>(k.drawInt) * k.dlScale));
    }
  }

  // block[4], the p6 part (0x140503bbb-0x140503c38): 5 * h(reach), kept if larger.
  if (in.p6) {
    float h;
    if (!(k.ramp1Step < tbl)) {
      h = k.one;
    } else {
      h = (tbl < k.pFull) ? (tbl - k.pFull) / k.pDiv : 0.0f;
    }
    const float v = h * k.ramp1Step;
    if (v > b[4]) b[4] = v;
  }

  // The angle factor block[3] (0x140503c3d-0x140503e13): the team angle, the card's
  // angle when the card path is taken, and the gap of the velocity angle.
  const float xTM = TurnWrap(in.teamAngleRaw, k);
  const float a1 = AngleOf(in.ballV1.x, in.ballV1.z, t);
  const float a2 = AngleOf(in.ballV2.x, in.ballV2.z, t);
  float x6 = a2;
  if (subValid && in.cardSub && (flag80 || r13)) {
    const CardFacts& c = *in.cardSub;
    const bool useSixty = flag80 && !c.bit11AtEight && !(c.w1a > c.w1c) &&
                          !(k.halfTurn < std::fabs(c.f60));
    x6 = TurnWrap(useSixty ? c.f60 : c.f11c, k);
  }
  const float dA = std::fabs(WrapAdjust(x6 - a1, k));

  // T2 (0x140503d95-0x140503dbb): the bound from p5, or zero when the holder's card
  // is class-5 with byte +0x40 == 5 (0x140503d97-0x140503db9).
  bool block3Zero = false;
  float t2 = k.plainBound;
  if (in.p5) {
    t2 = k.p5Bound;
  } else if (in.cardHolder && t.classes.Bit(in.cardHolder->cls, 5) && in.cardHolder->b40 == 5) {
    block3Zero = true;
  }
  float block3 = 0.0f;
  if (!block3Zero) {
    const float r1 = b[1];
    const float hh = k.ramp1Top - (r1 * k.ramp1Step);
    const float tt2 = t2 - (r1 * k.ramp1Low);
    if (hh < dA) {
      block3 = (dA < tt2) ? (dA - hh) / (tt2 - hh) : k.one;
    } else {
      block3 = 0.0f;
    }
  }
  b[3] = block3;

  // The tail (0x140503e18-0x140503fe7): the holder's facing, the table gap, and the
  // sign-matched factor. Skipped when forced or when the holder is not on the pitch.
  if (in.liveHolder && !forced) {
    const float wTb = TurnWrap(in.holderSlotTable, k);
    const float rb = std::fabs(WrapAdjust(wTb - in.liveHolder->facing, k));
    if (k.speedK1 <= rb) {
      const bool al2 = k.tailZero <= rb;
      const float yAbs = std::fabs(WrapAdjust(x6 - xTM, k));
      float q;
      if (!(k.tailFull < yAbs)) {
        q = k.one;
      } else {
        q = (yAbs < k.tailZero) ? (yAbs - k.tailZero) / k.tailDiv : 0.0f;
      }
      if (al2 && q < k.tailGate) {
        const float e1 = WrapAdjust(TurnWrap(in.holderSlotTable, k) - xTM, k);
        const float s1 = (0.0f <= e1) ? k.one : k.minusOne;
        const float s0 = AngleOf(in.anchor.x - in.ballSample0.x, in.anchor.z - in.ballSample0.z, t);
        const float e2 = WrapAdjust(s0 - xTM, k);
        const float s2 = (0.0f <= e2) ? k.one : k.minusOne;
        if (s1 == s2) q = k.tailGate;
      }
      b[3] = q * b[3];
    }
  }
  return out;
}

}  // namespace reactionblock
}  // namespace pes
