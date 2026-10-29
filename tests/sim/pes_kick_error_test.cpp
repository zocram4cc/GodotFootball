// PES 2021's kick execution error (docs/re/pes-re-kick-scatter.md,
// pes-re-kick-accuracy.md). Synthetic constants pin the arithmetic; the install
// test checks the loader through relations, not transcribed values.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "onthepitch/peskickerror.hpp"
#include "onthepitch/pesrandom.hpp"
#include "pes/exe.hpp"

namespace {

pes::KickErrorConstants Synthetic() {
  pes::KickErrorConstants k{};
  k.qFloorStat = 40; k.qMidStat = 60; k.qTopStat = 95; k.qLowGain = 0.1f; k.qLowSpan = 20;
  k.qMidGain = 0.85f; k.qMidSpan = 35; k.qTopGain = 0.05f; k.qTopQuarter = 0.25f;
  k.qTopBase = 0.95f; k.skillBonus = 5;
  k.angleScale_deg = 22.5f; k.speedScale = 0.3f; k.ampGain = 4.25f; k.ampBase = 0.75f;
  k.half = 0.5f; k.aBase = 0.25f;
  k.minute_s = 60; k.km_m = 1000; k.rampPerQuality = 10; k.rampLo_kmh = 60;
  k.groundHi_kmh = 85; k.otherHi_kmh = 80; k.wQuality = 0.95f; k.wGround = 0.3f;
  k.wOther = 0.375f; k.pitchLo_deg = 20; k.pitchHi_deg = 30;
  k.powerLo = 0.2f; k.powerSpan = 0.8f; k.shootPowerWeight = 0.25f;
  k.rWeight = 0.4f; k.dBase = 0.8f; k.contactLo = -20; k.contactSpan = 40; k.contactHi = 20;
  k.nearOpp_m = 1; k.farOpp_m = 2; k.dWeight = 1.5f;
  k.normalDiv = std::sqrt(-2.0f * std::log(1.0f / 512.0f)); k.normalCentre = 50;
  k.normalTabScale = 1.0f / 4096.0f; k.normalNegTwo = -2; k.normalTabSteps = 512;
  k.normalDraw = 1000; k.fullTurn_deg = 360; k.quarterTurn_deg = 90;
  for (int i = 1; i < pes::kNormalTableSize; ++i)
    k.logTable[i] = -std::log(512.0f / static_cast<float>(i)) * 4096.0f;
  k.logTable[0] = k.logTable[1];
  // The quarter-wave table: entry i is sin(i * 90 / 512 degrees), entry 512 is sin 90.
  for (int i = 0; i < pes::kNormalTableSize; ++i)
    k.sinTable[i] = std::sin(static_cast<float>(i) * 90.0f / 512.0f * 3.14159265f / 180.0f);
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

TEST(PesKickError, QualityCurveFollowsTheThreeSegments) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::KickQuality(k, 30), 0.0f);
  EXPECT_FLOAT_EQ(pes::KickQuality(k, 50), 0.05f);
  EXPECT_NEAR(pes::KickQuality(k, 77.5f), 0.525f, 1e-5);
  EXPECT_NEAR(pes::KickQuality(k, 99), 1.0f, 1e-5);
}

TEST(PesKickError, SoftMinLeansToTheWorstFactor) {
  EXPECT_FLOAT_EQ(pes::SoftMin4(1, 1, 1, 1), 1.0f);
  EXPECT_FLOAT_EQ(pes::SoftMin4(1, 0.5f, 1, 1), 0.5f);
  EXPECT_FLOAT_EQ(pes::SoftMin4(1, 0.5f, 0.5f, 1), 0.375f);
}

TEST(PesKickError, SoftMinFoldsTheSortedFactorsFromOne) {
  // FUN_1408413c0 qsorts the four factors descending (comparator 0x14083cc10 is
  // -1 when a > b), then folds from 1.0: x = min(x,v) * (max(x,v) + 1) / 2,
  // once per factor. Hand-folded for {1.0, 0.9, 0.5, 0.2}: 0.9, 0.475, 0.1475.
  EXPECT_FLOAT_EQ(pes::SoftMin4(1.0f, 0.9f, 0.5f, 0.2f), 0.1475f);
  // Order of the arguments does not matter: the sort fixes it.
  EXPECT_FLOAT_EQ(pes::SoftMin4(0.2f, 1.0f, 0.9f, 0.5f), 0.1475f);
}

