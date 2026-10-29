# PES match-info block and G-header records in GF's world window; the angle runs on GF's window (MatchInfo, 29-10-26)

Branch `EndClear`, worktree `<workdrive>/gf-EndClear`. Tags: CONFIRMED = read in tool output this session (decompile by `dec.sh`, capture read, live read, test run); INF = inference; OPEN = not settled. Capture used: `live2/pid407792/seq021` (the corridor-angle capture, oracle dir `gf-EndClear-scratch/oracle/seq021`, PES-derived, not committed).

## 0. Verdict

- Step 1 DONE: `origin/main` (`3fc5c1d0`), `origin/SpecGen` (`70cacd7d`) and `origin/FoulOffside` (`69466b20`, it carries the MI block in the window) merged into `EndClear`; pushed.
- Step 2 DONE for what GF models, with the rest listed OPEN (§3): MI's frame counter, the ball record's prediction table and velocity, the pitch record F, the team records' attack angle, and the frame-random record are now window state in PES layout, with a GF writer each. The GF copies they replace are deleted (clean cutover, §2).
- Step 3 DONE: `AngleOracle.TheCapturedSituationOnGfsWindowGivesTheCapturedAngle` rebuilds the captured situation in a `pes::PesWorld` through the window's own accessors and runs the translated `pes_fn_142108630` on it: the angle equals PES's bit for bit (CONFIRMED). The original oracle tests still pass. `EachWindowRecordBindingChangesTheAngle` flips one byte per binding (16) and checks the call's path changes.
- Steps 4 and 5 NOT DONE (§5). The window does not yet cover the angle's whole path: the AI context objects it reads (ctx, MA at ctx+0x18, the event store E at ctx+0xE0) are still test-supplied segments, and the danger writer's call tree is untranslated. `FillUnitTable` kind 1 is not wired; the stand-ins at `elizacontroller.cpp:2774` and `:2951` remain.

## 1. Identities decoded (CONFIRMED by decompile, `dec.sh`; offsets paired with the PES function)

