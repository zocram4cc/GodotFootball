// PES 2021's presser picker arithmetic (docs/re/pes-re-press-layer-port.md).
// See pespress.hpp for what each function is and where its constants live.

#include "onthepitch/pespress.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pes {
namespace {

// A .rdata float: PES keeps every one of these as a plain float.
bool ReadFloat(const PeImage& exe, uint64_t va, float* out) {
  return exe.ReadVa(va, sizeof(float), reinterpret_cast<uint8_t*>(out));
}

struct PressVa {
  uint64_t va;
  float PressConstants::*field;
};

constexpr PressVa kPressVas[] = {
    {0x14253ee30, &PressConstants::rampStart_deg},
    {0x14259be6c, &PressConstants::rampEnd_deg},
    {0x14259a764, &PressConstants::rampSpan_deg},
    {0x142588210, &PressConstants::radiusBase_m},
    {0x142598580, &PressConstants::halfTurn_deg},
    {0x14259858c, &PressConstants::fullTurn_deg},
    {0x14259be40, &PressConstants::radToDeg},
    {0x14259d634, &PressConstants::notSlotPenalty},
    {0x1425cee24, &PressConstants::notRolePenalty},
    {0x14259be18, &PressConstants::carrierRampStart_m},
    {0x14259be20, &PressConstants::carrierRampEnd_m},
    {0x14259bdf0, &PressConstants::carrierRampSpan_m},
    {0x14258c264, &PressConstants::pass2Lo},
    {0x14259bdec, &PressConstants::pass2Hi},
    {0x14259d654, &PressConstants::pass2Div},
    {0x14259be0c, &PressConstants::offsetBase_deg},
    {0x1425a1ff8, &PressConstants::offsetSpan_deg},
    {0x14259be38, &PressConstants::slot1Lo_deg},
    {0x1425cb488, &PressConstants::slot1Hi_deg},
    {0x14259be30, &PressConstants::lowBand_deg},
    {0x14259be60, &PressConstants::highBand_deg},
    {0x1425c48d4, &PressConstants::slot2Band_deg},
    {0x1425c6608, &PressConstants::slot3Lo_deg},
    {0x1425cee28, &PressConstants::slot3Hi_deg},
    {0x14259a764, &PressConstants::secPerMinute},
    {0x14256ae34, &PressConstants::metresPerKm},
    {0x14351ae54, &PressConstants::framesPerSec},
    {0x1425a9464, &PressConstants::pass2DelayDistSq},
    {0x1425c6780, &PressConstants::pass2ZeroRoleDistSq},
    {0x14259bdf4, &PressConstants::pass2SameCodeAdd},
    {0x1425656c0, &PressConstants::pass2ActionAdd},
    {0x1425a8a28, &PressConstants::speedCut0_kmh},
    {0x14259bdf4, &PressConstants::speedCut1_kmh},
    {0x1425a1ffc, &PressConstants::speedCut2_kmh},
    {0x1425c48c0, &PressConstants::speedCut3_kmh},
    {0x142588200, &PressConstants::targetTickRate},
    {0x142565600, &PressConstants::targetTickBias},
    {0x142588210, &PressConstants::evalThetaBase_m},
    {0x1425c48c0, &PressConstants::evalThetaFar_m},
    {0x14259be0c, &PressConstants::evalThetaZ_m},
    {0x1425c5948, &PressConstants::evalThetaB29_m},
    {0x14258c264, &PressConstants::evalIncumbentTheta_m},
    {0x14259222c, &PressConstants::evalIncumbentStep_m},
    {0x14259222c, &PressConstants::evalBlockASpeedKmh},
    {0x14259a764, &PressConstants::evalBlockAHeading_deg},
    {0x142588210, &PressConstants::evalBlockAAdd_m},
    {0x14259222c, &PressConstants::evalWindowSec_s},
    {0x14259bdec, &PressConstants::evalThetaTwoLow_m},
    {0x14259be04, &PressConstants::evalThetaTwoHigh_m},
    {0x14259be34, &PressConstants::evalMode7Angle_deg},
    {0x14259be2c, &PressConstants::evalMode7IncAngle_deg},
    {0x14253ee30, &PressConstants::evalMode4Angle_deg},
    {0x142588210, &PressConstants::evalMode4Margin_m},
    {0x14259bde4, &PressConstants::coverNear_m},
    {0x1425c48b8, &PressConstants::coverFar_m},
    {0x14259bdfc, &PressConstants::coverOuter_m},
    {0x14259be64, &PressConstants::coverCone_deg},
    {0x14259a764, &PressConstants::gateFacing_deg},
    {0x14259bddc, &PressConstants::gateBallHeight_m},
    {0x14258c264, &PressConstants::classFiveDist_m},
    {0x14259222c, &PressConstants::classFiveKindTen_m},
    {0x14259d60c, &PressConstants::maintainReason5DistSq_m2},
    {0x14259be30, &PressConstants::maintainBearing_deg},
};

}  // namespace

