// Stub models for out-of-set PES callees (mirrors tools/dev/translate/stubs.py).
// Each model is the callee's observable effect on its caller.
#pragma once
#include <cmath>
#include <cstdint>

// sqrt_xmm0: xmm0 = sqrtf(xmm0). IEEE-754 sqrt is correctly rounded, so
// std::sqrt(float) gives the same bits as sqrtss. Negative input yields the
// x86 default NaN (sign set), which the caller's float compare observes.
static inline float pes_model_sqrtf(float x) { return std::sqrt(x); }
