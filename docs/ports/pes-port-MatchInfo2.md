# World window: the ball velocity, E, ctx, MA and the Clear unit block are GF state; kind 1 runs the translated Clear unit (MatchInfo2, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`), pushed head `928373429f6c075f7514acba87fa8857608d4b94` (`git ls-remote origin EndClear`); earlier push `6df7caa0`. `origin/main` (`a99d31c9`) merged in. Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Capture: `live2/pid407792/seq021` (oracle dir `gf-EndClear-scratch/oracle/seq021`, PES-derived, not committed).

## 0. Verdict

| Step | State |
|---|---|
| 1 `MoveStepWire.TheKickoffTakerClosesToTheBall` | **Diagnosed, NOT fixed.** Base `origin/main` (`a99d31c9`): `first 4.38 closest 0.50`, passes. EndClear: `closest 0.80`, fails. With `Match::GetPesFrame` temporarily put back on the old derived value (`GetActualTime_ms() * 54 / 1000`) the same build gives `closest 0.50`, passes (experiment reverted). So the regression is **new**, caused by the stored MI frame replacing the lagging derived frame (CONFIRMED by the three runs). Re-stage by seed search or find the PES-faithful cause: not done (budget). |
| 2 ball velocity | **DONE.** `Ball::momentum` and the dead `previousMomentum`/`previousPosition` are deleted; `Ball::Momentum()`/`StoreMomentum()` read and write the window's B+0x2948; `GetMovement()` returns the window. `SetMomentum` used to leave the record stale until the next `Process`; it is now in the record at once. |
| 3 ctx, MA, E in the window | **DONE for E, ctx and the angle's MA fields; MA's sorted lists are a copy (OPEN).** E is `EventStoreImage` bytes at PES offsets in the window; `EventStore` is a view over it. ctx (per side, +0x18 -> MA, +0xE0 -> E) and MA (per side) are window regions. The angle oracle runs with no test-supplied segment. |
| 4 danger flag | **NOT done as asked.** `0x140699960` (closure: 108 functions, `closure.py --dry-run`) and the builder `0x142119390` (193) are not translated; no captures; the closure reaches lane/team records GF does not hold. The flag is now window state (`imageA+0x31C1`) with ONE writer site (`elizacontroller.cpp`, a named `ponytail:` stand-in); the two stand-in expressions (`:2774`, `:2951`) are replaced by reads of the window byte. |
| 5 kind 1 | **Wired, partly.** `FillUnitTable` kind 1 takes the translated Clear unit's answer (`pes_fn_142109EB0` run on the window by `PesWorld::RunClearUnit`); the walk's kind-1 winner issues a clearance toward the unit's angle (`_AddClearance`). `forcedClear` is NOT set (`0x140A4C820` undecoded: the rule-7 replacement always applies unless it is set). |

## 1. Step 1 detail

- Runs (same test, `PES21_ROOT` set, slot 2-3): base 0.50 (pass); EndClear 0.80 (fail, 342 s full-file run: 27 passed, 1 failed); EndClear + old derived frame 0.50 (pass).
- PES's counter: `0x140A128E4` (`inc dword [rsi+0x1198]`) sits in the tail of a function that first computes `edi = [r14+8] - [r14+4]` (a frame duration), adds it to `[rsi+0x118C]` and `[rsi+0x1194]`, then divides by the counts: **INF** it is the end-of-frame statistics block, gated on `[r14+0x31A] != 0`; consumers during frame N see N completed frames, so the first tick reads 0. GF's `AdvanceMatchFrame` is at the end of `Match::Process` (same order). The block also stores `[rsi+0x1E0]` (an average), which suggests MI+0x1198 may count only gated frames: OPEN. Value check on seq021: MI+0x1198 = 881, MI+0x118C/0x1190/0x1194 are about 2.04e9 (not counters).
- Why the test moved: the old derived frame was `floor(truncated integer ms * 54 / 1000)`, which lags the true tick count by 0-1 and repeats values; the stored counter is exactly one per tick. INF: the kickoff taker's timers (restart delays keyed on the frame) now fire up to one frame earlier, moving his approach. Not verified by a trace.
- Next: seed search as in `pes_restart_delay_test` (re-stage), or trace which PES timer in the approach path reads the frame.

## 2. Window layout added (`pesworldrecord.hpp`, `pesevents.hpp`)

