// PES's Trap action chain (the kind-0 control touch 0x14079A470 and its 340-function closure, translated) run on GF's own
// window, from captured calls. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets: P through its named
// fields (pestrapworld.hpp), the ball, match-info, slot, man and anime records through the window's records, the caller's frame
// and request through RunTrapKind0. What GF holds no state for yet (the install's image, the constant registry and the anime
// manager, the motion table's records) comes from the capture as bytes at their VAs (PesTrapExternals). The result must equal
// PES's: the issued flag and the out word. Each named binding is then changed by one byte and the call's path must change.
//
// GF_PES_TRAP_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/trap_oracle.py: mem.bin (a link to the capture), maps.txt, ok_calls.bin, ok_delta.bin, pages.bin, teb.txt,
// corpus.jsonl (the PES reference row) and manifest.tsv (the call's live-in set). PES_EXE names the unpacked image. Without
// either, skip.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pesworldrecord.hpp"
#include "pes_state.hpp"

namespace {

constexpr uint64_t kPage = 4096;
constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G

struct CallRec {
  uint64_t v[49];  // 16 GPRs, 16 XMM (lo, hi), the return address
};

struct Oracle {
  SnapView snap;
  int pagesFd = -1;
  std::vector<uint64_t> deltas;
  std::string row;  // corpus.jsonl's one reference row
  std::map<std::string, uint64_t> regs;
  std::vector<std::vector<std::string>> manifest;
};

std::vector<uint8_t> Slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

bool Load(const std::string& dir, Oracle& o) {
  o.snap.fd = open((dir + "/mem.bin").c_str(), O_RDONLY);
  o.pagesFd = open((dir + "/pages.bin").c_str(), O_RDONLY);
  if (o.snap.fd < 0 || o.pagesFd < 0) return false;
  FILE* mp = fopen((dir + "/maps.txt").c_str(), "r");
  if (mp == nullptr) return false;
  unsigned long long rva, rsize, roff;
  while (fscanf(mp, "%llx %llx %llx", &rva, &rsize, &roff) == 3) o.snap.runs.push_back({rva, rsize, roff});
  fclose(mp);
  const std::vector<uint8_t> delta = Slurp(dir + "/ok_delta.bin");
  o.deltas.resize(delta.size() / sizeof(uint64_t));
  std::memcpy(o.deltas.data(), delta.data(), o.deltas.size() * sizeof(uint64_t));
  std::ifstream corpus(dir + "/corpus.jsonl");
  std::getline(corpus, o.row);
  std::ifstream manifest(dir + "/manifest.tsv");
  for (std::string line; std::getline(manifest, line);) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    for (std::string col; std::getline(ss, col, '\t');) cols.push_back(col);
    if (cols.empty() || cols[0][0] == '#') continue;
    if (cols[0] == "R") o.regs[cols[1]] = std::strtoull(cols[2].c_str(), nullptr, 16);
    o.manifest.push_back(cols);
  }
  return !o.row.empty() && !o.manifest.empty();
}

uint64_t Read(const Oracle& o, uint64_t va, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) {
    const uint8_t* b = o.snap.at(va + i);
    if (b == nullptr) return 0;
    v |= static_cast<uint64_t>(*b) << (8 * i);
  }
  return v;
}

// The call's delta pages over the snapshot (the case's memory as the capture saw it at entry).
void SetOverlay(Oracle& o) {
  o.snap.over.clear();
  size_t pos = 0;
  const uint64_t np = o.deltas.at(pos++);
  for (uint64_t j = 0; j < np; ++j, pos += 2) {
    SnapView::Block b{o.deltas.at(pos), std::vector<uint8_t>(kPage)};
    if (pread(o.pagesFd, b.bytes.data(), kPage, static_cast<off_t>(o.deltas.at(pos + 1))) != static_cast<ssize_t>(kPage)) return;
    o.snap.over.push_back(std::move(b));
  }
  std::sort(o.snap.over.begin(), o.snap.over.end(),
            [](const SnapView::Block& a, const SnapView::Block& b) { return a.va < b.va; });
}

