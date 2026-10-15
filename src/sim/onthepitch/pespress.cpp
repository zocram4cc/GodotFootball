// PES 2021's presser picker arithmetic (docs/re/pes-re-press-layer-port.md).
// See pespress.hpp for what each function is and where its constants live.

#include "onthepitch/pespress.hpp"

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
    {0x14259be5c, &PressConstants::slotStepDeg},
    {0x14259bdf4, &PressConstants::pressTargetDistSq},
    {0x14259be44, &PressConstants::pickNearSq_m2},
    {0x1425c6784, &PressConstants::pickAngleHi_deg},
    {0x1425cee20, &PressConstants::fillReachSq_m2},
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

int PressManAngleCodeAt(const PressConstants& c, const PressPickInput& in, int man, int slot) {
  return PressManAngleCode(c, in, man, 0.0f, slot);
}

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
    r.manCode[k] = m.angleCode;
  }
  return r;
}

}  // namespace pes

namespace pes {
namespace {

constexpr int kRoleCountLocal = 11;  // men per side

float Sq(float v) { return v * v; }

}  // namespace

PressWriterOutput RunPressWriter(const PressConstants& c, const PressWriterInput& in, int side,
                                 TeamPressState& st, uint32_t now) {
  PressWriterOutput out;
  // A: the press slots and the old target are cleared whatever the gates say.
  for (int& slot : st.pressSlot) slot = kPressNoMan;
  const int oldTarget = in.oldPressTarget;

  // B: the entry gates (MI+0x78, FUN_141b92460, FUN_140a53f90, FUN_140a62690, MI+0x116c,
  // FUN_140a51320 / FUN_140a6ce10) arrive as one boolean; see pespress.hpp.
  if (!in.entryOk) return out;
  // C: a man with a clip countdown in the window exits; the caller folds it into entryOk.
  // D: the armed gate, the referee check, the team gate, the loose-ball and event tests.
  if (!in.armed) return out;
  if (in.refBlocks) return out;
  const bool refPhase16 = in.refPhase16;  // cStack_258
  if (!in.teamGateOpen) return out;
  if (in.looseBallFar) return out;
  if (!in.eventAllows) return out;
  out.ran = true;

  // E: candidate fill (FUN_140639830). Passers get a code and count toward slots.
  const PressFillResult fr = PressCandidateFill(c, in.fill);
  int count = fr.count;
  bool flagA = fr.flagA;
  bool flagB = fr.flagB;
  std::array<bool, 4> slotSet = fr.slotSet;
  std::array<int, 11> manCode = fr.manCode;

  // F: each passer presses (FUN_140a42940 0x34 on local man k), owns slot code, and
  // the arrival phase FUN_140535260 is written for that slot.
  for (int k = 0; k < kRoleCountLocal; ++k) {
    const int code = manCode[k];
    if (code < 0) continue;
    const int man = side * kRoleCountLocal + k;
    st.SetAction(k, kPressAction, now);
    st.pressSlot[code] = man;
    const float bearing = PressBearingDeg(c, in.pick.ballX, in.pick.ballZ, in.pick.posX[man],
                                          in.pick.posZ[man]);
    st.arrivalDeg[code] = PressWrapDeg(c, bearing + code * c.slotStepDeg);  // ponytail: FUN_140535260 direction
  }

  out.count = count;
  out.flagA = flagA;
  out.flagB = flagB;
  if (count >= 3) return out;

  // G: pass-2 picker on the side's 11 men, then the press-target loop over its bucket.
  PressPickInput pick = in.pick;
  for (int i = 0; i < 4; ++i) pick.pressSlotMan[i] = st.pressSlot[i];
  for (int i = 0; i < kRoleCountLocal; ++i) pick.role[i] = st.role[i];
  PressPickState ps;
  ps.codeTaken = {};
  for (int i = 0; i < 4; ++i) ps.codeTaken[i] = slotSet[i] ? 1 : 0;
  ps.zeroCodeFlag = flagB;
  for (int k = 0; k < kRoleCountLocal; ++k) ps.pressCode[k] = manCode[k];
  const std::vector<PressCandidate> pass1 = RunPresserPass1(c, pick, side);
  const std::vector<PressCandidate> bucket = RunPresserPicker(c, pick, pass1, ps);

  int presserTarget = kPressNoMan;   // uStack_248: the presser kept on role 0x3a
  int delayTarget = kPressNoMan;     // uStack_254: the DELAY man
  int oldHit = kPressNoMan;          // uStack_24c: the old target still close enough

  for (const PressCandidate& e : bucket) {
    const int man = e.man;
    if (man > 0x15) break;
    const int u = man % kRoleCountLocal;  // man < 0x16 here
    int code = manCode[u];
    const float d2 = Sq(in.pick.posX[man] - in.pick.ballX) + Sq(in.pick.posZ[man] - in.pick.ballZ);
    bool toFlagTest = false;  // true: LAB_1405375f5 (the record and slot tests)
    if (!(flagB == false && code != 2)) {
      // LAB_1405375e9: a code already in use skips this man unless the count allows the slot.
      if (code < 0) {
        // ponytail: aiStack_200[-1] is an out-of-bounds read in PES; the entry is not written
        // here (no GF reader for that aliasing). Counted in unresolved, not in the output.
        continue;
      }
      if (!slotSet[code]) {
        toFlagTest = true;
      } else {
        if (count > 2) break;
        continue;
      }
    } else {
      if (d2 >= c.pass2ZeroRoleDistSq && st.role[u] != kPresserRole) {
        // d^2 at or beyond 81 and not the presser role: LAB_1405375e9.
        if (code < 0) continue;
        if (!slotSet[code]) {
          toFlagTest = true;
        } else {
          if (count > 2) break;
          continue;
        }
      } else {
        code = 0;
        flagB = true;
        toFlagTest = true;
      }
    }
    if (!toFlagTest) continue;

    // LAB_1405375f5: the slot record test (FUN_140479470 == 1 skips the man).
    if (in.slotRecordIdle[man]) continue;
    const float fVar31 = (oldTarget == man) ? c.pass2Lo : 0.0f;
    bool bVar4;
    if (!refPhase16) {
      // ponytail: the ball heading is FUN_140a57720(G+0x40) through FUN_1404a53e0; the caller
      // supplies the bearing until the vector read is decoded.
      const bool outside = PressAngleOutsideBand(c, in.ballHeadingDeg, 60.0f);
      if (outside) {
        bVar4 = true;
      } else if (code == 0) {
        bVar4 = Sq(fVar31 + c.pass2ActionAdd) <= d2;
      } else {
        bVar4 = true;
      }
    } else {
      bVar4 = true;
    }

    bool toSlot = false;
    if (!in.conceptPick) {
      toSlot = true;
    } else {
      // The concept pick branch: a pick-blocked man or one within 8 m goes to the slot test;
      // otherwise the bearing from the man to the ball decides.
      if (in.pickBlocked[u] || d2 <= c.pickNearSq_m2) {
        toSlot = true;
      } else {
        const float bearing = PressBearingDeg(c, in.pick.posX[man], in.pick.posZ[man],
                                              in.pick.ballX, in.pick.ballZ);
        const float ang = PressSideAngleDeg(c, c.slotStepDeg, bearing);
        toSlot = ang <= c.carrierRampEnd_m || ang >= c.pickAngleHi_deg;
      }
    }
    if (!toSlot) continue;

    // The slot test (LAB_140537749).
    if (code == 0 && st.role[u] == kPresserRole && !refPhase16) {
      st.SetRecordAction(u, 0);
      presserTarget = man;
    } else if (bVar4) {
      if (st.role[u] == kPresserRole && !refPhase16)
        st.SetRecordAction(u, kPressAction);
      else
        st.SetAction(u, kPressAction, now);
    } else {
      if (st.role[u] == kPresserRole)
        st.SetRecordAction(u, kDelayAction);
      else
        st.SetAction(u, kDelayAction, now);
      delayTarget = man;
    }
    if (oldTarget == man && std::sqrt(in.tableDistSq[man]) < c.pressTargetDistSq) oldHit = man;

    st.pressSlot[code] = man;
    const float bearing = PressBearingDeg(c, in.pick.ballX, in.pick.ballZ, in.pick.posX[man],
                                          in.pick.posZ[man]);
    st.arrivalDeg[code] = PressWrapDeg(c, bearing + code * c.slotStepDeg);
    slotSet[code] = true;
    count++;
    if (count > 2) break;
  }

  // H: the tail. A presser left without a second slot presses, and the DELAY and PRESS
  // targets of the old target are swapped in.
  if (st.armed && count < 2 && presserTarget < 0x16) st.SetRecordAction(presserTarget % kRoleCountLocal, kPressAction);
  if (oldHit < 0x16 && delayTarget < 0x16 && oldHit != delayTarget) {
    st.SetAction(oldHit % kRoleCountLocal, kDelayAction, now);
    st.SetAction(delayTarget % kRoleCountLocal, kPressAction, now);
  }

  out.count = count;
  out.flagA = flagA;
  out.flagB = flagB;
  return out;
}

}  // namespace pes
