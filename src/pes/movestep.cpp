#include "pes/movestep.hpp"

#include "pes/exe.hpp"

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

}  // namespace pes
