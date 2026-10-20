#include "onthepitch/pesroles.hpp"

#include <cstring>
#include <utility>

namespace pes::roles {
namespace {

constexpr uint64_t IMAGE_BASE = 0x140000000;
// FUN_1408C51F0's jump table: 13 dwords, each an RVA from the image base (dispatch 0x1408C5230).
constexpr uint64_t CLASS_MAP_TABLE_VA = 0x1408C52A4;
constexpr uint8_t MOV_R10D_IMM[2] = {0x41, 0xBA};                // MOV R10D, imm32
constexpr uint8_t MOV_EAX_R10D_RET[4] = {0x41, 0x8B, 0xC2, 0xC3};  // MOV EAX, R10D; RET
constexpr int kLastClass = 9;                                    // classes are 0..9

}  // namespace

int RegisteredRoleOf(e_PlayerRole role) {
  switch (role) {
    case e_PlayerRole_GK: return 0;
    case e_PlayerRole_CB: return 1;
    case e_PlayerRole_LB: return 2;
    case e_PlayerRole_RB: return 3;
    case e_PlayerRole_DM: return 4;
    case e_PlayerRole_CM: return 5;
    case e_PlayerRole_LM: return 6;
    case e_PlayerRole_RM: return 7;
    case e_PlayerRole_AM: return 8;
    case e_PlayerRole_LW: return 9;
    case e_PlayerRole_RW: return 10;
    case e_PlayerRole_SS: return 11;
    case e_PlayerRole_CF: return 12;
  }
  return 12;  // the sheet's default registered role (FUN_1408C51B0)
}

std::optional<std::array<int, kRegisteredRoles>> LoadRoleClassMap(const PeImage& exe,
                                                                  std::string* error) {
  std::array<int, kRegisteredRoles> classOf{};
  for (int role = 0; role < kRegisteredRoles; ++role) {
    uint32_t rva = 0;
    if (!exe.ReadVa(CLASS_MAP_TABLE_VA + 4 * role, sizeof rva, reinterpret_cast<uint8_t*>(&rva))) {
      *error = "role class map: jump table unreadable";
      return std::nullopt;
    }
    const uint64_t target = IMAGE_BASE + rva;
    uint8_t code[10] = {};
    if (!exe.ReadVa(target, sizeof code, code)) {
      *error = "role class map: a case is unreadable";
      return std::nullopt;
    }
    if (std::memcmp(code, MOV_R10D_IMM, sizeof MOV_R10D_IMM) == 0 &&
        std::memcmp(code + 6, MOV_EAX_R10D_RET, sizeof MOV_EAX_R10D_RET) == 0) {
      int32_t imm = 0;
      std::memcpy(&imm, code + 2, sizeof imm);
      classOf[role] = imm;
    } else if (std::memcmp(code, MOV_EAX_R10D_RET, sizeof MOV_EAX_R10D_RET) == 0) {
      classOf[role] = 0;  // the bare tail: R10D is zero from the function's entry
    } else {
      *error = "role class map: a case is not MOV R10D,imm32 then MOV EAX,R10D; RET";
      return std::nullopt;
    }
    if (classOf[role] < 0 || classOf[role] > kLastClass) {
      *error = "role class map: a class is outside 0..9";
      return std::nullopt;
    }
  }
  return classOf;
}

void WriteSideRoles(const std::array<int, kRegisteredRoles>& classOf, const TeamSheet& sheet,
                    int phase, SideRoles* roles) {
  for (int slot = 0; slot < kSideSlots; ++slot) {
    const int role = sheet.RegisteredRole(slot, phase);
    // FUN_1408C51F0: a registered role past 12 returns class 0 (its JA to the zero tail).
    const int cls = role >= 0 && role < kRegisteredRoles ? classOf[role] : 0;
    roles->a[slot] = cls;
    roles->b[slot] = cls;
  }
}

int WritePhase(bool flag) { return 2 - (flag ? 1 : 0); }

bool TailSwapAllowed(const SideRoles& roles) {
  return roles.lastSwapA < kSideSlots && roles.lastSwapB < kSideSlots;
}

void SwapB(SideRoles* roles, int a, int b) {
  if (a < 0 || a >= kSideSlots || b < 0 || b >= kSideSlots) return;
  std::swap(roles->b[a], roles->b[b]);
  roles->lastSwapA = a;
  roles->lastSwapB = b;
}

}  // namespace pes::roles
