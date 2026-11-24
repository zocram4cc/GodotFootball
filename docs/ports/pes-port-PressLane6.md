# PressLane6: ActionPress wiring round 3 (wip patch applied, order and stand-in measured, writer OPEN): NOT LANDED

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`, tip `f95743c1` (Integrate `c4404052` merged). Nothing committed or pushed.
The worktree is reverted to the tip (`git status` clean). The three experiment states are kept as patches in
`<workdrive>/gf-PressLane4-scratch/`: `actionpress_wip.patch` (step 1), `wip_order_nostandin.patch` (steps 1-3, the last state measured).
Gate: not run (Main gates). Rig requests: 0 (static decodes only; no live reads, no hooks).

## 0. Result (CONFIRMED runs unless marked)

| state | ALoose (green) | ADesignated (green; unnamed of chased) | PesStallQuick (<= 53) | PesStall.ALiveBall (<= 2229) |
|---|---|---|---|---|
| merged tip, no wiring (PressLane5 section 9.1) | RED 21 of 10,022 (round 1) | RED 2 of 369 | 53 | PASS (count not printed) |
| 1. wip patch (`actionpress_wip.patch`) | **GREEN** (638 s) | RED 30 of 380 | **6623** (PASS not reached) | 19247 (PressLane5; not re-run) |
| 2. + designated owner recompute moved before the team tick | GREEN | RED 30 of 380 (no change) | 6623 (no change) | not run |
| 3. + press man = loose presser only (no designated stand-in) | not run (quick first) | not run | **9130** (worse) | not run |

Neither ratchet holds in any state. Per land-or-revert, nothing lands and the tree is reverted to the tip.

## 1. Wiring (step 1): applied, builds, ALoose green

- The patch applies cleanly to `f95743c1`. Both test binaries build.
- The three PressLane5 gaps are in the patch: G+0x3B0 skip pointer stored (`pesworldrecord.hpp`), P+0xAC0 speed-profile vtable read
  from the install (`pesimagetables.cpp`), interface vtable span 0x100 (`pesimagetables.hpp`).
- Run env needs `PES_EXE=<home>/ghidra-projects/scratch/Unpacker/pes_full.bin` as well as `PES21_ROOT` (without it every sim test stops at
  "install constants unavailable"). Not a code change.

## 2. Step 2 (designation order): moved, no effect on the numbers

- PES's designated owner MA+0x2600C and holder MA+0x2601C are both written by the team-AI recompute `FUN_140457070` on the listener
  step-5 path (`pes-re-holder.md` section 3, W1 at `0x140457259`), which precedes the team tick (`pes-re-holder.md:111`). Placement INF.
- GF's `RecomputePesDesignatedOwner` ran after the team ticks. It now runs beside `RecomputePesHolder` (`match.cpp:917-920`).
- **Finding:** the stand-in reads GF's `GetDesignatedPossessionPlayer()` (set after the team ticks, `match.cpp:963-983`), not
  `pesDesignatedOwner`. So the move changed nothing (30 of 380 and 6623 unchanged). The stand-in's own designation has no PES writer in
  that block: reordering cannot fix it.

## 3. Step 3 (writer of AI+0x1414, the presser base): OPEN

CONFIRMED (VA, tool):
- **Reader** (translated `pes_fn_140639100`): `0x140639136 LEA R14,[RAX+0x1414]`, RAX = `FUN_140A489F0(G, slot)` (`0x14063912F`).
  Point = x @ +0x1414, y @ +0x1418, z @ +0x141C.
- **AI record:** `FUN_140A489F0` returns `*(G + (slot+0xF3)*0x28)` for slot < 0x16, else `*(G+0x25F8)` (decompile).
- **Constructor zeroes it:** `FUN_1421ADC70` stores 0 at `+0x1414` (`0x1421ADD16`, decompile). A fresh record's base is zero, which is why a
  far designated man walks to a point 0.2 m from himself.
- **The only +0x1414 copy** is the copy-assign `FUN_140431800` (`0x140431B0F..B2F`, three dwords src+0x1414 to dst+0x1414, decompile
  `dec_anchor2.txt`), reached through the adjustor thunk `0x1404335C0` (`lea rdx,[rcx+0x15C8]; add rcx,8; jmp 0x140431800`): dst = O+8,
  src = O+0x15C8. The staging copy of the anchor is at O+0x29DC.
- **Writers of the staging copy:** not found. Image-wide scan of memory writes at disp 0x29DC..0x29E8 (`scan_writes.py`): four hits, a ctor
  `FUN_140A44940` and an unrelated object `FUN_1423571E0`. LEA scan (`scan_lea.py`, 34 sites of `LEA reg,[base+0x1414]`): no computed-base store
  except the copier. The copier has no E8 caller (`scan_calls.py`), one E9 at `0x1404335CB` (`scan_jmps.py`, the containing function has no
  .pdata), and no qword vtable slot holding the thunk (`.rdata` search).
- Verdict: **OPEN**. Next step: decode the caller of the thunk table `0x1404335xx`, or ask Main for one hook on `FUN_140431800` with a
  predicate on the destination slot (cap 12, FULL) to capture a real copy and read its source.
- Rule 8: the anchor is not proven out of reach, so the GF stand-in (`GetAdaptedFormationPosition`) is not added.

## 4. Step 4 (ActionPress only where PES runs it): the designated stand-in is not PES's mechanism

- ActionPress `0x140962440` is dispatched for the PRESS state 0x34 of the press writer `FUN_140536FC0` (`pes-re-first-touch.md` 7.1). Its
  free-ball arm needs a candidate within 9 m and the top one within 8 m. PressLane5 found 0 of 4527 free-ball ticks with a PRESS man (seed 7).
- The designated owner is **refused** reaction entry 3 (`0x1409FA57B`) and **admitted** the MOVE-tick TRAP 7 via `FUN_1407B05F0` /
  `FUN_1407B3470` (`0x1407B0D42`, `0x1407B35C9`; gates `*(ball+0x5C3)`, `plan+0x35`, `act+0x70`, `FUN_1406EBF90`), per
  `pes-re-walker-touch.md` section 1. GF has no MOVE-tick TRAP path (section 2 of that doc).
- So ADesignated's premise (the designated man in reach is commanded to the resting ball with a named ActionPress target) is a GF
  stand-in. PES's rule for him is the MOVE-tick TRAP, not ActionPress.
- Removing the stand-in (state 3) makes the quick census **worse**: 9130 vs 6623. The breaker then forces other men live, so the forced live
  ball is produced by the GF breaker (`match.cpp`, stalemate recovery), which has no PES writer. The breaker, not the designation, is the
  larger share of the stall.
- The ADesignated test is NOT changed. Rule 3's rewrite needs the MOVE-tick TRAP port to go green, so it cannot land as a green commit now.

## 5. Next exact steps (for Main)

1. Decode the writer of the staging anchor O+0x29DC (section 3), via the thunk-table caller or one hook on `FUN_140431800`.
2. Port the MOVE-tick TRAP admission for the designated owner (`FUN_1407B05F0` / `FUN_1407B3470`, section 4). Then rewrite
   ADesignated to PES's rule (TRAP admitted, or a named target), red first, with the decode in the commit.
3. Replace the breaker's designation of a live man with PES's own rule (the breaker has no PES writer: decode it first, `match.cpp`
   stalemate block), since removing the designated stand-in alone makes the stall larger (9130).
4. Then run the four numbers: ALoose, ADesignated, PesStallQuick, PesStall.ALiveBall with the count printed on pass.

## 6. Blockers

- The anchor writer (section 3): without it the presser base is zero (far walk); the GF stand-in is not PES.
- The designated-owner stand-in (section 4): not PES's mechanism; the breaker's live designation, which the stand-in feeds, is GF's.
- Builds waited 14-40 min each on `/tmp/gf-build.lock` behind other agents' builds; ALiveBall was not run (quick is 125x over its bound, and
  PressLane5's 19247 is on the same patch).
