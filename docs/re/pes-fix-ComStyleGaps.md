# pes-fix-ComStyleGaps

Result: Partially done. Items 18, 19, 20 and 22 are closed; item 21 is open. Branch `refs/heads/ComStyleGaps` is at `6420b5a` (confirmed by `git ls-remote`). One hand-off with A2Wiring is pending (see end).

## Commits (pushed)
- `6fde811` Port the Long score, veto and accepts; wire CurveShot's cancel and skill 7.
- `6420b5a` Long accept: designated player is the team's designated possession player.

## Items
- **F23 + item 18 (Long score): done.** `LongScore` reads the engagement base 30/60/80, COM 6 +50, skills 13/17 +50 each, the team level at +20 a step (two steps max), and the lofted ramp `10·clamp01((LoP−70)/29)`, truncated once. Path B (`teamSetting`) takes 10 per skill plus the ramp. Constants come from the install at 0x14211b2d9, 0x14211b2ec, 0x14211b30b and 0x14211b36b.
- **F24 + item 19 (Long veto and accepts): done.** `LongVetoed` is Intention 3/4 or engagement < 1 (0x14211a570). `LongAdmitted` runs the veto, then the accepts (team+4==3, designated, 47266) skip the roll, then `draw <= score`. The designated accept is live.
- **F25 + item 20 (CurveShot): done.** CurveShot was admitted but its cancel was always true (`planChoice(3)` was null), so an admitted curve shot was dropped at the next rethink. The cancel is now `PlanStillHolds` (head gates via `CurveShotScore`). Skill 7 is now `LongRangeDrive`, not `ScotchMove`: the engine bit order is in `pes-re-carrier-ai.md` §6.1, and the old mapping was wrong.
- **F22 + item 22 (comments): done.** The plan comment names the stats the code reads and says where PES reads more. Note: F22 in the §3 table is the Cross eligibility OR-gate, which is not a comment item. I read your "F22 + item 21" as item 22 (comments) plus item 21 (statTerm). Item 21 is open; F22 (Cross OR-gate) is open.

## Stand-ins (ponytails in elizacontroller.cpp)
- level = 0 (eval+0x54 / BallPlayer+0x70 not carried)
- Path B off (eval+0x3C not carried)
- team+4 == 3 = false; TeamData+47266 = false (also CurveShot's fullAttack)
- veto signed-X test and stored-slot test absent, so the veto never fires on them
- engagement = GF's nearest-opponent class (report 4.2) for sub_14069a3e0+16

## Tests
- RED: new tests fail to compile against the old API (`LongInput`, `LongVetoed`, `LongAdmitted`, `PlanStillHolds` and `longComBonus` etc. not members). Logged in the first `cmake --build` of the test targets.
- Added (carrier): `PesCarrierLong.{TheVetoRejects…, AnAcceptBypassSkipsTheRoll…, TheRollAdmits…, TheBaseFollows…, COMAndEachSkill…, ATeamLevel…, TheLoftedRamp…, PathB…}`. Install loader asserts the four new constants are loaded.
- Added (image): `PesImageSelection.ASelectedPlanHoldsWhileItsOwnGatesPass`.
- Removed: the old Long P-table expectation (Long is no longer in `PlanStartProbability`).
- GREEN: `pes_carrier_test` 38/38, `pes_image_test` 18/18, with `PES21_ROOT`.
- Mutations (six branches at once; each tripped exactly its own test, then reverted and green): veto `==4` removed → `TheVetoRejects…`; `fullAttack` accept removed → `AnAcceptBypass…`; roll `<=` → `<` → `TheRollAdmits…`; level cap removed → `ATeamLevel…`; Path B uses the Path A skill constant → `PathBTakesOnly…`; `CurveShotScore >= 0` broken → `ASelectedPlanHolds…`.

## Gate
- Build: exit 0 both times; the only warnings are pre-existing (animcollection.cpp:772, humanoid.cpp:2864).
- ctest, final state (`6420b5a`): **895/895, 100%**, with 2 skipped. `VfsInstall.NestedOriginalsSurviveWithoutDownloads` skips because the install has `download/` overrides. `EditSaveInstall.DecryptsAndMatchesTheRunningGame` skips because the local EDIT save fails its own SHA-512 check on block 2 (pre-existing, not touched).
- `repo_pii_scan` passed in ctest. machine paths grep on the diff: 0 hits.
- `ninja -t deps`: no `#deps 0` object.
- **Process error:** my first gate run (ctest) was inside `ulimit -v 2000000`, which broke `godot_match_scene_test` with `std::bad_alloc`. The brief forbids ulimit on a shell that runs Godot. I reran that test unlimited (passes, 16.7 s). The 895/895 above is the unlimited run.

## Smoke vs main
- Ours: `[balance-passing] passes 45-41 | accuracy 28%-9% | clean 24%-9% | clearances 2-3`, `[determinism] 63286486ec6c84de`. Identical to main.
- Plan admission: changed in code (Long can now be vetoed, accepted or rolled; CurveShot stays selected). Measured on seed 7, 20 minutes, with a temporary probe (removed before commit): rethinks with plan selected Basic 311, Breakthrough 36, CurveShot 133, Long 0, MiddleShot 0, Cross 0.
- The hash cannot show the change: in GF the image plan id reaches the sim only through `shoot.planId`, read solely for `kPlanShotPassFromGoal` (plan 8, not ported). I did not build main to compare per-plan counts, so "changed" holds for the code and the probe, not for sim output.

## Still open
- **Item 21:** `statTerm` is a single stat with no per-stat truncation for Breakthrough, MiddleShot, Cross or Long. CurveShot already truncates per stat.
- **F13–F17 (MiddleShot):** skill 34 RisingShots missing; COM 5 (Long Ranger) missing; state2 +40 never applied; KP missing; the floor VA comment is still wrong.
- **F18 (MiddleShot gate):** the region test is still an approximation.
- **F20, F21, F22 (Cross):** base precedence; KP; the OR-gate and roll bypass.
- **F27:** per-stat truncation for Breakthrough and MiddleShot (item 21).
- Long stand-ins above (level, Path B, team+4, 47266, signed-X, stored slot).

## Stale doc rows (not changed; flag for Main)
- F7 and F8 say selection persistence and the Intention ratchet are absent. Both exist in GF now (`AdvanceImageSelection`, `NextIntention`).
- F11/F12 say `breakthroughStatGain` and `crossStatGain` are never assigned in production. They are loaded from the install at `pescarrier.cpp:257–258`, and `PesCarrierInstall` asserts them > 0 and passes.
- F26/F10 say the Breakthrough divisor is 29. The code uses `breakthroughStatSpan` (20, loaded from 0x14259be1c).
- `pes-re-carrier-ai.md` row 6 says `BallPlayer+0x58` selects the 10+10 path. The asm (0x14211b27f) tests eval+0x3C, and §6.6.4 and §6.6.7 confirm it. I followed the asm.

## Pending hand-off (A2Wiring)
A2Wiring is deleting `Match::BallEvent`. Agreed sequence: they push an API-only commit (`GetPesEvents`, `GetPesActorPlayer`, `pes::NewestEvent`) and send me the SHA. I rebase onto it and apply two edits: the human dodge window (~1322, `newest`/`gate.newestEvent*`) and `_Carrier()` (~789, `last`). I then push `ComStyleGaps`. Not done yet at the time of this report.
