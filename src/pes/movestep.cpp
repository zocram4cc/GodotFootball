#include "pes/movestep.hpp"

#include "pes/exe.hpp"
#include <cassert>
#include <cmath>
#include <cstring>

namespace pes {
namespace {

// The .rdata floats behind the playback writers, by VA (docs/re/pes-re-anim-select.md 4.1).
// The install test checks their relations.
struct Source {
  uint64_t va;
  float PlaybackConstants::*field;
};
constexpr Source SOURCES[] = {
    {0x1425ddf78, &PlaybackConstants::gLow},      // DAT_1425ddf78
    {0x1425ddf80, &PlaybackConstants::gHigh},     // DAT_1425ddf80
    {0x1425ddfe0, &PlaybackConstants::gSlope},    // DAT_1425ddfe0
    {0x14256ae1c, &PlaybackConstants::one},       // DAT_14256ae1c
    {0x1425c48a0, &PlaybackConstants::stepSlope}, // DAT_1425c48a0
    {0x1425ab124, &PlaybackConstants::stepBase},  // DAT_1425ab124
    {0x142588200, &PlaybackConstants::turnP2},    // DAT_142588200
    {0x1425a8a28, &PlaybackConstants::turnT},     // DAT_1425a8a28
};

// The playback ramp t(G) of FUN_1406ba070 and FUN_1406e9f10: 1 at or below gLow,
// 0 at or above gHigh, linear between.
float PlaybackRamp(float g, const PlaybackConstants& k) {
  if (g <= k.gLow) return k.one;
  if (g < k.gHigh) return (g - k.gHigh) / k.gSlope;
  return 0.0f;
}

}  // namespace

void StoreRateCeiling(MoveStepRecord& r, int tick, float ceiling) {
  if (tick == r.rateStamp) return;
  r.rateStamp = tick;
  r.rateCeiling = ceiling;
}

std::optional<PlaybackConstants> LoadPlaybackConstants(const PeImage& exe, std::string* error) {
  PlaybackConstants k{};
  for (const Source& s : SOURCES) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*s.field)))) {
      *error = "playback constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

float StepPlayback(float g, const PlaybackConstants& k) {
  return PlaybackRamp(g, k) * k.stepSlope + k.stepBase;
}

float TurnPlayback(float g, float p, const PlaybackConstants& k) {
  return p * p * k.turnP2 + k.stepBase + PlaybackRamp(g, k) * k.turnT;
}

bool RunRateAllowed(float rate, float rateCeiling, float stepBase) {
  return rate <= stepBase * rateCeiling;
}

void ApplyTransition(MoveStepRecord& rec, MoveReq& req) {
  if (!req.Flag(moveword::kChangeState)) return;
  rec.Enter(static_cast<MoveState>(req.U(moveword::kNextState) & 0xffu));
}

// The carried request words: req[0x17..0x2e] (MoveStepRecord::carried), in PES's order.
static_assert(moveword::kDwellLimit2 - moveword::kRecordStampA + 1 == 24, "carried holds req[0x17..0x2e]");
void CarryOut(MoveStepRecord& rec, const MoveReq& req) {
  for (int w = moveword::kRecordStampA; w <= moveword::kDwellLimit2; ++w)
    rec.carried[w - moveword::kRecordStampA] = req.U(w);
}
void CarryIn(const MoveStepRecord& rec, MoveReq& req) {
  for (int w = moveword::kRecordStampA; w <= moveword::kDwellLimit2; ++w)
    req.SetU(w, rec.carried[w - moveword::kRecordStampA]);
}

namespace {
// The scalars of FUN_14081e340 and FUN_140a92c80, by VA (see SpeedClassConstants).
struct SpeedScalar {
  uint64_t va;
  float SpeedClassConstants::*field;
};
constexpr SpeedScalar SPEED_SCALARS[] = {
    {0x14259858c, &SpeedClassConstants::full},
    {0x142598598, &SpeedClassConstants::wrapFull},
    {0x142598580, &SpeedClassConstants::half},
    {0x1425ddffc, &SpeedClassConstants::wrapHalf},
    {0x142588218, &SpeedClassConstants::neg},
    {0x14256ae1c, &SpeedClassConstants::one},
    {0x14259be5c, &SpeedClassConstants::quarter},
    {0x1426550e8, &SpeedClassConstants::tabScale},
    {0x14259bde8, &SpeedClassConstants::lo0},
    {0x14259bdf4, &SpeedClassConstants::lo1},
    {0x1425a1ffc, &SpeedClassConstants::hi0},
    {0x1425c48c0, &SpeedClassConstants::hi1},
};
constexpr uint64_t SIN_QUARTER_VA = 0x142655950;  // FUN_140a92c80's 513-entry table
}  // namespace

std::optional<SpeedClassConstants> LoadSpeedClassConstants(const PeImage& exe, std::string* error) {
  SpeedClassConstants k{};
  for (const SpeedScalar& s : SPEED_SCALARS)
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*s.field)))) {
      *error = "speed class constants: .rdata unreadable";
      return std::nullopt;
    }
  if (!exe.ReadVa(SIN_QUARTER_VA, sizeof k.sinQuarter,
                  reinterpret_cast<uint8_t*>(k.sinQuarter.data()))) {
    *error = "speed class constants: sin table unreadable";
    return std::nullopt;
  }
  return k;
}

