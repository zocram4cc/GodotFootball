#include "pes/hud_clock.hpp"

#include <cstring>

#include "pes_state.hpp"

void pes_fn_140A6BD80(PesState& s);

namespace pes {

HmsSplit SplitHms(float totalSeconds) {
  // The two results sit in one host window: hours at the rdx VA, the minute 4 bytes after it.
  constexpr uint64_t kHoursVa = 0x20000000;
  constexpr uint64_t kMinuteVa = kHoursVa + 4;
  uint8_t out[8] = {};

  PesState s;
  s.mem.window.base = kHoursVa;
  s.mem.window.size = sizeof out;
  s.mem.window.rw = out;
  s.set_x_f32(0, totalSeconds);
  s.gpr[2] = kHoursVa;   // rdx: the hours result
  s.gpr[8] = kMinuteVa;  // r8: the minute result
  pes_fn_140A6BD80(s);

  HmsSplit split;
  std::memcpy(&split.hours, out, 4);
  std::memcpy(&split.minute, out + 4, 4);
  return split;
}

}  // namespace pes
