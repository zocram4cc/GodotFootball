# TeamTick: PES's pass-target selector chain (0x1404FB2A0 -> 0x1404FB380 / 0x1404FB780 -> 0x1404F0E50) runs on GF's window every team tick; the clear predicate settled from the assembly; four chain roots proven bit-exact on real captures; the pes-loop smoke now aborts at an untranslated keeper unit, so the branch is not landable yet (05-11-26)

Branch `TeamTick` from origin/main bf5954bd, pushed `a0c22c97` (first checkpoint `367718d5`). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Acceptance (`GATE fast: GREEN`, the smoke explained by an ablation) is **NOT met**: section 7.

## 1. The clear predicate (CONFIRMED, assembly)
`0x1404FB380`, read in full (253 lines):
- `0x1404FB3BD CALL 0x140A627E0` (X = [Y+0x6D48], edx = 0) answers `[[X+0x18]+0x2601C]` = MA+0x2601C, the ball owner. `0x1404FB3E7 CMP EBX,0x15` / `0x1404FB3EA JBE 0x1404FB71F`: **owner <= 0x15 (a man owns the ball) clears T+0x78** (`0x1404FB71F..72D`: `[T+0x78] := 0xFF`, `[Y+0x6D04] := 0`, `[Y+0x6CFC] := [Y+0x6D00] := 0xFF`). The Ghidra decompile (`carrier < 0x16 -> clear`) is right; `pes-re-lane-leaves.md` section 4 ("`u > 0x15` clears") is wrong.
- `0x1404FB3F3 CALL 0x140A57940` (ball record byte B+0x2928) / `0x1404FB3FA JZ 0x1404FB71F`: ball inactive clears too.
- Then `ESI = 0x140A51320(X)` (frames since the newest ball event: `MI+0x1198 - ring0.frame`, `0x1404FB44C JG`): `ESI <= 0` clears (`0x1404FB44E..47B`). With T+0x78 invalid (> 0x15) and `T+0x255 == 0` the tree recomputes only when `ESI > 3` and `ESI % 7 == 0` (`0x1404FB6E8..6FE`, magic `0x92492493`); with the latch set, on its own ESI cadence (`0x1404FB6B2..6CF`); the recompute is `0x1404FB702..71D: T+0x78 := FUN_1404F0E50(Y, old)`.
- `0x1404FB2A0` (61 lines): `MI+0x1775 == 0xC` returns; `MI+0x116C == 1`: if `[G+0x3B0]` is non-null and `0x1423E7FD0` (byte +0x9C) and `0x140A431E0` both hold -> `T+0x78 := 0xFF`, return 0, **else it calls `0x1404FB380`**; `== 2` -> `0x1404FB780`; `== 3` -> `0x1404FB380`; any other phase -> `T+0x78 := 0xFF`. lane-leaves has the arms of 1 and 3 mixed up (in play runs the 4FB380 tree).
- Live, real captures (`live2/pid281026`): seq732 (side 0, frame 640, ring-0 kind 1 = a kick at frame 613): MA+0x2601C = 0xFF, T+0x78 = 9. seq850 (frame 740, a touch at 711): MA+0x2601C = 9, T+0x78 = 0xFF. The target lives while the ball is loose and dies at the receiver's touch.

## 2. The team tick and its records (prior decodes reused: `pes-re-teamtick.md` 2.2, `pes-re-lane-leaves.md` 4, `pes-re-holder-tree.md`, `pes-re-press-coord.md`)
Caller chain: `FUN_1404F6C30(Y, S, T)` (per-side team tick, called by the frame owner H `0x14041ECF0`, step H.d) -> `0x1404F784B CALL 0x1404FB2A0` (step 40c, after the latch and the MA build); rcx = Y.

