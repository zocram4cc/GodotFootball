// PES 2021's penalty aim scatter (docs/re/pes-re-penalty-scatter.md):
// FUN_1406B4000's kick-kind-54 radius and the jitter FUN_1406E3210's
// kick-kind-54 case adds to the aim point. Synthetic constants pin the
// arithmetic; the install test checks the loader through relations, not
// transcribed values.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "onthepitch/pespenaltyscatter.hpp"
#include "onthepitch/pesrandom.hpp"
#include "pes/exe.hpp"

namespace {

// The stock values, so the arithmetic below can be read as the decode reads.
// The loader test re-reads them from the install.
pes::PenaltyScatterConstants Synthetic() {
  pes::PenaltyScatterConstants k{};
  k.placeKickingWeight = 0.6f;
  k.finishingWeight = 0.4f;
  k.accFloorStat = 40.0f;
  k.accTopStat = 99.0f;
  k.accSpan = 59.0f;
  k.baseScatter = 0.5f;
  k.specialistScatter = 0.4f;
  k.chipScatter = 0.2f;
  k.chipScatterNoControl = 0.4f;
  k.qScatterWeight = 0.2f;
  k.qPowerWeight = 0.3f;
  k.powerLo = 0.2f;
  k.powerSpan = 0.8f;
  k.chargeGain = 0.8f;
  k.qHeightWeight = 0.1f;
  k.heightGain = 0.15f;
  k.qLateralWeight = 0.2f;
  k.lateralGain = 0.3f;
  k.aimHeightDivisor = 3.0f;
  k.aimLateralRatio = 0.25f;
  k.aimTopRatio = 0.9f;
  k.mmPerUnit = 1000.0f;
  return k;
}

// A neutral taker: the two abilities at the ramp's floor, no card, no chip,
// the charge at the ramp's floor, and the aim point in the middle of the goal.
pes::PenaltyScatterInput Neutral() {
  pes::PenaltyScatterInput in;
  in.placeKicking = 40.0f;
  in.finishing = 40.0f;
  in.charge = 0.2f;
  in.aimLateral = 0.0f;
  in.aimHeight = 0.0f;
  in.heightScale = 2.44f;   // F3, the bar
  in.lateralScale = 3.66f;  // F2, the post
  return in;
}

pes::RngConstants SyntheticRng() {
  pes::RngConstants r{};
  r.scale = 1.0f / 2147483648.0f;
  r.lcgMul = 0x2d22b2e3;
  r.lcgSub = 0x4513;
  r.modulus = {100, 5000, 32768, 32768};
  r.defaultSeeds = {1, 2, 3, 4};
  return r;
}

// The accuracy core: Place Kicking x 0.6 + Finishing x 0.4, ramped over
// 40..99. At the floor q is 0, at the top q is 1.
TEST(PesPenaltyScatter, TheAccuracyCoreWeightsPlaceKickingAndFinishing) {
  const auto k = Synthetic();

  // Both abilities at the floor: q = 0, so the base is untouched and the
  // charge's term is at its full 0.8.
  pes::PenaltyScatterInput in = Neutral();
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.0f, 1e-5f);

  // Both at the top: q = 1, so the base loses 0.2 and the charge's term is
  // scaled by (1 - 0.3).
  in.placeKicking = 99.0f;
  in.finishing = 99.0f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f - 0.2f, 1e-5f);

  // The weight is on Place Kicking: 0.6 of it, so a Place Kicking of 99 and a
  // Finishing of 40 gives acc = 0.6*99 + 0.4*40 = 75.4, truncated to 75, and
  // q = (75 - 40) / 59.
  in.placeKicking = 99.0f;
  in.finishing = 40.0f;
  const float q = (75.0f - 40.0f) / 59.0f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f - 0.2f * q, 1e-5f);
}

// The Penalty Specialist card drops the base scatter from 0.5 to 0.4.
TEST(PesPenaltyScatter, ThePenaltySpecialistCardTightensTheBase) {
  const auto k = Synthetic();
  pes::PenaltyScatterInput in = Neutral();
  in.penaltySpecialist = true;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.4f, 1e-5f);
}

// The chip shot adds 0.4, or 0.2 with Chip Shot Control.
TEST(PesPenaltyScatter, TheChipShotWidensTheBaseAndControlHalvesIt) {
  const auto k = Synthetic();
  pes::PenaltyScatterInput in = Neutral();
  in.chip = true;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.4f, 1e-5f);

  in.chipShotControl = true;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.2f, 1e-5f);
}

