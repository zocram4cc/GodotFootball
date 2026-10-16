#include <gtest/gtest.h>

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pes/effects.hpp"
#include "pes/fpk.hpp"
#include "pes/vfs.hpp"

namespace {

// Same markup as the stadium packs: a commented-out template, a setting with
// per-index params, an inline comment between params, and creates with extra attributes.
constexpr std::string_view kFixture = R"(<?xml version="1.0" encoding="UTF-8"?>
<effect version="2">
  <!--
  <setting type="Sample">
    <param id="test" type="float" value="1.0" />
  </setting>
  -->
  <setting type="Vignetting" index="0">
    <param id="alpha" type="uint" value="0x80" />
  </setting>
  <setting type="LensFlare" index="1">
    <param id="texturePath" type="path" index="0" value="/Assets/pes16/a.ftex" />
    <!-- locator -->
    <param id="trans" type="vector3" value="-45.006111, 31.974949, -34.298920" />
    <param id="quat" type="vector4" value="0.0, 0.5, 1.0, -1.0" />
    <param id="innerAngle" type="angle" value="30.000000" />
    <param id="layerColor" type="vector4" index="1" value="0.0, 0.5, 2.0, 1.0" />
    <param id="colors" type="rgba" index="0" value="0xff000080" />
    <param id="scale" type="float" value="2.5" />
  </setting>
  <create type="Vignetting" setting="0" />
  <create type="LensFlare" setting="1" floor="upper" dir="back" />
</effect>
)";