std::optional<PressConstants> LoadPressConstants(const PeImage& exe, std::string* error) {
  PressConstants c{};
  for (const PressVa& v : kPressVas) {
    if (!ReadFloat(exe, v.va, &(c.*v.field))) {
      if (error) *error = "press constant unreadable";
      return std::nullopt;
    }
  }
  return c;
}

void SortPressBucket(std::vector<PressCandidate>& bucket) {
  // Insertion sort: stable, and the buckets hold at most 11 entries.
  for (size_t i = 1; i < bucket.size(); ++i) {
    PressCandidate cur = bucket[i];
    size_t j = i;
    while (j > 0 && bucket[j - 1].score > cur.score) {
      bucket[j] = bucket[j - 1];
      --j;
    }
    bucket[j] = cur;
  }
}

float PressBearingDeg(const PressConstants& c, float fromX, float fromZ, float toX, float toZ) {
  float dx = toX - fromX;
  float dz = toZ - fromZ;
  if (dx == 0.0f && dz == 0.0f) return 0.0f;
  // ponytail: std::atan2 in place of PES's own atan2 polynomial; the float
  // difference is below what the picker's thresholds can see.
  float deg = std::atan2(dx, dz) * c.radToDeg;
  if (deg < 0.0f) deg += c.fullTurn_deg;
  return deg;
}

float PressWrapDeg(const PressConstants& c, float deg) {
  float r = std::fmod(deg, c.fullTurn_deg);
  if (r < 0.0f) r += c.fullTurn_deg;
  return r;
}

float PressSideAngleDeg(const PressConstants& c, float bearingDeg, float referenceDeg) {
  float d = bearingDeg - referenceDeg;
  if (std::fabs(d) > c.halfTurn_deg) d -= std::copysign(c.fullTurn_deg, d);
  return d;
}

bool PressAngleOutsideBand(const PressConstants& c, float deg, float eps) {
  float w = deg;
  if (std::fabs(w) > c.halfTurn_deg) w -= std::copysign(c.fullTurn_deg, w);
  float a = std::fabs(w);
  return !(eps <= a && a <= c.halfTurn_deg - eps);
}

float PressSpeedKmh(const PressConstants& c, float perFrame) {
  return ((c.framesPerSec * c.secPerMinute * c.secPerMinute) / c.metresPerKm) * perFrame;
}

void PressStampPush(PressStampRing& ring, int id, uint32_t t) {
  if (ring.slots[ring.head].id == id) return;
  if (ring.slots[ring.head].first < t) {
    ring.head = (ring.head + 1) % static_cast<int>(ring.slots.size());
  }
  ring.slots[ring.head] = {id, t, t};
}

constexpr uint64_t kRoleTableVa = 0x1426484f0;

