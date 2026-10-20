// The MOVESTEP helpers the dispatcher port and the handler groups share. Each body names its PES
// site; constants come from the install and the install test checks their relations, while the
// synthetic tests pass their own.
#include "pes/movestep.hpp"

#include <cmath>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include "pes/exe.hpp"
#include "pes/uniform.hpp"
#include "pes/vfs.hpp"

namespace pes {

// a - b wrapped into (-180, 180]: the difference PES computes inline (SUBSS), not a FUN_14043a0a0 call.
float InlineDiffDeg(float a, float b, const RunConstants& k) {
  float d = a - b;
  if (d > 0.0f) {
    if (d >= k.full) d += k.wrapFull;
  } else {
    d += k.full;
  }
  if (k.half_turn < d) {
    const float sign = d < 0.0f ? k.neg : k.one;
    d -= sign * k.full;
  }
  return d;
}

// FUN_14043a0a0 as PES has it: both folded into [0, 360), then b - a, wrapped by 360 once when its magnitude passes 180.
float PesAngleDiff(float a, float b, const RunConstants& k) {
  float d = FoldFull(b, k) - FoldFull(a, k);
  const float magnitude = d < 0.0f ? -d : d;
  if (k.half_turn < magnitude) d -= (d < 0.0f ? k.neg : k.one) * k.full;
  return d;
}

// The 360 fold of a heading into [0, 360).
float FoldFull(float a, const RunConstants& k) {
  if (a >= 0.0f) {
    if (k.full <= a) return a + k.wrapFull;
    return a;
  }
  return a + k.full;
}

namespace movestep {
namespace {
// A unit conversion: atan2 gives radians, the PES headings are degrees.
constexpr float kDegPerRadian = 180.0f / 3.14159265358979323846f;
// FUN_140a92d40's heading of a ground vector in PES's frame (x, z), folded into [0, 360).
float TargetBearingDeg(float dx, float dz, const RunConstants& k) {
  return FoldFull(std::atan2(dx, dz) * kDegPerRadian, k);
}
// FUN_14043a0a0: an angle difference wrapped into (-180, 180].
float TargetTurnDeg(float deg, const RunConstants& k) {
  if (k.half_turn < deg) return deg - k.full;
  if (deg < -k.half_turn) return deg + k.full;
  return deg;
}
}  // namespace

void FillMoveTarget(MoveReq& req, const MoveTargetInput& in, const RunConstants& k,
                    const AngleBucketConstants& buckets) {
  const float toTargetX = in.targetX - in.originX;
  const float toTargetZ = in.targetZ - in.originZ;
  const float targetDist = std::sqrt(toTargetX * toTargetX + toTargetZ * toTargetZ);
  const float targetHeading =
      targetDist > 0.0f ? TargetBearingDeg(toTargetX, toTargetZ, k) : in.facingDeg;
  const float toLookX = in.lookX - in.originX;
  const float toLookZ = in.lookZ - in.originZ;
  const float lookDist = std::sqrt(toLookX * toLookX + toLookZ * toLookZ);
  const float lookHeading = lookDist > 0.0f ? TargetBearingDeg(toLookX, toLookZ, k) : in.facingDeg;
  // The body direction is the facing (req[4]); see MoveTargetInput's ponytail.
  const float body = in.facingDeg;
  const float turnToFacing = TargetTurnDeg(targetHeading - in.facingDeg, k);  // req[5]
  const float turnToBody = TargetTurnDeg(targetHeading - body, k);           // req[6]
  const float lookError = TargetTurnDeg(lookHeading - in.facingDeg, k);      // req[7]
  req.SetF(moveword::kHeading, targetHeading);
  req.SetF(moveword::kFacing, in.facingDeg);
  req.SetF(moveword::kLookHeading, lookHeading);
  req.SetF(moveword::kBodyDirection, body);
  req.SetF(moveword::kLookDelta, turnToFacing);
  req.SetF(moveword::kTurn, turnToBody);
  req.SetF(moveword::kFacingError, lookError);
  req.SetU(moveword::kMoveBucket,
           static_cast<uint32_t>(AngleBucket(buckets, turnToBody, 10, false)));
  req.SetU(moveword::kLookBucket,
           static_cast<uint32_t>(AngleBucket(buckets, turnToFacing, 10, false)));
  req.SetU(moveword::kLookBucketAlt,
           static_cast<uint32_t>(AngleBucket(buckets, lookError, 10, false)));
  req.SetF(moveword::kDistToTarget, targetDist);
  req.SetF(moveword::kDistToLook, targetDist);  // ponytail: req[0x12] is req[0x11] (see the header)
  req.SetU(moveword::kCommandSpeedClass, static_cast<uint32_t>(in.speedBand));
  req.SetF(moveword::kDesiredSpeedKmh, in.desiredKmh);
  req.SetU(moveword::kPrecise, in.precise ? 1u : 0u);
  req.SetF(moveword::kMoveTargetX, in.targetX);  // FUN_1406b7560: param_2[0x1a6] = the target's x
  req.SetF(moveword::kMoveTargetZ, in.targetZ);  // param_2[0x1a8] = the target's z
}
}  // namespace movestep

namespace {
// FUN_1406b9e80's table: ten records at 0x1425de010, stored low speed first; PES walks them high
// speed first, so the loader reverses the record order. The kind scale is 0x1425ddf5c.
constexpr uint64_t SPEED_RATE_TABLE_VA = 0x1425de010;
constexpr uint64_t SPEED_RATE_KIND_SCALE_VA = 0x1425ddf5c;

// Group A's install scalars, by VA (StayDecision FUN_1406b6bb0, IdleRow FUN_1406b93a0 and the
// idle tails).
struct MoveScalar {
  uint64_t va;
  float MoveConstants::* field;
};
constexpr MoveScalar MOVE_SCALARS[] = {
    {0x14259bde4, &MoveConstants::dist2m5},
    {0x14258c264, &MoveConstants::dist3},
    {0x142588210, &MoveConstants::near01},
    {0x14259bdf0, &MoveConstants::ball1p4},
    {0x1425c5100, &MoveConstants::lookScale},
    {0x1425ddff0, &MoveConstants::lookLow},
    {0x1425ddf68, &MoveConstants::lookHigh},
    {0x1425ddfe4, &MoveConstants::rootLow},
    {0x1425ddf58, &MoveConstants::rootHigh},
    {0x142591140, &MoveConstants::demoSpan},
};

// The idle row tables, by VA, each four row ids (FUN_1406b93a0's exits).
struct IdleRowSite {
  uint64_t va;
  std::array<int32_t, 4> IdleRowTables::* row;
};
constexpr IdleRowSite IDLE_ROW_SITES[] = {
    {0x1425ddc58, &IdleRowTables::fatigue},
    {0x1425ddc48, &IdleRowTables::flag},
    {0x1425ddbf0, &IdleRowTables::kind2},
    {0x1425ddc10, &IdleRowTables::mode},
    {0x1425ddc00, &IdleRowTables::mode2},
    {0x1425ddcc8, &IdleRowTables::slot},
    {0x1425ddcd0, &IdleRowTables::slotAlt},
    {0x1425ddc40, &IdleRowTables::kind6},
    {0x1425ddbe0, &IdleRowTables::near},
    {0x1425ddbd0, &IdleRowTables::far},
};
}  // namespace

std::optional<MoveConstants> LoadMoveConstants(const PeImage& exe, std::string* error) {
  MoveConstants k{};
  for (const MoveScalar& s : MOVE_SCALARS) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "move constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

std::optional<IdleRowTables> LoadIdleRowTables(const PeImage& exe, std::string* error) {
  IdleRowTables t{};
  for (const IdleRowSite& s : IDLE_ROW_SITES) {
    if (!exe.ReadVa(s.va, sizeof(std::array<int32_t, 4>),
                    reinterpret_cast<uint8_t*>(&(t.*(s.row))))) {
      *error = "idle row tables: .rdata unreadable";
      return std::nullopt;
    }
  }
  return t;
}

std::optional<SpeedRateTable> LoadSpeedRateTable(const PeImage& exe, std::string* error) {
  std::array<std::array<float, 4>, 10> stored{};
  if (!exe.ReadVa(SPEED_RATE_TABLE_VA, sizeof stored,
                  reinterpret_cast<uint8_t*>(stored.data()))) {
    *error = "speed->rate table: .rdata unreadable";
    return std::nullopt;
  }
  SpeedRateTable t;
  for (size_t i = 0; i < stored.size(); ++i) t.record[i] = stored[stored.size() - 1 - i];
  if (!exe.ReadVa(SPEED_RATE_KIND_SCALE_VA, sizeof(float),
                  reinterpret_cast<uint8_t*>(&t.kindScale))) {
    *error = "speed->rate table: kind scale unreadable";
    return std::nullopt;
  }
  return t;
}

namespace {
// FUN_1406ba2f0's .rdata reads, by VA; each field's comment names its site.
struct StyleSite {
  uint64_t va;
  float StyleConstants::* field;
};
constexpr StyleSite STYLE_SITES[] = {
    {0x1425ddf74, &StyleConstants::rate0p895}, {0x1425ddf6c, &StyleConstants::rate0p825},
    {0x14259be10, &StyleConstants::speed16},   {0x14253ee30, &StyleConstants::heightDiv},
    {0x14256ae34, &StyleConstants::kmhScale},  {0x14259be1c, &StyleConstants::sampleTicks},
    {0x14259be40, &StyleConstants::radToDeg},
};
}  // namespace

std::optional<StyleConstants> LoadStyleConstants(const PeImage& exe, std::string* error) {
  StyleConstants k{};
  for (const StyleSite& s : STYLE_SITES) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "style constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}
namespace {
// LateralDamping's .rdata reads, by VA; each field's comment names its site.
struct DampingSite {
  uint64_t va;
  float DampingConstants::* field;
};
constexpr DampingSite DAMPING_SITES[] = {
    {0x142590d98, &DampingConstants::rateGate},  {0x1425ccc30, &DampingConstants::rateSpan},
    {0x14257a194, &DampingConstants::rateCap},   {0x142572348, &DampingConstants::rateBase},
    {0x142598578, &DampingConstants::rateScale}, {0x14256ae30, &DampingConstants::driftSpeed},
    {0x14257a190, &DampingConstants::motionRate}, {0x1425ddf84, &DampingConstants::angleGate},
};
// The constant registry's freemove.o entry (see DampingRecord): the record's table name.
constexpr const char* DAMPING_RECORD_OBJECT = "freemove.o";
constexpr const char* CONSTANT_PLAYER_PATH = "common/match/constant/constant_player.bin";
}  // namespace

std::optional<DampingConstants> LoadDampingConstants(const PeImage& exe, std::string* error) {
  DampingConstants k{};
  for (const DampingSite& s : DAMPING_SITES) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "damping constants: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

std::optional<DampingRecord> LoadDampingRecord(const PesVfs& vfs, std::string* error) {
  auto blob = vfs.Read(CONSTANT_PLAYER_PATH, error);
  if (!blob) return std::nullopt;
  auto table = ParseUniformTable(*blob, error);
  if (!table) return std::nullopt;
  auto span = table->Find(DAMPING_RECORD_OBJECT);
  if (!span || span->size() < 0x20 + 3 * sizeof(float)) {
    *error = std::string(DAMPING_RECORD_OBJECT) + " is missing or short";
    return std::nullopt;
  }
  // The freemove bounds block: the record's +0x20 floats are stock {20.0, 18.0, 67.5} (dt18_all.cpk;
  // the 4cc mod reads {42.0, 18.0, 69.5}), the only plausible bounds in the object
  // (pes-re-a3helpers.md addendum item 2: the live path FUN_141e5ca20(0x2e)+0x10..0x18 reads
  // denormals, not these).
  // ponytail: no PES path to this record's bounds is known; the live registry path reads
  // the wrong object, so this block stands until that PES path is decoded.
  auto f = [&span](size_t at) {
    float v;
    std::memcpy(&v, span->data() + at, sizeof v);
    return v;
  };
  return DampingRecord{f(0x20), f(0x24), f(0x28)};
}


namespace movestep {

namespace {
// FUN_1406b97d0's nine float arms, by VA (AS 2.3). Each VA is the .rdata float the switch returns.
struct MotionStartSite {
  uint64_t va;
  float MotionStartConstants::* field;
};
constexpr MotionStartSite MOTION_START_SITES[] = {
    {0x142590d98, &MotionStartConstants::v590d98}, {0x142592228, &MotionStartConstants::v592228},
    {0x142565600, &MotionStartConstants::v565600}, {0x142588200, &MotionStartConstants::v588200},
    {0x142598578, &MotionStartConstants::v598578}, {0x1425c48a0, &MotionStartConstants::v5c48a0},
    {0x14259d5c8, &MotionStartConstants::v59d5c8}, {0x1425a8a28, &MotionStartConstants::v5a8a28},
    {0x14257a190, &MotionStartConstants::v57a190},
};
}  // namespace

std::optional<MotionStartConstants> LoadMotionStartConstants(const PeImage& exe,
                                                             std::string* error) {
  MotionStartConstants k{};
  for (const MotionStartSite& s : MOTION_START_SITES) {
    if (!exe.ReadVa(s.va, sizeof(float), reinterpret_cast<uint8_t*>(&(k.*(s.field))))) {
      *error = "motion start: .rdata unreadable";
      return std::nullopt;
    }
  }
  return k;
}

// FUN_1406b97d0(motion): the per-motion switch. Each motion number selects one arm (the field
// comments list them); every motion outside the switch starts at zero.
float MotionStartSeconds(const MotionStartConstants& k, uint16_t motionNo) {
  switch (motionNo) {
    case 0x1f: case 0x6e4: case 0x6e6: case 0x6e7:
      return k.v590d98;
    case 0x24: case 0xb16: case 0xbba: case 0xbbb: case 0xbbc: case 0xc05:
      return k.v592228;
    case 0x27: case 0xbc3:
      return k.v565600;
    case 0x23: case 0x26: case 0x46: case 0x6cb: case 0x6cf: case 0x6dc: case 0xc4c: case 0xc7c:
    case 0xb0e: case 0xb13: case 0xb17: case 0xb18: case 0xb1b: case 0xb1c: case 0xb25: case 0xb28:
    case 0xb29: case 0xb2c: case 0xb3e:
      return k.v588200;
    case 0x2b: case 0x6d9: case 0x6ec: case 0x6ef: case 0xb1d: case 0xbb7: case 0xc4a: case 0xc4e:
      return k.v598578;
    case 0x45: case 0xb0d: case 0xb14: case 0xb1a: case 0xb2e: case 0xb30: case 0xb31: case 0xb36:
      return k.v5c48a0;
    case 0xb11:
      return k.v59d5c8;
    case 0xb1e: case 0xb27: case 0xb2b: case 0xb35: case 0xb3d:
      return k.v5a8a28;
    case 0xe24: case 0xe29: case 0xe2b:
      return k.v57a190;
    default:
      return 0.0f;
  }
}

// FUN_1406b9e80(ap, speed km/h, kind): the playback rate for a ground speed. The speed is a
// m/s value divided by the physique scale, then the first record from the top whose vLo holds
// it gives the piecewise-linear rate.
float SpeedToRate(MoveHost& host, float speedKmh, int kind) {
  const RunConstants& k = host.RunConstantsView();
  const SpeedRateTable& t = host.SpeedRateTableView();
  const float v = (speedKmh / k.speed3600) / host.PhysiqueG();
  for (size_t i = 0; i < t.record.size(); ++i) {
    const float vHi = t.record[i][0];
    const float vLo = t.record[i][1];
    if (vLo <= v) {
      float share = (v - vLo) / (vHi - vLo);
      if (k.one < share) share = k.one;
      float rate = 0.0f;
      if (i + 1 == t.record.size() && (kind == 3 || kind == 6)) {
        rate = share * t.kindScale;
      } else {
        const float rHi = t.record[i][2];
        const float rLo = t.record[i][3];
        rate = rLo + share * (rHi - rLo);
      }
      if (k.one < rate) return k.one;
      return 0.0f <= rate ? rate : 0.0f;
    }
  }
  return 0.0f;
}
}  // namespace movestep

}  // namespace pes
