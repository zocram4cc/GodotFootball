// PES's Trap action chain (the kind-0 control touch 0x14079A470 and its 340-function closure, translated) run on GF's own
// window, from captured calls. Each capture's inputs are rebuilt in a pes::PesWorld at PES's offsets: the ball, match-info, slot,
// man and anime records through the window's records, the caller's frame and request through RunTrapKind0, and P derived from
// GF's state by pes::WriteActorP (pestrapactor.hpp: the actor's man record, slot, height, stamina, phase and action ids; none
// of P's bytes is copied from the capture except the named open inputs P+0x2FC, the G chain at +0xA08 and +0x3AC0). What GF
// holds no state for yet (the install's image, the constant registry and the anime manager, the motion table's records, the
// heap behind the open pointers) comes from the capture as bytes at their VAs (PesTrapExternals). The result must equal PES's:
// the issued flag, the out word and every byte written to the Trap action object (P+0xC80..+0xCDF, the clip at action+0x8
// among them). Each named binding is then changed by one byte and the call's path must change; P derived from the facts must
// equal the captured P field by field; the rate ceiling P+0x377C must equal PES's bit for bit.
//
// GF_PES_TRAP_ORACLE names a directory of per-capture directories (PES-derived, never committed), each made by
// tools/dev/translate/trap_oracle.py: mem.bin (a link to the capture), maps.txt, ok_calls.bin, ok_delta.bin, pages.bin, teb.txt,
// corpus.jsonl (the PES reference row) and manifest.tsv (the call's live-in set and the "A" actor facts). PES_EXE names the
// unpacked flat image, PES21_ROOT the install (PeImage reads PES2021.exe for the stamina constants). Without any, skip.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "onthepitch/pestrapdata.hpp"
#include "pes/exe.hpp"
#include "pes/physique.hpp"
#include "pes/vfs.hpp"
#include "pes/translated/translated.hpp"
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
  std::map<std::string, std::string> actor;  // the "A" lines: facts GF holds as state, read from the capture
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
    if (cols[0] == "A") o.actor[cols[1]] = cols[2];
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

// What PES's image gives the actor's derivation (the translated trap chain reads the same image): loaded once.
struct Derivation {
  pes::PesActorConstants actor;
  pes::StaminaConstants stamina;
  pes::PhysiqueConstants physique;
};
const Derivation* g_derivation = nullptr;
const pes::PesTrapLoaderData* g_loader = nullptr;  // built once per process (PES builds it once per match)
const pes::PesTrapSource* g_source = nullptr;      // the install's image slice, actor constants and loader data

float F32(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}
uint32_t Bits(const std::string& s) { return static_cast<uint32_t>(std::strtoull(s.c_str(), nullptr, 0)); }

// The captured G (*(*(P+0xA08)+0xF0)+0xC0): the one value of the physique chain the capture holds.
float CapturedG(const Oracle& o) {
  const uint64_t gx = Read(o, o.regs.at("rdx") + pes::kPScalePtrOffset, 8);
  return F32(static_cast<uint32_t>(Read(o, Read(o, gx + pes::kPhysiqueNextOffset, 8) + pes::kPhysiqueGOffset, 4)));
}

// The leg-length slider (physique block +0x30; the appearance nibble 0x11:0 decodes to 2n/14 - 1, 15 = 0, FUN_141AD98B0) is
// not in a slim capture (the player object is not): it is the NAMED oracle input recovered from PES's own G, the one of the
// sixteen decoded values whose scale at the actor's height is bit-equal to the captured G (0 when none: the G tests then fail).
// GF has no slider source (pestrapactor.hpp). Known capture: pid2670420/seq043, 191 cm, G = 1.0412 = slider -1.
float RecoverLegSlider(const Oracle& o, float heightCm) {
  const float want = CapturedG(o);
  float candidates[16];
  for (int n = 0; n < 15; ++n) candidates[n] = (2.0f * static_cast<float>(n)) / 14.0f - 1.0f;
  candidates[15] = 0.0f;
  for (const float slider : candidates)
    if (pes::PhysiqueG(g_derivation->physique, {heightCm, slider, false}) == want) return slider;
  return 0.0f;
}

// The facts GF holds as state, as the capture's player had them: P's own bytes for the slot, phase, action ids, motion and
// kick-approach word (what GF's MoveStep and action state map to), the height and the stamina from the "A" lines.
pes::PesActorFacts FactsOf(const Oracle& o) {
  const uint64_t p = o.regs.at("rdx");
  pes::PesActorFacts f;
  f.slot = static_cast<int>(Read(o, p + pes::kPByte2458Offset, 1));
  f.phase = static_cast<uint8_t>(Read(o, p + pes::kPPhaseOffset, 1));
  f.action = static_cast<uint8_t>(Read(o, p + pes::kPByteAC8Offset, 1));
  f.sub = static_cast<uint8_t>(Read(o, p + pes::kPByteAC9Offset, 1));
  f.prevAction = static_cast<uint8_t>(Read(o, p + pes::kPPrevActionOffset, 1));
  f.prevSub = static_cast<uint8_t>(Read(o, p + pes::kPPrevActionOffset + 1, 1));
  f.motion = static_cast<uint16_t>(Read(o, p + pes::kPWordAE0Offset, 2));
  f.kickApproach = static_cast<uint32_t>(Read(o, p + pes::kPWord430Offset, 4));
  f.word277C = F32(static_cast<uint32_t>(Read(o, p + pes::kPWord277COffset, 4)));
  f.word2780 = F32(static_cast<uint32_t>(Read(o, p + pes::kPWord277COffset + 4, 4)));
  f.heightCm = F32(Bits(o.actor.at("height_bits")));
  f.legSlider = RecoverLegSlider(o, f.heightCm);
  f.speedByte = static_cast<float>(std::atoi(o.actor.at("speed_byte").c_str()));
  f.stamina.cur = std::atoi(o.actor.at("stamina_cur").c_str());
  f.stamina.max = std::atoi(o.actor.at("stamina_max").c_str());
  f.stamina.breath = std::atoi(o.actor.at("breath").c_str());
  return f;
}

// The named oracle inputs (pestrapactor.hpp): captured bytes for what GF has no source for.
pes::PesActorOpenInputs OpenOf(const Oracle& o) {
  const uint64_t p = o.regs.at("rdx");
  pes::PesActorOpenInputs open;
  for (size_t i = 0; i < open.velocityRing.size(); ++i)
    open.velocityRing[i] = static_cast<uint8_t>(Read(o, p + pes::kPVelocityOffset + i, 1));
  return open;
}

// The actor's GF-side movement bytes: the man record's facing, position, previous position and velocity (+0x554..+0x57F),
// which the chain does not read itself (P is made from them).
void CopyActorMan(const Oracle& o, uint64_t g, pes::PesWorld& w, int slot) {
  uint8_t* man = w.ManRecord(slot);
  const uint64_t base = CapturedBase(o, g, "man", slot);
  for (size_t i = pes::kManFacingOffset; i < pes::kManVelXOffset + 12; ++i) man[i] = static_cast<uint8_t>(Read(o, base + i, 1));
}

struct Built {
  std::unique_ptr<pes::PesWorld> world = std::make_unique<pes::PesWorld>();
  pes::PesTrapRequest request;
  pes::PesTrapExternals ext;
  int slot = 0;  // the actor's slot (his P in the window)
};

