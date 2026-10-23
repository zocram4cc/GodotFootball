#include "pesforfeit.hpp"

#include <cstdint>
#include <cstring>

namespace pes {
namespace {

// PES2021.exe instructions that carry the rule's values (bytes checked in docs/re and by the
// install test). Each VA is the instruction's first byte; the opcode prefix must match, then the
// immediate of `width` bytes follows it.
constexpr uint64_t MIN_PLAYERS_PERIOD_VA = 0x140447EF8;  // mov edx, imm32
constexpr uint8_t MOV_EDX_OPCODE[] = {0xba};
constexpr uint64_t FORFEIT_PERIOD_GATE_VA = 0x140A23279;  // cmp dword ptr [r12+0x1168], imm8
constexpr uint8_t PERIOD_GATE_OPCODE[] = {0x41, 0x83, 0xbc, 0x24, 0x68, 0x11, 0x00, 0x00};
constexpr uint64_t SENT_OFF_VA = 0x140A2330E;  // cmp dword ptr [rax+8], imm8
constexpr uint8_t SENT_OFF_OPCODE[] = {0x83, 0x78, 0x08};
constexpr uint64_t FORFEIT_SCORE_VA = 0x140A23329;  // mov dword ptr [rax+0x1b74], imm32
constexpr uint8_t FORFEIT_SCORE_OPCODE[] = {0xc7, 0x80, 0x74, 0x1b, 0x00, 0x00};

// The immediate of the instruction at `va`, or false when its opcode prefix is not the expected one.
bool ReadImmediate(const PeImage& exe, uint64_t va, const uint8_t* opcode, size_t opcodeLen,
                   size_t width, int32_t* out) {
  uint8_t bytes[16] = {};
  if (opcodeLen + width > sizeof bytes || !exe.ReadVa(va, opcodeLen + width, bytes)) return false;
  if (std::memcmp(bytes, opcode, opcodeLen) != 0) return false;
  int32_t value = 0;  // zeroed, so a one-byte immediate fills the low byte
  std::memcpy(&value, bytes + opcodeLen, width);
  *out = value;
  return true;
}

}  // namespace

std::optional<ForfeitConstants> LoadForfeitConstants(const PeImage& exe, std::string* error) {
  int32_t postMatch = 0, gatePeriod = 0, sentOff = 0, score = 0;
  if (!ReadImmediate(exe, MIN_PLAYERS_PERIOD_VA, MOV_EDX_OPCODE, sizeof MOV_EDX_OPCODE, 4, &postMatch) ||
      !ReadImmediate(exe, FORFEIT_PERIOD_GATE_VA, PERIOD_GATE_OPCODE, sizeof PERIOD_GATE_OPCODE, 1,
                     &gatePeriod) ||
      !ReadImmediate(exe, SENT_OFF_VA, SENT_OFF_OPCODE, sizeof SENT_OFF_OPCODE, 1, &sentOff) ||
      !ReadImmediate(exe, FORFEIT_SCORE_VA, FORFEIT_SCORE_OPCODE, sizeof FORFEIT_SCORE_OPCODE, 4,
                     &score)) {
    if (error)
      *error = "PES2021.exe: the forfeit rule's instructions are not the ones this port was decoded from";
    return std::nullopt;
  }
  if (postMatch != gatePeriod) {
    if (error)
      *error = "PES2021.exe: the min-players writer and the forfeit period gate disagree";
    return std::nullopt;
  }
  return ForfeitConstants{postMatch, sentOff, score};
}

namespace forfeit {

bool BelowMinimumPlayers(int onPitch, int minimum) { return onPitch < minimum; }

// ponytail: the two inputs are GF's card and injury state standing in for PES's player record. The PES
// writer of player+0x0C == 4 is OPEN (docs/re/pes-re-missing-systems.md 2.12): GF's cards == 4 (yellow
// then red) stands in for it, and injuryLevel == 0 for the player+0x18 marker. Replace both when the
// writer is decoded.
bool IsForfeitOffence(const ForfeitConstants& k, int cardState, int injuryMarker) {
  return cardState == k.sentOffState && injuryMarker == 0;
}

}  // namespace forfeit

}  // namespace pes