std::optional<PressRoleTable> LoadPressRoleTable(const PeImage& exe, std::string* error) {
  PressRoleTable t{};
  std::array<uint8_t, kRoleCount * sizeof(uint32_t)> raw{};
  if (!exe.ReadVa(kRoleTableVa, raw.size(), raw.data())) {
    if (error) *error = "role table unreadable";
    return std::nullopt;
  }
  for (int i = 0; i < kRoleCount; ++i) {
    std::memcpy(&t.bits[i], raw.data() + i * sizeof(uint32_t), sizeof(uint32_t));
  }
  return t;
}

bool PressRoleHas(const PressRoleTable& t, int role, int bit) {
  if (role < 0 || role >= kRoleCount) return false;
  return (t.bits[role] >> bit) & 1u;
}

float PressCarrierRamp(const PressConstants& c, float carrierDist_m) {
  if (carrierDist_m <= c.carrierRampStart_m) return 0.0f;
  if (carrierDist_m < c.carrierRampEnd_m) {
    return (carrierDist_m - c.carrierRampStart_m) / c.carrierRampSpan_m;
  }
  return 1.0f;
}

float PressOffsetDeg(const PressConstants& c, float root) {
  float ramp;
  if (root <= c.pass2Lo) {
    ramp = 1.0f;
  } else if (root < c.pass2Hi) {
    ramp = (root - c.pass2Hi) / c.pass2Div;
  } else {
    ramp = 0.0f;
  }
  return ramp * c.offsetSpan_deg + c.offsetBase_deg;
}

float PressManScore(const PressConstants& c, float distSq, float angleDeg, int role, int slot) {
  float a = std::fabs(angleDeg);
  float r;
  if (a <= c.rampStart_deg) {
    r = 0.0f;
  } else if (a < c.rampEnd_deg) {
    r = (a - c.rampStart_deg) / c.rampSpan_deg;
  } else {
    r = 1.0f;
  }
  float radius = r * c.radiusBase_m + c.radiusBase_m;
  float coneTerm = c.rampStart_deg;
  if (distSq <= radius * radius) {
    float inner = (1.0f - r) * c.radiusBase_m;
    coneTerm = inner * inner;
  }
  float score = distSq + coneTerm;
  if (role != kPresserRole) score += c.notRolePenalty;
  if (slot < 0) score += c.notSlotPenalty;
  return score;
}

int PressAngleCode(const PressConstants& c, float a, int slot) {
  if (slot == 0) return 0;
  if (slot == 1 && c.slot1Lo_deg < a && a < c.slot1Hi_deg) return 1;
  if (slot == 2 && std::fabs(a) > c.slot2Band_deg) return 2;
  if (slot == 3 && c.slot3Hi_deg < a && a < c.slot3Lo_deg) return 3;
  // Fall-through: the band code on the magnitude and the sign.
  float m = std::fabs(a);
  if (m < c.lowBand_deg) return 0;
  if (m <= c.highBand_deg) return a <= 0.0f ? 3 : 1;
  return 2;
}

namespace {

// Press slot of `man` (team+0xa548 index), or -1.
int PressSlotOf(const std::array<int, 4>& pressSlotMan, int man) {
  for (int i = 0; i < 4; ++i) {
    if (pressSlotMan[i] == man) return i;
  }
  return -1;
}

// Angle code of `man` for the carrier bearing shifted by `offsetDeg`
// (FUN_140639600): bearing ball->man, plus offset, minus the side reference.
int PressManAngleCode(const PressConstants& c, const PressPickInput& in, int man, float offsetDeg,
                      int slot) {
  float bearing = PressBearingDeg(c, in.ballX, in.ballZ, in.posX[man], in.posZ[man]);
  float a = PressSideAngleDeg(c, PressWrapDeg(c, bearing + offsetDeg), in.referenceDeg);
  return PressAngleCode(c, a, slot);
}

}  // namespace
std::vector<PressCandidate> RunPresserPass1(const PressConstants& c, const PressPickInput& in,
                                            int side) {
  std::vector<PressCandidate> out;
  for (int slot = 0; slot < 11; ++slot) {
    const int man = side * 11 + slot;
    if (man >= 22 || !in.onField[man]) continue;
    float dx = in.posX[man] - in.ballX;
    float dz = in.posZ[man] - in.ballZ;
    float distSq = dx * dx + dz * dz;
    float angle = PressSideAngleDeg(
        c, PressWrapDeg(c, PressBearingDeg(c, in.ballX, in.ballZ, in.posX[man], in.posZ[man])),
        in.referenceDeg);
    int pressSlot = PressSlotOf(in.pressSlotMan, man);
    float score = PressManScore(c, distSq, angle, in.role[slot], pressSlot);
    out.push_back({man, score});
  }
  SortPressBucket(out);
  return out;
}