float WrapHalfTurn(float deltaDeg, const SpeedClassConstants& k) {
  if (std::fabs(deltaDeg) <= k.half) return deltaDeg;
  const float sign = deltaDeg < 0.0f ? k.neg : k.one;
  return deltaDeg - sign * k.full;
}

float TableSinDeg(float x, float s, const SpeedClassConstants& k) {
  // FUN_140a92c80's folds: into (0, 360), then the second half turn carries a sign, then the
  // mirror at 90 (sin(180 - x) = sin x).
  if (x > 0.0f) {
    if (x >= k.full) x += k.wrapFull;
  } else {
    x += k.full;
  }
  float sign = k.one;
  if (x >= k.half) {
    sign = k.neg;
    x += k.wrapHalf;
  }
  if (x >= k.quarter) x = k.half - x;
  const float idx = (x / k.quarter) * k.tabScale;
  const int i = static_cast<int>(idx);  // CVTTSS2SI truncates
  const float scale = sign * s;
  if (static_cast<uint32_t>(i) > 511u) return scale;  // the table ends at 512 (sin 90 = 1)
  const float frac = idx - static_cast<float>(i);
  return ((k.one - frac) * k.sinQuarter[i] + frac * k.sinQuarter[i + 1]) * scale;
}

int SpeedClassOf(float desiredKmh, float deltaDeg, const SpeedClassConstants& k) {
  const float turn = WrapHalfTurn(deltaDeg, k);
  // FUN_14081e340: the turn factor is FUN_140a92c80(|turn| + 90, 1) = sin(|turn| + 90) = cos(turn).
  const float factor = TableSinDeg(std::fabs(turn) + k.quarter, k.one, k);
  const float v = desiredKmh * factor;
  if (v <= k.lo0) return 0;
  if (v <= k.lo1) return 1;
  if (v > k.hi0) return v > k.hi1 ? 4 : 3;
  return 2;
}

