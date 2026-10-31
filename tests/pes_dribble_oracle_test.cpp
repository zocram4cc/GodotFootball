// PES's carrier-block constructor 0x14210D320 and turn-reference builder 0x14211D710 (translated) on GF's own window.
//
// The builder (BP+0x32C0 = the carrier manager, called every unit step by 0x142110D30 before the dribble-proposal writer) ticks the
// manager's step objects and writes BP+0x3380, the angle the carrier walk's Clear gate (0x14210F4E8) measures the dribble proposal
// against. Each captured entry state (forward-emulated from a real danger_builder capture to the builder's entry,
// tools/dev/translate/synth_walk.py --entry 0x14211D710) is rebuilt in a pes::PesWorld at PES's offsets; every byte PES's own code
// wrote into the manager object in the harness run (corpus.jsonl's write log) must be what GF's window run leaves.
//
// GF_PES_DRIBBLE_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/danger_oracle.py (builder mode: bp = r8 - 0x1C). Without it the oracle test skips; the constructor test always
// runs.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_oracle_world.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes_oracle;

uint64_t Word(const uint8_t* bytes, std::size_t offset) {
  uint64_t v;
  std::memcpy(&v, bytes + offset, sizeof v);
  return v;
}
constexpr std::size_t kManagerBytes = 0x3678 - pes::kBpManagerOffset;  // the manager's block ends where the unit-kind objects start

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

class TurnReferenceOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_DRIBBLE_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_DRIBBLE_ORACLE unset (PES-derived captures, not committed)";
    if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's tables are bound from its exe)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
};

// Every manager byte PES wrote equals the window's, the turn reference BP+0x3380 included, and GF's production run (no capture bytes
// outside the window) leaves the same turn reference.
TEST_F(TurnReferenceOracle, EveryCapturedEntryLeavesPesManagerBytesAndTurnReference) {
  int turnWritten = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    const uint64_t bp = o.regs.at("r8") - pes::kBpUnitInfoOffset;
    const std::map<uint64_t, uint8_t> want = ReferenceWrites(o, bp + pes::kBpManagerOffset, bp + pes::kBpManagerOffset + kManagerBytes);
    if (want.empty()) continue;  // PES's step wrote nothing on this state (the manager's step is a no-op out of play): nothing to compare
    Built b;
    Build(o, b, true, false, false);
    b.world->RunTurnReference(b.side, b.ext);
    const uint8_t* gf = b.world->Carrier(b.side) + pes::kBpManagerOffset;
    for (const auto& [addr, byte] : want) EXPECT_EQ(gf[addr - (bp + pes::kBpManagerOffset)], byte) << dir << ": BP+0x" << std::hex << (addr - bp);
    const uint64_t turn = bp + pes::kBpTurnReferenceOffset;
    if (want.count(turn)) ++turnWritten;
    Built production;
    Build(o, production, false, false, false);
    production.world->RunTurnReference(production.side);
    const uint8_t* prod = production.world->Carrier(production.side);
    for (std::size_t i = 0; i < sizeof(float); ++i)
      if (want.count(turn + i)) EXPECT_EQ(prod[pes::kBpTurnReferenceOffset + i], want.at(turn + i)) << dir << ": production run, turn byte " << i;
  }
  EXPECT_GT(turnWritten, 0) << "no captured step wrote BP+0x3380: the oracle set would prove nothing about it";
}

// The carrier manager's kind-6 step scores each candidate receiver 0x10..0x15 (0x14211CBD0: base term plus bonuses from the
// receiver's ability bytes 0x27, 0x2C and 0x31 of his slot record) and inserts (id, score) into its sorted list (0x14063E9D0). The
// scores are the 8-byte (id, f32) stores the insert makes; the PES run's are in the oracle's write log, the window run's in `ext`'s.
using Insert = std::pair<uint32_t, uint32_t>;  // candidate id, score bits
constexpr uint32_t kFirstCandidateId = 0x10, kLastCandidateId = 0x15;
constexpr float kScoreCeiling = 16.0f;  // the scores are sums of a few terms of at most 1.x each

