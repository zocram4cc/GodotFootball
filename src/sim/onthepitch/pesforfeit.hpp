// PES 2021's minimum-players rule and the 3-0 forfeit (docs/re/pes-re-missing-systems.md 2.7 and
// 2.13). The rule's three values are immediates in PES's code: the post-match period (10), the
// sent-off state of a player record (4) and the forfeit score (3). LoadForfeitConstants reads them
// from the user's PES2021.exe with each instruction's opcode checked first. The minimum-player
// threshold is not an immediate: PES compares the count with a settings byte (E+0x17), so it is a
// named stand-in in the forfeit namespace below.

#ifndef _HPP_ONTHEPITCH_PESFORFEIT
#define _HPP_ONTHEPITCH_PESFORFEIT

#include <optional>
#include <string>

#include "pes/exe.hpp"

namespace pes {

struct ForfeitConstants {
  int postMatchPeriod = 0;  // 10: `mov edx, 0xa` @0x140447EF8 (the min-players writer); the gate @0x140A23279 agrees
  int sentOffState = 0;     // 4: `cmp dword ptr [rax+8], 4` @0x140A2330E, player+0x0c (yellow then red)
  int forfeitScore = 0;     // 3: `mov dword ptr [rax+0x1b74], 3` @0x140A23329, the score the side is written against
};

// Reads the three immediates from the install. A different build fails here instead of yielding a
// wrong score.
std::optional<ForfeitConstants> LoadForfeitConstants(const PeImage& exe, std::string* error);

namespace forfeit {

// ponytail: stand-in for E+0x17, the settings byte that PES compares with a side's on-pitch count
// (`cmp al, byte ptr [rdx+0x17]` @0x140447E79). It reads 7 live (21-10-26), but its writer is not
// located (docs/re/pes-re-missing-systems.md K11-2, K11-6), so the install cannot supply it yet.
// Replace with a loader once the settings writer is traced.
inline constexpr int kPesMinimumPlayers = 7;

// PES's count test (0x140447E79): a side is short when its on-pitch count is below the minimum.
bool BelowMinimumPlayers(int onPitch, int minimum);

// The per-player test of FUN_140A23260 (0x140A2330E and 0x140A23308): a player on the pitch whose
// card state is the sent-off value and whose injury marker (player+0x18) is clear.
bool IsForfeitOffence(const ForfeitConstants& k, int cardState, int injuryMarker);

}  // namespace forfeit

}  // namespace pes

#endif
