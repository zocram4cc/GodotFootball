// PES's HMS helper FUN_140A6BD80, run from the translated layer (src/pes/translated/hud/
// pes_fn_140A6BD80.cpp, bit-exact on 64 recorded cases). The translated code writes its two
// results through the pointers in rdx and r8, so SplitHms gives those pointers a host window
// at a PES VA: the code reads and writes the same bytes PES would.
#pragma once

#include <cstdint>

namespace pes {

struct HmsSplit {
  uint32_t hours = 0;   // seconds / 3600: the rdx result
  uint32_t minute = 0;  // (seconds / 60) % 60: the r8 result
};

// The count is truncated toward zero, as the translated code's float-to-int step does.
HmsSplit SplitHms(float totalSeconds);

}  // namespace pes
