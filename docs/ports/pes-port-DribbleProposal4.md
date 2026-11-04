# Dribble proposal 4: the heading red was the oracle's input, the chain equals PES on every captured state, it runs in production (BP+0x3380 stand-in deleted), walk 22 + stand-down equal PES, smokes finish and keep main's hashes (DribbleProposal4, 31-10-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`, scratch `gf-DribbleProposal-scratch`, PES-derived, never committed, now 169 MB; it held 36 GB at the start and Main asked for less than 10). Image `pes_full.bin`. CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-DribbleProposal3.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main` (AQ), regenerate, `--check`, push | **DONE.** One conflict (`synth_walk.py`: main's `--entry` from ForcedClearReal vs this branch's `--entry` + `--bp-ctor`): this branch's file kept (superset). `gen_translated.py --check`: 4456 functions byte-identical. Pushed `5b909ab8`. |
| 2 heading red: fix GF's input; `TheProposalChain...` green | **DONE, CONFIRMED** (section 1): the window held only the manifest of the builder call; the chain reads far more. `CopyRecords` overlays the capture's whole records: **green on all 7 comparable states**, both windows. Pushed `d671a325`. |
| 3 `trap_oracle.py which()` + `pes_trap_oracle_test` | **DONE** (section 4). |
| 4 production wiring, delete the stand-in, walk 22 + 6, red-first stand-down test | **DONE** (sections 2, 3). Pushed `e89ee121` (wiring) and the closure fix below. |
| 5 smoke | **DONE** (section 5): both 2-min smokes finish (90 s and 135 s at load 32), determinism lines equal main's baselines on both paths, twice. |

## 1. The heading BP+0x4210 (CONFIRMED)

- Cause (INF from the fix, confirmed by it): `TurnReferenceOracle.TheProposalChain...` built the window from the manifest of the 0x14211D710 capture (`danger_oracle.py` builder mode = the bytes THAT call read) plus `RunCarrierConstruct`. The chain then runs the step object `0x14211ED10`, the proposal writer `0x1421102D0` and, inside it, the kind step `0x14212FE70`, which read the kind objects' evolved state, the step output record `BP+0x4258` and the slot records: none of them in that manifest, so the window held freshly constructed zeros (PES's `BP+0x4258` is `1,0,0,1,7,0x162E6`, the window's was 0). DribbleProposal3's guess ("the kind step's object state or BP+0x4258") was right; the GF code was not at fault.
- Fix: `pes_oracle_world.hpp` `CopyRecords(o, b, walk)` (the old `BuildFull` body, now shared; `BuildFull` calls it) copies every record the window holds whole from the capture through its G (builder shape: `bp = r8 - 0x1C`, `ctx = rdx`). The chain test calls `Build(..., construct = true)` then `CopyRecords(o, b, false)`.
- Result (`GF_PES_DRIBBLE_ORACLE` = the 10 dirs): `EveryCapturedEntryLeavesPesManagerBytesAndTurnReference` OK, `TheProposalChainLeavesPesTurnReferenceAndProposalRecord` OK (capture-bytes window and production window; 7 states with a `walkref.tsv`), `CarrierConstruct` OK: 3/3.
- Mutations (each run, rebuilt, reverted by `git checkout`): skip the writer call `0x1421102D0` -> chain test RED and `WalkOracle.TheDribbleProposalChain...` RED; skip `RunTurnReference` -> chain test RED; skip the adopted copy `BP+0x41E8 <- BP+0x4204` -> chain test RED. (Not killed: the `kind 0x13` branch (`0x140A638C0` true) and the flag-class return: no captured state takes them: OPEN 2.)

## 2. Production wiring (CONFIRMED run)