int PressExcludedMan(int side, const std::array<bool, 22>& onField, const std::array<int, 11>& flag) {
  for (int i = 0; i < 11; ++i) {
    const int man = side * 11 + i;
    if (man < 22 && onField[man] && flag[i] == 0) return man;
  }
  return kPressNoMan;
}

std::vector<PressCandidate> RunPresserPicker(const PressConstants& c, const PressPickInput& in,
                                             const std::vector<PressCandidate>& pass1Bucket,
                                             PressPickState& state) {
  // Radius of the pass-2 window: the carrier ramp scales it between the start and span.
  float carrierDist = std::hypot(in.ballX - in.refX, in.ballZ - in.refZ);
  float radius = PressCarrierRamp(c, carrierDist) * c.carrierRampSpan_m + c.carrierRampStart_m;
  float radiusSq = radius * radius;

  std::vector<PressCandidate> out;
  for (const PressCandidate& e : pass1Bucket) {
    const int man = e.man;
    if (man >= 22 || man == in.excludedMan || !in.onField[man]) continue;
    float dx = in.posX[man] - in.ballX;
    float dz = in.posZ[man] - in.ballZ;
    float distSq = dx * dx + dz * dz;
    if (distSq > radiusSq) continue;

    const int role = in.role[man % 11];
    const int slot = PressSlotOf(in.pressSlotMan, man);
    const float root = std::sqrt(e.score);
    const float offset = PressOffsetDeg(c, root);
    int code0 = PressManAngleCode(c, in, man, 0.0f, slot);
    int codePlus = PressManAngleCode(c, in, man, offset, slot);
    int codeMinus = PressManAngleCode(c, in, man, -offset, slot);

    // Skip a man on the zero code who is far from the ball, unless the team
    // has a first presser or the man is the presser role.
    if (!in.presserFlag && code0 == 0 && distSq > c.rampStart_deg && role != kPresserRole) continue;

    // DELAY (action 0x33/0x34): a man in a press slot close to the ball takes
    // that slot as his code.
    const bool delay = static_cast<unsigned>(in.action[man] - 0x33) < 2u;
    if (delay && slot >= 0 && distSq <= c.pass2DelayDistSq && code0 == 0) {
      codePlus = slot;
      codeMinus = slot;
    }

    float score = root;
    if (state.codeTaken[code0] == 0) {
      state.codeTaken[code0] = 1;
      if (state.pressCode[man % 11] < 0) state.pressCode[man % 11] = code0;
    } else if (codePlus != code0 || codeMinus != code0) {
      int chosen = codePlus != code0 ? codePlus : codeMinus;
      if (chosen == 0 && distSq >= c.pass2ZeroRoleDistSq && role != kPresserRole) {
        score += c.pass2SameCodeAdd;
      } else {
        if (!delay) score += c.pass2ActionAdd;
        state.pressCode[man % 11] = chosen;
      }
    } else {
      score += c.pass2SameCodeAdd;
    }
    if (state.pressCode[man % 11] == 0) state.zeroCodeFlag = true;
    out.push_back({man, score});
  }
  SortPressBucket(out);
  return out;
}