std::optional<AngleBucketConstants> LoadAngleBucketConstants(const PeImage& exe, std::string* error) {
  constexpr uint64_t WIDE_VA = 0x1425ddeb0;  // FUN_1406b9b20's wide windows
  constexpr uint64_t NARROW_VA = 0x1425ddee0;
  constexpr uint64_t BUCKET_VA = 0x1425ddf10;
  constexpr uint64_t HYST_UP_VA = 0x14259222c;
  constexpr uint64_t HYST_DOWN_VA = 0x14259be7c;
  constexpr uint64_t CENTRE_VA = 0x1425dd660;  // DAT_1425dd660: the bucket centre angles (the body fill)
  AngleBucketConstants k{};
  if (!exe.ReadVa(WIDE_VA, sizeof k.wide, reinterpret_cast<uint8_t*>(k.wide.data())) ||
      !exe.ReadVa(NARROW_VA, sizeof k.narrow, reinterpret_cast<uint8_t*>(k.narrow.data())) ||
      !exe.ReadVa(BUCKET_VA, sizeof k.bucket, reinterpret_cast<uint8_t*>(k.bucket.data())) ||
      !exe.ReadVa(CENTRE_VA, sizeof k.centre, reinterpret_cast<uint8_t*>(k.centre.data())) ||
      !exe.ReadVa(HYST_UP_VA, sizeof(float), reinterpret_cast<uint8_t*>(&k.hysteresisUp)) ||
      !exe.ReadVa(HYST_DOWN_VA, sizeof(float), reinterpret_cast<uint8_t*>(&k.hysteresisDown))) {
    *error = "angle bucket constants: .rdata unreadable";
    return std::nullopt;
  }
  return k;
}

namespace movestep {
int AngleBucket(const AngleBucketConstants& k, float angleDeg, int currentBucket, bool narrow) {
  const auto& edges = narrow ? k.narrow : k.wide;
  for (int w = 0; w < 10; ++w) {
    const int bucket = k.bucket[w];
    float low = 0.0f, high = 0.0f;  // this window's hysteresis
    if (bucket == currentBucket) {
      low = k.hysteresisDown;  // the current window widens on both sides
      high = k.hysteresisUp;
    } else if (w + 1 < 10 && k.bucket[w + 1] == currentBucket) {
      high = k.hysteresisDown;  // the window just below the current one shrinks at its top
    }
    if (edges[w] + low <= angleDeg && angleDeg <= edges[w + 1] + high) return bucket;
  }
  return 0;  // FUN_1406b9b20's miss
}
}  // namespace movestep

