// Tests of PesEngineFixture (pes_engine_fixture.hpp): the windows file and the named failure, the read-miss seam, and the kind-0
// captures run through the fixture. The capture tests need PES_EXE and PES21_ROOT, and GF_ENGINE_ORACLE (colon-separated oracle
// directories, each a kind-0 capture's oracle directory with its mem.bin unpacked) and GF_ENGINE_WINDOWS (the directory of
// readset.py --windows files named <oracle dir name>.windows.tsv); they skip without them (PES-derived, never committed).
//
// The run is the trap call's inputs as the oracle test builds them (pes_trap_oracle_test.cpp's Build; that file is OneChainCut's and
// is not migrated here): the world bound to the engine's memory, the records the manifest names, P derived from the facts, the
// request words, and the call's own windows (the match windows of the windows file) lent from the capture. Only the reads are checked
// here, not the outputs.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "onthepitch/pesmemory.hpp"
#include "onthepitch/pesstamina.hpp"
#include "onthepitch/pestrapactor.hpp"
#include "onthepitch/pestrapdata.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes/exe.hpp"
#include "pes/physique.hpp"
#include "pes/vfs.hpp"
#include "pes_engine_fixture.hpp"
#include "pes_engine_trap.hpp"
#include "pes_oracle_snap.hpp"
#include "pes_state.hpp"

namespace {

using pes_engine_fixture::CallWindows;
using pes_engine_fixture::FirstUnmapped;
using pes_engine_fixture::ReadMissLog;
using pes_engine_fixture::Standard;
using pes_oracle::Oracle;
using pes_oracle::Read;

constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G

// The install's constants and trap source, loaded once per process (the oracle test's SetUp does the same).
struct Install {
  std::optional<pes::PesTrapSource> source;
  std::optional<pes::StaminaConstants> stamina;
  std::optional<pes::PhysiqueConstants> physique;
};

const Install& InstallOf() {
  static Install install;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    const char* root = std::getenv("PES21_ROOT");
    const char* exe = std::getenv("PES_EXE");
    std::string err;
    if (root == nullptr || exe == nullptr) return install;
    if (auto pe = pes::PeImage::Open(std::string(root) + "/PES2021.exe", &err)) {
      install.stamina = pes::LoadStaminaConstants(*pe, &err);
      install.physique = pes::LoadPhysiqueConstants(*pe, &err);
      pes::PesVfs vfs;
      if (vfs.Mount(root, &err)) install.source = pes::LoadTrapSource(*pe, vfs, exe, &err);
    }
  }
  return install;
}

// The engine the trap calls run on: booted once per process from PES_EXE and PES21_ROOT (the oracle test's TrapEngine).
pes_engine::EngineTrap* Engine() {
  static std::unique_ptr<pes_engine::EngineTrap> engine;
  static bool tried = false;
  static std::string why;
  if (!tried) {
    tried = true;
    const char* root = std::getenv("PES21_ROOT");
    const char* exe = std::getenv("PES_EXE");
    if (root != nullptr && exe != nullptr) engine = pes_engine::EngineTrap::Open(exe, root, 0, &why);
  }
  return engine.get();
}

float F32(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}
uint32_t Bits(const std::string& s) { return static_cast<uint32_t>(std::strtoull(s.c_str(), nullptr, 0)); }