PressFillResult PressCandidateFill(const PressConstants& c, const std::array<PressFillMan, 11>& mans) {
  PressFillResult r;
  for (int k = 0; k < 11; ++k) {
    const PressFillMan& m = mans[k];
    // The k>10 branch is the team-flag test; k<=10 always passes it.
    const bool carrierOk = (m.carrierTagged && (k > 10 || m.teamFlagZero)) ||
                           (m.inStampedSet && m.notInA558);
    if (!(m.onField && !m.excluded && m.inDistance && carrierOk && m.gate479470)) continue;
    r.count++;
    // 4.0 (0x1425656c0) is the same float as pass2ActionAdd; read it from the constants.
    if (std::sqrt(m.tableDistSq) < c.pass2ActionAdd) r.flagA = true;
    if (m.angleCode == 0) r.flagB = true;
    if (m.angleCode >= 0 && m.angleCode < 4) r.slotSet[m.angleCode] = true;
  }
  return r;
}

// ---- Second presser (FUN_140632bc0, FUN_140632ae0, FUN_140632e70). ----

bool PressHolderKindExcluded(int pesKind) {
  constexpr int kExcludedKindBase = 0x11;
  constexpr uint64_t kExcludedKindMask = 0x8000000000000009ull;  // kinds 0x11, 0x14, 0x50
  constexpr unsigned kExcludedMaskBits = 64;
  constexpr int kExcludedKindA = 0x52;
  constexpr int kExcludedKindB = 0x57;
  const unsigned offset = static_cast<unsigned>(pesKind) - static_cast<unsigned>(kExcludedKindBase);
  const bool inMask = offset < kExcludedMaskBits && ((kExcludedKindMask >> offset) & 1u) != 0;
  return inMask || pesKind == kExcludedKindA || pesKind == kExcludedKindB;
}

int PressHolder(int possessionMan, int otherCarrier, int otherCarrierKind) {
  if (static_cast<unsigned>(possessionMan) < static_cast<unsigned>(kPressTeamMen)) return possessionMan;
  if (static_cast<unsigned>(otherCarrier) < static_cast<unsigned>(kPressTeamMen) &&
      !PressHolderKindExcluded(otherCarrierKind)) {
    return otherCarrier;
  }
  return kPressNoMan;
}

int PressClass(const PressClassInput& in) {
  if (in.flagFiveMan) return kPressClassFive;
  if (in.flagDNone) return kPressClassSeven;
  if (in.testFun632850) return kPressClassFour;
  if (in.teamB716) return kPressClassSix;
  return in.testFun632320 ? kPressClassOne : kPressClassSeven;
}


namespace {

// Squared distance on the pitch plane (x, z).
float PlaneDistSq(float ax, float az, float bx, float bz) {
  const float dx = bx - ax;
  const float dz = bz - az;
  return dx * dx + dz * dz;
}

// Signed difference of the bearings from `o` to `a` and from `o` to `b`, folded into [-180, 180].
float BearingDiffDeg(const PressConstants& c, float ox, float oz, float ax, float az, float bx, float bz) {
  return PressSideAngleDeg(c, PressBearingDeg(c, ox, oz, ax, az), PressBearingDeg(c, ox, oz, bx, bz));
}

// The assignment word the mode-7 takeover refuses (T+0xA2A0 == 0x3C; FUN_140633940 at 0x1406340BF).
constexpr int kAssignmentRefusedMode7 = 0x3c;

}  // namespace

