// A TeamAi with fixed answers for the MOVESTEP tests. Each field is the PES read it stands for (see
// pes::TeamAi in src/pes/movestep.hpp). A case sets the fields it reads and leaves the rest at the
// answer of a player with no team context: no opponent, no role, no latch.
#pragma once

#include <array>

#include "pes/movestep.hpp"

struct SyntheticTeam : pes::TeamAi {
  // The ability bytes the tests read: Acceleration, PC and DA.
  static constexpr int kAbilityAcceleration = 0x2b;
  static constexpr int kAbilityPc = 0x28;
  static constexpr int kAbilityDa = 0x15;
  static constexpr int kSlots = 22;

  using Slots = std::array<int, kSlots>;
  using Flags = std::array<bool, kSlots>;
  using Point = std::array<std::array<float, 3>, kSlots>;

  int nearest = kSlots;  // no opponent unless a case names one
  Slots accel{};
  Slots pc{};
  Slots da{};
  int condition = 100;
  Slots subState{};
  Point position{};
  Point velocity{};
  std::array<float, kSlots> heading{};
  bool actionBit27 = true;
  Flags keeper{};
  Flags tackled{};
  Flags latched{};
  Flags carries{};
  Flags slot78{};
  Flags routeRow{};
  Flags matchUp{};
  Flags queued{};
  Flags marked{};
  Flags setPiece{};

  int NearestOpponent(int) const override { return nearest; }
  int Ability(int slot, int ability) const override {
    if (!Valid(slot)) return 0;
    if (ability == kAbilityAcceleration) return accel[slot];
    if (ability == kAbilityPc) return pc[slot];
    if (ability == kAbilityDa) return da[slot];
    return 0;
  }
  int Condition(int) const override { return condition; }
  void Position(int slot, float out[3]) const override { Copy(position, slot, out); }
  void Velocity(int slot, float out[3]) const override { Copy(velocity, slot, out); }
  float Heading(int slot) const override { return Valid(slot) ? heading[slot] : 0.0f; }
  int SubState(int slot) const override { return Valid(slot) ? subState[slot] : 0; }
  bool ActionBit27(int) const override { return actionBit27; }
  bool IsSideKeeper(int slot) const override { return Has(keeper, slot); }
  bool SideTackled(int slot) const override { return Has(tackled, slot); }
  bool PossessionLatched(int slot) const override { return Has(latched, slot); }
  bool Carries(int slot) const override { return Has(carries, slot); }
  bool IsSideSlot78(int slot) const override { return Has(slot78, slot); }
  bool HasRouteRow(int slot) const override { return Has(routeRow, slot); }
  bool IsMatchUpMan(int slot) const override { return Has(matchUp, slot); }
  bool IsQueuedCover(int slot) const override { return Has(queued, slot); }
  bool SideSlotMarked(int slot) const override { return Has(marked, slot); }
  bool SetPieceAnswer(int slot) const override { return Has(setPiece, slot); }

 private:
  static bool Valid(int slot) { return 0 <= slot && slot < kSlots; }
  static bool Has(const Flags& f, int slot) { return Valid(slot) && f[slot]; }
  static void Copy(const Point& p, int slot, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = Valid(slot) ? p[slot][i] : 0.0f;
  }
};

// The ramp's and the jostle's install scalars, as synthetic copies of the values the install holds at the
// VAs pes::RunConstants names. The install test checks the loaded ones through their relations.
inline pes::RunConstants SyntheticRampConstants() {
  pes::RunConstants k{};
  k.one = 1.0f;
  k.neg = -1.0f;
  k.half = 0.5f;
  k.deg1125 = 11.25f;
  k.deg45 = 45.0f;
  k.deg675 = 67.5f;
  k.deg90 = 90.0f;
  k.deg1125b = 112.5f;
  k.deg1575 = 157.5f;
  k.speed2 = 2.0f;
  k.speed3600 = 3.6f;
  k.speed4 = 4.0f;
  k.kmh60 = 60.0f;
  k.frame50 = 50.0f;
  k.rec40 = 40.0f;
  k.rec59 = 59.0f;
  k.rec99 = 99.0f;
  k.rate06 = 0.6f;
  k.rate07 = 0.7f;
  k.rate095 = 0.95f;
  k.blend2 = 0.2f;
  k.blend3 = 0.3f;
  k.blend1333 = 2.0f / 15.0f;
  k.replay45 = 0.45f;
  k.rampSlope = 0.15f;
  k.rampFloor = 1.1920929e-07f;
  k.fFree = 0.93f;
  k.fStepA = 0.05f;
  k.fTargetA = 0.88f;
  k.fStepB = 0.03f;
  k.fStepC = 0.04f;
  k.daLow = 55.0f;
  k.daHigh = 65.0f;
  k.daSpan = -10.0f;
  k.reserveLow = 20.0f;
  k.reserveHigh = 80.0f;
  k.reserveSpan = -60.0f;
  k.capSpan = 49.0f;
  k.accelAbove = 1.9f;
  k.accelBelow = 1.4f;
  k.decelSpan = 1.25f;
  k.decelSub = -3.0f;
  return k;
}
