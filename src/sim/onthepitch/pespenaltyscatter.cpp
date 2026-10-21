#include "pespenaltyscatter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace pes {
namespace {

// `lo >= x -> 0, x >= hi -> 1, else (x - lo) / span`: the ramp idiom this
// function's inputs are built from (0x1406b40bf-e1 and the two aim-point
// ramps at 0x1406b41bf-0x1406b4273).
float Ramp(float x, float lo, float hi, float span) {
  if (lo >= x) return 0.0f;
  if (x >= hi) return 1.0f;
  return (x - lo) / span;
}

// FUN_1406eaa10: the signed draw the jitter uses. Two draws from slot 2: the
// first one below 50 of 100 takes the positive branch, the second one is the
// magnitude, its remainder modulo the radius in millimetres.
int SignedDraw(Random& rng, int modulus) {
  const int signDraw = rng.Range(kPenaltyRngSlot);
  const int magnitudeDraw = rng.Range(kPenaltyRngSlot);
  const int magnitude = magnitudeDraw % (modulus > 0 ? modulus : 1);
  return signDraw % 100 < 50 ? magnitude : -magnitude;
}

}  // namespace

std::optional<PenaltyScatterConstants> LoadPenaltyScatterConstants(const PeImage& exe,
                                                                     std::string* error) {
  PenaltyScatterConstants k{};
  bool ok = true;
  const std::pair<uint64_t, float*> rdata[] = {
      {0x14257a190, &k.placeKickingWeight},   {0x14259b89c, &k.finishingWeight},
      {0x14259be2c, &k.accFloorStat},         {0x1425c48cc, &k.accTopStat},
      {0x1425cb2fc, &k.accSpan},              {0x142565600, &k.baseScatter},
      {0x14259b89c, &k.specialistScatter},    {0x142598578, &k.chipScatter},
      {0x14259b89c, &k.chipScatterNoControl}, {0x142598578, &k.qScatterWeight},
      {0x142590d98, &k.qPowerWeight},         {0x142598578, &k.powerLo},
      {0x142572348, &k.powerSpan},            {0x142572348, &k.chargeGain},
      {0x142588200, &k.qLateralWeight},       {0x1425c48a0, &k.lateralGain},
      {0x142598578, &k.qHeightWeight},        {0x142590d98, &k.heightGain},
      {0x14258c264, &k.lateralDivisor},       {0x142592228, &k.heightRatio},
      {0x14258c260, &k.topRatio},             {0x14256ae34, &k.mmPerUnit}};
  for (auto [va, out] : rdata)
    ok &= exe.ReadVa(va, sizeof *out, reinterpret_cast<uint8_t*>(out));
  if (!ok) {
    *error = "penalty scatter constant: .rdata unreadable";
    return std::nullopt;
  }
  return k;
}

float PenaltyScatterRadius(const PenaltyScatterConstants& k, const PenaltyScatterInput& in) {
  // The accuracy core (0x1406b4050-0x1406b40e1): the two abilities, weighted,
  // truncated to a byte and ramped over 40..99.
  const float acc = static_cast<float>(
      static_cast<uint32_t>(in.placeKicking * k.placeKickingWeight +
                            in.finishing * k.finishingWeight) &
      0xffu);
  const float q = Ramp(acc, k.accFloorStat, k.accTopStat, k.accSpan);

  // The scatter base (0x1406b40f5, 0x1406b4116, 0x1406b4132-4f).
  float base = in.penaltySpecialist ? k.specialistScatter : k.baseScatter;
  if (in.chip)
    base += in.chipShotControl ? k.chipScatter : k.chipScatterNoControl;

  // The charge's term (0x1406b41a3-0x1406b41e1): the charge ramped over
  // 0.2..1.0, weighted by 0.8 less the accuracy's own 0.3.
  const float charge = Ramp(in.charge, k.powerLo, k.powerLo + k.powerSpan, k.powerSpan);

  // The two aim-point ramps (0x1406b41bf-0x1406b4273). The first reads the
  // aim record's lateral (+8) as a magnitude against the post, the second its
  // height (+4) against the bar.
  const float lateralLo = in.postScale / k.lateralDivisor;
  const float lateralHi = in.postScale * k.topRatio;
  const float lateral =
      Ramp(std::abs(in.aimLateral), lateralLo, lateralHi, lateralHi - lateralLo);
  const float heightLo = in.barScale * k.heightRatio;
  const float heightHi = in.barScale * k.topRatio;
  const float height = Ramp(std::abs(in.aimHeight), heightLo, heightHi, heightHi - heightLo);

  return (base - q * k.qScatterWeight) + charge * (k.chargeGain - q * k.qPowerWeight) +
         lateral * (k.lateralGain - q * k.qLateralWeight) +
         height * (k.heightGain - q * k.qHeightWeight);
}

PenaltyAim ApplyPenaltyJitter(Random& rng, const PenaltyScatterConstants& k, float radius_m,
                              const PenaltyAim& base) {
  // 0x1406e3421-0x1406e3482: the radius times 1000 is the modulus of two
  // signed draws, the lateral's first and the height's second.
  const int modulus = static_cast<int>(radius_m * k.mmPerUnit);
  const float lateral_mm = static_cast<float>(SignedDraw(rng, modulus)) / k.mmPerUnit;
  const float height_mm = static_cast<float>(SignedDraw(rng, modulus)) / k.mmPerUnit;

  PenaltyAim aim;
  aim.lateral_m = base.lateral_m + lateral_mm;
  // The height's sum is clamped at zero (0x1406e347d-0x1406e3482): the ball
  // cannot be aimed below the ground.
  aim.height_m = std::max(0.0f, base.height_m + height_mm);
  return aim;
}

}  // namespace pes
