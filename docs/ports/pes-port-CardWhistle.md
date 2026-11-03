# PES port CardWhistle: GF's foul stoppage words are what the translated whistle writes; commit kinds 1/3/4 and the G+0x400 arm proven (synthetic); a played match's foul stop is tested end to end (03-11-26)

Branch `FoulSanction` (local worktree `<workdrive>/gf-FoulSanction`, Release dir `build-rel`). CONFIRMED = tool output this session; INF = inference; OPEN = blocker named. Continues `docs/ports/pes-port-CardProof.md` (open items 1-4).

## 1. Merge (CONFIRMED)
`git merge origin/main` (`8429cfbc`, docs only) merged clean; pushed to `FoulSanction` (`git ls-remote` confirmed after every push).

## 2. The cutover (CONFIRMED: built, tests run)
Before: `DecideJudge` ran the commit `0x143F546F0` at the decision, its whistle `0x14050E770` wrote GF's private JR buffer, and `Referee::PublishPesStoppage` wrote every stoppage word from GF's `buffer` and `foul` (restart, side, spot, reason, foul kind, card level, fouler, victim), in play and at the stop.
Now:
- The commit is the **whistle**: `Referee::CheckFoul` runs `PesWorld::CommitJudge()` when it whistles a foul (after the hold and the advantage), not at the decision. `DecideJudge` only builds the pending foul and reports its level word (`judge+0x2C`), which the advantage reads (as PES's `0x14050FF50` reads the pending record). The whistle's level (JR+0x38) is the booking and the sanction's `foul.level`.
- JR is its own record again, **0x1000 bytes** (`kJudgeResultBytes`), and the whistle copies its first `kJudgeSyncBytes` = 0x80 into `MI+0x1554` (PES's frame sync `0x140422030`: `MI+X = JR+(X-0x1554)`). The stoppage words of a foul stop (`MI+0x1554` restart, `+0x1558` side, `+0x155C/0x1564` spot, `+0x1574` reason, `+0x1588` foul kind, `+0x158C` card level, `+0x1590` fouler, `+0x1594` victim, and the commit's stamp/count words `+0x15A0 ..`) are the whistle's, not GF's.
- Deleted: the commit at the decision, and `PublishPesStoppage`'s writes of foul kind, card level, fouler and victim from `foul` (during play too: PES holds zeros there until the whistle). `stoppageWhistled` makes the mirror leave a whistled stop alone until it ends; then `PesWorld::EndWhistle()` zeroes JR and its copy (PES clears JR at the restart: `0x14050A8C0` / `0x14050A990`, callers not traced).
- Kept, named `// ponytail:`: the mirror still writes every stop the translated whistle is not driven for (kick-off, goal kick, corner, throw-in, period end, offside). PES writes those through the same whistle from `0x14050E110`, `0x14050E020`, `0x14050E320`, `0x14050CC40` and the offside commit, which are translated but need the ball-out events and the offside SetPending record. The sync copy runs once at the whistle (PES's every frame).

**Finding that shaped the design (CONFIRMED by the smoke).** The first cut put JR *on* `MI+0x1554` (no copy). `SetPending` writes a replay snapshot at `JR+0x148` and the advantage flag is `JR+0xD08` (`docs/re/pes-re-duel-sim.md`), so JR is far longer than the 0x80 bytes mapped and the snapshot landed on `MI+0x169C ..` (the on-pitch bytes at `MI+0x1748`, the walk state `MI+0x1775`). The `--pes-loop` smoke aborted: `PES_DISPATCH_UNTRANSLATED 0x14211BF40` (a dribble unit's slot-0 reset), then `0x142132E20`. Ablation (temporary `GF_ABL` switches, reverted): putting back the old pre-whistle words (1), the old spot (2), zeroing the commit's extra words (4), all three (7) and none (0) all aborted at the same target, so none of the stoppage words was the cause; making JR a 0x1000 record with the 0x80-byte copy brought the smoke back to main's hash. (The old 0x80-byte JR overran the same way, into `judgeCtx` and past it: INF.)

**Translated on the way:** the five dribble-unit slot-0 resets `0x14211BF10/20/30/40/50` (one or two stores each, called by `0x14211E2D0` when the active unit's slot 0x18 returns true), because the first abort was inside one. Proven by `tools/dev/translate/synth_unit_resets.py` on 3 synthetic states each (rcx = stack scratch filled 0x00 / 0xAA / 0xFF; the unit object is chosen), 15 of 15 bit-exact, own-block 1.0, records in `results_core.json` marked `synthetic`. `closure.py --spec-set core --tag dribblereset`; `gen_translated.py`: 4,842 functions (3,724 recorded bit-exact, 1,118 closure), `--check` byte-identical. The smoke no longer reaches them; they stay (PES functions the dribble proposal calls, proven).

## 3. Commit kinds 1, 3, 4 and the G+0x400 arm (CONFIRMED: snapdiff --bounded; SYNTHETIC cases labelled)
`tools/dev/translate/synth_commit.py` (usage header in the file): `synth_call.py` over the memory of one real commit (`live2/pid2738437/seq147059`, unpacked, repacked after), registers as captured, bytes poked:

| arm | synthetic cases (poked) |
|---|---|
| kind (record word 5 → JR+0x38) | 1, 3, 4, 0, 5, 7 (kinds 1 and 4 → 1, 3 → the yellow constant, others → 0) |
| level word (kind 2 copies it) | 0, 1, 4 (a red: no capture holds one) |
| fouler's side class | slots 0, 3, 10, 11, 21, 22, 0xFFFFFFFF (the real fouls are 12 and 20) |
| `G+0x400` | pointer to a byte 1 with level 3 and level 4 (level forced to 1), pointer to a byte 0 (kind arms run) |

Replayed with the 11 real calls (pid2738437 seq147059 280370 376981 495001 538957 677650 769716 856163 882740 971365, pid2670420 seq7443) in one snapdiff run: **30 of 30 bit-exact, 0 mismatches, own-block 27 of 27 (was 20 of 27)**. 11 REAL + 19 SYNTHETIC. `results_core.json` (`cases` 11, `synthetic_cases` 19, note says which are chosen) and `results_judge.json` (the raw union). No translated body was mutated to show snapdiff's comparison bites; that is snapdiff's own proof on the roots. Not shown: that kinds 1/3/4 or the `G+0x400` flag ever occur in play (the registers are chosen).

## 4. Tests and mutations (CONFIRMED: built, run, reverted)
`pes_judge_decide_test` 21/21 (Release). Replaced/new in `tests/sim/pes_judge_decide_test.cpp`: `JudgeCommit.TheWhistleWritesTheStoppageWordsOfTheMatchInfoBlock` (the two recorded pending records, fouler of either side: restart, side = the side that is not the fouler's, reason 5, kind, level, fouler, victim, spot), `ASpotPastTheGoalLineIsWhistledOnIt`, `ADecidedFoulIsWhistledAtItsPendingLevelAndNotBefore` (the decision leaves MI zero, the whistle writes it), and the earlier recorded-commit test.
`tests/sim/pes_foul_whistle_test.cpp` (new, 3 tests, `PES21_ROOT`, each searched seed is one two-minute PES-loop match; the search stops at the first hit): seed 7 reaches a direct foul stop and takes it (21 s alone on a quiet machine). Logged: `restart 5 side 1 level 1 fouler 2 victim 20 yellow +0 red +0`.
- `ThePlayedFoulStopCarriesTheWhistlesWords`: MI kind 2, restart = the set piece GF armed (5 free kick, 6 penalty), side = GF's `buffer.teamID` and not the fouler's side, reason 5, fouler and victim on opposite sides.
- `TheBookingIsTheLevelTheCommitWrote`: card level 1..4 from the commit; yellows added = (level 3), reds added = (level 4) on that tick.
- `TheWhistlesWordsEndWithTheStop`: after the restart is taken, restart, level, stamp are 0 and fouler is `0xFF`.

| mutation | killed by |
|---|---|
| Ma: `CommitJudge` does not copy JR into MI | judge tests (3: words, spot, decided-foul) and the match test (no foul stop found within 900 s) |
| Mb: `DecideJudge` runs the commit | `ADecidedFoulIsWhistledAtItsPendingLevelAndNotBefore` |
| Mc: `CheckFoul` does not take the level from the commit and the commit never runs | the match test (no whistle words: no foul stop detected, 900 s) |
| Md: `EndWhistle` clears nothing | `TheWhistlesWordsEndWithTheStop` |
| Me: the mirror always writes (the guard removed) | the match test (the whistle's words overwritten: no foul stop detected, 900 s) |
Ma, Mc and Me fail the match test by timeout (its search never sees the whistle), not by an assertion: a weak kill, stated as such. Equivalent mutant, not run: taking `foul.level` from the pending word instead of JR+0x38 (equal for kind 2, the only kind GF's judge builds). The match test covers a plain foul (level 1, no card); a card at match level is OPEN (item 1).

## 5. Smokes and gate
Tree `af46a663`, Release `build-rel`, seed 7, 2 minutes, both rc 0 and deterministic:

| smoke | hash | main's baseline | stoppages / longest |
|---|---|---|---|
| switch-off | `3b73c968b4a377e0` | `3b73c968b4a377e0` | 26, 23.2 s |
| `--pes-loop` | `0e480b158cf80bd4` | `0e480b158cf80bd4` | 21, 15.9 s |
**Neither moved** (no ablation needed for the final tree; the ablation of section 2 explains the intermediate abort).
Test binaries run directly (Release): `pes_judge_decide_test` 21/21, `pes_foul_whistle_test` 3/3, `pes_judge_test` 12/12, `pes_sanction_test` 10/10, `pes_involvement_test` 6/6, `pes_offside_test` 17/17. `gen_translated.py --check`: 4,842 functions byte-identical. `ledger.py --check`: LEDGER.md current (38,081 rows).
Merged tree `85dec3cf` (origin/main `bd81fd2d`: batch AZ and DribbleArms merged in; only `docs/ports/README.md` conflicted), Release `build-rel`, `GATE_TIER=fast GATE_SLOTS="1 0"`, gate env set: ctest **`100% tests passed out of 2290`** (4,107 s), recorded re-check 0 of 245 / 685 / 546 / 2,253 / 34 selected (no recorded function changed), ledger current (38,081 rows), no unrecorded status regression. The gate's own smokes on the merged tree (seed 7, 2 min, rc 0): switch-off `3b73c968b4a377e0` (26 stoppages, 23.2 s) and `--pes-loop` `0e480b158cf80bd4` (21, 15.9 s), both equal to the baselines above: neither moved.

Final line: **`GATE fast: GREEN`**.

## 6. Open items
1. A card at match level: seed 7 has only plain fouls (GF's CPU never sets PES's deliberate-foul flag `ai+0x134`, so levels 3/4 do not arise in a quiet match: INF). The booking at levels 2/3/4 is covered by the decide and commit tests, not by a played match; a seed census for a level >= 3 foul would add it.
2. The other stops' stoppage words still come from the mirror (section 2); driving `0x14050E110/E020/E320/CC40` and the offside commit retires it.
3. GF's `buffer` (free kick or penalty, spot, side) still decides the restart; the whistle's words agree on seed 7 but nothing makes GF read them. A penalty-box foul at match level is not tested.
4. The dribble units' other virtual slots (`0x14211A550`, `A630`, `A6E0`, `A700`, `A330`, `A340`, `E0F0`, `E140`) are untranslated: any timeline change that finishes such a unit aborts with `PES_DISPATCH_UNTRANSLATED` (a tripwire; `0x14211BF40` and `0x142132E20` were the first two hit). `0x142132E20` is not translated.
5. Level-1 and level-4 captures: `judge_commit` `--when d(rdx+0x18)>=4` stays armed for a real red.