// The manifest's "A" lines: facts the capture's player had (height, stamina) and the speed byte.
std::map<std::string, std::string> ActorLines(const Oracle& o) {
  std::map<std::string, std::string> actor;
  for (const auto& line : o.manifest)
    if (line.size() >= 3 && line[0] == "A") actor[line[1]] = line[2];
  return actor;
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

// The captured G (*(*(P+0xA08)+0xF0)+0xC0): the one value of the physique chain the capture holds.
float CapturedG(const Oracle& o) {
  const uint64_t gx = Read(o, o.regs.at("rdx") + pes::kPScalePtrOffset, 8);
  return F32(static_cast<uint32_t>(Read(o, Read(o, gx + pes::kPhysiqueNextOffset, 8) + pes::kPhysiqueGOffset, 4)));
}

// The leg-length slider is not in a slim capture: it is the oracle input recovered from PES's own G (the oracle test's RecoverLegSlider).
float RecoverLegSlider(const Oracle& o, float heightCm) {
  const float want = CapturedG(o);
  float candidates[16];
  for (int n = 0; n < 15; ++n) candidates[n] = (2.0f * static_cast<float>(n)) / 14.0f - 1.0f;
  candidates[15] = 0.0f;
  for (const float slider : candidates)
    if (pes::PhysiqueG(*InstallOf().physique, {heightCm, slider, false}) == want) return slider;
  return 0.0f;
}

pes::PesActorFacts FactsOf(const Oracle& o, const std::map<std::string, std::string>& actor) {
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
  f.heightCm = F32(Bits(actor.at("height_bits")));
  f.legSlider = RecoverLegSlider(o, f.heightCm);
  f.speedByte = static_cast<float>(std::atoi(actor.at("speed_byte").c_str()));
  f.stamina.cur = std::atoi(actor.at("stamina_cur").c_str());
  f.stamina.max = std::atoi(actor.at("stamina_max").c_str());
  f.stamina.breath = std::atoi(actor.at("breath").c_str());
  return f;
}

pes::PesActorOpenInputs OpenOf(const Oracle& o) {
  const uint64_t p = o.regs.at("rdx");
  pes::PesActorOpenInputs open;
  for (size_t i = 0; i < open.velocityRing.size(); ++i) open.velocityRing[i] = static_cast<uint8_t>(Read(o, p + pes::kPVelocityOffset + i, 1));
  return open;
}

void CopyActorMan(const Oracle& o, uint64_t g, pes::PesWorld& w, int slot) {
  uint8_t* man = w.ManRecord(slot);
  const uint64_t base = CapturedBase(o, g, "man", slot);
  for (size_t i = pes::kManFacingOffset; i < pes::kManVelXOffset + 12; ++i) man[i] = static_cast<uint8_t>(Read(o, base + i, 1));
}

// The manifest's N lines: the heap objects the chain reads that GF holds as state (MA, E, the squad records of table B).
void CopyNamed(const Oracle& o, uint64_t g, pes::PesWorld& w, pes::PesTrapExternals& ext, const std::vector<std::string>& line, int side) {
  const std::string& name = line[1];
  const size_t rel = std::strtoul(line[2].c_str(), nullptr, 16), len = std::strtoul(line[3].c_str(), nullptr, 10);
  const uint64_t an = Read(o, o.regs.at("rdx") + pes::kPPtr3AC0Offset, 8);
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
    const uint64_t record = Read(o, g + pes::kSideTableBOffset + pes::kTableEntryBytes * static_cast<size_t>(s), 8);
    w.SetEntry(pes::WorldTable::kSideB, s, record);
    pes::PesTrapSegment seg;
    seg.va = record + rel;
    for (size_t i = 0; i < len; ++i) seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
    ext.segments.push_back(std::move(seg));
    return;
  } else {
    return;  // An, GX, GY: derived
  }
  for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
}

// One kind-0 call's world and externals, as the oracle test builds them, with the call's match windows lent from the capture.
struct Built {
  std::unique_ptr<pes::PesWorld> world = std::make_unique<pes::PesWorld>();
  pes::PesTrapRequest request;
  pes::PesTrapExternals ext;
  int slot = 0;
};