std::set<Insert> Inserts(const std::vector<std::pair<uint64_t, uint8_t>>& log) {
  std::set<Insert> out;
  for (std::size_t i = 0; i + 8 <= log.size(); ++i) {
    bool run = true;
    for (std::size_t k = 1; k < 8; ++k) run = run && log[i + k].first == log[i].first + k;
    if (!run) continue;
    uint32_t id = 0, bits = 0;
    for (std::size_t k = 0; k < 4; ++k) {
      id |= static_cast<uint32_t>(log[i + k].second) << (8 * k);
      bits |= static_cast<uint32_t>(log[i + 4 + k].second) << (8 * k);
    }
    const float score = std::bit_cast<float>(bits);
    if (id >= kFirstCandidateId && id <= kLastCandidateId && score > 0.0f && score < kScoreCeiling) out.insert({id, bits});
  }
  return out;
}

std::vector<std::pair<uint64_t, uint8_t>> PesLog(const Oracle& o) {
  std::vector<std::pair<uint64_t, uint8_t>> log;
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18)
    log.push_back({std::strtoull(w.substr(at, 16).c_str(), nullptr, 16), static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16))});
  return log;
}

// GF's player data stands in as the capture's own ability bytes (the roster byte of a flat team is the live byte at cells 1.0): every
// slot's record is zeroed at the three ids, then written the way production writes it (StoreLiveAbility, all 22 slots), and the
// scores the window run inserts must be PES's. The old production wrote only the carrier's height byte: other slots read zero.
TEST_F(TurnReferenceOracle, CandidateScoresEqualPesWhenEverySlotsAbilitiesAreWrittenFromPlayerData) {
  constexpr uint32_t kIds[] = {pes::kAbilitySpeedId, pes::kAbilityJumpId, pes::kAbilityHeightId};
  int compared = 0, carrierOnlyWrong = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    const std::set<Insert> want = Inserts(PesLog(o));
    if (want.empty()) continue;  // the manager's kind-6 step did not run on this state
    const uint64_t g = Read(o, kImageG, 8);
    for (bool allSlots : {true, false}) {
      Built b;
      Build(o, b, true, false, false);
      uint8_t carrierSlot = 0;
      std::memcpy(&carrierSlot, b.world->Carrier(b.side) + pes::kBpUnitInfoOffset + pes::kUnitSlotOffset, 1);
      for (int slot = 0; slot < pes::kSlotCount; ++slot)
        for (uint32_t id : kIds) b.world->Block(slot)[pes::kSlotRecordOffset + pes::kSlotAbilityOffset + id] = 0;
      for (int slot = 0; slot < pes::kSlotCount; ++slot) {
        if (!allSlots && slot != carrierSlot) continue;
        const uint64_t record = CapturedBase(o, g, "slot", slot, false);
        for (uint32_t id : kIds)
          if (allSlots || id == pes::kAbilityHeightId)
            b.world->StoreLiveAbility(slot, id, static_cast<uint8_t>(Read(o, record + pes::kSlotAbilityOffset + id, 1)));
      }
      std::vector<std::pair<uint64_t, uint8_t>> log;
      b.ext.writeLog = &log;
      b.world->RunTurnReference(b.side, b.ext);
      const std::set<Insert> got = Inserts(log);
      if (allSlots) {
        EXPECT_EQ(got, want) << dir << ": candidate scores with every slot written";
        ++compared;
      } else if (got != want) {
        ++carrierOnlyWrong;
      }
    }
  }
  EXPECT_GT(compared, 0) << "no captured state ran the kind-6 candidate scores";
  EXPECT_GT(carrierOnlyWrong, 0) << "the carrier-only writer matched PES on every state: the oracle would prove nothing about the other slots";
}