PressEvalResult PressEvalPick(const PressConstants& c, const PressEvalInput& in) {
  if (in.ref < 0 || in.ref >= kPressTeamMen) return {};
  const PressEvalMan& holder = in.men[in.ref];
  // The early exit: flag 13 with the holder already past the line on the attack side.
  if (in.flag13 && in.sign * holder.x > in.sign * in.earlyLine) return {};
  // The side's block of the holder's nearest list, ascending d^2; ties keep the table order.
  const int base = in.side * kPressSideMen;
  std::array<int, kPressSideMen> order{};
  for (int k = 0; k < kPressSideMen; ++k) order[k] = k;
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const PressEvalMan& ma = in.men[base + a];
    const PressEvalMan& mb = in.men[base + b];
    return PlaneDistSq(holder.x, holder.z, ma.x, ma.z) < PlaneDistSq(holder.x, holder.z, mb.x, mb.z);
  });
  bool w = in.flag13;  // W: the D_ev test sets it, and it stays set for the later men
  for (int k : order) {
    const int cand = base + k;
    const PressEvalMan& m = in.men[cand];
    const PressEvalSlot& sl = in.slots[k];
    if (cand == in.excluded || !m.active) continue;      // G3, G2
    if (w && m.g180Reject) continue;                     // G4 (only while W)
    if (m.excludedKind) continue;                        // G5
    if (!sl.flagged || m.flag15) continue;               // G6
    if (sl.orderTwentyFour) continue;                    // G7
    if (in.windowEvent) w = true;                        // the D_ev test
    const bool inc = cand == in.incumbent;
    if (in.mode == 4) {                                  // the mode-4 pre-gate
      if (in.levelClass == 6 || w || in.phaseAggressive || sl.roleClass != 2) {
        if (sl.roleClass == 0) continue;
      } else if (!m.recordBit23 && in.attackLevel <= 2) {
        continue;
      }
    }
    // S = |ref - cand|, and the radius: the press timer or flag 13 widen it, flag 14 and the aggressive phase narrow it.
    const float dist = std::sqrt(PlaneDistSq(holder.x, holder.z, m.x, m.z));
    float theta = c.evalThetaBase_m;
    if (in.pressTimer || w) theta = c.evalThetaFar_m;
    else if (in.flag14) theta = c.evalThetaZ_m;
    else if (in.phaseAggressive) theta = c.evalThetaB29_m;
    if (inc) theta += c.evalIncumbentTheta_m;
    // Block A: a fast holder running at the man widens the accept-two gate.
    bool blockA = false;
    const float headingToCand = PressBearingDeg(c, holder.x, holder.z, m.x, m.z);
    if (in.blockAOppFree && in.refSpeedKmh > c.evalBlockASpeedKmh && in.levelClass == 6 &&
        std::fabs(PressSideAngleDeg(c, headingToCand, PressWrapDeg(c, in.refHeadingDeg))) <
            c.evalBlockAHeading_deg) {
      blockA = true;
      if (theta < c.evalThetaBase_m) theta += c.evalBlockAAdd_m;
    }
    if (dist > theta) continue;                          // accept 1
    const bool window =
        in.pressTimer && static_cast<int>(c.evalWindowSec_s * in.fps + 0.5f) < in.pressTimerY;
    if (!(w || window || in.sign * m.x > in.sign * holder.x)) continue;  // G_sub: ahead of the holder
    float theta2;
    if (in.attackLevel >= 3) theta2 = std::max(c.evalThetaTwoHigh_m, theta);
    else if (in.mode == 4) theta2 = theta;
    else theta2 = (in.levelClass == 6 || in.phaseAggressive) ? c.evalThetaTwoHigh_m : c.evalThetaTwoLow_m;
    if (!(dist <= theta2 || blockA || w || window)) continue;  // accept 2
    if (in.mode == 7) {                                  // the mode-7 takeover
      int lr = 3;
      if (in.excluded >= 0 && in.excluded < kPressTeamMen) {
        const int riX = in.slots[in.excluded % kPressSideMen].roleClass;
        if (riX == 0) lr = 1;
        else if (riX == 1) lr = 2;
        else if (riX == 2 && in.aggressive) lr = 2;
        const float d = BearingDiffDeg(c, holder.x, holder.z, m.x, m.z, in.men[in.excluded].x,
                                       in.men[in.excluded].z);
        if (std::fabs(d) < (inc ? c.evalMode7IncAngle_deg : c.evalMode7Angle_deg)) continue;
      }
      if (!((sl.roleClass == lr || w) && sl.roleClass != 0 && sl.assignment != kAssignmentRefusedMode7)) {
        continue;
      }
    }
    if (in.mode == 4) {                                  // the mode-4 lateral test
      if (!inc && in.excluded >= 0 && in.excluded < kPressTeamMen) {
        const float d = BearingDiffDeg(c, holder.x, holder.z, m.x, m.z, in.men[in.excluded].x,
                                       in.men[in.excluded].z);
        if (std::fabs(d) < c.evalMode4Angle_deg) continue;
      }
      const float incStep = inc ? c.evalIncumbentStep_m : 0.0f;
      if (!(in.sign * m.x - c.evalMode4Margin_m <= in.sign * sl.keepX + incStep)) continue;
    }
    PressEvalResult accepted;
    accepted.man = cand;
    accepted.mode = in.mode;
    return accepted;
  }
  return {};
}

