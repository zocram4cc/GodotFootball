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

// sinf_xmm0 / cosf_xmm0: xmm0 = sinf(xmm0) / cosf(xmm0), the host C library (the key unpacker 0x1423BA9D0 reaches them through
// the thunks 0x1415A1792 / 0x1415A178C). Special values follow C99 Annex F (test_libm_models.py).
// ponytail: an ordinary argument can differ from ucrtbase's bits by one ulp (test_libm_models.py against the snapshot's
// ucrtbase: sinf 47 of 4,521 inputs, cosf 41 of 4,521, never more than one ulp, every special value bit-identical); the
// removal is ucrtbase's own algorithm read from the user's ucrtbase module.
static inline float pes_model_sinf(float x) { return std::sin(x); }
static inline float pes_model_cosf(float x) { return std::cos(x); }

// acosf_xmm0: xmm0 = acosf(xmm0), the host C library (the kind feasibility test 0x140829E80 reaches it through the thunk
// 0x1415A1798). Special values are bit-identical to ucrtbase's.
// ponytail: as sinf/cosf: 76 of 6,021 ordinary inputs differ from ucrtbase by one ulp; same removal.
static inline float pes_model_acosf(float x) { return std::acos(x); }