// The heap objects the chain reads that GF holds as state (or names as inputs), from the manifest's "N name rel len" lines. An
// and the physique chain are derived (WriteActorP); MA and the event store E go into the window's records of the actor's side
// at the captured offsets; the squad record of table B (SBn) is bound to a captured segment at its own VA.
void CopyNamed(const Oracle& o, uint64_t g, Built& b, const std::vector<std::string>& line, int side) {
  const std::string& name = line[1];
  const size_t rel = std::strtoul(line[2].c_str(), nullptr, 16), len = std::strtoul(line[3].c_str(), nullptr, 10);
  const uint64_t an = Read(o, o.regs.at("rdx") + pes::kPPtr3AC0Offset, 8);
  uint8_t* dst = nullptr;
  uint64_t base = 0;
  if (name == "MA") {
    dst = b.world->Ma(side);
    base = Read(o, an + pes::kCtxMaOffset, 8);
  } else if (name == "E") {
    dst = reinterpret_cast<uint8_t*>(&b.world->EventImage());
    base = Read(o, an + pes::kCtxEventsOffset, 8);
  } else if (name.rfind("SB", 0) == 0) {
    const int s = name[2] - '0';
    const uint64_t record = Read(o, g + pes::kSideTableBOffset + pes::kTableEntryBytes * static_cast<size_t>(s), 8);
    b.world->SetEntry(pes::WorldTable::kSideB, s, record);
    pes::PesTrapSegment seg;
    seg.va = record + rel;
    for (size_t i = 0; i < len; ++i) seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
    b.ext.segments.push_back(std::move(seg));
    return;
  } else {
    return;  // An, GX, GY: derived
  }
  for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
}

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
  const int side = static_cast<int>(Read(o, rdx + pes::kPByte2458Offset, 1)) / pes::kSlotsPerSide;
  for (const auto& line : o.manifest) {
    if (line[0] == "T") {
      uint8_t* dst = RecordOf(w, line[1], std::atoi(line[2].c_str()));
      if (dst == nullptr) continue;  // G (the window holds its tables) and P (named fields below)
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()));
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    } else if (line[0] == "N") {
      CopyNamed(o, g, b, line, side);
    }
  }
  // P is derived from the facts GF holds (pestrapactor.hpp), the actor's man record bytes and the named open inputs; none of
  // its bytes is copied from the capture.
  pes::PesActorFacts facts = FactsOf(o);
  facts.side = facts.slot / pes::kSlotsPerSide;
  b.slot = facts.slot;
  CopyActorMan(o, g, w, facts.slot);
  pes::ConstructActorP(w, facts.slot, g_derivation->actor);  // as a match does once, before the per-tick writes
  pes::ConstructAnimPlugin(w, facts.slot);
  pes::WriteActorP(w, facts, OpenOf(o), g_derivation->actor, g_derivation->stamina, g_derivation->physique);
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
  g_loader->AddTo(b.ext);  // the loader data (motion table, Animetable, registry, trap.o): built from the install, never copied from the capture
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
  // The environment variable naming the directory of oracle directories (the tick oracle reads its own).
  virtual const char* OracleEnv() const { return "GF_PES_TRAP_ORACLE"; }
  void SetUp() override {
    const char* dir = std::getenv(OracleEnv());
    const char* exe = std::getenv("PES_EXE");
    if (dir == nullptr || *dir == '\0' || exe == nullptr || *exe == '\0')
      GTEST_SKIP() << OracleEnv() << " / PES_EXE unset (PES-derived captures and image, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv") && std::filesystem::exists(e.path() / "mem.bin"))
        dirs_.push_back(e.path().string());  // (exists follows the capture link: a removed capture is skipped)
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
    exe_ = exe;
    // The install's trap data: the actor constants P is made from, the image slice and the loader data, built once per process
    // (PES builds the loader once per match). The chain itself is run on the flat image below and, in the match-memory test, on
    // the slice.
    std::string err;
    const char* root = std::getenv("PES21_ROOT");  // (PES_EXE is the flat image; PeImage reads the install's file)
    if (root == nullptr || *root == '\0') GTEST_SKIP() << "PES21_ROOT unset";
    const auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err);
    ASSERT_TRUE(pe.has_value()) << err;
    const auto stamina = pes::LoadStaminaConstants(*pe, &err);
    ASSERT_TRUE(stamina.has_value()) << err;
    const auto physique = pes::LoadPhysiqueConstants(*pe, &err);
    ASSERT_TRUE(physique.has_value()) << err;
    pes::PesVfs vfs;
    ASSERT_TRUE(vfs.Mount(root, &err)) << err;
    static const auto source = pes::LoadTrapSource(*pe, vfs, exe, &err);
    ASSERT_TRUE(source.has_value()) << err;
    g_source = &*source;
    g_loader = &source->loader;
    derivation_ = Derivation{source->actor, *stamina, *physique};
    g_derivation = &derivation_;
  }
  std::vector<std::string> dirs_;
  std::string exe_;
  Derivation derivation_;
};

// The Trap action object's bytes a call writes: P+0xC80..+0xCDF (action +0x8..+0x64), as offset from P+0xC80 to the last
// byte written. `u16 action+0x8` (offsets 0 and 1) is the committed clip (docs/re/pes-re-trap-inputs.md 3.2).
constexpr uint64_t kActionLo = 0xC80, kActionHi = 0xCE0;
using ActionBytes = std::map<uint32_t, uint8_t>;

struct RunOut {
  pes::PesTrapResult result;
  std::string writes;
  ActionBytes action;
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
  out.result = b.world->RunTrapKind0(b.slot, b.request, b.ext);
  char buf[24];
  for (const auto& wr : log) {
    // Window addresses differ from the capture's: the written values, in order, are what the path leaves.
    std::snprintf(buf, sizeof buf, "%02x", static_cast<unsigned>(wr.second));
    out.writes += buf;
    const uint64_t pva = pes::PesWorld::PVa(b.slot);
    if (wr.first >= pva + kActionLo && wr.first < pva + kActionHi)
      out.action[static_cast<uint32_t>(wr.first - pva - kActionLo)] = wr.second;
  }
  return out;
}

// PES's writes to the action object: the last byte the reference log stored at each address of P+0xC80..+0xCDF.
ActionBytes ReferenceAction(const Oracle& o) {
  ActionBytes action;
  const uint64_t p = o.regs.at("rdx"), lo = p + kActionLo, hi = p + kActionHi;
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    const uint64_t addr = std::strtoull(w.substr(at, 16).c_str(), nullptr, 16);
    if (addr >= lo && addr < hi)
      action[static_cast<uint32_t>(addr - lo)] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return action;
}

// The call's result, the bytes it commits to the Trap action object (the clip at action+0x8 among them) and the bytes it
// leaves alone all equal PES's on every capture. An issued call writes the action object; a refusal writes nothing there.
TEST_F(TrapOracle, EveryCapturedCallOnGfsWindowGivesPesOutput) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    const RunOut got = RunCall(o, exe_);
    const bool wantIssued = (std::strtoull(Field(o.row, "rax").c_str(), nullptr, 16) & 0xFF) != 0;
    EXPECT_EQ(got.result.issued, wantIssued) << dir;
    EXPECT_EQ(got.result.out, ReferenceOut(o)) << dir;
    const ActionBytes want = ReferenceAction(o);
    EXPECT_EQ(got.action, want) << dir << ": the action object's bytes (offset from P+0xC80 -> byte)";
    EXPECT_EQ(want.empty(), !wantIssued) << dir << ": PES's issued flag and its writes to the action object disagree";
  }
}

// P derived from GF's facts (pestrapactor.hpp) equals the capture's P on every field the derivation owns, field by field. The
// fields that stay captured are the named open inputs (pestrapactor.hpp PesActorOpenInputs): P+0x2FC, +0xA08 (G), +0x3AC0.
TEST_F(TrapOracle, DerivedPEqualsPesPOnEveryDerivedField) {
  struct DerivedField {
    const char* name;
    size_t offset, bytes;
  };
  const std::vector<DerivedField> fields = {
      {"position", pes::kPPosOffset, 12},
      {"previous position", pes::kPNextPosOffset, 12},
      {"heading", pes::kPHeadingOffset, 4},
      {"slot", pes::kPByte2458Offset, 1},
      {"locomotion phase", pes::kPPhaseOffset, 1},
      {"interface vtable 0", pes::kPInterfaceVtable0Offset, 8},
      {"interface vtable 1", pes::kPInterfaceVtable1Offset, 8},
      {"action id", pes::kPByteAC8Offset, 1},
      {"sub id", pes::kPByteAC9Offset, 1},
      {"previous action id", pes::kPPrevActionOffset, 1},
      {"previous sub id", pes::kPPrevActionOffset + 1, 1},
      {"motion", pes::kPWordAE0Offset, 2},
      {"kick approach", pes::kPWord430Offset, 4},
      {"locomotion class", pes::kPByteB08Offset, 1},
      {"physique scale", pes::kPF3B18Offset, 4},
      {"rate ceiling", pes::kPF377CScaleOffset, 4},
  };
  Image image;
  ASSERT_TRUE(image.Open(exe_));
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    Built b;
    Build(o, image.bytes, image.size, b);
    const uint64_t p = o.regs.at("rdx"), g = Read(o, kImageG, 8);
    for (const DerivedField& f : fields) {
      uint8_t want[16];
      for (size_t i = 0; i < f.bytes; ++i) want[i] = static_cast<uint8_t>(Read(o, p + f.offset + i, 1));
      EXPECT_EQ(std::memcmp(b.world->P(b.slot) + f.offset, want, f.bytes), 0) << dir << ": " << f.name;
    }
    // The slot-record pointer is a window address: it names the actor's own slot record at the captured offset.
    const int slot = static_cast<int>(Read(o, p + pes::kPByte2458Offset, 1));
    uint64_t derived;
    std::memcpy(&derived, b.world->P(b.slot) + pes::kPSlotPtrOffset, sizeof derived);
    EXPECT_EQ(derived, pes::PesWorld::SlotVa(slot) + pes::kSlotPPtrTargetOffset) << dir;
    EXPECT_EQ(Read(o, p + pes::kPSlotPtrOffset, 8), CapturedBase(o, g, "slot", slot) + pes::kSlotPPtrTargetOffset) << dir;
  }
}