- `EventStoreImage` (1.5 MB, `static_assert`ed): rings at `r*0x1908`, overflow at `0xFA50`, 55 kind blocks at `0x1C258 + K*0x328`, log at `0x26FF0`, newest-event payload copy at `0x180698`, second copy at `0x1806FC`. Corrections to the old GF store: 55 kind blocks, not 56 (PES's run ends where the log begins, so a kind-55 block would be the log; kind 55 is the 0x37 sentinel); the newest copy is 0x60 payload bytes with an empty-record header before it. Two GF-only presence flags remain in the image (`gfHasNewest`, `gfHasNewestSecond`): PES has no empty test, GF's readers answer null until the first kind-7 route.
- `Match::pesEvents` is deleted; `Match::GetPesEvents()` and the event stage take `pesWorld->Events()`.
- ctx: `PesCtxRecord` per side, `ctx+0x18` = MA VA, `ctx+0xE0` = E VA (written at window construction). MA: `PesMaRecord` per side, 0x24900 bytes: lane lists at `+0x229B4` / `+0x22B9C` (team stride 0x58, entries of (slot id u32, value f32)), ball copy `+0x2482C` (x, height, z), carrier id `+0x2483C`.
- Clear unit block per side: unit (`+0` carrier record VA, `+8` ctx VA, `+0x28` imageA VA), carrier record (u32 slot id), answer (u32 type at +0, f32 angle at +8), imageA (danger byte `+0x31C1`).
- GF writers: `TeamAIController::WriteMaRecord` (called from `TickScoreRows`) writes the lists from the score rows, the ball copy from the ball's record 0, and the holder id. **INF/stand-in:** MA+0x2482C's PES writer and MA+0x2483C's are not decoded (`ponytail:` in the code). The MA lists are a serialization of `ScoreRows::list` each tick: the rows (`SortedRow`) are still the store, so this is a copy, not a move (OPEN item 2).

## 3. Verification (run directly or in a slot; Main gates)

| Binary | Result |
|---|---|
| `pes_angle_oracle_test` (`GF_PES_ANGLE_ORACLE=seq021`) | 4/4. `TheCapturedSituationOnGfsWindowGivesTheCapturedAngle` passes with ctx, MA and E as window bytes (no `CopySeg`); `EachWindowRecordBindingChangesTheAngle` now flips 19 bindings (3 new: MA ball copy x, MA list A team 1 rank 0 id, E newest payload word 0). MA's carrier id (+0x2483C) is **not** on this call's path (flipping it left the path unchanged) and is not claimed. |
| `pes_tick_a1_test` | 25/25 (4 new: velocity has no store beside the window, event store at PES offsets, MA written each tick, Clear unit runs on the window and answers only on the danger byte) |
| `pes_events_test` | 39/39 (store now a view over an owned image) |
| `pes_carrier_test` | 189/189 (2 new: kind 1 carries the unit's answer and angle; rule 7's clear gives way to an answering pass unless forced) |
| `pes_movestep_wire_test` | 27 passed, 1 failed (`TheKickoffTakerClosesToTheBall`, §1) |
| Not run after the change | `pes_involvement`, `pes_possession`, `pes_event_wiring`, `pes_team_tick`, `pes_clock_match`, `pes_world`, `pes_match_info`, `pes_offball`, `pes_scorerows`, `pes_mark_table` etc. were **built** (build rc 0) but not run (budget). They link `Match`/`pesevents`; Main's gate covers them. |

Red first: the new `pes_tick_a1_test`/`pes_carrier_test` cases do not compile on `origin/main` (`BallVelocity`, `Ma`, `Events`, `RunClearUnit`, `FillUnitTable`'s 11th argument, `EventStoreImage` undeclared). The velocity test is also red by behaviour on main (a separate store). The step 2 red was not seen separately before the change (code was edited first).

Mutations (applied together in one build, each caught by its own test, all reverted with `git checkout`, `git status` clean):

| Mutation | Result |
|---|---|
| M1 `Ball::StoreMomentum` writes (x, y, z) instead of (x, z, y) | `TheBallsVelocityHasNoStoreBesideTheWindow` FAILED |
| M3 `FillUnitTable` kind 1 -> kind 12 | `KindOneCarriesTheClearUnitsAnswerAndItsAngle` and `ARulesSevenClear...` FAILED |
| M4 `SetClearDanger` inverted | `TheClearUnitRunsOnTheWindowAndAnswersOnlyOnTheDangerByte` FAILED |
| M5 newest copy memcpy shifted by 4 bytes | `TheEventStoreLivesInTheWindowAtPesOffsets` and five `pes_events_test` cases FAILED |
| M2 `WriteMaRecord` writes (x, z, height) | **SURVIVED**: the first MA test's ball had height and lateral both 0 after three ticks. The test was strengthened (rest ball at (3, 0.5, 0.11), asserts height != lateral) and passes on correct code; the strengthened test was **not re-mutated** (OPEN). |

## 4. Open items (numbered)

1. **Step 1**: re-stage the kickoff test by seed search, or find the PES timer that reads the frame in the approach (§1). Also settle whether MI+0x1198 counts every frame or only gated ones (`[r14+0x31A]`).
2. **MA lists are a copy**: move `SortedRow`/`ScoreRows` storage into the MA window region (PES block: 200 bytes; entries decoded, count and rank bytes' layout is not).
3. **Danger flag**: translate `0x140699960` (108-function closure) and `0x142119390` (193) through `closure.py` + the layer pipeline (Main regenerates the layer), prove on harness cases; needs the lane/team records they read. Then delete the one stand-in site in `elizacontroller.cpp` (`ponytail:` naming both VAs). Hooks wanted from Main: `0x140699960`, `0x142119390`, and a sampler that reaches Clear with danger = 1 (EndClear3 §6).
4. **`forcedClear`** (rule 7's opponent-count test `0x140A4C820`): undecoded; `walk.forcedClear` stays false, so a clear always gives way to an answering 3/4/9/6.
5. **Angle inputs without GF writers** (still zero in a match, so the Clear's angle is computed over partly empty records): ball +0x1C/+0x24/+0x80, man action +0x5D0, anime +0x11C, slot +0xA1/+0xA4, frame-random record, MA's two stand-in fields. The kind-1 clearance direction is therefore not yet PES-faithful in play; the angle is faithful only where the window is (the oracle case).
6. Not run: the other `pes_*` binaries listed in §3, the smoke (Main), the 5-mutation re-run of M2.
7. Cleanup: remove worktree `gf-EndClear-base` and scratch `gf-EndClear-scratch` once archived (`origin/main` base build, 4143 objects).
