# pes-port-StallLive2: the DRIBBLE handler joins the layer; the DRIBBLE carry and hasIt = PES holder measured, both worse, neither landed (StallLive, 19-11-26)

Status: NOT GREEN. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` is red at 606 live firings (unchanged from base f2458223). One layer change landed (inert in the census). Branch `StallLive`, head **81edc15d4502e35ece7269f465bd0c4fe7958968** (ls-remote confirmed; parent a2869d9f = the census dump commit).

Tags: CONFIRMED = measured in this tree or read in the image; INF = inference; OPEN = not settled (blocker named).

## 1. Step 1: the untranslated DRIBBLE target (CONFIRMED)

- Image qword search (`pes_full.bin`, flat, control 0x14256AE1C = 1.0f): 0x1407C4FA0 is stored at **0x1425E24D0**, which is slot **+0x90** of the DRIBBLE action vtable block (base 0x1425E2440; +0x20 = 0x1407B37D0, +0x58 = 0x1407A38E0, +0x70 = 0x1407B9280, matching the earlier decode).
- Survey (`GF_PES_SURVEY`, carry on, seed 7, 2 min): 0x1407C4FA0 hit **84,644** times, first on logic frame **224**, caller offset **0x5411FE** = `pes_fn_1406F6CE0` +0x581E (`nm -n` of the test binary). That function reads `[obj]` then `[vt+0x90]` and dispatches (`pes_fn_1406F6CE0.cpp:1077`).
- The other 257 survey hits are all frame 0 from `pes::PesBoot`'s `RunStaticInitializers` (`pesboot.cpp:77-87`). That loop looks each initializer up and silently skips a missing one in normal runs, but survey mode counts it. They are not reached by the match. So **the only untranslated target the match reaches is 0x1407C4FA0.**
- Brought into the layer: `tools/dev/translate/closure_trapdribble.json` member (sorted, after 0x1407C44A0); universe row already present (bridge row, subsystem AI, indirect `CALL [RAX+0x70]` and `CALL [RAX+0x88]`, no direct callees); `closure.py --tag dribble90 --no-specs` (1 member, meta closure written, set file `closure-dribble90.txt`); `gen_translated.py --image` regenerated the layer (10,150 functions: 3,824 recorded, 6,326 closure; new `closure/pes_fn_1407C4FA0.cpp`, `pes_dispatch_gen.cpp` +2 lines, `translated.hpp` +1). The three "held out" lines are pre-existing mistranslated-callee holds in other functions.
- Checks: `gen_translated.py --check` **CHECKS 0, 10,150 functions byte-identical** (rc 0). `ledger.py` rc 0, `ledger.py --check` rc 0 ("LEDGER.md current (47642 rows)").
- Commit **81edc15d**: the six layer/closure paths only.
- Census with this function and no carry: **606 live firings, unchanged** (rc 1, base red). The function is not reached on the base flow, because the base run never calls slot +0x90 of the DRIBBLE object.

## 2. Step 2: DRIBBLE-only carry (MEASURED, not landed)

Carry (test of the change, reverted after): in `WritePesTrapActors` (`teamAIcontroller.cpp`), when the stored P+0xAC8 is 4 (DRIBBLE), carry the stored pair into `PesActorFacts` instead of MOVE, so `WriteActorP` keeps it.

| Config | live firings | episodes (start s, firings) |
|---|---|---|
| base f2458223 (GF `hasIt` band, MOVE for all) | 606 | 160.3 (248), 269.3 (68), 280.5 (13), 314.9 (127), 429.6 (36), 591.9 (28), 639.9 (86) |
| function only (no carry, no hasIt) | 606 | same as base |
| **carry (DRIBBLE only) + function** | **920** | 160.3 (248), 269.3 (68), 280.5 (13), 314.9 (127), 643.2 (266), 697.5 (198) |

- The carry is not inert. Its effect is within a tick: the stored byte is 4 for part of a tick, then `WriteActorP` rewrites it. At dump time, ac8 = 4 never appears (0 of 606 firings).
- First episode is unchanged (248 firings, holder 123, forced man is a non-holder MOVE walker). The late episodes move: base 639.9-641.5 (86) becomes 643-648 (266) and 697-701 (198).
- Earlier attempts at this carry aborted with `PES_DISPATCH_UNTRANSLATED: 0x1407c4fa0` (2 runs, 53 s and 75 s). Step 1 resolves that abort.

**Verdict on step 2:** carrying the DRIBBLE byte does not reach a DRIBBLE tick on the holder. The trap-end DRIBBLE admission (`0x140796C1B`, `FUN_140796B30`, falls back to MOVE 2) is refused in GF: every trap clip ends at ac8 = 2 (MOVE) in the dumps (7 then 2). The admission family is `FUN_14079EE70` (translated, `src/pes/translated/trapaction/pes_fn_14079EE70.cpp`, admit at 0x14079F17D). Which gate refuses inside it is OPEN: reading ~580 lines of translated gates was not finished in budget. A hook or a trace of its return value is the next step.

## 3. Step 3: hasIt = PES holder (MEASURED, reverted, worse)

`elizacontroller.cpp` `hasIt = GetBallRetainer()==player || (PesLoop ? GetPesHolder()==me : HasPossession())`.
- With the carry: **6,698 live firings**. 2,107 firings have ac8 = 4 (DRIBBLE) on men who are **not** the PES holder (`holder=0`). The carry keeps DRIBBLE on a man after the holder moved on, and that man stands in a DRIBBLE clip with no touch. The holder (3 firings) is almost absent.
- Without the carry, hasIt = PES holder: **not measured** (budget). This is the open half of step 3.
- Reverted: `elizacontroller.cpp` and `teamAIcontroller.cpp` restored to f2458223 (the `git checkout` on both paths, after the symlink swap).

## 4. Tests (run on branch 81edc15d: layer change only)

- `pes_restart_delay_test --gtest_filter=PesStall.*` (ran in one sim slot, 2,379 s): **5 PASSED**; **FAILED** `ALiveBallNeverLiesStillLongEnoughForTheBreaker` (606, base-red, the open item) and `ATrapTouchIsTheBodyScansGrantAndNothingElse` (StallContact's, exempt). `ATrapClipIsNeverRestartedBeforeItsContactFrame` 578.8 s OK. The full-time assertion passes in every test (the seed-7 2-min pes-loop match reaches full time, which is the smoke).
- `pes_pass_target_test`: **1 PASSED** (`PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver`); the stale test stays a named gap, not unskipped.
- `pes_throwin_release_test` (45-minute sim timeout, 6 of 9 tests ran): OK `ThePlayStateIsSetPlayReadyUntilTheReleaseTouch`, `TheMatchIsBuiltWithTheInstallsPlayerDatabase`, `ACarriedThrowUsesTheShortRowUpToThePesDistanceAndTheMidRowBeyond`, `ACarriedThrowInIsReleased`, `EveryManHasHisDemoInfoRef`; **FAILED** `TheThrowInTakersAnklesStayBehindTheLine` (11 of 233 taking ticks kept the nearer ankle short of F[1] + 0.1 m, worst 0.0269 m, `pes_throwin_release_test.cpp:374`). The brief says 9/9 on the base. **OPEN:** whether this failure is pre-existing on f2458223 (not re-run on base: budget). Three tests did not run.
- Movestep unit tests (built and run): `pes_movestep_test` 121 PASSED; `pes_movestep_run_test` 110 PASSED; `pes_movestep_step_test` 75 PASSED; `pes_movestep_turn_test` 82 PASSED; `pes_movestep_turn_entry_test` PASSED (custom runner, no gtest count); `pes_movestep_wire_test`: not built in this configuration (OPEN).
- Fast gate (`GATE_TIER=fast tools/dev/gate.sh`): **not run** (budget; the brief's fallback line: every `pes_*` binary linking changed files was run above, plus `gen_translated.py --check`: CHECKS clean).
- Mutation: none. No fix landed, so no red-to-green mutation was made.

## 5. Open items (numbered)

1. Which gate in `FUN_14079EE70` (translated) refuses the DRIBBLE admission at the trap end (`0x14079F17D`, EDX=4). Settles why DRIBBLE never follows a trap. Needs a return-value trace (a test-only probe is not possible without hand-editing the generated layer; a live hook or a survey-style recovery is the route).
2. The DRIBBLE carry: once item 1 gives a DRIBBLE holder, carry only for the PES holder (`GetPesHolder()==man`), so a man who lost the ball returns to MOVE; the census measures that.
3. hasIt = PES holder without the carry: measure (the 6,698 run mixes in the carry).
4. Throw-in ankle failure (11/233 ticks): base status unknown on this run; check f2458223.
5. `pes_movestep_wire_test` not built in this config; the fast gate not run.

## 6. Branch

`StallLive` at **81edc15d** (this report's code change: layer only). Previous: a2869d9f (census dump, test-only). Worktree `<workdrive>/gf-StallLive`; scratch `<workdrive>/gf-StallLive-scratch/` (census0..6.log, survey1.txt, check1.log, ledger logs).
