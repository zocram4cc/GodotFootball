#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pes/fpk.hpp"
#include "pes/stadium.hpp"
#include "pes/vfs.hpp"

namespace {

// A 392-byte Stadium.bin record: u16 id at +4. Synthetic ids (2 and 5); no PES
// bytes anywhere in this test.
std::vector<uint8_t> MakeStadiumBinBlob() {
  auto rec = [&](uint16_t id) {
    std::vector<uint8_t> r(392, 0);
    // u16 id at +4, little-endian.
    r[4] = static_cast<uint8_t>(id & 0xFF);
    r[5] = static_cast<uint8_t>((id >> 8) & 0xFF);
    return r;
  };
  std::vector<uint8_t> blob;
  for (auto& r : {rec(2), rec(5)})
    blob.insert(blob.end(), r.begin(), r.end());
  return blob;
}

// The one installed PES VFS, mounted once for the whole test binary.
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

bool Is180Y(const pes::Quat& q) {
  return std::fabs(std::fabs(q.y) - 1.0f) < 1e-4f && std::fabs(q.x) < 1e-4f &&
         std::fabs(q.z) < 1e-4f && std::fabs(q.w) < 1e-4f;
}
}  // namespace

TEST(Stadium, ParsesStadiumRecordsSynthetically) {
  const std::vector<uint8_t> blob = MakeStadiumBinBlob();
  std::string err;
  auto list = pes::ParseStadiumRecords({blob.data(), blob.size()}, &err);
  ASSERT_EQ(list.size(), 2u) << err;
  EXPECT_EQ(list[0].id, 2);
  EXPECT_EQ(list[1].id, 5);
}

TEST(Stadium, RejectsTruncatedStadiumRecords) {
  const std::vector<uint8_t> blob = MakeStadiumBinBlob();
  std::string err;
  auto bad = pes::ParseStadiumRecords({blob.data(), blob.size() - 1}, &err);
  EXPECT_TRUE(bad.empty());
  EXPECT_FALSE(err.empty());
}

// A model's transform is exactly the fox2 TransformEntity's scale/quat/translation;
// the goal's 180-degree rotation about Y is (0,1,0,0).
TEST(Stadium, TransformHoldsFoxValues) {
  pes::Transform t;
  t.scale = {2.0f, 1.0f, 1.0f};
  t.rotation = {0.0f, 1.0f, 0.0f, 0.0f};
  t.translation = {-52.5f, 0.0f, 0.0f};
  EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
  EXPECT_FLOAT_EQ(t.rotation.y, 1.0f);
  EXPECT_FLOAT_EQ(t.rotation.w, 0.0f);
}

TEST(StadiumInstall, StadiumListNamesStadiumTwo) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto list = pes::ReadStadiumList(*vfs, &err);
  ASSERT_FALSE(list.empty()) << err;
  EXPECT_TRUE(std::any_of(list.begin(), list.end(), [](const auto& s) { return s.id == 2; }))
      << "stadium 2 missing among " << list.size() << " records";
}

TEST(StadiumInstall, AssemblesStadiumTwoWithPitchAndTwoGoals) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  auto models = pes::LoadStadiumScene(*vfs, 2, pes::kWeatherFine, &err);
  ASSERT_TRUE(models.has_value()) << err;
  const auto& ms = *models;

  // Every placed model's fmdl really is in its sibling .fpk.
  for (const auto& m : ms) {
    auto fpkBlob = vfs->Read(m.fpk, &err);
    ASSERT_TRUE(fpkBlob.has_value()) << m.fpk << ": " << err;
    auto fpk = pes::FpkArchive::Parse(*fpkBlob, &err);
    ASSERT_TRUE(fpk.has_value()) << m.fpk << ": " << err;
    const bool found = std::any_of(fpk->entries.begin(), fpk->entries.end(),
                                   [&](const auto& e) { return e.path == m.member; });
    EXPECT_TRUE(found) << "member " << m.member << " not in " << m.fpk;
  }

  // The scene: every fmdl the st002 fox2 names is placed. A pack may ship an
  // fmdl no fox2 names (vanilla st002 ships scenes/cover.fmdl unreferenced);
  // that one is not the scene's to place.
  {
    const std::string fpk = "Asset/model/bg/st002/#Win/st002.fpk";
    auto fpkBlob = vfs->Read(fpk, &err);
    ASSERT_TRUE(fpkBlob.has_value()) << err;
    auto pack = pes::FpkArchive::Parse(*fpkBlob, &err);
    ASSERT_TRUE(pack.has_value()) << err;
    auto fox2Blob = vfs->Read(fpk + "d", &err);
    ASSERT_TRUE(fox2Blob.has_value()) << err;
    const std::string fox2(fox2Blob->begin(), fox2Blob->end());
    int named = 0;
    for (const auto& e : pack->entries)
      if (e.path.size() > 5 && e.path.compare(e.path.size() - 5, 5, ".fmdl") == 0 &&
          fox2.find(e.path) != std::string::npos) {
        ++named;
        EXPECT_TRUE(std::any_of(ms.begin(), ms.end(),
                                [&](const auto& m) { return m.fpk == fpk && m.member == e.path; }))
            << "the fox2 names " << e.path << " but it was not placed";
      }
    EXPECT_GT(named, 0);
  }

  // The pitch is present.
  EXPECT_TRUE(std::any_of(ms.begin(), ms.end(), [](const auto& m) {
    return m.fpk == "Asset/model/bg/st002/pitch/#Win/pitch_st002.fpk";
  })) << "pitch missing";

  // Two goals: the common goal_post places goal.fmdl twice, one of them 180 deg
  // about Y (the mesh is authored at -X and mirrored for +X), plus two shadows.
  int goals = 0, shadow = 0;
  bool goal180 = false;
  for (const auto& m : ms)
    if (m.fpk == "Asset/model/bg/common/goal/#Win/goal_post.fpk") {
      if (m.member == "Assets/pes16/model/bg/common/goal/scenes/goal.fmdl") {
        ++goals;
        if (Is180Y(m.transform.rotation)) goal180 = true;
      } else if (m.member == "Assets/pes16/model/bg/common/goal/scenes/shadow.fmdl")
        ++shadow;
    }
  EXPECT_EQ(goals, 2) << "expected two goal posts";
  EXPECT_EQ(shadow, 2) << "expected two goal shadows";
  EXPECT_TRUE(goal180) << "one goal is authored at -X and mirrored 180 deg about Y";
}

