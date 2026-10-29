// PES's carrier walk 0x14210F110 (translated; GF's forced clear and every other carrier arbitration is its return) on GF's own
// window. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets (pes_oracle_world.hpp: BP with its priority
// list, ctx, MA, E, the match-info word MI+0x116C, the man, anime and pitch records the call reads); the kind GF's window walk
// returns must equal what PES's own bytes returned in the harness run of the same capture (corpus.jsonl's rax), the run without
// the capture's out-of-window bytes (GF's production run) included.
//
// GF_PES_WALK_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/danger_oracle.py ... walk. A directory named syn_* is a SYNTHETIC state (tools/dev/translate/synth_walk.py
// --patch-clear: a real danger-builder capture run forward to the walk's entry with the kind-1 record's type set to 5, the Clear
// unit's output; everything else is PES's state); the others are real calls. Without the variable the oracle tests skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_oracle_world.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes_oracle;
using pes::LoadU32;
using pes::StoreU32;

constexpr uint32_t kClearKind = 1;
constexpr uint32_t kClearType = 5;

bool Synthetic(const std::string& dir) { return std::filesystem::path(dir).filename().string().rfind("syn_", 0) == 0; }

uint32_t PesReturn(const Oracle& o) { return static_cast<uint32_t>(std::strtoull(Field(o.row, "rax").c_str(), nullptr, 16)); }

uint8_t* KindRecord(pes::PesWorld& w, int side, uint32_t kind) {
  return w.Carrier(side) + pes::kBpListOffset + pes::kListRecordOffset + pes::kListRecordStride * kind;
}

class WalkOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_WALK_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_WALK_ORACLE unset (PES-derived captures, not committed)";
    if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's tables are bound from its exe)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
};

TEST_F(WalkOracle, EveryCapturedWalkCallOnGfsWindowReturnsPesKind) {
  std::set<uint32_t> realKinds, syntheticKinds;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b, true, true);
    const uint32_t want = PesReturn(o);
    const uint32_t got = b.world->RunWalk(b.side, b.ext);
    EXPECT_EQ(got, want) << dir << (Synthetic(dir) ? " (synthetic)" : " (real)");
    (Synthetic(dir) ? syntheticKinds : realKinds).insert(want);
    // GF's production run holds no capture bytes outside the window: the kind must not depend on them.
    Built production;
    Build(o, production, false, true);
    EXPECT_EQ(production.world->RunWalk(production.side), want) << dir << ": production run";
  }
  EXPECT_TRUE(realKinds.count(pes::kWalkNoKind)) << "the real set holds a call PES answers 'no kind' for";
  EXPECT_TRUE(realKinds.count(3)) << "the real set holds a call PES answers a chain kind (3) for";
  EXPECT_TRUE(syntheticKinds.count(kClearKind)) << "the synthetic set reaches the Clear arm and PES keeps the clear";
}

// The Clear arm is live on the synthetic states: the kind-1 record's type word (5) is what the walk keys on, so a window whose
// record answers nothing must not come back with the clear.
TEST_F(WalkOracle, TheSyntheticClearArmKeysOnTheClearRecordsType) {
  int checked = 0;
  for (const std::string& dir : dirs_) {
    if (!Synthetic(dir)) continue;
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b, true, true);
    ASSERT_EQ(LoadU32(KindRecord(*b.world, b.side, kClearKind), 0), kClearType) << dir << ": the patched type";
    StoreU32(KindRecord(*b.world, b.side, kClearKind), 0, 0);
    EXPECT_NE(b.world->RunWalk(b.side, b.ext), kClearKind) << dir;
    ++checked;
  }
  EXPECT_GT(checked, 0) << "no synthetic state in the oracle set";
}

// A list whose records all answer nothing: the walk skips every kind and returns PES's 'no kind' word.
TEST(CarrierWalk, ListedKindsThatAnswerNothingReturnNoKind) {
  pes::PesWorld w;
  BindWorld(w);
  w.SetUnitList(0, {{0, {}}, {1, {}}, {4, {}}});
  EXPECT_EQ(w.RunWalk(0), pes::kWalkNoKind);
}

}  // namespace