- `PesWorld::RunCarrierUpdate`: the first call of a side builds the block with `RunCarrierConstruct` (`0x14210D320`, whose last call is the reset); a new carrier slot runs the driver's block reset `0x14210E5F0` (`0x142110C90`'s inactive branch: the state constructor `0x14063A3B0`, `BP+0x41E4 = 0x19`, the proposal records, the manager, every kind object) instead of the state constructor alone. The `ponytail:` text that waited for this report is gone.
- `ElizaController::GetOnTheBallCommands`: `pesWorld.RunDribbleProposal(sideIndex, slot)` replaces the `BP+0x3380` stand-in (the carrier's facing, `PesBearingDeg`), after the danger builder, the plan word `BP+0x32B8` and `SetCarrierProfile`, before the walk: PES's order of `0x142110D30` (`0x14211D710`, `0x14211ED10`, `0x1421102D0`, the copies). `BP+0x4204` now comes from the translated writer.
- A second defect surfaced only in the `--pes-loop` smoke (the switch-off path never reached it): **`PES_DISPATCH_UNTRANSLATED: 0x140a57450`** (abort, rc 134 after 66 s), from `0x142131810` (the kind step's helper) under `RunDribbleProposal`. `0x142131810` is a closure member whose calls to `0x140A57450` and `0x14063D2F0` (and `0x14211A030`'s to `0x140A57450`, `0x14211E650`'s to `0x14211CA50`, `0x14212FE70`'s to `0x14211CB00`) had been translated as `OUT-OF-SET CALL`: the targets are translated elsewhere but were not in this closure's spec set. Found with `gdb -batch -ex run -ex bt` (frame 4 `pes_fn_142131810`). Fix: those four as seeds of `closure-dribble.txt` (`closure.py --spec-set dribble --tag dribbleX`, 4 lines merged, the other 20 closure members were already in the set), `gen_translated.py` rewrote exactly four files (`14211A030`, `14211E650`, `14212FE70`, `142131810`), no header change. Not scanned further: the remaining out-of-set calls of the set are the heap models (`0x1403FC080` ... `0x1403FEC90`, dispatched to `pes_native_heap_*`).

## 3. Walk oracle 22 + 6 and the stand-down test (CONFIRMED)

- Regenerated: `walkdir.sh` (scratch) per state: `synth_walk.py` (default entry, no `--patch-clear`) from the 11 remaining full `pid483669` `carrier_clear_2109EB0` captures (seq11497 earlier as the trial: 12 in all), `frame_trace.py recorded`, `snapdiff.py --only 0x14210F110 --bounded --spec-dir specs/core`, `danger_oracle.py ... walk`; the 8 older real walk captures (`pid1965411` seq035, seq627; `pid1981120` seq3914, 4518, 5130, 5878, 6294, 6695) directly; `syn_seq4046`, `syn_seq4444` with `--patch-clear`. 22 dirs, 7 min.
- `WalkOracle.*` on the 22: 3/3 as before (kind equal on both windows) **and the new test** `TheDribbleProposalChainStandsTheClearDownWherePesDoes`: from each Clear-record state's whole records with `BP+0x3380` and the proposal record `BP+0x4204` (0x1C bytes) ZEROED (as GF's window held them), `RunDribbleProposal` then `RunWalk` returns PES's kind on all of them (the six stand-downs 6, 0x19 x4, 0xB; and the ones PES clears: 1), on the capture-bytes window and the production window. **Red-first:** the same states with the zeroed inputs and no chain: `EXPECT_GT(zeroedWrong, 0)` holds (the window answers 1 where PES answers 0x19 / 0xB: the five non-6 stand-downs; on seq11497 the Clear block hands to kind 6, which is also PES's answer, so that state is not a red). Mutations above kill it.
- The answer to DribbleProposal3's open measurement (writers re-run on a post-writer state): the chain's outputs on the walk-entry state equal PES's on all 12 (the kind objects tick a second time; heading, record type and copies unchanged).
- Disk: the synthesized states are slimmed to the pages the tests read (a sparse `mem.bin`, +-1 page, `slimlog.py` + a temporary read log in `Read`, removed before commit): 2 GB -> 1.8 MB each, tests re-run green on the slim dirs. Because they are slim, `GF_PES_UNIT_ORACLE` (needs whole states) must be pointed at freshly regenerated dirs (20 s each with `walkdir.sh`).

