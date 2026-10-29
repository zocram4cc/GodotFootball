# PES port CardLevel2: the judge closure is translated and proven bit-exact on all 18 real judge calls (levels 0 and 1 only: no card, no scoring path); the GF cutover is written but parked on `CardLevel2Cutover` because the thresholds leaf has a layer gap (29-10-26)

Branches: `FoulSanction` `22ea7cb0` (translation, proofs, layer; GF source reverted to the hand port so the branch is safe to merge) and `CardLevel2Cutover` `5eefd011` (the GF cutover on top; both confirmed by `git ls-remote`). Worktree `<workdrive>/gf-FoulSanction`, scratch `<workdrive>/gf-FoulSanction-scratch`. Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. worktree, merge `origin/main`, push | DONE (one conflict in `hook_plan.py`: main dropped the gate/pending/SetPending hooks, took main's side) |
| 2. translate the 84-member judge closure, prove on the captures | translated and proven (below). **No capture covers a card or the scoring path: say so to Main** (section 2). Closure has 3 gaps that make the scoring path unusable at run time (section 3) |
| 3. GF: judge inputs on the window, level from the translated judge, delete `JudgePoints`/`JudgeLevelFromPoints`/`JudgeLevel` | written and parked on `CardLevel2Cutover` (section 4); NOT on `FoulSanction`: it aborts at the first scored foul (`PES_DISPATCH_UNTRANSLATED: 0x1405b82e0`) |
| 4. fast-tier line, 2-minute smoke | fast-tier interim line in section 6; **smoke not run**: the cutover cannot run a foul to completion, and `FoulSanction` changes no behaviour |

## 1. Proof (CONFIRMED: `snapdiff.py`, harness runs PES's bytes, the translated closure runs on the same snapshot; rax, xmm0 and every written byte compared)

| function | hook | calls replayed | mismatches | own-block coverage |
|---|---|---|---|---|
| `0x1405B7470` decide | judge_decide_5B7470 | 2 of 2 (pid2670420 seq7473, pid91171 seq316) | 0 | 0.195 (39/200) |
| `0x143F546F0` commit | judge_commit_43F546F0 | 1 of 1 (pid2670420 seq7443) | 0 | 0.7037 (19/27) |
| `0x14050E770` whistle | judge_whistle_50E770 | 15 of 15 (pid2670420 seq028, seq7444, seq7535, seq7699; pid91171 seq028, seq308; pid132854 seq028, seq804, seq866, seq1161, seq1328, seq1493, seq2126, seq2673, seq2939) | 0 | 0.5072 (35/69) |

Records: `results_core.json` (0x14050E770 replaced its "harness cannot run" record; 0x1405B7470 and 0x143F546F0 appended), `results_judge.json` (the snapdiff output). Layer regenerated: 3,989 functions (3,685 recorded, 304 closure), 24 new files (decide, commit, builder 0x1405B8000, SetPending 0x14050EE40, scorers 0x143F738A0, 0x143FA8420, 0x143F71690, ...). Pitfall found: snapdiff keys a snapshot by its directory basename, so two captures named `seq028` from different launches collide and report a false mismatch (rax differs by stack base); symlink them under unique names (`links/a_seq028`, `b_seq028`).

## 2. What the captures contain (answer for Main)

- **seq7473 / pid91171 seq316 (decide):** `A=0x12,B=0xa` and `A=9,B=0x12`. Both leave 0x1405B7470 at the early exit with no foul (rax 0, only `JR+0x30 = 1` written). Calls reach `0x1405B6870`, `0x140A639A0`, `0x140A63DA0`, never the scorers `0x143F738A0`, `0x143FA8420`, the thresholds, nor the builder `0x1405B8000`.
- **seq7443 (commit):** a direct foul, kind 2, **level 1** (pending record at judge+0x14: restart 5, foul kind 2, level 1, points 0x29 = 41), fouler slot 0x12 / victim 0x18 in the record's id words. This is the only foul in the set. `JR+0x38` is written with the level.
- **whistles:** kick-off (1), throw-in (2), goal kick (3), corner (4), free kick (5). The free-kick whistle (seq7444) is the commit's own call.
- **So the captures cover level 0 and level 1 only.** No yellow, no red, no warning, no penalty, and no capture of the points, the thresholds, the repeat-offender memory, the professional-foul minimum or the keeper exception. Main installed `--when` predicates (commit: level `[rdx+0x18] >= 2`; decide: B's action in 0xB..0xD) on pid132854; at the time of writing none had fired (pid132854 holds only whistles). When they fire, replay with `tr.sh` + `sd.sh` in the scratch dir and append to `results_judge.json`.
- **Synthetic cases do not help yet (INF, tried):** memory-edited decide cases on seq7473 (B tackling/sliding, A fallen, 30 combinations) reach the scorers, but the slimmed snapshot dropped the protected region's pages (97,273 of 97,342 pages of the mapping at 0x143D14000) and the scorers read heap words of the protection's own state (0x7ff5e00106b4, 0x7ff5e0020058, 0x33f8b08) that the capture never held. Filling image pages did not close it. Real calls that reach the scorers carry those pages in their read set, so they are the only way to prove the scoring path.

## 3. Closure gaps (CONFIRMED: scan of the generated files from the three roots, 90 functions reached; PES_DISPATCH abort seen at run time)

1. **`0x1405B82E0`** (5-byte `jmp 0x143FA8760`, no `.pdata` entry, not a Ghidra function) is called by the thresholds function `0x143F71690`; `0x143FA8760` (the thresholds proper, `.pdata` 0x143FA8760..0x143FA8834) is not in the universe. The translation holds `/*OUT-OF-SET CALL 0x1405B82E0*/ pes_dispatch_missing(...)`, which aborts: **every contact that reaches the level step aborts.** Cause: the universe rows of the protected leaves (`0x143F71690`, `0x143F738A0`) are bridge rows with `callees: {}` and `unresolved: true`, so `closure.py` never walks their calls, and `decode_row` accepts only `.pdata` starts (a non-`.pdata` jmp thunk island is rejected). Fix: in `closure.py` `crawl`, re-decode an `unresolved` row that has a `.pdata` entry, and let `decode_row` take a lone `jmp rel32` thunk to a `.pdata` start; rerun `closure.py --tag cardlevel`, regenerate.
2. **`0x1415A01DC`** (the CFG icall check thunk, called by `__ehvec_dtor` `0x14159FA0C` and `0x14159EFF8`) is an OUT-OF-SET call in both; the builder `0x1405B8000` calls `0x14159FA0C` for a 22-element array with the destructor `0x141794410` (a bare `ret`, in the dispatch table). Fix: model `0x1415A01DC` as `noop` in `stubs.json` (it is a check, not an effect), regenerate (touches every caller).
3. **`0x143E1C3A1`** (OUT-OF-RANGE JMP in the commit `0x143F546F0`, protected code): not reached by the one commit capture (proof 1/1 stands), reachable on other commit paths. OPEN: needs the commit's other branches (level 2+ commits) to show what it is.

Checked first, per the ticket: indirect calls in the 90 reached functions are the two `__ehvec_dtor` comparator-style calls only (target `0x141794410`, present); global stores: the judge writes only judge/JR/MI-adjacent window bytes (JR+0x30, JR+0x5C, judge+0x08.. record, repeat-offender table judge+0xE0/0x130/0x270); no image-global store was found in the reached set.

## 4. GF cutover (branch `CardLevel2Cutover`; CONFIRMED the code builds and links, one test run)

- `pesworldrecord.{hpp,cpp}`: window gains the judge (0x3C8 bytes), JR (0x80), the ctx cell, A1's block grown to +0x26020 (team lines +0x40/+0x54, mode bytes, last-toucher word); anime and slot field offsets the judge reads (action id +0x1, flags +0x8 bits 15/16/29/30, +0x3C, +0x44; slot +0x134, +0x12A0, +0x1474, +0x1478, +0x1480.., +0x148C, +0x14A0), `MI+0x1198` frame. API: `DecideJudge(victim, fouler)` (runs translated 0x1405B7470, reads the pending foul: level judge+0x2C, points +0x30, restart +0x24, type +0x55), `ContactFacts` + `DecideContact` (stages the facts in PES's records and restores the anime action bytes, which also hold GF's keeper-class stand-in), `SetTeamLine`.
- `referee.{hpp,cpp}`: `JudgeContact` now writes the two men's movement (`WritePesManMovement`, exposed from teamAIcontroller), the match frame (`GetPesFrame`), the DOGSO team lines (GF offside line stands in) and the facts, then returns the pending level. `StagePesField` (field info, attack signs) shared with the sanction path. **Deleted:** `pes::JudgePoints`, `JudgeLevelFromPoints`, `JudgeLevel`, `JudgeContact` (struct), `JudgeAction`, 23 `JudgeConstants` fields and their image loaders (`LoadJudgeConstants` keeps the advantage, offside and referee-spot constants), `FoulSeverity::DeniesObviousChance` (the file), and the 12 hand-port unit tests of `pes_judge_test`.
- ponytails, each naming the PES writer that removes it (in the code): A's flags 29/30 and B's flag word (PES's animation writer), "played the ball" (GF: last toucher), tackle sub-kind +0x44, intent +0x134 (PES writes it only from the pad-table scan, so a CPU player never has it: GF stays 0), fair flag +0x148C, predicted point +0x1480.. (the victim's own), the DOGSO team lines (writer of the match-analysis floats), `MI+0x1198` (the frame counter's writer), event ring 0 empty (no ball events on the window).
- Not changed: GF's gate in `ContactEvent` (PES's gate `0x14050D6A0` is already bit-exact in `results_core.json` but is not wired), the penalty decision (`CheckFoul` geometry; the judge's `restart` field 5/6 is available as `JudgeVerdict::restart`), commit `0x143F546F0` (GF takes the pending level, which equals JR+0x38 for kind 2), the deferred-card step.
- Red-first test `tests/sim/pes_judge_decide_test.cpp` (5 cases): no foul when the tackler never played the ball and is not aimed at the man (PES step 2; the hand port scored it by points); face flag outside the box is a yellow (step 1); a deliberate foul on a fallen man is at least a yellow, and the same tackle without intent is below a yellow (step 5); the staged action bytes are restored. Red on `origin/main` by construction (`DecideContact` does not exist). **Run once at -O0: case 1 passed, case 2 aborted with `PES_DISPATCH_UNTRANSLATED: 0x1405b82e0`** (gap 1). Cases 3 to 5 not run; **no mutations run**: nothing is green yet. Mutations planned: victim fall not staged, intent not staged, played-ball flag not staged, ctx cell unbound, fouler register unset.

## 5. Open items (numbered)

1. Fix gaps 1 and 2 (tool changes in `closure.py` and `stubs.json`), rerun `closure.py --tag cardlevel`, regenerate the layer (15 min), rebuild; run `pes_judge_decide_test` on `CardLevel2Cutover`; resolve gap 3 or show it unreachable.
2. Run the red-first test red/green and each mutation (section 4), then the 2-minute smoke on both paths (`--pes-loop` and default; two runs each, same hash).
3. Prove the scoring path: replay pid132854 decide captures (B in 0xB..0xD) and commit captures (level >= 2) when the predicates deliver; a capture with a card (level 3/4) is needed for the "proven on a card" claim. Append to `results_judge.json`.
4. The gate `0x14050D6A0` into `ContactEvent`; the penalty restart from `JudgeVerdict::restart`; run Commit and read JR+0x38; the repeat-offender memory needs the judge to persist across calls (it does: judge+0xE0 is window state).
5. Event ring 0 (ball events) on the window so "A made the newest ball event" and the keeper exception can fire; `G+0x40`'s +0x2920 (past the ball record) for the keeper exception.
6. Main: remove `<workdrive>/gf-FoulSanction*` after merge; `closure_file` (StoppageFlow `gen_translated.py`) is needed if any captured indirect target is outside the direct closure.

## 6. Checks run

- `snapdiff.py`: decide 2/2, commit 1/1, whistle 15/15, 0 mismatches (section 1). `frame_trace.py recorded`: all 18 captures return cleanly.
- Fast-tier interim line (`GATE_TIER` is not on main): `pes_judge_test` 22/22, `pes_sanction_test` 10/10, `pes_involvement_test` 6/6, `pes_match_info_test` 2/2 on `FoulSanction` `22ea7cb0` (build flags `-O0 -DNDEBUG` for speed; the full layer compiled: 4,186 objects), plus `gen_translated.py --check`: **3,989 functions (3,685 recorded bit-exact, 304 closure) regenerate byte-identical, rc 0**. The other `pes_*` binaries link the same sources and the same layer; not run.
- **GATE fast: not run.** Smoke: not run (the branch changes no behaviour; the cutover branch cannot yet run a foul).
