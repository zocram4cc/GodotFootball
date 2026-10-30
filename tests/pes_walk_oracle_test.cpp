// PES's carrier walk 0x14210F110 (translated; GF's forced clear and every other carrier arbitration is its return) on GF's own
// window. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets (pes_oracle_world.hpp: BP with its priority
// list, ctx, MA, E, the match-info word MI+0x116C, the man, anime and pitch records the call reads); the kind GF's window walk
// returns must equal what PES's own bytes returned in the harness run of the same capture (corpus.jsonl's rax), the run without
// the capture's out-of-window bytes (GF's production run) included.
//
// GF_PES_WALK_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/danger_oracle.py ... walk. A directory named syn_* is a SYNTHETIC state (tools/dev/translate/synth_walk.py
// --patch-clear: a real danger-builder capture run forward to the walk's entry with the kind-1 record's type set to 5, the Clear
// unit's output; everything else is PES's state); the others are real: the walk's own captures, and the states
// tools/dev/translate/synth_walk.py (no --patch-clear) runs forward from a danger-predicated carrier_clear_2109EB0 capture to the
// walk's entry, whose kind-1 record is PES's own type 5. Without the variable the oracle tests skip.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "pes/translated/translated.hpp"
#include "onthepitch/pescarrier.hpp"
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
  std::set<uint32_t> realKinds, syntheticKinds, clearRecordKinds;
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
    if (!Synthetic(dir) && LoadU32(KindRecord(*b.world, b.side, kClearKind), 0) == kClearType) clearRecordKinds.insert(want);
    // GF's production run holds no capture bytes outside the window: the kind must not depend on them.
    Built production;
    Build(o, production, false, true);
    EXPECT_EQ(production.world->RunWalk(production.side), want) << dir << ": production run";
  }
  EXPECT_TRUE(realKinds.count(pes::kWalkNoKind)) << "the real set holds a call PES answers 'no kind' for";
  EXPECT_TRUE(realKinds.count(3)) << "the real set holds a call PES answers a chain kind (3) for";
  EXPECT_TRUE(syntheticKinds.count(kClearKind)) << "the synthetic set reaches the Clear arm and PES keeps the clear";
  EXPECT_TRUE(clearRecordKinds.count(kClearKind)) << "the real set holds a call whose own Clear record PES keeps";
  EXPECT_GT(clearRecordKinds.size(), 1u) << "the real set holds a call whose own Clear record PES does not keep";
}

// The Clear arm keys on the kind-1 record's type word (5): where PES keeps the clear (synthetic or real), a window whose record
// answers nothing must not come back with the clear.
TEST_F(WalkOracle, TheClearArmKeysOnTheClearRecordsType) {
  int syntheticChecked = 0, realChecked = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b, true, true);
    if (LoadU32(KindRecord(*b.world, b.side, kClearKind), 0) != kClearType || PesReturn(o) != kClearKind) continue;
    StoreU32(KindRecord(*b.world, b.side, kClearKind), 0, 0);
    EXPECT_NE(b.world->RunWalk(b.side, b.ext), kClearKind) << dir;
    ++(Synthetic(dir) ? syntheticChecked : realChecked);
  }
  EXPECT_GT(syntheticChecked, 0) << "no synthetic Clear-arm state in the oracle set";
  EXPECT_GT(realChecked, 0) << "no real Clear-arm state in the oracle set";
}

// The list GF writes is PES's list: read each capture's list from the capture's own memory (count, kinds, the record at each
// kind's slot; the window holds only the bytes the walk read, and the Clear arm's chain reads a kind's record without reading its
// position, as seq15049's kind 6), rewrite it with SetUnitList over a zeroed list region, and the walk must still return PES's
// kind. A wrong rank byte or a record at the wrong slot changes the walk's order or its answers.
TEST_F(WalkOracle, TheListGfWritesReproducesPesKindOnEveryCapture) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    Build(o, b, true, true);
    uint8_t* list = b.world->Carrier(b.side) + pes::kBpListOffset;
    const uint64_t pesList = o.regs.at("rcx") + pes::kBpListOffset;
    std::vector<uint8_t> captured(pes::kListBytes);
    for (std::size_t at = 0; at < captured.size(); ++at) captured[at] = static_cast<uint8_t>(Read(o, pesList + at, 1));
    std::vector<pes::PesWorld::WalkEntry> entries;
    for (uint32_t position = 0, count = LoadU32(captured.data(), 0); position < count; ++position) {
      pes::PesWorld::WalkEntry e;
      e.kind = LoadU32(captured.data(), sizeof(uint32_t) * (1 + position));
      std::copy_n(captured.begin() + pes::kListRecordOffset + pes::kListRecordStride * e.kind, e.record.size(), e.record.begin());
      entries.push_back(e);
    }
    ASSERT_FALSE(entries.empty()) << dir;
    b.world->SetUnitList(b.side, entries);
    // PES's own list bytes: the count, the kinds, every single-listed kind's rank byte and each listed kind's record.
    for (std::size_t at = 0; at < sizeof(uint32_t) * (1 + entries.size()); ++at) EXPECT_EQ(list[at], captured[at]) << dir << " list+" << at;
    // A rank byte is the kind's position in the list. PES's list can name a kind twice (seq5878 lists kind 0 at two positions); GF's
    // builder never does, so only single kinds count.
    for (std::size_t position = 0; position < entries.size(); ++position) {
      const uint32_t kind = entries[position].kind;
      if (std::count_if(entries.begin(), entries.end(), [kind](const auto& e) { return e.kind == kind; }) != 1) continue;
      const std::size_t at = pes::kListRankOffset + kind;
      EXPECT_EQ(captured[at], position) << dir << " captured rank of kind " << kind;
      EXPECT_EQ(list[at], position) << dir << " rank of kind " << kind;
    }
    for (const auto& e : entries)
      for (std::size_t at = 0; at < pes::kListRecordStride; ++at) {
        const std::size_t off = pes::kListRecordOffset + pes::kListRecordStride * e.kind + at;
        EXPECT_EQ(list[off], captured[off]) << dir << " record of kind " << e.kind << "+" << at;
      }
    EXPECT_EQ(b.world->RunWalk(b.side, b.ext), PesReturn(o)) << dir;
  }
}