## 4. `trap_oracle.py which()` (CONFIRMED)

Same fix as `danger_oracle.py` (nearest base below among the first match's kind). `origin/StoppageFlow` already carries an independent, equivalent rewrite of the same function (`best` by largest base) inside a larger rewrite of the file (N-lines): expect a trivial conflict there, take StoppageFlow's. The 24 trap oracle dirs of `gf-TrapOpen-scratch` were made by StoppageFlow's script (N lines) and cannot feed this branch's test; I regenerated three (`pid2580963/seq069`, `pid2670420/seq1396`, `pid463316/seq4469`) with the fixed script: `pes_trap_oracle_test` 1/1 green on them. None of the 24 existing manifests carries the symptom (no `T slot` line with rel >= 0x2B90), so the defect did not change a trap manifest here.

## 5. Smokes (CONFIRMED)

2-min seed 7, `GF_SIM_SLOTS=4`, load ~32:

| path | main (`gf-MergeLane` binary, c834c1df) | this branch, wired | this branch, rerun |
|---|---|---|---|
| switch-off | `e663d1e917d6c4f4`, 33,628 ticks, rc 0, 1m32 | `e663d1e917d6c4f4`, 33,628 ticks, rc 0, 1m31 | `e663d1e917d6c4f4`, rc 0 |
| `--pes-loop` | `980c54e27f93ced2`, 25,942 ticks, rc 0, 2m42 | (first run: abort, section 2) after the closure fix `980c54e27f93ced2`, 25,942 ticks, rc 0, 2m15 | `980c54e27f93ced2`, rc 0 |

- The non-finish of DribbleProposal3 is NOT reproduced: main's smoke finishes in 92 s at load 32, so it was not machine load; DribbleProposal3's logs came from a build that could not be told apart (`/usr/bin/time` is absent on this machine: a first attempt of this session died instantly with "command not found" and left empty logs, which is what "log empty" looks like). INF only: I cannot prove that was the earlier cause.
- Hash equal to main on both paths with the chain wired (INF): in 2 minutes no state reaches the walk's Clear gate with a proposal that moves the answer, and `BP+0x3380` was only read there. The wiring is exercised (the pes-loop abort came from inside `RunDribbleProposal`). Not a quality claim.

## 6. Left, numbered

1. `GF_PES_UNIT_ORACLE`: the unit/danger-builder oracles of ForcedClearReal need whole (unslimmed) states; regenerate with `walkdir.sh` if they are to be re-run.
2. Coverage: the proposal kind `0x13` branch (`0x140A638C0` true) and the flag-class return (`0x14210DD60` untranslated, `ponytail:` named in `RunDribbleProposal`) are in no captured state; the 8 closure members of DribbleProposal3 and the 4 seeds of this report have no replay proof of their own.
3. Production slot stats (`+0xA1`, `+0x9C`, `+0x97`) for every slot record: only the carrier's `+0xA1` is written (`SetCarrierProfile`), so the manager's kind-6 candidate scores read zeros for the others (`ponytail:` at the call). Real matches therefore feed the writer less than PES does.
4. A smoke over 20 minutes was not run (Main's batch).
5. `GATE fast`: see the final message.

## 7. Checks run

`gen_translated.py --check` (4456 identical after the merge; regenerated once after the closure fix: 4 files); `pes_dribble_oracle_test` 3/3 (10 dirs, also on the slim dirs); `pes_walk_oracle_test` `WalkOracle.*` + `CarrierWalk` + `CarrierProfile` 7/7 (22 dirs, slim); `pes_danger_oracle_test` `DangerBuilder.*` 2/2 (the carrier-inputs test now binds the install tables: `RunCarrierUpdate` builds the block); `pes_trap_oracle_test` 1/1 (3 regenerated dirs); smokes of section 5; mutations of section 1 (run, rebuilt, reverted).
