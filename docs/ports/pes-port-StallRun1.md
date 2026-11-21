# pes-port-StallRun1: the RUN entry is still not fixed; the GOTO_RUN trap is measured, its root cause open; no commit (StallRun, 21-11-26)

Status: NOT MET. No sim change landed. Branch `StallRun`, worktree equals HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (`git diff HEAD -- src tests` empty; diagnostics reverted). Nothing pushed: no change was green, and a red test alone was not committed (StallLive16 rule). `build/` and the scratch binary are deleted.

## 1. Measurements (seed 7, 2-minute PES-loop match, temporary `PesDiag` test)

Population: every tick a loose presser (not PASS_GET receiver, not in possession) has a targeted movement command at >= 3 m/s. Bins: MOVESTEP state (`MoveState`: 1 IDLE, 3 RUN_LOOP, 4 STEP_LOOP, 7 GOTO_RUN, 9 ACCELERATE), body-to-target distance, body/command speed ratio. `preciseArrival` is 0 on every command, so the precise word seen by MOVESTEP is derived, not commanded.

Three runs, one build family, only line 241 (`in.commandSpeedClass`) differs:

| run | line 241 | GOTO_RUN > 3 m: ratio (ticks) | RUN_LOOP > 3 m: ratio (ticks) | source log |
|---|---|---|---|---|
| A. HEAD | `CommandSpeedClassOf(speed)` (raw cmd[0x17], 0..4) | 0.267 (3,379) | 0.640 (1,904) | first `PesDiag` run, 608 s |
| B. class zeroed | `0` (derived precise off) | 0.271 (1,353) | 0.681 (3,103) | `diag_0.log`, GF_DIAG_CLS=0 |
| C. mapped band | `speedBand` (0..3) | 0.279 (1,292) | 0.501 (1,398) | `diag_m.log`, GF_DIAG_CLS=m |

Other bins in run A: STEP_LOOP > 3 m 0.51 (135 ticks); ACCELERATE > 3 m 0.40 (775); IDLE > 3 m 0.33 (134); GOTO_RUN 1-3 m 0.32 (590).

B and C were run in one build, started at 08:28 after two earlier attempts were killed by simslot's 1500 s timeout under machine load (~22); they finished in 765 s.

## 2. Killed theories

- K1. The derived precise flag (from the raw class) keeps the presser in GOTO_RUN: killed. Zeroing the class (B) leaves the GOTO_RUN ratio at 0.27.
- K2. The mapped band fixes the GOTO row or RUN entry: killed. C leaves GOTO_RUN at 0.28 and makes RUN_LOOP worse (0.50 vs 0.64).
- K3. `PositionAt` index 0..1 returns a position instead of a per-tick displacement: not current. `PositionAt` returns `GetMovement() / TicksPerSecond()` for index < 2 (pesmovestep.cpp:347-350).
- K4 (StallLive15/16): the blend stand-in's playback explains the slow body: still killed (playback near 1.0 at matched rates).

## 3. CONFIRMED structure

- `FUN_1406b7560` derived precise: `fVar16 = param_3[0x17]` (raw cmd[0x17], line 55 of the decompile). Lines 282-296 set precise from the raw class, kind, precise word and `cmd[0]` in {0x33, 0x34, 2}. The port matches. `CommandIdFor` returns 2 for every GF movement (`kPesCommandMove`), so every outfield move at raw class 1..3 becomes derived-precise when the anime state is nonzero. PES-faithful given its inputs; the input is a stand-in.
- `req[9]` = `CommandClassFor(cmd[0x17])` = install table `UNK_1425DDCD8`. `EnterGotoRun` picks the goto row by `GotoRowFor(kind, req[9])` = `gotoRow[k + c*8]`. Consistent with PES's `goto_run_0_{1..4}` naming.
- GOTO_RUN exit (`movestep_run.cpp` GotoRun, around lines 880-905): at clip end, if the displacement's forward speed is under 8 km/h (`k.kmh8 > forward`), `EnterGotoRun` runs again. With a slow body the state re-enters GOTO_RUN every clip. This is the self-sustaining loop behind the 3,379 ticks in run A.
- `Issue` (pesmovestep.cpp ~376-395): SetMotion goes to `PlayIssuedClip(request)`; SetParametric is the nearest blend member with playback = rate / member param (documented ponytails).
- `HumanoidBase::PlayIssuedClip` (humanoidbase.cpp:514-544) looks the motion up by family and `specialvar2 == motion`. An empty set returns false and the body keeps its own clip. The locomotion family is loaded (pesclips.cpp:47-60), so an empty lookup means a motion number with no clip. NOT MEASURED: how often GOTO/STEP issues return false.

## 4. Open items (numbered, each with the step that settles it)

1. Count `PlayIssuedClip` returns of false for GOTO_RUN and STEP issues (motion number, family, present or absent). If absent, the fix is the missing PES locomotion clips, not the class. Settle: a temporary counter in `Issue` and one diagnostic run.
2. The GOTO_RUN clip-end re-plan under 8 km/h: compare the forward speed at clip end with PES's goto clip root speed for the same motion (pes-re-anim-select.md 3.4). Needs item 1's motion numbers.
3. PES's real `cmd[0]` for an AI press or move command. The GF stand-in (2 for every move) drives derived precise. Settle: a live capture of a presser's command words (pes-re-press-cmd.md section 8.1).
4. Raw `cmd[0x17]` for a presser: writer unknown (A+0x874 via FUN_1409257B0). GF's RangeVelocity bands of continuous press speeds give class 0 below 1.8 m/s. Settle with item 3.
5. The speed test (ACommandedPresserMovesAtTheSpeedTheCommandAsks, 3 m/s, red 0.387 on StallLive15) and a RUN_LOOP entry test: write them with the fix, not before.

## 5. Next highest-value step

Item 1: one temporary counter in `Issue`/`PlayIssuedClip` to see whether the GOTO_RUN and STEP motion numbers exist in the GF clip set. It decides whether the RUN entry is a clip-library gap (fix in the clip loader) or a logic gap (the re-plan). Cost: one build (~2 min) and one diagnostic run (~13 min under load).

## 6. Blockers

None for tooling. Under load (~22) each 2-minute match takes 10-13 min; diagnostic runs need simslot timeouts of at least 3600 s.

## 7. Verification run

- `pes_press_approach_test` builds (81 s, flock). Diagnostic runs: `PesDiag.FastPresserStates` PASSED (3 runs above; the tests were temporary and are reverted).
- Not run this round: `PesStall.*`, PesHang, throw-in, pass-target, `gen_translated --check`, `ledger --check`, seed-7 smoke (nothing sim-side changed, so these are unchanged from StallLive16).
