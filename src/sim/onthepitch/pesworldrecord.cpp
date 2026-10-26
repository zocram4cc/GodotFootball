// PES's world object's table accessors, run in place. 0x140A48910 (anime) and 0x140A48A10 (man) answer the VA of the
// record an entry names, and fall back to entry 0 past the table's count, as PES does. GF's readers of the two tables
// run them on the world window.

#include <cstdint>

#include "onthepitch/pesworldrecord.hpp"
#include "pes/translated/translated.hpp"
#include "pes_state.hpp"

namespace pes {
namespace {

// rcx = G (the window's base VA), edx = the slot index. An int argument in edx zero-extends into rdx; the answer is rax.
uint64_t Answer(void (*accessor)(PesState&), uint8_t* world, int index) {
  PesState s;
  s.mem.window.base = kWorldWindowVa;
  s.mem.window.size = kWorldBytes;
  s.mem.window.rw = world;
  s.gpr[1] = kWorldWindowVa;
  s.gpr[2] = static_cast<uint32_t>(index);
  accessor(s);
  return s.gpr[0];
}

uint8_t* InWindow(uint8_t* world, uint64_t va) {
  if (va < kWorldWindowVa || va >= kWorldWindowVa + kWorldBytes) return nullptr;
  return world + (va - kWorldWindowVa);
}

// PES's bearing 0x140439EF0 on the window: rcx = from, rdx = to, the heading in xmm0 (coincident reads 0, else the fold
// of atan2(to.x - from.x, to.z - from.z)). The points are read in place; the stack is the scratch's.
float RunBearing(uint8_t* world, uint64_t fromVa, uint64_t toVa) {
  PesState s;
  s.mem.window.base = kWorldWindowVa;
  s.mem.window.size = kWorldBytes;
  s.mem.window.rw = world;
  s.gpr[1] = fromVa;
  s.gpr[2] = toVa;
  s.gpr[4] = kScratchVa + kScratchBytes;
  ::pes_fn_140439EF0(s);
  return s.x_f32(0);
}

}  // namespace

uint8_t* PesWorld::AnimeRecord(int slot) { return InWindow(Bytes(), Answer(&::pes_fn_140A48910, Bytes(), slot)); }

uint8_t* PesWorld::ManRecord(int slot) { return InWindow(Bytes(), Answer(&::pes_fn_140A48A10, Bytes(), slot)); }

float PesWorld::Bearing(const uint8_t* from, const uint8_t* to) {
  return RunBearing(Bytes(), VaOf(from) + kManPosXOffset, VaOf(to) + kManPosXOffset);
}

float PesWorld::BearingOfPoints(float fromX, float fromZ, float toX, float toZ) {
  uint8_t* points = Bytes() + (kScratchVa - kWorldWindowVa);
  StoreF32(points, kPointXOffset, fromX);
  StoreF32(points, kPointZOffset, fromZ);
  StoreF32(points, kPointBytes + kPointXOffset, toX);
  StoreF32(points, kPointBytes + kPointZOffset, toZ);
  return RunBearing(Bytes(), kScratchVa, kScratchVa + kPointBytes);
}

}  // namespace pes
