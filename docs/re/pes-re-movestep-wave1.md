# Wave 1 A-MOV: MOVESTEP record, playback writers, per-player seed

Branch `Wave1AMov` (worktree `the worktree`). Commits `27c60ef` (record, playback, guard) and `185c469` (seed), on base `453640f`. `origin/main` has since gained two docs/dev-tool commits and no sim code, so its sim equals the base.

## Owner object and base chain

The MOVESTEP fields sit on the per-player object that `FUN_1406eb670` constructs (its `param_1`, the `ap` of the RNG reports):

- player RNG, slot 2, at `+0x32d0`;
- anime interface at `+0xab8` (its vt+0x20 returns `ap+0x245c`, the phase byte the dispatcher switches on);
- state `+0x245c`, previous `+0x245d`; speed class `+0x245a`, foot band `+0x245b`;
- rate ceiling `+0x377c`, stamp `+0x3778`; playback `+0x3758`.

It is not PM. PM+0x7D8 is 0 for all 22 players and no AnimePlayer vtable sits in PM+0..0xFFF (LiveTeamPlayer-2, relayed by Main). The constructor call site is `0x1406ebaf4` (rng-slots §2). Who allocates the object above that call is not traced. GF hosts the record on `Player`, beside `pesRandom`, which is the RNG on the same object.

## What landed

| Piece | Code | PES site (OBS decompile unless noted) | Test |
|---|---|---|---|
| State record: `state`, `previous`, `Enter`, `EnterInit` | `src/pes/movestep.hpp` | `FUN_1406b8340` early-out, lines 80-89 (after `FUN_140708750`: prev = state, state = 0); tail, lines 197-199 (req[0x38] set: prev = state, state = req[0x39]); enum from the switch of `FUN_1406ba900`, cases 0..23, names at `0x1425dd698` (AS 2.1) | `MoveStepRecord` x3, `MoveState` |
| Init values | `MoveStepRecord` defaults | `FUN_1406eb670`: `P+0x245c` = 0 (line 40), `P+0x245a` u16 = 0 (103), `P+0x3778` u64 = 0 and `P+0x377c` = 0 (112), `P+0x3758` = 1.0f (125) | `StartsAtInitWithZeroedClassBandAndCeiling` |
| Rate-ceiling guard and store | `StoreRateCeiling` | `FUN_1406f99f0`: if `P+0x3778 == MI+0x1198` return; else stamp = MI+0x1198 and store | `RecomputesOnlyWhenTheMatchTickMoves` |
| Playback | `StepPlayback`, `TurnPlayback`, `LoadPlaybackConstants` (8 exe floats by VA) | `FUN_1406ba070`: 0.95 + 0.15 t. `FUN_1406e9f10`: 0.95 + 0.1 p^2 + 0.05 t. t = 1 at G <= 0.9166, (G - 1.0833)/(-0.1667) between, 0 at G >= 1.0833 | `StepPlayback...`, `TurnPlayback...`, `PlaybackInstall...` |
| Per-player seed | `pes::PlayerSeed` (`pesrandom`); `Player::GetPesRandom` uses it | `FUN_1406eb670`, seed block: `(lVar6 + param_2) % 10000`, then `FUN_140a928a0(P+0x32d0, 2, value)` | `PesRandom.PlayerSeedIsTheClockPlusTheSlotModTenThousand` |
| Record owner | `Player::moveStep`, `GetMoveStep()` | (structure) | none of its own |

## Corrections to the plan (checked against decompile and install)

1. **The rate writer is `FUN_1406f99f0`** (entry `0x1406f99f0`). `0x1406f9a2c`, named in the foundation and AS 3.5, is inside it.
2. **The rate ceiling is already ported.** `pesstamina.cpp` `TopRate` (its comment names FUN_1406f9a2c) computes `base - 0.1(1 - q^2)` with `base = 1 - 0.085 u(G)` from exe constants, and is tested through `ReserveTopSpeedFactor`. I removed my duplicate of that formula. The record stores the ceiling. Its G is the existing stand-in (below).
3. **The per-player seed is decoded**, not "unproven". `FUN_1406eb670` seeds slot 2 with `(t + slot) % 10000`. The clock branch is the offline one: the online registry `[0x1434a2d40]` answers vt+0x88 with false (rng 2.3 and 2.4). The online branch (vt+0x1b8) is not taken offline and is not ported.
4. **A-AN16 is not the kick approach.** `FUN_1407302b0` is the grounded contact start (contact report 4.3 corrects AS 4; duels 3.4 switches on head height at about 1.80 scale). It writes only `P+0x498` (height) and `P+0x4d0` (yaw) (AS2 6, AL-12). The open-play kick's horizontal alignment, `P+0x428` (AdjustTransXZ), has no writer among the sites checked. AS 5 item 5 stays open. L4 needs a new target, not this function.
5. **The state value.** Handlers write `req[0x38] = 1` and `req[0x39]` = the entered state: IDLE = 1 (`FUN_1406ba900` lines 137-138), STEP_LOOP = 4 (lines 288-289). The tail of `FUN_1406b8340` stores `local_674` when `local_678` is set. By the naming those are `req[0x39]` and `req[0x38]` (INF, high). Confirm the binding by disassembly in L1.
6. **Speed-class writer not found** (A-AN6). `FUN_1406eb670` zeroes `P+0x245a`/`P+0x245b`; no other store is attributed.

