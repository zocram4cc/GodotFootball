// PesState: the machine the committed translated functions run on.
// Mirrors tools/dev/diffharness/run.py: flat memory (image baked at VAs +
// caller regions), 16 GPRs, 16 XMM as u128, x86 flags. Bit-exact by
// construction: integer ops are width-tagged, float ops are SSE-semantic,
// comparisons set flags like COMISS/UCOMISS (NaN -> ZF=PF=CF=1).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <utility>

// Snapshot pages for the diff main's snapshot mode. A run is a readable VA range of the snapshot
// with its file offset in mem.bin (maps.txt). A page is copied in 256 KiB blocks on first touch;
// reset() drops the blocks, so every case starts from pristine snapshot memory and the process
// never maps the whole snapshot. Holes and unmapped addresses have no block (at() returns null).
struct SnapView {
  struct Run { uint64_t va, size, off; };
  struct Block { uint64_t va; std::vector<uint8_t> bytes; };
  static constexpr uint64_t kBlock = 0x40000;
  int fd = -1;
  std::vector<Run> runs;              // sorted by va
  mutable std::vector<Block> blocks;  // sorted by va, disjoint
  mutable std::vector<Block> over;  // the call's delta pages (4 KiB each, sorted by va): set per call, read before the runs
  uint8_t* at(uint64_t addr) const {
    uint64_t pg = addr & ~uint64_t(0xFFF);
    auto ob = std::lower_bound(over.begin(), over.end(), pg, [](const Block& b, uint64_t a) { return b.va < a; });
    if (ob != over.end() && ob->va == pg) return ob->bytes.data() + (addr - pg);
    auto by_va = [](uint64_t a, const Block& b) { return a < b.va; };
    auto it = std::upper_bound(blocks.begin(), blocks.end(), addr, by_va);
    if (it != blocks.begin()) {
      --it;
      if (addr < it->va + it->bytes.size()) return it->bytes.data() + (addr - it->va);
    }
    auto r = std::upper_bound(runs.begin(), runs.end(), addr,
                              [](uint64_t a, const Run& x) { return a < x.va; });
    if (r == runs.begin()) return nullptr;
    --r;
    if (addr >= r->va + r->size) return nullptr;
    uint64_t lo = std::max(r->va, addr & ~(kBlock - 1));
    uint64_t hi = std::min(r->va + r->size, lo + kBlock);
    Block nb{lo, std::vector<uint8_t>(hi - lo)};
    if (pread(fd, nb.bytes.data(), hi - lo, (off_t)(r->off + (lo - r->va))) != (ssize_t)(hi - lo))
      return nullptr;
    auto pos = std::upper_bound(blocks.begin(), blocks.end(), lo, by_va);
    pos = blocks.insert(pos, std::move(nb));
    return pos->bytes.data() + (addr - lo);
  }
  void reset() const { blocks.clear(); }
};

