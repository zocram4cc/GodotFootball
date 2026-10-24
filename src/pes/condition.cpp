#include "pes/condition.hpp"

#include "pes/exe.hpp"

#include <algorithm>
#include <cstdio>

namespace pes {
namespace {

// FUN_140AF2DA0's ladder (condition-roll §0): `cmp eax, imm8` is 83 F8 ib.
struct Cmp {
  uint64_t va;
  uint8_t opcode[2];
};
constexpr Cmp kBandCmps[ConditionConstants::kBandEdges] = {
    {0x140AF2EAC, {0x83, 0xF8}}, {0x140AF2EB1, {0x83, 0xF8}}, {0x140AF2EBD, {0x83, 0xF8}},
    {0x140AF2EC9, {0x83, 0xF8}}, {0x140AF2ED5, {0x83, 0xF8}}, {0x140AF2EE1, {0x83, 0xF8}},
    {0x140AF2EED, {0x83, 0xF8}},
};
// The multiplier table (condition-arrows §Confirmed: 0x142632D70, read at
// 0x14092B5D4, indexed 5*tier+arrow).
constexpr uint64_t kArrowTableVa = 0x142632D70;

bool ReadCmpImm(const PeImage& exe, const Cmp& cmp, uint8_t* out, std::string* error) {
  uint8_t code[3] = {};
  if (!exe.ReadVa(cmp.va, sizeof code, code) || code[0] != cmp.opcode[0] || code[1] != cmp.opcode[1]) {
    char va[32];
    std::snprintf(va, sizeof va, "%#llx", static_cast<unsigned long long>(cmp.va));
    *error = std::string("condition constants: unexpected instruction at ") + va;
    return false;
  }
  *out = code[2];
  return true;
}

}  // namespace

std::optional<ConditionConstants> LoadConditionConstants(const PeImage& exe, std::string* error) {
  ConditionConstants k{};
  for (int i = 0; i < ConditionConstants::kBandEdges; ++i)
    if (!ReadCmpImm(exe, kBandCmps[i], &k.bandEdges[size_t(i)], error)) return std::nullopt;
  if (!exe.ReadVa(kArrowTableVa, sizeof k.arrowTable, reinterpret_cast<uint8_t*>(k.arrowTable.data()))) {
    if (error) *error = "condition constants: multiplier table unreadable";
    return std::nullopt;
  }
  return k;
}

int ConditionBand(const ConditionConstants& k, unsigned int conditionByte) {
  if (conditionByte < k.bandEdges[0]) return 0;
  for (int i = 1; i < ConditionConstants::kBandEdges; ++i)
    if (conditionByte < k.bandEdges[size_t(i)]) return i;
  return 7;
}

int ConditionArrow(int band) {
  // FUN_141FC1380: 0->4, 1->3, 2->2, 3->1, 4->0, else 2.
  switch (band) {
    case 0: return 4;
    case 1: return 3;
    case 3: return 1;
    case 4: return 0;
    default: return 2;  // bands 2, 5, 6, 7
  }
}

int ApplyConditionShift(int pesArrow, bool ownTeam, bool teamModeThree, bool ownLevelClassSix,
                        bool oppLevelClassSix) {
  (void)ownLevelClassSix;  // the opponent path tests the other side (0x14092B516 R12D^1)
  int arrow = pesArrow;
  if (ownTeam) {
    if (teamModeThree) arrow = 0;  // 0x14092B4EF cmp eax,3 / je -> mov ebx,r14d
  } else if (oppLevelClassSix && arrow < 4) {
    arrow += 1;  // 0x14092B57B cmp ebx,4 / 0x14092B580 inc ebx
  }
  return arrow;
}

bool ConditionArrowWritesStats(int pesArrow) { return pesArrow != 2 && pesArrow < 5; }

float ConditionMultiplier(const ConditionConstants& k, int tier, int pesArrow) {
  return k.arrowTable[size_t(std::clamp(tier, 0, ConditionConstants::kArrowTiers - 1))]
                     [size_t(std::clamp(pesArrow, 0, ConditionConstants::kArrowRows - 1))];
}

}  // namespace pes