bool PressGatePasses(const PressConstants& c, const PressGateInput& in) {
  if (in.ballHeight_m > c.gateBallHeight_m) return false;
  if (in.teamKickValid) return false;
  if (in.pressTimer || in.flag13 || in.attackLevel >= 3 || in.classFive || in.cover) return true;
  return in.facingToAttack_deg <= c.gateFacing_deg;
}

bool PressCoverByHolder(const PressConstants& c, float dist_m, float bearingDeg, float headingDeg) {
  if (dist_m < c.coverNear_m) return true;
  if (dist_m > c.coverFar_m) return false;
  if (!(dist_m > 0.0f && dist_m < c.coverOuter_m)) return false;
  return std::fabs(PressSideAngleDeg(c, bearingDeg, headingDeg)) < 0.5f * c.coverCone_deg;
}

int PressClassFiveMan(const PressConstants& c, const PressClassFiveInput& in) {
  if (in.holder < 0 || in.holder >= kPressTeamMen) return kPressNoMan;
  if (!in.clauseOk) return kPressNoMan;
  if (in.cand < 0 || in.cand >= kPressTeamMen || in.cand == in.matchUp) return kPressNoMan;
  if (in.keeperKind) return kPressNoMan;
  if (in.roleClass == 0 && !in.kindTen) return kPressNoMan;
  const float threshold = in.kindTen ? c.classFiveKindTen_m : c.classFiveDist_m;
  return in.ballDist_m < threshold ? in.cand : kPressNoMan;
}

bool PressPresserKept(const PressConstants& c, const PressMaintainInput& in) {
  if (!in.inPlay) return false;                                                // step 1
  if (in.slot >= kPressSideMen || in.tableWord != kPressAction) return false;  // step 2
  if (in.keeperHolds) return false;                                            // step 3
  if (in.holderNow != in.holderRecorded) return false;                         // step 4
  if (in.reason == 5 && in.classFiveNow != in.man) return false;               // step 5
  const bool kickValid = in.teamKickMan < kPressTeamMen;
  if (in.lifted && kickValid) return false;                                    // step 6, lifted
  if (!in.lifted && kickValid && !(in.matchUp < kPressTeamMen && in.matchUp == in.teamKickMan)) {
    return false;                                                              // step 6, grounded
  }
  // Step 7 (reason 3) is unreachable: nothing writes reason 3 here. Step 8 (the refusal) is the eval's, not ported.
  if (in.reason == 5) {                                                        // step 9a
    if (in.holderNow >= kPressTeamMen) return false;
    if (in.manToHolderDistSq >= c.maintainReason5DistSq_m2) return false;
  }
  if (!in.holderFromTouch && in.holderNow < kPressTeamMen && in.xMan < kPressTeamMen &&
      in.xCloserToTeamPoint) {
    return false;                                                              // step 9b
  }
  if (in.reason == 4 || in.reason == 7) {                                      // step 9c
    if (!in.flag13 && in.bearingVsCoverDeg < c.maintainBearing_deg) return false;
    if (in.roleClass == 0) return false;
  }
  if (in.stampLast - in.stampFirst <= in.windowFrames) return true;            // step 9d
  if (in.xMan >= kPressTeamMen) return true;                                   // step 9e
  return in.manToXDistSq <= c.carrierRampEnd_m;                                // step 9f
}