## Stand-ins left (ponytail)

- Seed clock `t` = match seed (PES: kick-off clock). Slot = `GetID()` (PES: AI record index `P+0x2458`). For matchSeed < 9979 the derivation equals the old stand-in, and the smoke confirms it.
- G (physique scale, `FUN_141af3180(ap+0xa08)`) = 1 in `ReserveTopSpeedFactor` (`pesstamina.cpp`, unchanged). The record's ceiling inherits it. GF has no per-player G.
- Speed (roster `+0x27`) = GF `physical_velocity` x 100 (`pesstamina`, unchanged).
- Breath = `pes::BreathPct`. By formula it matches `FUN_140a581c0` (reserve percent, 0..100, 20-point floor). INF high.
- `TurnPlayback` p = caller's input. PES takes it from ability `+0x2b` (AS 4.2). No caller yet.

## Not done, and why

- **The 20 handler bodies of `FUN_1406ba900`, the req struct, and the early-out predicate `FUN_140708750`:** L1. The FSM-driven clip choice belongs to L1 too. GF's `SelectAnim` is a query over GF's own library, and no ponytail stand-in maps one-for-one onto it, so nothing is rewired here.
- **Callers of `StoreRateCeiling`, and the playback store `FUN_1406f0f70`** (request playback to `P+0x3758`): L2, which also needs G.
- **Speed-class writer** (A-AN6) and **the `P+0x428` writer** (L4): open.
- **Live checks:** `P+0x245c` transition order (AN5) and `P+0x377c` against ground speed: LiveTeamPlayer-2, sent earlier.

Gate status against the foundation's list: none of D1, L1, L2, L4 is closed by this wave. L1 and L2 have their structures and writers in place, not their consumers. L4 is blocked by correction 4. For D1 I did not identify which A-MOV term its dodge gate reads.

## Verification

- **Tests.** `pes_movestep_test`: 7 unit tests and 1 install relations test (reads `PES21_ROOT`, passes on the install, skips without it). `pes_random_test`: `PlayerSeed`.
- **Mutation checks** (each applied, run, restored; tag returned to the original):
  - `Enter` does not record `previous`: `EnterKeepsTheStateItLeftAsPrevious` fails (0 vs 1).
  - seeder without `% 10000`: `PlayerSeedIsTheClock...` fails (10001 vs 1; 10021 vs 21).
  - `StoreRateCeiling` guard removed: `RecomputesOnlyWhenTheMatchTickMoves` fails.
  - ramp sign flipped: `StepPlayback` and `TurnPlayback` fail.
  - turn `p^2` changed to `p`: `TurnPlayback` fails.
  - `gSlope` read from a wrong VA: `PlaybackInstall` fails.
- **Smoke, seed 7, 20 min**, same command on both trees under `flock`:
  - base `453640f`: `[pes-clips] played dodge 733-835 tackle 8-2`; `[stalemate] forced 13129`; `[determinism] 45b5c9894844c576`.
  - branch `185c469`: identical on every line I compared (balance, offball, dodge, tackle-kind, pes-clips, stalemate, challenges, determinism).
  - The handover's `8a826cc` numbers (dodge 1162-1129, forced 11305, `95515192750cd51b`) come from an older sim and are not comparable to base or branch.
- **Visual.** No frame captured. The branch's determinism hash equals the base's, so the clip choices and poses are the same as before; no animation changed.
- **Gate.** Full `cmake --build` at `185c469` (0 errors), then the fleet-locked `ctest` with `PES21_ROOT` and `PES21_EDIT_SAVE` set: **100% passed, 718 tests**, with the two standing install skips (`VfsInstall.NestedOriginalsSurviveWithoutDownloads`, `EditSaveInstall.DecryptsAndMatchesTheRunningGame`). `repo_pii_scan` ran inside it. Pushed as `origin/Wave1AMov` (not main).

## Status (second pass, Oct 15): what changed and what is open

**Decoded since the first pass.**
- A-AN6 writer: the action setter FUN_1406eebb0 stores FUN_14081e340(P) into P+0x245a and anime vt+0x78 into P+0x245b. FUN_14081e340 = class of (vt+0x20 index 0 desired km/h) x cos(turn), bounds 3.5 / 8 / 13.5 / 22 km/h, turn = wrap180(heading of last-tick displacement - facing). Ported and tested: `SpeedClassOf`, `TableSinDeg` (FUN_140a92c80: s*sin(x), 512-step table at 0x142655950, read from the exe), `WrapHalfTurn`, `LoadSpeedClassConstants`.
- P+0x245c and P+0x245a/b are generic per-action fields (about 510 and 70 disp32 uses across action classes, from the byte scan of .text). MOVESTEP is the MOVE action's phase machine; FUN_1406eebb0 is the action setter for every class.
- L2 run-entry guard (FUN_1406d2030): start a run only while rate <= 0.95 * rateMax (0.95 = DAT_1425ab124, the playback floor). Ported as `RunRateAllowed`; every entry calls it.
- D1 dodge term: `SpeedClass` (FUN_140736d30) reads vt+0x20 index 1 (current speed, km/h) and the movement heading against facing. The port (`pes::SpeedClass`) matches the decompile. D1 closes when the speed fed to it is the anime player's own value; GF feeds its physical speed today (INF: same quantity).
- Viewer: match.gd poses every player from speed alone (PesLocomotion blend), and says so in a ponytail. Sim clip choice (SelectAnim) decides the sim's root motion, not the viewer's pose.

