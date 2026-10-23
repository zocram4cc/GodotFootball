#pragma once

// PES 2021's condition byte and arrow chain (docs/re/pes-re-condition-import.md,
// pes-re-condition-roll.md, pes-re-team-status.md).
//
// Every number is read from the user's own PES2021.exe through
// pes::PeImage::ReadVa - the .rdata tables and the band ladder's compare
// immediates, each checked against its opcode bytes before the value is
// trusted. Nothing here is transcribed.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace pes {

class PeImage;

// FUN_140AF2DA0's 7-edge ladder (CMP EAX,imm: 0x41/JC then 0x46/4B/50/55/5A/5F
// at 0x140AF2EAC-0x140AF2EED) through FUN_141FC1380's inverse map to the arrow,
// and FUN_14092AF10's apply-time shift, all read from the install.
struct ConditionConstants {
  static constexpr int kBandEdges = 7;
  static constexpr int kArrowRows = 5;
  static constexpr int kArrowTiers = 3;
  std::array<uint8_t, kBandEdges> bandEdges{};  // 0x41 46 4B 50 55 5A 5F
  // The multiplier table 0x142632D70: 3 tiers x 5 PES arrows (0 Top..4 Terrible).
  std::array<std::array<float, kArrowRows>, kArrowTiers> arrowTable{};
};

std::optional<ConditionConstants> LoadConditionConstants(const PeImage& exe, std::string* error);

// The band byte the per-player condition byte deals (0..7), and the PES arrow
// index (0 Top..4 Terrible) the band deals through the inverse map.
int ConditionBand(const ConditionConstants& k, unsigned int conditionByte);
int ConditionArrow(int band);

// FUN_14092AF10 (0x14092B4CE-0x14092B5DB) in PES space: the own team with
// T+0xA298 state 3 takes index 0, the opponent one step worse capped at 4, both
// gated by the /100 == 6 level-class predicate (docs/re/pes-re-team-status.md).
int ApplyConditionShift(int pesArrow, bool ownTeam, bool teamModeThree, bool levelClassSix);

// Green x1.0 and out-of-range indices write nothing (0x14092B59B/5A0).
bool ConditionArrowWritesStats(int pesArrow);

// The 0x142632D70 multiplier for a stat tier and a PES arrow index.
float ConditionMultiplier(const ConditionConstants& k, int tier, int pesArrow);

}  // namespace pes