// The store step of the stat rebuild 0x14092AF10 against whole-process captures (GF_PES_ABILITY_ORACLE: one `*.tsv` per capture, made
// by reading PES's own memory: per row `slot id base a1 a2 a3 a4 a5 live`, the roster row's byte, the player object's five cells for
// the id (P+0xC798, +0xC888, +0xC978, +0xCA68, +0xCB58) and the byte PES left in the slot record's ability block, 22 slots x every
// ability id but the four the breath penalty touches, five captures of three launches). StoreLiveAbility on the window must leave
// the live byte PES left.
TEST(AbilityStore, LeavesTheLiveByteOfEveryCapturedSlotAndId) {
  const char* dir = std::getenv("GF_PES_ABILITY_ORACLE");
  if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_ABILITY_ORACLE unset (PES-derived captures, not committed)";
  int rows = 0, scaled = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".tsv") continue;
    std::ifstream in(entry.path());
    int slot, id, base, live;
    pes::AbilityCells cells;
    while (in >> slot >> id >> base >> cells.a1 >> cells.a2 >> cells.a3 >> cells.a4 >> cells.a5 >> live) {
      pes::PesWorld w;
      w.StoreLiveAbility(slot, static_cast<uint32_t>(id), static_cast<uint8_t>(base), cells);
      EXPECT_EQ(w.Block(slot)[pes::kSlotRecordOffset + pes::kSlotAbilityOffset + id], live)
          << entry.path().filename() << " slot " << slot << " id 0x" << std::hex << id << " base " << std::dec << base;
      ++rows;
      if (live != base) ++scaled;
    }
  }
  EXPECT_GT(rows, 0) << "no *.tsv under " << dir;
  EXPECT_GT(scaled, 0) << "no captured byte differs from its base: the oracle would prove nothing about the multipliers";
}

// The clamp bounds are the install's table 0x142613C60 (two bytes per stat id: lower, upper): a live byte past either bound is the
// bound, and the byte a multiplier of one leaves is the base. The bounds are read from the exe here, never typed.
TEST(AbilityStore, ClampsToTheInstallsBoundsOfTheId) {
  constexpr uint64_t kClampTableVa = 0x142613C60;
  constexpr float kBoost = 10.0f, kCut = 0.01f;
  const char* root = std::getenv("PES21_ROOT");
  if (root == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the bounds are read from its exe)";
  std::string error;
  auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
  ASSERT_TRUE(exe) << error;
  for (uint32_t id : {pes::kAbilitySpeedId, pes::kAbilityJumpId, pes::kAbilityHeightId}) {
    uint8_t bounds[2];
    ASSERT_TRUE(exe->ReadVa(kClampTableVa + 2 * id, sizeof bounds, bounds));
    const auto live = [&](uint8_t base, float cell) {
      pes::PesWorld w;
      pes::AbilityCells cells;
      cells.a2 = cell;
      w.StoreLiveAbility(0, id, base, cells);
      return w.Block(0)[pes::kSlotRecordOffset + pes::kSlotAbilityOffset + id];
    };
    const uint8_t mid = static_cast<uint8_t>((bounds[0] + bounds[1]) / 2);
    EXPECT_EQ(live(mid, kBoost), bounds[1]) << "id 0x" << std::hex << id << " above the upper bound";
    EXPECT_EQ(live(mid, kCut), bounds[0]) << "id 0x" << std::hex << id << " below the lower bound";
    EXPECT_EQ(live(mid, 1.0f), mid) << "id 0x" << std::hex << id << " unscaled";
  }
}

// PES's post-writer bytes of the same unit step (walkref.tsv per oracle directory: BP-relative offset, length, hex; made by running
// PES's own code from the builder capture on to the carrier walk's entry, tools/dev/translate/synth_walk.py): the turn reference
// BP+0x3380, the previous-kind word BP+0x41E4, the adopted copy BP+0x41E8 and the dribble-proposal record BP+0x4204 itself.
// GF's chain RunDribbleProposal (0x14211D710, 0x14211ED10, 0x1421102D0, the copies of 0x142110D30) from the entry state must leave
// them, on the window with the capture's out-of-window bytes and on the production window without.
struct RefField {
  std::size_t offset = 0;
  std::vector<uint8_t> bytes;
};