// P+0x377C (FUN_1406F99F0): the ceiling GF computes from the captured Speed byte, stamina and G equals PES's stored float bit
// for bit on every capture. Every capture's reserve is full (percent 100), so the reserve branches are not exercised here.
TEST_F(TrapOracle, RateCeilingEqualsPesBitForBit) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    const pes::PesActorFacts f = FactsOf(o);
    const float g = pes::PhysiqueG(g_derivation->physique, {f.heightCm, f.legSlider, false});
    const float got = pes::MoveTopRate(g_derivation->stamina, f.speedByte, f.stamina, g);
    uint32_t gotBits;
    std::memcpy(&gotBits, &got, sizeof gotBits);
    EXPECT_EQ(gotBits, static_cast<uint32_t>(Read(o, o.regs.at("rdx") + pes::kPF377CScaleOffset, 4))) << dir;
  }
}

// G, as the chain reads it (*(*(P+0xA08)+0xF0)+0xC0, FUN_141AF3180): the window's chain, derived from the actor's height by
// pes::PhysiqueG, holds the very float PES's chain held in the capture, on every capture. (The captures are slim: the player
// object is not in them, so the height is P+0x3B18 * 180; tests/pes_physique_test.cpp holds the port to PES's stored G on every
// player of the full captures, with the real block.)
TEST_F(TrapOracle, DerivedGEqualsPesChainGOnEveryCapture) {
  Image image;
  ASSERT_TRUE(image.Open(exe_));
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    Built b;
    Build(o, image.bytes, image.size, b);
    const uint64_t p = o.regs.at("rdx");
    const uint64_t capturedChain = Read(o, Read(o, p + pes::kPScalePtrOffset, 8) + pes::kPhysiqueNextOffset, 8);
    const uint32_t want = static_cast<uint32_t>(Read(o, capturedChain + pes::kPhysiqueGOffset, 4));
    uint32_t got;
    std::memcpy(&got, b.world->PhysiqueB(b.slot) + pes::kPhysiqueGOffset, sizeof got);
    EXPECT_EQ(got, want) << dir << ": G of " << FactsOf(o).heightCm << " cm";
  }
}

// P+0xB08's branches that no capture reaches (all six are in the table range, phase != 1; FUN_1406D8BF0, decompile only): the
// phase-0 test on the root speed, the phase bound, and the phase-1 test. The table branch is confirmed 6/6 above.
TEST_F(TrapOracle, LocomotionClassBranches) {
  const pes::PesActorConstants& c = g_derivation->actor;
  std::array<uint8_t, pes::kTrapPBytes> p{};
  pes::PesActorFacts f;
  const auto classOf = [&] { return pes::LocomotionClass(c, f, p.data()); };
  // Phase 0: class 3 above the standing speed, else 1. One metre a tick is far above it, standing still is below.
  f.phase = 0;
  EXPECT_EQ(classOf(), 1);
  pes::StoreF32(p.data(), pes::kPPosOffset, 1.0f);
  EXPECT_EQ(classOf(), 3);
  pes::StoreF32(p.data(), pes::kPPosOffset, 0.0f);
  pes::StoreF32(p.data(), pes::kPPosOffset + 8, 1.0f);  // the z displacement counts too
  EXPECT_EQ(classOf(), 3);
  // A phase at or past the table's bound is class 0.
  f.phase = static_cast<uint8_t>(pes::kLocomotionPhases);
  EXPECT_EQ(classOf(), 0);
  f.phase = 0xFF;
  EXPECT_EQ(classOf(), 0);
  // Phase 1 is class 4 only while word 0x2780 exceeds word 0x277C; otherwise its table entry.
  f.phase = 1;
  const uint8_t table1 = classOf();
  f.word2780 = 2.0f, f.word277C = 1.0f;
  EXPECT_EQ(classOf(), 4);
  f.word2780 = 1.0f;
  EXPECT_EQ(classOf(), table1);
  f.word2780 = 0.5f;
  EXPECT_EQ(classOf(), table1);
}

// The trap end handler 0x14079E5C0 returns 0 at once while the clip that plays is in the anime's motion table (FUN_1406EBEE0:
// true for a listed motion; the six live captures seq884..896 all take that exit). The window lists the body's playing clip
// (PesActorFacts::playing) and PES's own FUN_1406EBEE0, run on the window, must answer by the listing: true for the
// clip that plays, false for another motion, false again once the clip has finished and the entry is dropped.
TEST_F(TrapOracle, TheMotionTableListsThePlayingClipSoTheEndHandlerWaits) {
  auto world = std::make_unique<pes::PesWorld>();
  for (int k = 0; k < pes::kSlotCount; ++k) world->BindSlot(k);
  constexpr int kSlot = 7;
  constexpr uint16_t kPlaying = 0x0D18, kOther = 0x001F;
  const auto listed = [&](uint16_t motion) {
    ::PesState s;
    world->BindCall(s);
    s.gpr[1] = pes::PesWorld::PVa(kSlot);  // rcx: P
    s.gpr[2] = motion;                     // dx: the motion number
    ::pes_fn_1406EBEE0(s);
    return (s.gpr[0] & 0xFF) != 0;
  };
  pes::PesActorFacts f;
  f.slot = kSlot;
  pes::PesActorOpenInputs open;
  const pes::PesActorConstants& c = g_derivation->actor;
  pes::WriteActorP(*world, f, open, c, g_derivation->stamina, g_derivation->physique);
  EXPECT_FALSE(listed(kPlaying)) << "nothing plays: nothing is listed";
  f.playing[0].id = kPlaying;
  f.playing[0].seconds = 0.05f;
  f.playingCount = 1;
  pes::WriteActorP(*world, f, open, c, g_derivation->stamina, g_derivation->physique);
  EXPECT_TRUE(listed(kPlaying));
  EXPECT_FALSE(listed(kOther));
  f.playingCount = 0;
  pes::WriteActorP(*world, f, open, c, g_derivation->stamina, g_derivation->physique);
  EXPECT_FALSE(listed(kPlaying)) << "a finished clip is dropped from the table";
}

// FUN_141EB4650's table block: each entry carries the seconds played, the seconds it advanced since the last rebuild (the new
// seconds minus the old table's seconds for the same motion, else the member rate P+0x41C) and the layer's rate; a motion that
// left the list is gone, and an empty list clears the cells. (Entry layout CONFIRMED against seq884's table: pestrapworld.hpp.)
TEST_F(TrapOracle, TheMotionTableAdvancesEachEntryFromTheOldTable) {
  auto world = std::make_unique<pes::PesWorld>();
  for (int k = 0; k < pes::kSlotCount; ++k) world->BindSlot(k);
  constexpr int kSlot = 7;
  constexpr float kMemberRate = 0.0125f;  // a value no other cell holds
  uint8_t* p = world->P(kSlot);
  pes::StoreF32(p, pes::kPMemberOffset + pes::kMemberRateOffset, kMemberRate);
  pes::PesActorFacts f;
  f.slot = kSlot;
  pes::PesActorOpenInputs open;
  const pes::PesActorConstants& c = g_derivation->actor;
  const auto rebuild = [&] { pes::WriteActorP(*world, f, open, c, g_derivation->stamina, g_derivation->physique); };
  const auto entry = [&](std::size_t i) { return p + pes::kPMotionTableOffset + i * pes::kPMotionEntryBytes; };
  constexpr uint16_t kA = 0x0D18, kB = 0x001F;
  f.playing[0] = {kA, pes::kMotionEntryListed, 0.05f, 1.25f};
  f.playingCount = 1;
  rebuild();
  EXPECT_EQ(pes::LoadU32(p, pes::kPMotionCountOffset), 1u);
  EXPECT_EQ(pes::LoadU32(entry(0), 0), kA);
  EXPECT_EQ(entry(0)[pes::kMotionEntryFlagOffset], pes::kMotionEntryListed);
  EXPECT_EQ(pes::LoadF32(entry(0), pes::kMotionEntrySecondsOffset), 0.05f);
  EXPECT_EQ(pes::LoadF32(entry(0), pes::kMotionEntryAdvanceOffset), kMemberRate) << "a motion new to the table advances by the member rate";
  EXPECT_EQ(pes::LoadF32(entry(0), pes::kMotionEntryRateOffset), 1.25f);
  f.playing[0].seconds = 0.0725f;
  f.playing[1] = {kB, pes::kMotionEntryListed, 0.0565f, 1.0f};
  f.playingCount = 2;
  rebuild();
  EXPECT_EQ(pes::LoadU32(p, pes::kPMotionCountOffset), 2u);
  EXPECT_EQ(pes::LoadF32(entry(0), pes::kMotionEntryAdvanceOffset), 0.0725f - 0.05f) << "the same motion advances by the new seconds minus the old";
  EXPECT_EQ(pes::LoadF32(entry(1), pes::kMotionEntryAdvanceOffset), kMemberRate);
  f.playing[0] = f.playing[1];  // kA left the list: the table lists only kB, which keeps its old seconds
  f.playing[1] = {};
  f.playing[0].seconds = 0.0765f;
  f.playingCount = 1;
  rebuild();
  EXPECT_EQ(pes::LoadU32(p, pes::kPMotionCountOffset), 1u);
  EXPECT_EQ(pes::LoadU32(entry(0), 0), kB);
  EXPECT_EQ(pes::LoadF32(entry(0), pes::kMotionEntryAdvanceOffset), 0.0765f - 0.0565f);
  EXPECT_EQ(pes::LoadU32(entry(1), 0), 0u) << "the cleared entries stay zero";
  f.playingCount = 0;
  rebuild();
  EXPECT_EQ(pes::LoadU32(p, pes::kPMotionCountOffset), 0u);
  EXPECT_EQ(pes::LoadU32(entry(0), 0), 0u);
}

