# pes-fix-TSheetPort: free-kick kicker chooser FUN_1404a8ad0

Branch: `refs/heads/TSheetPort`, head `e40e971` (confirmed by `git ls-remote origin TSheetPort`).
Worktree: `<workdrive>/gf-TSheetPort` (detached from `origin/main` 36108d0).

## Commits

- `03ca407`: `53b9742` (pesteamconcepts: the team sheet) replayed onto `origin/main` 36108d0.
  Cherry-pick, not a rewrite; message unchanged (its "index-to-kick map not decoded" line is now stale).
- `af6fcc0`: WIP, red tests and API with stub bodies (pushed per the stall check).
- `e40e971`: the six-step waterfall, the constants, the caller, eight tests, the gate.

## Decode check (Ghidra, not just the doc)

Decompiled `FUN_1404a7d60` (selector, case 5), `FUN_1404a8ad0` (chooser) and `FUN_140a621f0` (rating fallback). Confirmed:

- Kind: second (excluded < 22) -> 2; dAtt < 32 -> 1; dAtt < 60 -> 0; else 6 (no designated). Designated is `S+0x143c+4k`.
- Keeper step uses `|z| < F7 && (dOwn < 28 || s*x < -F6)`; the `-F6` is `F6 * DAT_142588218`, so `-1` is used here and only here.
- Loop 1 (own half, `s*x < 0`): roles A in {1,2}, strict `< 65`, excludes `T+0x4b40`, no excluded-slot test.
- Loop 2 (`dAtt > 60`): role A `< 7` (keeper included), no exclusion, starts at 65.
- Fallback keeper rule: eligible iff role B is not 0 or `kind' == 0`. Ties keep the first slot.
- Re-pick gate (case 5): first re-picked if out0 is >= 22 or off-pitch; second only for FK type in {1,2,3} and when either is invalid; type 0 sets the second to 255.
- `MODE == 1` forces both picks, but case 5 is never MODE 1, so it does not apply.

Doc §4 matches the decompile. Doc §9 items 1, 2, 3, 6, 10, 14 remain open (see "Left").

## Changes

- `src/sim/onthepitch/pessetpieceteam.hpp/.cpp`: `SetPieceCandidate` (slot index, role, rating, slot target, on-pitch); `FreeKickInput`; `FreeKickTaker` (six steps); `PickBySuitability(candidates, kind, excluded)`; `FreeKickRepickGate`; `fkSignFlip` read from `0x142588218` via `ReadVa`. Stale claims about `T+0x1454` removed.
- `src/sim/onthepitch/teamAIcontroller.cpp`: the free-kick branch builds candidates from the full squad (keeper included; the old code stripped him, so the keeper step could never fire). Designated values come from `teamSheet.Designated(0..2)`. The taker is `nullptr` when no one qualifies, which hits the existing closest-player fallback. The old caller-side best-rating fallback is removed (the chooser now owns it). Comment corrected: the chooser reads `T+0xa244`, not `T+0x1454`.
- `tests/sim/pes_setpiece_team_test.cpp`: the free-kick tests rewritten, one per step; re-pick gate test; install test extended with 32, 60, 28, 65 and -1.

## Tests

Red stage (stubs): 8 `SetPieceTaker` tests and `SetPieceTeamConstants.ReadFromTheExe` failed on the stub values (`Which is: -1`, and `Which is: 0` for the flip). Note: the red was on this branch's stub API, not on `origin/main`.

Green, step by step: each step's test went green at its step, and no earlier test regressed.

Mutation checks (one branch broken per run, source restored byte-identical; each run listed its failing tests):

