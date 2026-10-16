#pragma once

// Locomotion blend data as PES plays it (docs/re/pes-re-animselect.md): the parametric blend table
// (Parametricblendtable.bin) and its groups, e.g. group 80 walk_1_1 .. run_5_5, all members at one
// normalised phase. The viewer's member pair and phase come from the MOVESTEP rate.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pes/exe.hpp"
#include "pes/vfs.hpp"

namespace pes {

inline constexpr int LOCOMOTION_BLEND_GROUP = 80;  // walk_1_1 0 .. run_5_5 1 (report 2.6)

struct BlendMember {
  int motionNo;
  int group;
  bool mirror;
  float param;
};

// 8-byte records: u32 bits 0-11 motionNo - 0x520, 12-18 group, 19 mirror; f32 parameter (FUN_1406b2950).
std::vector<BlendMember> ParseBlendTable(const std::vector<uint8_t>& bytes);
std::optional<std::vector<BlendMember>> LoadBlendTable(const PesVfs& vfs, std::string* error);
std::vector<BlendMember> BlendGroup(const std::vector<BlendMember>& table, int group);  // sorted by param

struct Bracketed {
  int lo, hi;
  float t;  // weight of hi
};
// The two members around `x` on a sorted axis, clamped to its ends.
Bracketed Bracket(const std::vector<float>& axis, float x);

}  // namespace pes
