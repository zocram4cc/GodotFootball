#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pes/fpk.hpp"
#include "pes/fox2.hpp"
#include "pes/lighting.hpp"
#include "pes/vfs.hpp"

namespace {

// The report's fitted sun anchors (docs/re/pes-re-lighting.md §3.3): the 4cc
// st071 rig (lat 45, lon 90, gmt 1, 2019-03-06, 17:30, north 90): the model
// (0.5672, 0.7051, −0.4256) vs the observed (0.56439, 0.70551, −0.42862).
pes::Atmosphere LiveAtmosphere() {
  pes::Atmosphere a;
  a.latitude = 45;
  a.longitude = 90;
  a.gmtTimeDifference = 1;
  a.year = 2019;
  a.month = 3;
  a.day = 6;
  a.northAngle = 90;
  return a;
}

TEST(LightingSun, LivePointMatchesModelAndObservation) {
  const pes::Vec3 toSun = pes::SunToDirection(LiveAtmosphere(), 17, 30);
  const pes::Vec3 model{0.5672, 0.7051, -0.4256};
  const pes::Vec3 observed{0.56439, 0.70551, -0.42862};
  for (int k = 0; k < 3; ++k) {
    EXPECT_NEAR((&toSun.x)[k], (&model.x)[k], 0.005) << "model, axis " << k;
    EXPECT_NEAR((&toSun.x)[k], (&observed.x)[k], 0.005) << "observed, axis " << k;
  }
  EXPECT_NEAR(std::sqrt(toSun.x * toSun.x + toSun.y * toSun.y + toSun.z * toSun.z), 1.0, 1e-6);
  EXPECT_GT(toSun.y, 0.7);  // 44.9° elevation, mid-afternoon
}

TEST(LightingSun, NightSunFadesOut) {
  EXPECT_DOUBLE_EQ(pes::SunHorizonFade({0, -1, 0}), 1.0);  // to-sun overhead
  EXPECT_DOUBLE_EQ(pes::SunHorizonFade({0, 1, 0}), 0.0);   // to-sun nadir
  EXPECT_GT(pes::SunHorizonFade({-0.5672, -0.7051, 0.4256}), 0.99);  // the live point
}

TEST(LightingMerge, SpanReplacesOnlyOverriddenGroups) {
  pes::LightSettings base, span;
  base.sunLux = 150000;
  base.globalFogDensity = 0.03;
  span.overrideAtmosphere = true;
  span.sunLux = 70000;
  const pes::LightSettings m = pes::MergeLightSettings(base, span);
  EXPECT_DOUBLE_EQ(m.sunLux, 70000);
  EXPECT_DOUBLE_EQ(m.globalFogDensity, 0.03);  // no override flag: base kept
  EXPECT_FALSE(m.overrideGlobalFog);
}

// A minimal .atsh: header {1, 0x20, size}, type-name pad, hash-keyed params
// at 0x70 ({u64 hash, u32 0x1c, u32 value}: numDiv 40, numWeathers 2), 40
// times at 0xB0, then 2x40 entries of sh[9][3] (54 B) + light[3] (6 B).
// Entry [0][0]: sh = (1, 2, 4), light = (131008 sat, 1.0, 0.5).
std::vector<uint8_t> MakeAtsh() {
  std::vector<uint8_t> blob(0x150 + 2 * 40 * 60, 0);
  auto w32 = [&](size_t o, uint32_t v) { std::memcpy(blob.data() + o, &v, 4); };
  auto w16 = [&](size_t o, uint16_t v) { std::memcpy(blob.data() + o, &v, 2); };
  auto w64 = [&](size_t o, uint64_t v) { std::memcpy(blob.data() + o, &v, 8); };
  w32(0, 1);
  w32(4, 0x20);
  w32(8, uint32_t(blob.size()));
  w64(0x70, 0x0a4a00100000ull);  // low 48 bits of the numDiv hash
  w32(0x70 + 8, 0x1c);
  w32(0x70 + 12, 40);
  w64(0x80, 0x0a9b00000000ull);  // low 48 bits of the numWeathers hash
  w32(0x80 + 8, 0x1c);
  w32(0x80 + 12, 2);
  for (int k = 0; k < 40; ++k) w32(0xb0 + 4 * k, 2160 * k);
  w16(0x150 + 0, 0x3c00);  // sh[0][0] = 1.0
  w16(0x150 + 2, 0x4000);  // sh[0][0] = 2.0
  w16(0x150 + 4, 0x4400);  // sh[0][0] = 4.0
  w16(0x150 + 54, 0x7fff);  // light[0] red = 0x7FFF (exponent-31 saturation)
  w16(0x150 + 56, 0x3c00);  // light[0] green = 0x3C00 = 1.0
  w16(0x150 + 58, 0x3800);  // light[0] blue = 0x3800 = 0.5
  return blob;
}

TEST(LightingAtsh, NonIeeeHalfDecode) {
  std::string err;
  auto blob = MakeAtsh();
  auto t = pes::ParseAtsh(blob, &err);
  ASSERT_TRUE(t) << err;
  EXPECT_EQ(t->times[0], 0u);
  EXPECT_EQ(t->times[39], 84240u);
  EXPECT_FLOAT_EQ(t->sh[0][0][0].x, 1.0f);
  EXPECT_FLOAT_EQ(t->sh[0][0][0].y, 2.0f);
  EXPECT_FLOAT_EQ(t->sh[0][0][0].z, 4.0f);
  EXPECT_FLOAT_EQ(t->light[0][0].x, 131008.0f);  // exponent-31 saturation
  EXPECT_FLOAT_EQ(t->light[0][0].y, 1.0f);
  EXPECT_FLOAT_EQ(t->light[0][0].z, 0.5f);
  const pes::Vec3 sun = pes::AtshSunColor(*t, false, 0, 0);
  EXPECT_FLOAT_EQ(sun.x, 131008.0f);  // nearest-time entry
}

const pes::PesVfs* Vfs() {
  static pes::PesVfs vfs;
  static bool ready = [] {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) return false;
    std::string err;
    return vfs.Mount(root, &err);
  }();
  return ready ? &vfs : nullptr;
}

