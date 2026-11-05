// PES's team pass-target selector 0x1404FB2A0 (the team tick's step 40c; it reaches 0x1404FB380, 0x1404FB780 and the holder tree 0x1404F0E50)
// on GF's own window. Each captured call's live-in bytes (tools/dev/translate/teampass_oracle.py: every byte the call read that it had
// not written) are rebuilt in a pes::PesWorld at PES's offsets: the brain Y, the ctx, MA, the event store E, the roster, the skip object,
// the ball, match-info, pitch, team, anime, man and slot records, and both side records. The pass target GF's window run leaves in the team
// record (T+0x78) and the selector's answer byte must equal what PES's own bytes left (corpus.jsonl's write log and rax).
//
// GF_PES_TEAMPASS_ORACLE names a directory of per-capture directories (PES-derived, never committed). Without it the test skips.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_oracle_world.hpp"

namespace {

using namespace pes_oracle;
using pes::LoadU32;

using pes::StoreU32;
constexpr uint64_t kBrainRegister = 0;  // the captured selector's rcx is the brain Y

// The window's record for a manifest kind and index, or null (G is the window's own; the entries of G's side table that name the side
// the brain's own record stands for are the brain's copy T instead).
uint8_t* RecordOf(pes::PesWorld& w, int own, const std::string& kind, int idx) {
  if (kind == "Y") return w.Brain(own);
  if (kind == "ctx") return w.Ctx(own);
  if (kind == "ma") return w.Ma(own);
  if (kind == "e") return reinterpret_cast<uint8_t*>(&w.EventImage());
  if (kind == "skip") return w.Skip();
  if (kind == "roster") return w.Roster();
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "sideE") return idx == own ? nullptr : w.SideRecord(idx);
  if (kind == "team") return w.TeamInfoRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  return nullptr;
}

uint64_t CapturedBase(const Oracle& o, uint64_t g, int own, const std::string& kind, int idx) {
  const uint64_t y = o.regs.at("rcx");
  const uint64_t ctx = Read(o, y + pes::kBrainCtxOffset, 8);
  const auto entry = [&](size_t table) { return Read(o, g + table + static_cast<size_t>(idx) * pes::kTableEntryBytes, 8); };
  if (kind == "Y") return y;
  if (kind == "ctx") return ctx;
  if (kind == "ma") return Read(o, ctx + pes::kCtxMaOffset, 8);
  if (kind == "e") return Read(o, ctx + pes::kCtxEventsOffset, 8);
  if (kind == "skip") return Read(o, g + pes::kSkipPtrOffset, 8);
  if (kind == "roster") return Read(o, g + pes::kRosterPtrOffset, 8);
  if (kind == "B") return Read(o, g + pes::kBallPtrOffset, 8);
  if (kind == "MI") return Read(o, g + pes::kMatchInfoPtrOffset, 8);
  if (kind == "fr") return Read(o, g + pes::kFrameRandomPtrOffset, 8);
  if (kind == "pitch") return Read(o, g + pes::kPitchPtrOffset, 8);
  if (kind == "side") return Read(o, y + pes::kBrainTeamOffset, 8);
  if (kind == "sideE") return entry(pes::kSideTableAOffset);
  if (kind == "team") return entry(pes::kSideTableBOffset);
  if (kind == "anime") return entry(pes::kAnimeTableOffset);
  if (kind == "man") return entry(pes::kManTableOffset);
  if (kind == "slot") return entry(pes::kSlotTableOffset);
  (void)own;
  return 0;
}

// The u32 PES left at `va`: the last write of its own run when it wrote all four bytes, else what the capture held.
uint32_t PesWord(const Oracle& o, uint64_t va) {
  uint32_t word = static_cast<uint32_t>(Read(o, va, 4));
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr < va || addr >= va + 4) continue;
    const unsigned shift = 8 * static_cast<unsigned>(addr - va);
    word = (word & ~(0xFFu << shift)) | (static_cast<uint32_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16)) << shift);
  }
  return word;
}

// Whether PES's own run stored to any byte of the u32 at `va`.
bool PesWrote(const Oracle& o, uint64_t va) {
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr >= va && addr < va + 4) return true;
  }
  return false;
}

class TeamPassOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_TEAMPASS_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_TEAMPASS_ORACLE unset (PES-derived captures, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
};

TEST_F(TeamPassOracle, EveryCapturedSelectorCallOnGfsWindowLeavesPesPassTarget) {
  (void)kBrainRegister;
  int changed = 0, kept = 0, cleared = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    pes::PesWorld world;
    BindWorld(world);
    const uint64_t g = Read(o, kImageG, 8);
    const uint64_t y = o.regs.at("rcx");
    const int own = static_cast<int>(Read(o, y + pes::kBrainSideOffset, 4));
    for (const auto& line : o.manifest) {
      if (line[0] != "T") continue;
      const int idx = std::atoi(line[2].c_str());
      uint8_t* dst = RecordOf(world, own, line[1], line[1] == "Y" || line[1] == "ctx" || line[1] == "ma" ? own : idx);
      if (dst == nullptr) continue;
      const uint64_t base = CapturedBase(o, g, own, line[1], idx);
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      // The window's own pointers stay: the capture's name the capture's heap.
      if (line[1] == "ctx" && (rel == pes::kCtxMaOffset || rel == pes::kCtxEventsOffset)) continue;
      if (line[1] == "Y" && rel >= pes::kBrainCtxOffset) continue;
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    }
    const uint64_t team = Read(o, y + pes::kBrainTeamOffset, 8);
    const uint32_t before = static_cast<uint32_t>(Read(o, team + pes::kSidePassTargetOffset, 4));
    const uint32_t want = PesWord(o, team + pes::kSidePassTargetOffset);
    const uint32_t phase = world.Period().playState;
    std::vector<std::pair<uint64_t, uint8_t>> log;
    const bool answer = world.RunPassTarget(own, phase, &log);
    // Where PES's own run stores T+0x78 (even the same value back), GF's window run must store it too.
    const uint64_t slotVa = pes::PesWorld::SideVa(own) + pes::kSidePassTargetOffset;
    const bool gfWrote = std::any_of(log.begin(), log.end(), [&](const auto& w) { return w.first >= slotVa && w.first < slotVa + 4; });
    EXPECT_EQ(gfWrote, PesWrote(o, team + pes::kSidePassTargetOffset)) << dir << ": the store to T+0x78";
    EXPECT_EQ(world.PassTarget(own), want) << dir << ": phase " << phase << ", before " << before;
    EXPECT_EQ(answer, (std::strtoull(Field(o.row, "rax").c_str(), nullptr, 16) & 0xFF) != 0) << dir << ": the answer byte";
    changed += want != before;
    kept += want == before && want != pes::kNoPassTarget;
    cleared += want == pes::kNoPassTarget;
  }
  RecordProperty("changed", changed);
  RecordProperty("kept", kept);
  RecordProperty("cleared", cleared);
}

}  // namespace