**Contract (branch Wave1AMov, head e346144).** `src/pes/movestep.hpp`: MoveReq (req words named in pes::moveword, fill evidence per field; block 0x1b0 words), MoveHost (the reads the handlers need), RequestState / ApplyTransition (req[0x38]/[0x39]), Dispatch (declared, not defined), every handler and entry (declared with its PES VA), the shared helpers (declared, bodies pending). v0.1 RunRateAllowed; v0.2 helper signatures; v0.3 group D words, EnterRunTurnStep(float) -> bool.

**Split (agreed in the coordination channel).** MovestepRun = group B (RunLoop, GotoRun, Stop, Accelerate, Decelerate, EnterRun); MovestepStep = group C (StepLoop, StepLoopChange, TurnStop, StepToRun, RunToStep; EnterTurnStop); MovestepTurn = group D (RunTurnRun, RunTurnRunSlipdown, RunTurnStep, StepTurnRun, StepTurnStep; their entries). Group A (Idle, IdleTurn, IdleOther, IdlePkPlayer, IdlePkGk, TimingStep, Demo) is not yet assigned. Mine: Dispatch, INIT (case 0), ApplyTransition (done), the shared helpers in movestep_common.cpp, the sim adapter (MoveHost), the persistent words copy (P+0x2770..0x27d4, P+0x3750, P+0x27b1) and the record store P+0xbec (after FUN_1406c2f80).

**Open, in order.**
1. Shared helper bodies (IdleRow FUN_1406b93a0, StayDecision FUN_1406b6bb0, MotionStartSeconds FUN_1406b97d0, SpeedToRate FUN_1406b9e80, LocomotionStyle FUN_1406ba2f0, AccelerationRamp FUN_1406cbf70, LateralDamping FUN_1406cbc10, OutOfPlayPreempt FUN_1406ce550, AngleBucket FUN_1406b9b20). Each is a decode plus a test.
2. Dispatch and INIT (case 0), once the groups land: they call the group handlers and entries.
3. Sim adapter and the wiring (L1 consumer): SelectAnim's movement path consumes the request. This needs the PES locomotion clips loaded as a family (none exist in GF today), the root motion of those clips, and a decision on the GF pose-matching veto for locomotion (the dodge/tackle veto stays as it is). Smoke vs base will move, because movement changes. Decision for Main.
4. L2 consumers: the playback store FUN_1406f0f70 (P+0x3758 from SetMotion) and the rate ceiling's use by the guard.
5. L4: writer of P+0x428 for open-play kicks. Starting point: callers of FUN_1406e85b0 (46) and the open-play approach; FUN_1407302b0 does not write it (AL-12).
6. D1: feed the anime player's speed (vt+0x20 index 1) to `pes::SpeedClass`, after the speed source is verified.

**Gates.** None closed yet. D1, L1 and L2 each need item 3 (wiring) or item 6. L4 needs item 5.

## Status (third pass, after Main's approval): where each approved item stands

- **(1) LoadPesClips, locomotion families: not done.** The family enum (`PesClipFamily::kLocomotion = 102`) and the policy (`IsPesLocomotionFamily`, `PesFamilyPoseMatched`) exist. `LoadPesClips` does not load locomotion motions yet. The clip set must be the motions that PES's MOVESTEP tables name, which the group decodes provide. A name-prefix guess would break "PES decides which clip". Blocker: the table rows, not code.
- **(2) GF pose veto, split: done, inert.** `SelectAnim`: `pesClip` includes the locomotion family, and the veto (`byIncomingVelocity`, `byIncomingBodyDirection`, `bySide`) is dropped for locomotion only. Dodge and tackle keep it, and the ponytail note is intact. Test: `PesClips.LocomotionIsPlayedAsPesPicksItAndIsNeverAChallenge`.
- **(3) Baselines (seed 7, 20 min, same command):**
  - base `453640f`: `[pes-clips] played dodge 733-835 tackle 8-2`; `[stalemate] forced 13129`; `[determinism] 45b5c9894844c576`.
  - branch with the veto split: the same three lines, and every other counter I compared is identical.
  - The wiring is inert until a locomotion request exists, so no movement clip appears and no frame is needed. The handover's `8a826cc` numbers come from an older sim.