// The install's own st002 rig: the mechanism (§2.3 slots, §2.4 merge, §4.2
// proxy enable lists) against the file's own content.
TEST(LightingRig, St002SpansAndGroups) {
  if (!Vfs()) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto rig = pes::LoadLightRig(*Vfs(), 2, false, &err);
  ASSERT_TRUE(rig) << err;
  ASSERT_EQ(rig->spans.size(), 9u);
  EXPECT_EQ(rig->SpanForTime(15, 0).name, "TimeSpan1500_day");
  EXPECT_EQ(rig->SpanForTime(15, 30).name, "TimeSpan1500_day");
  EXPECT_EQ(rig->SpanForTime(11, 0).name, "TimeSpan1200");  // before first: clamp
  const pes::TimeSpanSettings& day = rig->SpanForDayNight(false, 0);
  EXPECT_EQ(day.name, "TimeSpan1500_day");
  const pes::TimeSpanSettings& night = rig->SpanForDayNight(true, 1);
  EXPECT_EQ(night.name, "TimeSpan1900_night_w");
  EXPECT_EQ(night.settings.gameInitialExposureValue, -9);
  EXPECT_DOUBLE_EQ(day.settings.sunLux, 100000);  // span overrideAtmosphere
  // Proxy enable lists (report §4.2): the day spans name the ReflectionProbe
  // subtree; the late spans add the interior-light subtree.
  ASSERT_EQ(day.groups.size(), 1u);
  EXPECT_EQ(day.groups[0], "ReflectionProbe");
  EXPECT_GT(night.groups.size(), 1u);
  EXPECT_EQ(rig->SpanForTime(12, 0).groups, day.groups);
  // The merged global fog applies where spans leave the override off.
  EXPECT_TRUE(day.settings.globalFogEnabled);
}

TEST(LightingRig, St002LightsAndProbes) {
  if (!Vfs()) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto rig = pes::LoadLightRig(*Vfs(), 2, false, &err);
  ASSERT_TRUE(rig) << err;
  EXPECT_EQ(rig->lights.size(), 147u);
  EXPECT_EQ(rig->probes.size(), 9u);
  EXPECT_EQ(rig->globalLights.size(), 10u);
  const pes::RigLight* pitch = nullptr;
  for (const auto& l : rig->lights)
    if (l.name == "SL_F_Pitch0002") pitch = &l;
  ASSERT_NE(pitch, nullptr);
  EXPECT_TRUE(pitch->spot);
  EXPECT_DOUBLE_EQ(pitch->lumen, 8000000);
  EXPECT_DOUBLE_EQ(pitch->outerRange, 180);
  EXPECT_DOUBLE_EQ(pitch->umbraAngle, 110);
  EXPECT_FALSE(pitch->castShadow);
  EXPECT_TRUE(pitch->hasSpecular);
  ASSERT_FALSE(pitch->groups.empty());
  EXPECT_EQ(pitch->groups.front(), "Light_Pich");
  const pes::RigProbe* rp = nullptr;
  for (const auto& p : rig->probes)
    if (p.name == "RP_F_Stnd0000") rp = &p;
  ASSERT_NE(rp, nullptr);
  EXPECT_GT(rp->box.x, 200);  // 250x55x70
  EXPECT_DOUBLE_EQ(rp->capture.x, rp->origin.x);
}

// The .atsh beside the st002 af rig: noon direct light ≈ sunLux ×
// transmittance (report Part B §9), moon at night = moonColor.
TEST(LightingRig, St002AtshSunAndMoon) {
  if (!Vfs()) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto atsh = pes::LoadAtsh(*Vfs(), 2, false, &err);
  ASSERT_TRUE(atsh) << err;
  const pes::Vec3 noon = pes::AtshSunColor(*atsh, false, 12, 0);
  EXPECT_GT(noon.x, 80000);  // daytime sun, red saturated at 131008
  EXPECT_LT(noon.x, 200000);
  const pes::Vec3 midnight = pes::AtshSunColor(*atsh, false, 0, 0);
  EXPECT_NEAR(midnight.x, 0.996, 0.01);  // moonColor
}

// The st002 rig's plugin group (docs/re/pes-re-lighting.md Part C §2): the adaptation speed is
// per rig.
TEST(LightingRig, St002PluginSettings) {
  if (!Vfs()) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  auto rig = pes::LoadLightRig(*Vfs(), 2, false, &err);
  ASSERT_TRUE(rig) << err;
  // The rig's one PesPluginSettings: this install's af file reads this value
  // (the report's survey row for st002 differs; the file is authoritative).
  EXPECT_NEAR(rig->plugin.tonemapSpeed, 10, 1e-6);
}

}  // namespace
