#pragma once

// PES 2021's random number generator, math::Random (docs/re/pes-re-rng.md 1).
// Four 32-bit slots: slots 0..2 step a 31-bit LCG,
//   s = (sub - s*mul) & 0x7FFFFFFF,
// and map through a per-slot modulus table; slot 3 is a separate 15-bit LCG,
//   s = (mul3*s + inc3) & mask3.
// Every constant is read from the user's PES2021.exe (stock bytes) through
// pes::PeImage::ReadVa: the instruction immediates of the draw function and
// the constructor, and the modulus table and 2^-31 scale from .rdata.
//
// Seeding (report 2.1): the match-context constructor commits one value t at
// kick-off as slots 0, 1, 2 = t, t+1, t+2; slot 3 keeps its constructor
// default. MatchSetup::seed is that t.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

inline constexpr int kRandomSlots = 4;
inline constexpr int kWeakSlot = 3;  // the 15-bit LCG slot
// The slot Range0 draws (0x140a92530's slot path, pesrandom.cpp's MATCH_SLOT):
// PES's per-system draw sites take their own slot (report 3), and this is the one
// Range0 always uses.
inline constexpr int kRange0Slot = 1;

// RandomInfo's draws (report 3.14, 0x140435ef0): the producer's generator slot, and
// the two moduli PES gives its percent and coin draws (`mov edx, 0x64`, then
// `lea r8d, [rdx-0x62]`, which is 2). Its unit draw's modulus is read from the exe.
inline constexpr int kRandomInfoSlot = 2;
inline constexpr int kRandomInfoPercentModulus = 100;
inline constexpr int kRandomInfoCoinModulus = 2;
// The producer's slot-2 seed is the match value L times this (0x140435e10 scales L by 3000
// on slot 2; slots 0 and 1 take 1000 and 2000 there, and the producer draws only slot 2).
inline constexpr uint32_t kRandomInfoSeedScale = 3000;

struct RngConstants {
  uint32_t lcgMul;
  uint32_t lcgSub;
  std::array<int32_t, kRandomSlots> modulus;
  float scale;  // 2^-31
  uint32_t weakMul, weakInc, weakMask;
  std::array<uint32_t, kRandomSlots> defaultSeeds;
  // RandomInfo's unit draw (report 3.14): RangeRaw(slot 2, randomInfoModulus) divided by
  // randomInfoDivisor. The modulus is the immediate of the producer's `mov edx, 999999`
  // (0x140435f34) and the divisor is its .rdata float (0x14259bc10).
  int32_t randomInfoModulus;
  float randomInfoDivisor;
};

std::optional<RngConstants> LoadRngConstants(const PeImage& exe, std::string* error);

class Random {
 public:
  explicit Random(const RngConstants& k);

  void SetSeed(int slot, uint32_t value) { slots_[slot] = value; }
  uint32_t State(int slot) const { return slots_[slot]; }

  // The 31-bit step of slots 0..2 (0x140a92470's slot path); returns the new state.
  uint32_t Step(int slot);
  // Range(rng, slot) 0x140a92470: the step mapped to [0, modulus[slot]).
  int Range(int slot);
  // RangeRaw 0x140a924e0: the step's new state as a float in [0, 1).
  float Unit(int slot);
  // RangeRaw's other half, 0x140a924e0: `trunc(u * M)` on an explicit M, not
  // the slot's modulus entry. This is the injury roll's draw: PES passes
  // `Range(rng, 100, 2)` through it, so the modulus is 100, not modulus[2].
  int RangeRaw(int slot, int modulus);
  // Range0(rng, count) 0x140a92530: slot 1 mapped to [0, count).
  int Range0(int count);
  // Slot 3's bare advance (the 0x140a92470 idx == 3 branch).
  void AdvanceWeak();

 private:
  const RngConstants& k_;
  std::array<uint32_t, kRandomSlots> slots_;
};

// RandomInfo, the record the producer republishes (report 3.14, 0x140435ef0). One publish is
// four draws on slot 2, in this order: Range into `draw`, RangeRaw(100) into `percent`,
// RangeRaw(2) into `coin`, and RangeRaw(randomInfoModulus) / randomInfoDivisor into `unit`
// (+0x8, the value a carrier's seed member copies).
struct RandomInfo {
  int draw = 0;       // +0x0
  int percent = 0;    // +0x4
  int coin = 0;       // +0x5
  float unit = 0.0f;  // +0x8
};
RandomInfo PublishRandomInfo(Random& rng, const RngConstants& k);

// The producer behind RandomInfo (report 3.14-3.15, 0x1404356e0 and 0x140435ef0). It owns one
// generator and republishes its record on every logic frame. Its creation draws happen in the
// constructor, before the first frame; no frame reads the record before then.
class RandomInfoProducer {
 public:
  // `matchSeed` stands in for the match value L that PES seeds the generator from (the ponytail
  // at the match's constructor).
  RandomInfoProducer(const RngConstants& k, uint32_t matchSeed);
  RandomInfoProducer(const RandomInfoProducer&) = delete;
  RandomInfoProducer& operator=(const RandomInfoProducer&) = delete;

  // One logic frame: the four draws, in PES's order, replace the record.
  void Tick();
  const RandomInfo& Record() const { return record_; }

 private:
  RngConstants k_;  // a copy: Random keeps a reference to the constants it is built from
  Random rng_;
  RandomInfo record_;
};

// The kick-off seeding of the match generator (report 2.1).
void SeedAtKickoff(Random& rng, uint32_t t);

// A player's own generator (slot 2 of player+0x32d0) is seeded once, at the player's
// construction, with (t + slot) % 10000 (FUN_1406eb670, rng 2.4). `t` is the clock reading
// PES takes there; `slot` is the player's AI record index (P+0x2458).
uint32_t PlayerSeed(uint32_t t, int slot);

}  // namespace pes
