// PES's danger builder 0x142119390 (the only writer of imageA+0x31C1, the danger flag every carrier unit tests: the pressure
// builder 0x14069A500 and the writer 0x140699960 run from its tail), translated, on GF's own window. Each capture's inputs
// are rebuilt in a pes::PesWorld at PES's offsets: the carrier block BP (unit info, state, imageA's live bytes), ctx, MA, E, and
// the ball, match-info, pitch, side, slot, man and anime records the call reads. The call allocates through PES's allocator on
// GF's native heap (pes_native.hpp). The danger byte, the pressure record and the lists the builder leaves must equal what PES's
// own bytes left in the harness run of the same capture (corpus.jsonl's write log).
//
// GF_PES_DANGER_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/danger_oracle.py. Without it the oracle tests skip; the zero-world tests always run.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_oracle_world.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes_oracle;

using pes::LoadU32;
using pes::StoreU32;


// The bytes PES's own code wrote (corpus.jsonl's write log, last write per address) in [lo, hi).
std::map<uint64_t, uint8_t> ReferenceWrites(const Oracle& o, uint64_t lo, uint64_t hi) {
  std::map<uint64_t, uint8_t> out;
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr >= lo && addr < hi) out[addr] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return out;
}

class DangerOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_DANGER_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_DANGER_ORACLE unset (PES-derived captures, not committed)";
    if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's tables are bound from its exe)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
};

TEST_F(DangerOracle, EveryCapturedCallOnGfsWindowLeavesPesDangerFlag) {
  int dangerOne = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b);
    const uint64_t imageA = o.regs.at("rcx");
    const bool danger = b.world->RunDangerBuilder(b.side, b.ext);
    const std::map<uint64_t, uint8_t> ref = ReferenceWrites(o, imageA + pes::kImageADangerOffset, imageA + pes::kImageADangerOffset + 1);
    ASSERT_EQ(ref.size(), 1u) << dir << ": PES wrote the danger byte once";
    EXPECT_EQ(danger, ref.begin()->second != 0) << dir;
    dangerOne += ref.begin()->second != 0 ? 1 : 0;
    // GF's production run holds no capture bytes outside the window: the flag must not depend on them.
    Built production;
    Build(o, production, false);
    EXPECT_EQ(production.world->RunDangerBuilder(production.side), danger) << dir << ": production run";
  }
  EXPECT_GT(dangerOne, 0) << "the oracle set must hold a call PES answers danger 1 for (seq6161), or the test proves only 0";
}

// The builder's every write into imageA below the danger byte (the list kinds at +0xB6C, the corridor tables, the pressure record's
// lane lists at +0x2124..0x31C0) equals what PES's own bytes wrote on the same capture. MatchInfo6: before the install's tables were
// bound (pesimagetables.hpp) the lane lists differed on seq4046 and seq4891 (467 bytes: man 2's anime class gate read the table's
// zeros) and every capture's list kinds differed (the frame-rate cell read 0, so the predictions did not move).
TEST_F(DangerOracle, EveryCapturedCallLeavesPesImageARecords) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b);
    const uint64_t imageA = o.regs.at("rcx");
    b.world->RunDangerBuilder(b.side, b.ext);
    const uint8_t* gf = b.world->Carrier(b.side) + pes::kBpImageAOffset;
    const auto writes = ReferenceWrites(o, imageA, imageA + pes::kImageADangerOffset);
    EXPECT_GT(writes.size(), 0x2000u) << dir << ": the reference run wrote the lists";
    for (const auto& [addr, want] : writes)
      EXPECT_EQ(gf[addr - imageA], want) << dir << ": imageA+0x" << std::hex << (addr - imageA);
  }
}

// The writer's first test (0x140699960: the side record's word at +0x6D4 == 6 answers 1 at once) is a spec fact; flipping the
// window's word changes the flag on every captured call whose own answer was 0, so the binding is live.
TEST_F(DangerOracle, SideModeSixAnswersDangerOnEveryCapturedCall) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b);
    StoreU32(b.world->SideRecord(b.side), pes::kSideModeOffset, 6);
    EXPECT_TRUE(b.world->RunDangerBuilder(b.side, b.ext)) << dir;
  }
}

// A world that holds nothing but the tables: the builder runs to the end on GF memory, allocates and frees through PES's
// allocator (no abort), and the pressure builder left its marker: *P = slot % 11 (0x14069A500: the slot's index in its team).
TEST(DangerBuilder, ZeroWorldRunsAndPressureRecordNamesTheCarrier) {
  pes::PesWorld w;
  BindWorld(w);
  const uint32_t slot = 14;
  StoreU32(w.Carrier(1), pes::kBpUnitInfoOffset + pes::kUnitSlotOffset, slot);
  StoreU32(w.Carrier(1), pes::kBpUnitInfoOffset + pes::kUnitSideOffset, 1);
  StoreU32(w.Carrier(1), pes::kBpFlagsOffset, 0x1F);
  EXPECT_FALSE(w.RunDangerBuilder(1));
  EXPECT_EQ(LoadU32(w.Carrier(1), pes::kBpImageAOffset + pes::kImageAPressureOffset), slot % 11);
}

// The unit info the builder reads, written by the translated unit updater 0x14063C9F0 from the window's records: slot, slot % 11,
// side, the side's heading and sign, the man record's position and that position times the sign (a side that attacks -x holds
// -1); the state's owned byte follows the ball owner in MA.
TEST(DangerBuilder, CarrierInputsAreTheWindowsRecordsTimesTheAttackSign) {
  pes::PesWorld w;
  BindWorld(w);
  const uint32_t slot = 17;
  w.SetAttackSign(1, -1);
  w.SetAttackAngle(1, 270.0f);
  pes::StoreF32(w.ManRecord(static_cast<int>(slot)), pes::kManPosXOffset, -12.0f);
  pes::StoreF32(w.ManRecord(static_cast<int>(slot)), pes::kManPosZOffset, 3.0f);
  w.SetMaOwner(1, slot);
  w.RunCarrierUpdate(1, slot);
  const uint8_t* bp = w.Carrier(1);
  const uint8_t* unit = bp + pes::kBpUnitInfoOffset;
  EXPECT_EQ(LoadU32(unit, pes::kUnitSlotOffset), slot);
  EXPECT_EQ(LoadU32(unit, pes::kUnitSlotInTeamOffset), 6u);
  EXPECT_EQ(LoadU32(unit, pes::kUnitSideOffset), 1u);
  EXPECT_EQ(pes::LoadF32(unit, pes::kUnitHeadingOffset), 270.0f);
  EXPECT_EQ(pes::LoadF32(unit, pes::kUnitSignOffset), -1.0f);
  EXPECT_EQ(pes::LoadF32(unit, pes::kUnitPositionOffset), -12.0f);
  EXPECT_EQ(pes::LoadF32(unit, pes::kUnitSignedPositionOffset), 12.0f);
  EXPECT_EQ(pes::LoadF32(unit, pes::kUnitSignedPositionOffset + 8), -3.0f);
  EXPECT_EQ(bp[pes::kBpStateOffset + pes::kStateOwnedOffset], 1);
  EXPECT_EQ(LoadU32(bp, pes::kBpFlagsOffset), pes::kBpRefreshAll);
  w.SetMaOwner(1, pes::kMiNoSlot);
  w.RunCarrierUpdate(1, slot);
  EXPECT_EQ(bp[pes::kBpStateOffset + pes::kStateOwnedOffset], 0) << "a carrier that is not the ball's owner is not owned";
}

}  // namespace
