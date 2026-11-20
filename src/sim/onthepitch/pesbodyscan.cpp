#include "onthepitch/pesbodyscan.hpp"

#include <cstring>

namespace pes {
namespace {

// FUN_141EB54D0 (the joint-block mask): `cmp edx,0x13` bounds the bone, `lea rcx,[rip+disp]` names the table, `movzx eax,[rax+rcx]` reads it.
constexpr uint64_t kBoundVa = 0x141EB54D0;
constexpr uint8_t kBoundOpcode[] = {0x83, 0xFA, static_cast<uint8_t>(kScanBoneCount)};
constexpr uint64_t kLeaVa = 0x141EB54DB;
constexpr uint8_t kLeaOpcode[] = {0x48, 0x8D, 0x0D};
constexpr std::size_t kLeaDispOffset = 3, kLeaLength = 7;

}  // namespace

bool LoadJointMask(const PeImage& exe, std::array<bool, kScanBoneCount>* mask, std::string* error) {
  uint8_t bound[sizeof kBoundOpcode], lea[sizeof kLeaOpcode];
  int32_t disp = 0;
  const bool ok = exe.ReadVa(kBoundVa, sizeof bound, bound) && std::memcmp(bound, kBoundOpcode, sizeof bound) == 0 &&
                  exe.ReadVa(kLeaVa, sizeof lea, lea) && std::memcmp(lea, kLeaOpcode, sizeof lea) == 0 &&
                  exe.ReadVa(kLeaVa + kLeaDispOffset, sizeof disp, reinterpret_cast<uint8_t*>(&disp));
  uint8_t table[kScanBoneCount] = {};
  const uint64_t at = kLeaVa + kLeaLength + static_cast<uint64_t>(static_cast<int64_t>(disp));
  if (!ok || !exe.ReadVa(at, sizeof table, table)) {
    if (error != nullptr) *error = "PES image: the joint-block mask's instructions did not match their opcodes";
    return false;
  }
  for (std::size_t i = 0; i < kScanBoneCount; ++i) (*mask)[i] = table[i] != 0;
  return true;
}

}  // namespace pes
