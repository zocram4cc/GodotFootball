// PesState: the machine the committed translated functions run on.
// Mirrors tools/dev/diffharness/run.py: flat memory (image baked at VAs +
// caller regions), 16 GPRs, 16 XMM as u128, x86 flags. Bit-exact by
// construction: integer ops are width-tagged, float ops are SSE-semantic,
// comparisons set flags like COMISS/UCOMISS (NaN -> ZF=PF=CF=1).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

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
    const uint8_t* p = find(addr, n);
    uint64_t v = 0;
    if (p) std::memcpy(&v, p, n);
    return v;
  }
  void wr(uint64_t addr, uint64_t v, unsigned n) {
    if (uint8_t* p = wfind(addr, n)) std::memcpy(p, &v, n);
  }
  float rdf(uint64_t addr) const {
    float f = 0;
    const uint8_t* p = find(addr, 4);
    if (p) std::memcpy(&f, p, 4);
    return f;
  }
  void wrf(uint64_t addr, float f) {
    if (uint8_t* p = wfind(addr, 4)) std::memcpy(p, &f, 4);
  }
  double rdd(uint64_t addr) const {
    double d = 0;
    const uint8_t* p = find(addr, 8);
    if (p) std::memcpy(&d, p, 8);
    return d;
  }
  void wrd(uint64_t addr, double d) {
    if (uint8_t* p = wfind(addr, 8)) std::memcpy(p, &d, 8);
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
