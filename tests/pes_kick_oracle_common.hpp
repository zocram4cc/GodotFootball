// What the kick oracle tests share (pes_kick_chooser_oracle_test, pes_kick_header_oracle_test): a capture's inputs rebuilt in a
// pes::PesWorld at PES's offsets from the manifest of tools/dev/translate/trap_oracle.py (the call's live-in set: window records
// by kind, named heap objects, heap ranges, image globals, the caller's frame), and a private copy-on-write mapping of the unpacked
// image.

#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "onthepitch/pesanimechain.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"
#include "pes/vfs.hpp"
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

inline uint64_t CapturedTableB(const Oracle& o, uint64_t g, int side) {
  return Read(o, g + pes::kSideTableBOffset + pes::kTableEntryBytes * static_cast<size_t>(side), 8);
}

// The heap objects the call reads that GF holds as state, from the manifest's "N name rel len" lines. MA and the event store E go into
// the window's records of the kicker's side at the captured offsets; the squad record SBn into the team-info record table B names. The
// side's context An (P+0x3AC0, +0x18 -> MA, +0xE0 -> E) is the window's own, as WriteActorP binds it (Build sets P+0x3AC0 last).
// GX and GY (the anime player A = *(P+0xA08) and its child B = *(A+0xF0), which the root-motion reader FUN_141AFDF10 walks down to the
// layer and clip data) are not copied: the window's own chain stands in (PesWorld::BindAnimeChain, pesanimechain.hpp), built from the
// install, so the reader's answer comes from GF's data and is checked against PES's (pes_kick_root_test).
inline void CopyNamed(const Oracle& o, uint64_t g, Built& b, const std::vector<std::string>& line, int side) {
  const std::string& name = line[1];
  const size_t rel = std::strtoul(line[2].c_str(), nullptr, 16), len = std::strtoul(line[3].c_str(), nullptr, 10);
  const uint64_t rdx = o.regs.at("rdx");
  const uint64_t an = Read(o, rdx + pes::kPPtr3AC0Offset, 8), gx = Read(o, rdx + pes::kPScalePtrOffset, 8);
  pes::PesWorld& w = *b.world;
  uint8_t* dst = nullptr;
  uint64_t base = 0;
  if (name == "MA") {
    dst = w.Ma(side);
    base = Read(o, an + pes::kCtxMaOffset, 8);
  } else if (name == "E") {
    dst = reinterpret_cast<uint8_t*>(&w.EventImage());
    base = Read(o, an + pes::kCtxEventsOffset, 8);
  } else if (name.rfind("SB", 0) == 0) {
    const int s = name[2] - '0';
    dst = w.TeamInfoRecord(s);
    base = CapturedTableB(o, g, s);
  } else if (name == "GX" || name == "GY") {
    return;  // the window's anime chain
  } else {
    return;  // An: the window's own
  }
  for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
}

// `requestReg` names the register that holds the request's address in the captured call (the chooser's r8; the builder's rcx): the
// Kick start's two callers (FUN_140724CC0, FUN_1407253D0) keep the request at different offsets from rsp (0xD8 and 0xC8 above it).
inline void Build(const Oracle& o, const uint8_t* image, uint64_t imageBytes, Built& b, const pes::PesAnimeChain* chain,
                  const char* requestReg = "r8") {
  pes::PesWorld& w = *b.world;
  const uint64_t g = Read(o, kImageG, 8);
  for (int k = 0; k < pes::kSlotCount; ++k) w.BindSlot(k);
  for (int k = 0; k < pes::kOfficialCount; ++k) w.BindOfficial(pes::kRefereeOfficialIndex + k);
  for (int t = 0; t < pes::kSideCount; ++t) {
    w.SetEntry(pes::WorldTable::kSideA, t, pes::PesWorld::SideVa(t));
    w.SetEntry(pes::WorldTable::kSideB, t, pes::PesWorld::TeamInfoVa(t));
  }
  const uint64_t rsp = o.regs.at("rsp");
  const int side = static_cast<int>(Read(o, o.regs.at("rdx") + pes::kPByte2458Offset, 1)) / pes::kSlotsPerSide;
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;  // G: the window holds its tables
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()));
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    } else if (line[0] == "H") {
      const uint64_t va = std::strtoull(line[1].c_str(), nullptr, 16);
      const size_t n = std::strtoul(line[2].c_str(), nullptr, 10);
      // Bytes of a squad record beyond its named head (the player-info entries: the key at +0x30, the id at +0x34 and +0x118, read by
      // FUN_140A3B2A0 and FUN_140A62D90) belong to the window's team-info record that table B names; every other heap range stays at
      // its own VA.
      bool inRecord = false;
      for (int s = 0; s < pes::kSideCount; ++s) {
        const uint64_t record = CapturedTableB(o, g, s);
        if (va < record || va + n > record + pes::kTeamInfoRecordBytes) continue;
        for (size_t i = 0; i < n; ++i) w.TeamInfoRecord(s)[va - record + i] = static_cast<uint8_t>(Read(o, va + i, 1));
        inRecord = true;
      }
      if (inRecord) continue;
      pes::PesTrapSegment seg;
      seg.va = va;
      for (size_t i = 0; i < n; ++i) seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
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
    } else if (line[0] == "N") {
      CopyNamed(o, g, b, line, side);
    }
  }
  // P+0x3AC0 names the side's context in the window, as WriteActorP binds it (the copied word is the capture's heap VA).
  const uint64_t ctx = pes::PesWorld::CtxVa(side);
  std::memcpy(w.P(kSlot) + pes::kPPtr3AC0Offset, &ctx, sizeof ctx);
  // The roster table the window holds as a stand-in (G+0x180): the capture's own bytes, where its pointer is readable.
  if (const uint64_t roster = Read(o, g + pes::kRosterPtrOffset, 8); roster != 0) {
    uint8_t* dst = w.RosterRecord();
    for (size_t i = 0; i < pes::kRosterEntries * pes::kRosterEntryBytes; ++i) dst[i] = static_cast<uint8_t>(Read(o, roster + i, 1));
  }
  if (chain != nullptr) {
    w.BindAnimeChain(kSlot);
    chain->AddTo(b.ext);  // after the capture's own globals: the chain's database global wins
  }
  b.ext.image = const_cast<uint8_t*>(image);
  b.ext.imageBytes = imageBytes;
}

// The anime chain GF builds from the install (PES21_ROOT): the body archives, the key table, the layer objects. Built once.
inline const pes::PesAnimeChain* GfAnimeChain(std::string* why) {
  static std::optional<pes::PesAnimeChain> chain;
  static std::string error;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    if (root == nullptr || *root == '\0') {
      error = "PES21_ROOT unset";
    } else {
      static pes::PesVfs vfs;
      auto exe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &error);
      if (exe && vfs.Mount(root, &error)) chain = pes::BuildAnimeChain(*exe, vfs, &error);
    }
  }
  *why = error;
  return chain ? &*chain : nullptr;
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