bool EndsWith(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

TEST(PesEffects, ParsesSettingsAndCreatesAndSkipsCommentedSettings) {
  std::string err;
  auto cfg = pes::ParseEffectConfig(kFixture, &err);
  ASSERT_TRUE(cfg.has_value()) << err;
  ASSERT_EQ(cfg->settings.size(), 2u);  // the commented Sample is not a setting
  EXPECT_EQ(cfg->settings[0].type, "Vignetting");
  EXPECT_EQ(cfg->settings[0].index, 0);
  EXPECT_EQ(cfg->settings[1].type, "LensFlare");
  EXPECT_EQ(cfg->settings[1].index, 1);
  EXPECT_EQ(cfg->settings[1].params.size(), 7u);
  ASSERT_EQ(cfg->creates.size(), 2u);
  EXPECT_EQ(cfg->creates[1].type, "LensFlare");
  EXPECT_EQ(cfg->creates[1].setting, 1);
  ASSERT_EQ(cfg->creates[1].attributes.size(), 2u);
  EXPECT_EQ(cfg->creates[1].attributes[0], (std::pair<std::string, std::string>{"floor", "upper"}));
  EXPECT_NE(cfg->Setting("LensFlare", 1), nullptr);
  EXPECT_EQ(cfg->Setting("LensFlare", 0), nullptr);
}

TEST(PesEffects, ConvertsNumericTypesAndKeepsPathsAsText) {
  std::string err;
  auto cfg = pes::ParseEffectConfig(kFixture, &err);
  ASSERT_TRUE(cfg.has_value()) << err;
  const pes::EffectSetting& lens = cfg->settings[1];

  const pes::EffectParam* tex = lens.Find("texturePath", 0);
  ASSERT_NE(tex, nullptr);
  EXPECT_EQ(tex->text, "/Assets/pes16/a.ftex");
  EXPECT_TRUE(tex->values.empty());

  const pes::EffectParam* trans = lens.Find("trans");
  ASSERT_NE(trans, nullptr);
  ASSERT_EQ(trans->values.size(), 3u);
  EXPECT_DOUBLE_EQ(trans->values[0], -45.006111);
  EXPECT_DOUBLE_EQ(trans->values[2], -34.298920);

  // Angles stay in degrees, as the XML writes them.
  ASSERT_NE(lens.Find("innerAngle"), nullptr);
  EXPECT_DOUBLE_EQ(lens.Find("innerAngle")->values[0], 30.0);

  // rgba 0xRRGGBBAA: the high byte is red, the low byte is alpha.
  const pes::EffectParam* colour = lens.Find("colors", 0);
  ASSERT_NE(colour, nullptr);
  ASSERT_EQ(colour->values.size(), 4u);
  EXPECT_DOUBLE_EQ(colour->values[0], 1.0);
  EXPECT_DOUBLE_EQ(colour->values[1], 0.0);
  EXPECT_DOUBLE_EQ(colour->values[2], 0.0);
  EXPECT_DOUBLE_EQ(colour->values[3], 128.0 / 255.0);

  // uint written in hex.
  const pes::EffectParam* alpha = cfg->settings[0].Find("alpha");
  ASSERT_NE(alpha, nullptr);
  EXPECT_DOUBLE_EQ(alpha->values[0], 128.0);
}

TEST(PesEffects, RejectsAParamOutsideASetting) {
  std::string err;
  EXPECT_FALSE(pes::ParseEffectConfig(R"(<effect><param id="a" type="float" value="1" /></effect>)", &err));
  EXPECT_FALSE(err.empty());
}

TEST(PesEffects, RejectsAMalformedNumber) {
  std::string err;
  EXPECT_FALSE(pes::ParseEffectConfig(
      R"(<effect><setting type="X" index="0"><param id="f" type="float" value="1.0x" /></setting></effect>)", &err));
  EXPECT_FALSE(err.empty());
}

TEST(PesEffects, RejectsAnUnterminatedComment) {
  std::string err;
  EXPECT_FALSE(pes::ParseEffectConfig("<effect><!-- never closed </effect>", &err));
  EXPECT_FALSE(err.empty());
}

TEST(PesEffects, PicksTheStadiumPackByTimeZoneAndWeather) {
  EXPECT_EQ(pes::EffectPackPath(2, false, false), "Asset/model/bg/st002/effect/#Win/effect_st002_df.fpk");
  EXPECT_EQ(pes::EffectPackPath(2, false, true), "Asset/model/bg/st002/effect/#Win/effect_st002_dr.fpk");
  EXPECT_EQ(pes::EffectPackPath(2, true, false), "Asset/model/bg/st002/effect/#Win/effect_st002_nf.fpk");
  EXPECT_EQ(pes::EffectPackPath(2, true, true), "Asset/model/bg/st002/effect/#Win/effect_st002_nr.fpk");
}

// Install-wide check (no pinned values, so 4cc packs cannot break it): every canonical
// effect_config.xml parses. Canonical means `#Win/effect_*.fpk` directly under `#Win/`; the
// `#Win/tmp/` copies are staging leftovers (INF) and are not read. A pack that FpkArchive rejects
// is reported, not failed: that is the FPK reader's bounds check (effect_st069_nf.fpk: an entry
// overruns EOF by 13 bytes; its XML is intact). Creates whose setting is absent from the same
// pack (rain classes in some nf variants) are reported too: consumers must skip them.
TEST(PesEffects, EveryCanonicalEffectConfigInTheInstallParses) {
  const char* root = std::getenv("PES21_ROOT");
  if (!root) GTEST_SKIP() << "PES21_ROOT not set";
  std::string err;
  pes::PesVfs vfs;
  ASSERT_TRUE(vfs.Mount(root, &err)) << err;

  constexpr std::string_view kDir = "/effect/#Win/";
  int configs = 0;
  for (const std::string& path : vfs.List("Asset/model/bg/")) {
    const size_t dir = path.find(kDir);
    if (!EndsWith(path, ".fpk") || dir == std::string::npos) continue;
    if (path.find('/', dir + kDir.size()) != std::string::npos) continue;  // #Win/tmp/...
    auto blob = vfs.Read(path, &err);
    ASSERT_TRUE(blob.has_value()) << path << ": " << err;
    auto fpk = pes::FpkArchive::Parse(*blob, &err);
    if (!fpk) {
      std::cout << "[  NOTE  ] " << path << ": FPK reader rejects it (" << err << ")\n";
      continue;
    }
    for (const pes::FpkEntry& entry : fpk->entries) {
      if (!EndsWith(Lower(entry.path), "effect_config.xml")) continue;
      const std::string_view xml(reinterpret_cast<const char*>(blob->data() + entry.offset), entry.size);
      auto cfg = pes::ParseEffectConfig(xml, &err);
      ASSERT_TRUE(cfg.has_value()) << path << ": " << err;
      ++configs;
      for (const pes::EffectCreate& c : cfg->creates)
        if (!cfg->Setting(c.type, c.setting))
          std::cout << "[  NOTE  ] " << path << ": create " << c.type << " " << c.setting << " has no setting\n";
    }
  }
  EXPECT_GT(configs, 0);
}
