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
#include "pes_state.hpp"

namespace {

using namespace pes_oracle;

constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G
using pes::LoadU32;
using pes::StoreU32;

void BindWorld(pes::PesWorld& w) {
  for (int k = 0; k < pes::kSlotCount; ++k) w.BindSlot(k);
  for (int k = 0; k < pes::kOfficialCount; ++k) w.BindOfficial(pes::kRefereeOfficialIndex + k);
  for (int t = 0; t < pes::kSideCount; ++t) {
    w.SetEntry(pes::WorldTable::kSideA, t, pes::PesWorld::SideVa(t));
    w.SetEntry(pes::WorldTable::kSideB, t, pes::PesWorld::SideVa(t));
  }
}

// The window's record for a manifest kind, or null (G: the window holds its tables; ctx and its pointers: below).
uint8_t* RecordOf(pes::PesWorld& w, int side, const std::string& kind, int idx) {
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  if (kind == "bp") return w.Carrier(side);
  if (kind == "ctx") return w.Ctx(side);
  if (kind == "ma") return w.Ma(side);
  if (kind == "e") return reinterpret_cast<uint8_t*>(&w.EventImage());
  return nullptr;
}

// The capture's record of `kind`, located through G as PES's accessors do (bp, ctx, ma and e from the entry registers).
uint64_t CapturedBase(const Oracle& o, uint64_t g, const std::string& kind, int idx) {
  const auto entry = [&](size_t table) { return Read(o, g + table + static_cast<size_t>(idx) * pes::kTableEntryBytes, 8); };
  const uint64_t ctx = o.regs.at("rdx");
  if (kind == "B") return Read(o, g + pes::kBallPtrOffset, 8);
  if (kind == "MI") return Read(o, g + pes::kMatchInfoPtrOffset, 8);
  if (kind == "pitch") return Read(o, g + pes::kPitchPtrOffset, 8);
  if (kind == "fr") return Read(o, g + pes::kFrameRandomPtrOffset, 8);
  if (kind == "side") return entry(pes::kSideTableAOffset);
  if (kind == "anime") return entry(pes::kAnimeTableOffset);
  if (kind == "man") return entry(pes::kManTableOffset);
  if (kind == "slot") return entry(pes::kSlotTableOffset);
  if (kind == "bp") return o.regs.at("r8") - pes::kBpUnitInfoOffset;
  if (kind == "ctx") return ctx;
  if (kind == "ma") return Read(o, ctx + pes::kCtxMaOffset, 8);
  if (kind == "e") return Read(o, ctx + pes::kCtxEventsOffset, 8);
  return 0;
}

struct Built {
  std::unique_ptr<pes::PesWorld> world = std::make_unique<pes::PesWorld>();
  pes::PesDangerExternals ext;
  int side = 0;
};

// `osBytes`: also hand the call the capture's bytes outside the window (the allocator's and the thread block's words the PES
// run read); GF's production run has none of them.
void Build(const Oracle& o, Built& b, bool osBytes = true) {
  pes::PesWorld& w = *b.world;
  BindWorld(w);
  const uint64_t g = Read(o, kImageG, 8);
  b.side = static_cast<int>(Read(o, o.regs.at("r8") + pes::kUnitSideOffset, 4));
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, b.side, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()));
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      // The window's own pointers (ctx -> MA, ctx -> E) stay: the capture's name the capture's heap.
      if (line[1] == "ctx" && (rel == pes::kCtxMaOffset || rel == pes::kCtxEventsOffset)) continue;
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    } else if (osBytes && (line[0] == "H" || line[0] == "I")) {
      pes::PesTrapSegment seg;
      seg.va = std::strtoull(line[1].c_str(), nullptr, 16);
      if (seg.va == kImageG) continue;  // G is the window's
      for (size_t i = 0, n = std::strtoul(line[2].c_str(), nullptr, 10); i < n; ++i)
        seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
      b.ext.segments.push_back(std::move(seg));
    }
  }
  // BP+0x18 is the caller's sixth argument (the refresh flags): the capture's stack word.
  StoreU32(w.Carrier(b.side), pes::kBpFlagsOffset,
           static_cast<uint32_t>(Read(o, o.regs.at("rsp") + pes::kDangerArg6Offset, 4)));
}

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

// OPEN (MatchInfo5 7.1): the pressure record's lane lists 0x14069A500 builds (imageA+0x2124..0x31C0) equal PES's on eight of the ten
// captures; on seq4046 and seq4891 the (slot id, f32) entries of the sorted lane lists differ (467 bytes), though the danger flag
// is equal on all ten and the same captures replay bit-exact on the snapshot's own memory (snapdiff). So an input of the lists
// the window does not bind yet (the manifest's live-in set holds no other record) is the blocker. Run with
// --gtest_also_run_disabled_tests; the forced clear (rule 7) reads these lists.
TEST_F(DangerOracle, DISABLED_EveryCapturedCallLeavesPesPressureLaneRecords) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b);
    const uint64_t imageA = o.regs.at("rcx");
    b.world->RunDangerBuilder(b.side, b.ext);
    const uint8_t* gf = b.world->Carrier(b.side) + pes::kBpImageAOffset;
    for (const auto& [addr, want] : ReferenceWrites(o, imageA + pes::kImageAPressureOffset, imageA + pes::kImageADangerOffset))
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

// The unit info the builder reads, as the ten captures show it: slot, slot % 11, side, the side's heading and sign, the man
// record's position and that position times the sign (a side that attacks -x holds -1), the state's ball copy and owned byte.
TEST(DangerBuilder, CarrierInputsAreTheWindowsRecordsTimesTheAttackSign) {
  pes::PesWorld w;
  BindWorld(w);
  const uint32_t slot = 17;
  w.SetAttackSign(1, -1);
  w.SetAttackAngle(1, 270.0f);
  pes::StoreF32(w.ManRecord(static_cast<int>(slot)), pes::kManPosXOffset, -12.0f);
  pes::StoreF32(w.ManRecord(static_cast<int>(slot)), pes::kManPosZOffset, 3.0f);
  w.SetMaBall(1, -10.0f, 0.5f, 0.25f);
  w.SetCarrierInputs(1, slot, true);
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
  const uint8_t* state = bp + pes::kBpStateOffset;
  EXPECT_EQ(pes::LoadF32(state, pes::kStateBallOffset), -10.0f);
  EXPECT_EQ(pes::LoadF32(state, pes::kStateBallOffset + 8), 0.25f);
  EXPECT_EQ(state[pes::kStateOwnedOffset], 1);
  EXPECT_EQ(LoadU32(bp, pes::kBpFlagsOffset), pes::kBpRefreshAll);
}

}  // namespace