TEST(PesKickError, AmpSoftMinFoldsThreeNonUnitFactorsInPesOrder) {
  // a = 0.5 gives A = 0.75a + 0.25 = 0.625. The yaw amp soft-min takes
  // {I = 0.9, A = 0.625, F = 1, R = 0.6}; the descending fold from 1.0 gives
  // q4 = 0.475 (1 -> 0.9 -> 0.59375 -> 0.475).
  const auto k = Synthetic();
  const auto s = pes::BuildScatter(k, 0.5f, 0.9f, 0.6f, 1.0f);
  const float q4 = 0.475f;
  EXPECT_NEAR(s.yaw.amp, (1.0f - q4) * (1.0f - q4) * 4.25f + 0.75f, 1e-4);
}

TEST(PesKickError, PerfectKickHasNoSpreadAndAPoorKickerAWideOne) {
  const auto k = Synthetic();
  const auto good = pes::BuildScatter(k, 1.0f, 1.0f, 1.0f, 1.0f);
  EXPECT_FLOAT_EQ(good.yaw.scale, 0.0f);
  EXPECT_FLOAT_EQ(good.yaw.amp, 0.75f);
  EXPECT_FLOAT_EQ(good.yaw.pSign, 0.5f);
  const auto poor = pes::BuildScatter(k, 0.0f, 1.0f, 1.0f, 1.0f);
  // A = 0.25, q4 = 0.25, amp = 0.75^2 * 4.25 + 0.75.
  EXPECT_NEAR(poor.yaw.amp, 0.5625f * 4.25f + 0.75f, 1e-5);
  // A hurried, pressed kick widens the scale.
  const auto pressed = pes::BuildScatter(k, 1.0f, 0.5f, 1.0f, 1.0f);
  EXPECT_NEAR(pressed.yaw.scale, 0.5f * 22.5f, 1e-4);
  EXPECT_NEAR(pressed.speed.scale, 0.5f * 0.3f, 1e-5);
}

TEST(PesKickError, NormalWithZeroAmpIsCentredAndWiderAmpSpreads) {
  const auto k = Synthetic();
  const auto rc = SyntheticRng();
  pes::Random rng(rc);
  rng.SetSeed(pes::kKickRngSlot, 12345);
  for (int i = 0; i < 50; ++i) EXPECT_EQ(pes::Normal(rng, k, 0.0f), 50);
  auto meanDev = [&](float amp) {
    float sum = 0;
    for (int i = 0; i < 2000; ++i) {
      const int n = pes::Normal(rng, k, amp);
      EXPECT_GE(n, 0);
      EXPECT_LE(n, 99);
      sum += std::fabs(n - 50.0f);
    }
    return sum / 2000;
  };
  EXPECT_LT(meanDev(0.75f), meanDev(5.0f));
}

TEST(PesKickError, PressureOnlyFromAnOpponentWithinReach) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::PressureFactor(k, 70, 70, 3.0f), 1.0f);
  // Equal contact: D = 0.8 - 0.4*0.5 = 0.6, R = 1 - 0.4*1.5*0.6.
  EXPECT_NEAR(pes::PressureFactor(k, 70, 70, 0.5f), 1.0f - 0.36f, 1e-5);
  // The stronger kicker shrugs more of it off.
  EXPECT_GT(pes::PressureFactor(k, 90, 60, 0.5f), pes::PressureFactor(k, 60, 90, 0.5f));
}

TEST(PesKickError, LaunchPenaltyRampsWithSpeedAndPower) {
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::PassLaunchPenalty(k, 0.5f, 10.0f, 0.0f, true), 1.0f);
  // 120 km/h is past every window: 1 - w, w = (1 - 0.95*0.5) * 0.3.
  EXPECT_NEAR(pes::PassLaunchPenalty(k, 0.5f, 120.0f / 3.6f, 0.0f, true),
              1.0f - 0.525f * 0.3f, 1e-5);
  // A steep ball ramps on elevation alone.
  EXPECT_LT(pes::PassLaunchPenalty(k, 0.5f, 10.0f, 30.0f, false), 1.0f);
  EXPECT_FLOAT_EQ(pes::ShotLaunchPenalty(k, 0.1f), 1.0f);
  EXPECT_NEAR(pes::ShotLaunchPenalty(k, 1.0f), 0.75f, 1e-6);
}

TEST(PesKickError, DrawWithZeroScaleLeavesTheKickAlone) {
  const auto k = Synthetic();
  const auto rc = SyntheticRng();
  pes::Random rng(rc);
  const auto e = pes::DrawKickError(k, rng, pes::BuildScatter(k, 1, 1, 1, 1), false);
  EXPECT_FLOAT_EQ(e.yaw_deg, 0.0f);
  EXPECT_FLOAT_EQ(e.pitch_deg, 0.0f);
  EXPECT_FLOAT_EQ(e.speedFactor, 1.0f);
}

