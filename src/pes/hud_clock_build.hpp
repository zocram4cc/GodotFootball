// PES's HUD clock-state builder FUN_1420EFEC0 (0x1420EFEC0), run from the translated layer
// (src/pes/translated/hud/pes_fn_1420EFEC0.cpp, bit-exact on the hand spec's cases). GF fills the
// PES-layout objects the builder reads (the controller, the match env E, the MI record, the G
// global, the attention table), calls the translated function, and reads the 17-word output struct.
// The displayed-minute slot is OPEN (docs/re/pes-re-clock-record.md 2.2): this port hands back the
// words PES's code writes, in PES's order, and nothing else.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace pes {

class PeImage;

// The fields the builder reads from the match env E (param_1[7] of the controller) and MI.
struct ClockEnvInputs {
  float liveSeconds = 0.0f;     // E+0xA0: the live-half clock, in seconds (used while E+0xA4 == 0)
  float frozenSeconds = 0.0f;   // E+0x11C: the clock float when E+0xA4 != 0
  uint8_t liveFlag = 0;         // E+0xA4
  uint8_t clockStopped = 0;     // E+0x114: clock block hidden when non-zero
  uint8_t limitYellow[2] = {};  // E+0x9E..0x9F (clamped to 99 by the builder)
  uint8_t limitRed[2] = {};     // E+0xB0..0xB1 (clamped to 5 by the builder)
  uint8_t addedMinutes = 0;     // E+0x2DA8: added minutes (stoppage digits)
  uint32_t period = 0;          // MI+0x1168
  uint32_t inPlay = 0;          // MI+0x116C
  bool replayFlag = false;      // DAT_1437F968A (the replay flag the builder reads)
};

// The builder's output struct (param_2): 17 words, in PES's order.
using ClockStateWords = std::array<uint32_t, 17>;

class ClockStateBuilder {
 public:
  // The install supplies the .rdata and .data bytes the translated code reads (the builder's
  // constants and globals). Returns null and sets error when the image does not map them.
  static std::unique_ptr<ClockStateBuilder> Create(const PeImage& exe, std::string* error);
  ~ClockStateBuilder();
  ClockStateBuilder(const ClockStateBuilder&) = delete;
  ClockStateBuilder& operator=(const ClockStateBuilder&) = delete;

  ClockStateWords Build(const ClockEnvInputs& in);

 private:
  ClockStateBuilder();
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pes