| Record | Layout the chain reads | Writer in PES | GF now |
|---|---|---|---|
| Y (brain) | +8 side id (u32); +0x6D48 = X; +0x6D50 = T (bound for the tick, cleared at its end); words +0x6CFC/+0x6D00/+0x6D04 | the tick | `PesBrainRecord` (0x6E00 bytes) per side; side id and X at construction; T bound by `PesWorld::RunPassTarget` |
| X (ctx) | +0x18 MA, +0xE0 E | listener | already in the window |
| T | +0x78 pass target (u32 slot id, 0xFF none), +0x254 sign, +0x255 latch, +0x6E7, +0x218, +0x4C00 words; reads +0xB8CD | the chain (+0x78) | the window side record, now 0xB900 bytes (was 0xA2A0); T+0x78 starts 0xFF |
| MA | +0x2601C owner, +0x2482C ball copy, +0x24908 | W1 `0x14045725A` | owner staged each tick (4) |
| MI | +0x116C, +0x1198, +0x1748 + slot (22 on-pitch bytes), +0x1775 | clock / restart / roster | +0x116C from `PlayStateFor` for the call; +0x1748 staged each tick (new); +0x1775 = 0 (existing stand-in) |
| B | +0x2928 active byte, sample 0 (+0x88), +4 | ctor sets +0x2928 = 1 (`0x140A572B4`); setter `0x140A57990` only at `0x141FD00BD` (FixDemo) | +0x2928 = 1; sample 0 staged each tick (**no GF code wrote B's samples before**; MA's ball copy read zeros) |
| G+0x3B0 | skip object: +0x9C (`0x1423E7FD0`), +0x8C and +0xA0+0xE2A0 behind it | OPEN (0 in every capture) | `PesSkipRecord`, clear |
| G+0x180 | roster, 24 x 0x74, u32 +0x20 = slot id | roster loader | `PesRosterBlock`, +0x20 = 0xFF (as captured, in play too) |
| 0x14351AE50/54 | frame-rate cells: `0x141E532D0/E0` (F6110 is `(long)(fps*x+0.5)`, **not** noise) | engine | a segment holding `kPesTickHz` (54, as captured) |
| man records | +0x5D0 action word (5 = PASS_GET), +0x55C/+0x564 | mover `FUN_140A6CA10` | positions (existing writer); +0x5D0 staged (4) |

**Finding (CONFIRMED, seq732/seq850: 41,578 of 41,632 bytes equal): PES keeps two copies of the team record.** The tick's T (`Y+0x6D50`) sits 0xC144 bytes past the record G's side table names (`*(G+0x1928+0x28*side)`, `0x140A48A30`); they differ by what the tick wrote since (T+0x218 one frame ahead). The chain writes the first; the table's readers (press, mover `0x140931C60`, planner `0x1404405D0`) read the second. GF's window has one record per side, so the chain writes it in place. **OPEN:** where PES copies T into the table's record (the one-frame lag of every table reader is not modelled).

## 3. Proofs (CONFIRMED: snapdiff replays, PES bytes in the harness against the translated closure over each capture; return value and every written byte)
Real captures only (hooks `team_pass_*`, `live2/pid281026`), no synthetic case. Tool `tools/dev/translate/proof_teampass.py`; spec set `teampass` (88 closure members via `closure.py`); records in `results_teampass.json`, registered in `gen_translated.SETS`; the layer was regenerated (`0x1404F0E50`, `0x1404FB380`, `0x1404FB780` moved from `closure/` to `ai/`).

| Root | real calls | bit-exact | own-block coverage |
|---|---|---|---|
| `0x1404FB380` | 6 (seq709, 754, 9351, 9356, 11334, 11343) | 6/6 | 0.7015 (47/67) |
| `0x1404F0E50` | 1 (seq9357) | 1/1 | 0.285 (285/1000) |
| `0x1404FB780` | 4 (seq005, 010, 011, 012) | 4/4 | 0.0735 (5/68; all pre-kick-off phase 2) |
| `0x1404FB2A0` (already recorded in the impure set; not re-recorded) | 4 (seq003, 732, 850, 9244) | 4/4 | 0.625 (10/16) |

Coverage is low for F0E50 and FB780 (rare arms); recorded, not gated. More F0E50/FB780 captures are still landing (`wanted.txt`): re-run `proof_teampass.py` as they arrive. `gen_translated.py --check` was NOT run after the regeneration (it re-translates: 8 min under load); `ledger.py --check` not run.

