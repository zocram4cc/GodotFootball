// What the kick oracle tests share (pes_kick_chooser_oracle_test, pes_kick_header_oracle_test): a capture's inputs rebuilt in a
// pes::PesWorld at PES's offsets from the manifest of tools/dev/translate/trap_oracle.py (the call's live-in set: window records
// by kind, heap ranges, image globals, the caller's frame), and a private copy-on-write mapping of the unpacked image.

#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_state.hpp"

namespace pes_kick_oracle {

using namespace pes_oracle;

constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G
constexpr int kSlot = 0;  // the window's P that stands for the capture's kicker (any slot: the request names it)

inline uint8_t* RecordOf(pes::PesWorld& w, const std::string& kind, int idx) {
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  if (kind == "P") return w.P(kSlot);
  return nullptr;
}

// The capture's record of `kind`, located through G as PES's accessors do (P is the call's rdx).
inline uint64_t CapturedBase(const Oracle& o, uint64_t g, const std::string& kind, int idx) {
  const auto entry = [&](size_t table) { return Read(o, g + table + static_cast<size_t>(idx) * pes::kTableEntryBytes, 8); };
  if (kind == "P") return o.regs.at("rdx");
  if (kind == "B") return Read(o, g + pes::kBallPtrOffset, 8);
  if (kind == "MI") return Read(o, g + pes::kMatchInfoPtrOffset, 8);
  if (kind == "pitch") return Read(o, g + pes::kPitchPtrOffset, 8);
  if (kind == "fr") return Read(o, g + pes::kFrameRandomPtrOffset, 8);
  if (kind == "side") return entry(pes::kSideTableAOffset);
  if (kind == "anime") return entry(pes::kAnimeTableOffset);
  if (kind == "man") return entry(pes::kManTableOffset);
  if (kind == "slot") return entry(pes::kSlotTableOffset);
  return 0;
}

struct Built {
  std::unique_ptr<pes::PesWorld> world = std::make_unique<pes::PesWorld>();
  pes::PesKickRequest request;  // the request bytes of the capture's frame (the chooser's capture: the builder's output)
  pes::PesTrapExternals ext;
};

// `requestReg` names the register that holds the request's address in the captured call (the chooser's r8; the builder's rcx): the
// Kick start's two callers (FUN_140724CC0, FUN_1407253D0) keep the request at different offsets from rsp (0xD8 and 0xC8 above it).
inline void Build(const Oracle& o, const uint8_t* image, uint64_t imageBytes, Built& b, const char* requestReg = "r8") {
  pes::PesWorld& w = *b.world;
  const uint64_t g = Read(o, kImageG, 8);
  for (int k = 0; k < pes::kSlotCount; ++k) w.BindSlot(k);
  for (int k = 0; k < pes::kOfficialCount; ++k) w.BindOfficial(pes::kRefereeOfficialIndex + k);
  for (int t = 0; t < pes::kSideCount; ++t) {
    w.SetEntry(pes::WorldTable::kSideA, t, pes::PesWorld::SideVa(t));
    w.SetEntry(pes::WorldTable::kSideB, t, pes::PesWorld::TeamInfoVa(t));
  }
  const uint64_t rsp = o.regs.at("rsp");
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;  // G: the window holds its tables
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()));
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    } else if (line[0] == "H") {
      pes::PesTrapSegment seg;
      seg.va = std::strtoull(line[1].c_str(), nullptr, 16);
      for (size_t i = 0, n = std::strtoul(line[2].c_str(), nullptr, 10); i < n; ++i)
        seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
      b.ext.segments.push_back(std::move(seg));
    } else if (line[0] == "I") {
      const uint64_t va = std::strtoull(line[1].c_str(), nullptr, 16);
      if (va != kImageG) b.ext.globals.push_back({va, Read(o, va, 8)});
    } else if (line[0] == "S") {
      // The caller's frame: the request lives at the register's address; the other words are the return address.
      const size_t off = std::strtoul(line[1].c_str(), nullptr, 16), len = std::strtoul(line[2].c_str(), nullptr, 10);
      const uint64_t requestAt = o.regs.at(requestReg) - rsp;
      for (size_t i = 0; i < len; ++i) {
        const size_t at = off + i;
        if (at < requestAt || at >= requestAt + pes::kKickRequestBytes) continue;
        b.request.bytes[at - requestAt] = static_cast<uint8_t>(Read(o, rsp + at, 1));
      }
    }
  }
  // The roster table the window holds as a stand-in (G+0x180): the capture's own bytes, where its pointer is readable.
  if (const uint64_t roster = Read(o, g + pes::kRosterPtrOffset, 8); roster != 0) {
    uint8_t* dst = w.RosterRecord();
    for (size_t i = 0; i < pes::kRosterEntries * pes::kRosterEntryBytes; ++i) dst[i] = static_cast<uint8_t>(Read(o, roster + i, 1));
  }
  b.ext.image = const_cast<uint8_t*>(image);
  b.ext.imageBytes = imageBytes;
}

struct Image {
  uint8_t* bytes = nullptr;
  uint64_t size = 0;
  ~Image() {
    if (bytes != nullptr) munmap(bytes, size);
  }
  // A private copy-on-write mapping: the translated code writes image globals, and the file stays as it is.
  bool Open(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0) return false;
    size = static_cast<uint64_t>(st.st_size);
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return false;
    bytes = static_cast<uint8_t*>(p);
    return true;
  }
};

}  // namespace pes_kick_oracle
