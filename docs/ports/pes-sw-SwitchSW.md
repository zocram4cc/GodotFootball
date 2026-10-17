# SW: pes_loop switch (branch SwitchSW)

Result: Done for the switch, plumbing, tests and both smokes. Open items at the end.

Branch `SwitchSW`, worktree `<workdrive>/gf-SwitchSW`, not merged. Remote head `a8dbef6` (ls-remote confirmed).

## Commits
- `827b7ad` build: gfsim_run links pesmovestepcore again after pesfmt (prerequisite, see Notes)
- `45b38bb` sim: Match records the order of its tick stages; referee and ball are helpers (no behaviour change)
- `c8fde46` red: pes_loop on runs the ball and events before the referee
- `9f8f9f2` sim: pes_loop switch picks the head-of-tick order (green)
- `a8dbef6` plumb pes_loop: gfsim_run `--pes-loop`, GfMatch::start(pes_loop), match.gd `--pes-loop`

## 1. Inventory: PES-native vs GF, on origin/main d5f3065
PES-native, running in the tick:
- A1 ball tick: `Ball::Process` → pesballphysics/pesballtrack (ball.cpp:15, :18).
- A2 event store and detectors: `ProcessPesEventStage` (matchevents.cpp:68).
- Contacts `ProcessPesContacts` (match.cpp, after CheckHumanoidCollisions); possession `Team::UpdatePesPossession` (team.cpp:442); strategy decider via `TeamAIController::UpdateStrategy` (teamAIcontroller.cpp:2063) → pesstrategy.
- A6 clock: `PesClock::Advance` from `AdvanceMatchSeconds` (match.cpp:451-455); `PesClock::TimeUpGateOpen` read by Referee (referee.cpp:170).
- MOVESTEP: `pesmovestep::ProcessMoveStep` for each active player (player.cpp:467), before the controller.
- PES RandomInfo producer (head of Process).

GF, still in the tick:
- Referee and penalty shootout (referee.cpp, penaltyshootout.cpp): whistle, set-piece choreography, restart programmes; stalemate forcer (match.cpp stalemate block); MatchProgression (referee.cpp:196, match.cpp:552). Owned by A5.
- Controllers: TeamAIController / ElizaController (`teamController->Process`, CalculateManMarking, CalculateDynamicRoles in team.cpp), GF CastController and humanoid anim selection (player.cpp after ProcessMoveStep). Owned by A4.
- MentalImage: GF object built once at the head of the tick. No PES MA exists in src.
- CheckBallCollisions (GF), goals, possession side history, substitutions, UpdateBodies, heatmap, linesmen (officials.cpp).

## Plan premise: corrected, not implemented
The plan (§A2, §SW) and `docs/re/pes-re-tick-order.md` §2 call `FUN_14042b9f0` the match listener with "ten steps". `docs/re/pes-re-gaps-a2.md` (a4 and its corrections) says it is the analyze-channel listener, and the team tick runs through MatchObjectListener slot 2 (`FUN_140433760`). Steps 4-8 have undecoded bodies, and tick-order §4 says a reorder "cannot be justified from decoded PES sites". So I did not port "ten steps". The on-path implements only what is decoded: ball, then event stage, then the judge, then MA and teams. The judge-after-events placement is INF (referee event-driven, gaps-match §9.6 and a2 a3).

## 2. Switch and order
`Match` reads `pes_loop` (default false) once, in its constructor (`pesLoop`). `Process()` runs `CheckBallCollisions` (GF, both paths), then one order per tick:
- off (GF, = main): ProcessRefereeStage, ProcessBallStage, ProcessPesEventStage
- on (PES): ProcessBallStage, ProcessPesEventStage, ProcessRefereeStage

Then the GF MentalImage and the teams, unchanged. Named ponytails: the judge (`ponytail: GF's Referee … (A5)`), the team controllers (`ponytail: … A4`), and the MA placement (existing).