// A list whose records all answer nothing: the walk skips every kind and returns PES's 'no kind' word.
TEST(CarrierWalk, ListedKindsThatAnswerNothingReturnNoKind) {
  pes::PesWorld w;
  BindWorld(w);
  w.SetUnitList(0, {{0, {}}, {1, {}}, {4, {}}});
  EXPECT_EQ(w.RunWalk(0), pes::kWalkNoKind);
}

// The unit updater's writers on GF's window. GF_PES_UNIT_ORACLE names a directory of per-capture directories (tools:
// statedirs.py: mem.bin, maps.txt, the entry registers) of COMPLETE (unslimmed) carrier-walk captures: the BP the walk saw holds
// what 0x14063C9F0 (unit info BP+0x1C) and 0x14063A490 (state BP+0x78) wrote earlier in the same tick. The window is rebuilt from
// the capture's whole records, the two writers run on it, and every byte of BP+0x1C..BP+0xEC must equal the capture's, except the
// state's two counters (the capture is after the update, the run is a second one: +1, and the owned-tick count restarts at 0
// while the carrier is not the owner), plus BP+0x41E4 (the previous kind the driver stores before the walk).
class UnitOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_UNIT_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_UNIT_ORACLE unset (PES-derived captures, not committed)";
    if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's tables are bound from its exe)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
};

TEST_F(UnitOracle, UnitInfoAndStateFromTheWindowEqualPesOnEveryCapture) {
  constexpr std::size_t kTickCounter = pes::kBpStateOffset + 2 * sizeof(uint32_t);   // state word 2: ticks since the unit began
  constexpr std::size_t kOwnedCounter = pes::kBpStateOffset + 3 * sizeof(uint32_t);  // state word 3: ticks owned
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    BuildFull(o, b);
    b.world->AdoptCarrierState(b.side);
    uint8_t* bp = b.world->Carrier(b.side);
    const std::vector<uint8_t> want(bp, bp + pes::kBpImageAOffset);
    const uint32_t wantKind = LoadU32(bp, pes::kBpChosenKindOffset);
    const uint32_t ticks = LoadU32(bp, kTickCounter), owned = LoadU32(bp, kOwnedCounter);
    const bool carrierOwns = bp[pes::kBpStateOffset + pes::kStateOwnedOffset] != 0;
    b.world->RunCarrierUpdate(b.side, LoadU32(bp, pes::kBpUnitInfoOffset + pes::kUnitSlotOffset), b.ext);
    EXPECT_EQ(LoadU32(bp, kTickCounter), ticks + 1) << dir;
    EXPECT_EQ(LoadU32(bp, kOwnedCounter), carrierOwns ? owned + 1 : 0u) << dir;
    EXPECT_EQ(LoadU32(bp, pes::kBpChosenKindOffset), wantKind) << dir << ": the previous kind the walk read";
    for (std::size_t i = pes::kBpUnitInfoOffset; i < want.size(); ++i) {
      if ((i >= kTickCounter && i < kTickCounter + 4) || (i >= kOwnedCounter && i < kOwnedCounter + 4)) continue;
      EXPECT_EQ(bp[i], want[i]) << dir << " BP+0x" << std::hex << i;
    }
  }
}

