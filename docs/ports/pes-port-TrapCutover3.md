# pes-port-TrapCutover3: the live-ball stall is a ball-dodge gate, not the restart taker; a refused receive falls through the reaction walk like PES's; S5 decoded, not ported (30-10-26)

Branch `StoppageFlow`. Commits (confirm with `git ls-remote origin StoppageFlow`): `b4f9c844` merge of `origin/main` (AO, `GATE_TIER`), `41f83d6f` the fix below. Tags: **CONFIRMED** = a tool run this session; **INF** = inference; **OPEN** = blocker named. Image `pes_full.bin`, base `0x140000000`.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, push | DONE (`b4f9c844`; conflicts: `CMakeLists.txt` pesfmt source list, `gen_translated.py` SETS: both sides kept) |
| 2. restart taker's reaction walk; `pes_stall_test` live-ball green | **GREEN, CONFIRMED**: `pes_stall_test` 2/2 (`ALiveBallNeverLiesStillLongEnoughForTheBreaker` 0 firings; was 140), on PES's chain, no weakening. The premise was wrong (section 1); the root cause is two defects (section 2) |
| 3. S5 action tick | **NOT PORTED. Blocker named (section 4).** Decoded, replayable, mis-description of `FUN_1406F02F0` corrected; the 12 captures exist; `sqrtf` import model added for the snapdiff replay |
| 4. Animetable mirror flag | RESOLVED as to where it lives (section 5); GF's unmirrored play still differs from PES on the captured trap (OPEN, not fixed) |
| 5. smoke determinism, `GATE_TIER=fast gate.sh` | **NOT RUN** (load 30-50; each stall run took 8-18 min; the budget went to the red/green/mutation cycle). Interim: `pes_dodge_test` 47/47 (45 + the two new, run on the final tree), `pes_stall_test` 2/2 |

## 1. What the 140 firings were (CONFIRMED, per-tick traces, scratch only)

- The `[stalemate]` line's `taker H2 ... piece 1/5 ... started 1 active 0` is the **stale referee buffer** (the last restart, long taken: `buf.taker` is never cleared), not a released taker. The two episodes of the seed-7 2-minute match are open play: a loose ball at 0.08 m/s at (3.8, -30.6) and (-25.7, -3.5) that nobody plays for five seconds. The 140 firings are two episodes of about 70 ticks.
- `_ReactionCommands` per tick for the men within 3 m of that ball (new `[rdbg]` trace, removed): the **receive entry (`kReceive`) wins every tick** for the designated man (H9, 0.9 m off the ball), the trap chain refuses it (kind 0, `out[0]` took 0, 1, 2, 4, 5, 6, 10, 11, 12 over the run: a ball at rest is not a ball arriving), and `_TrapCommand` queued nothing, so he stood there; the nearest opponent (A10) had **`kDodgeBall` chosen every tick from 3 m out** (`dB=1` with the ball at 0.08 m/s) and played dodge Special clips (`curfn=14`, frames cycling) and then the block entry (`kBlock`, `bl=1`), never touching the ball. Both men are in the 5-s window; nobody ran at the ball.
- Why the earlier fall-through experiment (140 -> 1226) went wrong: with the refusal falling through, the ungated ball dodge fired for the refused men.

## 2. The fix (two PES-faithful changes)

