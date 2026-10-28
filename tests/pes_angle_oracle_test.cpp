// The corridor angle 0x142108630 (the Clear unit's answer, PES's own code) replayed from a captured call, in GF's build.
// The translated function runs on the capture's memory, viewed as the harness views it (SnapView over mem.bin, the
// snapshot's maps and the call's delta pages), and every case must equal the PES reference row bit for bit: rax, xmm0
// and the write log. Each record binding (a record the angle reads through G, or through the Clear unit's ctx) is then
// changed by one byte in the captured memory, and the result must change.
//
// GF_PES_ANGLE_ORACLE names a directory of PES-derived files, never committed: mem.bin (a link to the capture),
// maps.txt, ok_calls.bin, ok_delta.bin, pages.bin, teb.txt and corpus.jsonl (the reference rows). Without it, skip.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "translated.hpp"
#include "pes_state.hpp"

namespace {

// One traced call as the harness records it (snapcase.py CALL_REC): 16 GPRs, 16 XMM (lo, hi), the return address.
struct CallRec {
  uint64_t v[49];
};

constexpr int kRcx = 1;   // A0 = ctx (the Clear unit's +8)
constexpr int kRsp = 4;
constexpr int kRetSlot = 48;
constexpr uint64_t kPage = 4096;

// PES layout facts the angle reads (docs/re/pes-re-record-ids.md §0; docs/re/pes-re-record-layouts-chains.md §1-4).
constexpr uint64_t kImageG = 0x1436F3DC8ULL;  // the image global that holds G
constexpr uint64_t kG40 = 0x40;               // G+0x40: the carrier record
constexpr uint64_t kG270 = 0x270;             // G+0x270: MI
constexpr uint64_t kG360 = 0x360;
constexpr uint64_t kG2AA8 = 0x2AA8;
constexpr uint64_t kCtx18 = 0x18;             // ctx+0x18: the record the angle reads field pairs from
constexpr uint64_t kCtxE0 = 0xE0;             // ctx+0xE0: the E0 ring

struct Oracle {
  SnapView snap;
  int pagesFd = -1;
  std::vector<CallRec> calls;
  std::vector<uint64_t> deltas;   // ok_delta.bin: per case, a page count, then (va, pages.bin offset) pairs
  std::vector<std::string> rows;  // corpus.jsonl: the PES reference, one row per case
  uint64_t teb = 0;
};

struct Outcome {
  std::string rax, xmm0, writes;
};

struct ByteFlip {
  uint64_t va;
  uint8_t mask;
};

std::vector<uint8_t> Slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::vector<std::string> Lines(const std::string& path) {
  std::ifstream f(path);
  std::vector<std::string> out;
  for (std::string line; std::getline(f, line);) {
    if (!line.empty()) out.push_back(line);
  }
  return out;
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

  const std::vector<uint8_t> calls = Slurp(dir + "/ok_calls.bin");
  if (calls.empty() || calls.size() % sizeof(CallRec) != 0) return false;
  o.calls.resize(calls.size() / sizeof(CallRec));
  std::memcpy(o.calls.data(), calls.data(), calls.size());

  const std::vector<uint8_t> delta = Slurp(dir + "/ok_delta.bin");
  if (delta.size() % sizeof(uint64_t) != 0) return false;
  o.deltas.resize(delta.size() / sizeof(uint64_t));
  std::memcpy(o.deltas.data(), delta.data(), delta.size());

  o.rows = Lines(dir + "/corpus.jsonl");
  const std::vector<std::string> teb = Lines(dir + "/teb.txt");
  if (teb.empty()) return false;
  o.teb = std::strtoull(teb[0].c_str(), nullptr, 16);
  return o.rows.size() == o.calls.size();
}

// The value of a JSON string field in a corpus row ("key": "value").
std::string Field(const std::string& row, const char* key) {
  const std::string pat = std::string("\"") + key + "\": \"";
  const size_t at = row.find(pat);
  if (at == std::string::npos) return {};
  const size_t from = at + pat.size();
  return row.substr(from, row.find('"', from) - from);
}

// The delta pages of case i: (va, offset in pages.bin) pairs, walked past the earlier cases' counts.
std::vector<std::pair<uint64_t, uint64_t>> DeltaOf(const Oracle& o, size_t caseIndex) {
  size_t pos = 0;
  for (size_t k = 0; k < caseIndex; ++k) pos += 1 + 2 * static_cast<size_t>(o.deltas.at(pos));
  const uint64_t np = o.deltas.at(pos++);
  std::vector<std::pair<uint64_t, uint64_t>> out;
  for (uint64_t j = 0; j < np; ++j, pos += 2) out.push_back({o.deltas.at(pos), o.deltas.at(pos + 1)});
  return out;
}

// The captured bytes at va, as the replay sees them (delta pages included).
uint64_t Read(const Oracle& o, uint64_t va, unsigned n) {
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) {
    const uint8_t* b = o.snap.at(va + i);
    if (b == nullptr) return 0;
    v |= static_cast<uint64_t>(*b) << (8 * i);
  }
  return v;
}

