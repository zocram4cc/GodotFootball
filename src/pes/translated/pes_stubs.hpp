// Stub models for out-of-set PES callees (mirrors tools/dev/translate/stubs.py).
// Each model is the callee's observable effect on its caller.
#pragma once
#include <cmath>
#include <cstdint>

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