// FUN_1406D9010(table, id): slot[id] when id < 0x5E and the slot is set, else slot[1]. P+0xC40 is built once per man; the switch calls the
// exit and enter methods of the objects it returns, so each must be a window object of this man with a vtable.
TEST_F(TrapOracle, TheActionTableResolvesEveryIdToThisMansOwnObject) {
  auto world = std::make_unique<pes::PesWorld>();
  for (int k = 0; k < pes::kSlotCount; ++k) world->BindSlot(k);
  constexpr int kSlot = 7, kOther = 8;
  pes::ConstructActorP(*world, kSlot, g_derivation->actor);
  pes::ConstructActorP(*world, kOther, g_derivation->actor);
  const auto resolve = [&](int slot, std::size_t id) {
    const uint8_t* p = world->P(slot);
    const auto at = [&](std::size_t i) { return pes::LoadU64(p, pes::kPActionTableOffset + pes::kActionTablePointersOffset + 8 * i); };
    return id < pes::kActionTableSlots && at(id) != 0 ? at(id) : at(1);
  };
  const uint64_t base = pes::PesWorld::PVa(kSlot) + pes::kPActionTableOffset;
  for (std::size_t id = 0; id < pes::kActionTableSlots; ++id) {
    const uint64_t object = resolve(kSlot, id);
    ASSERT_GE(object, base + 8) << "id " << id;
    ASSERT_LT(object, base + pes::kActionTablePointersOffset) << "id " << id << ": an action object lies before the pointer array";
    EXPECT_GE(pes::LoadU64(world->P(kSlot), object - pes::PesWorld::PVa(kSlot)), pes::kImageBaseVa) << "id " << id << ": no vtable";
  }
  EXPECT_EQ(resolve(kSlot, pes::kAnimeActionTrap), pes::PesWorld::PVa(kSlot) + pes::kPActionOffset) << "the chain's action object is the table's Trap";
  EXPECT_NE(resolve(kSlot, pes::kAnimeActionMove), resolve(kSlot, pes::kAnimeActionTrap));
  EXPECT_EQ(resolve(kSlot, pes::kActionTableSlots), resolve(kSlot, 1)) << "an id past the table falls back to slot 1";
  EXPECT_EQ(resolve(kOther, pes::kAnimeActionMove) - pes::PesWorld::PVa(kOther), resolve(kSlot, pes::kAnimeActionMove) - pes::PesWorld::PVa(kSlot))
      << "each man's table names its own P";
}

// Every fact lands in its own field of P. The captures have action id == sub id (and the previous pair equal too), so only
// distinct values tell those bytes apart; position, previous position and heading come from the man record's own fields.
TEST_F(TrapOracle, WriteActorPStoresEachFactInItsOwnField) {
  auto world = std::make_unique<pes::PesWorld>();
  for (int k = 0; k < pes::kSlotCount; ++k) world->BindSlot(k);
  pes::PesActorFacts f;
  f.slot = 7, f.phase = 5, f.action = 3, f.sub = 5, f.prevAction = 2, f.prevSub = 4;
  f.motion = 0x0B86, f.kickApproach = 2, f.heightCm = 190.0f, f.speedByte = 77.0f;
  uint8_t* man = world->ManRecord(f.slot);
  const float pos[3] = {1.0f, 2.0f, 3.0f}, prev[3] = {4.0f, 5.0f, 6.0f};
  for (int i = 0; i < 3; ++i) {
    pes::StoreF32(man, pes::kManPosXOffset + 4 * i, pos[i]);
    pes::StoreF32(man, pes::kManPrevPosXOffset + 4 * i, prev[i]);
  }
  pes::StoreF32(man, pes::kManFacingOffset, 90.0f);
  pes::PesActorOpenInputs open;
  for (size_t i = 0; i < open.velocityRing.size(); ++i) open.velocityRing[i] = static_cast<uint8_t>(0x10 + i);
  f.side = 1;
  const pes::PesActorConstants& c = g_derivation->actor;
  pes::WriteActorP(*world, f, open, c, g_derivation->stamina, g_derivation->physique);
  const uint8_t* p = world->P(f.slot);
  const auto u64 = [p](size_t off) {
    uint64_t v;
    std::memcpy(&v, p + off, sizeof v);
    return v;
  };
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(pes::LoadF32(p, pes::kPPosOffset + 4 * i), pos[i]) << i;
    EXPECT_EQ(pes::LoadF32(p, pes::kPNextPosOffset + 4 * i), prev[i]) << i;
  }
  EXPECT_EQ(pes::LoadF32(p, pes::kPHeadingOffset), 90.0f);
  EXPECT_EQ(p[pes::kPByte2458Offset], 7);
  EXPECT_EQ(p[pes::kPPhaseOffset], 5);
  EXPECT_EQ(p[pes::kPByteAC8Offset], 3);
  EXPECT_EQ(p[pes::kPByteAC9Offset], 5);
  EXPECT_EQ(p[pes::kPPrevActionOffset], 2);
  EXPECT_EQ(p[pes::kPPrevActionOffset + 1], 4);
  EXPECT_EQ(p[pes::kPWordAE0Offset] | p[pes::kPWordAE0Offset + 1] << 8, 0x0B86);
  EXPECT_EQ(pes::LoadU32(p, pes::kPWord430Offset), 2u);
  EXPECT_EQ(pes::LoadF32(p, pes::kPF3B18Offset), 190.0f / c.heightDivisor);
  EXPECT_EQ(u64(pes::kPInterfaceVtable0Offset), c.interfaceVtable[0]);
  EXPECT_EQ(u64(pes::kPInterfaceVtable1Offset), c.interfaceVtable[1]);
  EXPECT_EQ(u64(pes::kPSlotPtrOffset), pes::PesWorld::SlotVa(7) + pes::kSlotPPtrTargetOffset);
  // The physique chain is derived: P+0xA08 -> A, A+0xF0 -> B, B+0xC0 = G of the facts (190 cm, neutral slider); P+0xA00 holds it too.
  const float g = pes::PhysiqueG(g_derivation->physique, {190.0f, 0.0f, false});
  EXPECT_EQ(u64(pes::kPScalePtrOffset), pes::PesWorld::PhysiqueAVa(f.slot));
  uint64_t next;
  std::memcpy(&next, world->PhysiqueA(f.slot) + pes::kPhysiqueNextOffset, sizeof next);
  EXPECT_EQ(next, pes::PesWorld::PhysiqueBVa(f.slot));
  EXPECT_EQ(pes::LoadF32(world->PhysiqueB(f.slot), pes::kPhysiqueGOffset), g);
  EXPECT_EQ(pes::LoadF32(p, pes::kPGOffset), g);
  EXPECT_EQ(u64(pes::kPPtr3AC0Offset), pes::PesWorld::CtxVa(1));  // P+0x3AC0 names the side the fact gives (1)
  EXPECT_EQ(std::memcmp(p + pes::kPVelocityOffset, open.velocityRing.data(), open.velocityRing.size()), 0);
}