TEST(PesKickError, DrawStaysInsideTheScaleAndHalvesLowPitch) {
  const auto k = Synthetic();
  const auto rc = SyntheticRng();
  pes::Random rng(rc);
  rng.SetSeed(pes::kKickRngSlot, 777);
  const auto s = pes::BuildScatter(k, 0.3f, 0.6f, 0.8f, 1.0f);
  bool sawYaw = false;
  for (int i = 0; i < 500; ++i) {
    const auto e = pes::DrawKickError(k, rng, s, true);
    EXPECT_LE(std::fabs(e.yaw_deg), s.yaw.scale + 1e-4f);
    EXPECT_LE(e.pitch_deg, s.pitch.scale + 1e-4f);
    EXPECT_GE(e.pitch_deg, -0.5f * s.pitch.scale - 1e-4f);
    EXPECT_LE(std::fabs(e.speedFactor - 1.0f), s.speed.scale + 1e-4f);
    sawYaw |= std::fabs(e.yaw_deg) > 0.1f;
  }
  EXPECT_TRUE(sawYaw);
}

TEST(PesKickError, ApplyTurnsLiftsAndScales) {
  float v[3] = {10, 0, 0};
  pes::ApplyKickError({90.0f, 0.0f, 1.0f}, v);
  EXPECT_NEAR(v[0], 0.0f, 1e-4);
  EXPECT_NEAR(v[1], 10.0f, 1e-4);
  float w[3] = {10, 0, 0};
  pes::ApplyKickError({0.0f, 45.0f, 0.5f}, w);
  EXPECT_NEAR(w[0], 5.0f / std::sqrt(2.0f), 1e-4);
  EXPECT_NEAR(w[2], 5.0f / std::sqrt(2.0f), 1e-4);
}

TEST(PesKickError, SinTableIsDegreesScaledByAmpAndWrapped) {
  // 0x140a92c80: sin in degrees, times amp. An angle at or past one lap wraps once;
  // past 180 the sine flips sign. Runs PES's translated code over the table.
  const auto k = Synthetic();
  EXPECT_FLOAT_EQ(pes::SinTableDeg(k, 90, 1), 1.0f);
  EXPECT_NEAR(pes::SinTableDeg(k, 30, 1), 0.5f, 1e-3);
  EXPECT_NEAR(pes::SinTableDeg(k, 30, 3), 1.5f, 3e-3);
  EXPECT_NEAR(pes::SinTableDeg(k, 210, 1), -0.5f, 1e-3);
  EXPECT_NEAR(pes::SinTableDeg(k, -150, 1), -0.5f, 1e-3);
  EXPECT_NEAR(pes::SinTableDeg(k, 390, 1), 0.5f, 1e-3);
}

TEST(PesKickError, SinTableIsTheSineAtItsKnots) {
  // Knot i sits at 90 * i / 512 degrees; the table holds sin there to float precision.
  const auto k = Synthetic();
  const double kPiRad = std::acos(-1.0) / 180.0;
  for (int i = 0; i < pes::kNormalTableSize - 1; i += 37) {
    const float deg = static_cast<float>(i) * (90.0f / 512.0f);
    EXPECT_NEAR(pes::SinTableDeg(k, deg, 1.0f), std::sin(deg * kPiRad), 1e-5) << "knot " << i;
  }
}

// The soft minimum on PES's own arguments. tools/dev/rendezvous/kick_replay.py replays each kick_scatter capture and
// writes one row per FUN_1408413c0 call (four factors, PES's result). The rows are PES-derived: they live in scratch and
// reach the test through PES_KICK_SOFTMIN_TSV. A TSV with no rows fails: an empty oracle is not a pass.
TEST(PesKickErrorOracle, SoftMinOnPesArgumentsGivesPesResult) {
  const char* path = std::getenv("PES_KICK_SOFTMIN_TSV");
  if (!path) GTEST_SKIP() << "PES_KICK_SOFTMIN_TSV not set";
  std::ifstream in(path);
  ASSERT_TRUE(in) << path;
  std::string line;
  std::getline(in, line);  // header: seq, call, a, b, c, d, out
  int rows = 0;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    std::string seq;
    int call = 0;
    float a = 0, b = 0, c = 0, d = 0, want = 0;
    row >> seq >> call >> a >> b >> c >> d >> want;
    ASSERT_FALSE(row.fail()) << line;
    EXPECT_EQ(pes::SoftMin4(a, b, c, d), want) << seq << " call " << call;
    ++rows;
  }
  EXPECT_GT(rows, 0);
}

