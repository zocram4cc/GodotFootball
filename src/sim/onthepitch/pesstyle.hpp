#pragma once

// PES 2021's playing-style gate, FUN_1408CB0B0 (docs/re/pes-re-playing-styles.md
// section 2, R1): a style bit in the player's style word AND the compat row of
// his role class. The rows are read from the install at 0x142613D20; the engine
// bits follow the binding table 0x1434A10D0 (section 0).

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "data/playingstyles.hpp"
#include "pes/exe.hpp"

namespace pes::style {

// The engine bit of a style, or -1 for None.
int EngineBit(PlayingStyles::Player style);

// The style word a player carries: that style's single bit, or 0 for None.
uint32_t StyleWord(PlayingStyles::Player style);

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