| Mutation | Failing test(s) |
|---|---|
| 1. kind-1 boundary `<` -> `<=` (32 m) | DesignatedKickerIsChosenByDistanceToTheAttackedGoal |
| 2. drop `excluded < 0` from reuse | PublishedKickerIsReusedOnlyInModeThree |
| 3. keeper boundary `<` -> `<=` (28 m) | KeeperTakesAFreeKickNearHisOwnGoalAheadOfTheLoops |
| 3b. drop `|z| < F7` gate | KeeperTakesAFreeKickNearHisOwnGoalAheadOfTheLoops |
| 4. loop-1 bound 65 -> 75 m | OwnHalfLoopTakesTheNearestBackWithinSixtyFiveMetres |
| 5. drop striker exclusion from loop 2 | FarLoopTakesTheNearestNonStrikerFromFarOut (+ fallback test, collateral) |
| 6a. keeper eligible at every kind | RatingFallbackKeepsTheKeeperForKindZeroAndTheFirstOnATie |
| 6b. tie `>` -> `>=` | RatingFallbackKeepsTheKeeperForKindZeroAndTheFirstOnATie |
| 7. drop excluded check from designated | SecondKickerIsDesignatedWhateverTheDistanceAndNeverTheFirst |
| 8. second gate ignores first validity | RepickGateFollowsTheSelectorsCaseFive |

Found by test, fixed: `FreeKickInput::designated` defaulted to `{0,0,0}`, which named the keeper (slot 0) as every designated kicker. Default is now `{-1,-1,-1}`.

## Gates

- Full build (`cmake --build build -j3`): pass, 1238 targets.
- Full ctest (`flock ... ctest --test-dir build -j3 --timeout 2400`, install and save env set), on `e40e971`: **963/963 pass**, 2 standing install skips (`VfsInstall.NestedOriginalsSurviveWithoutDownloads`, `EditSaveInstall.DecryptsAndMatchesTheRunningGame`). This ran before Main's rule change.
- After the rule change: ran directly, no lock: `pes_setpiece_team_test` 18/18 (PES21_ROOT set), `pes_team_concepts_test` 31/31 (PES21_ROOT set, install relation test included).
- `#deps 0`: none. PII grep on the four changed files: clean.

## Smoke

**Not run.** Main's rule change (no gate or `gfsim_run`, no lock) arrived while my queued smoke was running. I cancelled that job (`bg_805`, my own process) before it produced output. **I have no smoke number and do not claim one.** Main runs the smoke for this batch.

Expect a large move: the designated table is the sheet's install default (10 for all six kinds, see below), so every free kick inside 60 m goes to the player at team index 10 while he is on the pitch. That was not tuned.

## Stand-ins (all `// ponytail:` in code)

- Six designated values (`S+0x143c`, `T+0xa244`): the sheet's install default, read from the exe by `DefaultTeamSheet` (10 for all six). PES's tactics import that writes them is not ported.
- Per-player six-kind rating (`R_k+0x16e+kind`): one GF stat for every kind (`technical_curl`, as before).
- Slot target (`T+0x4b4c+12i`): the formation point the default placement uses.
- Outlet exclusion `T+0x4b40`: not carried. It reads 255 live, so PES excludes no slot.
- Mode-three phase (`MI+0x116c == 3`): no GF field; the reuse step is in the chooser and tested, but the caller passes `reuseMode = false`.
- Designated `p >= 11` aliasing onto the other side (doc §9.3): not reproduced. The chooser only sees the restarting side's squad.
- Online-practice branch (`G+0x400`): out of scope, ponytail comment in the header.

## Left

1. **Second kicker not wired.** `excluded` and the second-kicker kind (2) are ported and tested. GF has no second-kicker publish and no FK type (`Flow+0x2ab4`), so the caller makes no second call.
2. **`FreeKickRepickGate` is not called by the caller.** GF re-chooses the kicker once per restart (`PrepareSetPiece` runs once per team), so there is no persisted published kicker to gate on. Wiring it means deciding whether `taker` persists across restarts, which PES's `MI+0x173c` may or may not do. Not decided here.
3. Designated import (doc §9.1): replace the install default when the tactics import is ported.
4. Mode-three phase (doc §9.8): needs the restart phase.
5. A substitute does not inherit the starter's slot (`index` is the team-list index), so a designated starter who is subbed off falls through.
6. `elizacontroller.cpp` and `humanoid_utils.cpp` changes came in with `53b9742` (replayed unchanged). `elizacontroller.cpp` is on the busy list (ComStyleGaps); not touched by this work.