TEST(PesKickErrorInstall, LoadsFromTheExe) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
  ASSERT_TRUE(exe) << err;
  auto k = pes::LoadKickErrorConstants(*exe, &err);
  ASSERT_TRUE(k) << err;
  // The curve is continuous at both joints.
  EXPECT_NEAR(k->qLowGain * (k->qMidStat - k->qFloorStat) / k->qLowSpan, k->qLowGain, 1e-6);
  EXPECT_NEAR(k->qLowGain + k->qMidGain * (k->qTopStat - k->qMidStat) / k->qMidSpan,
              k->qTopBase, 1e-5);
  // The table is -ln(512/i) in 2^-12 units, and the divisor is its far end.
  EXPECT_FLOAT_EQ(k->logTable[512], 0.0f);
  EXPECT_FLOAT_EQ(k->sinTable[512], 1.0f);
  EXPECT_FLOAT_EQ(pes::SinTableDeg(*k, 90.0f, 1.0f), 1.0f);
  EXPECT_NEAR(k->logTable[256] * k->normalTabScale, -std::log(2.0f), 1e-3);
  EXPECT_NEAR(std::sqrt(k->normalNegTwo * k->logTable[1] * k->normalTabScale), k->normalDiv,
              1e-3);
  // A perfect kicker's amp is the floor, a poor one's sits inside Normal's range.
  EXPECT_GT(k->ampGain + k->ampBase, k->ampBase);
}

// ---- Situation factor Q (FUN_14083cc30, docs/ports/pes-port-KickQ.md) -----------------------------------------------

std::optional<pes::KickErrorConstants> InstallConstants(std::string* err) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) return std::nullopt;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", err);
  if (!exe) return std::nullopt;
  return pes::LoadKickErrorConstants(*exe, err);
}