1. **Ball dodge entry head `FUN_140a070e0`** (headless decompile, `dec.sh`; the bridge has no function there): `uVar1 = man id; fVar12 = *(float*)(MA + 0x24710 + (rank[id] (MA+0x247BE) + 11*half) * 8)` (his entry of the ball-distance table, a squared distance); `fVar10 = FUN_141E532D0() (fps) * _DAT_142590D98 (0.3)`; `pfVar6 = FUN_140A57450(B, buf, 0)` = the ball's per-frame step (pes-re-locomotion §6: `p+0x2948/0x294C/0x2950 / 54`); the entry runs only if `fVar12 <= (fVar10*pfVar6[2])^2 + (fVar10*pfVar6[0])^2`, i.e. his squared distance to the ball is within the square of (ball speed x 0.3 s) (fps cancels). Only then `FUN_140A072D0` (dispatcher: `FUN_1406B4C10(anime,0x15)`, `FUN_140CA1A50(0x23)`, eligibility `FUN_140A07BF0`, the sub-tests) runs. GF's `_DodgeBallCandidate` had the eligibility and the selector but not this head, so a ball at rest within 3 m was dodged by every eligible man. Ported: `pes::BallDodgeEntryGate(k, manDistSq, ballSpeed)` (`pesdodge.cpp`), constant `DodgeConstants::ballLead_s` read from `0x142590D98` (the word `ballReachMargin_m` already reads, 0.3), wired at the head of `ElizaController::_DodgeBallCandidate`. `ponytail:` in code: PES's table entry (`MA+0x24710`, sorted by distance to the ball) is stood in by his squared distance to the ball now, as the receive entry's table read already is; removed by porting the table writer (the translated readers `1404C8E60`, `1405A7FE0`, `1404AFC60` read it; the writer is not ported).
2. **Reaction walk continues past a refused receive.** `FUN_1409F8220` returns 0 on a refusal (`0x1409F8538`, CONFIRMED in TrapCutover2): `PlayerController::_TrapCommand` now returns bool (false = refused, nothing queued), and `ElizaController::_ReactionCommands` re-runs the chooser with `receive` cleared, so the refused man takes the next entry that says yes (the table order is the existing `ChooseReaction`: tackle, slide, receive, the four dodges, block, jostle). The old `ponytail:` that deferred this is deleted.

## 3. Tests and mutations (run, rebuilt, reverted)

- `pes_dodge_test` (new): `EntryGateWantsTheBallToReachHimWithinTheLeadTime` (a boundary at speed x lead, both sides), `ABallAtRestIsNeverDodgedByAManWhoIsNotOnIt` (0.5, 1, 2, 3 m off a 0.08 m/s ball: refused); install test: `ballLead_s > 0` and equal to `ballReachMargin_m` (a relation, no pinned value). Red before: they do not compile on `origin/main` (no `BallDodgeEntryGate`); the stall test failed with 140 firings.
- **M1** gate always open: both new tests FAIL (45 pass, 2 fail). Reverted.
- **M2** no fall-through (refused receive keeps the choice and queues nothing, gate kept): `pes_stall_test` live case FAILS (nine `[stalemate]` reports, one ball lying still 17 s, `stall_m2.log`): the ball dodge gate alone is not enough, the refused men must take the later entries. Reverted (tree equals `41f83d6f`).
- Not mutated: the ball-speed term (covered by the boundary test only through the product), `_TrapCommand`'s bool.

## 4. S5: the action tick `FUN_140797CD0` (CONFIRMED decompile + captures; NOT ported)