## 4. GF integration (`team.cpp`, `team.hpp`, `match.hpp`, `humanoid.cpp`, `teamAIcontroller.cpp`, `pesworldrecord.*`)
- `Team::RunPesPassTarget` (team tick step 40c position: after the mark table, before the in-play AI) stages the records the chain reads and runs `PesWorld::RunPassTarget(side, playState)` on the window. `Team::GetPassTarget()` answers from the window's T+0x78 on the PES path (`Match::GetPesSlotPlayer`); the kick no longer writes a target there; the press's `teamKickMan` reads T+0x78 instead of the newest-touch stand-in it had (`teamAIcontroller.cpp`). GF mode (switch off) keeps its own `passTarget`, cleared at ResetSituation, as before.
- Named stand-ins (`// ponytail:` in `team.cpp`): MA+0x2601C is GF's ball retainer, not W1's persisting holder (the captured owner is 0xFF between the kick and the touch; W1's port keeps the holder through it) - replace with W1's own write once proven against the captured owner; man+0x5D0 is 5 for the man the mover sends (`PassGetReceiver`) and the captured idle word 1 for the rest (`FUN_140A6CA10` is not ported); the roster, the slot records' +0x1430 counters, the anime class byte and the MA ball-distance table behind `0x14044E0D0` are GF's existing run-world stand-ins or absent. Rule 8: these inputs have no GF source; each writer is named.

## 5. Tests
- `pes_teampass_oracle_test` (`GF_PES_TEAMPASS_ORACLE`, oracle dirs from `teampass_oracle.py`; the captures must be unpacked): each capture's live-in bytes rebuilt in a `PesWorld` at PES's offsets; GF's window run must leave PES's T+0x78, perform PES's store to it, and return PES's answer byte. **4/4 captures pass** (seq003 phase 0; seq732 in play, kept target 9; seq850 owner clears; seq9244). Weakness: no captured call changed the target (255 -> x) or hit the stoppage arm, so those paths are proven by section 3 only.
- Mutations run (rebuilt, run, reverted): (C) T not bound (`Y+0x6D50` store removed): killed (4 failures, in the poison-variant of the test; the final write-log variant was not re-run on it). (A) play state forced to 1: **survived** (no captured selector call is in phase 2 or 3). Not run: the fps cell, the ball-active byte, the roster default (the oracle copies the capture's bytes over them).
- `pes_pass_target_test` (a PES-loop match, 2 min, seed 7: a target is named in flight; none stays set past PES's live bound 2.94 s with the ball at rest): written, **never ran to completion** (aborts, section 6). Not a red-first test on main: the stale episode needs ~295 s of seed 7.

## 6. Smoke
`gfsim_run --seed 7 --minutes 2 --pes-loop` on this branch: **aborts** `PES_DISPATCH_UNTRANSLATED: 0x142132e20` (a data reference in the vtable at `0x142BEEFE0`) after 21 min of wall time queued under load 20+ and a few minutes run. `0x142132E20` is the keeper's GKDribble carrier unit (kind 19, `pes-re-A4Carrier.md` row 19 "type 3 absent"; its `closure.py --dry-run` is 27 members). INF: the changed pass targets change the trajectory (the main smoke `85c7d7c515cf92ae` finishes) until a keeper owns the ball and the carrier walk picks kind 19. Not diagnosed further, not ablated, no census. Switch-off smoke not run (the change is PES-path only; `399e180f39459c2b` should be unchanged: INF).

## 7. NOT done / resume state
1. **The keeper unit `0x142132E20` (GKDribble) must be translated and proven (27 members; needs real captures of the kind-19 step: the carrier hooks, `carrier_clear` family) before any pes-loop match survives**, or the path is taken by a pass-target consequence that should not occur (check first: print the tick's stack at the abort - the core of the run is lost; rerun with `GF_PES_SURVEY` from the SurveyTool branch). Then: the smoke line, the ablation (revert `Team::RunPesPassTarget`'s call and the `kickMan` read separately), `pes_pass_target_test`, `GATE_TIER=fast tools/dev/gate.sh <workdrive>/gf-TeamTick/build` with the brief's env, census in the merge lane.
2. `gen_translated.py --check` and `ledger.py --check` after the regeneration; F0E50 (1 call) and FB780 (4, one phase) coverage from the captures still landing.
3. Whether GF's staged records equal PES's in a match (roster, class bytes, +0x1430, the MA table, the owner, man+0x5D0): unmeasured; compare per team tick against a live capture.
4. PES's T -> table copy point and the writer of G+0x3B0's byte +0x9C (OPEN, section 2).
5. Correct `pes-re-lane-leaves.md` section 4 and `pes-re-teamtick.md` 4.2 to section 1.
6. Tests run: `pes_teampass_oracle_test` 4/4 (unpacked captures). Not run: `pes_holder_latch_test`, `pes_mentality_test` (built), any gate test.
