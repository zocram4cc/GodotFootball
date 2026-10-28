// Lockstep trace hook: compiled only into a snapdiff build with GF_XLATE_TRACE=1 (snapdiff.py adds it with -include).
// The translator emits GF_TRACE_PC(va, s) before each translated instruction in that build. Each record is the PC
// then the pre-instruction state: gpr[16] and xmm[16][2] (u64 pairs), 392 bytes, written to $GF_TRACE_OUT (stdout
// when unset). snapcase.py writes the same record layout from the harness. Committed code never contains this.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>

inline FILE* gf_trace_f = nullptr;  // inline: one stream for every translation unit of the binary

template <class S>
inline void gf_trace_pc(uint64_t pc, const S& s) {
  if (!gf_trace_f) {
    const char* p = std::getenv("GF_TRACE_OUT");
    gf_trace_f = p ? std::fopen(p, "wb") : stdout;
  }
  std::fwrite(&pc, 8, 1, gf_trace_f);
  std::fwrite(s.gpr, 8, 16, gf_trace_f);
  std::fwrite(&s.xmm[0][0], 8, 32, gf_trace_f);
}

#define GF_TRACE_PC(va, st) gf_trace_pc(static_cast<uint64_t>(va), st)