// A flipped byte can send PES's own code to one of its aborts (a trap in a validating helper): that is a change of the call's
// path, but it would end the test process. The call runs in a child; one that dies reports as a path of its own.
RunOut RunForked(const Oracle& o, const std::string& exe, const std::function<void(Built&)>& mutate) {
  int fds[2];
  EXPECT_EQ(pipe(fds), 0);
  const pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    const RunOut r = RunCall(o, exe, mutate);
    const uint32_t head[3] = {r.result.issued ? 1u : 0u, r.result.out, static_cast<uint32_t>(r.writes.size())};
    if (write(fds[1], head, sizeof head) < 0 || write(fds[1], r.writes.data(), r.writes.size()) < 0) _exit(2);
    _exit(0);
  }
  close(fds[1]);
  RunOut out;
  uint32_t head[3];
  if (read(fds[0], head, sizeof head) == static_cast<ssize_t>(sizeof head)) {
    out.result.issued = head[0] != 0;
    out.result.out = head[1];
    out.writes.resize(head[2]);
    for (size_t got = 0; got < head[2];) {
      const ssize_t n = read(fds[0], &out.writes[got], head[2] - got);
      if (n <= 0) break;
      got += static_cast<size_t>(n);
    }
  } else {
    out.writes = "the call aborted";
  }
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  return out;
}

// The record indices the call reads (first and last of a kind in the manifest): the actor's slot and its man records.
struct Indices {
  int slot = 0, manFirst = 0, manLast = 0;
};
Indices IndicesOf(const Oracle& o) {
  Indices idx;
  bool seenMan = false;
  for (const auto& line : o.manifest) {
    if (line[0] != "T") continue;
    const int i = std::atoi(line[2].c_str());
    if (line[1] == "slot") idx.slot = i;
    if (line[1] == "man") {
      if (!seenMan) idx.manFirst = i;
      idx.manLast = i;
      seenMan = true;
    }
  }
  return idx;
}

// Every binding of the call's inputs the window holds is one byte of its records; flipping it must change the call's path (the
// result or the bytes the chain writes) in at least one capture, or the binding is not read and is not claimed. Not claimed
// (no remaining capture's call changes with them): P+0xB08, P+0x377C, the play state MI+0x116C, the pitch F0, the anime class
// byte, ball+0x4, man B's facing and +0x5B0, and the holder MA+0x2601C (read in 20 of 24 captures, but flipping its low bit changes
// no capture's path); the removed captures (seq4310, 4329, 4351, 4402) read some of them.
TEST_F(TrapOracle, EachWindowBindingChangesTheCall) {
  struct Binding {
    const char* name;
    std::function<uint8_t*(Built&, const Indices&)> byteOf;
    uint8_t flip = 0x01;  // the lowest bit by default: a larger flip can turn a byte into one PES's own code rejects
  };
  const std::vector<Binding> bindings = {
      {"P position x", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPPosOffset; }},
      {"P next position z", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPNextPosOffset + 8; }},
      {"P heading", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPHeadingOffset; }},
      {"P velocity x", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPVelocityOffset; }},
      {"P byte 0xAC9", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPByteAC9Offset; }},
      {"P word 0xAE0", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPWordAE0Offset; }},
      // (P+0x2458 is a state byte the chain validates: any value but the captured one reaches a trap in 0x140A58550 (a PES
      // abort), so it cannot be flipped and is not claimed here.)
      {"P scale 0x3B18", [](Built& b, const Indices&) { return b.world->P(b.slot) + pes::kPF3B18Offset; }},
      {"ball sample 0 x", [](Built& b, const Indices&) { return b.world->BallRecord() + pes::kBallSamplesOffset; }},
      {"ball sample 3 height",
       [](Built& b, const Indices&) { return b.world->BallRecord() + pes::kBallSamplesOffset + 3 * pes::kBallSampleStride + 4; }},
      {"ball velocity x", [](Built& b, const Indices&) { return b.world->BallRecord() + pes::kBallVelocityOffset + 2; }, 0x40},
      {"MI frame", [](Built& b, const Indices&) { return b.world->MatchInfo() + pes::kMiFrameOffset; }},
      {"man A position x", [](Built& b, const Indices& i) { return b.world->ManRecord(i.manFirst) + pes::kManPosXOffset; }},
      {"man A velocity x", [](Built& b, const Indices& i) { return b.world->ManRecord(i.manFirst) + pes::kManVelXOffset; }},
      {"slot +0x9B", [](Built& b, const Indices& i) { return b.world->Block(i.slot) + pes::kSlotRecordOffset + 0x9B; }},
      {"slot +0xC8", [](Built& b, const Indices& i) { return b.world->Block(i.slot) + pes::kSlotRecordOffset + 0xC8; }},
      {"slot +0x104", [](Built& b, const Indices& i) { return b.world->Block(i.slot) + pes::kSlotRecordOffset + 0x104; }},
      {"E newest event kind",
       [](Built& b, const Indices&) { return reinterpret_cast<uint8_t*>(&b.world->EventImage()) + pes::kEventNewestOffset; }},
      {"E newest event side",
       [](Built& b, const Indices&) { return reinterpret_cast<uint8_t*>(&b.world->EventImage()) + pes::kEventNewestOffset + 4; }},
      {"E newest event slot",
       [](Built& b, const Indices&) { return reinterpret_cast<uint8_t*>(&b.world->EventImage()) + pes::kEventNewestOffset + 8; }},
      {"E newest event tick",
       [](Built& b, const Indices&) { return reinterpret_cast<uint8_t*>(&b.world->EventImage()) + pes::kEventNewestOffset + 0x10; }},
  };
  std::map<std::string, int> changed;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    const RunOut base = RunCall(o, exe_);
    const Indices idx = IndicesOf(o);
    for (const Binding& binding : bindings) {
      const RunOut flipped = RunForked(o, exe_, [&](Built& b) { *binding.byteOf(b, idx) ^= binding.flip; });
      if (!(flipped == base)) ++changed[binding.name];
    }
  }
  for (const Binding& binding : bindings) EXPECT_GT(changed[binding.name], 0) << binding.name << ": no capture's call reads it";
}

}  // namespace

// The match runs the chain on its own memory: a private copy of the install's image slice (not the flat image) and of the loader
// data (PesTrapMemory, built once). On every capture that memory gives the flat image's output, and the two issued captures
// commit PES's clip: seq4469 0xB86 (Animetable cell of row 0x520), seq23906 0xA41 (rows 0x7E1..0x7FA).
TEST_F(TrapOracle, TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures) {
  const std::map<std::string, uint16_t> committed = {{"seq4469", 0xB86}, {"seq23906", 0xA41}};
  int checked = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    const RunOut flat = RunCall(o, exe_);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    Built b;
    Build(o, image.bytes, image.size, b);
    pes::PesTrapMemory memory(*g_source);
    pes::PesTrapExternals& ext = memory.Externals();
    for (const pes::PesTrapSegment& seg : b.ext.segments)  // the captured table-B squad records (SBn), not the loader
      if (seg.va != g_source->loader.segment.va) ext.segments.push_back(seg);
    const pes::PesTrapResult r = b.world->RunTrapKind0(b.slot, b.request, ext);
    EXPECT_EQ(r.issued, flat.result.issued) << dir;
    EXPECT_EQ(r.out, flat.result.out) << dir;
    if (!r.issued) continue;
    const uint16_t clip = pes::CommittedTrapClip(b.world->P(b.slot));
    for (const auto& [name, want] : committed)
      if (dir.find(name) != std::string::npos) {
        EXPECT_EQ(clip, want) << dir;
        ++checked;
      }
  }
  EXPECT_EQ(checked, static_cast<int>(committed.size())) << "both issued captures must be in the oracle set";
}