std::string Field(const std::string& row, const char* key) {
  const std::string pat = std::string("\"") + key + "\": \"";
  const size_t at = row.find(pat);
  if (at == std::string::npos) return {};
  const size_t from = at + pat.size();
  return row.substr(from, row.find('"', from) - from);
}

// PES's out word: the last bytes the reference's write log stored at the out pointer (the chain's 6th argument).
uint32_t ReferenceOut(const Oracle& o) {
  const uint64_t out = Read(o, o.regs.at("rsp") + pes::kFrameOutPtrOffset, 8);
  uint32_t word = static_cast<uint32_t>(Read(o, out, 4));
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr < out || addr >= out + 4) continue;
    const uint32_t byte = static_cast<uint32_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
    word = (word & ~(0xFFu << (8 * (addr - out)))) | (byte << (8 * (addr - out)));
  }
  return word;
}

uint8_t* RecordOf(pes::PesWorld& w, const std::string& kind, int idx) {
  if (kind == "B") return w.BallRecord();
  if (kind == "MI") return w.MatchInfo();
  if (kind == "pitch") return w.PitchRecord();
  if (kind == "fr") return w.FrameRandomRecord();
  if (kind == "side") return w.SideRecord(idx);
  if (kind == "anime") return w.AnimeRecord(idx);
  if (kind == "man") return w.ManRecord(idx);
  if (kind == "slot") return w.Block(idx) + pes::kSlotRecordOffset;
  return nullptr;
}

// The capture's record of `kind`, located through G as PES's accessors do.
uint64_t CapturedBase(const Oracle& o, uint64_t g, const std::string& kind, int idx) {
  const auto entry = [&](size_t table) { return Read(o, g + table + static_cast<size_t>(idx) * pes::kTableEntryBytes, 8); };
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

// P's named fields (pestrapworld.hpp): the only bytes of P the window takes from the capture.
struct PField {
  const char* name;
  size_t offset, bytes;
};
const std::vector<PField>& PFields() {
  static const std::vector<PField> fields = {
      {"position", pes::kPPosOffset, 12},        {"next position", pes::kPNextPosOffset, 12},
      {"heading", pes::kPHeadingOffset, 4},      {"velocity", pes::kPVelocityOffset, 12},
      {"word 0x430", pes::kPWord430Offset, 4},   {"scale pointer", pes::kPScalePtrOffset, 8},
      {"interface vtable 0", pes::kPInterfaceVtable0Offset, 8}, {"interface vtable 1", pes::kPInterfaceVtable1Offset, 8},
      {"byte 0xAC8", pes::kPByteAC8Offset, 1},   {"byte 0xAC9", pes::kPByteAC9Offset, 1},
      {"word 0xAE0", pes::kPWordAE0Offset, 2},   {"byte 0xB08", pes::kPByteB08Offset, 1},
      {"byte 0x2458", pes::kPByte2458Offset, 1}, {"scale 0x377C", pes::kPF377CScaleOffset, 4},
      {"pointer 0x3AC0", pes::kPPtr3AC0Offset, 8}, {"scale 0x3B18", pes::kPF3B18Offset, 4},
  };
  return fields;
}

struct Built {
  std::unique_ptr<pes::PesWorld> world = std::make_unique<pes::PesWorld>();
  pes::PesTrapRequest request;
  pes::PesTrapExternals ext;
};

void Build(const Oracle& o, const uint8_t* image, uint64_t imageBytes, Built& b) {
  pes::PesWorld& w = *b.world;
  const uint64_t g = Read(o, kImageG, 8);
  for (int k = 0; k < pes::kSlotCount; ++k) w.BindSlot(k);
  for (int k = 0; k < pes::kOfficialCount; ++k) w.BindOfficial(pes::kRefereeOfficialIndex + k);
  for (int t = 0; t < pes::kSideCount; ++t) {
    w.SetEntry(pes::WorldTable::kSideA, t, pes::PesWorld::SideVa(t));
    w.SetEntry(pes::WorldTable::kSideB, t, pes::PesWorld::SideVa(t));
  }
  const uint64_t rdx = o.regs.at("rdx");
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;  // G (the window holds its tables) and P (named fields below)
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
    }
  }
  for (const PField& f : PFields())
    for (size_t i = 0; i < f.bytes; ++i) w.P()[f.offset + i] = static_cast<uint8_t>(Read(o, rdx + f.offset + i, 1));
  for (const auto& line : o.manifest) {
    if (line[0] != "F") continue;  // a pointer of P into another window record
    const uint64_t at = std::strtoull(line[1].c_str(), nullptr, 16);
    const uint64_t target = (line[2] == "slot" ? pes::PesWorld::SlotVa(std::atoi(line[3].c_str())) : 0) +
                            std::strtoull(line[4].c_str(), nullptr, 16);
    std::memcpy(w.P() + at, &target, sizeof target);
  }
  // The request, as the dispatcher 0x1407A01C0 took it (kind 0): its words are the chain's stack arguments in the capture.
  const uint64_t rsp = o.regs.at("rsp"), action = rdx + pes::kPActionOffset;
  uint8_t* req = b.request.bytes.data();
  const auto put = [req](size_t off, uint64_t v, size_t n) { std::memcpy(req + off, &v, n); };
  put(pes::kReqWord0Offset, Read(o, o.regs.at("r9"), 4), 4);
  put(pes::kReqWord1Offset, Read(o, rsp + pes::kFrameArg5Offset, 4), 4);
  put(pes::kReqWord3Offset, Read(o, rsp + pes::kFrameArg7Offset, 4), 4);
  put(pes::kReqByte1EOffset, Read(o, rsp + pes::kFrameArg9Offset, 1), 1);
  put(pes::kReqByte1FOffset, Read(o, rsp + pes::kFrameArg10Offset, 1), 1);
  put(pes::kReqByte20Offset, Read(o, rsp + pes::kFrameArg11Offset, 1), 1);
  put(pes::kReqByte22Offset, Read(o, rsp + pes::kFrameArg12Offset, 1), 1);
  put(pes::kReqFlagAOffset, Read(o, rsp + pes::kFrameArg13Offset, 1), 1);
  put(pes::kReqByte1DOffset, Read(o, action + pes::kActionByte3DOffset, 1), 1);
  put(pes::kReqByte21Offset, Read(o, action + pes::kActionByte3EOffset, 1), 1);
  b.ext.image = const_cast<uint8_t*>(image);
  b.ext.imageBytes = imageBytes;
}