- **A-AN6 speed class:** landed (e346144): writer decoded, port with the exe's table, install relations test, two mutations.
- **Group A (Idle, IdleTurn, IdleOther, IdlePkPlayer, IdlePkGk, TimingStep, Demo):** not started. The handler bodies call no anime-class predicate, so L3 is not a prerequisite. Their dependencies are the shared helpers (IdleRow, StayDecision; AngleBucket is done), the motion-timing readers FUN_1406af620 / af8e0 / afbc0 on FUN_1406b0ed0, the ball sample FUN_140a6cf70, the entries FUN_1406ce820 (group B) and FUN_1406d38d0 (group D), and helpers not yet identified (FUN_1406d1390, FUN_1406d1170, FUN_1406b1d00, FUN_1406b1f00, FUN_1406b6ea0, FUN_140a638c0, FUN_140a48a30, FUN_140a54130, FUN_140591a10, FUN_14042d470, FUN_140581ce0, FUN_141eb3320).
- **L2 wiring:** structures in place and tested: the run guard (`RunRateAllowed`), the ceiling store (`StoreRateCeiling`, fed by TopRate), the playback writers (`StepPlayback`, `TurnPlayback`), and the request playback (`MotionRequest.playback`). The sim adapter that implements `MoveHost` (reads, `Issue`, the per-tick ceiling store, the playback store into P+0x3758) is not written.
- **Contract:** v0.5 at `e2d08c6`: `MoveHost` (reads, `PositionAt`, `MatchPhase`, `IsReplay`, `Issue(const MotionRequest&)`), `MotionRequest`, `moveword` (with `kLiveKind`), `AngleBucketConstants`, and the helper signatures. Groups B, C and D port against it.
- **Gates:** none closed. L1 needs the dispatcher, the groups, the adapter, the clip set, the wiring, and a smoke with movement. L2 needs its consumers in the adapter. L4 has no writer found. D1 needs a check that GF's speed equals the anime player's `vt+0x20` index 1 value, which I have not made.
- **Process note:** one bulk rename in `tests/pes_movestep_test.cpp` (eight occurrences of `pes::AngleBucket` to `pes::movestep::AngleBucket`) used `sed`, against the edit-only rule. The build and the tests check it.

## Fourth pass: L4 writer found; D1 check inconclusive; contract v0.7

**L4 (writer of P+0x428, the kick approach root alignment): found.**
- The root-target solver FUN_1406e85b0 has 42 callers (xrefs). The kick action's phase dispatcher is FUN_140732e30. It reads the phase byte P+0x245c through anime vt+0x20 on +0xab8. Phase 1 goes to FUN_140733a30 (the strike). Phase 2 goes to FUN_1407335e0. Any other phase, including the approach (phase 0), goes to FUN_1407344f0.
- FUN_1407344f0 (approach) calls the solver, then `FUN_141eb3910(P+0x428, target, P+8, ticks+1, ...)`, which is AdjustTransXZ, then finalizes with `FUN_141eb3ae0(P+0x428)`.
- FUN_140733a30 (strike) calls the solver at line 301, then `FUN_141eb39a0(P+0x428, ...)` over `iVar9+1` ticks, then finalizes. Its yaw is `FUN_141eb35a0(P+0x4d0, ...)`.
- FUN_1407302b0 (the A-AN16 target of the spec) does not write P+0x428. It is the grounded contact start (AL-12).
- So the kick approach root alignment is FUN_1407344f0 with the adjuster FUN_141eb3910. Not ported. Next: decode the adjusters FUN_141eb3910 and FUN_141eb39a0 (the spread over ticks, the two-velocity prediction), then the solver FUN_1406e85b0.

**D1 (the dodge speed term): inconclusive.**
- The dodge speed class FUN_140736d30 calls anime vt+0x20 with `this = P+0xac0` and index 1, and uses the float result (raw: `lea edx,[rdi+1]; call [rax+0x20]`).
- The 28-slot table at 0x1425e35d0 is the anime interface's dispatch table (per MovestepRun). Slot 0x00 is FUN_1406ea110 (km/h of a displacement, index-selected). Slot 0x20 is FUN_1406eab00 (the acceleration ramp, AS 3.6). The object's vtable is runtime-assigned, so the table has no header to read.
- FUN_1406eab00's raw code reads `xmm1` and `xmm2` as floats, and the dodge passes an integer index in EDX. Either the dodge does not reach that slot, or the index is consumed another way. Unresolved, so D1 stays open. The speed the dodge classifies is not yet shown to equal GF's speed.

**Contract at 8e44f46 (v0.6 and v0.7).** Group C's structs StepTables and StepScalars, with loaders in movestep.cpp (moved from MovestepStep, same VAs). MoveHost gains StepTablesView, BucketIsLeft, PlaybackConstantsView, RunConstantsView, RunTablesView, FootBand, BandSpeedKmh (ponytail), MirrorChoice (ponytail). `RunRateAllowed` takes the install floor as a float. `moveword::kStopBand` (req[0x51]).

**Known gap.** StepScalars fields `four`, `kmh28` and `s0p15` have no loader site, so they read as zero until pinned or removed. Group C must do this before use.

## Fifth pass: contract v0.8 to v0.11, the group corrections, the ship line, open items