// ---- The Trap action tick 0x140797CD0 (docs/ports/pes-port-TrapTick.md) --------------------------------------------------------
//
// GF_PES_TRAP_TICK_ORACLE names a directory of oracle directories of the 14 captured ticks (tools/dev/translate/trap_oracle.py over
// snapdiff's 0x140797CD0 case dirs). Each capture's window is built as the kind-0 oracle's is (records, the man, P by
// pes::WriteActorP, which now also writes the anime layer's pointer structure and the root point), then the bytes the tick reads
// that GF holds no state for are the NAMED open inputs, overlaid from the capture only where GF wrote nothing:
//   P: the tick's own persisted state and the other PES writers' (the action object P+0xC78.., +0x2471/2472/2460, the modes
//      +0x2704.., the adjusters +0x41C/428/498/4D0/4D8, +0x354, +0x55C/5B0, +0xA04, +0xAD0, +0x2814.., +0x32E0, +0x3758);
//   the layer objects A, O1, O2, Q, R, S (pestrapworld.hpp): their scalars;
//   the caller frame F (+0x5C3/5C4/5E4/740..) and F+0x868's object (+0x5E4), and the caller's stack locals (S lines).
// Where GF did write a byte and the capture differs, that is a derivation error and fails (pointer cells aside: the window's
// pointers name window objects). The loader data (motion records of five Mbinfo kinds, the Animetable, trap.o, the EyesTarget
// table) is built from the install, as for the chain.
namespace {

enum Obj : int { kObjP, kObjA, kObjO1, kObjO2, kObjQ, kObjR, kObjS, kObjFrame, kObjFrameX, kObjBall, kObjCount };
struct ObjSpan {
  uint64_t base = 0, size = 0;
};
using ObjSpans = std::array<ObjSpan, kObjCount>;
using WriteMap = std::map<std::pair<int, uint32_t>, uint8_t>;

ObjSpans CapturedObjects(const Oracle& o) {
  const uint64_t p = o.regs.at("rdx"), a = Read(o, p + pes::kPScalePtrOffset, 8), q = Read(o, p + pes::kPQPtrOffset, 8),
                 f = Read(o, o.regs.at("r8"), 8);
  ObjSpans s;
  s[kObjP] = {p, pes::kTrapPBytes};
  s[kObjA] = {a, pes::kPhysiqueObjectBytes};
  s[kObjO1] = {Read(o, a + pes::kAChildO1Offset, 8), pes::kO1Bytes};
  s[kObjO2] = {Read(o, a + pes::kAChildO2Offset, 8), pes::kO2Bytes};
  s[kObjQ] = {q, pes::kQBytes};
  s[kObjR] = {Read(o, q + pes::kQChainPtrOffset, 8), pes::kRBytes};
  s[kObjS] = {Read(o, p + pes::kPSPtrOffset, 8), pes::kSBytes};
  s[kObjFrame] = {f, pes::kTickFrameBytes};
  s[kObjFrameX] = {Read(o, f + pes::kFrameXPtrOffset, 8), pes::kTickFrameXBytes};
  s[kObjBall] = {Read(o, f + pes::kFrameBallOffset, 8), pes::kBallRecordBytes};  // the prediction samples the tick indexes (FUN_140A6CF70)
  return s;
}

ObjSpans WindowObjects(int slot) {
  ObjSpans s;
  s[kObjP] = {pes::PesWorld::PVa(slot), pes::kTrapPBytes};
  s[kObjA] = {pes::PesWorld::PhysiqueAVa(slot), pes::kPhysiqueObjectBytes};
  s[kObjO1] = {pes::PesWorld::O1Va(slot), pes::kO1Bytes};
  s[kObjO2] = {pes::PesWorld::O2Va(slot), pes::kO2Bytes};
  s[kObjQ] = {pes::PesWorld::QVa(slot), pes::kQBytes};
  s[kObjR] = {pes::PesWorld::RVa(slot), pes::kRBytes};
  s[kObjS] = {pes::PesWorld::SVa(slot), pes::kSBytes};
  s[kObjFrame] = {pes::PesWorld::TickFrameVa(), pes::kTickFrameBytes};
  s[kObjFrameX] = {pes::PesWorld::TickFrameXVa(), pes::kTickFrameXBytes};
  s[kObjBall] = {pes::PesWorld::BallVa(), pes::kBallRecordBytes};
  return s;
}

uint8_t* WindowBytes(pes::PesWorld& w, int slot, int obj) {
  switch (obj) {
    case kObjP: return w.P(slot);
    case kObjA: return w.PhysiqueA(slot);
    case kObjO1: return w.O1(slot);
    case kObjO2: return w.O2(slot);
    case kObjQ: return w.Q(slot);
    case kObjR: return w.R(slot);
    case kObjS: return w.S(slot);
    case kObjFrame: return w.TickFrame();
    case kObjBall: return w.BallRecord();
    default: return w.TickFrameX();
  }
}

bool Classify(const ObjSpans& spans, uint64_t addr, int* obj, uint32_t* rel) {
  for (int k = 0; k < kObjCount; ++k)
    if (spans[k].base != 0 && addr >= spans[k].base && addr < spans[k].base + spans[k].size) {
      *obj = k;
      *rel = static_cast<uint32_t>(addr - spans[k].base);
      return true;
    }
  return false;
}

// The window's pointer cells (they name window objects, so they differ from the capture's heap addresses by construction).
bool IsPointerCell(int obj, uint32_t rel) {
  const auto in = [rel](std::size_t cell, std::size_t n = 8) { return rel >= cell && rel < cell + n; };
  switch (obj) {
    case kObjP:
      return in(pes::kPScalePtrOffset) || in(pes::kPQPtrOffset) || in(pes::kPSPtrOffset) || in(pes::kPPtr3AC0Offset) ||
             in(pes::kPSlotPtrOffset) || pes::IsActionTablePointer(rel);
    case kObjA: return in(pes::kPhysiqueNextOffset) || in(pes::kAChildO1Offset) || in(pes::kAChildO2Offset);
    case kObjQ: return in(pes::kQChainPtrOffset);
    case kObjR: return in(pes::kRArrayPtrOffset) || in(pes::kRArrayOffset, 88);  // the joint pointer array
    case kObjFrame: return in(pes::kFrameXPtrOffset) || in(pes::kFrameBallOffset);
    default: return false;
  }
}

// Every address the capture's tick wrote that lies in one of the objects, with its last byte: the reference row's write log.
WriteMap ReferenceWrites(const Oracle& o, const ObjSpans& spans) {
  WriteMap out;
  const std::string w = Field(o.row, "w");
  for (size_t at = 0; at + 18 <= w.size(); at += 18) {
    int obj;
    uint32_t rel;
    if (Classify(spans, std::strtoull(w.substr(at, 16).c_str(), nullptr, 16), &obj, &rel))
      out[{obj, rel}] = static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16));
  }
  return out;
}

struct TickBuilt {
  Built built;
  std::vector<std::string> derivedMismatches;  // bytes GF wrote that differ from the capture's (outside pointer cells)
  int overlaid = 0;                            // bytes taken from the capture
  std::set<std::pair<int, uint32_t>> overlaidAt;  // (object, offset) of each of them
};

// The tick's caller stack: the window stack's entry rsp (RunTrapTick), where the S lines' bytes sit.
constexpr uint64_t kTickEntry = pes::kStackVa + pes::kStackBytes - pes::kTrapFrameBytes;

void BuildTick(const Oracle& o, const uint8_t* image, uint64_t imageBytes, TickBuilt& t, bool overlay = true) {
  Build(o, image, imageBytes, t.built);
  if (!overlay) return;
  pes::PesWorld& w = *t.built.world;
  const int slot = t.built.slot;
  const ObjSpans captured = CapturedObjects(o);
  const auto put = [&](int obj, uint32_t rel, uint64_t capAddr, size_t len) {
    uint8_t* dst = WindowBytes(w, slot, obj) + rel;
    for (size_t i = 0; i < len; ++i) {
      const uint8_t cap = static_cast<uint8_t>(Read(o, capAddr + i, 1));
      if (IsPointerCell(obj, rel + static_cast<uint32_t>(i))) continue;  // names a window object: its zero bytes are not gaps to fill
      const auto cell = obj == kObjO1 ? pes::AnimPluginCellAt(rel + i) : pes::AnimPluginCell::kNotStored;
      if (cell == pes::AnimPluginCell::kSelfPointer) continue;  // likewise
      // A layer's weights and blend state: the constructor's value is the start state, PES's animation engine (which GF does not run)
      // has moved it by the time of the tick. OPEN input, taken from the capture (the animation engine's writers are not decoded).
      // An action object's cells: the constructor's value is the start state; the action has run since (the chain commits and the
      // tick write it), so the capture's cell is the tick's input.
      const bool actionState = obj == kObjP && rel + i >= pes::kPActionTableOffset && rel + i < pes::kPActionTableOffset + pes::kActionTableBytes;
      if (dst[i] == 0 || cell == pes::AnimPluginCell::kState || actionState) {
        if (cap != 0 && dst[i] != cap) {
          ++t.overlaid;
          t.overlaidAt.insert({obj, rel + static_cast<uint32_t>(i)});
        }
        dst[i] = cap;
      } else if (dst[i] != cap) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "obj %d +0x%x: GF %02x, capture %02x", obj, rel + static_cast<unsigned>(i), dst[i], cap);
        t.derivedMismatches.push_back(buf);
      }
    }
  };
  const uint64_t rdx = o.regs.at("rdx");
  // The ball record's prediction samples the tick indexes (FUN_140A6CF70; GF holds them as PesWorld::SetBallSample): the manifest's H
  // lines miss them (the replay serves them from the page set), so they are supplied whole, 250 samples.
  put(kObjBall, pes::kBallSamplesOffset, captured[kObjBall].base + pes::kBallSamplesOffset,
      static_cast<size_t>(pes::kBallSampleCount) * pes::kBallSampleStride);
  for (const auto& line : o.manifest) {
    if (line[0] == "T" && line[1] == "P") {
      put(kObjP, static_cast<uint32_t>(std::strtoul(line[3].c_str(), nullptr, 16)), rdx + std::strtoul(line[3].c_str(), nullptr, 16),
          std::strtoul(line[4].c_str(), nullptr, 10));
    } else if (line[0] == "H") {
      const uint64_t va = std::strtoull(line[1].c_str(), nullptr, 16);
      const size_t len = std::strtoul(line[2].c_str(), nullptr, 10);
      int obj;
      uint32_t rel;
      if (Classify(captured, va, &obj, &rel)) {
        if (obj != kObjP) put(obj, rel, va, len);
      } else if (std::getenv("GF_TICK_ORACLE_VERBOSE") != nullptr) {
        std::printf("  unsupplied read: %llx +%zu\n", static_cast<unsigned long long>(va), len);
      }
    } else if (line[0] == "S") {
      const uint32_t rel = static_cast<uint32_t>(std::strtoul(line[1].c_str(), nullptr, 16));
      if (rel == 0 || rel == pes::kTickEntryFrameSlotOffset) continue;  // the return address and the frame slot: the call's own
      const size_t len = std::strtoul(line[2].c_str(), nullptr, 10);
      uint8_t* dst = w.Bytes() + (kTickEntry - pes::kWorldWindowVa) + rel;
      for (size_t i = 0; i < len; ++i) dst[i] = static_cast<uint8_t>(Read(o, o.regs.at("rsp") + rel + i, 1));
    }
  }
}

