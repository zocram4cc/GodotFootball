// Stub models for out-of-set PES callees (mirrors tools/dev/translate/stubs.py).
// Each model is the callee's observable effect on its caller.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

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