| G slot | What it is | Evidence |
|---|---|---|
| G+0x40 | **the ball record B** (not a "carrier record") | `0x140A57800` returns B+0x88 (sample 0), `0x140A57720` returns B+0x2948, `0x140A6CF70(B, j)` = B+((j*5)+0x11)*8 for j < 250 else B+0x2770 (= sample 249): a 250-entry prediction table, stride 0x28, first three floats x, height y, z. On seq021 sample 1 − sample 0 times 54 = (9.40, 3.24, 19.62) and B+0x2948 = (9.58, 3.38, 19.65): the vector is a velocity in m/s. |
| G+0x270 | MI (FoulOffside2) | frame counter MI+0x1198: `0x140A128E4` is `inc dword [rsi+0x1198]` and divides MI+0x1194 by it. `0x14063A490` reads it at its `*(lVar16 + 0x1198)` (param_1[1]). |
| G+0x360 | the **frame-random record**, three words: +0 u32 0..32767, +4 u32 (the byte the set-piece kicker tests), +8 a float in [0.25, 1) (INF: unit draw) | `0x14063A490` copies `*(*(G+0x360)+8)` into its state (`param_1[6]`). Live read, PES pid 1467004, 4 reads over 3 s: words +0, +4, +8 all changed every read (+0xC stayed 0x682C). |
| G+0x2AA8 | the pitch record F, F[k] f32 at +4k, 18 floats | seq021: F[0] = 52.56, F[1] = 34.06, F[2] = 3.66, F[3] = 2.44, F[4] = 47.06, F[5] = 9.16, F[6] = 36.14, F[7] = 20.12 equal `gametypes.hpp`. The slot is stored by PES's G reset `0x140A48273` (also G+0x270, G+0x360; INF that it is the reset). |
| side record +0x250 | attack angle f32 (90 or 270), +0x254 sign s8 | `0x1404FEBD0` stores it (AttackAngleDeg's note). seq021: side 1 = 270.0 / 0xFF, side 0 = 90.0. |
| ctx (angle A0) | an AI context: +0x18 = MA (pointer), +0xE0 = **a pointer to the event store E** (the earlier "E0 ring word 0" flip was the pointer's own byte) | seq021 ctx+0xE0 = `0x7ff5dff60018`. `0x140A3E190(E, 0)` returns E+0x180698 (the newest-event copy) whose first word is on the path. |
| MA | ball copy at MA+0x2482C/30/34 (44.04, 0.96, -9.78), carrier id u32 at MA+0x2483C, lane tables at MA+0x229B4 and MA+0x22B9C by side (`0x140A714E0`), 11 entries of (slot id u32, distance f32) | on-path ids MA+0x22A0C..0x22A5F (seq021, scan). |
| `0x14063A490` | the carrier-state builder the angle calls through `0x14063A290`: reads MI+0x1198, +0x116C, +0x1554, +0x155C, B+0x2920, G+0x360, the pitch record, E records, man +0x554/+0x55C/+0x5D0, slot +0x1414/+0x1420, anime +0x1A/+0x1C | decompile |

Corrections to earlier docs: `pes-re-record-layouts-chains.md` §2 and `pes-port-WorldWindow2.md` §3 call G+0x40 "the carrier record"; it is the ball record. Its `+0x2770/+0x2778` "fallback position" (FoulOffside) is sample 249 of the table. `kSlotLo/Hi/CapOffset` (slot record +0x1A/+0x1C/+0x22) were never read or written; PES's pair is on the anime entry (pes-re-record-ids.md §4), now `kAnimeLoOffset/HiOffset` (names only; writers OPEN).

## 2. What changed in GF (commit `d99bbbf2` and the test commit after it)

Layout (`pesworldrecord.hpp`): `PesWorldTables` names `frameRandom` (G+0x360) and `pitch` (G+0x2AA8); `PesMatchInfo.frame` (MI+0x1198); `PesBallRecord` grows to 0x2960 with the table and velocity constants; new `PesPitchRecord` (0x48) and `PesFrameRandomRecord` (0x10) regions; `kSideAttackAngleOffset`; `PesWorld::BindCall(PesState&)` points a translated call at the window.

| Record field | GF writer (site) | PES writer | Copy deleted / reader repointed |
|---|---|---|---|
| MI+0x1198 frame | `Match::Process`, once per tick beside the clock (`match.cpp`) | `0x140A128E4` `inc [MI+0x1198]` | `Match::GetPesFrame` read `PesFrameOf(actualTime_ms, gs)` (floor of the integer ms times 54 / 1000); now reads the window word. `PesFrameOf` and its two tests deleted. |
| B+0x88+0x28*j samples | `Ball::CalculatePrediction`, `Ball::ResetSituation` (`ball.cpp`) via `SetTrackSample` | the sampler `0x140A6CF70` indexes it; its rebuild per tick is officials-ball 5.3 (the writer's VA is not decoded: OPEN) | `Ball::track` (`std::array<Vector3, 250>`) deleted; `Predict`, `TrackSample`, `GetTrack` read the window. `Referee::PublishPesStoppage`'s `SetBall` (it wrote sample 249 = the current ball) deleted: the stoppage functions now read the real table. |
| B+0x2948 velocity | `Ball::Process` / `ResetSituation` via `PublishMomentum` | OPEN (reader `0x140A57720`) | none (the ball's `momentum` is GF's physics state; the record is the PES copy it publishes: INF mirror, see §3 item 2) |
| pitch F[0..17] | `Match` constructor, from `gametypes.hpp` | PES's FieldInfo (`pes-re-fieldinfo.md`) | none (constants) |
| side +0x250 / +0x254 | `Referee::PublishPesStoppage` | `0x1404FEBD0` | none |
| man pos, vel, facing | `WriteManMovement` (already) | | |

## 3. Not in the window yet (OPEN, each with the bytes the angle reads on seq021)

1. Ball record: f32 +0x4, f32 +0x1C / +0x24, u8 +0x4C, u8 +0x80, u32 +0x2920. The flag +0x80 and the point +0x1C/+0x24 flip the call's path; +0x4, +0x4C, +0x2920 are copied from the capture but not on this call's path. No GF writer (semantics INF only). `+0x84` was read as on-path by the byte scan but flipping it on the window left the path unchanged: not claimed.
2. B+0x2948 is a PES-format copy of GF's `momentum`, which is a second store for the same quantity. Clean fix: make the PES velocity the store and `Ball::GetMovement` read it. Not done.
3. Frame-random record: no GF writer (PES writes it every frame; writer not found; GF has no frame-random draw, and adding one would consume RNG). Zero in the window.
4. Anime +0x11C f32, +0x1A/+0x1C u16; slot +0xA1, +0xA4, +0x12A4: constants named, no GF writer (the test copies them from the capture).
5. Man records: only facing, pos, vel (+0x554, +0x55C..0x564, +0x574..0x57C) and the contact byte have GF writers; the action word +0x5D0 does not.
6. ctx, MA (lane tables, ball copy, carrier id), E (the event store): GF's `EventStore` is a struct, not window bytes; `newest_` is an `EventRecord` whose layout equals PES's 800-byte record, so it can be re-seated into a window region (as `PesClock::record` was) when E is moved.
7. Other angle captures (seq028, 029, 032, 034, 035, and pid463316's) were not scanned: this on-path set is a lower bound.

## 4. Verification

Run directly, not through ctest; env `PES21_ROOT=<workdrive>/PES21`, `GF_PES_ANGLE_ORACLE=<scratch>/oracle/seq021`; sim tests in a slot (`tools/dev/simslot.sh`, `GF_SIM_SLOTS=4`):

| Binary | Result |
|---|---|
| `pes_angle_oracle_test` | 4/4 (the two older tests, the window test, the 16-flip test) |
| `pes_tick_a1_test` | 21/21 (3 new: window ball record, frame counter, pitch record) |
| `pes_involvement_test` | 6/6 (its ball is now a 250-sample track at rest) |
| `pes_match_info_test` | 2/2 |
| `pes_possession_test` | 24/24 (PES21_ROOT set; `PesFrameOf`'s test removed) |
| `pes_world_test` | 6/6 |
| `pes_clock_test` | 51/51 |
| `pes_period_end_test` | 11/11 |
| `pes_clock_match_test` | 2/2 |
| `pes_team_tick_test` | 26/26 |
| `pes_event_wiring_test` | 4/4 |
| `pes_tick_order_test` | 4/4 |
| `pes_movestep_wire_test` | 27 passed, **1 FAILED**: `MoveStepWire.TheKickoffTakerClosesToTheBall` (closest 0.80 m, expects < 0.65 m; 17.5 s, rerun alone: same). Not measured on the base `a769434c`, so whether it is pre-existing is OPEN. INF: it is a seeded kick-off approach and `GetPesFrame` now leads the old derived value by up to one frame (the old floor of the integer ms times 54 / 1000 gave 0 for the first tick), moving PES's timers; the fix is a seed search as in `pes_restart_delay_test`. |

Mutations (each applied, rebuilt, run on the single test, reverted; `git diff` clean after):

| Mutation | Caught by |
|---|---|
| M1 `Ball::SetTrackSample` writes (x, z, height) instead of (x, height, z) | first version of the window ball test **survived** (it read the window back through the same mapping); strengthened to read sample 0 raw against a known GF position; now `TheBallsTrackAndVelocityAreTheWindowsBallRecord` FAILS |
| M2 `Ball::PublishMomentum` writes z into the height slot | `TheBallsTrackAndVelocityAreTheWindowsBallRecord` FAILS |
| M3 `Match::Process` drops `AdvanceMatchFrame` | `TheFrameCounterIsTheMatchInfoWordAndCountsTicks` FAILS |
| M4 pitch F0 written as F1 | `ThePitchRecordHoldsTheFieldConstants` FAILS |
| angle bindings (16) | `EachWindowRecordBindingChangesTheAngle`: each flip changes the call's path (result or write log); the 17th, ball +0x84, did not and was dropped (§3 item 1) |

Not mutation-checked: the side attack angle/sign writer in `Referee::PublishPesStoppage` (no GF-side test reads it; only the angle oracle's window carries it, from the capture) and `Match`'s pitch write beyond F0.

Red first: the three new `pes_tick_a1_test` cases and the window angle test did not compile on `a769434c` (`BallRecord`, `MatchFrame`, `BindCall`, the `kBall*`/`kMi*` constants were undeclared). Gate not run (Main gates). Smoke not run. The switch-off smoke hash and other seeded tests may move with the frame counter (see `pes_movestep_wire_test` above); not measured.

## 5. Steps 4 and 5 (Clear wiring) and what blocks them

1. The danger flag imageA+0x31C1 is written by the unit builder `0x142119390` (`*(param_1+0x31C1) = FUN_140699960(...)`, then set to 1 when `param_3[5]*B... < F0*...` at its tail); `0x140699960` and its callees (`0x14069A500`, `0x140698340`, `0x142138220`, `0x142137F90`, `0x140698D10`, `0x14063DD20`, `0x142117C30`, `0x14069A3E0`, `0x140A4C930`, `0x140A4C820`, `0x140A4FEA0`, `0x140A8F890`...) are not translated. Decompiles are in `gf-EndClear-scratch/dec_danger.txt`.
2. The unit's own struct (+0 carrier record, +8 ctx, +0x28 imageA) and ctx/MA/E have no GF state (§3 item 6).
3. Danger = 1 has no real call in the captures (`carrier_clear_2109EB0` is danger 0 in all five).
Until 1-3 land, wiring kind 1 would feed the translated angle with a ctx GF cannot build, and deleting the stand-ins would drop the danger flag. Order of work: (a) re-seat `EventStore::newest_` into a window E region and write MA's ball copy / carrier id / lane tables at their GF sites (PesPressTick writes the MA ball copy today); (b) translate the danger writer's closure; (c) wire kind 1, mutation per binding, delete `elizacontroller.cpp:2774` and `:2951`.

## 6. Notes

- Source edits were made with the edit tool, except two `#include` lines added to the oracle test with `sed`, and the mutation runs (`sed` applied, rebuilt, `git checkout` reverted).
- Live PES reads: one read-only `dd` of `/proc/1467004/mem` (G, G+0x360, B), 4 reads over 3 s, no pause.
- Request for Main: a hook on `0x14063A490` (carrier-state builder) and a sampler on the Clear unit for danger = 1.
