#pragma once

// PES 2021's playing-style gate, FUN_1408CB0B0 (docs/re/pes-re-playing-styles.md
// section 2, R1): a style bit in the player's style word AND the compat row of
// his role class. The rows are read from the install at 0x142613D20; the engine
// bits follow the binding table 0x1434A10D0 (section 0).

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes::style {

// PES's playing styles by their bit in the roster row's style word (row +0x48): the binding table 0x1434A10D0, which
// editsave.hpp's kEditStyleMaskBit reaches from the save's style value. A player has the bits his row sets: no GF style
// enum stands between the row and the gate.
enum class Bit : int {
  GoalPoacher = 0,
  DummyRunner,
  FoxInTheBox,
  TargetMan,
  CreativePlaymaker,
  ProlificWinger,
  RoamingFlank,
  CrossSpecialist,
  ClassicNo10,
  HolePlayer,
  BoxToBox,
  AnchorMan,
  TheDestroyer,
  Orchestrator,
  BuildUp,
  ExtraFrontman,
  OffensiveFullBack,
  DefensiveFullBack,
  FullBackFinisher,
  OffensiveGoalkeeper,
  DefensiveGoalkeeper,
};
constexpr int EngineBit(Bit style) { return static_cast<int>(style); }
constexpr uint32_t StyleWord(Bit style) { return 1u << EngineBit(style); }

// Row r lists the styles a player of role class r may use. Role classes are
// PES's (FUN_1408c51f0): GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9.
struct Compat {
  std::array<uint32_t, 10> row{};
};

// Reads the ten rows at 0x142613D20. Fails when the table is all zero.
std::optional<Compat> LoadCompat(const PeImage& exe, std::string* error);

// PES's gate: the style bit is on the player and his role's row lists it.
bool Gate(const Compat& compat, int roleClass, uint32_t styleWord, int bit);

}  // namespace pes::style