void Build(const Oracle& o, const CallWindows& call, bool osArena, Built& b) {
  pes::PesWorld& w = *b.world;
  Engine()->BindWorld(w, -1);  // the window moves into the engine's memory; before any record is written
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
      if (dst == nullptr) continue;  // G (the window holds its tables) and P (derived below)
      const uint64_t base = CapturedBase(o, g, line[1], std::atoi(line[2].c_str()));
      const size_t rel = std::strtoul(line[3].c_str(), nullptr, 16), len = std::strtoul(line[4].c_str(), nullptr, 10);
      for (size_t i = 0; i < len; ++i) dst[rel + i] = static_cast<uint8_t>(Read(o, base + rel + i, 1));
    } else if (line[0] == "N") {
      CopyNamed(o, g, w, b.ext, line, side);
    }
  }
  // P is derived from the facts GF holds, the actor's man record and the named open inputs; none of its bytes is copied from the capture.
  const auto actor = ActorLines(o);
  pes::PesActorFacts facts = FactsOf(o, actor);
  facts.side = facts.slot / pes::kSlotsPerSide;
  b.slot = facts.slot;
  CopyActorMan(o, g, w, facts.slot);
  const Install& install = InstallOf();
  pes::ConstructActorP(w, facts.slot, install.source->actor);
  w.BindAnimeChain(facts.slot, Engine()->NewPlayer());  // the slot's anime chain: a fresh engine player's, as the match binds it
  pes::WriteActorP(w, facts, OpenOf(o), install.source->actor, *install.stamina, *install.physique);
  // The request, as the dispatcher took it (kind 0): its words are the chain's stack arguments in the capture.
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
  // The externals: the engine's image and shared regions, as a match lends them; the arena only when the case maps it.
  b.ext = Engine()->Externals();
  if (!osArena) {
    auto& borrowed = b.ext.borrowed;
    borrowed.erase(std::remove_if(borrowed.begin(), borrowed.end(),
                                  [](const PesMem::Window& x) { return x.base == pes::PesMemory::kOsArenaVa; }),
                   borrowed.end());
  }
  install.source->loader.AddTo(b.ext);  // the loader data (motion table, Animetable, registry, trap.o) from the install
  // The call's match windows (readset.py's read set), lent as the capture's own bytes: the heap the call reads that no record covers.
  for (const pes_engine_fixture::Window& win : call.windows) {
    if (win.source != "match") continue;
    pes::PesTrapSegment seg;
    seg.va = win.lo;
    for (uint64_t a = win.lo; a < win.hi; ++a) seg.bytes.push_back(static_cast<uint8_t>(Read(o, a, 1)));
    b.ext.segments.push_back(std::move(seg));
  }
  // The ball's prediction table (the samples the call reads) and the pad mask, as the oracle test takes them from the capture.
  const uint64_t capturedBall = o.regs.at("r8");
  for (std::size_t i = 0; i < static_cast<std::size_t>(pes::kBallSampleCount) * pes::kBallSampleStride; ++i)
    w.BallRecord()[pes::kBallSamplesOffset + i] = static_cast<uint8_t>(Read(o, capturedBall + pes::kBallSamplesOffset + i, 1));
  const uint32_t padMask = static_cast<uint32_t>(Read(o, Read(o, pes::kPadManagerGlobalVa, 8) + pes::kPadMaskOffset, 4));
  const auto loaderSegment = std::find_if(b.ext.segments.begin(), b.ext.segments.end(),
                                          [&install](const pes::PesTrapSegment& s) { return s.va == install.source->loader.segment.va; });
  if (loaderSegment != b.ext.segments.end() && install.source->loader.padMaskVa + sizeof padMask <= loaderSegment->va + loaderSegment->bytes.size())
    std::memcpy(loaderSegment->bytes.data() + (install.source->loader.padMaskVa - loaderSegment->va), &padMask, sizeof padMask);
}

struct Case {
  std::string dir;
  std::string windowsPath;
};

