# PES port CardProof: the judge commit 0x143F546F0 is proven bit-exact on 11 real calls (5 foul + warning, 5 yellow) and GF's card level now comes from it (03-11-26)

Branches `FoulSanction` and `CardLevel2Cutover` (local worktree `<workdrive>/gf-FoulSanction`, Release dir `build-rel`). CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

**Correction to the brief.** The hook's VA is `0x143F546F0` (`/PES21/rendezvous/hooks.cfg`: `hook judge_commit_43F546F0 0x143f546f0`, and each capture's `index.json` `rv.entry_va` = 5435115248 = 0x143F546F0), not `0x1443F546F0`.

**Level naming.** PES's card level is `JR+0x38` = `MI+0x158C`: 1 foul, **2 foul + warning, 3 yellow, 4 red** (`docs/re/pes-re-duels-fouls.md` 2). The brief's "level 2 (yellow) and level 3 (red)" are therefore: level 2 = a warning, level 3 = a yellow. **No capture holds level 4 (red).**

## 1. Merge (CONFIRMED)
`git merge origin/main` (`d808d8cb`, batch AX) was a fast-forward; pushed to `FoulSanction` (`git ls-remote` confirmed after every push).

## 2. Captures and the proof (CONFIRMED)
`live2/pid2738437/` holds 10 `judge_commit_43F546F0` captures (not three) and 8 `judge_decide_5B7470`. All unpacked (`capz.py unpack`), traced (`frame_trace.py recorded`, 18/18 rc 0), replayed with `snapdiff.py --bounded` (harness runs PES's bytes, the translated closure runs on the same snapshot; rax, xmm0 and every written byte compared). Level read from the capture (`rdx+0x18`, the pending record's level word):

| capture | level | | capture | level |
|---|---|---|---|---|
| seq147059 | 2 | | seq677650 | 3 |
| seq280370 | 3 | | seq769716 | 3 |
| seq376981 | 2 | | seq856163 | 3 |
| seq495001 | 2 | | seq882740 | 2 |
| seq538957 | 2 | | seq971365 | 3 |

| function | calls replayed | mismatches | own-block coverage | closure |
|---|---|---|---|---|
| `0x143F546F0` commit (11: these 10 + pid2670420 seq7443, level 1) | **11 / 11** | 0 | 0.7407 (20/27; was 0.7037) | bounded 12 functions, all translated, no blocked callee |
| `0x1405B7470` decide (12: these 8 + the four earlier) | **12 / 12** | 0 | 0.59 (118/200; was 0.43) | bounded 49 functions, all in the layer, no blocked callee |

The ten commits are 5 x level 2 and 5 x level 3, all direct fouls (kind 2, restart 5, free kick). The commit's own replay is bit-exact on all of them, including the whistle `0x14050E770` it calls. The 8 decide captures are all challenge actions (the `--when` predicate); none reaches the pending-foul builder `0x1405B8000` (it leaves with no foul or at the scorers / thresholds): the builder's output is the pending record the commit captures carry in memory, so it is still proven only by the two earlier cases and the GF tests.

