#pragma once

// PES 2021's per-player signature motions. common/anime/playerID/bin/Derivationdatatable.bin
// (read through the VFS winner) holds six lists of 8-byte substitutions. The walkers search them:
// FUN_1406B3560 by player id (with its tail) and FUN_1406B3780 by a stat for the generic entries.
// FUN_14071E570 selects the long-kick clip with both. docs/re/pes-re-special-players-decode.md
// §0-§2 has the VAs. Every constant here is read from PES2021.exe's own instructions
// (LoadDerivationLayout and LoadDerivationRules check the opcodes). Pure apart from the loaders,
// and not wired into clip selection yet.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pes {

class PeImage;
class PesVfs;

inline constexpr int DERIVATION_KINDS = 6;
inline constexpr int BECKHAM_CARDS = 5;  // FUN_14071E570's id compares (0x14071E5C3..0x14071E5FF)

// The entry packing and the list lengths. Each value is an immediate or displacement of the
// walkers' own code.
struct DerivationLayout {
  std::array<int, DERIVATION_KINDS> listLength;      // kind 0..5: 8, 35, 23, 15, 16, 9
  int motionInMask, motionInBase;                    // motion in  = (word1 & motionInMask) + motionInBase
  int motionOutMask, motionOutBase, motionOutShift;  // motion out = ((dword0 >> motionOutShift) & motionOutMask) + motionOutBase
  int playerIdMask;                                  // stored = dword0 & playerIdMask: 0 is generic, else id = stored - 1
  int subMask, subShift;                             // sub = (word1 >> subShift) & subMask
};

// The walkers' and the selection's constants.
struct DerivationRules {
  // FUN_14071E570's five cards: 815, 118718, 123447, 134448, 134563 (0x14071E5C3..0x14071E5FF).
  std::array<int, BECKHAM_CARDS> beckhamIds;
  // FUN_1406B3560's tail cards, which have no list rows: 134563 and 134448 (0x1406B36C1, 0x1406B36C9).
  int tailIdA, tailIdB;
  int freeKickIdle, freeKickRunup;  // 1233 and 1248 (0x1406B36D1, 0x1406B36FB)
  int freeKickKind;                 // kind 2, the free-kick lists (0x1406B36DA)
  int beckham2002Offset;            // +3: fk_idle -> fk_idle_beckham_2002, fk_runup -> fk_runup_beckham_2002 (0x1406B36DF)
  int curveKind;                    // kind 1, the tail's curve rewrite (0x1406B372E)
  int curvePlacekick;               // 2380 (0x1406B3725)
  int curveBeckham2002;             // 2367 (0x1406B3731, and 0x14071E630)
  int curve;                        // 2365: the selection's range base (0x14071E60B) and round-trip clip
  int curveSpan;                    // the curve range is curve..curve + curveSpan (0x14071E611)
  int curveMax;                     // 2376, the third curve pick (0x14071E616)
  int beckhamVariant;               // the anime state the Beckham branch needs (0x14071E62B)
  int playerKind;                   // kind 3: the selection's player walk (0x14071E65C)
  int genericKind;                  // kind 1: the selection's generic walk (0x14071E698)
  int curvePlacekickDelta;          // -15: placekick - 15 = the curve (0x14071E6AF)
  int remapFrom, remapDelta;        // 2654 -> 2654 + 29 (0x14071E6B5, 0x14071E6BD)
  int remapFrom2, remapTo2;         // 2750 -> 2780 (0x14071E6C7, 0x14071E6C2)
};

// One decoded entry. Bits above the walkers' fields are never read.
struct DerivationEntry {
  std::optional<int> playerId;  // nullopt: a generic entry (the id field is 0)
  int motionIn;                 // the clip replaced
  int motionOut;                // the clip substituted
  int sub;                      // the 5-bit sub field; the generic walk matches sub - 1 against the stat
};

struct DerivationTable {
  DerivationRules rules;
  std::array<std::vector<DerivationEntry>, DERIVATION_KINDS> lists;  // by kind
};

// The layout and the rules, from the exe's instructions. Both walkers must agree on the packing.
std::optional<DerivationLayout> LoadDerivationLayout(const PeImage& exe, std::string* error);
std::optional<DerivationRules> LoadDerivationRules(const PeImage& exe, std::string* error);

// The file through the VFS winner, decoded with the exe's layout and rules.
std::optional<DerivationTable> LoadDerivationTable(const PeImage& exe, const PesVfs& vfs, std::string* error);
// The file's bytes: the list lengths give the entry count, and the size must be exactly that.
std::optional<DerivationTable> ParseDerivationTable(const std::vector<uint8_t>& bytes, const DerivationLayout& layout,
                                                    const DerivationRules& rules, std::string* error);

// FUN_1406B3560: the player walk over list `kind`, then the tail's rewrites. Returns motionIn
// when nothing substitutes it.
int DerivePlayerMotion(const DerivationTable& t, int playerId, int motionIn, int kind);
// FUN_1406B3780: the generic walk over list `kind`. Matches entries with no player id, whose sub
// minus one is the stat. Returns motionIn when nothing substitutes it.
int DeriveGenericMotion(const DerivationTable& t, int stat, int motionIn, int kind);

// FUN_14071E570's inputs, each read by the caller: the player's id; the clip FUN_1406ABB30 picked
// for the row and column; the caller's flag (param_4); the anime state, which is vt+0x28 of the
// player's anime interface (player+0xAB8) and must equal the Beckham variant; whether the picked
// clip is substitutable (FUN_1406B2080 over the anime records); and the stat byte
// FUN_1406E6660(player, 0xE).
struct LongKickInput {
  int playerId;
  int picked;
  bool flag;
  int animeVariant;
  bool substitutable;
  int stat;
};

// FUN_14071E570: the clip for a long kick.
int SelectLongKickClip(const DerivationTable& t, const LongKickInput& in);

}  // namespace pes