Plumbing: `gfsim_run --pes-loop` (flag, no value); `GfMatch::start(..., pes_loop)` with DEFVAL(false), so `test_match.gd`'s 7-arg calls still work; `match.gd` passes `args.has("pes-loop")`.

## Tests
`tests/sim/pes_tick_order_test.cpp`, two tests on a real Match from PES21_ROOT (both ran, neither skipped):
- `PesTickOrder.SwitchOffRunsTheRefereeBeforeTheBall`: Referee, Ball, Events, MentalImage, Teams. Passes at `45b38bb`; pins main's order.
- `PesTickOrder.SwitchOnRunsTheBallAndEventsBeforeTheReferee`: Ball, Events, Referee, MentalImage, Teams, checked over 3 ticks.
  - RED at `c8fde46`: actual Referee, Ball, Events on all three ticks (switch not read).
  - GREEN at `9f8f9f2`: 2/2 pass.

Mutations (each applied, rebuilt, run, reverted; tree back to committed, `git diff` empty):
- M1: on-path order Ball, Referee, Events → on test FAILED at ticks 0, 1 and 2.
- M2: config key typo `pes_loops` → on test FAILED at ticks 0, 1 and 2.
- M3: branch flipped to `!pesLoop` → both tests FAILED.

## Smoke (gfsim_run direct, PES21_ROOT set, no lock, 2 min, seed 7)
- Switch off, final binary: `[determinism] d4a22e3211c6940e`. Baseline (origin/main plus only the link fix): `d4a22e3211c6940e`. Identical.
  - `[restarts] 16 stoppages, longest 14.4 s`; passes 7-11; shots 3-4.
- Switch on (`--pes-loop`): rc 0 on both runs. `[determinism] 617e345d10608e97` on both (deterministic).
  - `[restarts] 18 stoppages, longest 14.2 s` (bound 32.5 s); no hang or crash.
  - passes 7-11; shots 1-1 (not zero).
  - `[idle] 20 episodes, stalls 13` (off: 14 and 7). Not a target under the smoke policy; recorded only.

## Verification run
- Built: gfsim_run, pes_tick_order_test, libgfsim, and the gdext object `gf_match.cpp.o`, all without errors. `ninja -t deps` shows `#deps 0` = 0 after the struct change.
- Ran: pes_tick_order_test, 2/2 pass (restored tree).
- Not run: full gate (Main gates the batch); ctest; the other test binaries; the godotfootball shared library (only its gf_match object compiled); any Godot scene (no visual change claimed).
- PII: grep of the branch diff for machine paths finds nothing.

## Notes
- The link fix `827b7ad` was needed to build origin/main at all. `gfsim_run` on d5f3065 fails to link: `pes::movestep::LocomotionStyle` is in pesmovestepcore, linked before pesfmt, whose movestep_run.cpp needs it. I re-listed pesmovestepcore after pesfmt for gfsim_run only. Moving movestep_style into pesfmt would break the per-group probe-stub tests, so I did not.
- Gating conflict: the brief rules say agents do not run gfsim_run or take the lock, but this assignment asks for the hashes and its contract says to run touched binaries directly without the lock. I followed the assignment.
- A relative-path edit targeted the user's `<local>` `CMakeLists.txt`. It was rejected before any write, and I used absolute worktree paths from then on. That checkout was not modified.

## Open items
1. The "ten steps" premise is wrong per `pes-re-gaps-a2.md`; steps 4-6 are undecoded. The plan's SW text needs the owner's correction.
2. MA placement: the GF MentalImage still builds at the head. PES builds the MA inside the team tick (a1). Not moved.
3. Judge after events is INF.
4. `Match::TickStages()` is a test-observable recorder in production code. It reuses its capacity each tick. Remove it once the switch is the default.
5. Other test binaries may hit the same `LocomotionStyle` link-order issue on main. My new target linked without a change.
6. `docs/` and `tasks/15-10-26.md` §SW are not updated (left to Main).
7. The §SW deletion list waits on A4 and A5 and the default flip.
