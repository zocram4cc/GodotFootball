// PES 2021's team-possession latch: the team record's +0x255 byte, evaluated by
// FUN_1404fe9c0 in the team tick. The stand-ins for its inputs are listed in
// docs/re/pes-re-dodge-gates.md.
//
// Inputs GF has no counterpart for are supplied by the caller. Each such stand-in
// carries a `ponytail:` note at its use, naming the PES address it stands for, so it
// can be replaced once GF carries that input.
#ifndef _HPP_ONTHEPITCH_PESPOSSESSION
#define _HPP_ONTHEPITCH_PESPOSSESSION

#include <cstdint>

#include "onthepitch/pesreceive.hpp"

namespace pes {

struct PossessionLatchInput {
  int team = 0;             // side 0 or 1 (the latch's ctx+8)
  bool old = false;         // T+0x255 before this frame

  // Rule R (FUN_1404fe9c0 lines 17-26; FUN_140a3c950). MI+0x116c, MI+0x1574, MI+0x1558.
  int restartState = 1;     // 1 open play; 2 or 3 restart
  int restartGate = 0;      // MI+0x1574 (R is off at state 2 when this is 1)
  int restartTeam = -1;     // MI+0x1558: the side taking the restart

  // Ranked-list heads: u32 at MA+0x24840 + 0x58*side, valid when < 22.
  bool ownHeadValid = false;
  bool otherHeadValid = false;

  // Step 1: the carrier (MA+0x2601c, FUN_140a627e0(ctx,0)).
  bool carrierValid = false;
  int carrierSide = 0;

  // The newest event (E+0x180698, FUN_140a3e190(E,0)): kind at +0 (PES numbered
  // kinds; 0 when there is no event), frame at +0x10, actor at +4 (FUN_140a62ea0).
  int newestKind = 0;
  int newestFrame = 0;
  bool actorValid = false;  // FUN_140a62ea0(newest+4) < 22
  int actorSide = 0;
  int newestSubKind = 0;    // +0x40
  bool newestFlag2d = false;  // byte +0x2d != 0
  int newestField1c = 0;    // +0x1c

  // Clock: the frame counter (MI+0x1198), the game speed (the install's 54).
  int frame = 0;
  int gs = 54;
  uint32_t ballSumRaw = 0;  // MI+0x119c, raw bits (3a's sum test)
  bool stateFlag40_80 = false;  // G+0x40+0x80 byte != 0 (the sub-3 exclusion)

  // Step 3b: the holder (MA+0x2483c), the chase predicate, and the two clauses.
  bool holderValid = false;
  int holderSide = 0;
  bool chaseBlocked = false;        // FUN_1404f4be0 != 0
  bool holderTeamLatch = false;     // the holder side's own T+0x255
  bool otherHeadFlagClear = false;  // FUN_140a489f0(G, other head).+0x1470 == 0
};

// PES's contest predicate FUN_1404F4BE0 (docs/re/pes-re-first-touch.md 3.2), in the gates GF can
// express. Returns true while the holder's side is still contesting the loose ball, so latch step 4b
// does not award it. ponytail: not ported: the zone gate (zone 0 returns false), the budget overrides
// (zone 1 halves gs, the near-ball override sets gs, the +2gs term, the facing halving), the event
// branch (FUN_1404C8EA0 < 64), the time-since-event branch, and the leader branch (dS + 12 > the other
// side's leader). The budget here is the game speed (zone 2, no overrides).
struct ContestInput {
  bool holderValid = false;           // holder id < 22 (FUN_1404F4BE0 returns 0 above 21)
  bool teamHasBall = false;           // the holder's side's T+0x255 != 0 (returns 0)
  bool otherHeadValid = false;        // the other side's head is on the pitch (returns 0)
  float holderArrival_frames = 0.0f;  // FUN_14044E020(H, s): stand-in, time to the ball in frames
  float otherArrival_frames = 0.0f;   // FUN_14044E020(H, oth): stand-in
  float budget_frames = 0.0f;         // the zone budget (stand-in: the game speed)
};
bool ContestBallPredicate(const ContestInput& in);

// The latch's value for `in.team` at this frame; the caller stores it.
bool TeamPossessionLatch(const PossessionLatchInput& in);

// PES's logic frame at `ms` real milliseconds: the logic steps `gs` times per real second
// (one 1/54 s step per frame), so the frame is floor(ms * gs / 1000). MI+0x1198 counts those
// steps (docs/re/pes-re-clock-live.md section 3: about 57 per wall second in play).
int PesFrameOf(uint64_t ms, int gs);

}  // namespace pes

#endif
