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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>
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