**Closure widening: nothing needed.** The layer closure of the commit (18 functions, scanned from the generated files) and of the decide (84) holds no `OUT-OF-SET` call; every function either replay entered is in the layer (12 of 12, 49 of 49). `0x143E1C3A1` (CardLevel2's "OUT-OF-RANGE JMP") is not in the generated commit and no capture entered anything beyond the commit's 12.
Records: `results_judge.json` (the union snapdiff output: decide 12, commit 11, whistle 28 unchanged) and `results_core.json` (the two `proof: snapdiff` records: cases, coverage, notes). `gen_translated.py --check` (byte-identical) and `ledger.py --check` (current): section 5.

Commit semantics (from the Ghidra decompile of `0x143F546F0`, `dec.sh`): `FUN(judge, pending)`: calls the whistle `0x14050E770(judge, side-class, restart, 5, &pending[8], kind, pending[0], pending[1])`, copies pending words 2, 3, 0xB, 0xC, 0xE, 0xF, 0x10 into JR at +0x44, +0x48, +0x4C, +0x58, +0x54, +0x60, +0x64, then writes **JR+0x38**: kind (pending +0x14, judge +0x28) 2 -> the record's level word (pending +0x18, judge +0x2C); kind 3 -> 3; kinds 1 and 4 -> 1; else 0; and 1 outright when `G+0x400 != 0` and `0x141F9C3B0()` is true. Only kind 2 is in the captures.

## 3. GF wiring (CONFIRMED: built, tests run)
Before: GF's card level was the pending record's level word (`judge+0x2C`) read straight after the decide; the commit never ran.
Now (`pesworldrecord.{hpp,cpp}`): `PesWorld::CommitJudge()` runs the translated `0x143F546F0` over the judge's pending record (rcx = the judge, rdx = judge+0x14) and returns JR+0x38; `DecideJudge` runs it for every pending foul and reports that as `JudgeVerdict::level`. `Referee::JudgeContact` returns it, `RecordFoul` keeps it whole (booking: levels 3 and 4 give the yellow and the red, 2 gives a warning and no card), `PublishPesStoppage` writes it to MI+0x158C, `DecideSanction` feeds it to the event ring and the sanction tree `0x14047F170` reads it. So the card level, the booking and the sanction scene all take the commit's value. For the recorded kind-2 fouls it equals the pending level (the earlier path's value), so no behaviour moves for them; kinds 1, 3, 4 and the `G+0x400` case now follow PES (INF: the judge in GF produces only kind 2 today).
Not changed (ponytail kept): the whistle half of the commit writes its restart record into GF's own JR buffer (`judgeResult`, not MI+0x1554), so GF's `PublishPesStoppage` mirror is still the writer of the stoppage words (docs/ports/pes-port-FoulOffside2.md section 5 item 4).

## 4. Tests and mutations (CONFIRMED: `pes_judge_decide_test`, Release, 19/19)
New in `tests/sim/pes_judge_decide_test.cpp`:
- `JudgeCommit.ARecordedWarningAndYellowAreCommittedAtTheirLevel`: the pending record words of `seq147059` (level 2) and `seq280370` (level 3) staged at judge+0x14; `CommitJudge()` = 2 and 3, JR+0x38 equals it, and the stamp / count words (+0x4C, +0x58) carry the record's. Red on `origin/main` by construction (`CommitJudge` does not exist).
- `JudgeCommit.ADecidedFoulIsCommittedToTheResultRecord`: a forced yellow (face flag) leaves JR+0x38 = the verdict's level = 3; a no-foul contact leaves JR+0x38 = 0.

Mutations (each rebuilt, run, reverted; `git diff -- src tests` empty afterwards):
| mutation | killed by |
|---|---|
| M1 `DecideJudge` reads the level word (judge+0x2C): the commit is not called | `ADecidedFoulIsCommittedToTheResultRecord` |
| M3 rdx = judge+0x18 (the wrong record) | `ARecordedWarningAndYellowAreCommittedAtTheirLevel` |
| M6 the level read from JR+0x3C instead of JR+0x38 | 10 or more `JudgeDecide` tests (the output was cut at 10 lines; the whole `JudgeDecide` set reads the level) |
Equivalent mutants (not run): returning the record's level word instead of JR+0x38 (equal for kind 2, the only captured kind).

## 5. Smokes and gate
Tree `622086db` (origin/main `d808d8cb` merged; results records, `CommitJudge`, tests), Release `build-rel` (absolute path), `GATE_TIER=fast GATE_SLOTS="3 2"`, gate env set (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS=banks_union9l`): ctest **`100% tests passed out of 2282`** (3,004 s; includes `gen_translated --check`: 4,837 functions, 3,719 recorded bit-exact, 1,118 closure, regenerate byte-identical), recorded re-check 0 of 245 / 685 / 546 / 2,253 / 34 selected (no recorded function changed: only `results_*.json` notes), ledger current (38,076 rows), no unrecorded status regression. Smokes, seed 7 / 2 min, both rc 0 and deterministic:
| smoke | hash | main's baseline | stoppages / longest |
|---|---|---|---|
| switch-off | `3b73c968b4a377e0` | `3b73c968b4a377e0` | 26, 23.2 s |
| `--pes-loop` | `0e480b158cf80bd4` | `0e480b158cf80bd4` | 21, 15.9 s |
**The smokes did not move** (no ablation needed): GF's judge produces kind-2 fouls only, for which the commit's level is the pending level it replaced.

Final line: **`GATE fast: GREEN`**.

## 6. Open items
1. A level-4 (red) commit capture: the `--when 'judge_commit_43F546F0=d(rdx+0x18)>=2'` predicate delivered 5 x 2 and 5 x 3 in this launch and no 4; keep it armed (`d(rdx+0x18)>=4` would target it). Until then red is covered only by the decide tests (level 4 through the judge).
2. Kinds 1, 3, 4 and the `G+0x400` arm of the commit are not in any capture (blocks 7 of 27 unreached).
3. The whistle half of the commit writes GF's private JR; moving JR onto MI+0x1554 would retire the `PublishPesStoppage` mirror's card level, restart kind, fouler and victim words.
4. No test drives a `Match` to a foul stop and reads the booking; the `Referee` wiring is covered through `DecideContact` only.
