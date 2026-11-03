// GF's port of PES's FixDemo camera record list (pes::BuildCameraList, FixDemoScene::LayerSetWord, CameraListIndex): the builder
// inside the cut start 0x141F2DD80, the set word writer 0x141F5D2E0 and the scene generators of 0x141F55110.
//
// Relation tests over records of the shape the install's layers have, plus the PES oracle: cases whose inputs are read from a live
// capture's memory (docs/ports/pes-port-CameraSelect.md section 3) and whose outputs are what PES's own bytes (0x141F2DD80
// emulated by the diff harness over that memory) left: the built list and the generator's state after it.
//
// Needs the install (PES21_ROOT/PES2021.exe) for the immediates of the draws; skips without it.

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/fixdemo_camera_list.hpp"

namespace {

using pes::fdc::CameraRecord;

CameraRecord Rec(uint32_t key, float start) {
  CameraRecord r;
  r.key = key;
  r.startTime = start;
  return r;
}

class CameraListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* root = std::getenv("PES21_ROOT");
    if (!root) GTEST_SKIP() << "PES21_ROOT is not set";
    std::string err;
    exe_ = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(exe_) << err;
    auto k = pes::LoadFixDemoRandomConstants(*exe_, &err);
    ASSERT_TRUE(k) << err;
    k_ = *k;
  }
  std::optional<pes::PeImage> exe_;
  pes::FixDemoRandomConstants k_{};
};

TEST_F(CameraListTest, SetCountStopsAtTheFirstSetWithoutATensZeroRecord) {
  EXPECT_EQ(pes::CameraSetCount({Rec(0, 0), Rec(100, 0), Rec(300, 0)}), 2u);  // no set 2: the count stops there
  EXPECT_EQ(pes::CameraSetCount({Rec(10, 0), Rec(11, 0)}), 0u);              // tens group 1 only
  EXPECT_EQ(pes::CameraSetCount({Rec(0, 0), Rec(200, 0)}), 1u);
  EXPECT_EQ(pes::CameraSetCount({}), 0u);
}

TEST_F(CameraListTest, TheListTakesOneRecordPerTensGroupOfTheChosenSet) {
  const std::vector<CameraRecord> records = {Rec(0, 0),   Rec(10, 300), Rec(11, 300), Rec(12, 400),
                                             Rec(100, 0), Rec(110, 250), Rec(120, 500)};
  for (uint32_t seed = 1; seed < 40; ++seed) {
    pes::FixDemoGenerator r2(k_, seed);
    for (uint32_t word = 0; word < 6; ++word) {
      const std::vector<size_t> list = pes::BuildCameraList(records, word, r2);
      const uint32_t set = word % 2;
      ASSERT_FALSE(list.empty());
      uint32_t lastTens = 0;
      for (size_t k = 0; k < list.size(); ++k) {
        const CameraRecord& r = records[list[k]];
        EXPECT_EQ(r.key / 100, set) << "word " << word;
        const uint32_t tens = (r.key % 100) / 10;
        if (k > 0) EXPECT_GT(tens, lastTens);  // tens groups in order, one record each
        lastTens = tens;
        if (k >= 2) EXPECT_GT(r.startTime, records[list[k - 1]].startTime);  // later entries must start later
      }
      EXPECT_EQ(records[list[0]].key, set * 100u);  // tens group 0 holds one record: always taken first
    }
  }
}

TEST_F(CameraListTest, OneDrawPerNonEmptyTensGroupAndNoneWithoutASet) {
  const std::vector<CameraRecord> records = {Rec(0, 0), Rec(10, 300), Rec(11, 300), Rec(30, 900)};  // groups 0, 1, 3
  pes::FixDemoGenerator used(k_, 12345), fresh(k_, 12345);
  pes::BuildCameraList(records, 0, used);
  fresh.Pick(1);
  fresh.Pick(2);
  fresh.Pick(1);
  EXPECT_EQ(used.state(), fresh.state());
  EXPECT_EQ(used.word(), fresh.word());
  pes::FixDemoGenerator idle(k_, 12345);
  EXPECT_TRUE(pes::BuildCameraList({Rec(10, 0)}, 0, idle).empty());  // no tens-0 record: no set, no draw
  EXPECT_EQ(idle.state(), pes::FixDemoGenerator(k_, 12345).state());
}

TEST_F(CameraListTest, TheIndexWalksTheListAsTheClockReachesEachStart) {
  const std::vector<CameraRecord> records = {Rec(0, 0), Rec(10, 300), Rec(20, 480)};
  const std::vector<size_t> list = {0, 1, 2};
  EXPECT_EQ(pes::CameraListIndex(records, list, 0), 0u);
  EXPECT_EQ(pes::CameraListIndex(records, list, 299.9f), 0u);
  EXPECT_EQ(pes::CameraListIndex(records, list, 300), 1u);
  EXPECT_EQ(pes::CameraListIndex(records, list, 1000), 2u);
}

TEST_F(CameraListTest, ASetWordModeOneIsTheScenesAndModeTwoAvoidsThePreviousPick) {
  pes::FixDemoScene scene(k_, 777);
  const uint32_t first = scene.LayerSetWord(1, 3);
  EXPECT_EQ(scene.LayerSetWord(1, 3), first);  // the scene's word is shared by every layer that asks for it
  uint32_t previous = 0;
  bool have = false;
  for (int i = 0; i < 200; ++i) {
    const uint32_t word = scene.LayerSetWord(2, 3);
    EXPECT_LT(word, 3u);
    if (have) EXPECT_NE(word, previous);
    previous = word;
    have = true;
  }
}

}  // namespace