- Captures: `live2/pid483669/seq6634..6678` (12, `trap_action_tick_797CD0`, one man, `P = 0x3D06A570`, action object `rcx`) plus 6 `trap_kind0_79A470`. A replay with `frame_trace.py recorded` stops at `ucrtbase.dll!sqrtf` ("needs an import model"): the recorded mode loads no import models; `snapdiff` does. Added `ucrtbase.dll!sqrtf` to `tools/dev/translate/import_stubs.json` (`code_hex f30f51c0c3`, `sqrtss xmm0,xmm0; ret`; the export's own code differs from that only for NaN and negative arguments: OPEN, not checked on the captured arguments).
- Corrections to `pes-re-trap-inputs.md` S5: `FUN_1406F02F0(P, k, mode)` is **not** the motion issue: it sets `P+0x2704+4k = mode` (`k < 0x13`, `*(P+0x3AA8)` hook `FUN_14081DA00/D920`). The class `k` is `FUN_1406B0180(table, motion, mirror, segment)`, a motion-class lookup (`((rec>>0x1B)+1)`, or `DAT_1425DC5A0[class]` when the mirror byte is set).
- The anime layer at `P+0xAB8` (vtable `0x1425E3508`; accessors disassembled from the image): `+0x18` -> `P+0xAE0` (u16 motion), `+0x20` -> `P+0x245C` (the phase byte the tick switches on), `+0x30` -> `P+0x2472` (mirror byte), `+0x40` -> `P+0x2471` (segment index, incremented by the tick). The 12 captures: phase 0 (1 capture), 1 (4), 3 (7); segment 0/1; the committed plan `action+8` = `0x9C4` then `0x9DC`; `P+0xAE0` = `0x23` then `0x32B`.
- The tick is the phase machine (phases 0..9; cases call `0x140796A40`, `0x140796B30`, `0x140796CE0`, `0x1407976D0`, `0x14079E530`, `0x14079EB30`, vtable slot `+0x58`), with the root solver `FUN_1406E85B0` and a command record dispatched through `FUN_1406D9010(P+0xC40, 5)`'s vtable `+0x38`. `closure.py --dry-run` of the root: **884 members, 487 not yet translated** (the direct-callee walk reaches far outside the Trap action; the earlier trap closures were capture-bounded by `snapdiff`). Porting it means: a capture-bounded closure (`snapdiff` over the 12 captures), the translated layer growing by the reached members, the GF window gaining the anime layer object and `P+0xC40` slots, and replacing the play-at-once Special with the tick. Not started: each build of this tree took 10-35 minutes under load.
- Stand-in 2 in `_TrapCommand` stays (named `ponytail:`, corrected text).

## 5. Animetable mirror flag (item 4)

- The Animetable buffer stores the mirror flag as **one byte per cell after the s16 cells** (`FUN_1406AA440`, `pestrapdata.cpp:29`), not as a bit of the motion number.
- In the action tick the mirror is read from the **anime layer's byte `P+0x2472`** (vtable `+0x30`) and passed to `FUN_1406B0180` (class remap `DAT_1425DC5A0`) and the segment-clip lookups. On the 12 captures `P+0x2472 = 1` for the committed trap (this clip is played mirrored in PES). GF plays the clip unmirrored (`AddPesClips` / the Special command), so GF's trap differs from PES for that clip: OPEN (needs the tick or the layer writer: who writes `P+0x2472` at the motion issue, `FUN_1406F0F70` SetMotion's `mirror` argument).

## 6. Verification

- Ran: `pes_dodge_test` 47/47 on the fix; `pes_stall_test` 2/2 on the fix (`stall4.log`); M1 and M2 (section 3).
- Not run: `GATE_TIER=fast tools/dev/gate.sh`, the 2-min smoke determinism lines (both paths), `gen_translated.py --check` (no translated file changed; only `import_stubs.json` gained an entry, which `xlate.py` reads only for a slot call into `ucrtbase!sqrtf`: none today), the other `pes_*` binaries that link `elizacontroller.cpp` / `playercontroller.cpp`. Main gates.
- Scratch: `<workdrive>/gf-TrapCutover3-scratch/` (decompiles `d_*.c`, `dec1.txt`, runs `stall*.log`).

## 7. Open items

1. S5 port (section 4): needs the capture-bounded closure and the GF window objects; start from `snapdiff.py --only 0x140797CD0` over `live2/pid483669/seq6634..6678` with the `sqrtf` model.
2. Entry 4's table: port the `MA+0x24710` / `+0x247BE` writer so the gate reads PES's entry, not GF's distance.
3. The ball dodge dispatcher `FUN_140A072D0` (event-kind branch, `FUN_140A07570` / `FUN_140A07880` sub-tests): ported as pure functions (`BallDodgeSubTestFast/Mouth`) but still not wired; the block (`bl=1`) and human-dodge (`dH=1`) predicates fire for a still ball in the trace and are reached now that a refusal falls through: audit their gates against PES the same way.
4. Mirror byte `P+0x2472` (section 5).
5. Fast gate and smokes (section 0 row 5).