struct PesMem {
  // Image bytes baked at build time: sorted (va, bytes) runs.
  struct Run { uint64_t va; std::vector<uint8_t> bytes; };
  std::vector<Run> img;
  // Writable scratch: sorted (base, bytes) regions (harness regions).
  struct Seg { uint64_t base; std::vector<uint8_t> bytes; };
  std::vector<Seg> segs;
  // Whole-file flat image view (run.py maps a flat dump: file offset = VA -
  // flat_base). A private copy-on-write mapping in the diff main, so writes
  // persist across cases as in the harness. Null means no image view.
  uint8_t* flat = nullptr;
  uint64_t flat_base = 0, flat_size = 0;
  // Snapshot mode (the diff main's snapshot replay): memory is the snapshot's pages (SnapView); null
  // otherwise. The write log (the diff main) records every byte a store writes, in order; null otherwise.
  SnapView* snap = nullptr;
  std::vector<std::pair<uint64_t, uint8_t>>* wlog = nullptr;
  void log_bytes(uint64_t addr, uint64_t v, unsigned n) {
    if (wlog)
      for (unsigned i = 0; i < n; ++i) wlog->push_back({addr + i, (uint8_t)(v >> (8 * i))});
  }
  // A host window onto GF's own memory (no copy): translated code addresses [base, base + size) by VA and
  // reads or writes the bytes in place. `ro` is read-only (writes are dropped); `rw` is writable.
  struct Window { uint64_t base = 0, size = 0; const uint8_t* ro = nullptr; uint8_t* rw = nullptr; };
  Window window;
  bool window_hit(uint64_t addr, unsigned n) const {
    return (window.ro || window.rw) && window.base <= addr && addr + n <= window.base + window.size;
  }
  const uint8_t* find(uint64_t addr, unsigned n) const {
    if (window_hit(addr, n)) return (window.rw ? window.rw : window.ro) + (addr - window.base);
    for (const auto& r : img) {
      if (r.va <= addr && addr + n <= r.va + r.bytes.size())
        return r.bytes.data() + (addr - r.va);
    }
    for (const auto& s : segs) {
      if (s.base <= addr && addr + n <= s.base + s.bytes.size())
        return s.bytes.data() + (addr - s.base);
    }
    if (flat && flat_base <= addr && addr + n <= flat_base + flat_size)
      return flat + (addr - flat_base);
    return nullptr;
  }
  uint8_t* wfind(uint64_t addr, unsigned n) {
    if (window_hit(addr, n)) return window.rw ? window.rw + (addr - window.base) : nullptr;
    for (auto& s : segs) {
      if (s.base <= addr && addr + n <= s.base + s.bytes.size())
        return s.bytes.data() + (addr - s.base);
    }
    if (flat && flat_base <= addr && addr + n <= flat_base + flat_size)
      return flat + (addr - flat_base);
    return nullptr;
  }
  uint64_t rd(uint64_t addr, unsigned n) const {
    if (snap) {
      uint64_t v = 0;
      for (unsigned i = 0; i < n; ++i)
        if (const uint8_t* b = snap->at(addr + i)) v |= (uint64_t)*b << (8 * i);
      return v;
    }
    const uint8_t* p = find(addr, n);
    uint64_t v = 0;
    if (p) std::memcpy(&v, p, n);
    return v;
  }
  void wr(uint64_t addr, uint64_t v, unsigned n) {
    if (snap) {
      for (unsigned i = 0; i < n; ++i)
        if (uint8_t* b = snap->at(addr + i)) {
          *b = (uint8_t)(v >> (8 * i));
          log_bytes(addr + i, v >> (8 * i), 1);
        }
      return;
    }
    if (uint8_t* p = wfind(addr, n)) {
      std::memcpy(p, &v, n);
      log_bytes(addr, v, n);
    }
  }
  float rdf(uint64_t addr) const {
    uint32_t b = (uint32_t)rd(addr, 4);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
  }
  void wrf(uint64_t addr, float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    wr(addr, b, 4);
  }
  double rdd(uint64_t addr) const {
    uint64_t u = rd(addr, 8);
    double d;
    std::memcpy(&d, &u, 8);
    return d;
  }
  void wrd(uint64_t addr, double d) {
    uint64_t u;
    std::memcpy(&u, &d, 8);
    wr(addr, u, 8);
  }
};

// Read n bytes at offset off of a baked read-only image window of len bytes
// (build-time bytes of PES's image). Offsets outside the window read 0; the
// window's bound comes from the translator's guard analysis (xlate.bounded_index).
static inline uint64_t ro_rd(const uint8_t* t, uint64_t len, uint64_t off,
                             unsigned n) {
  uint64_t v = 0;
  if (off <= len && n <= len - off) std::memcpy(&v, t + off, n);
  return v;
}

