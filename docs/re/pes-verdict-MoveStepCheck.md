# MOVESTEP verdict (MoveStepCheck)

Base: origin/main a17df0d7507dd6d028f0e4376cb81bb0e1bfe216. Worktree <workdrive>/gf-MoveStepCheck (branch MoveStepCheck, no commits, nothing pushed). Source restored to HEAD after instrumentation. Smokes: gfsim_run only, `--seed 7 --minutes 2 --home 1 --away 2`, direct, no gf-sim.lock (contract).

## Verdict

MOVESTEP does not drive any player's simulated motion in this engine, switch off or on. SeamWire's merge cannot move a hash by construction, for three reasons.

1. Default smoke (no `PES21_ROOT` in the environment) makes `ProcessMoveStep` a no-op. `pesmovestep.cpp:43-44` reads `getenv("PES21_ROOT")` and returns nullopt when unset. `apps/gfsim_run.cpp:49` reads the env, and `:65` stores `--pes21-root` in a local that never reaches `setenv`. Instrumented no-env run: 851,246 of 851,246 calls returned at the constants check.
2. With `PES21_ROOT` set, MOVESTEP runs (706,750 calls) but its outputs do not reach positions:
   - Motion output: `Issue()` (`pesmovestep.cpp:266-270`) only calls `pes::RecordIssue` (`pes/movestep.cpp:321`). The `IssuedClip` is read only by `gdext/gf_match.cpp:129`, a viewer dictionary. The humanoid never reads it. `pesmovestep.hpp:3-5` states the same design.
   - Only sim-visible write: `player.SetPlaybackRate(rec.playbackSpeed)` (`pesmovestep.cpp`, end of `ProcessMoveStep`). It lands in `humanoidbase.hpp:225`, advances `frameNum` in `humanoid.cpp:241-245`, and sets position from `positions[frameNum]` at `humanoidbase.cpp:1198`. But `rec.playbackSpeed` was 1.0 on every call (`rateNot1=0`), so the store overwrites any other rate to 1.0 each tick. Without MOVESTEP the humanoid saw 5,216 non-1 frame advances; with it, 0.
   - Ablation: removing both playback writes (per-tick store and the one in `Issue`) returns the env-set 2-min hash to exactly the no-env value `a3d5e43a3b7b03af`. The playback overwrite is the entire env-on difference.
3. The seam's consumers never fire in the real run, and when forced to fire their output still does not move positions.
   - State histogram (env set, switch off): state 0 (INIT) 22 ticks, state 7 (GOTO_RUN) 706,728 ticks. No run state is reached.
   - `NearestOpponent` is called 0 times in both switch states. Its only callers are `movestep_ramps.cpp:181` (`FactorCeiling`, reached only via `AccelerationRamp`, `movestep_ramps.cpp:217-224`, run only in RUN_LOOP/STEP_TO_RUN by `RampsBeforeDispatch`, `:228-232`) and `movestep_run.cpp:1803` (`JostleStyle`, RUN_LOOP arm 5, called at `movestep_run.cpp:2146`).
   - Why GOTO_RUN never exits: `CommandSpeedClass()` reads `rec.speedClass` (`pesmovestep.cpp:186`; `movestep.hpp:64`), which has no GF writer. The action setter that stores it (FUN_1406eebb0, `movestep.hpp:693`) is not ported. Experiment (throwaway, reverted): forcing `speedClass = 2` moves the FSM into RUN_LOOP for 706,728 ticks. The seam then answers 1,407,142 times and the jostle changes style 852 times, but `issues=0` and the hash stays `d4a22e3211c6940e`. Style changes reach no position.
   - `rateCeiling` is also never written: `StoreRateCeiling` (`pes/movestep.cpp:38`) has no caller, so `RateCeiling()` reads 0 (`pesmovestep.cpp:187`) and `RunRateAllowed` (`movestep.cpp:63-64`) and every `EnterRunTurnStep` threshold use 0. A throwaway 0.9 ceiling alone did not move the FSM (state 7, `nearCalls=0`).
   - `CurrentMotion()` returns GF's `anim->id` (`pesmovestep.cpp:283-286`), while `movestep.hpp:612-613` defines it as PES's motion number. Reading the issued clip's motion instead cut `Issue` calls from 371,364 to 22 with no hash change.

