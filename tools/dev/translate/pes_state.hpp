// PesState: the machine the translated functions run on (build tree only).
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
  const uint8_t* find(uint64_t addr, unsigned n) const {
    for (const auto& r : img) {
      if (r.va <= addr && addr + n <= r.va + r.bytes.size())
        return r.bytes.data() + (addr - r.va);
    }
    for (const auto& s : segs) {
      if (s.base <= addr && addr + n <= s.base + s.bytes.size())
        return s.bytes.data() + (addr - s.base);
    }
    return nullptr;
  }
  uint8_t* wfind(uint64_t addr, unsigned n) {
    for (auto& s : segs) {
      if (s.base <= addr && addr + n <= s.base + s.bytes.size())
        return s.bytes.data() + (addr - s.base);
    }
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
  bool cond(const char* cc) const;
};

// Canonical translated-function signature. Reads entry gpr/xmm, runs, leaves
// results in gpr[0] (rax) / xmm[0] lane 0, like hardware.
#define PES_FN(va) void pes_fn_##va(PesState& s)
