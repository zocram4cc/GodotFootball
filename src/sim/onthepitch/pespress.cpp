// PES 2021's press command arithmetic (docs/re/pes-re-press-life.md 8).
// See pespress.hpp for what each function is and where its constants live.

#include "onthepitch/pespress.hpp"

#include <algorithm>

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
    {0x14351ae54, &PressConstants::framesPerSec},
    {0x1425a8a28, &PressConstants::speedCut0_kmh},
    {0x14259bdf4, &PressConstants::speedCut1_kmh},
    {0x1425a1ffc, &PressConstants::speedCut2_kmh},
    {0x1425c48c0, &PressConstants::speedCut3_kmh},
    {0x142588200, &PressConstants::targetTickRate},
    {0x142565600, &PressConstants::targetTickBias},
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

}  // namespace pes
