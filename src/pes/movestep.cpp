#include "pes/movestep.hpp"

#include "pes/exe.hpp"
#include <cmath>

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

bool RunRateAllowed(float rate, float rateCeiling, const PlaybackConstants& k) {
  return rate <= k.stepBase * rateCeiling;
}

void ApplyTransition(MoveStepRecord& rec, MoveReq& req) {
  if (!req.Flag(moveword::kChangeState)) return;
  rec.Enter(static_cast<MoveState>(req.U(moveword::kNextState) & 0xffu));
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

}  // namespace pes