## Why SeamWire did not move the smoke hashes

2-min smokes, seed 7, teams 1 vs 2:

| build | PES21_ROOT | switch | determinism |
|---|---|---|---|
| a17df0d (current main) | unset (brief command) | off | a3d5e43a3b7b03af |
| a17df0d | unset | on | c28ed174f671c91b |
| a17df0d | set | off | d4a22e3211c6940e |
| a17df0d | set | on | 617e345d10608e97 |
| pre-merge seam files (e703fe7 pesmovestep.cpp, teamAIcontroller.cpp/.hpp) | set | off | d4a22e3211c6940e |
| pre-merge seam files | set | on | 617e345d10608e97 |
| ablation (playback writes removed) | set | off | a3d5e43a3b7b03af |

Pre-merge equals post-merge in both switch states. The seam is unreachable in the default smoke (reason 1) and in the env-set smoke (reason 3). The 20-min hash was not run (policy: Main's batch merge).

## Wiring vs port

- Wiring bug, not fixed: MOVESTEP reads its constants from env `PES21_ROOT` (`pesmovestep.cpp:43-44`), while every other PES table reaches the match through `MatchSetup` and throws when absent (`match.cpp:235-238` pattern). gfsim_run and gdext already hold the install. Fixing this turns MOVESTEP on in default runs, and by the ablation the only effect is the playback overwrite (changes default GF behaviour). The fix touches match.hpp/cpp, matchsetup.hpp, gfsim_run.cpp, gf_match.cpp and pes_library.cpp, which are outside MoveStepCheck's ownership. Needs Main's decision.
- Port gaps, not wiring: (a) action speed class A-AN6 (FUN_1406eebb0), undecoded; (b) ceiling = TopRate at physique G (`pesstamina.cpp:500`, private to its anonymous namespace; G is a stand-in at `pesmovestep.cpp:192`; the speed stat and penalty share are not chosen); (c) no MOVESTEP-to-humanoid link: the humanoid never plays MOVESTEP's issued clip, so MOVESTEP cannot move players until that link is designed.

## Tests and changes

No fix shipped, so no test was added and no mutation was run. Temporary instrumentation and experiments were reverted with `git checkout HEAD --` on five files; `src/` matches HEAD. `build/` was rebuilt at HEAD.

## Verification

- Ran: gfsim_run, 2-min smokes (matrix above), plus instrumented/experiment runs. No ctest. Gate not run (Main gates).
- Pre-existing `EXPERIMENTAL` comments in `humanoid.cpp:536,582` and `match.cpp:2095` are the repo's own, not mine.
- `git status` in the worktree errors with "expected submodule path 'extern/godot-cpp' not to be a symbolic link" (the godot-cpp symlink). No effect on the build.

## Open items

1. Decide: MOVESTEP constants via `MatchSetup` (required, like stamina and carrier) instead of env. Note the default smoke hash then moves to d4a22e3211c6940e (2-min) through the playback overwrite. Decide that overwrite first, or accept it.
2. Port: action speed class (FUN_1406eebb0). Without it MOVESTEP stays in GOTO_RUN and the seam is dead.
3. Port: feed `rateCeiling` (`StoreRateCeiling` has no caller) from TopRate. Needs a public TopRate accessor and stand-ins for speed and G.
4. Design: MOVESTEP-to-humanoid link. Until it exists, no MOVESTEP output can move a player, and the seam cannot change any hash by design.
5. Bug candidate: `CurrentMotion()` returns GF `anim->id`, not the PES motion number (`pesmovestep.cpp:283-286` vs `movestep.hpp:612-613`). Candidate: the motion MOVESTEP last issued (`IssuedClip.request.motion`). Needs a PES check. Hash unchanged in experiment.
6. Interaction: with MOVESTEP on, its per-tick `SetPlaybackRate(1.0)` overrides contact stagger/rise-up rates (`match.cpp:1925,1941`). PES ordering not verified [INFERENCE].
7. Game path: gdext has the same env gate and no MatchSetup field for MOVESTEP. Not run [INFERENCE].
