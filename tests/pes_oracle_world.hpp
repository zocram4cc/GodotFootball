// A captured call's inputs rebuilt in a pes::PesWorld at PES's offsets, shared by the danger-builder and carrier-walk oracle
// tests: the carrier block BP (unit info, state, imageA's live bytes, the walk's list), ctx, MA, E, and the ball, match-info,
// pitch, side, slot, man and anime records the call reads (the manifest's T lines, tools/dev/translate/danger_oracle.py).
// Header-only, one include per test binary.
#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_state.hpp"

namespace pes_oracle {

constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G

inline void BindWorld(pes::PesWorld& w) {
  for (int k = 0; k < pes::kSlotCount; ++k) w.BindSlot(k);
  for (int k = 0; k < pes::kOfficialCount; ++k) w.BindOfficial(pes::kRefereeOfficialIndex + k);
  for (int t = 0; t < pes::kSideCount; ++t) {
    w.SetEntry(pes::WorldTable::kSideA, t, pes::PesWorld::SideVa(t));
    w.SetEntry(pes::WorldTable::kSideB, t, pes::PesWorld::TeamInfoVa(t));
  }
}

// The window's record for a manifest kind, or null (G: the window holds its tables; unit: the walk's rdx object, built by the
// window; ctx and its pointers: below).
inline uint8_t* RecordOf(pes::PesWorld& w, int side, const std::string& kind, int idx) {
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "team") return w.TeamInfoRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  if (kind == "bp") return w.Carrier(side);
  if (kind == "ctx") return w.Ctx(side);
  if (kind == "ma") return w.Ma(side);
  if (kind == "e") return reinterpret_cast<uint8_t*>(&w.EventImage());
  return nullptr;
}

// The capture's record of `kind`, located through G as PES's accessors do. A builder call: bp = r8 - 0x1C, ctx = rdx. A walk call
// (`walk`): bp = rcx, ctx = *(rdx + 8).
inline uint64_t CapturedBase(const Oracle& o, uint64_t g, const std::string& kind, int idx, bool walk) {
  const auto entry = [&](size_t table) { return Read(o, g + table + static_cast<size_t>(idx) * pes::kTableEntryBytes, 8); };
  const uint64_t ctx = walk ? Read(o, o.regs.at("rdx") + pes::kUnitCtxOffset, 8) : o.regs.at("rdx");
  if (kind == "B") return Read(o, g + pes::kBallPtrOffset, 8);
  if (kind == "MI") return Read(o, g + pes::kMatchInfoPtrOffset, 8);
  if (kind == "pitch") return Read(o, g + pes::kPitchPtrOffset, 8);
  if (kind == "fr") return Read(o, g + pes::kFrameRandomPtrOffset, 8);
  if (kind == "side") return entry(pes::kSideTableAOffset);
  if (kind == "team") return entry(pes::kSideTableBOffset);
  if (kind == "anime") return entry(pes::kAnimeTableOffset);
  if (kind == "man") return entry(pes::kManTableOffset);
  if (kind == "slot") return entry(pes::kSlotTableOffset);
  if (kind == "bp") return walk ? o.regs.at("rcx") : o.regs.at("r8") - pes::kBpUnitInfoOffset;
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

// The install's table bytes, read once from PES21_ROOT's PES2021.exe as GF's loaders do (the oracle fixture skips without it).
inline const pes::PesImageTables& InstallTables() {
  static const pes::PesImageTables tables = [] {
    std::string error;
    auto exe = pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", &error);
    EXPECT_TRUE(exe) << error;
    auto loaded = exe ? pes::LoadPesImageTables(*exe, &error) : std::nullopt;
    EXPECT_TRUE(loaded) << error;
    return loaded ? *loaded : pes::PesImageTables{};
  }();
  return tables;
}

// `osBytes`: also hand the call the capture's bytes outside the window (the allocator's and the thread block's words the PES
// run read); GF's production run has none of them. `walk`: the capture is a carrier-walk call (its BP flags are not a stack word).
// `flagsFromStack` false: a builder-shaped call (bp = r8 - 0x1C) whose sixth argument is not the refresh flags (0x14211D710's is
// the plan record's address).
// `construct`: run PES's block constructor 0x14210D320 on the empty window first (the objects the capture's reads did not touch,
// the kind objects among them, then exist as PES's constructor leaves them); the capture's bytes are overlaid after it.
inline void Build(const Oracle& o, Built& b, bool osBytes = true, bool walk = false, bool flagsFromStack = true,
                  bool construct = false) {
  pes::PesWorld& w = *b.world;
  BindWorld(w);
  w.BindImageTables(InstallTables());
  const uint64_t g = Read(o, kImageG, 8);
  const uint64_t bp = walk ? o.regs.at("rcx") : o.regs.at("r8") - pes::kBpUnitInfoOffset;
  b.side = static_cast<int>(Read(o, bp + pes::kBpUnitInfoOffset + pes::kUnitSideOffset, 4));
  if (construct) w.RunCarrierConstruct(b.side);
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, b.side, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()), walk);
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
  // A builder call: BP+0x18 is the caller's sixth argument (the refresh flags): the capture's stack word.
  if (!walk && flagsFromStack)
    pes::StoreU32(w.Carrier(b.side), pes::kBpFlagsOffset,
                  static_cast<uint32_t>(Read(o, o.regs.at("rsp") + pes::kDangerArg6Offset, 4)));
}

}  // namespace pes_oracle

