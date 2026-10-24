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

int ReclassifyLineOf(int cls) {
  // The stock reclassify (MentalityPort doc §15): classes 3-6 -> 1, 7-9 -> 2, else 0. The lane
  // writers need the class line without threading the install table through the tick.
  if (cls >= 3 && cls <= 6) return kMidLine;
  if (cls >= 7 && cls <= 9) return kForwardLine;
  return kBackLine;
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

namespace {
// The instruction ids CE150 tests (lane-post §1 steps 2-3, CE150 l.113-115).
constexpr int kInstrSixteen = 0x10;
constexpr int kInstrFifteen = 0xF;
constexpr int kInstrPartnerNone = 0xFF;
constexpr int kInstrRows = 4;
// The SMF/WF band (lane-post §1 step 4): roles 5/8 are the midfield line 1.
constexpr int kSmfRole = 5;
constexpr int kWfRole = 8;
// The SB roles counted in step 6 (roles 1, 2: role - 1 < 2); the special needs fewer than 5.
constexpr int kSbCountCap = 5;
}  // namespace

bool InstructionClaimed(const InstrTable& table, int side, int id, int slot) {
  // FUN_140A63950 (VA 0x140A63950): id < 17, side < 2; some row j < 4 holds (id, slot).
  if (id >= 17 || side < 0 || side >= 2) return false;
  for (int j = 0; j < kInstrRows; ++j)
    if (table.ids[side][j] == id && table.slots[side][j] == slot) return true;
  return false;
}

bool SpecialPredicate(const SpecialInput& in) {
  // FUN_140A64410 (VA 0x140A64410, lane-post §1): uVar15 = side·11 + k; GF passes the local
  // k with the side, and only slots < 0x16 occur (CE150 loops k 0..10).
  if (in.side < 0 || in.side >= 2 || in.slot < 0 || in.slot >= kSlots) return false;
  // Step 2: some (0x10, 0xFF) row must exist on the side, else 0.
  bool sixteen = false;
  for (int j = 0; j < kInstrRows; ++j)
    if (in.instr.ids[in.side][j] == kInstrSixteen &&
        in.instr.slots[in.side][j] == kInstrPartnerNone)
      sixteen = true;
  if (!sixteen) return false;
  // Step 3: an instruction-15 claim on this slot excludes the special.
  if (InstructionClaimed(in.instr, in.side, kInstrFifteen, in.side * kSlots + in.slot))
    return false;
  // Step 4: the own role must be SMF/WF (roles[k] reads T+0x4C00 row-wise; GF roles.b is 1:1).
  const int ownRole = in.roles[in.slot];
  if (ownRole != kSmfRole && ownRole != kWfRole) return false;
  // Step 5: the slot must be active (MI+0x1748).
  if (in.active[in.slot] == 0) return false;
  // Step 6: 5 or more SB (roles 1, 2) on the side forces special.
  int sb = 0;
  for (int m = 0; m < kSlots; ++m)
    if (in.roles[m] - 1 < 2) ++sb;
  if (sb >= kSbCountCap) return true;
  // Step 7: a WF (role 8) is special without the sign scan.
  if (ownRole != kSmfRole) return true;
  // Step 8: an SMF (role 5) needs a same-sign role-5 mate: s = +1 unless (f <= 0 && f != 0).
  // s1 reads the S+0x3A9C f0 of k; s2 the T+0x46EC float of the mate (ponytail: not carried).
  const float f1 = in.slotF0[in.slot];
  const float s1 = (f1 <= 0.0f && f1 != 0.0f) ? -1.0f : 1.0f;
  for (int m = 0; m < kSlots; ++m) {
    if (m == in.slot || in.roles[m] != kSmfRole || in.active[m] == 0) continue;
    const float f2 = in.secondF0[m];
    const float s2 = (f2 <= 0.0f && f2 != 0.0f) ? -1.0f : 1.0f;
    if (s1 == s2) return false;
  }
  return true;
}

int Cd540Line(int cls) {
  // FUN_1405CD540 (lane-post §2a): B[k] 0 -> 3; 1-2 -> 0; 3-6 -> 1; 7-9 -> 2.
  if (cls == kKeeperClass) return 3;
  return ReclassifyLineOf(cls);
}
std::optional<LaneThresholds> LoadLaneThresholds(const PeImage& exe, std::string* error) {
  // CCD50's float operands (lane-post §0): DAT_14259B89C, DAT_142565600, DAT_14257A190.
  LaneThresholds t;
  constexpr uint64_t kBackZeroVa = 0x14259B89C;
  constexpr uint64_t kBackNonZeroVa = 0x142565600;
  constexpr uint64_t kFwdOtherVa = 0x14257A190;
  if (!ReadF32(exe, kBackZeroVa, &t.backZero) || !ReadF32(exe, kBackNonZeroVa, &t.backNonZero) ||
      !ReadF32(exe, kFwdOtherVa, &t.fwdOther) || !ReadF32(exe, kBackNonZeroVa, &t.fwdOne)) {
    *error = "lane thresholds: unreadable";
    return std::nullopt;
  }
  // The back arm's 0.4-for-keeper vs 0.5 select, and the forward arm's 0.5/0.6 select.
  if (!(t.backZero < t.backNonZero) || !(t.fwdOne < t.fwdOther)) {
    *error = "lane thresholds: out of order";
    return std::nullopt;
  }
  return t;
}

int Ccd50Word(const LaneThresholds& t, const Ccd50Input& in) {
  // FUN_1405CCD50 (VA 0x1405CCD50, lane-post §2). uVar15 = side·11 + k; GF passes local k.
  // Pre-branch: out of play (MI+0x116C != 1) with MI+0x54 == 0 and restart kind 3/4/5 -> 0.
  if (in.playState116c != kPlayStateInPlay && in.mi54 == 0 && in.restartKind1554 - 3 < 3)
    return 0;
  const int line = Cd540Line(in.cls);
  // The class record at LX+0xB8 selects the threshold (CCD50 reads the B class, not the line).
  if (line != kForwardLine || !in.fwdProductNeg) {
    const float threshold = (in.cls == kKeeperClass) ? t.backZero : t.backNonZero;
    return (in.backDist <= threshold) ? 0 : 1;
  }
  const float threshold = (in.cls == 1) ? t.fwdOne : t.fwdOther;
  return ((in.fwdDist <= threshold) ? 0 : 1) + 1;
}

int Fa70Count(const std::array<int, kSlots>& cls, const std::array<int, kSlots>& active, int line) {
  // FUN_14063FA70 (VA 0x14063FA70, lane-post §4): per active slot with class != 0, the switch
  // maps 3-6 -> 1, 7-9 -> 2, default -> 0 (ReclassifyLineOf); counts line matches.
  int count = 0;
  for (int k = 0; k < kSlots; ++k) {
    if (active[k] == 0 || cls[k] == 0) continue;
    if (ReclassifyLineOf(cls[k]) == line) ++count;
  }
  return count;
}

bool OpponentLine(float x, float y, int sideSign, float z) {
  // FUN_140A4C720 (VA 0x140A4C720, lane-post §7): (c*z + x)*c < c*y with c = (float)(int)sideSign.
  const float c = static_cast<float>(sideSign);
  return (c * z + x) * c < c * y;
}

bool Is38State(int actionState) {
  // FUN_1405CCD00 (VA 0x1405CCD00, lane-post §4d): the T+0xA2A0 word == 0x3A.
  return actionState == kActionState38;
}

bool Cd780Claimed(const InstrTable& table, int side, int slot, int playState116c,
                   int restartKind1554) {
  // FUN_1405CD780 (VA 0x1405CD780, lane-post §4d): in play (MI+0x116C == 1) -> false; restart
  // kind 4 (MI+0x1554 == 4) with FUN_140A63950(side-team, side, 0xF, slot).
  if (playState116c == kPlayStateInPlay || restartKind1554 != 4) return false;
  return InstructionClaimed(table, side, kInstrFifteen, slot);
}
std::array<int, kSlots> Cfc80FirstLoop(const Cfc80Input& in) {
  // FUN_1405CFC80's first loop (VA 0x1405CFC80, lane-post §4b): per on-pitch slot (class != 0)
  // with CCD00 == 0 (not 0x3A), CD780 == 0 (no instruction-15 claim at kind 4), slot != outlet,
  // the 0x3A arm (slot%11 > 10, never, or state != 0x3A), T+0xB8A2 == 0 or B != 2, and
  // (A > 1 && (B == 1 || (T+0xB8A4 < 2 && B != 0))): A := 1 until
  // (backTarget - alreadyOneAt1 + forwardTarget) <= done. The sorted-order second loop needs
  // the A71500 table: ponytail at VA 0x1405CFC80 (second loop).
  // GF: CD780's instruction table is empty and kind-4 play state comes from the caller, so the
  // claim test runs exact over the caller's words; outlets stay -1 (ponytail: T+0x4B40).
  std::array<int, kSlots> a = in.a;
  int alreadyOne = 0;
  int alreadyZero = 0;
  for (int k = 0; k < kSlots; ++k) {
    if (in.active[k] == 0) continue;
    if (a[k] == 1) ++alreadyOne;
    if (a[k] == 0) ++alreadyZero;
  }
  int need = in.backTarget - alreadyZero;
  if (need < 0) need = 0;
  int done = 0;
  for (int k = 0; k < kSlots; ++k) {
    if (in.cls[k] == 0) continue;
    if (in.active[k] == 0) continue;
    if (Is38State(in.states[k])) continue;
    if (k == in.outlet) continue;
    if (in.fullAttack && in.b[k] == 2) continue;
    if (a[k] <= 1) continue;
    if (!(in.b[k] == 1 || (in.attackLevel < 2 && in.b[k] != 0))) continue;
    ++done;
    a[k] = 1;
    if (need - alreadyOne + in.forwardTarget <= done) break;
  }
  return a;
}
std::array<int, kSlots> Cf310FirstHalf(const Cf310Input& in) {
  // FUN_1405CF310's first half (VA 0x1405CF310, lane-post §4a): per slot with B-rank 0, active,
  // on-pitch (class != 0), A-word != 0, CCD00 == 0 (not 0x3A), slot != outlet and CD780 == 0:
  // the in-play arm (local_1D0 == 0, local_1E0 == 0) zeroes A when
  // OpponentLine(sideSign * fVar22, manLateral, sideSign) with
  // fVar22 = 12.0 + ({3 if B == 3} / {+3 if B in {5,6}} / {-20 if B < 3}).
  // 12.0 = DAT_14259BE04, 3.0 = DAT_14258C264, 20.0 = DAT_14259BE1C (lane-post §0).
  // ponytail: the CD460/A57800 arm (T+0x44/T+0x8, OPEN) and the stoppage arm (A714C0, OPEN);
  // GF passes the ball now for the analysis ball and zeros (no 0x3A states yet).
  constexpr float kBase = 12.0f;    // DAT_14259BE04
  constexpr float kThree = 3.0f;    // DAT_14258C264
  constexpr float kTwenty = 20.0f;  // DAT_14259BE1C
  std::array<int, kSlots> a = in.a;
  for (int k = 0; k < kSlots; ++k) {
    if (in.bRank[k] != 0) continue;
    if (in.active[k] == 0) continue;
    if (in.cls[k] == 0) continue;
    if (a[k] == 0) continue;
    if (Is38State(in.states[k])) continue;
    if (k == in.outlet) continue;
    float adjust = -kTwenty;
    if (in.b[k] == 5 || in.b[k] == 6) {
      adjust = kThree;
    } else if (in.b[k] >= 3) {
      adjust = kThree;
    }
    const float fVar22 = kBase + adjust;
    if (OpponentLine(static_cast<float>(in.sideSign) * fVar22, in.manLateral[k], in.sideSign, 0.0f))
      a[k] = 0;
  }
  return a;
}
std::array<int, kSlots> Cffe0Bands(const Cffe0Input& in) {
  // FUN_1405CFFE0's value rule (VA 0x1405CFFE0, lane-post §4c): per active on-pitch 0x3A-state
  // slot, iVar2 is the B-word, iVar18 the B class. Keeper B (iVar2 == 3) keeps 3 (unless the
  // 0x3A frame/fps fall-through fires: ponytail, in.fpsMatters). Else the band compare of
  // ballDepth vs back/mid/fwd edges picks cVar16: behind (0) -> 1, or (backEdge + 5 <= depth)
  // + 1 when !guarded and B == 1 and class == 1; mid (1) -> 1 when !guarded, else the ramp
  // (((back - mid) * 50) / 100 + mid <= depth) + 1 with the B == 2/1 overrides. 5.0, 50.0 and
  // 100.0 are DAT_14259222C/14259BE34/14253EE30 (lane-post §0).
  // ponytail: the F9C3B0 early arm (OPEN), the forward deep-narrow arm (A71500/4C720/T+0x8770),
  // and the cVar16 == 3 arm. GF has no 0x3A states yet, so the gate is dormant on the PES path.
  constexpr float kFive = 5.0f;      // DAT_14259222C
  constexpr float kFifty = 50.0f;    // DAT_14259BE34
  constexpr float kHundred = 100.0f;  // DAT_14253EE30
  std::array<int, kSlots> a = in.a;
  for (int k = 0; k < kSlots; ++k) {
    if (in.active[k] == 0) continue;
    if (in.cls[k] == 0) continue;
    if (!Is38State(in.states[k])) continue;
    const int word = in.b[k];
    if (word == 3 && !in.fpsMatters) {
      a[k] = 3;
      continue;
    }
    int band = 0;
    if (in.ballDepth >= in.midEdge) band = (in.ballDepth >= in.fwdEdge) ? 2 : 1;
    if (band == 0) {
      int v = 1;
      if (!in.guarded && word == 1 && in.cls[k] == 1)
        v = ((in.backEdge + kFive <= in.ballDepth) ? 1 : 0) + 1;
      a[k] = v;
    } else if (band == 1) {
      if (!in.guarded) {
        a[k] = 1;
      } else {
        float edge = in.midEdge;
        if (word == 2 && in.cls[k] == 2) {
          edge = kHundred - 1.0f;  // ponytail: 100 - T+0x254[opp]; side word not carried
        } else if (word == 1 && in.cls[k] == 1) {
          edge = kHundred;
        } else {
          edge = ((in.backEdge - in.midEdge) * kFifty) / kHundred + in.midEdge;
        }
        a[k] = ((edge <= in.ballDepth) ? 1 : 0) + 1;
      }
    }
  }
  return a;
}
int KickManKeep(int current, int mode1775, int phase116c, bool gate431E0, bool gate23E7FD0) {
  // FUN_1404FB2A0 (VA 0x1404FB2A0, lane-leaves §4): mode 0x0C keeps; in play (1) both gates
  // must hold, else 0xFF; phases 2/3 run 4FB780/4FB380 (not here); any other phase clears.
  // ponytail: 4FB780/4FB380 arms, and the 431E0/23E7FD0 gate interiors, are OPEN or
  // HolderSplit's; GF's MatchModeByte stand-in is 0, so the keep arm never fires.
  if (mode1775 == kKeepMode) return current;
  if (phase116c == kPlayStateInPlay) return (gate431E0 && gate23E7FD0) ? current : kNoKickMan;
  if (phase116c == 2 || phase116c == 3) return current;  // 4FB780/4FB380 own these phases
  return kNoKickMan;
}

int KickManFallback(int carrier, int carrierSide, int holder, int holderSide, int current,
                     int currentSide, int side) {
  // FUN_140A63020 (VA 0x140A63020, lane-leaves §4): carrier (MA+0x2601C) -> holder
  // (MA+0x2483C) -> T+0x78, each gated by id < 0x16 and sideMatch(id) == side.
  constexpr int kSideSlots = 22;  // ids 0..21 are the two sides' slots; 0xFF = none
  if (carrier >= 0 && carrier < kSideSlots && carrierSide == side) return carrier;
  if (holder >= 0 && holder < kSideSlots && holderSide == side) return holder;
  if (current >= 0 && current < kSideSlots && currentSide == side) return current;
  return kNoKickMan;
}

float KickRamp(float a, float b, float c) {
  // FUN_1404F4AB0 (holder-tree §4, 11 lines): a<=b ? 0 : c<=a ? 1 : (a-b)/(c-b).
  if (a <= b) return 0.0f;
  if (c <= a) return 1.0f;
  return (a - b) / (c - b);
}

int ProtectedSlot(const std::array<int, kSlots>& classes) {
  for (int slot = kSlots - 1; slot >= 0; --slot)
    if (classes[slot] == kCentreForwardClass) return slot;
  return -1;
}

std::array<int, kSlots> LaneBWords(const LaneInput& in) {
  // FUN_1405CE150 (lane-writers §3): keeper (B 0) -> 3, designated (P+0x3EF8) -> 2, else the
  // reclassify of the slot's class (FUN_1405CD540). Inactive slots keep their B word here; the
  // recompute path (LaneAWords) overwrites them with 3.
  // ponytail: the restart-kind-4 + instruction-15 arm (FUN_140A63950, VA 0x140A63950) reads the
  // instruction table MA+0x25CF8/0x25D38, which GF does not carry; the A64410 special arm reads the
  // slot-point rows + F760/F490 distances, also not carried.
  std::array<int, kSlots> w{};
  for (int k = 0; k < kSlots; ++k) {
    if (in.classes[k] == 0) {
      w[k] = 3;
    } else if (k == in.designated) {
      w[k] = 2;
    } else {
      w[k] = ReclassifyLineOf(in.classes[k]);
    }
  }
  return w;
}

int LaneOfWord(int a) {
  // The wrapper map (0x140682090-0x1406820B4): A 0 -> lane 1, 1 -> 4, 2 -> 7, else 0 (skipped).
  if (a == 0) return 1;
  if (a == 1) return 4;
  if (a == 2) return 7;
  return 0;
}

void LaneAWords(const std::array<int, kSlots>& b, std::array<int, kSlots>& a, int designated,
                const std::array<int, kSlots>& active) {
  // FUN_1405CE4F0's recompute (lane-writers §2): every A-word := 3; active slots with A > 1 and
  // (designated or B == 2) -> 2; inactive -> 3, else active with A > 2 take their B word.
  // ponytail: the CFFE0 -> CF310 -> CFC80 post-steps run before these loops at CE4F0 l.65-140.
  // An all-zero `active` means no mask was passed: every slot is active.
  bool anyActive = false;
  for (int k = 0; k < kSlots; ++k) anyActive = anyActive || active[k] != 0;
  a.fill(3);
  for (int k = 0; k < kSlots; ++k) {
    if (anyActive && active[k] == 0) continue;
    if (k == designated || b[k] == 2) a[k] = 2;
  }
  for (int k = 0; k < kSlots; ++k) {
    if (anyActive && active[k] == 0) {
      a[k] = 3;
    } else if (a[k] > 2) {
      a[k] = b[k];
    }
  }
}

void CopyLaneAWords(const std::array<int, kSlots>& b, std::array<int, kSlots>& a) {
  // CE4F0's copy path (flag LX+0x1B0 == 0, l.52-64): A-record := B-record for all 11 slots.
  a = b;
}

bool StoppageResetsCopyFlag(int playState116c, int word11d4, int word11b0) {
  // CE4F0 l.41-50 (lane-writers §2 step 3): MI+0x11D4 == 0 and (MI+0x116C == 2, or MI+0x116C == 3
  // with the frame gate, or MI+0x11B0 < 0x15) clears the flag. GF carries no frame counter for
  // the == 3 arm and no MI+0x11B0 word, so only the == 2 and < 0x15 arms port; the == 3 arm is a
  // ponytail leaf at VA 0x1405CE4F0.
  if (word11d4 != 0) return false;
  if (playState116c == 2) return true;
  return word11b0 < 0x15;
}

bool CopyPathRuns(bool copyFlagSet) {
  // CE4F0's gate (lane-writers §2 step 2): flag LX+0x1B0 == 0 runs the copy path (A := B).
  // GF stores the flag set (copy runs while set) and clears it on the stoppage step above.
  return copyFlagSet;
}

std::array<uint8_t, kSlots> LaneBRank(const std::array<int, kSlots>& b,
                                      const std::array<uint8_t, kSlots>& order) {
  // CE150's B-rank (lane-writers §3, decompile l.150-182): per slot in the sorted order, the
  // count of earlier slots with the same B-word; inactive slots keep rank 0.
  std::array<uint8_t, kSlots> rank{};
  std::array<int, 4> seen{};
  for (int i = 0; i < kSlots; ++i) {
    const int slot = order[i];
    const int word = b[slot];
    if (word < 0 || word > 3) continue;
    rank[slot] = static_cast<uint8_t>(seen[word]);
    ++seen[word];
  }
  return rank;
}
std::array<uint8_t, kSlots> Ce930Ranks(const std::array<int, kSlots>& a,
                                       const std::array<uint8_t, kSlots>& order,
                                       const std::array<int, kSlots>& cls,
                                       const std::array<int, kSlots>& active) {
  // FUN_1405CE930's head (VA 0x1405CE930, lane-leaves §3.1): local_2E4 counts start at 0 per
  // word; per slot in the sorted order, active on-pitch (class != 0) slots take the count for
  // their current A-word. 0x3A-state slots are substituted by local_2DC (the nearest slot);
  // the tail that resolves it needs OPEN gates, so the caller passes the substituted order.
  // ponytail: the param_3 != 0 tail (l.123-424, FUN_1408C5900/CD5E0/76240/C8EA0 gates) is OPEN.
  constexpr uint8_t kUnranked = 0xA0;  // the 0xA0 fill of the rank bytes
  std::array<uint8_t, kSlots> rank{};
  rank.fill(kUnranked);
  std::array<int, 4> seen{};
  for (int i = 0; i < kSlots; ++i) {
    const int slot = order[i];
    if (active[slot] == 0 || cls[slot] == 0) continue;
    const int word = a[slot];
    if (word < 0 || word > 3) continue;
    rank[slot] = static_cast<uint8_t>(seen[word]);
    ++seen[word];
  }
  return rank;
}

float Ce930Key(float depthKey, bool special, float f2) {
  // FUN_1405CE930 (VA 0x1405CE930, lane-leaves §3.1): an A64410 slot whose S+0x3AA4-depth row
  // is above 0 sorts at +100000.0 (DAT_1425C65FC, fVar22); otherwise the |depth - ballX| key.
  // ponytail: 100000.0 is DAT_1425C65FC, the same install word as the fold's initial best.
  if (special && f2 > 0.0f) return 100000.0f;
  return depthKey;
}

void SortLaneSlots(const std::array<uint8_t, kSlots>& slots, const std::array<float, kSlots>& keys,
                   std::array<uint8_t, kSlots>& order, std::array<uint8_t, kSlots>& rank) {
  // FUN_140A79640 with param_2 = 0 (lane-leaves §1): insertion sort, ascending; equal keys keep
  // their pre-sort relative order (slot order from the compact step). Rank = index in the order.
  order = slots;
  for (int i = 1; i < kSlots; ++i) {
    const uint8_t s = order[i];
    const float key = keys[s];
    int j = i;
    while (j > 0 && keys[order[j - 1]] > key) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = s;
  }
  for (int i = 0; i < kSlots; ++i) rank[order[i]] = static_cast<uint8_t>(i);
}
}  // namespace pes::mentality
