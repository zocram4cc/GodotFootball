# MoveStepWire: MOVESTEP drives the humanoid on the PES path

Branch `MoveStepWire` at `9939c40` (pushed, `ls-remote` confirmed). Merged `origin/main` `9cd4c8a` on top of `542f2f8`. Not merged to main.

## Commits (in order)
- `c7199fc` red tests (first attempt)
- `5fc173f` WIP, does not build, not gated (as Main asked)
- `8497a10` behaviour red tests run against main's source; seam `CurrentMotionOf` returns the GF anim id
- `6884672` merge `origin/main` `542f2f8`
- `b1ca485` items 1–2: constants via `MatchSetup`, env path deleted; MOVESTEP gated on `PesLoop()`; playback written only at issue
- `b1e5449` items 3–5: clips play on the humanoid, GOTO_RUN exits, speed class and rate ceiling wired, `CurrentMotion` is the PES motion
- `bcf2839` guard tests (each run red on a mutant, green on the committed code)
- `bf94903` gdext: include `pes/movestep_bundle.hpp` in `pes_library.hpp` (the godotfootball target did not compile without it)
- `9939c40` merge `origin/main` `9cd4c8a` (A1Spin spin pump behind `pes_loop`); clean merge

## Items (Main's seven)
1. Constants via `MatchSetup::pesMoveStepConstants` from `pes::LoadMoveStepConstants` (`src/pes/movestep_bundle.{hpp,cpp}`). Env loader deleted from `pesmovestep.cpp`. Match ctor throws if absent. CONFIRMED by test `ConstantsComeFromTheMatchNotTheEnvironment`.
2. `ProcessMoveStep` returns unless `match.PesLoop()`. Per-tick `SetPlaybackRate(1.0)` removed; playback written only on a SetMotion issue.
3. GOTO_RUN exit: the straight re-plan reads the current clip frame, `CurrentClipFrames()` (vt+0x38), as `iVar10` in `FUN_1406bdf10` (decompile line 47). Before this, `clipLen` was the record's blend value, so GOTO_RUN never exited. Exit enters RUN_LOOP via `EnterRun`. Speed class stored per tick (`StoreSpeedClass`), `MoveTopRate` feeds `StoreRateCeiling`.
4. Link: `PlayIssuedClip` on the humanoid (`humanoidbase.cpp`), locomotion family in `pesclips.cpp` `kFamilies`, parametric issues pick the nearest blend member (`pes::BlendGroup`). A same-motion re-issue on a locomotion clip only sets the rate. Tail issue of a due request (`req[0x2f]`, `FUN_1406b8340` tail) unless the handler already issued this tick. `req[0x37]` init 0x11 = SetMotion, verified: `FUN_1406b68d0` writes 0x11 at byte offset 0xdc (= word 0x37).
5. `CurrentMotionOf` returns the PES motion number from the clip's `specialvar2` for PES families, -1 for the AI's clip.
6. Ordering: `Player::Process` runs MOVESTEP first, then `Match::CheckHumanoidCollisions` (push, then `ProcessPesContacts`, `match.cpp:865`). MOVESTEP never writes a Trip body: `PlayIssuedClip` refuses a non-Movement, non-locomotion clip, and the contact stage writes only Trip bodies. Guarded by `MoveStepLeavesATrippedBodysContactRateAlone`. The PES-side contact order is taken from the existing code comment, not re-decoded [INFERENCE].
7. gdext: `gf_match.cpp` passes `library->movestep_constants()`. No `getenv` left under `src/gdext` or in MOVESTEP code. `godotfootball` target builds (exit 0) after `bf94903`. Not run in Godot.

## Tests (wire binary `build/tests/sim/pes_movestep_wire_test`: 11/11 green)
Red on main's code (first 8, verified before fixes): constants (rate 0), off-path (player changed), playback (quiet ticks overwritten), GOTO_RUN (stuck in state 7 for 19800 ticks), ceiling (0.0000), link (3782 issued, 0 played), advance (0 same-clip ticks), CurrentMotion (-1 vs PES motion, 1110 mismatches).

Added later, each red on its mutant and green on the committed code:
- `MoveStepLeavesATrippedBodysContactRateAlone`: M1 red (202 of 202 trip ticks changed)
- `AReIssueOfThePlayedMotionDoesNotRestartTheClip`: M5 red (re-issue at frame 3 → frame 0)
- `TheFsmKeepsIssuingAfterItsEntryClip`: M6 red (0 players with >1 issue)

