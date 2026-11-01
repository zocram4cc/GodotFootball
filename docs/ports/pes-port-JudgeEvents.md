# PES port JudgeEvents: which ball-event kinds the translated foul judge reads, every one pinned through the event store; the ring mutations now kill old tests (01-11-26)

Branches `FoulSanction` and `CardLevel2Cutover`: same head `2e0486ec` (`git ls-remote` confirmed). Worktree `<workdrive>/gf-FoulSanction`, Release dir `build-rel`. CONFIRMED = tool output this session (Ghidra bridge decompile of `0x1405B7470`, the translated function, test runs); INF = inference; OPEN = blocker named.

## 1. Merge (CONFIRMED)
`git merge origin/main` was a fast-forward to `0a9d1564` (nothing of this branch was ahead of main); pushed to both branches. No source file changed afterwards: only `tests/sim/pes_judge_decide_test.cpp`.

## 2. Decode: what the judge reads from the ball events (CONFIRMED: bridge decompile of `0x1405B7470` + translated `pes_fn_1405B7470.cpp`)
Names: A = victim (`param_2`), B = fouler (`param_3`). E0 = newest ball event (`0x140A3E190(E,0)` = the copy at E+0x180698), E1 = the one before it (`0x140A3E190(E,1)`: ring 0, head - 1). Per event the judge reads the kind (payload+0), the maker (payload+4 = {side, squad place}, resolved by the roster `0x140A62EE0`, 0xFF for a key `0x140A60530` refuses) and the frame (payload+0x10). "keeper" below is `bVar24`: `0x140A62690(ctx, side(B)) == B` (the side's first on-pitch man whose side-record unit word, +0x4C00 + 4 * place, is 0; the on-pitch byte is MI+0x1748+slot, `0x140A62610`). The side of a slot is `0` unless `11 <= slot < 22`, so an invalid maker (0xFF) is **side 0** (PES quirk, pinned).

Jump table `0x1405B7F6C` (14 dwords, kind - 1), read from the translated function's baked table:

| E0 kind | case target | what it does |
|---|---|---|
| 1, 2, 3, 4 | `0x1405B79E8` | cancel flag = keeper. Maker == A: "victim made the event" flag set; if B's action is 0xE the score is zeroed and a FORCE foul cleared. Maker != A: cancel only if keeper. |
| 5 | `0x1405B7A1A` | shot: victim-shot flag = (maker == A); cancel flag = keeper. A shot by A cancels (score zeroed, FORCE foul cleared, and the step-5 raise is skipped); a keeper is cancelled whoever shot. |
| 6, 7 | `0x1405B7B56` | cancel (score zeroed, FORCE foul cleared) only when keeper **and** side(maker) == side(B). |
| 8, 10, 11, 12, 13, 14 | `0x1405B7A50` | cancel flag = (side(maker) == side(B) and B's action is 0xE or 0x3D). Then E1 is read: E1 kind 1..4 sets victim-shot = 0, E1 kind 5 sets it to (maker(E1) == A); for E1 kind 1..5, keeper cancels, and maker(E1) == A sets the "victim made it" flag (the raise to at least a foul). E1 kind 6.. (not 5) or none: only the cancel flag above. |
| 9 and the rest | `0x1405B7B9D` | default: no case. |

After the switch (step 5): the ring walk E0, E1, ... (≤ 8 entries, `0x140A3E190(E,i)`) stops at the first event whose maker is A (not professional) or the first older than `gs/3` frames (`0x141E532E0` / 3): professional (a deliberate foul on a man down is then at least a yellow). The kind is not read there. The victim-made flag raises a zero score to a foul (level 1), the victim-shot flag skips it.

## 3. Tests (all in `tests/sim/pes_judge_decide_test.cpp`, 17/17; staged with the match's own `EventStore::Route` into ring 0 and the newest copy, so E0/E1 are exactly what the match's event stage leaves)
New helpers: `StageEvents` (events newest first, maker slot or none, age), `MakeKeeper` (MI+0x1748+slot), `Level` (verdict level, 0 = none).

| test | pins |
|---|---|
| `ABlockOnAManWhoJustKickedIsAFoul` (kept, now via Route) | E0 kinds 1..4, each: victim's kick raises a block to a foul, another maker does not |
| `TheVictimsOwnShotClearsAFoul` (kept) | E0 kind 5 by A clears, by B does not |
| `AKeepersContactAfterAKickOrAShotIsPlayOn` | keeper cancels for kinds 1..5 (each), makers B / B's mate / A's mate; maker A: foul for 1..4, none for 5; non-keeper unaffected |
| `AKeepersContactAfterATouchOfHisOwnSideIsPlayOn` | kinds 6 and 7 each: keeper + maker on B's side (B, mate, no actor) cancels; A / A's mate do not; non-keeper does not; kind 9 does not |
| `AnEventWithNoMakerCountsForSideZero` | invalid maker is side 0: cancels a side-0 keeper, leaves a side-1 keeper called |
| `TheEventBeforeAVictimsShotClearsAFoulWhateverTheNewestKind` | each of 8, 10..14: E1 = shot by A clears a tackle; shot by B / touch by A / no E1 does not; E0 = 9 and 15 do not look back |
| `TheEventBeforeAVictimsKickMakesABlockAFoulWhateverTheNewestKind` | each of 8, 10..14 x E1 kind 1..4 each: victim's kick makes a block a foul, B's does not; E1 shot / touch / E0 = 9 do not |
| `AKeepersContactIsPlayOnAfterAnyKickOrShotBeforeTheNewestEvent` | each of 8, 10..14 x E1 kind 1..5 each: keeper cancelled; E1 touch / no E1 not |
| `ABlockAfterAnEventOfTheFoulersSideIsPlayOn` | a block from behind (a foul alone, level 1) is cancelled by each of 8, 10..14 when the maker is on B's side (B, mate, no actor), not by A's side, not by kinds 9 / 6 / 1 |
| `AProfessionalFoulNeedsNoBallEventOfTheVictimsInTheWindow` | the ring walk: A's fresh event at depth 0 or 1 or 2 takes the yellow away; a stale event, or a stale entry in front of A's event, does not |
| `ANoBallContactAimedAtTheManIsAForceFoul`, `AProfessionalFoulIsAtLeastAYellow` (old, strengthened) | now also: the FORCE foul is cleared by A's own shot (not B's); the professional yellow is gone with A's fresh event |

### Mutations (each built, run, reverted; env-driven in a throwaway edit of the generated `pes_fn_1405B7470.cpp`, `git checkout` afterwards, generated file byte-identical again)
| mutation | killed by |
|---|---|
| drop kind 1, 2, 3, 4 (each) | `ABlockOnAManWhoJustKicked…`, `AKeepersContactAfterAKickOrAShot…` |
| drop kind 5 | keeper kicks test, `ANoBallContact…AForceFoul`, `TheVictimsOwnShot…` |
| drop kind 6 / 7 | `AKeepersContactAfterATouch…`, `AnEventWithNoMaker…` (6) / `AKeepersContactAfterATouch…` (7) |
| drop kind 8, 10, 11, 12, 13, 14 (each) | `ABlockAfterAnEvent…`, `AKeepersContactIsPlayOnAfterAnyKick…BeforeTheNewest…`, both `TheEventBefore…` tests |
| drop kind 9 | **equivalent mutant**: 9 already goes to the default |
| swap A for B: kinds 1..4 / kind 5 / second-event maker | block-kick + keeper-kick / force-foul + own-shot + keeper-kick / the three E1 tests |
| side(B) for side(A): group 8..14 / kinds 6, 7 | `ABlockAfterAnEvent…` / `AKeepersContactAfterATouch…`, `AnEventWithNoMaker…` |
| keeper ignored: kinds 1..5 / E1 group | `AKeepersContactAfterAKickOrAShot…` / `AKeepersContactIsPlayOnAfterAnyKick…Before…` |
| keeper requirement dropped for kinds 6, 7 | `AKeepersContactAfterATouch…`, `ABlockAfterAnEvent…` |
| group: drop B's action 0xE / ignore the side test | `ABlockAfterAnEvent…` |
| E1 range `kind <= 4` -> `<= 3` / E1 `== 5` -> `== 6` | the E1 block-kick and keeper tests / the E1 shot and keeper tests |
| ring walk: ignore the stale test / ignore maker == A / stop after one entry | `AProfessionalFoul…` (both) / both / `…NeedsNoBallEvent…` |

### The ring mutations (env-driven in `BindSanction`, reverted)
| mutation | old tests it kills now | the other three old tests |
|---|---|---|
| M9 `A1+0xE0 = 0` | `ANoBallContactAimedAtTheManIsAForceFoul`, `AProfessionalFoulIsAtLeastAYellow` (+ every new event test) | `pes_sanction_test`: 7 SanctionTree tests fail too (the tree reads ring 4 through A1+0xE0); `pes_judge_test` 10/10 |
| M8 judge ctx cell zero | the same two (+ every new event test) | `pes_sanction_test` 10/10; `pes_judge_test` 10/10 |
The five old tests that still survive both (`ATackleThatNeverPlayedTheBallIsNoFoul`, `ANoBallContactNotAimedAtTheManIsNoFoul`, `TheFaceFlagOutsideTheBoxIsAYellow`, `TheSameTackleWithoutIntentIsBelowAYellow`, `TheStagedActionBytesAreRestored`) test no store read: the verdict they assert is the same with an empty store.

## 4. Gate (interim)
**`GATE fast: not run (interim): tests run directly, nice 19, sim slots contended; pes_judge_decide_test 17/17, pes_sanction_test 10/10, pes_judge_test 12/12 (PES21_ROOT set, install tests ran)`**. Only a test file changed; `gen_translated.py --check` was not needed (the generated judge was mutated and restored byte-identical by `git checkout`; `git status` clean).

## 5. Not pinned / open
1. The staleness window `gs/3`: `0x14351AE50` (the install's 54, `0x141E532E0`) is 0 in the world window without the install tables, so only "age 0 fresh, age 5000 stale" is tested, not the 18-frame boundary. A boundary test needs the tables loaded (an install test).
2. B's action `0x3D` (the second action of the group's cancel test): no staged contact scores above 0 for it (288 contacts scanned: action, played, fell, angle, intent, aimed, keeper), so only the 0xE half is driven. OPEN: what `0x3D` is and which facts make it score.
3. What the kinds are as game events: only 1, 2, 3 (kicks), 5 (shot) and 7 (touch) are named elsewhere in the repo (`pesevents.hpp`); 6, 8, 10..14 are pinned by their table entry, not named. OPEN.
4. The step-5 early skip (ball within 5 m of A with no holder or A the holder, `0x1405B7C0E`) reads the ball and the MA holder, not the store; untouched here.
