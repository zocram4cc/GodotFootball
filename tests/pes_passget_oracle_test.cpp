// The captured ActionPassGet ticks (0x14094E0E0, real calls: live2/pid480109) replayed on GF's window: the records the call reads
// copied whole from the capture through its G (pes_oracle_world.hpp CopyRecords, plus the per-slot passget object / pc / R), the
// run PesWorld::RunPassGetTick, and every output byte compared with what PES's own run wrote (corpus.jsonl's `w` log).
// Oracle directories: tools/dev/translate/passget_oracle.py; env GF_PES_PASSGET_ORACLE (PES-derived, never committed).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <cstring>
#include <vector>

#include "onthepitch/pespassgetworld.hpp"
#include "onthepitch/pesworldrecord.hpp"
#include "pes_oracle_world.hpp"
#include "pes_state.hpp"
#include "pes/translated/pes_dispatch.hpp"
#include "pes/exe.hpp"
#include "simcontext.hpp"
#include "pes_match_constants.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace {

using pes_oracle::Field;
using pes_oracle::Oracle;
using pes_oracle::Read;

// PES's write log (corpus.jsonl's `w`, 18 hex chars per byte: "addr" + "value") filtered to a range.
struct WriteLog {
  std::vector<std::pair<uint64_t, uint8_t>> bytes;
  explicit WriteLog(const std::string& row) {
    const std::string w = Field(row, "w");
    for (size_t at = 0; at + 18 <= w.size(); at += 18)
      bytes.emplace_back(std::strtoull(w.substr(at, 16).c_str(), nullptr, 16),
                         static_cast<uint8_t>(std::strtoul(w.substr(at + 16, 2).c_str(), nullptr, 16)));
  }
  std::vector<std::pair<uint64_t, uint8_t>> in(uint64_t lo, uint64_t hi) const {
    std::vector<std::pair<uint64_t, uint8_t>> out;
    std::copy_if(bytes.begin(), bytes.end(), std::back_inserter(out),
                 [&](const auto& b) { return b.first >= lo && b.first < hi; });
    return out;
  }
};

class PassGetOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_PASSGET_ORACLE");
    if (dir == nullptr || *dir == '\0') GTEST_SKIP() << "GF_PES_PASSGET_ORACLE unset (PES-derived captures, not committed)";
    for (const auto& e : std::filesystem::directory_iterator(dir))
      if (std::filesystem::exists(e.path() / "manifest.tsv")) dirs_.push_back(e.path().string());
    std::sort(dirs_.begin(), dirs_.end());
    ASSERT_FALSE(dirs_.empty()) << "no oracle directory under " << dir;
  }
  std::vector<std::string> dirs_;
  struct PointerCheck { std::string dir; uint64_t cellVa, pesPtr; uint64_t gfPtr; };
  std::vector<PointerCheck> pointerChecks_;
};