std::vector<RefField> WalkRef(const std::string& dir) {
  std::vector<RefField> fields;
  std::ifstream in(std::filesystem::path(dir) / "walkref.tsv");
  std::string off, len, hex;
  while (in >> off >> len >> hex) {
    RefField f;
    f.offset = std::stoul(off, nullptr, 16);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) f.bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    EXPECT_EQ(f.bytes.size(), std::stoul(len)) << dir;
    fields.push_back(std::move(f));
  }
  return fields;
}

TEST_F(TurnReferenceOracle, TheProposalChainLeavesPesTurnReferenceAndProposalRecord) {
  int compared = 0, proposalTypes = 0;
  for (const std::string& dir : dirs_) {
    const std::vector<RefField> ref = WalkRef(dir);
    if (ref.empty()) continue;  // PES's own code did not reach the walk from this capture (no walkref.tsv)
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    for (bool captureBytes : {true, false}) {
      Built b;
      Build(o, b, captureBytes, false, false, true);
      // The chain reads far more than the builder's own call did (the kind objects, BP+0x4258, the slot records): all of it from the capture.
      CopyRecords(o, b, false);
      const uint8_t* block = b.world->Carrier(b.side);
      b.world->RunDribbleProposal(b.side, pes::LoadU32(block, pes::kBpUnitInfoOffset + pes::kUnitSlotOffset), b.ext);
      for (const RefField& f : ref)
        for (std::size_t i = 0; i < f.bytes.size(); ++i)
          EXPECT_EQ(block[f.offset + i], f.bytes[i]) << dir << (captureBytes ? " (capture bytes)" : " (production window)") << ": BP+0x"
                                                     << std::hex << f.offset + i;
    }
    ++compared;
    // The record's type word differs between the states: a chain that writes nothing would pass a state whose record is all zero.
    if (pes::LoadU32(ref.back().bytes.data(), 0) != 0) ++proposalTypes;
  }
  EXPECT_GT(compared, 0) << "no walkref.tsv under " << std::getenv("GF_PES_DRIBBLE_ORACLE");
  EXPECT_GT(proposalTypes, 0) << "every compared proposal record was all zero: the oracle would prove nothing about the writer";
}

// The null slot of the constructor on a zero window (the old abort `PES_DISPATCH_UNTRANSLATED: 0`): the block reset 0x14210E5F0,
// the constructor's last call, calls slot +8 of every kind object (0x1421105B0), and the kind-0x18 object (BP+0x3DD0) takes the
// vtable 0x142BF0F70 (0x1421349E0), which the bound vtable span did not cover: its slot read 0. The span now ends past it.
// PES's constructor leaves the block's objects with their vtables: the carrier manager's, the kind objects' (the first, the dribble
// kind and the last) first words point into the image, are equal for both sides, and the dribble kind (BP+0x33A0) and the carrier
// manager do not share one.
TEST(CarrierConstruct, EveryObjectHoldsAnImageVtableOnBothSides) {
  if (std::getenv("PES21_ROOT") == nullptr) GTEST_SKIP() << "PES21_ROOT unset (the install's vtables are bound from its exe)";
  pes::PesWorld w;
  BindWorld(w);
  w.BindImageTables(InstallTables());  // the constructor's own virtual calls read the vtables the block's objects were just given
  w.RunCarrierConstruct(0);
  w.RunCarrierConstruct(1);
  constexpr std::size_t kDribbleKindOffset = 0x33A0;  // 0x1421105B0(BP, 0x10)
  constexpr std::size_t kLastKindOffset = 0x3DD0;     // 0x1421105B0(BP, 0x18)
  for (std::size_t off : {std::size_t{0}, pes::kBpManagerOffset, kDribbleKindOffset, kLastKindOffset}) {
    const uint64_t a = Word(w.Carrier(0), off), c = Word(w.Carrier(1), off);
    EXPECT_EQ(a, c) << "BP+0x" << std::hex << off;
    EXPECT_GE(a, pes::kImageBaseVa) << "BP+0x" << std::hex << off;
    EXPECT_LT(a, pes::kWorldWindowVa) << "BP+0x" << std::hex << off << ": an image address, not a window one";
  }
  EXPECT_NE(Word(w.Carrier(0), pes::kBpManagerOffset), Word(w.Carrier(0), kDribbleKindOffset));
}

}  // namespace