struct PesState {
  uint64_t gpr[16] = {};
  // The GS segment base (the thread environment block). gs:-relative operands read at gs_base + disp. The caller sets
  // it: the snapshot diff main from the same TEB the harness maps; the game's runtime sets the thread's TEB.
  uint64_t gs_base = 0;
  // XMM as raw u128: u64[2]; lane views reinterpreted per op.
  uint64_t xmm[16][2] = {};
  bool zf = false, pf = false, cf = false, sf = false, of = false;
  PesMem mem;
  // Emitted-code memory shorthands (forward to mem).
  uint64_t rd(uint64_t a, unsigned n) const { return mem.rd(a, n); }
  void wr(uint64_t a, uint64_t v, unsigned n) { mem.wr(a, v, n); }
  float rdf(uint64_t a) const { return mem.rdf(a); }
  void wrf(uint64_t a, float f) { mem.wrf(a, f); }
  double rdd(uint64_t a) const { return mem.rdd(a); }
  void wrd(uint64_t a, double d) { mem.wrd(a, d); }
  unsigned __int128 rd128(uint64_t a) const {
    return ((unsigned __int128)mem.rd(a + 8, 8) << 64) | mem.rd(a, 8);
  }
  void wr128(uint64_t a, unsigned __int128 v) {
    mem.wr(a, (uint64_t)v, 8);
    mem.wr(a + 8, (uint64_t)(v >> 64), 8);
  }
  static float f32(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
  }
  static uint32_t bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
  }
  static double f64(uint64_t u) {
    double d;
    std::memcpy(&d, &u, 8);
    return d;
  }
  float x_f32(int x) const {
    uint32_t b = (uint32_t)xmm[x][0];
    return f32(b);
  }
  void set_x_f32(int x, float f) {
    uint32_t b = bits(f);
    xmm[x][0] = (xmm[x][0] & 0xFFFFFFFF00000000ULL) | b;
  }
  double x_f64(int x) const {
    double d;
    uint64_t u = xmm[x][0];
    std::memcpy(&d, &u, 8);
    return d;
  }
  void set_x_f64(int x, double d) {
    uint64_t u;
    std::memcpy(&u, &d, 8);
    xmm[x][0] = u;
  }
  // COMISS/UCOMISS flags (exception behavior ignored; flags are identical).
  // NaN (either side) -> ZF=PF=CF=1.
  void cmp_ss(float a, float b) {
    if (__builtin_isnan(a) || __builtin_isnan(b)) {
      zf = pf = cf = true;
      sf = of = false;
      return;
    }
    zf = (a == b);
    cf = (a < b);
    pf = false;
    sf = of = false;
  }
  // COMISD/UCOMISD flags: the cmp_ss rules on doubles.
  void cmp_sd(double a, double b) {
    if (__builtin_isnan(a) || __builtin_isnan(b)) {
      zf = pf = cf = true;
      sf = of = false;
      return;
    }
    zf = (a == b);
    cf = (a < b);
    pf = false;
    sf = of = false;
  }
  // Packed 32-bit lanes of a 128-bit value: lane i is bits 32i..32i+31.
  static uint32_t lane_u32(unsigned __int128 v, int i) { return (uint32_t)(v >> (32 * i)); }
  static float lane_f32(unsigned __int128 v, int i) { return f32(lane_u32(v, i)); }
  static unsigned __int128 put_u32(unsigned __int128 v, int i, uint32_t x) {
    unsigned __int128 m = (unsigned __int128)0xFFFFFFFFu << (32 * i);
    return (v & ~m) | ((unsigned __int128)x << (32 * i));
  }
  // CVTTSD2SI: truncation; NaN or out of range gives the integer indefinite value.
  static int32_t cvtt_i32(double d) {
    if (!(d > -2147483649.0 && d < 2147483648.0)) return (int32_t)0x80000000u;
    return (int32_t)d;
  }
  static int64_t cvtt_i64(double d) {
    if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) return (int64_t)0x8000000000000000ull;
    return (int64_t)d;
  }
  bool cond(const char* cc) const;
};

// Canonical translated-function signature. Reads entry gpr/xmm, runs, leaves
// results in gpr[0] (rax) / xmm[0] lane 0, like hardware.
#define PES_FN(va) void pes_fn_##va(PesState& s)