namespace {
// Group C's install tables and scalars (STEP_LOOP, STEP_LOOP_CHANGE, TURN_STOP and the transitions).
// Each VA is the target of one instruction a handler reads, so a different executable fails the load
// rather than reading wrong numbers.
constexpr uint64_t STEP_COLUMN_TABLE_VA = 0x1425dd988;    // STEP_LOOP 0x1406c5d82, TURN_STOP 0x1406cb36e
constexpr uint64_t STEP_GRID_TABLE_VA = 0x1425dd9b0;      // STEP_LOOP 0x1406c5d8e, TURN_STOP 0x1406cb37f
constexpr uint64_t STEP_BUCKET_DEGREES_VA = 0x1425dd960;  // TURN_STOP entry 0x1406d2d8e

#define STEP_FIELD(a) &StepScalars::a
const struct {
  uint64_t va;
  float StepScalars::* field;
} STEP_SCALAR_SITES[] = {
    {0x14256ae1c, STEP_FIELD(one)},
    {0x142565600, STEP_FIELD(half)},
    {0x14259858c, STEP_FIELD(full)},
    {0x142598598, STEP_FIELD(wrapFull)},
    {0x142598580, STEP_FIELD(halfTurn)},
    {0x142588218, STEP_FIELD(negOne)},
    {0x14259a764, STEP_FIELD(ticks60)},
    {0x14256ae34, STEP_FIELD(scale1000)},
    {0x14259be10, STEP_FIELD(kmh16)},
    {0x14259222c, STEP_FIELD(kmh5)},
    {0x14259bdfc, STEP_FIELD(kmh11)},
    {0x14259bdf4, STEP_FIELD(kmh8)},
    {0x1425a1ffc, STEP_FIELD(kmh13p5)},
    {0x1425c48c0, STEP_FIELD(kmh22)},
    {0x14259d618, STEP_FIELD(deg22p5)},
    {0x1425cc680, STEP_FIELD(deg11p25)},
    {0x1425cc688, STEP_FIELD(deg33p75)},
    {0x1425cca34, STEP_FIELD(deg67p5)},
    {0x14259be5c, STEP_FIELD(deg90)},
    {0x14259be60, STEP_FIELD(deg135)},
    {0x1425cb30c, STEP_FIELD(deg112p5)},
    {0x1425c8380, STEP_FIELD(deg175)},
    {0x1425ddf84, STEP_FIELD(deg5p625)},
    {0x1425ddf94, STEP_FIELD(deg28p125)},
    {0x1425ddf8c, STEP_FIELD(deg16p875)},
    {0x1425ddfcc, STEP_FIELD(deg168p75)},
    {0x1425ddfbc, STEP_FIELD(deg146p25)},
    {0x1425ddfb8, STEP_FIELD(deg140p625)},
    {0x1425ddfd4, STEP_FIELD(sq1139)},
    {0x1425ddfd8, STEP_FIELD(sq8100)},    // 8100.0: FUN_1406c1070 0x1406c1243, FUN_1406c6f80 0x1406c701c
    {0x1425ddfd0, STEP_FIELD(deg174p375)},
    {0x1425a2008, STEP_FIELD(deg270)},
    {0x1425cb494, STEP_FIELD(sq2025)},    // 2025.0: FUN_1406c6f80 0x1406c702e
    {0x1425ddfdc, STEP_FIELD(sq12656p25)},  // 12656.25: FUN_1406c1070 0x1406c1254
    {0x1425ddf98, STEP_FIELD(deg31p64)},
    {0x1425c5958, STEP_FIELD(neg90)},
    {0x1425aa1c8, STEP_FIELD(neg45)},
    {0x1425dbdb8, STEP_FIELD(neg67p5)},
    {0x1425dbdbc, STEP_FIELD(neg112p5)},
    {0x1425ddffc, STEP_FIELD(neg180)},
    {0x14259be30, STEP_FIELD(deg45)},
    {0x14259be1c, STEP_FIELD(kmh20)},
    {0x14258c264, STEP_FIELD(dist3)},
    {0x14258c260, STEP_FIELD(rate0p9)},
    {0x14257a190, STEP_FIELD(s0p6)},
    {0x14259b89c, STEP_FIELD(s0p4)},
    {0x142590d98, STEP_FIELD(s0p3)},
    {0x1425ddf4c, STEP_FIELD(s0p1333)},
    {0x1425ddf50, STEP_FIELD(s0p1667)},
    {0x142598578, STEP_FIELD(s0p2)},
    {0x142592228, STEP_FIELD(s0p25)},
    {0x1425ddf60, STEP_FIELD(s0p35)},
    {0x14259bddc, STEP_FIELD(dist1p5)},
    {0x14259bdd8, STEP_FIELD(dist1p25)},
    {0x142572348, STEP_FIELD(clamp0p8)},
    {0x1425a8a28, STEP_FIELD(step0p05)},
    {0x14259bdc4, STEP_FIELD(eps1e7)},
    {0x14259ce38, STEP_FIELD(kmh3p6)},
    {0x1425ab124, STEP_FIELD(rate0p95)},
    {0x142588210, STEP_FIELD(deg10)},
    {0x14259be50, STEP_FIELD(deg78p75)},
    {0x1425ab120, STEP_FIELD(s0p85)},
    {0x14259bdec, STEP_FIELD(six)},
    {0x14259bdd4, STEP_FIELD(threeQuarters)},
    {0x14257a194, STEP_FIELD(s0p7)},   // 0.7: FUN_1406c1070 0x1406c1411, FUN_1406c6480 0x1406c6b48
    {0x1425656c0, STEP_FIELD(four)},   // FUN_1406c6480: comiss at 0x1406c6b18
    {0x1425c48a0, STEP_FIELD(s0p15)},  // FUN_1406c6480: movss at 0x1406c6bb8
};
#undef STEP_FIELD

bool ReadI32Array(const PeImage& exe, uint64_t va, int32_t* out, size_t n) {
  return exe.ReadVa(va, 4 * n, reinterpret_cast<uint8_t*>(out));
}
}  // namespace

