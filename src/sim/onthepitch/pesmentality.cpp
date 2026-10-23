#include "onthepitch/pesmentality.hpp"

#include <cstdint>
#include <cstring>

#include "pes/exe.hpp"

namespace pes::mentality {
namespace {

constexpr uint64_t kImageBase = 0x140000000;  // PES2021.exe's image base: file offset = VA - base

// EC600's reclassify jump table (blocks §2): nine RVAs from the image base, index class - 1.
constexpr uint64_t kLineTableVa = 0x1405ED1F4;
constexpr int kLineTableEntries = 9;
constexpr size_t kLineArmBytes = 8;
// The default arm, which classes 0, 1 and 2 and any class past 9 reach: MOV R10,R14; MOV EDI,R14D.
// R14 is zero from EC600's XOR at 0x1405EC714, so the arm stores line 0.
constexpr uint8_t kDefaultArm[6] = {0x4D, 0x8B, 0xD6, 0x41, 0x8B, 0xFE};
constexpr uint8_t kMovEdiImm32 = 0xBF;                  // MOV EDI, imm32 (5 bytes)
constexpr size_t kImmArmTail = 5;                       // the arm's MOV R10D,EDI follows the immediate
constexpr uint8_t kMovR10dEdi[3] = {0x44, 0x8B, 0xD7};  // MOV R10D, EDI

// Block B's float operands (blocks §3.1): DAT_14259D634 read by MOVSS at 0x14067CDE3, and
// DAT_1425C65FC, the pick's starting minimum.
constexpr uint64_t kForwardPenaltyVa = 0x14259D634;
constexpr uint64_t kInitialBestVa = 0x1425C65FC;
constexpr int kKeeperClass = 0;         // a GK is never a fold candidate (0x14067CDB3)
constexpr int kForwardClassAbove = 5;   // role > 5 takes the penalty: AMF, SS, WF, CF (0x14067CE72)
constexpr int kCentreForwardClass = 9;  // CF

// The dial's neutral rung (b8a4 == 2, the init at FUN_140A415A0).
constexpr int kNeutralDial = 2;

// The role level's values (EC600's ADD EBP,2 and the CMOVZ override).
constexpr int kLevelOutOfPossession = 2;
constexpr int kLevelInPossession = 1;
constexpr int kLevelOutOfPlay = 0;

bool ReadF32(const PeImage& exe, uint64_t va, float* out) {
  uint8_t bytes[4] = {};
  if (!exe.ReadVa(va, sizeof bytes, bytes)) return false;
  std::memcpy(out, bytes, sizeof *out);
  return true;
}

// The line one arm stores. A default arm is line 0; a MOV EDI,imm32 arm stores its immediate,
// which must be a line.
bool DecodeLineArm(const uint8_t (&code)[kLineArmBytes], int* line) {
  if (std::memcmp(code, kDefaultArm, sizeof kDefaultArm) == 0) {
    *line = kBackLine;
    return true;
  }
  if (code[0] != kMovEdiImm32 || std::memcmp(code + kImmArmTail, kMovR10dEdi, sizeof kMovR10dEdi) != 0)
    return false;
  int32_t imm = 0;
  std::memcpy(&imm, code + 1, sizeof imm);
  if (imm < kBackLine || imm > kForwardLine) return false;
  *line = imm;
  return true;
}

}  // namespace

int RoleLevel(bool possession, int playState, int restartKind) {
  // EC600's override: CMP [MI+0x116C],1; JZ; CMP [MI+0x1554],1; CMOVZ EBP,0.
  if (playState != kPlayStateInPlay && restartKind == kRestartKindOne) return kLevelOutOfPlay;
  return possession ? kLevelInPossession : kLevelOutOfPossession;
}

std::optional<LineTable> LoadReclassifyLines(const PeImage& exe, std::string* error) {
  LineTable table;
  for (int i = 0; i < kLineTableEntries; ++i) {
    uint32_t rva = 0;
    if (!exe.ReadVa(kLineTableVa + 4 * static_cast<uint64_t>(i), sizeof rva,
                    reinterpret_cast<uint8_t*>(&rva))) {
      *error = "reclassify table: unreadable";
      return std::nullopt;
    }
    uint8_t code[kLineArmBytes] = {};
    if (!exe.ReadVa(kImageBase + rva, sizeof code, code)) {
      *error = "reclassify arm: unreadable";
      return std::nullopt;
    }
    if (!DecodeLineArm(code, &table.lineOfClass[i])) {
      *error = "reclassify arm: not a line store";
      return std::nullopt;
    }
  }
  return table;
}

int ReclassifyLine(const LineTable& table, int cls) {
  if (cls < 1 || cls > kLineTableEntries) return kBackLine;  // the default arm
  return table.lineOfClass[cls - 1];
}

Block BlockFor(bool possession) {
  return possession ? Block::kInPossession : Block::kOutOfPossession;
}

std::optional<FoldConstants> LoadFoldConstants(const PeImage& exe, std::string* error) {
  FoldConstants k;
  if (!ReadF32(exe, kForwardPenaltyVa, &k.forwardPenalty) || !ReadF32(exe, kInitialBestVa, &k.initialBest)) {
    *error = "fold constants: unreadable";
    return std::nullopt;
  }
  if (!(k.forwardPenalty > 0.0f) || !(k.initialBest > k.forwardPenalty)) {
    *error = "fold constants: out of range";
    return std::nullopt;
  }
  return k;
}

std::vector<int> FoldPicks(const FoldConstants& k, const std::array<FoldCandidate, kSlots>& cands,
                           int protectedSlot, int count, bool fullAttack) {
  std::vector<int> picks;
  if (fullAttack) return picks;  // CMP BYTE [R13+0xB8A2] (0x14067CDFB): the pass is skipped
  // ponytail: the byte filter [base+0x173C] != prev excludes only the previous pick; here every
  // earlier pick of this pass is excluded, which is the same for the two folds the dial reaches.
  std::array<bool, kSlots> taken{};
  for (int n = 0; n < count; ++n) {
    float best = k.initialBest;
    int pick = -1;
    for (int slot = 0; slot < kSlots; ++slot) {
      const FoldCandidate& c = cands[slot];
      if (!c.onPitch || c.cls == kKeeperClass || slot == protectedSlot || taken[slot]) continue;
      float score = c.x * c.x + c.z * c.z;
      if (c.cls > kForwardClassAbove) score += k.forwardPenalty;
      if (score < best) {
        best = score;
        pick = slot;
      }
    }
    if (pick < 0) break;
    taken[pick] = true;
    picks.push_back(pick);
  }
  return picks;
}

int FoldCount(int dial) { return dial < kNeutralDial ? kNeutralDial - dial : 0; }

int ProtectedSlot(const std::array<int, kSlots>& classes) {
  for (int slot = kSlots - 1; slot >= 0; --slot)
    if (classes[slot] == kCentreForwardClass) return slot;
  return -1;
}

}  // namespace pes::mentality