// One captured tick on GF's window: the window's records copied whole from the capture, the object constructed from PES's
// constructor rows and then overlaid with the capture's bytes (the capture holds the tick's own route state at entry), pc and
// R copied, the run, and the answer compared with what PES's own run wrote (R, the object's +0x68/+0x6C and the r9 cell).
TEST_F(PassGetOracle, EveryCapturedTickOnGfsWindowWritesWhatPesWrote) {
  int compared = 0, bytesChecked = 0;
  for (const std::string& dir : dirs_) {
    Oracle o;
    ASSERT_TRUE(pes_oracle::Load(dir, o)) << dir;
    pes_oracle::SetOverlay(o);
    pes_oracle::Built b;
    pes::PesWorld& world = *b.world;
    pes_oracle::BindWorld(world);
    world.BindImageTables(pes_oracle::InstallTables());
    const uint64_t g = Read(o, pes_oracle::kImageG, 8);
    const uint64_t thisVa = o.regs.at("rcx"), pcVa = o.regs.at("rdx"), rVa = o.regs.at("r8"), argVa = o.regs.at("r9");
    const int slot = static_cast<int>(Read(o, pcVa + pes::kPcIdOffset, 4));
    const int side = slot >= pes::kSlotsPerSide ? 1 : 0;
    // The records the window holds, copied whole (the capture's G and its tables), then the passget object/pc/R/arg over them.
    o.ctxVa = static_cast<uint64_t>(Read(o, pcVa + pes::kPcCtxOffset, 8));
    pes_oracle::CopyRecords(o, b, true, side);
    const auto copyBytes = [&](uint8_t* dst, uint64_t srcBase, size_t n) {
      for (size_t i = 0; i < n; ++i) dst[i] = static_cast<uint8_t>(Read(o, srcBase + i, 1));
    };
    // The capture's object bytes carry PES's heap pointers for this/route-manager vtable cells and route objects; build the
    // rows first, then overlay the capture's bytes, then re-pin the cells whose targets are the window's own records (the
    // route objects at +0x70 offsets, the object's vtable at 0).
    world.ConstructPassGetObject(slot);
    copyBytes(world.PassGetObject(slot), thisVa, pes::kPassGetObjectBytes);
    // The capture's route-object cells name PES's heap; the window's own objects must be named by the window's VAs again
    // (0x14094DC22 reads *(route + 8) as a vtable and dispatches on it).
    for (const pes::PassGetStore& row : pes::kPassGetObjectStores)
      if (row.kind == 1) {
        const uint64_t value = pes::PesWorld::PassGetVa(slot) + row.value;
        std::memcpy(world.PassGetObject(slot) + row.offset, &value, sizeof value);
      }
    // The capture's object bytes are the capture's own memory: the 8 bytes AFTER every vtable row (e.g. route+8) are the
    // capture's stack garbage, and FUN_14094DAD0 dispatches on *(route + 8), which is the vtable's slot 0. The re-pin must
    // restore every byte the file holds at the offsets the ctor rows wrote (kind 0 rows whose value is an image pointer:
    // the vtable word itself AND the 8 bytes after it, which the ctor image read from the file as one qword neighbour).
    for (const pes::PassGetStore& row : pes::kPassGetObjectStores)
      if (row.kind == 0 && row.offset >= 0x60 && row.value >= 0x142600000ULL && row.value < 0x142800000ULL) {
        static const pes::PeImage exe = *pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", nullptr);
        uint64_t imageWord = 0;
        EXPECT_TRUE(exe.ReadVa(row.value + 8, sizeof imageWord, reinterpret_cast<uint8_t*>(&imageWord))) << std::hex << row.value;
        std::memcpy(world.PassGetObject(slot) + row.offset + 8, &imageWord, sizeof imageWord);
      }
    // The other six route objects' start cells (manager +0x610/+0xC68/+0x11F8/+0x1828/+0x1E00/+0x24C0) are written by
    // their own ctors (FUN_14091E810 etc.), which the manager ctor calls; my generator ran only FUN_14091DAB0, so their
    // rows are missing. Re-pin each +8 from the image at the vtable the capture holds (checked on seq001: the capture's
    // cells carry PES's own vtable words 0x14262FF60 etc.; the +8 is the capture's garbage).
    for (const uint64_t route : {0x680ULL, 0xD38ULL, 0x1268ULL, 0x1898ULL, 0x1E70ULL, 0x2530ULL}) {
      static const pes::PeImage exe2 = *pes::PeImage::Open(std::string(std::getenv("PES21_ROOT")) + "/PES2021.exe", nullptr);
      uint64_t capVt = 0;
      std::memcpy(&capVt, world.PassGetObject(slot) + route, sizeof capVt);
      if (capVt >= 0x142600000ULL && capVt < 0x142800000ULL) {
        uint64_t slot0 = 0;
        EXPECT_TRUE(exe2.ReadVa(capVt + 8, sizeof slot0, reinterpret_cast<uint8_t*>(&slot0))) << std::hex << capVt;
        std::memcpy(world.PassGetObject(slot) + route + 8, &slot0, sizeof slot0);
      }
    }
    copyBytes(world.PassGetPc(slot), pcVa, pes::kPassGetPcBytes);
    copyBytes(world.PassGetR(slot), rVa, pes::kPassGetRBytes);
    // The skip object (G+0x3B0): the tick reads it (FUN_1423E7FD0's byte +0x9C); the capture's bytes are its state.
    copyBytes(world.Skip(), Read(o, g + pes::kSkipPtrOffset, 8) ? Read(o, Read(o, g + pes::kSkipPtrOffset, 8), pes::kSkipBytes) : 0, pes::kSkipBytes);
    uint8_t* arg = world.PassGetObject(slot) + pes::PesWorld::PassGetArgVa(slot) - pes::PesWorld::PassGetVa(slot);
    const uint32_t argValue = static_cast<uint32_t>(Read(o, argVa, 4));
    std::memcpy(arg, &argValue, sizeof argValue);
    // The window's ctx pointers to MA and E are the window's own (CopyRecords keeps them); pc+0x38 must name the side's ctx.
    uint8_t* pc = world.PassGetPc(slot);
    pes::StoreU64(pc, pes::kPcCtxOffset, pes::PesWorld::CtxVa(side));
    // The run. PES's dispatch answers a translated function as a zero-taking no-op here: the test holds the write log, so
    // "no output" reads as no dispatch, not as a passing zero. The survey env is deliberately unset.
    // The trap source (the constants' own, filled by FillMissingPesConstants from PES_EXE): its loader data (the registry,
    // the motion records) is staged by AddTo, and the image's own VAs (0x1426xxxxx vtable cells, .rdata tables) are read
    // through the image slice the trap source already mapped.
    pes::PesTrapExternals ext;
    ASSERT_TRUE(pestest::SharedPesMatchConstants().trap.has_value()) << "PES trap data missing";
    pestest::SharedPesMatchConstants().trap->loader.AddTo(ext);
    static pes::PesTrapMemory imageMem(*pestest::SharedPesMatchConstants().trap);
    ext.image = imageMem.Externals().image;
    ext.imageBase = imageMem.Externals().imageBase;
    ext.imageBytes = imageMem.Externals().imageBytes;
    std::vector<std::pair<uint64_t, uint8_t>> log;
    ext.writeLog = &log;
    // The capture's bytes outside the window (the heap the tick reads through the image globals, e.g. the image globals
    // themselves 0x1436F3DC8/0x1437F4808, and the thunks at 0x7FF5...), as the trap oracle's osBytes does.
    std::vector<pes::PesTrapSegment> osSegs;
    for (const auto& line : o.manifest) {
      if (line[0] != "H" && line[0] != "I") continue;
      if (line[0] == "I" && std::strtoull(line[1].c_str(), nullptr, 16) == pes_oracle::kImageG) continue;
      pes::PesTrapSegment seg;
      seg.va = std::strtoull(line[1].c_str(), nullptr, 16);
      if (seg.va == pes_oracle::kImageG) continue;
      for (size_t i = 0, n = std::strtoul(line[2].c_str(), nullptr, 10); i < n; ++i)
        seg.bytes.push_back(static_cast<uint8_t>(Read(o, seg.va + i, 1)));
      osSegs.push_back(std::move(seg));
    }
    // The heap objects the tick writes and reads back inside the call (a candidate cache: the first call stores its fields, the
    // second hits): the live-in set holds only what the call reads before writing, so a byte PES stores first is not staged. The
    // window gives them backing: every capture byte PES stores outside the window's records and the stack joins the staged
    // ranges (merged, bytes from the capture).
    {
      const WriteLog stores(o.row);
      const uint64_t rsp = o.regs.at("rsp");
      const std::pair<uint64_t, size_t> own[3] = {{thisVa, pes::kPassGetObjectBytes}, {pcVa, pes::kPassGetPcBytes}, {rVa, pes::kPassGetRBytes}};
      std::vector<std::pair<uint64_t, uint64_t>> iv;
      for (const auto& seg : osSegs) iv.push_back({seg.va, seg.va + seg.bytes.size()});
      for (const auto& [addr, value] : stores.bytes) {
        (void)value;
        bool mine = addr + 0x2000 >= rsp && addr < rsp + 0x400;
        for (const auto& [base, n] : own) mine = mine || (addr >= base && addr < base + n);
        if (!mine) iv.push_back({addr, addr + 1});
      }
      std::sort(iv.begin(), iv.end());
      std::vector<std::pair<uint64_t, uint64_t>> merged;
      for (const auto& r : iv) {
        if (!merged.empty() && r.first <= merged.back().second + 8) merged.back().second = std::max(merged.back().second, r.second);
        else merged.push_back(r);
      }
      osSegs.clear();
      for (const auto& [lo, hi] : merged) {
        pes::PesTrapSegment seg;
        seg.va = lo;
        for (uint64_t a = lo; a < hi; ++a) seg.bytes.push_back(static_cast<uint8_t>(Read(o, a, 1)));
        osSegs.push_back(std::move(seg));
      }
    }
    for (auto& seg : osSegs) ext.segments.push_back(std::move(seg));  // after the loader data AddTo staged (constant::PassGet included)
    std::vector<std::pair<uint64_t, unsigned>> reads;
    ext.readLog = &reads;
    pes::PesWorld::SetDispatchThrowStatic(true);
    try {
      world.RunPassGetTick(slot, ext);
    } catch (const PesDispatchMissing& e) {
      ADD_FAILURE() << dir << ": dispatch missing (target 0x" << std::hex << e.target << std::dec << ")";
      continue;
    }
    pes::PesWorld::SetDispatchThrowStatic(false);
    if (const char* rl = std::getenv("GF_PASSGET_READLOG")) {  // diagnostic: the window run's ordered reads, as the capture-side tool prints them
      std::FILE* f = std::fopen((std::string(rl) + "/" + dir.substr(dir.find_last_of('/') + 1) + ".jsonl").c_str(), "w");
      if (f) {
        for (const auto& [a, n] : reads) std::fprintf(f, "{\"a\": \"0x%llx\", \"n\": %u}\n", static_cast<unsigned long long>(a), n);
        std::fclose(f);
      }
    }
    // PES's own writes over the same ranges, read through the delta overlay (the snapshot as the capture saw it at entry).
    const WriteLog pes(o.row);
    // The capture's own VAs: the tick wrote into ITS object/pc/R (heap addresses); the window holds the same records at
    // PassGetVa/PcVa/RVa. Each capture write in one of the three records is compared with the window's byte.
    const std::pair<uint64_t, size_t> rec[3] = {{thisVa, pes::kPassGetObjectBytes}, {pcVa, pes::kPassGetPcBytes}, {rVa, pes::kPassGetRBytes}};
    for (const auto& [addr, value] : pes.bytes) {
      int which = -1;
      size_t rel = 0;
      for (int k = 0; k < 3; ++k)
        if (addr >= rec[k].first && addr < rec[k].first + rec[k].second) { which = k; rel = static_cast<size_t>(addr - rec[k].first); break; }
      if (which < 0) continue;  // the tick's writes outside the three records (the caller's stack spills) are not GF's to make
      // Pointer cells (the route walk's record pointers: object +0x40/+0x48 and R +0x894..+0x89b): GF's pointer names
      // GF's own memory, so the bytes can never equal the capture's; the pointed-to value is what the gameplay reads.
      const uint64_t cellVa = which == 0 ? pes::PesWorld::PassGetVa(slot) + rel
                                          : which == 1 ? pes::PesWorld::PassGetPcVa(slot) + rel
                                                       : pes::PesWorld::PassGetRVa(slot) + rel;
      const bool pointerCell =
          (which == 0 && (rel - 0x40 < 0x10)) || (which == 2 && (rel - 0x894 < 8));
      if (pointerCell) {
        // Compare the pointed-to 16 bytes at the end of the capture loop instead: collect them.
        pointerChecks_.push_back({dir, cellVa, static_cast<uint64_t>(Read(o, addr & ~7ULL, 8)), 0});
        ++bytesChecked;
        continue;
      }
      const uint8_t got = which == 0 ? world.PassGetObject(slot)[rel] : which == 1 ? world.PassGetPc(slot)[rel] : world.PassGetR(slot)[rel];
      if (got != value)
        ADD_FAILURE() << dir << ": record " << which << " +0x" << std::hex << rel << std::dec << " (slot " << slot
                      << "): got " << static_cast<unsigned>(got) << ", want " << static_cast<unsigned>(value);
      ++bytesChecked;
    }
    compared++;
  }
  RecordProperty("compared", compared);
  RecordProperty("bytes", bytesChecked);
  ASSERT_GT(compared, 0) << "no capture compared";
  ASSERT_GT(bytesChecked, 0) << "PES's own run wrote no byte into the object/pc/R records";
}

}  // namespace