std::optional<StepTables> LoadStepTables(const PeImage& exe, std::string* error) {
  StepTables t;
  if (!ReadI32Array(exe, STEP_COLUMN_TABLE_VA, t.bucketColumn.data(), t.bucketColumn.size())) {
    *error = "movestep step: the bucket->column table is unreadable";
    return std::nullopt;
  }
  if (!ReadI32Array(exe, STEP_GRID_TABLE_VA, t.kindBandRow.data(), t.kindBandRow.size())) {
    *error = "movestep step: the kind/band grid is unreadable";
    return std::nullopt;
  }
  uint8_t degrees[sizeof(t.bucketDegrees)];
  if (!exe.ReadVa(STEP_BUCKET_DEGREES_VA, sizeof degrees, degrees)) {
    *error = "movestep step: the bucket->degrees table is unreadable";
    return std::nullopt;
  }
  std::memcpy(t.bucketDegrees.data(), degrees, sizeof degrees);
  // FUN_1406b70d0's distance rows (StepTables' turn rows), four floats each.
  constexpr uint64_t TURN_FRONT_VA = 0x1425dde70, TURN_BACK_VA = 0x1425dde80, TURN_CENTRE_VA = 0x1425dde90;
  if (!exe.ReadVa(TURN_FRONT_VA, sizeof t.turnFront, reinterpret_cast<uint8_t*>(t.turnFront.data())) ||
      !exe.ReadVa(TURN_BACK_VA, sizeof t.turnBack, reinterpret_cast<uint8_t*>(t.turnBack.data())) ||
      !exe.ReadVa(TURN_CENTRE_VA, sizeof t.turnCentre, reinterpret_cast<uint8_t*>(t.turnCentre.data()))) {
    *error = "movestep step: the run-turn distance rows are unreadable";
    return std::nullopt;
  }
  return t;
}

std::optional<StepScalars> LoadStepScalars(const PeImage& exe, std::string* error) {
  StepScalars s;
  for (const auto& site : STEP_SCALAR_SITES) {
    if (!exe.ReadVa(site.va, sizeof(float), reinterpret_cast<uint8_t*>(&(s.*site.field)))) {
      *error = "movestep step: a scalar is unreadable";
      return std::nullopt;
    }
  }
  return s;
}

// DAT_14344e0e0: the style-to-group map, 17 little-endian i32 entries (docs/re/pes-re-anim-select-2.md
// 2.3, read from the image). The install test checks each style names a blend group with members.
std::optional<ParametricGroups> LoadParametricGroups(const PeImage& exe, std::string* error) {
  constexpr uint64_t PARAMETRIC_GROUP_MAP_VA = 0x14344e0e0;
  std::array<int32_t, kStyleCount> raw{};
  if (!exe.ReadVa(PARAMETRIC_GROUP_MAP_VA, sizeof raw, reinterpret_cast<uint8_t*>(raw.data()))) {
    *error = "movestep: the parametric group map is unreadable";
    return std::nullopt;
  }
  ParametricGroups g;
  for (int i = 0; i < kStyleCount; ++i) g.group[i] = raw[i];
  return g;
}

void RecordIssue(IssuedClip& issued, const MotionRequest& request, const ParametricGroups& groups) {
  issued.request = request;
  if (request.group >= 0) {
    assert(request.group < kStyleCount);  // the handlers issue only the table's style indices
    issued.request.group = groups.group[request.group];
  }
  ++issued.serial;
}


}  // namespace pes