float FromBits(const std::string& hex) {
  const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// tools/dev/rendezvous/kick_replay.py --q replays kick_scatter captures (each capture itself, and with each input Q
// reads moved across the thresholds it is tested against) and records the inputs and PES's Q as f32 bits. Rows are
// PES-derived: they stay in scratch and reach the test through PES_KICK_Q_TSV. desc is the motion-descriptor byte PES
// read (-1: subclass 0, 1 or 8..: not read); 1 sends PES into the in-flight branch (0x14083E42E), which is not ported,
// so those rows are not compared. A TSV with no comparable row fails.
TEST(PesKickErrorOracle, SituationFactorEqualsPesBitForBit) {
  const char* path = std::getenv("PES_KICK_Q_TSV");
  if (!path || !std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES_KICK_Q_TSV or PES21_ROOT not set";
  std::string err;
  auto k = InstallConstants(&err);
  ASSERT_TRUE(k) << err;
  std::ifstream in(path);
  ASSERT_TRUE(in) << path;
  std::string line;
  std::getline(in, line);  // header
  int compared = 0, skipped = 0, wrong = 0;
  while (std::getline(in, line)) {
    if (line.rfind("seq\t", 0) == 0) continue;  // a concatenated file's repeated header
    std::istringstream row(line);
    std::string seq, window, vx, vz, stamina, heading, body, l0, l2, a, q;
    int kind, b2754, entry, first, motion, touch, desc;
    uint32_t flags;
    std::getline(row, seq, '\t');
    row >> kind >> b2754 >> entry >> first >> window >> flags >> vx >> vz >> stamina >> heading >> body >> l0 >> l2 >>
        a >> motion >> touch >> desc >> q;
    ASSERT_FALSE(row.fail()) << line;
    constexpr int kDescriptorInFlight = 1;  // cmp eax,1 at 0x14083E42E
    if (desc == kDescriptorInFlight) {
      ++skipped;
      continue;
    }
    pes::KickSituation s;
    s.kind = kind;
    s.b2754 = static_cast<int8_t>(b2754);
    s.entry2755 = static_cast<int8_t>(entry);
    s.firstTime = static_cast<int8_t>(first);
    s.window = FromBits(window);
    s.aiBit = (flags >> 29) & 1;
    s.motion = static_cast<uint16_t>(motion);
    s.touch = touch != 0;
    s.velX = FromBits(vx);
    s.velZ = FromBits(vz);
    s.stamina = FromBits(stamina);
    s.heading_deg = FromBits(heading);
    s.body_deg = FromBits(body);
    s.launchX = FromBits(l0);
    s.launchZ = FromBits(l2);
    s.quality = FromBits(a);
    const float want = FromBits(q);
    const float got = pes::SituationFactor(k->situation, s);
    ++compared;
    if (std::memcmp(&got, &want, sizeof got) != 0) {
      if (++wrong <= 10) ADD_FAILURE() << seq << " kind " << kind << ": got " << got << " want " << want << "\n" << line;
    }
  }
  RecordProperty("compared", compared);
  RecordProperty("skippedDescriptor", skipped);
  EXPECT_GT(compared, 0);
  EXPECT_EQ(wrong, 0) << wrong << " of " << compared << " rows differ";
  std::printf("[ oracle   ] Q rows compared %d, descriptor-branch rows skipped %d\n", compared, skipped);
}

// A still kicker facing his kick, a perfect strike, no window and no bucket loses nothing: Q is the Base of the kind.
// The Base's gate multiplier switches on with the AI bit off and off with it on, by exactly one of the three gate
// multipliers, so every kind of the table must show Base(no AI) = Base(AI) x multiplier or no gate at all.
TEST(PesKickErrorInstall, SituationBaseGateAndAUnhurriedKickKeepsTheBase) {
  std::string err;
  auto k = InstallConstants(&err);
  if (!k && !std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(k) << err;
  const pes::SituationConstants& c = k->situation;
  int gated[3] = {0, 0, 0};
  for (int kind = 1; kind <= 0x96; ++kind) {
    pes::KickSituation s;
    s.kind = kind;
    s.entry2755 = 0x12;  // no bucket
    s.window = c.noWindow;
    s.quality = 1.0f;
    s.launchX = 0.0f;
    s.launchZ = 1.0f;  // bearing 0, the heading
    const float base = pes::KickBase(c, s);
    s.aiBit = true;
    const float ungated = pes::KickBase(c, s);
    s.aiBit = false;
    s.motion = 0x9F1;
    EXPECT_EQ(pes::KickBase(c, s), ungated) << "motion 0x9F1 switches the gate off, kind " << kind;
    s.motion = 0;
    if (base == ungated) {
      ++gated[0];  // no gate for this kind
    } else if (base == ungated * c.p95) {
      ++gated[1];
    } else if (base == ungated * c.p925 || base == ungated * c.p90) {
      ++gated[2];
    } else {
      ADD_FAILURE() << "kind " << kind << ": gate multiplier is none of the install's";
    }
    EXPECT_EQ(pes::SituationFactor(c, s), base) << "a still, aligned, perfect kick keeps the Base, kind " << kind;
  }
  EXPECT_GT(gated[0], 0);
  EXPECT_GT(gated[1], 0);
  EXPECT_GT(gated[2], 0);
}

// A kicker running flat out, striking away from his heading with the stamina accumulator (ap+0x281C) full, loses Q
// against one standing, in every class a kick can be.
TEST(PesKickErrorInstall, SituationFactorFallsWithTheKickersSpeed) {
  std::string err;
  auto k = InstallConstants(&err);
  if (!k && !std::getenv("PES21_ROOT")) GTEST_SKIP() << "PES21_ROOT not set";
  ASSERT_TRUE(k) << err;
  const pes::SituationConstants& c = k->situation;
  for (int kind : {1, 28, 42, 57}) {  // one kind of each of the classes 3, 2, 1, 0
    pes::KickSituation s;
    s.kind = kind;
    s.entry2755 = 0x12;
    s.window = c.noWindow;
    s.quality = 0.5f;
    s.launchZ = -1.0f;  // bearing 180 against heading 0
    s.stamina = 100.0f;
    const float still = pes::SituationFactor(c, s);
    s.velZ = 0.5f;  // m per tick: far above the sprint
    EXPECT_LT(pes::SituationFactor(c, s), still) << "kind " << kind;
  }
}

TEST(PesKickErrorInstall, HeadingIsPesBearing) {
  EXPECT_FLOAT_EQ(pes::PesHeadingDeg(0.0f, 1.0f), 0.0f);
  EXPECT_NEAR(pes::PesHeadingDeg(1.0f, 0.0f), 90.0f, 1e-3);
  EXPECT_NEAR(pes::PesHeadingDeg(0.0f, -1.0f), 180.0f, 1e-3);
  EXPECT_NEAR(pes::PesHeadingDeg(-1.0f, 0.0f), 270.0f, 1e-3);
}

}  // namespace
