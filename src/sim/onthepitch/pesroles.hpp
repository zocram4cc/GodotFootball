#pragma once

// PES 2021's per-side role-class arrays (docs/re/pes-re-role-writer.md). Each side keeps two
// arrays of eleven slots: A = S+0x3F8C (T+0x4BD4) and B = S+0x3FB8 (T+0x4C00), S = T+0xC48.
// Both hold the role CLASS 0..9 (GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9) of the slot's
// registered role, through FUN_1408C51F0's map. The bulk writer FUN_1404F5F10 stores A and B
// with the same value at every team tick; the tail swap FUN_140A422C0 exchanges two B entries.
// The off-ball and team readers take their class from B (SideRoles::b). A has no GF reader yet, and
// the stamina-sub reader (pessubs.cpp SubRoleClass) still maps the GF role itself: not ported.

#include <array>
#include <optional>
#include <string>

#include "gametypes.hpp"
#include "onthepitch/pesteamconcepts.hpp"
#include "pes/exe.hpp"

namespace pes::roles {

inline constexpr int kSideSlots = 11;                      // FUN_1408C51F0's `< 0xb` slot bound
inline constexpr int kRegisteredRoles = kSheetRoleBound;   // registered positions 0..12
inline constexpr int kSideBackClass = 2;                   // SB: LB and RB (FUN_1408C51F0 2/3)
inline constexpr int kKeeperClass = 0;                     // GK: the keeper selector reads B == 0
inline constexpr int kRunnerClassAbove = 4;                // FUN_14053E9A0: T+0x4C00 > 4
inline constexpr int kSupporterClassAbove = 2;             // FUN_140541010: T+0x4C00 > 2
inline constexpr int kReachClassBelow = 4;                 // FUN_140606CD0: reach path `< 4`
inline constexpr int kAttackSkipClassMax = 4;              // FUN_140606CD0: attack-level skip `<= 4`
inline constexpr int kAlwaysAdmitClass = 7;                // FUN_140500200: `< 7` is scored, else -100

// GF role -> PES registered position (0..12): GK0 CB1 LB2 RB3 DMF4 CMF5 LMF6 RMF7 AMF8 LWF9
// RWF10 SS11 CF12, the enumeration of docs/re/pes-re-matchlogic.md row 0x0e.
int RegisteredRoleOf(e_PlayerRole role);

// FUN_1408C51F0's class map, read from PES2021.exe: the jump table at 0x1408C52A4 holds 13
// RVAs, and each case is `MOV R10D,imm32` (41 ba imm32) falling into `MOV EAX,R10D; RET`
// (41 8b c2 c3). The entry for role 0 is the bare tail, where R10D is the zero the function
// starts with. An unexpected case byte fails the load.
std::optional<std::array<int, kRegisteredRoles>> LoadRoleClassMap(const PeImage& exe,
                                                                  std::string* error);

struct SideRoles {
  std::array<int, kSideSlots> a{};  // A (FUN_140A425A0): the stamina-sub class
  std::array<int, kSideSlots> b{};  // B (FUN_140A42590): the class the run, style and AI readers use
  // S+0x7D8C / S+0x7D90: the last tail swap's pair, which FUN_140A41BE0 checks before the next.
  int lastSwapA = 0;
  int lastSwapB = 0;
};

// FUN_1404F5F10's bulk write for one side: A[i] = B[i] = the class of slot i's registered role
// at `phase` of the current plan (FUN_1408C51F0 with plan 3). A role past 12 reads class 0.
void WriteSideRoles(const std::array<int, kRegisteredRoles>& classOf, const TeamSheet& sheet,
                    int phase, SideRoles* roles);

// The phase the bulk writer takes from its flag, `2 - (flag != 0)`.
int WritePhase(bool flag);

// FUN_140A41BE0: the tail swap runs only while its last pair is two slots (both latches < 11).
bool TailSwapAllowed(const SideRoles& roles);

// FUN_140A422C0's B half (flag != 0): exchanges B[a] and B[b] and latches the pair. A is never
// touched. PES reads B[0] for a slot past 10 and writes past B; GF refuses such a pair.
// ponytail: FUN_140A422C0 also swaps the two 12-byte slot-point rows (0x3A9C / 0x3B20) and its
// flag-0 call sites (set-piece paths) only latch; GF has no slot-point rows, so neither is ported.
void SwapB(SideRoles* roles, int a, int b);

}  // namespace pes::roles