// The kind-0 captures under test, or nullopt when the environment does not name them.
std::optional<std::vector<Case>> CasesFromEnv() {
  const char* oracle = std::getenv("GF_ENGINE_ORACLE");
  const char* windows = std::getenv("GF_ENGINE_WINDOWS");
  if (oracle == nullptr || windows == nullptr || *oracle == '\0' || *windows == '\0') return std::nullopt;
  std::vector<Case> cases;
  std::stringstream ss(oracle);
  for (std::string dir; std::getline(ss, dir, ':');) {
    if (dir.empty()) continue;
    const std::string name = dir.substr(dir.find_last_of('/') + 1);
    cases.push_back({dir, std::string(windows) + "/" + name + ".windows.tsv"});
  }
  return cases;
}

// Runs one case's kind-0 call with the arena lent or not; returns the reads that found no region, in call order.
std::vector<std::pair<uint64_t, unsigned>> RunCase(const Oracle& o, const CallWindows& call, bool osArena) {
  Built b;
  Build(o, call, osArena, b);
  ReadMissLog log;
  static_cast<void>(b.world->RunTrapKind0(b.slot, b.request, b.ext));
  return log.Misses();
}

bool InArena(uint64_t addr) {
  return addr >= pes::PesMemory::kOsArenaVa && addr < pes::PesMemory::kOsArenaVa + pes::PesMemory::kOsArenaBytes;
}

// The round-11 captures: pid3727195 seq027 to seq037 (the eleven refusals the arena fixed).
bool IsRoundElevenCapture(const std::string& dir) {
  const std::string name = dir.substr(dir.find_last_of('/') + 1);
  for (int seq = 27; seq <= 37; ++seq) {
    char want[32];
    std::snprintf(want, sizeof want, "pid3727195_seq%03d", seq);
    if (name == want) return true;
  }
  return false;
}

}  // namespace

// Self tests: no PES data.
TEST(PesEngineFixtureSelfTest, AReadOutsideEveryRegionIsLoggedAndReadsZero) {
  PesMem m;
  uint8_t mapped[16] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0};
  m.window.base = 0x5000;
  m.window.size = sizeof mapped;
  m.window.rw = mapped;
  ReadMissLog log;
  EXPECT_EQ(m.rd(0x5000, 8), 0x0807060504030201ULL) << "a mapped read returns the window's bytes";
  EXPECT_TRUE(log.Misses().empty()) << "a mapped read is not a miss";
  EXPECT_EQ(m.rd(0x9000, 8), 0u) << "an unmapped read returns zero, as it always has";
  ASSERT_EQ(log.Misses().size(), 1u);
  EXPECT_EQ(log.Misses()[0].first, 0x9000u);
  EXPECT_EQ(log.Misses()[0].second, 8u);
}

TEST(PesEngineFixtureSelfTest, AnUnmappedArenaWindowIsNamedByItsFirstAddress) {
  std::istringstream in(
      "call\tsynthetic\tnone\t-\t0x1000\t0x2000\n"
      "window\tmatch\t0x17f8b08\t0x17f8b18\t2\n"
      "window\tos_arena\t0x3000000040\t0x3000000048\t1\n");
  const std::vector<CallWindows> calls = pes_engine_fixture::ReadWindows(in);
  ASSERT_EQ(calls.size(), 1u);
  ASSERT_EQ(calls[0].windows.size(), 2u);
  EXPECT_EQ(FirstUnmapped(calls[0], Standard{true}), nullptr) << "with the arena lent, every window is mapped";
  const pes_engine_fixture::Window* bad = FirstUnmapped(calls[0], Standard{false});
  ASSERT_NE(bad, nullptr);
  EXPECT_EQ(bad->source, "os_arena");
  EXPECT_NE(pes_engine_fixture::Describe(calls[0], *bad).find("0x3000000040"), std::string::npos)
      << pes_engine_fixture::Describe(calls[0], *bad);
}

TEST(PesEngineFixtureSelfTest, AMalformedWindowsFileIsRejected) {
  std::istringstream orphan("window\tmatch\t0x1\t0x2\t1\n");
  EXPECT_THROW(pes_engine_fixture::ReadWindows(orphan), std::runtime_error);
  std::istringstream short_line("call\tx\n");
  EXPECT_THROW(pes_engine_fixture::ReadWindows(short_line), std::runtime_error);
}