struct TickRun {
  pes::PesTickResult result;
  WriteMap writes;
  std::vector<std::pair<std::pair<int, uint32_t>, uint8_t>> ordered;  // the same writes in the order the tick made them
  std::vector<std::pair<uint64_t, uint8_t>> raw;                      // every byte the tick wrote, by VA (debugging)
};

TickRun RunTick(TickBuilt& t) {
  std::vector<std::pair<uint64_t, uint8_t>> log;
  t.built.ext.writeLog = &log;
  TickRun run;
  run.result = t.built.world->RunTrapTick(t.built.slot, t.built.ext);
  const ObjSpans spans = WindowObjects(t.built.slot);
  for (const auto& wr : log) {
    int obj;
    uint32_t rel;
    if (Classify(spans, wr.first, &obj, &rel)) {
      run.writes[{obj, rel}] = wr.second;
      run.ordered.push_back({{obj, rel}, wr.second});
    }
  }
  run.raw = std::move(log);
  return run;
}

class TrapTickOracle : public TrapOracle {
 protected:
  const char* OracleEnv() const override { return "GF_PES_TRAP_TICK_ORACLE"; }
};

// What PES's tick left in P at `off` (n bytes): its last write there, else the byte the call started with.
uint64_t PesPAfter(const Oracle& o, const WriteMap& writes, uint32_t off, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) {
    const auto at = writes.find({kObjP, off + i});
    const uint64_t b = at != writes.end() ? at->second : Read(o, o.regs.at("rdx") + off + i, 1);
    v |= b << (8 * i);
  }
  return v;
}

// GF's window run of each captured tick writes PES's bytes into P and the layer objects, and leaves PES's motion, mirror, segment
// and phase: on all 14 captures (phases 0, 1 and 3: the only ones captured; every other path aborts loudly in the dispatch).
TEST_F(TrapTickOracle, EveryCapturedTickOnGfsWindowEqualsPes) {
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    TickBuilt t;
    BuildTick(o, image.bytes, image.size, t);
    for (const std::string& m : t.derivedMismatches) ADD_FAILURE() << dir << ": derived " << m;
    if (const char* only = std::getenv("GF_TICK_ORACLE_TRAP"); only != nullptr && dir.find(only) != std::string::npos) {
      std::printf("A+0x48 host %p\n", static_cast<void*>(WindowBytes(*t.built.world, t.built.slot, kObjA) + 0x48));
      std::printf("A VA %llx  stack VA %llx..%llx  entry %llx  P VA %llx\n",
                  static_cast<unsigned long long>(pes::PesWorld::PhysiqueAVa(t.built.slot)),
                  static_cast<unsigned long long>(pes::kStackVa), static_cast<unsigned long long>(pes::kStackVa + pes::kStackBytes),
                  static_cast<unsigned long long>(kTickEntry), static_cast<unsigned long long>(pes::PesWorld::PVa(t.built.slot)));
      std::fflush(stdout);
      raise(SIGTRAP);
    }
    const TickRun run = RunTick(t);
    const WriteMap want = ReferenceWrites(o, CapturedObjects(o));
    // The addresses either side wrote and the two disagree on, as ranges per object (P=0 A=1 O1=2 O2=3 Q=4 R=5 S=6):
    // "obj+0xLO..0xHI(n bytes)".
    std::set<std::pair<int, uint32_t>> bad;
    for (const auto& [key, byte] : want) {
      const auto at = run.writes.find(key);
      if (at == run.writes.end() || at->second != byte) bad.insert(key);
    }
    for (const auto& kv : run.writes)
      if (want.find(kv.first) == want.end()) bad.insert(kv.first);
    std::string diff;
    if (std::getenv("GF_TICK_ORACLE_VERBOSE") != nullptr) {
      std::vector<std::pair<std::pair<int, uint32_t>, uint8_t>> pesOrdered;
      const std::string w = Field(o.row, "w");
      const ObjSpans caps = CapturedObjects(o);
      for (size_t at = 0; at + 18 <= w.size(); at += 18) {
        int obj;
        uint32_t rel;
        if (Classify(caps, std::strtoull(w.substr(at, 16).c_str(), nullptr, 16), &obj, &rel))
          pesOrdered.push_back({{obj, rel}, static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16))});
      }
      size_t i = 0;
      while (i < pesOrdered.size() && i < run.ordered.size() && pesOrdered[i] == run.ordered[i]) ++i;
      std::printf("%s: first ordered divergence at write %zu (PES %zu, GF %zu)\n", dir.c_str(), i, pesOrdered.size(), run.ordered.size());
      {
        const ObjSpans gfSpans = WindowObjects(t.built.slot);
        size_t j = 0;
        for (; j < run.raw.size(); ++j) {
          int ob;
          uint32_t rl;
          if (Classify(gfSpans, run.raw[j].first, &ob, &rl) && ob == kObjA) break;
        }
        std::printf("  raw GF write log: first A write at raw index %zu of %zu; preceding 24 (VA:byte):", j, run.raw.size());
        for (size_t k = j > 24 ? j - 24 : 0; k <= j && k < run.raw.size(); ++k)
          std::printf(" %llx:%02x", static_cast<unsigned long long>(run.raw[k].first), run.raw[k].second);
        std::printf("\n");
      }
      for (size_t k = i > 48 ? i - 48 : 0; k < i + 8; ++k) {
        const auto show = [k](const auto& v, char* out) {
          if (k < v.size()) std::snprintf(out, 40, "%d+0x%x=%02x", v[k].first.first, v[k].first.second, v[k].second);
          else std::snprintf(out, 40, "--");
        };
        char a[40], b[40];
        show(pesOrdered, a);
        show(run.ordered, b);
        std::printf("   [%zu] PES %s | GF %s\n", k, a, b);
      }
    }
    if (std::getenv("GF_TICK_ORACLE_VERBOSE") != nullptr)
      for (const auto& key : bad) {
        const auto w = want.find(key), g = run.writes.find(key);
        std::printf("%s [%d+0x%x] PES %s GF %s\n", dir.c_str(), key.first, key.second,
                    w == want.end() ? "--" : (std::to_string(w->second)).c_str(), g == run.writes.end() ? "--" : (std::to_string(g->second)).c_str());
      }
    for (auto it = bad.begin(); it != bad.end();) {
      auto last = it;
      for (auto next = std::next(it); next != bad.end() && next->first == last->first && next->second == last->second + 1; ++next) last = next;
      char buf[96];
      std::snprintf(buf, sizeof buf, " [%d+0x%x..0x%x]", it->first, it->second, last->second);
      diff += buf;
      it = std::next(last);
    }
    EXPECT_TRUE(diff.empty()) << dir << ": " << run.writes.size() << " GF writes, " << want.size() << " PES writes; differing ranges:" << diff;
    // The outputs the body takes, where the tick wrote them (a field it left alone is not an output).
    const auto wrote = [&want](std::size_t off) { return want.count({kObjP, static_cast<uint32_t>(off)}) != 0; };
    if (wrote(pes::kPWordAE0Offset)) EXPECT_EQ(run.result.motion, PesPAfter(o, want, pes::kPWordAE0Offset, 2)) << dir;
    if (wrote(pes::kPMirrorOffset)) EXPECT_EQ(run.result.mirror, PesPAfter(o, want, pes::kPMirrorOffset, 1)) << dir;
    if (wrote(pes::kPSegmentOffset)) EXPECT_EQ(run.result.segment, PesPAfter(o, want, pes::kPSegmentOffset, 1)) << dir;
    if (wrote(pes::kPPhaseOffset)) EXPECT_EQ(run.result.phase, PesPAfter(o, want, pes::kPPhaseOffset, 1)) << dir;
  }
}