// Weather-only layers. Stadium 29's scene pack places a puddle decal (member
// st029_center1_rain_detail, one `rain` mesh) and a snow layer (center1_snow, one
// snow mesh) in every match. PES draws each only in its own weather, and in fine
// weather the puddle sits 3 cm over the pitch and hides it.
TEST(StadiumInstall, WeatherOnlyLayersAreKeptOnlyInTheirWeather) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  auto placed = [](const std::vector<pes::PlacedModel>& ms, const std::string& tail) {
    return std::any_of(ms.begin(), ms.end(), [&](const auto& m) {
      return m.member.size() >= tail.size() &&
             m.member.compare(m.member.size() - tail.size(), tail.size(), tail) == 0;
    });
  };
  const std::string rainDecal = "st029_center1_rain_detail.fmdl";
  const std::string snowLayer = "center1_snow.fmdl";
  std::string err;

  auto fine = pes::LoadStadiumScene(*vfs, 29, pes::kWeatherFine, &err);
  ASSERT_TRUE(fine.has_value()) << err;
  EXPECT_FALSE(placed(*fine, rainDecal)) << "puddle decal placed in fine weather";
  EXPECT_FALSE(placed(*fine, snowLayer)) << "snow layer placed in fine weather";

  auto rain = pes::LoadStadiumScene(*vfs, 29, pes::kWeatherRain, &err);
  ASSERT_TRUE(rain.has_value()) << err;
  EXPECT_TRUE(placed(*rain, rainDecal)) << "puddle decal missing in rain";
  EXPECT_FALSE(placed(*rain, snowLayer)) << "snow layer placed in rain";

  auto snow = pes::LoadStadiumScene(*vfs, 29, pes::kWeatherSnow, &err);
  ASSERT_TRUE(snow.has_value()) << err;
  EXPECT_TRUE(placed(*snow, snowLayer)) << "snow layer missing in snow";
  EXPECT_FALSE(placed(*snow, rainDecal)) << "puddle decal placed in snow";
}

// Team.bin: one 1532-byte record per team, u32 team id at +8, u16 home stadium
// at +0x3C. Synthetic values here; the install test reads the install's file.
TEST(Stadium, ParsesTeamRecordIdAndHomeStadium) {
  std::vector<uint8_t> rec(1532, 0);
  rec[8] = 0xC4;  // team 708 (0x2C4), little-endian
  rec[9] = 0x02;
  rec[0x3C] = 54;
  std::string err;
  auto teams = pes::ParseTeamRecords({rec.data(), rec.size()}, &err);
  ASSERT_EQ(teams.size(), 1u) << err;
  EXPECT_EQ(teams[0].id, 708);
  EXPECT_EQ(teams[0].stadiumId, 54);
}

// PES's match resolver: the stored stadium when it is valid, else 29 (Konami
// Stadium). A stored 0 is the same fallback (pes-re-stadium-cloth-vfx.md 1.2).
TEST(Stadium, ZeroStadiumResolvesToTwentyNine) {
  EXPECT_EQ(pes::ResolveStadiumId(0, {0, 2, 29}), 29);
}

TEST(Stadium, StadiumWithoutPackResolvesToTwentyNine) {
  EXPECT_EQ(pes::ResolveStadiumId(54, {2, 29}), 29);
}

TEST(Stadium, ValidStoredStadiumKeepsItsId) {
  EXPECT_EQ(pes::ResolveStadiumId(2, {2, 29}), 2);
}

// BNT (708) and W (760) store stadium 29 in the 4cc Team.bin override, so they
// resolve to 29 in this install. That pins a pack value on the owner's request;
// the test skips when the install lacks either team.
TEST(StadiumInstall, BntAndWResolveToTwentyNine) {
  const pes::PesVfs* vfs = Vfs();
  if (!vfs) GTEST_SKIP() << "PES21_ROOT not set or mount failed";
  std::string err;
  const auto teams = pes::ReadTeamList(*vfs, &err);
  ASSERT_FALSE(teams.empty()) << err;
  for (int id : {708, 760}) {
    if (!std::any_of(teams.begin(), teams.end(), [&](const auto& t) { return t.id == id; }))
      GTEST_SKIP() << "team " << id << " not in this install";
    EXPECT_EQ(pes::TeamStadiumId(*vfs, id), 29) << "team " << id;
  }
}
