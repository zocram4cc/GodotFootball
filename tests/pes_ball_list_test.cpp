// PES's ball-list writer FUN_14045B090 (translated, set kick; proven bit-exact on 12 real calls) run on GF's world window: the AI
// object record (pesworldrecord.hpp kAi*) names MI, the pitch, the ball and the men's records, and the writer fills MA+0x2470C
// with each team's men on the pitch by squared distance to the ball. The test sets the men and the ball in the window, runs the
// writer, and checks the list against an independent recompute of the same squared distances (float, PES's operand order).
//
// PES_EXE names the unpacked image (the writer reads its constants from it). Without it, skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_kick_oracle_common.hpp"

namespace {

constexpr int kMen = 22, kTeamMen = 11;
constexpr uint8_t kRankAbsent = 0x0A;

struct Man {
  float x, z;
};

class BallList : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* exe = std::getenv("PES_EXE");
    if (exe == nullptr || *exe == '\0') GTEST_SKIP() << "PES_EXE unset (the writer reads its constants from the image)";
    ASSERT_TRUE(image_.Open(exe));
    world_ = std::make_unique<pes::PesWorld>();
    ext_.image = image_.bytes;
    ext_.imageBytes = image_.size;
  }
  void Place(const std::array<Man, kMen>& men, float ballX, float ballZ) {
    for (int slot = 0; slot < kMen; ++slot) {
      world_->BindSlot(slot);  // the man table names the slot's record, as the match binds it
      uint8_t* man = world_->ManRecord(slot);
      pes::StoreF32(man, pes::kManPosXOffset, men[static_cast<size_t>(slot)].x);
      pes::StoreF32(man, pes::kManPosZOffset, men[static_cast<size_t>(slot)].z);
    }
    world_->SetBallSample(0, ballX, 0.0f, ballZ);
  }
  // team -> its on-pitch slots ascending by squared distance (PES's float order: z first)
  std::vector<std::pair<uint32_t, float>> Expect(const std::array<Man, kMen>& men, const std::array<bool, kMen>& on, int team,
                                                 float bx, float bz) {
    std::vector<std::pair<uint32_t, float>> out;
    for (int slot = team * kTeamMen; slot < (team + 1) * kTeamMen; ++slot) {
      if (!on[static_cast<size_t>(slot)]) continue;
      const float dz = bz - men[static_cast<size_t>(slot)].z, dx = bx - men[static_cast<size_t>(slot)].x;
      out.push_back({static_cast<uint32_t>(slot), dz * dz + dx * dx});
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    return out;
  }
  pes_kick_oracle::Image image_;
  std::unique_ptr<pes::PesWorld> world_;
  pes::PesTrapExternals ext_;
};

TEST_F(BallList, EachTeamIsSortedByDistanceToTheBallAndRanked) {
  std::array<Man, kMen> men{};
  for (int slot = 0; slot < kMen; ++slot)  // distinct distances: no tie to order
    men[static_cast<size_t>(slot)] = {static_cast<float>((slot * 37) % 23) * 3.1f - 30.0f, static_cast<float>((slot * 11) % 17) * 2.3f - 18.0f};
  const float bx = 4.5f, bz = -2.25f;
  Place(men, bx, bz);
  std::array<bool, kMen> on{};
  on.fill(true);
  world_->RunBallNearest(0, on, ext_);
  const uint8_t* ma = world_->Ma(0);
  for (int team = 0; team < 2; ++team) {
    const auto want = Expect(men, on, team, bx, bz);
    ASSERT_EQ(ma[pes::kMaBallListCountOffset + team], want.size());
    for (size_t k = 0; k < want.size(); ++k) {
      const uint8_t* entry = ma + pes::kMaBallListOffset + team * pes::kMaListTeamStride + k * 8;
      EXPECT_EQ(pes::LoadU32(entry, 0), want[k].first) << "team " << team << " place " << k;
      EXPECT_EQ(pes::LoadF32(entry, 4), want[k].second) << "team " << team << " place " << k;
      EXPECT_EQ(ma[pes::kMaBallListRankOffset + want[k].first], k) << "rank of slot " << want[k].first;
    }
  }
}

TEST_F(BallList, AManOffThePitchIsNotListedAndHoldsTheAbsentRank) {
  std::array<Man, kMen> men{};
  for (int slot = 0; slot < kMen; ++slot) men[static_cast<size_t>(slot)] = {static_cast<float>(slot) * 2.0f - 20.0f, static_cast<float>(slot % 5)};
  Place(men, 0.0f, 0.0f);
  std::array<bool, kMen> on{};
  on.fill(true);
  on[3] = false;
  on[14] = false;
  world_->RunBallNearest(1, on, ext_);
  const uint8_t* ma = world_->Ma(1);
  EXPECT_EQ(ma[pes::kMaBallListCountOffset], kTeamMen - 1);
  EXPECT_EQ(ma[pes::kMaBallListCountOffset + 1], kTeamMen - 1);
  EXPECT_EQ(ma[pes::kMaBallListRankOffset + 3], kRankAbsent);
  EXPECT_EQ(ma[pes::kMaBallListRankOffset + 14], kRankAbsent);
  for (int team = 0; team < 2; ++team)
    for (unsigned k = 0; k < ma[pes::kMaBallListCountOffset + team]; ++k) {
      const uint32_t id = pes::LoadU32(ma + pes::kMaBallListOffset + team * pes::kMaListTeamStride + k * 8, 0);
      EXPECT_NE(id, 3u);
      EXPECT_NE(id, 14u);
    }
}

}  // namespace
