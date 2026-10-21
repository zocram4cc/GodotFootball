// PES 2021's penalty shootout aim (docs/ports/pes-port-PenaltyScatter.md): the
// taker's aim is the kick-kind-54 scatter around the base point, not a roll of
// the outcome. Synthetic constants pin the arithmetic, as pes_penalty_scatter_test
// does; the install loader is tested there.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "onthepitch/penaltyshootout.hpp"
#include "onthepitch/pespenaltyscatter.hpp"
#include "onthepitch/pesrandom.hpp"

namespace {

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
  k.qLateralWeight = 0.1f;
  k.lateralGain = 0.15f;
  k.qHeightWeight = 0.2f;
  k.heightGain = 0.3f;
  k.lateralDivisor = 3.0f;
  k.heightRatio = 0.25f;
  k.topRatio = 0.9f;
  k.mmPerUnit = 1000.0f;
  return k;
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

// Both abilities at the ramp's top and a full charge: the perfect taker.
pes::PenaltyScatterInput PerfectTaker() {
  pes::PenaltyScatterInput in;
  in.placeKicking = 99.0f;
  in.finishing = 99.0f;
  in.charge = 1.0f;
  in.postScale = 3.7f;  // goalHalfWidth (gametypes.hpp)
  in.barScale = 2.5f;   // goalHeight (gametypes.hpp)
  return in;
}

}  // namespace

TEST(PenaltyShootoutAim, APerfectTakersAimStaysInsideTheScatterRadius) {
  const pes::PenaltyScatterConstants k = Synthetic();
  const pes::RngConstants rngK = SyntheticRng();
  for (uint32_t seed = 1; seed <= 200; seed++) {
    pes::Random rng(rngK);
    rng.SetSeed(2, seed);
    const PenaltyShootout::KickAim aim = PenaltyShootout::AimPenaltyKick(k, rng, PerfectTaker());
    ASSERT_GT(aim.radius_m, 0.0f);
    // The base point is the goal's centre line, so the lateral error is the aim.
    EXPECT_LE(std::fabs(aim.lateral_m), aim.radius_m) << "seed " << seed;
    EXPECT_GE(aim.height_m, 0.0f) << "seed " << seed;
  }
}

TEST(PenaltyShootoutAim, ThePkSpecialistCardTightensTheRadius) {
  const pes::PenaltyScatterConstants k = Synthetic();
  const pes::RngConstants rngK = SyntheticRng();
  pes::Random rng(rngK);
  pes::PenaltyScatterInput plain = PerfectTaker();
  plain.placeKicking = 60.0f;
  plain.finishing = 60.0f;
  plain.charge = 0.8f;
  pes::PenaltyScatterInput card = plain;
  card.penaltySpecialist = true;
  const float plainRadius = PenaltyShootout::AimPenaltyKick(k, rng, plain).radius_m;
  const float cardRadius = PenaltyShootout::AimPenaltyKick(k, rng, card).radius_m;
  EXPECT_LT(cardRadius, plainRadius);
}

TEST(PenaltyShootoutAim, TheKickDrawsFourTimesFromTheTakersSlotTwo) {
  // The lateral and the height each take a sign draw and a magnitude draw
  // (FUN_1406eaa10, called twice by FUN_1406e3210's kick-kind-54 case): four
  // slot-2 draws in all, so the fifth is the next value on the stream.
  const pes::PenaltyScatterConstants k = Synthetic();
  const pes::RngConstants rngK = SyntheticRng();
  pes::Random kicked(rngK);
  pes::Random counted(rngK);
  kicked.SetSeed(2, 7);
  counted.SetSeed(2, 7);
  PenaltyShootout::AimPenaltyKick(k, kicked, PerfectTaker());
  for (int i = 0; i < 4; i++) counted.Range(2);
  EXPECT_EQ(kicked.Range(2), counted.Range(2));
}