namespace pes_oracle {

// Every record the window holds, copied whole from a capture through its G (the carrier-walk call: bp = rcx, ctx = *(rdx + 8)),
// for tests that run a PES writer on the window and compare its output to the capture's bytes (the walk's live-in manifest
// holds only what the walk read). The window's own ctx pointers to MA and E stay.
inline void BuildFull(const Oracle& o, Built& b) {
  pes::PesWorld& w = *b.world;
  BindWorld(w);
  w.BindImageTables(InstallTables());
  const uint64_t g = Read(o, kImageG, 8);
  b.side = static_cast<int>(Read(o, o.regs.at("rcx") + pes::kBpUnitInfoOffset + pes::kUnitSideOffset, 4));
  const auto copy = [&](const char* kind, int idx, size_t bytes, size_t skipLo = 0, size_t skipHi = 0) {
    uint8_t* dst = RecordOf(w, b.side, kind, idx);
    const uint64_t base = CapturedBase(o, g, kind, idx, true);
    ASSERT_NE(dst, nullptr) << kind;
    if (base == 0) return;  // an official the capture's table does not hold (the fourth official has no entry)
    for (size_t i = 0; i < bytes; ++i)
      if (i < skipLo || i >= skipHi) dst[i] = static_cast<uint8_t>(Read(o, base + i, 1));
  };
  copy("B", 0, pes::kBallRecordBytes);
  copy("MI", 0, pes::kMatchInfoBytes);
  copy("pitch", 0, pes::kPitchRecordBytes);
  copy("fr", 0, pes::kFrameRandomBytes);
  for (int t = 0; t < pes::kSideCount; ++t) copy("side", t, pes::kSideRecordBytes);
  for (int t = 0; t < pes::kSideCount; ++t) copy("team", t, pes::kTeamInfoRecordBytes);
  for (int k = 0; k < pes::kAnimeCount; ++k) copy("anime", k, pes::kManRecordOffset);
  for (int k = 0; k < pes::kManCount; ++k) copy("man", k, pes::kManRecordBytes);
  for (int k = 0; k < pes::kSlotCount; ++k) copy("slot", k, pes::kPlayerBlockBytes - pes::kSlotRecordOffset);
  copy("bp", 0, pes::kBpBytes);
  uint8_t* ctx = w.Ctx(b.side);
  uint64_t maVa = 0, eventsVa = 0;
  std::memcpy(&maVa, ctx + pes::kCtxMaOffset, sizeof(maVa));
  std::memcpy(&eventsVa, ctx + pes::kCtxEventsOffset, sizeof(eventsVa));
  copy("ctx", 0, pes::kCtxBytes);
  pes::StoreU64(ctx, pes::kCtxMaOffset, maVa);
  pes::StoreU64(ctx, pes::kCtxEventsOffset, eventsVa);
  copy("ma", 0, pes::kMaBytes);
  copy("e", 0, sizeof(pes::EventStoreImage));
}

}  // namespace pes_oracle
