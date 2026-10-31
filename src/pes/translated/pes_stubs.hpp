// Stub models for out-of-set PES callees (mirrors tools/dev/translate/stubs.py).
// Each model is the callee's observable effect on its caller.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

#include "pes_state.hpp"

// sqrt_xmm0: xmm0 = sqrtf(xmm0). IEEE-754 sqrt is correctly rounded, so
// std::sqrt(float) gives the same bits as sqrtss. Negative input yields the
// x86 default NaN (sign set), which the caller's float compare observes.
static inline float pes_model_sqrtf(float x) { return std::sqrt(x); }

// logf_xmm0 / powf_xmm0: xmm0 = logf(xmm0) / powf(xmm0, xmm1), the host C library. Every special value (zero,
// negative, NaN, infinity, denormal arguments; exact powers; overflow and underflow) returns the bits the PES-loaded
// ucrtbase returns (tools/dev/translate/test_libm_models.py, run against the snapshot's ucrtbase code).
// ponytail: an ordinary argument can differ from ucrtbase by one ulp (test_libm_models.py's fixed sample: 28 of 8,013
// logf inputs, 10 of 6,551 powf inputs, among them powf results next to the smallest denormal that ucrtbase rounds to
// 0). The removal is ucrtbase's own double-precision algorithm and tables, read from the user's ucrtbase module.
static inline float pes_model_logf(float x) { return std::log(x); }
static inline float pes_model_powf(float x, float y) { return std::pow(x, y); }

// isnan_xmm0: rax = ucrtbase's _isnan(double xmm0): 1 when the magnitude bits exceed the infinity's, else 0 (exact; the
// export's own integer test). floorf_xmm0: xmm0 = floorf(xmm0), exact in every libm.
static inline uint64_t pes_model_isnan(double x) {
  uint64_t b;
  std::memcpy(&b, &x, 8);
  return (b & 0x7FFFFFFFFFFFFFFFULL) > 0x7FF0000000000000ULL ? 1 : 0;
}
static inline float pes_model_floorf(float x) { return std::floor(x); }

// sinf / cosf / tanf / asinf / atan2f (xmm0, xmm1 for atan2f): the host C library, reached through the thunks 0x1415A1792 /
// 0x1415A178C / 0x1415A17D4 / 0x1415A179E / 0x1415A17A4.
// ponytail: an ordinary argument can differ from ucrtbase's bits by one ulp; the removal is ucrtbase's own algorithm read
// from the user's ucrtbase module (not measured here). Special values follow C99 Annex F.
static inline float pes_model_sinf(float x) { return std::sin(x); }
static inline float pes_model_cosf(float x) { return std::cos(x); }
static inline float pes_model_tanf(float x) { return std::tan(x); }
static inline float pes_model_asinf(float x) { return std::asin(x); }
static inline float pes_model_atanf(float x) { return std::atan(x); }
static inline float pes_model_acosf(float x) { return std::acos(x); }
static inline float pes_model_atan2f(float y, float x) { return std::atan2(y, x); }

// vsprintf_s: ucrtbase's __stdio_common_vsprintf_s, the import model of tools/dev/translate/vsprintf_model.s register for register
// where the caller can see it: the character count in rax, the output with its NUL, and the stack bytes the model's pushes
// leave (rbx, rsi, rdi, then each number's digits, least significant first, 8 bytes each). Win64: rdx = buffer, r8 = count,
// r9 = format, [rsp + 0x30] at the model's entry = the va_list (a pointer to 8-byte argument slots). Anything the model does
// not support is a trap, as its int3 is.
static inline void pes_model_vsprintf_s(PesState& s) {
  const uint64_t rsp0 = s.gpr[4];  // the model's entry rsp: [rsp] is the return address
  s.wr(rsp0 - 8, s.gpr[3], 8);
  s.wr(rsp0 - 16, s.gpr[6], 8);
  s.wr(rsp0 - 24, s.gpr[7], 8);
  const uint64_t buf = s.gpr[2], count = s.gpr[8];
  uint64_t fmt = s.gpr[9], va = s.rd(rsp0 + 0x30, 8), out = buf;
  if (count == 0) __builtin_trap();
  const uint64_t last = buf + count - 1;
  auto put = [&](uint8_t c) {
    if (out >= last) __builtin_trap();
    s.wr(out++, c, 1);
  };
  auto next = [&]() { return (uint8_t)s.rd(fmt++, 1); };
  for (uint8_t c = next(); c != 0; c = next()) {
    if (c != '%') {
      put(c);
      continue;
    }
    uint32_t flags = 0;  // the model's ebx: bit 8 = the zero flag, the low byte = the width
    c = next();
    if (c == '%') {
      put('%');
      continue;
    }
    if (c == '0') {
      flags = 0x100;
      c = next();
    }
    for (; c >= '0' && c <= '9'; c = next()) flags = (flags & 0x100) | ((flags & 0xFF) * 10 + (c - '0'));
    if (c == 's') {
      if (flags & 0xFF) __builtin_trap();
      uint64_t p = s.rd(va, 8);
      va += 8;
      if (p == 0) __builtin_trap();
      for (uint8_t ch; (ch = (uint8_t)s.rd(p, 1)) != 0; ++p) put(ch);
      continue;
    }
    if (c != 'd' && c != 'i' && c != 'u' && c != 'x') __builtin_trap();
    uint32_t v = (uint32_t)s.rd(va, 4);
    va += 8;
    if ((c == 'd' || c == 'i') && (int32_t)v < 0) {
      put('-');
      v = 0u - v;
      if (flags & 0xFF) {
        if (!(flags & 0x100)) __builtin_trap();
        --flags;
      }
    }
    const uint32_t base = c == 'x' ? 16 : 10;
    uint32_t n = 0;
    uint64_t sp = rsp0 - 24;
    do {
      uint32_t d = v % base;
      v /= base;
      sp -= 8;
      s.wr(sp, d < 10 ? d + '0' : d + 'a' - 10, 8);
      ++n;
    } while (v != 0);
    for (uint32_t pad = (flags & 0xFF) > n ? (flags & 0xFF) - n : 0; pad != 0; --pad) put(flags & 0x100 ? '0' : ' ');
    for (; n != 0; --n, sp += 8) put((uint8_t)s.rd(sp, 8));
  }
  s.wr(out, 0, 1);
  s.gpr[0] = out - buf;
}