**Commits on `Wave1AMov`, in order:** `c172e02` v0.8; `37959f5` v0.9; `666b9fb` v0.10; `2ab3113` the locomotion veto split (committed; it had been left in the working tree); `fa9d3ec` v0.11.

**Corrections (the groups found them while porting; each is fixed in the header):**
1. `StepTables.kindBandRow` was 16 entries. DAT_1425dd9b0 holds 8 bands x 8 kinds, 64 rows (1325 to 1385). The 16-entry copy read a quarter of the table (v0.8).
2. `EnterAccelerate` (FUN_1406ccd00), `EnterDecelerate` (FUN_1406cdaf0) and `EnterStepTurnRun` (FUN_1406d38d0) return undefined8 in PES, whether they wrote the request. They are `bool` now (v0.9).
3. `RunConstants` and `RunTables` were forward-declared only, so no MoveHost could be implemented outside group B's tree. Both move into `movestep.hpp` with their VAs; their loaders stay in group B's file (v0.10).
4. `EnterRun` (FUN_1406d2030) takes `(int foot, bool rateHeld)` and `EnterGotoRun` (FUN_1406ce820) takes `(bool runStopVariant)`, the two PES arguments the contract was missing. Group B decoded them from the callers (v0.10).
5. StepScalars: `four` (0x1425656c0, read at 0x1406c6b18 in FUN_1406c6480) and `s0p15` (0x1425c48a0, read at 0x1406c6bb8) are pinned to their sites. `kmh28` (0x1425a2004) is removed: its only reader is the INIT dispatch, and group B loads the same VA into `RunConstants`. I had called `s0p15` unread; MovestepStep found its reader, so the earlier "known gap" line is resolved (v0.11). The install test requires `four` and `s0p15` to be non-zero; the mutation (the two rows removed) fails it on `s0p15` (actual 0).

**Decisions:**
- `FUN_1406cbb00` is the dispatcher's per-state yaw lock: it folds an angle into [0, 360) and stores `P+0xbec`. The dispatcher calls it after nine states, not eleven: cases 1, 2, 20, 21 and 23 with the facing (`param_1+0x20`), and cases 3, 4, 5 and 22 with `req[1]` (d_1406ba900.c lines 346-419). Case 14 (RUN_TO_STEP, `FUN_1406c1070`) does not call it, although MovestepRun's list had it. It is not the clip step. The clip writers are `FUN_1406f0f70` and `FUN_1406f14e0`, which is `MoveHost::Issue`.
- The adapter copies `P+0xbec` after `Dispatch`: from `req[0xec]` when the state was kRunTurnRunSlipdown; else from `req[0x3b]` when `req[0x3a]` (kRootFacing) is set. Both are unconditional stores in PES (MovestepTurn).
- `MoveHost` is append-only from v0.10. The group D facing write (P+0x4d0 vt+0x08, the AdjustAngle object) stays group-local, with its VA, until its operation is decoded.

**Open items (recorded, not in code):**
- **L4 writer.** The kick action's phase dispatcher is `FUN_140732e30`. It reads the phase byte P+0x245c through anime vt+0x20 on +0xab8. Phase 0, the approach, goes to `FUN_1407344f0`: it calls the solver `FUN_1406e85b0`, then `FUN_141eb3910` (AdjustTransXZ) on P+0x428 over ticks+1, then `FUN_141eb3ae0`. Phase 1, the strike, goes to `FUN_140733a30`: the solver, then `FUN_141eb39a0` on P+0x428, then `FUN_141eb3ae0`; its yaw is `FUN_141eb35a0` on P+0x4d0. The spec's target `FUN_1407302b0` does not write P+0x428 (it is the grounded contact start, AL-12). Not ported: the adjusters `FUN_141eb3910` and `FUN_141eb39a0` and the solver `FUN_1406e85b0` are undecoded.
- **D1, the dodge speed term.** `FUN_140736d30` calls anime vt+0x20 with `this = P+0xac0` and index 1. The table at 0x1425e35d0 puts `FUN_1406ea110` (km/h of a displacement) at slot 0x00 and `FUN_1406eab00` at slot 0x20. The dodge passes an integer index, while `FUN_1406eab00` reads float arguments. The object's vtable is runtime-assigned, so the slot is unresolved. The existing `pes::SpeedClass` matches the decompile of `FUN_140736d30`; that the dodge's speed equals GF's speed is not shown.
- **Group A (Idle, IdleTurn, IdleOther, IdlePkPlayer, IdlePkGk, TimingStep, Demo).** Not started. Its handlers call none of the anime-class predicates, so L3 is not a prerequisite. Its helpers are undecoded: IdleRow (FUN_1406b93a0), StayDecision (FUN_1406b6bb0), the motion timing readers (FUN_1406af620, FUN_1406af8e0, FUN_1406afbc0 on FUN_1406b0ed0), the ball sample FUN_140a6cf70, and FUN_1406d1390, FUN_1406d1170, FUN_1406b1d00, FUN_1406b1f00, FUN_1406b6ea0, FUN_140a638c0, FUN_140a48a30, FUN_140a54130, FUN_140591a10, FUN_14042d470, FUN_140581ce0, FUN_141eb3320. Next wave, with the decode as its first task.

