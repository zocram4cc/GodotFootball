# PES port PressAI: press writer 0x140536FC0, target eval 0x140633940 and coordinator 0x140634470 translated and proven; GF cutover written but NOT closed (pes-loop smoke aborts, enrolment test disabled) (03-11-26)

Branch `PressAI` (worktree `gf-PressAI`, Release dir `build-rel`), head confirmed by `git ls-remote origin PressAI` (see final message). CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. Proofs (CONFIRMED, `snapdiff.py --bounded`, `frame_trace.py recorded`, live2/pid296049)
| root | calls | replayed | mismatches | own-block | closure |
|---|---|---|---|---|---|
| 0x140536FC0 writer | 23 (3 kick-off captures seq009/4264/4754, 4 in-play captures seq94346/95361/95772/96616 that leave at the body filter, **16 SYNTHETIC** states over seq95772) | 23 | 0 | 0.4412 | replays 34, plus static closure = 117 |
| 0x140633940 eval | 3 (seq028, seq4684, seq11437) | 3 | 0 | 0.5699 | replays 29, static 56 |
| 0x140634470 coordinator | 3 (seq008, seq4263, seq4753) | 3 | 0 | 0.4464 | replays 43, static 84 |
- Records `results_press.json` (+ `closure_press_{writer,eval,coord}.json`, `specs/press`, `closure-press.txt`, `SETS` entry). Members beyond the replayed set are the roots' static direct-call closure: translated, **not replay-proven** (so GF cannot hit a loud abort inside the three roots; the paths they have are covered by the translation only). Layer 4,855 functions; `gen_translated.py --check` byte-identical (rc 0).
- Synthetic writer states (`synth_call.py`, scratch scripts not committed): own men's MI+0x1748 flags cleared so the 11-slot body filter passes, T+0xF1 armed, random men positions within 6/12/22 m of the ball, T+0xB709, slot words 0x3A/0x34. PES's bytes are the reference on the same poked memory.
- **Why captures are shallow (CONFIRMED by block traces):** the old 9 captures were at kick-off (MI+0x116C = 2). Main re-armed predicated hooks (in play); the 11 new captures are in play but the writer still leaves at the body filter (an on-pitch man in an eligible action class: classes 4-8 of the table at 0x1426484F0), the coordinator at 0x1406345E9 or in its first half (no holder / no T+0x7C). **No real capture reaches the writer's enrolment, the coordinator's class-5/maintain/eval dispatch, or the eval's accept paths with a non-trivial mode.** Needed, exact: writer `--when` that implies the filter passes cannot be written as a register predicate; use `--when` on the hook's own exit instead is impossible, so ask for more in-play writer captures at a higher cap (cap 60) with the predicate already armed, and coordinator captures with `d(q(q(0x1436F3DC8)+0x270)+0x116c)==1 and d(rcx+4)<0x16` (PP[1] valid).

## 2. Cutover (written; built; partly verified)
- `PesWorld` (pesworldrecord.{hpp,cpp}): side record = T (`kSideRecordBytes` 0xC138; in the captures table A's entry and r8 hold byte-identical copies, +0x78/+0x7C/+0xF1/+0x250/+0x254/+0xA294/+0xA53C/+0xB709/+0xB8A4 equal, r8 = table A entry + 0xC144, CONFIRMED seq4753/4754; the window keeps one record per side and passes it as both); `PesPressImage` (PP and the {ctx,0,side} frame per side); `StagePress`, `RunPressCoordinator`, `RunPressWriter`, `ResetPress` (FUN_140536690's first loop, ponytail: tail not ported), `PressAction`, `PressSecond`.
- `TeamAIController::PesPressTick` stages GF facts and runs coordinator -> reset -> writer (FUN_140535C60's order). **Deleted**: `PesLooseWriterTick`, `PesSecondPresserTick`, `pressState`, `coordState`, `TeamPressState`, and from `pespress.*` the picker, eval, gate, cover, class-five, maintain, candidate-fill, stamp-ring and role-table code (now ActionPress's command side only). `pes_press_test` 95 tests -> 6 (the deleted subjects' tests went with them). Consumers (`GetLoosePressMan`, `IsLoosePresser`, `PressOrDelay`) read T+0xA2A0; `PressOrDelay` now counts DELAY (0x33) as PES's flag does (pesreceive.hpp).
- Stand-ins, each `ponytail:` at its PES site: T+0x7C none (writer open), T+0xF1 armed always (event-walk writers 0x140501283/0x140501468), T+0x558/0x620 "no partner", T+0x4B4C = man x, G+0x180 list "no man", ResetPress tail, attack angle 90/270.
- Man record +0x5D0 mirrors T+0xA2A0 at staging (the candidate table 0x140639830 enrols only men whose own word is PRESS: **a fresh press is not started by the writer**; PES starts it in the dispatcher FUN_140535C60's order-word loop (T+0xB748 == 0x26) or the coordinator; the loop is NOT ported, so GF has no fresh loose-ball presser except through the coordinator: INF, behaviour change vs the retired hand port).

## 3. Tests
- `pes_press_world_test`: **4 pass, 1 DISABLED** (the enrolment case: the staged scene never reaches T+0xA548[0]; a gate before 0x140537380 refuses it; gate not identified: candidates the keeper hold FUN_140A53F90 and FUN_140639600's score code). The 4 passing cases are negative (designated man, not in play, off pitch, outside radius -> nobody enrolled): they check the head gates and staging only. **No positive red-first test, and no mutation was run** (not done: budget).
- `pes_press_test` 6 tests (command side) built; NOT run (simslot wait). `ctest` not run.

## 4. Smokes (seed 7, 2 min)
- switch-off: `3b73c968b4a377e0` = main baseline (unchanged, CONFIRMED).
- **pes-loop: ABORTS `PES_DISPATCH_UNTRANSLATED: 0x14211bf40`** (RED ALERT in Humanoid::Process), before and after two mitigations (MA+0x2601C and T+0x4C00 borrowed and restored around the press run). 0x14211BF40 is not in the press closures; it is a carrier-chain function. Cause NOT found: no ablation done (disabling `PesPressTick` would tell whether the press or the larger side record triggers it). Main's baseline `0e480b158cf80bd4` could not be compared.

## 5. Open items (numbered)
1. Ablate the pes-loop abort: comment the `PesPressTick` body; if the abort stays, it is the record growth (check `ninja -t deps` for `#deps 0` objects after the layout change, brief rule) or a pre-existing state; if it goes, bisect staged fields (T+0x78/0x7C, man +0x5D0 mirror, MI+0x1748, MA+0x2601C).
2. Find the writer's gate for the enrolment test (scratch: decompile in `dec_writer.c` lines 70-140; use `readset.py`-style trace on a harness state) and enable the test; add mutations (designated, playState, onPitch, radius, sign, role class).
3. More in-play captures (see section 1) and widen the proofs; coordinator and eval synthetic states (`sync.py` was written, not run).
4. Port the dispatcher's order-word loop (FUN_140535C60 +0x26 branch, FUN_14062AF10) so fresh pressers start as in PES.
5. `GATE fast`: NOT RUN. Interim: `gen_translated.py --check` GREEN (4,855 functions), `pes_press_world_test` 4 pass / 1 disabled, switch-off smoke = baseline.
6. Delete worktree/scratch `gf-PressAI-scratch` (small now) and `build-rel` when landed.