// The danger byte BP+0x32AD (the walk's guard) is the builder's imageA+0x31C1, and the zone record BP+0xAB8 is written by the same
// builder run: both zeroed in the window, the builder run on the capture's records must give the capture's bytes back.
TEST_F(UnitOracle, DangerBuilderOnTheWindowWritesTheWalksDangerByteAndZone) {
  constexpr std::size_t kZoneBytes = 0x180;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Built b;
    BuildFull(o, b);
    b.world->AdoptCarrierState(b.side);
    uint8_t* bp = b.world->Carrier(b.side);
    const std::vector<uint8_t> wantZone(bp + pes::kBpZoneOffset, bp + pes::kBpZoneOffset + kZoneBytes);
    const uint8_t wantDanger = bp[pes::kBpGuardOffset];
    std::memset(bp + pes::kBpZoneOffset, 0, kZoneBytes);
    bp[pes::kBpGuardOffset] = 0;
    b.world->RunDangerBuilder(b.side, b.ext);
    EXPECT_EQ(bp[pes::kBpGuardOffset], wantDanger) << dir << " danger byte";
    for (std::size_t i = 0; i < kZoneBytes; ++i)
      EXPECT_EQ(bp[pes::kBpZoneOffset + i], wantZone[i]) << dir << " zone+0x" << std::hex << i;
  }
}

// What GF's production profile writer (SetCarrierProfile, called by the carrier controller before every walk) puts in the window,
// read back through PES's own readers: the skill bit 0x14063E960(ctx, slot, bit) and the height stat byte 0x14063D8A0(ctx, slot,
// 0x31). Both go through the slot -> info map (0x140A3AF90) of G's team-info table, so a record written under the wrong index, a
// wrong side record or a wrong map entry changes what PES reads for some slot.
uint64_t PesRead(pes::PesWorld& w, void (*reader)(PesState&), uint32_t slot, uint32_t arg) {
  PesState s;
  w.BindCall(s);
  s.gpr[2] = slot;
  s.gpr[8] = arg;
  reader(s);
  return s.gpr[0];
}

TEST(CarrierProfile, PesReadersSeeTheCarriersMaskAndHeightAndNobodyElses) {
  constexpr uint32_t kHeightStatIndex = 0x31;  // the walk's `MOV R8D,0x31` for 0x14063D8A0 (height cm)
  constexpr int kSkills[] = {pes::kSkillOneTouchShot, pes::kSkillOneTouchPass, pes::kSkillPinpointCross};
  constexpr uint32_t kPlace = 7;
  const uint64_t masks[2] = {(1ull << pes::kSkillOneTouchShot) | (1ull << pes::kSkillPinpointCross), 1ull << pes::kSkillOneTouchPass};
  const uint8_t heights[2] = {171, 193};
  pes::PesWorld w;
  BindWorld(w);
  for (int side = 0; side < 2; ++side) w.SetCarrierProfile(side, side * pes::kSlotsPerSide + kPlace, masks[side], heights[side]);
  for (uint32_t slot = 0; slot < 2 * pes::kSlotsPerSide; ++slot) {
    const int side = static_cast<int>(slot / pes::kSlotsPerSide);
    const bool carrier = slot % pes::kSlotsPerSide == kPlace;
    for (int skill : kSkills)
      EXPECT_EQ(PesRead(w, &pes_fn_14063E960, slot, skill) & 1, carrier && ((masks[side] >> skill) & 1) ? 1u : 0u)
          << "slot " << slot << " skill " << skill;
    EXPECT_EQ(PesRead(w, &pes_fn_14063D8A0, slot, kHeightStatIndex) & 0xFF, carrier ? heights[side] : 0u)
        << "slot " << slot;
  }
}

// 0x14210F110 in a stopped play state 3: a carrier who is not the side record's designated taker (+0x7C of G's first side table,
// 0x140A48A30; `CMP EDX,[RAX+0x7C]` 0x1421101C7) is handed the first listed kind; the taker himself skips a dribble answer (record
// type 3) and, with nothing else listed, gets no kind. SetCarrierProfile names the carrier the taker (a ponytail: the restart
// set-up writer is undecoded), so the carrier's own walk skips; the unit object's slot is what the walk reads as the carrier.
TEST(CarrierProfile, TheWalkReadsTheTakerFromTheSideRecordAndTheCarrierFromTheUnitObject) {
  constexpr uint32_t kPlayStateStopped = 3;
  constexpr uint32_t kListedKind = 3;
  if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's tables are bound from its exe)";
  for (int side = 0; side < 2; ++side) {
    pes::PesWorld w;
    BindWorld(w);
    w.BindImageTables(InstallTables());
    w.Period().playState = kPlayStateStopped;
    const uint32_t slot = side * pes::kSlotsPerSide + 4;
    StoreU32(w.Carrier(side), pes::kBpUnitInfoOffset + pes::kUnitSideOffset, side);
    w.SetCarrierProfile(side, slot, 0, 180);
    w.SetUnitList(side, {{kListedKind, pes::EncodeAnswerRecord({pes::kAnswerDribble, 0, 0.0f})}});
    StoreU32(w.Carrier(side), pes::kBpUnitInfoOffset + pes::kUnitSlotOffset, slot);
    EXPECT_EQ(w.RunWalk(side), pes::kWalkNoKind) << "the taker himself, side " << side;
    StoreU32(w.Carrier(side), pes::kBpUnitInfoOffset + pes::kUnitSlotOffset, slot + 1);
    EXPECT_EQ(w.RunWalk(side), kListedKind) << "another carrier, side " << side;
  }
}

}  // namespace