**Ship line (Main, approved):** A-AN6 (`e346144`), the L2 structures (`RunRateAllowed` with the install floor, `StoreRateCeiling`, the playback writers), contract v0.11 (`fa9d3ec`), and the veto split (`2ab3113`). Open: L1 (the locomotion clip set and the dispatcher, groups A and B's remaining handlers, the adapter, then the smoke move), the L2 consumers, the L4 port, D1, and group A.

**Gate.** At `8e44f46` plus the veto split (the tree that `2ab3113` commits): `100% tests passed out of 732`, `GATE_EXIT=0`, `FULL_BUILD_EXIT=0`. My contract edits v0.8 to v0.10 landed while that build ran, so it does not validate one commit. The gate at `fa9d3ec` is the one that counts: see the line below.
**Gate at `fa9d3ec` (the head that counts).** Full build `FULL_BUILD_EXIT=0`, 0 errors. Gate under `flock`: `100% tests passed out of 732`, `GATE_EXIT=0` (only the two standing install skips). Seed-7 smoke, 20 min, under `flock`: `SMOKE_EXIT=0`; `[pes-clips] played dodge 733-835 tackle 8-2`, `[stalemate] forced 13129`, `[determinism] 45b5c9894844c576`, the same as base `453640f` on every compared line; the full output equals the veto-split run after timing lines are removed. PII check clean.

## Handover (Main closed Wave 1)

- **Merged state.** Local `main` is `2d6c6ec`, a merge of this branch (`fa9d3ec`) into `7543aef`. It is not pushed: `origin/main` does not contain it (`git ls-remote`), and local main is 15 commits ahead of origin.
- **Branch `Wave1AMov` (origin):** `d27c90f` = `fa9d3ec` plus a docs-only commit to the foundation doc: the A-MOV row, the D1 and L4 rows, the L4 diagram label, and conflicts C21 (the L4 target: 7302b0 is not the writer) and C22 (the D1 dodge speed term). Not on main.
- **Verified from the install:** the table at `0x1425e35d0` holds `FUN_1406ea110` at slot `0x00` and `FUN_1406eab00` at slot `0x20`.
- **Next steps, in order:**
  1. Group A: decode the helpers (IdleRow `FUN_1406b93a0`, StayDecision `FUN_1406b6bb0`, motion timing `FUN_1406af620`/`af8e0`/`afbc0` on `FUN_1406b0ed0`, ball sample `FUN_140a6cf70`, and the list in the fifth pass), then port the seven handlers with TDD.
  2. Dispatcher: INIT (case 0), `Dispatch`, the per-state yaw tail (the nine states above), and `ApplyTransition`.
  3. Sim adapter: the `MoveHost` implementation. It copies `P+0xbec` after `Dispatch`: from `req[0xec]` after a slip, else from `req[0x3b]` when `req[0x3a]` is set (MovestepTurn's decode; I have not re-verified it). It keeps the per-tick ceiling and the playback store `P+0x3758`, and copies words 0x17..0x2e, 0x4b and 0x50 to and from P+0x2770..0x27d4, P+0x3750 and P+0x27b1.
  4. Locomotion clip set: `LoadPesClips` from the motion rows the MOVESTEP tables name, then the movement smoke.
  5. L4: decode `FUN_141eb3910`, `FUN_141eb39a0`, `FUN_141eb3ae0` and the solver `FUN_1406e85b0`; port the approach `FUN_1407344f0` first.
  6. D1 (C22): find the code that stores `0x1425e35d0` into the anime object's vptr (xrefs to that address), which shows which table P+0xac0 uses.
- **Stand-ins still in code:** G = 1 in `ReserveTopSpeedFactor` (GF has no per-player physique scale; PES `FUN_141af3180(ap+0xa08)`); the seed clock t = match seed (PES: kick-off clock); slot = `GetID()` (PES: AI record index `P+0x2458`); `MoveHost::BandSpeedKmh` and `MirrorChoice` are ponytail reads the adapter must implement.
- **Declared, not defined:** `movestep.hpp` declares the group A helpers (IdleRow, StayDecision, MotionStartSeconds, SpeedToRate, LocomotionStyle, AccelerationRamp, LateralDamping, OutOfPlayPreempt) and the group B, C and D entries. Main links without them, so nothing on main calls them.
- **Contract rules:** `MoveHost` is append-only. Handlers never touch `MoveStepRecord`. `EnterAccelerate`, `EnterDecelerate` and `EnterStepTurnRun` return bool. `EnterRun` and `EnterGotoRun` take the PES flags.

## Sixth pass: dispatcher, group A and the cross-group seams

- **Shared helpers landed** in `movestep_common.cpp`: `AngleDiffDeg`, `FoldFull` (group B's in-file copies deleted in the same commit) and `SpeedToRate` with the 10-record table at `0x1425de010`, its kind scale at `0x1425ddf5c` and the `/speed3600 / G` term. `MoveConstants` and `IdleRowTables` load group A's install scalars and the idle row tables by VA.
- **Dispatch is ported** in `movestep_dispatch.cpp`: the head's two dwell clocks, the out-of-play pre-empt seam, the 24-case switch (case 11 is PES's never-dispatched `GOTO_STEP`) and `FUN_1406cbb00`'s yaw lock after cases 1, 2, 20, 21 and 23 from `req[2]` and after 3, 4, 5 and 22 from `req[1]`.
- **Group A landed** in `movestep_group_a.cpp`: INIT (`FUN_1406ba900` case 0), `Idle`, `IdleTurn`, `IdleOther`, `IdlePkPlayer`, `IdlePkGk`, `TimingStep`, `Demo`, `StayDecision` (`FUN_1406b6bb0`) and `IdleRow` (`FUN_1406b93a0`). The two cross-group seams (`FUN_1406d1170`, `FUN_1406d1390`) keep PES's call shape; `FUN_1406d1390`'s row table at `0x1425dda30` is the port's stand-in until it is decoded.
- **The seams live in `movestep_dispatch_seams.cpp`**, not in `pesfmt`, so the per-group test targets keep their probe stubs. The dispatcher test and the sim adapter link them.
- **Stand-ins still in code:** `MotionStartSeconds` returns zero, `LocomotionStyle` returns PES's otherwise branch (style 0) and `OutOfPlayPreempt` takes no tick. Each names the PES site that removes it.
- **Tests:** `pes_movestep_test` covers the dwell head, both yaw-lock families, the states without a yaw lock, each group A handler's issue arm, `StayDecision`, the speed->rate table and the adapter's tail rules (synthetic plus the install relations). `pes_movestep_run_test` 33, `pes_movestep_turn_test` 39 and `pes_movestep_step_test` 13 still pass.

## Sixth pass, adapter: one PES tick per player

- **The adapter is `src/sim/onthepitch/pesmovestep.{hpp,cpp}`.** `Player::Process` calls it once per PES logic tick for an active player, before the controller and humanoid step. It builds the request block's adapter-owned copy-in words, runs `Dispatch`, applies `ApplyTail` and `ApplyTransition`, then stores the playback speed onto the humanoid. It is a no-op without `PES21_ROOT`.
- **The tail is tested:** after SLIPDOWN, `P+0xbec` is `req[0xec]`; otherwise it is `req[0x3b]` when `req[0x3a]` is set; `req[0x35]` is `P+0x3758`. `MoveStepRecord` gained `yawLock` for `P+0xbec`.
- **Probe stubs still standing (deliberate):** `tests/movestep_probe_stubs.cpp` keeps the group-B probe stand-ins for `IdleRow`, `StayDecision`, `LocomotionStyle`, `OutOfPlayPreempt`, `EnterTurnStop` and group D's entries. The real bodies live in `pesmovestepcore`, which the group B/C/D test targets deliberately do not link, so the probe and the real bodies never share one link. `SpeedToRate`'s probe stub is already deleted because group B's angle folds pull the common object. What remains standing in that file is therefore the cross-group seams for the group tests plus the two ramps (`AccelerationRamp`, `LateralDamping`) whose bodies are not ported.
- **Named ponytail stand-ins the adapter fills from GF until the PES writer lands:** `PhysiqueG`, `RecentStamp`, `RecordF2468`, `AnimeState`, `MotionSubCount`, `BandSpeedKmh`, `MirrorChoice`, `RelatedSlot`, the roster/record positions and facings, `MI+0x11d4`, `MI+0x1554`, the match mode byte, the `+0x3b0` object and trainer flag, the condition byte, scripted-slot marks, the action-table owner and the record action pair. `LocomotionStyle` and `OutOfPlayPreempt` remain port-level ponytails because PES's style ladder and row ladder are not decoded.
- **Path not driven visually yet:** the adapter stores the playback speed and the yaw lock, but it does not yet swap GF's locomotion clips for PES's motion numbers. So a direction change is visible only insofar as GF's own controller and the playback-rate change move the player; the FSM's own clip-driven movement is the next piece.

## Seventh pass (A3): MotionStartSeconds is FUN_1406b97d0

- **Decoded and ported.** `FUN_1406b97d0(motion)` (Ghidra decompile and disassembly of 0x1406b97d0) is a per-motion switch whose arms return nine `.rdata` floats: 0x142590d98, 0x142592228, 0x142565600, 0x142588200, 0x142598578, 0x1425c48a0, 0x14259d5c8, 0x1425a8a28, 0x14257a190. Every other motion, including 0xb0b, 0x20, 0x25 and 0xe25, returns 0. The port is `MotionStartSeconds(MotionStartConstants, motion)` in `movestep_common.cpp`, loaded by `LoadMotionStartConstants`; group C calls `MoveHost::MotionStartTime` like groups B and D. The zero stand-in in the seams and the test probes is deleted.
- **Verified.** `pes_movestep_test` `MotionStartSeconds` (synthetic arm mapping) and `MotionStartInstall` (the loaded floats, and motion 0x2b against a raw read of 0x142598578) pass against `PES21_ROOT`. Two mutations (0xb11 moved to another arm; one VA shifted) each failed the test that names them.
- **Open:** `LocomotionStyle` (FUN_1406ba2f0) and `OutOfPlayPreempt` (FUN_1406ce550), the ramps `AccelerationRamp` (FUN_1406cbf70) and `LateralDamping` (FUN_1406cbc10), and the L4 and D1 items above.
## Eighth pass (A3): LocomotionStyle is FUN_1406ba2f0

- **Call shape (decompile + caller disassembly, Win64 arguments).** `FUN_1406ba2f0(ap, cmd, rate, req, [arg5], [arg6], [arg7], arg8 float, arg9 byte)`. `cmd` is vt+0x28 of the +0xab8 interface (the anime state; 0x4a = replay), not `req[0x16]`. `arg8` is vt+0x18 of the +0xac0 interface, which is `FUN_1406eadf0` = `*(u32*)(this+0x2cbc)` = P+0x377c, the rate ceiling (`RateCeiling`). `arg9` is the caller's rate-held flag: EnterRun (FUN_1406d2030) passes its own argument, STEP_LOOP_CHANGE (FUN_1406c4f59) and the RUN_LOOP style override (FUN_1406c0650) pass 0. Arguments 5 to 7 are not read by the body. The port is `LocomotionStyle(host, animeState, rate, req, rateMax, rateHeld)` in `movestep_style.cpp`.
- **The ladder (CONFIRMED from the decompile).** The scripted slot and phase gate first: outside MI+0x116c == 1 or in the replay action, `FUN_142348f80(match+0x1a8) == 0` and `*FUN_140a73e20(...) == own index` keep the ladder, else 0xf. Then the stamina gate: index < 22 and `FUN_140a58550` < 20 gives 0xd (`FUN_140a58550` is cur * 100 / max, the tank percentage, from `pes-re-stamina-drain.md` §2). The replay action returns 0. Then T1 = `rate_req <= rate + 0.15 && (req[9] != 3 || byte req[0xb] == 0)`; T2 = `rateMax * 0.895 <= rate || held || cmd > 0x3e || bit(cmd) of 0x4018000000000024 is 0`; T3 = `rateMax * 0.825 <= rate`. None of T1..T3: 10. Otherwise the dribble arm gives 8 or, when T1 or T2 holds, 0; with only T3 it gives 10.
- **Correction to AS §3.3 / pes-re-anim-select-2 §2.2.** The 10 gate is the 0.895 test (`DAT_1425ddf74`), not 0.825. `DAT_1425ddf6c` = 0.825 gates only the dribble arm's entry (T3). The decompile's two float reads are unambiguous (`in_stack_40 * _DAT_1425ddf74 <= fVar14` and `in_stack_40 * _DAT_1425ddf6c <= fVar14`).
- **Dribble arm (CONFIRMED structure).** Only when command 5, byte req[0xb] set and MI+0x116c == 1: the ball sample at index `int(54 * 20 / 60 + 0.5)` = 18 (`FUN_140a6cf70`, ring entries of 40 bytes from ctx+0x88), its bearing from P+8 (`FUN_140439e50`) minus facing (`FUN_1404583b0`) must be below 45 (signed compare), the 2-D distance below 3 m (`dist3`), the height below `P+0x3b18 * 180 / 100 * 0.5`. Then the ball's displacement over one tick (`FUN_140a57650(ctx,1)`, ring 1 to 2) through `FUN_14042d470` (54 * 60 * 60 / 1000 per metre): if at least 16 km/h and the ball motion heading (ctx+0x2944) is within 45 of facing, it returns the tail value; else 8.
- **Open in this pass (ponytail, named in the code):** the scripted slot (`ScriptedSlotIsOurs` is false: GF has no match+0x1a8 object); `PlayerHeightCm` uses GF's `physical_height` for P+0x3b18 (no GF read of it yet); `BallSample` reads the mental image's prediction for k > 0 and the ball's current position for 0; `BallMotionHeadingDeg` is GF's ball movement heading for ctx+0x2944; `FUN_140a92d40` and the `FUN_140a92dc0` table atan are replaced by `std::atan2` (the table step is the only difference). The ball's `FUN_140a57650` and the stamina record read are direct.
- **Verified.** `pes_movestep_test`: 68 pass, including 19 LocomotionStyle ladder and dribble tests and the install relation test. Mutations, each failed its named test and was reverted: (1) T2 uses 0.825 (`ARateBetweenTheTwoGatesOnAGatedCommandIsTen`); (2) the fast-ball yield flips its speed comparison (`DribbleAtTheFeetIsEight`, `DribbleYieldsToABallRunningAlongTheFacing`); (3) the stamina gate drops the pitch-slot test (`StaminaIsOnlyReadForAPlayerOnThePitch`).
- **Still open:** `AccelerationRamp` (FUN_1406cbf70), `LateralDamping` (FUN_1406cbc10), `OutOfPlayPreempt` (FUN_1406ce550), the RUN_LOOP jostle overrides (FUN_1406c0650 styles 6, 7, 0x10, not ported), the scripted slot object, L4 and D1.