float PressPickRadius_m(const PressConstants& c, float root) {
  float ramp;
  if (root <= c.pass2Lo) {
    ramp = 1.0f;
  } else if (root < c.pass2Hi) {
    ramp = (root - c.pass2Hi) / c.pass2Div;
  } else {
    ramp = 0.0f;
  }
  return c.carrierRampStart_m + c.carrierRampSpan_m * ramp;
}

// ponytail: FUN_140536FC0's gate (0x140537332; S1Watch's decode: xz only, squared distance against 169) measures the
// ball sample (B+0x88, sample 0) against the MA ball copy at MA+0x2482C/+0x24834 (pes-re-defensive-shape.md §0; the object
// at team+0x18, its type not decoded). GF sets both to the ball now (PesPressTick), so the gate never refuses here. PES
// refuses where the two are more than 13 m apart; whether that happens (the docs put the in-flight gap at metres) is not
// measured. The copy's lag is not modelled: its writer (0x140a6f29e) is NOFUNC and its cadence is not decoded. Upgrade:
// measure the gap in a live match, then model it.
bool PressBallGatePasses(const PressPickInput& in) {
  const float dx = in.sampleX - in.ballX;
  const float dz = in.sampleZ - in.ballZ;
  return dx * dx + dz * dz <= kPressBallGate_m * kPressBallGate_m;
}

int PressWinner(const PressConstants& c, const PressPickInput& in,
                const std::vector<PressCandidate>& bucket) {
  if (bucket.empty() || !PressBallGatePasses(in)) return -1;
  const int man = bucket[0].man;
  const float dx = in.posX[man] - in.ballX;
  const float dz = in.posZ[man] - in.ballZ;
  const float radius = PressPickRadius_m(c, std::sqrt(bucket[0].score));
  return dx * dx + dz * dz <= radius * radius ? man : -1;
}

bool PressTargetIsBall(PressPoint sample, PressPoint ball, PressPoint base) {
  const float toBall = (sample.x - ball.x) * (sample.x - ball.x) +
                       (sample.z - ball.z) * (sample.z - ball.z);
  const float toBase = (sample.x - base.x) * (sample.x - base.x) +
                       (sample.z - base.z) * (sample.z - base.z);
  return toBall <= toBase;
}

int PressTargetTick(const PressConstants& c) {
  return static_cast<int>(c.framesPerSec * c.targetTickRate + c.targetTickBias);
}

int PressSpeedClass(const PressConstants& c, float speedKmh) {
  if (speedKmh <= c.speedCut0_kmh) return 0;
  if (speedKmh <= c.speedCut1_kmh) return 1;
  if (speedKmh <= c.speedCut2_kmh) return 2;
  if (speedKmh <= c.speedCut3_kmh) return 3;
  return 4;
}

float PressCommandSpeed_ms(const PressConstants& c, float dist_m, float speedKmh, float slope) {
  const float want = dist_m * slope;
  const int cls = PressSpeedClass(c, speedKmh);
  // One band above the current class, in m/s (km/h / 3.6); class 4 uncapped.
  // The divisor is arithmetic, not a PES number.
  float cap = want;
  if (cls == 0)
    cap = c.speedCut1_kmh / 3.6f;
  else if (cls == 1)
    cap = c.speedCut2_kmh / 3.6f;
  else if (cls == 2)
    cap = c.speedCut3_kmh / 3.6f;
  return std::min(want, cap);
}

int PressWriterWord(int man, int winner, int second) {
  return (man == winner || man == second) ? kPressState : kNothingState;
}

bool PressIsLoosePresser(const int* role, int side, int k) {
  if (side < 0 || side > 1 || k < 0 || k >= kPressSideMen) return false;
  return role[side * kPressSideMen + k] == kPressState;
}

}  // namespace pes