// The call's memory: its delta pages, then the record bytes flipped for this mutation (a copy of the captured page).
void SetOverlay(Oracle& o, size_t caseIndex, const std::vector<ByteFlip>& flips) {
  o.snap.over.clear();
  for (const auto& [va, off] : DeltaOf(o, caseIndex)) {
    SnapView::Block b{va, std::vector<uint8_t>(kPage)};
    if (pread(o.pagesFd, b.bytes.data(), kPage, static_cast<off_t>(off)) != static_cast<ssize_t>(kPage)) return;
    o.snap.over.push_back(std::move(b));
  }
  for (const ByteFlip& f : flips) {
    const uint64_t pg = f.va & ~(kPage - 1);
    auto it = std::find_if(o.snap.over.begin(), o.snap.over.end(),
                           [pg](const SnapView::Block& b) { return b.va == pg; });
    if (it == o.snap.over.end()) {
      SnapView::Block b{pg, std::vector<uint8_t>(kPage)};
      for (uint64_t k = 0; k < kPage; ++k) {
        const uint8_t* p = o.snap.at(pg + k);
        b.bytes[k] = p != nullptr ? *p : 0;
      }
      o.snap.over.push_back(std::move(b));
      it = o.snap.over.end() - 1;
    }
    it->bytes[f.va - pg] ^= f.mask;
  }
  std::sort(o.snap.over.begin(), o.snap.over.end(),
            [](const SnapView::Block& a, const SnapView::Block& b) { return a.va < b.va; });
}

// Runs the translated angle on case i's registers and memory, and formats its outcome as the corpus rows do.
Outcome Replay(Oracle& o, size_t caseIndex) {
  const CallRec& c = o.calls[caseIndex];
  PesState s;
  s.mem.snap = &o.snap;
  s.gs_base = o.teb;
  for (int g = 0; g < 16; ++g) s.gpr[g] = c.v[g];
  for (int x = 0; x < 16; ++x) {
    s.xmm[x][0] = c.v[16 + 2 * x];
    s.xmm[x][1] = c.v[17 + 2 * x];
  }
  s.wr(c.v[kRsp], c.v[kRetSlot], 8);  // the recorded return address, before the write log starts
  std::vector<std::pair<uint64_t, uint8_t>> log;
  s.mem.wlog = &log;
  pes_fn_142108630(s);

  Outcome out;
  char buf[40];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(s.gpr[0]));
  out.rax = buf;
  std::snprintf(buf, sizeof buf, "%016llx%016llx", static_cast<unsigned long long>(s.xmm[0][1]),
                static_cast<unsigned long long>(s.xmm[0][0]));
  out.xmm0 = buf;
  for (const auto& w : log) {
    std::snprintf(buf, sizeof buf, "%016llx%02x", static_cast<unsigned long long>(w.first), static_cast<unsigned>(w.second));
    out.writes += buf;
  }
  return out;
}

Outcome Reference(const Oracle& o, size_t caseIndex) {
  const std::string& row = o.rows[caseIndex];
  return Outcome{Field(row, "rax"), Field(row, "xmm0"), Field(row, "w")};
}

// The byte of each record the angle reads, located through the captured pointers (G, then its records, then ctx).
struct Binding {
  const char* name;
  uint64_t va;
};

std::vector<Binding> BindingsOf(const Oracle& o, size_t caseIndex) {
  const uint64_t ctx = o.calls[caseIndex].v[kRcx];
  const uint64_t g = Read(o, kImageG, 8);
  return {
      {"G+0x40 carrier record +0x1C", Read(o, g + kG40, 8) + 0x1C},
      {"G+0x40 carrier record +0x80", Read(o, g + kG40, 8) + 0x80},
      {"G+0x270 MI +0x1198 frame", Read(o, g + kG270, 8) + 0x1198},
      {"G+0x360 record +0x8", Read(o, g + kG360, 8) + 0x8},
      {"G+0x2AA8 record +0x0", Read(o, g + kG2AA8, 8)},
      {"ctx+0x18 record +0x2482C", Read(o, ctx + kCtx18, 8) + 0x2482C},
      {"ctx+0xE0 E0 ring word 0", ctx + kCtxE0},
  };
}

class AngleOracle : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("GF_PES_ANGLE_ORACLE");
    if (dir == nullptr || *dir == '\0') {
      GTEST_SKIP() << "GF_PES_ANGLE_ORACLE unset (PES-derived capture, not committed)";
    }
    ASSERT_TRUE(Load(dir, oracle_)) << "bad oracle directory " << dir;
  }
  Oracle oracle_;
};

TEST_F(AngleOracle, CapturedCallsMatchThePesReferenceBitForBit) {
  for (size_t i = 0; i < oracle_.calls.size(); ++i) {
    ASSERT_NE(oracle_.rows[i].find("\"ok\": true"), std::string::npos) << "case " << i << " has no PES reference";
    SetOverlay(oracle_, i, {});
    const Outcome got = Replay(oracle_, i);
    const Outcome want = Reference(oracle_, i);
    EXPECT_EQ(got.rax, want.rax) << "case " << i;
    EXPECT_EQ(got.xmm0, want.xmm0) << "case " << i;
    EXPECT_EQ(got.writes, want.writes) << "case " << i;
  }
}

TEST_F(AngleOracle, EachRecordBindingChangesTheResult) {
  SetOverlay(oracle_, 0, {});
  const std::vector<Binding> bindings = BindingsOf(oracle_, 0);
  for (const Binding& b : bindings) {
    ASSERT_NE(oracle_.snap.at(b.va), nullptr) << b.name << " is not mapped in the capture";
    SetOverlay(oracle_, 0, {ByteFlip{b.va, 0xFF}});
    const Outcome got = Replay(oracle_, 0);
    const Outcome want = Reference(oracle_, 0);
    const bool changed = got.rax != want.rax || got.xmm0 != want.xmm0 || got.writes != want.writes;
    EXPECT_TRUE(changed) << "flipping " << b.name << " left the result unchanged";
  }
}

}  // namespace