// The kind-0 captures through the fixture. The arena is lent: every capture's call reads only mapped memory.
TEST(PesEngineFixtureCaptures, KindZeroCallsReadOnlyMappedMemoryWithTheArenaLent) {
  const auto cases = CasesFromEnv();
  if (!cases) GTEST_SKIP() << "GF_ENGINE_ORACLE / GF_ENGINE_WINDOWS unset (PES-derived captures, not committed)";
  ASSERT_NE(Engine(), nullptr) << "the engine boots from PES_EXE and PES21_ROOT";
  ASSERT_TRUE(InstallOf().source.has_value()) << "the install's trap source (PES21_ROOT, PES_EXE)";
  int ran = 0;
  for (const Case& c : *cases) {
    Oracle o;
    ASSERT_TRUE(pes_oracle::Load(c.dir, o)) << c.dir;
    pes_oracle::SetOverlay(o);
    std::ifstream wf(c.windowsPath);
    ASSERT_TRUE(wf) << "the windows file " << c.windowsPath;
    const auto calls = pes_engine_fixture::ReadWindows(wf);
    ASSERT_EQ(calls.size(), 1u) << c.windowsPath;
    ASSERT_EQ(FirstUnmapped(calls[0], Standard{true}), nullptr) << pes_engine_fixture::Describe(calls[0], *FirstUnmapped(calls[0], Standard{true}));
    const auto misses = RunCase(o, calls[0], true);
    EXPECT_TRUE(misses.empty()) << c.dir << ": the call reads " << std::hex << misses.front().first << " outside every mapped window";
    ++ran;
  }
  std::printf("[engine fixture] %d kind-0 captures, arena lent\n", ran);
}

// The mutation: the arena is not lent. Each round-11 capture reads an arena address the call does not map, and the fixture names it.
TEST(PesEngineFixtureCaptures, WithoutTheArenaTheReadsThatMissNameAnArenaAddress) {
  const auto cases = CasesFromEnv();
  if (!cases) GTEST_SKIP() << "GF_ENGINE_ORACLE / GF_ENGINE_WINDOWS unset (PES-derived captures, not committed)";
  ASSERT_NE(Engine(), nullptr) << "the engine boots from PES_EXE and PES21_ROOT";
  ASSERT_TRUE(InstallOf().source.has_value()) << "the install's trap source (PES21_ROOT, PES_EXE)";
  int failing = 0, round11 = 0;
  for (const Case& c : *cases) {
    Oracle o;
    ASSERT_TRUE(pes_oracle::Load(c.dir, o)) << c.dir;
    pes_oracle::SetOverlay(o);
    std::ifstream wf(c.windowsPath);
    ASSERT_TRUE(wf) << "the windows file " << c.windowsPath;
    const auto calls = pes_engine_fixture::ReadWindows(wf);
    ASSERT_EQ(calls.size(), 1u) << c.windowsPath;
    const auto misses = RunCase(o, calls[0], false);
    if (IsRoundElevenCapture(c.dir)) {
      ++round11;
      EXPECT_FALSE(misses.empty()) << c.dir << ": the round-11 capture must read the arena";
    }
    if (IsRoundElevenCapture(c.dir) && !misses.empty())
      std::printf("[engine fixture] %s: first unmapped read 0x%llx (%zu reads unmapped)\n", c.dir.c_str(),
                  static_cast<unsigned long long>(misses.front().first), misses.size());
    if (misses.empty()) continue;
    ++failing;
    EXPECT_TRUE(InArena(misses.front().first)) << c.dir << ": the first unmapped read " << std::hex << misses.front().first
                                               << " is not an arena address";
  }
  std::printf("[engine fixture] arena not lent: %d captures read unmapped memory (%d of them round-11)\n", failing, round11);
}