// The charge is the largest single term: 0.8 at the top of its ramp, and the
// accuracy scales it down by up to 0.3.
TEST(PesPenaltyScatter, TheChargeIsTheLargestTerm) {
  const auto k = Synthetic();
  pes::PenaltyScatterInput in = Neutral();
  in.charge = 1.0f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.8f, 1e-5f);

  // At the top of the accuracy ramp the charge's term is 0.8 - 0.3.
  in.placeKicking = 99.0f;
  in.finishing = 99.0f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f - 0.2f + 0.5f, 1e-5f);
}

// Aiming at the post and aiming high add their own terms.
TEST(PesPenaltyScatter, AimingAtThePostAndAimingHighAddTheirTerms) {
  const auto k = Synthetic();

  // The lateral ramp runs from F2 * 0.25 = 0.915 to F2 * 0.9 = 3.294, so a
  // lateral at the post is at the ramp's top and adds the full 0.3.
  pes::PenaltyScatterInput in = Neutral();
  in.aimLateral = 3.66f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.3f, 1e-5f);

  // The height ramp runs from F3 / 3 = 0.813 to F3 * 0.9 = 2.196, so a height
  // at the bar is at the ramp's top and adds the full 0.15.
  in = Neutral();
  in.aimHeight = 2.44f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.15f, 1e-5f);

  // Both together add both.
  in.aimLateral = 3.66f;
  EXPECT_NEAR(pes::PenaltyScatterRadius(k, in), 0.5f + 0.3f + 0.15f, 1e-5f);
}

// The jitter draws two signed values inside the radius, in millimetres.
TEST(PesPenaltyScatter, TheJitterStaysInsideTheRadius) {
  const auto k = Synthetic();
  const auto r = SyntheticRng();
  pes::Random rng(r);

  const float radius = 0.5f;
  pes::PenaltyAim base;
  base.lateral_m = 2.0f;
  base.height_m = 1.0f;

  for (int draw = 0; draw < 200; draw++) {
    const pes::PenaltyAim aim = pes::ApplyPenaltyJitter(rng, k, radius, base);
    EXPECT_LE(std::abs(aim.height_m - base.height_m), radius + 1e-6f);
    EXPECT_LE(std::abs(aim.lateral_m - base.lateral_m), radius + 1e-6f);
  }
}

// The lateral's sum is clamped at zero, the height's is not.
TEST(PesPenaltyScatter, TheLateralIsClampedAtZeroAndTheHeightIsNot) {
  const auto k = Synthetic();
  const auto r = SyntheticRng();
  pes::Random rng(r);

  pes::PenaltyAim base;
  base.lateral_m = 0.0f;
  base.height_m = 0.0f;

  bool sawClampedLateral = false;
  for (int draw = 0; draw < 200; draw++) {
    const pes::PenaltyAim aim = pes::ApplyPenaltyJitter(rng, k, 0.5f, base);
    EXPECT_GE(aim.lateral_m, 0.0f);
    if (aim.lateral_m == 0.0f) sawClampedLateral = true;
    EXPECT_GE(aim.height_m, -0.5f - 1e-6f);
  }
  EXPECT_TRUE(sawClampedLateral);
}

// The install's own bytes: the loader reads the stock values.
TEST(PesPenaltyScatter, TheInstallCarriesTheStockValues) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe.has_value()) << err;

  const auto k = pes::LoadPenaltyScatterConstants(*exe, &err);
  ASSERT_TRUE(k.has_value()) << err;

  // Relations, not transcribed numbers: the accuracy core's weights sum to
  // one, the specialist card tightens the base, the chip terms bracket the
  // control's, and the ramp runs from 40 to 99.
  EXPECT_NEAR(k->placeKickingWeight + k->finishingWeight, 1.0f, 1e-5f);
  EXPECT_LT(k->specialistScatter, k->baseScatter);
  EXPECT_LT(k->chipScatter, k->chipScatterNoControl);
  EXPECT_LT(k->accFloorStat, k->accTopStat);
  EXPECT_NEAR(k->accSpan, k->accTopStat - k->accFloorStat, 1e-4f);
  EXPECT_NEAR(k->mmPerUnit, 1000.0f, 1e-3f);
  EXPECT_GT(k->baseScatter, 0.0f);
  EXPECT_LT(k->baseScatter, 1.0f);
}

}  // namespace
