// PES's kick chooser FUN_1408364D0 (translated, capture-bounded closure) run on GF's own window from the recorded
// kick_chooser_8364D0 calls. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets: the kicker P and the ball,
// match-info, man and slot records through the window's records (the call's live-in set, manifest.tsv), the request through
// PesWorld::RunKickChooser's frame. What GF holds no state for yet (the install's image, the loader data, the capture's heap
// ranges) comes from the capture as bytes at their VAs (PesTrapExternals). The descriptor must equal PES's on every byte the
// reference call wrote.
//
// GF_PES_KICK_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/trap_oracle.py from a snapdiff case of 0x1408364D0 (see docs/ports/pes-port-KickKind2.md). PES_EXE names the
// unpacked image. Without either, skip.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_state.hpp"

namespace {

using namespace pes_oracle;

constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G

uint8_t* RecordOf(pes::PesWorld& w, const std::string& kind, int idx) {
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  if (kind == "P") return w.P();
  return nullptr;
}

// The capture's record of `kind`, located through G as PES's accessors do (P is the call's rdx).
uint64_t CapturedBase(const Oracle& o, uint64_t g, const std::string& kind, int idx) {
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
  pes::PesKickRequest request;
  pes::PesTrapExternals ext;
};

void Build(const Oracle& o, const uint8_t* image, uint64_t imageBytes, Built& b) {
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
      // The caller's frame: the request lives at rsp + kKickRequestFrameOffset (r8); the other words are the return address.
      const size_t off = std::strtoul(line[1].c_str(), nullptr, 16), len = std::strtoul(line[2].c_str(), nullptr, 10);
      for (size_t i = 0; i < len; ++i) {
        const size_t at = off + i;
        if (at < pes::kKickRequestFrameOffset || at >= pes::kKickRequestFrameOffset + pes::kKickRequestBytes) continue;
        b.request.bytes[at - pes::kKickRequestFrameOffset] = static_cast<uint8_t>(Read(o, rsp + at, 1));
      }
    }
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

// PES's descriptor: the bytes the reference call wrote into [rcx, rcx + kKickDescriptorBytes), from its write log.
std::map<size_t, uint8_t> ReferenceWrites(const Oracle& o) {
  std::map<size_t, uint8_t> bytes;
  const uint64_t rcx = o.regs.at("rcx");
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr < rcx || addr >= rcx + pes::kKickDescriptorBytes) continue;
    bytes[addr - rcx] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return bytes;
}

class KickChooserOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_KICK_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_KICK_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
    exe_ = exe;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
};

TEST_F(KickChooserOracle, EveryCapturedCallOnGfsWindowGivesPesDescriptor) {
  size_t real = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b);
    const pes::PesKickDescriptor got = b.world->RunKickChooser(b.request, b.ext);
    const std::map<size_t, uint8_t> want = ReferenceWrites(o);
    ASSERT_FALSE(want.empty()) << dir << ": the reference wrote no descriptor byte";
    for (const auto& [off, value] : want) EXPECT_EQ(got.bytes[off], value) << dir << " descriptor+0x" << std::hex << off;
    real += got.Kind() != pes::kKickNoKind;
  }
  RecordProperty("real_kinds", static_cast<int>(real));
}

}  // namespace