## Mutations (each applied with edit, rebuilt, run, reverted; `src/` diff vs HEAD empty after all six)
- M1 per-tick `player.SetPlaybackRate(1.0f)` in `ProcessMoveStep`: red in `MoveStepLeavesATrippedBodysContactRateAlone` (202/202 changed) and `PesLoopWritesPlaybackOnlyWhenItIssues` (fails all 300 quiet ticks: rate 1 vs 0.5).
- M2 drop `if (!match.PesLoop()) return;`: red in `PesLoopOffLeavesThePlayerAlone`.
- M3 `CurrentMotionOf` returns -1: red in `CurrentMotionIsThePesMotionTheBodyPlays` (19800 checks).
- M4 drop `StoreRateCeiling` call: red in `TheRateCeilingIsStoredEachTick` (0.0000).
- M5 drop same-motion hold in `PlayIssuedClip`: red only in the new direct test `AReIssueOfThePlayedMotionDoesNotRestartTheClip`. The advance test stays green, because a same-motion re-issue almost never happens in a full run (74 issues in 19800 ticks), so the hold is rarely exercised there.
- M6 drop due-request tail: red in `TheFsmKeepsIssuingAfterItsEntryClip` (issues 74 → 22, each player exactly 1). Also turns the contact test red for coverage (0 trip ticks in the run), which is not a true contact failure.

## Counts (2-min seed 7 run, 22 active players, 19800 player-ticks)
- FSM states reached: 2. RUN_LOOP (3) 19778 ticks, GOTO_RUN (7) 22 ticks (one entry per player). Other states never reached.
- Clips issued: 74 total (22 goto entries, 52 from the tail). Played by the humanoid: 70 of 74.
- Same-clip ticks: 5977, advanced 5825, restarted 0.
- Rate ceiling: 0.9167.
- Seam call counts per seam not instrumented: not measured.
- Test binary counts (pre-merge): `pes_movestep_run_test` 40, `pes_movestep_test` 92, `pes_movestep_step_test` 13, `pes_movestep_turn_test` 43, `pes_clips_test` 7, `pes_stamina_test` 15. Post-merge: `pes_tick_order_test` 2, `pes_nearest_opponent_test` 2, wire 11.
- Gate not run (Main gates). Full ctest not run. No `flock` taken.

## Smoke (2-min, seed 7, teams 1 vs 2, `--pes21-root`)
Post-merge `9939c40`:
- Off, env set: `a3d5e43a3b7b03af` (matches main's env-unset off `a3d5e43a3b7b03af`)
- Off, env unset: `a3d5e43a3b7b03af`
- On, env set: `f260529762061bca` (two runs, identical)
- Pre-merge, same code minus A1Spin: on `e5621118762dcc30`, identical env set and unset.

Pre-change main (`a17df0d`) on hashes for comparison: on env unset `c28ed174f671c91b`, on env set `617e345d10608e97`. Main's on hash at `9cd4c8a` was not measured.

## Open items
1. FSM reaches only GOTO_RUN and RUN_LOOP in this run. STOP, ACCELERATE and the other states are unverified in a match. Likely more ports missing.
2. Word 0x26 (GOTO_RUN's foot flag) is unnamed in the headers and not written by the port. The port writes the mirror word 0x31 and tests 0x1f/0x24.
3. GOTO entry row overrides unapplied: foot band rows 0x561..0x566 and band 0x55f..0x571 (FUN_140a58550, FUN_14044e020), the distance arm (FUN_140439f90, 20 m gate, Percent(40)), and the out-of-play arm (FUN_140a6cf70, event ring). Existing `// ponytail:` notes.
4. `AnimeState()` returns 0 (stand-in for vt+0x28). The `AnimeState() == 0x4a` gate is never true.
5. Stand-ins: Speed byte 0x27 = `physical_velocity`; `PhysiqueG()` = 1.0; `BandSpeedKmh` = current speed; `MirrorChoice` and `RecentStamp` stand-ins.
6. Speed class is stored per tick, not at action entry (PES stores at FUN_1406eebb0's entry). `// ponytail:` note present.
7. Parametric blend picks the nearest member only; no crossfade.
8. Contact guard depends on trips occurring in the run (202 ticks here). A run with no trips fails it for coverage, not for behaviour.
9. Seam call counts not instrumented.
10. `docs/re/pes-re-anim-select.md` cites the tail (`FUN_1406b8340`, step 7). The VA was taken from that doc, not re-decoded in this session.