// The capture set reaches the start (phase 0, which commits the clip and its mirror byte) and the later phases; the mirrored and
// the unmirrored clip are both in it (seq4824 plays unmirrored, the others mirrored: pes-port-TrapS5.md 3).
TEST_F(TrapTickOracle, TheCapturesCoverTheStartAndBothMirrors) {
  int starts = 0, mirrored = 0, plain = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    TickBuilt t;
    BuildTick(o, image.bytes, image.size, t);
    const TickRun run = RunTick(t);
    if (run.writes.count({kObjP, static_cast<uint32_t>(pes::kPWordAE0Offset)}) != 0) ++starts;  // SetMotion ran: a start
    (run.result.mirror != 0 ? mirrored : plain)++;
  }
  EXPECT_EQ(starts, 2);
  EXPECT_GT(mirrored, 0);
  EXPECT_GT(plain, 0);
}

// Diagnostic: the window bytes the oracle takes from the capture because GF wrote none (the union over the captures), as ranges per
// object. Every range here is an input a GF match does not supply.
TEST_F(TrapTickOracle, ListWhatTheOverlaySupplies) {
  std::set<std::pair<int, uint32_t>> all;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    Image image;
    ASSERT_TRUE(image.Open(exe_));
    TickBuilt t;
    BuildTick(o, image.bytes, image.size, t);
    all.insert(t.overlaidAt.begin(), t.overlaidAt.end());
  }
  std::string out;
  for (auto it = all.begin(); it != all.end();) {
    auto last = it;
    for (auto next = std::next(it); next != all.end() && next->first == last->first && next->second <= last->second + 1; ++next) last = next;
    char buf[64];
    std::snprintf(buf, sizeof buf, " [%d+0x%x..0x%x]", it->first, it->second, last->second);
    out += buf;
    it = std::next(last);
  }
  std::printf("overlay-supplied ranges (P=0 A=1 O1=2 O2=3 Q=4 R=5 S=6):%s\n", out.c_str());
}

// Every byte of loader data the tick read (the manifest's H lines in the Mbinfo motion records, the lists they point to, the
// EyesTarget table and the Animetable buffer) equals what GF's loader built from the install: the loader is the input the window
// cannot overlay (it holds no copy of the capture's heap), so a record GF built differently changes the solver's output unseen.
// A read byte that lies in none of these structures, nor in the window's objects, is reported as unexplained.
// KNOWN RED (31-10-26, settled): the 14 oracle dirs are captures of launches that predate the "No Mod" pin (30-10-26 14:35); sider's
// GameplayLoader.lua had selected a gameplay mod for their match, and its constant_player.bin carries a different trap.o (+0x8 = 0x0a;
// the VFS rule that mounts the loader's answer landed with VfsOrder, and a default mount is "No Mod"). Captures of the pinned launch
// (live2/pid3061186 seq2016, seq2028, seq2035; live2/pid3439759 seq3244) hold +0x8 = 0x07 = what GF's default mount reads: stock, not a
// GF defect. The oracle is re-taken from FULL No-Mod trap_action_tick captures (a slim capture lacks the actor-fact pages
// trap_oracle.py reads); the first full one (seq3244) also shows the loader lacks Mbinfo kind 4 (motion 0xda9 count +0x4: PES 1, GF 0).
TEST_F(TrapTickOracle, LoaderDataTheTickReadsEqualsTheCapture) {
  constexpr uint64_t kMotionTableGlobal = 0x1436F23C0ULL, kAnimeGlobal = 0x1436F2378ULL, kEyesGlobal = 0x1436F2648ULL;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(Load(dir, o)) << dir;
    SetOverlay(o);
    std::vector<std::pair<uint64_t, uint64_t>> reads;  // the call's H ranges: [lo, hi)
    for (const auto& line : o.manifest)
      if (line[0] == "H") {
        const uint64_t va = std::strtoull(line[1].c_str(), nullptr, 16);
        reads.push_back({va, va + std::strtoul(line[2].c_str(), nullptr, 10)});
      }
    std::set<uint64_t> explained;
    std::string diff;
    // Compares the read bytes of [cap, cap + n) with `gf`.
    const auto compare = [&](uint64_t cap, const uint8_t* gf, std::size_t n, const std::string& what) {
      for (const auto& r : reads)
        for (uint64_t x = std::max(cap, r.first); x < std::min(cap + n, r.second); ++x) {
          explained.insert(x);
          const uint8_t want = static_cast<uint8_t>(Read(o, x, 1));
          if (gf == nullptr || gf[x - cap] != want) {
            char buf[128];
            std::snprintf(buf, sizeof buf, " [%s +0x%llx: capture %02x, GF %02x]", what.c_str(),
                          static_cast<unsigned long long>(x - cap), want, gf == nullptr ? 0 : gf[x - cap]);
            diff += buf;
          }
        }
    };
    const uint64_t capRecords = Read(o, Read(o, kMotionTableGlobal, 8), 8);
    const uint8_t* gfRecords = g_loader->At(g_loader->motionRecordsVa, pes::kMotionSlotCount * pes::kMotionRecordBytes);
    ASSERT_NE(gfRecords, nullptr);
    for (std::size_t m = 0; m < pes::kMotionSlotCount; ++m) {
      const uint64_t capRec = capRecords + m * pes::kMotionRecordBytes;
      const uint8_t* gfRec = gfRecords + m * pes::kMotionRecordBytes;
      char name[48];
      std::snprintf(name, sizeof name, "motion %#zx counts", m);
      compare(capRec, gfRec, 14, name);  // the per-kind count bytes
      for (const pes::TrapMbinfoKind& k : pes::kTrapMbinfoKinds)
        for (unsigned i = 0; i < gfRec[k.kind] && i < 16; ++i) {
          uint64_t gfPtr;
          std::memcpy(&gfPtr, gfRec + k.listOffset + 8 * i, 8);
          std::snprintf(name, sizeof name, "motion %#zx kind %d #%u", m, k.kind, i);
          compare(Read(o, capRec + k.listOffset + 8 * i, 8), g_loader->At(gfPtr, k.recordBytes), k.recordBytes, name);
        }
    }
    uint64_t gfEyes = 0, gfAnime = 0;
    for (const auto& [va, value] : g_loader->globals) {
      if (va == kEyesGlobal) gfEyes = value;
      if (va == kAnimeGlobal) gfAnime = value;
    }
    const uint8_t* eyesObject = g_loader->At(gfEyes, 0x10);
    uint64_t eyesTable;
    std::memcpy(&eyesTable, eyesObject + 8, 8);
    compare(Read(o, Read(o, kEyesGlobal, 8) + 8, 8), g_loader->At(eyesTable, pes::kEyesTargetTableBytes), pes::kEyesTargetTableBytes, "EyesTarget");
    compare(Read(o, Read(o, kAnimeGlobal, 8), 8), g_loader->At(g_loader->animetableVa, pes::kAnimetableCellCount * 3),
            pes::kAnimetableCellCount * 3, "Animetable");
    {  // constant::Trap: registry (0x1437F4808) -> entry array -> entry[kTrapRegistryId] -> payload at +8
      const uint64_t registry = Read(o, 0x1437F4808ULL, 8), array = Read(o, registry, 8);
      const uint64_t entry = Read(o, array + 8 * pes::kTrapRegistryId, 8);
      compare(Read(o, entry + 8, 8), g_loader->At(g_loader->trapVa, pes::kTrapRuntimeBytes), pes::kTrapRuntimeBytes, "constant::Trap");
    }
    (void)gfAnime;
    const ObjSpans captured = CapturedObjects(o);
    std::string unexplained;
    for (const auto& r : reads) {
      uint64_t x = r.first;
      while (x < r.second) {
        int obj;
        uint32_t rel;
        if (explained.count(x) || Classify(captured, x, &obj, &rel) || (x >= 0x140000000ULL && x < 0x144000000ULL)) { ++x; continue; }
        char buf[48];
        std::snprintf(buf, sizeof buf, " %#llx", static_cast<unsigned long long>(x));
        unexplained += buf;
        break;  // the first byte of each H range is enough
      }
    }
    EXPECT_TRUE(diff.empty()) << dir << ": loader bytes differ:" << diff;
    std::printf("%s: unexplained H ranges (first byte):%s\n", dir.c_str(), unexplained.c_str());
  }
}

}  // namespace