struct Image {
  uint8_t* bytes = nullptr;
  uint64_t size = 0;
  ~Image() {
    if (bytes != nullptr) munmap(bytes, size);
  }
  // A private copy-on-write mapping: the chain writes image globals, and the file stays as it is.
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

class TrapOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_TRAP_ORACLE");
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << "GF_PES_TRAP_ORACLE / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
    exe_ = exe;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
};

struct RunOut {
  pes::PesTrapResult result;
  std::string writes;
  bool operator==(const RunOut& o) const {
    return result.issued == o.result.issued && result.out == o.result.out && writes == o.writes;
  }
};

RunOut RunCall(const Oracle& o, const std::string& exe, const std::function<void(Built&)>& mutate = {}) {
  Image image;
  EXPECT_TRUE(image.Open(exe));
  Built b;
  Build(o, image.bytes, image.size, b);
  if (mutate) mutate(b);
  std::vector<std::pair<uint64_t, uint8_t>> log;
  b.ext.writeLog = &log;
  RunOut out;
  out.result = b.world->RunTrapKind0(b.request, b.ext);
  char buf[24];
  for (const auto& wr : log) {
    // Window addresses differ from the capture's: the written values, in order, are what the path leaves.
    std::snprintf(buf, sizeof buf, "%02x", static_cast<unsigned>(wr.second));
    out.writes += buf;
  }
  return out;
}

TEST_F(TrapOracle, EveryCapturedCallOnGfsWindowGivesPesOutput) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    const RunOut got = RunCall(o, exe_);
    const bool wantIssued = (std::strtoull(Field(o.row, "rax").c_str(), nullptr, 16) & 0xFF) != 0;
    EXPECT_EQ(got.result.issued, wantIssued) << dir;
    EXPECT_EQ(got.result.out, ReferenceOut(o)) << dir;
  }
}

}  // namespace
