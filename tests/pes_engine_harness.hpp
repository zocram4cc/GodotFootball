// Test harness for the engine's frame (AnimeLayer7). The trap path no longer runs a fixed phase list: it runs PES's own driver
// FUN_141EB5E70 (PesWorld::RunAdjusters), which runs the chain's phases itself. The engine tests keep the list they measured (the
// frame order of BodyAnimControl::Exec, as the fox world ran it), so their poses stay comparable.
#pragma once

#include <cstdint>

#include "onthepitch/pesfoxworld.hpp"
#include "onthepitch/pestrapworld.hpp"
#include "pes/translated/translated.hpp"

namespace pes_test {

// One engine frame of the player at `p`: BodyAnimControl::Exec (FUN_141AF4650) for each phase of the measured list, in order.
inline void RunEngineFrame(pes::PesFoxWorld& world, uint64_t p) {
  constexpr uint32_t kPhases[] = {1, 2, 3, 4, 5, 7, 8, 9, 0xB, 0xD, 0xF, 0x10, 0x11};
  const uint64_t a = world.Read64(p + pes::kPScalePtrOffset);
  for (uint32_t phase : kPhases) world.Call(::pes_fn_141AF4650, {a, phase});
}

}  // namespace pes_test
