# PES loose-ball approach: decode and the press-law experiment (StallRegress2)

Status: NOT landed. Branch `StallLiveRun` stays at `68872384` (the 151 fix, already pushed; `git ls-remote` confirms). The PES-approach replacement was measured worse (782) and reverted; the translation it needs is not done. The <=118 target is still not met.

## 1. What PES does with a loose ball (decoded, with status)

Source docs: `docs/re/pes-re-approach.md` §1 (its operands verified by a script), `docs/re/pes-re-press-life.md` §8-§9.

- The approacher is a PRESS (0x34) man chosen by the press writer `FUN_140536FC0` (translated: `src/pes/translated/closure/pes_fn_140536FC0.cpp`). CONFIRMED structure (press-life §3).
- His command is ActionPress `FUN_140962440`. On a resting ball, its target is the ball: `FUN_140639100`, no-carrier arm, k = int(0.1·54 + 0.5) = 5 ticks; S_k = B so target = ball-now. CONFIRMED structure; the chunk of `FUN_140639100` is not pdata-verified (approach §6 item 6, OPEN).
- The speed: `FUN_140962440` calls the planner `FUN_140678950`, a passthrough to `FUN_1406788B0`, which returns `max(1, int(fps·dist))` ticks, fps = 54 (press-life §9, CONFIRMED decompile + asm). `dist` comes from `FUN_1406B9270`, the movement solver that fills it through `FUN_140707EE0`. Its body is not reduced. So PES's approach speed is the solver's, not a stated law. OPEN.
- ActionPress has no distance arrival test. It ends only when the state leaves 0x34 (press-life §8, §10). Contact is the TRAP admission at reach ≤ 10 m (approach §1.5, CONFIRMED). The stall's men had no reach test to pass, so GF's trap path was never reached.
- The speed class `FUN_140A75BE0` is translated (`src/pes/translated/gameplay/pes_fn_140A75BE0.cpp`). The planner's ETA cut is not.

## 2. What is in the translated layer, and what is missing

| function | role | in layer |
|---|---|---|
| `FUN_140536FC0` | press writer | yes (`closure/`) |
| `FUN_140A75BE0`, `FUN_140A75B20` | speed class, stamina word | yes (`gameplay/`) |
| `FUN_140962440` | ActionPress (the command writer) | NO |
| `FUN_140639100` | press target (no-carrier arm) | NO |
| `FUN_140678950` / `FUN_1406788B0` | planner passthrough / ETA | NO |
| `FUN_1406B9270` | movement solver: sets dist, flag | NO |
| `FUN_140707EE0` | movement solver core | NO |
| `FUN_140A4D350`, `FUN_1406378F0` | ball ETA, arrival writer | NO |

To run PES's command instead of GF's stand-in, the chain above must be translated through `tools/dev/translate/gen_translated.py`, with records and specs. Not started this round (see §5).

## 3. Experiment: PES press law for the designated non-holder (MEASURED, reverted)

Change (saved: `scratch/press-law-782.patch`): in `_BallControlCommand`, the designated man without possession took target = ball-now and speed = `pes::PressCommandSpeed_ms` (the loose presser's law, `pespress.cpp:603`), instead of `AI_GetBallControlMovement`.

Result (`fix2_full.log`, full ALiveBall census, same seed): **782 live firings** (vs 151 committed).
Episodes after the change: 52.6 s (58, unchanged), 297.8-303.9 s (329, a dribbling man), 339.3 s (54), 472.4-475.1 s (144), 578.2-580.6 s (128), 655.6-656.8 s (69).

Reading: `PressCommandSpeed_ms` is min(2.6·dist, next speed band) with the band cut at the man's current class. A standing man is capped near 2.2 m/s and his speed falls with distance, so the approach slows. The stalls moved to other men. This is not a PES approach: the 2.6·dist slope is a GF constant, not a decoded PES value (approach §1.4). Reverted per the brief's land-or-revert rule. The worktree is clean against `68872384` (`git diff` empty).

## 4. Numbers for the landed state (`68872384`, unchanged this round)

- ALiveBall census: 151 (not <=118). The 58 at 52.6 s (also PesStallQuick's window) plus three approaches of 25, 29 and 39 ticks. The approaches did not shrink in any run this round.
- `pes_throwin_release_test` (ankle): 9/9 (`ankle_fix1.log`, round 1, same tree).
- `pes_hang_test`: 1/1 (`hang_fix1.log`, round 1).
- `gen_translated.py --image pes_full.bin --check`: exit 0, 10,150 functions byte-identical (round 1).
- No new code landed this round, so those results still describe `68872384`. I did not re-run them.

## 5. Next step (named, not done)

1. Translate the approach chain with the pipeline: `FUN_140962440`, `FUN_140639100`, `FUN_140678950`, `FUN_1406788B0`, then `FUN_1406B9270` and `FUN_140707EE0`. Each needs a spec, a PES harness record and a drift check. This is the work that would make the approach speed PES's.
2. Before translating, measure the planner's ETA on PES, not in GF. Captures exist: `live2/pid3218130/seq009..` (hook `planner_1406B9270`, args only, from a 1-in-N rig sample). They record the call arguments, not the return value. A return-value hook on `FUN_1406788B0` at its `ret` (VA to be read from the image) would give the ETA in ticks per call. Main owns the rig, so it needs a request.
3. Do not put the 2.6·dist law back: it measured 782.

## 6. Hygiene

- Worktree `<workdrive>/gf-StallLive` clean against `68872384`. The godot-cpp symlink is restored.
- Scratch: `<workdrive>/gf-StallRegress-scratch/` (census and build logs, `press-law-782.patch`).
- Build directory removed at the end of the round (per the 20-11-26 rule).
- Red test: the existing ALiveBall census. No new test landed, since no code landed.
