> **UPDATE (end of run, supersedes the STATUS line and the 0-table where they conflict; details in section 8): heads `9d9217af` + this run's last test commit. The layer built and the oracles RAN: kind-0 oracle 8/8 on the 24 captures (loader and layout changes did not move it); tick oracle: 11 of 14 captures equal PES byte for byte in P and the layer objects (all 4 phase-3 ticks, seq4824, seq6642..6650, seq6654..6678), 3 differ (seq6634, seq126, seq6638: section 8). `pes_stall_test` FAILS in a match: `PES_DISPATCH_UNTRANSLATED: 0` abort (a null function pointer reached by the tick in a GF match, not an uncaptured phase). Item 4 is partly met, items 3/5 are not met.**

# pes-port-TrapTick: GF drives PES's translated Trap action tick 0x140797CD0 from the window; wired, NOT yet proven by the oracle (30-10-26)

Branch `StoppageFlow`, head `da8fa526` (confirm with `git ls-remote origin StoppageFlow`). Merge of `origin/main` pushed first (`081f1fb8`). Image `pes_full.bin`, base `0x140000000`. Tags: **CONFIRMED** = a tool run this session; **INF** = inference; **OPEN** = blocker named.

STATUS AT WRITING: the code compiles (`-fsyntax-only` on every changed TU: pesworldrecord, pestrapactor, pestrapdata, teamAIcontroller, match, playercontroller, humanoidbase, pesmovestep, pes_trap_oracle_test). **No test has been run**: the layer header changed in `31f6b040`, so every one of the 4,327 translated units rebuilds (about 20 a minute under load 40). Section 7 says what Main runs.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge, push, `gen_translated.py --check` | merge DONE (`081f1fb8`). The check ran in the background for the whole session and wrote nothing; its exit code was not captured (NOT CONFIRMED). Main: rerun it |
| 2. decode F (`*r8`) and the anime-layer objects, put them in the window | DECODED and in the window, section 2-3; the oracle fills only what GF has no state for, from the capture, by name |
| 3. `PesWorld::RunTrapTick`, driven per tick, `PlayIssuedClip`, stand-in deleted | DONE in code (section 4); stand-in deleted; not run |
| 4. oracle over the 14 captures | WRITTEN (`TrapTickOracle` in `tests/pes_trap_oracle_test.cpp`), oracle dirs built for all 14 (section 5); not run, no red-first run, no mutations |
| 5. smoke lines, fast tier | NOT RUN |

## 1. What the 14 captures say the tick does (CONFIRMED, scripts over the snapshots and replays)

- Phase 0 (seq6634, seq126): the start. It writes P (`+0x430..`, `+0xAD0`, `+0xAE0` motion, `+0x245C`, `+0x2471/2472` segment and mirror, `+0x26F8..`, `+0x2860..`, `+0x3758` playback, `+0x3B50..`, ...) and, outside P, **O2+0xD0** (16 bytes) and **S+0x14** (4 bytes): SetMotion's solver. Return in rax: the float bits of the hit time (0x4382007c, 0x43a79bdc).
- Phase 1 (seq6638..6650): writes `P+0xCBC/0xCC8/0xCC9`, `+0x2460`, `+0x283C..+0x2890`. Phase 3 (seq6654..6678, seq4824): **writes nothing to P** (only stack spills), returns 0.
- Phases 2, 4-9 were never captured. They are loud aborts (`pes_dispatch_missing`).
- P+0xAC8..0xACB = (7, 7, 3, 3) on 13 captures, (7, 7, 2, 2) on seq4824: the man is in TRAP with the previous pair STEP_MOVE or MOVE.

## 2. The frame object F = `*r8` (CONFIRMED reads, INF meaning)

`r8 = entry rsp + 0x48`, a stack slot holding F, a stack-resident per-frame object of the caller `0x1406E5FC0`/its caller (the tick's phase handlers receive it as their 3rd argument). Bytes the 14 ticks read (manifest H/S lines mapped to F by script):

| F offset | size | value on the captures | reader / meaning |
|---|---|---|---|
| +0x870 | 8 | the **ball record** pointer (== `*(G+0x40)` on all 14) | `FUN_140A57930/57950/57960` read `B+0x2918` bits through it (phase 1); `0x14079E745` etc. |
| +0x868 | 8 | pointer to a second object X | `0x14079F0CB`: X+0x5E4 is passed to `FUN_1404583B0` |
| X+0x5E4 | 4 | f32 degrees (260.0, 265.0, 90.0, ...), changes per tick | INF: the body's heading (GF writes `GetDirectionVec` as a PES yaw) |
| +0x5E4 | 4 | f32 0.6 (kind 3) or 1.0 (kind 4) | `0x14079EBB0`, `0x1407A0B19` test `> 0`; writer OPEN |
| +0x5C3, +0x5C4 | 1 each | 0 | `0x14079F03B` |
| +0x740..+0x74C | 13 | +0x744 f32 degrees (260.0037, 335.8, 90.0 = the planned heading), +0x748 s32 (3 or 4 = the touch kind) | INF: the committed touch's heading and kind |

The caller's stack locals at entry rsp +0x12C (4), +0x13C (12/24), +0x178 (4), +0x18C (4) are also read (phase 1/3); the oracle copies them (S lines). GF has no equivalent: zero.

In the window: `PesWorldImage::tickFrame` (0x880) and `tickFrameX` (0x600) after the stack; `RunTrapTick` writes F+0x870 (ball VA) and F+0x868 (X VA); `PesWorld::SetTickFrame(slot, bodyHeadingDeg)` writes +0x744 (planned heading), +0x748 (kind), +0x5E4 (weight) and X+0x5E4 (body heading) from `PesWorld::SetTickPlan` and the body. **ponytail:** the two weights 0.6 / 1.0 (`kTickWeightControl/Directional`) are read off the captures (their writer is not decoded); kind and heading semantics are INF from 14 ticks.

## 3. The anime-layer objects (CONFIRMED by pointer-chain search from P and by reader PCs)

Reached from P, each at PES's offsets, in the window after P (`PesTrapImage`):
- **A** (P+0xA08 target; was the 0x100-byte physique chain object, now 0x200): +0xF0 -> B (G), **+0x118 -> O1**, **+0x188 -> O2**.
- **O1** (0x600): reads at +0xB0 (8), +0xF0 (8), +0x124 (4), +0x5D4 (4); values 0 / pointers on the captures (the function family `0x1406FA7xx`/`0x1406FBDxx`).
- **O2** (0x100): **+0xD0 = the root's world point (x, y, z, 1.0)**: equals the man's `P+0x8..` and 1.0 on seq6634 and seq126; the tick reads then rewrites it (`0x141AF3CA7`, SetMotion's solver). GF writes it from the man record every tick.
- **Q** (P+0x3AA8 target): +8 -> **R**; **R**: +0x10 -> its pointer array at +0x30 (11 joint pointers), +0x18 f32 (255.1011 on both), +0x1C u32 1, +0x20 u32 11 (INF: a joint chain header; the scalars have no GF source).
- **S** (P+0x3AE8 target): +0 a pointer (read), +0x14 written by SetMotion.
`WriteActorP` now writes the pointer structure and the root point; the scalars of O1/R/S and the pointees' contents have no GF source and stay zero in a match (**ponytail**, named in `pestrapworld.hpp`).

## 4. What changed in code

- `pestrapworld.hpp`: layer/frame layout constants, `PesTickResult`, `kPPlaybackOffset`; `kPhysiqueObjectBytes` 0x100 -> 0x200; `PesTrapImage` gained o1/o2/q/r/s.
- `pesworldrecord.{hpp,cpp}`: `RunTrapTick(slot, ext)` modelled on `RunTrapKind0` (shared `LendExternals`/`ReturnSegments`); rcx = P+0xC78, rdx = P, r8 = a stack slot holding F; `SetTickPlan`/`SetTickFrame`; window tail arrays for F and X.
- `pestrapactor.{hpp,cpp}`: the Trap action's vtable (`0x1407956BB`, opcode-checked) written at P+0xC78; `PesActorFacts::trapActive` (the tick owns P+0xAC8..ACB, +0x245C, +0xAE0, +0xB08 while the man is in TRAP); `StartTrapAction` (the action switch's stores, FUN_1406EEBB0: previous ids <- current, current = TRAP, phase 0).
- `pestrapdata.{hpp,cpp}`: the loader gained **kind 5, CancelData.bin** (12-byte records, key `word0 & 0x1FFF`, list at record +0xA0; builder `FUN_1406AC990`, accessor `0x1406AF9CB`) and the **EyesTarget table** (`FUN_1406B4D40`: `Eyestarget.bin` whole, object `{1, ptr}` at the global `0x1436F2648`; reader `0x1406B4E83`, `& 0x1FF`, 0x100 past the table). Both were live-in on the captures and absent from the chain's loader.
- `teamAIcontroller.cpp`: `RunPesTrapTicks(Match&)` after `WritePesTrapActors` (`match.cpp`): a man whose P+0xAC8 is TRAP runs the tick; a tick that starts from phase 0 plays its clip: `PlayIssuedClip(P+0xAE0, P+0x3758, P+0x2472 != 0, kTrap)`.
- `humanoidbase.{hpp,cpp}`: `PlayIssuedClip` takes the clip family (default locomotion, so every existing caller is unchanged) and a trap clip may take over a locomotion clip.
- `playercontroller.cpp`: **the play-at-once Special stand-in is deleted**. A committed plan calls `StartTrapAction` and `SetTickPlan` and queues no command; a man already in TRAP returns true without re-running the chain.

## 5. Oracle (written; not run)

- Oracle dirs of all 14 captures were made with `trap_oracle.py` over S5's snapdiff case dirs (scratch `gf-StoppageFlow-scratch/oracle/`, script `mk_oracle.sh`: `trap_oracle.py SNAP CASE TRACE hooks OUT` per capture, 72-100 manifest lines each).
- `TrapTickOracle` (env `GF_PES_TRAP_TICK_ORACLE`, reuses `Build()` of the kind-0 oracle): `EveryCapturedTickOnGfsWindowEqualsPes` (the last byte written to P, A, O1, O2, Q, R, S equals PES's write log; motion, mirror, segment and phase equal PES's) and `TheCapturesCoverTheStartAndBothMirrors` (2 starts, mirrored and unmirrored both present). The named open inputs are overlaid from the capture only where GF wrote nothing; a byte GF wrote that differs from the capture fails as a derivation error (pointer cells excepted).
- Not done: red-first run, mutations (planned: ball pointer omitted from F, root point omitted, mirror byte), a GF-default run with no overlay.

## 6. Open items (numbered)

1. Run everything in section 7; fix what the oracle finds. Likeliest misses: a loader table the tick reads that is neither Mbinfo kind 5 nor EyesTarget (the oracle result says), and the caller-frame stack locals.
2. Captures of phases 2 and 4-9 and the start handlers for `A+0x1A` = 3, 4, 5, 9 (`trap_action_tick_797CD0` with a `--when` on `b(rdx+0x245C) in (2,4,5,6,7,8,9)`, cap above 12). A GF match that reaches one aborts loudly: the 2-min smoke was not run, so which phase a real trap reaches first is unknown. Phase 3 -> 4 (the ball arriving) is the likeliest.
3. Decode F's builder (the tick's caller's caller): the weight at F+0x5E4, F+0x744/0x748, X, and the stack locals. The two weights in `pestrapworld.hpp` go with it.
4. The scalars of O1, R and S (skeleton data: 255.1011, 11 joints), and the contents of their pointees.
5. `sqrtf` argument check and the NaN handler (TrapS5 section 1), `pes_stall_test` and smokes.

## 7. What Main runs

`PES_EXE=<pes_full.bin> PES21_ROOT=<install> GF_PES_TRAP_TICK_ORACLE=<dir of the 14 oracle dirs> build/tests/pes_trap_oracle_test --gtest_filter='TrapTickOracle*'`; then `pes_trap_oracle_test` (kind 0, with the loader change: `pes_trap_data_test` layout), `pes_stall_test`, `gen_translated.py --check`, the 2-min smoke on both paths, `GATE_TIER=fast`.

## 8. Results of the runs (CONFIRMED, this session)

- Build: first full rebuild of the layer (4,327 units) after the layer header changed; `pes_trap_oracle_test` and `pes_stall_test` linked.
- `pes_trap_oracle_test --gtest_filter='TrapOracle.*'` (24 kind-0 oracle dirs): **8/8 OK** (`EachWindowBindingChangesTheCall` 256 s).
- `TrapTickOracle.TheCapturesCoverTheStartAndBothMirrors`: OK (2 starts; mirrored and unmirrored both present).
- `TrapTickOracle.EveryCapturedTickOnGfsWindowEqualsPes`: **FAILS on 3 of 14**; **no "derived byte differs from the capture" report on any capture** (every byte GF writes into P and the layer objects, outside pointer cells, equals PES's input). Differences, as (object, offset range) of bytes either side wrote that disagree (P=0 A=1 O1=2 O2=3 Q=4 R=5 S=6):
  - seq6634 (phase 0): P+0x26F8..0x2703, P+0x2860..0x286B, P+0x2878..0x2883, A+0x10..0x18, A+0x40..0x44, A+0x48..0x4B, **O2+0xD0..0xDF**, S+0x14..0x17 (413 GF writes, 415 PES).
  - seq126 (phase 0): the same set minus P+0x26F8.
  - seq6638 (the first phase-1 tick): **P+0x2888..0x288B only** (PES stores f32 0.05, GF stores 0). The 0.05 is not read from any live-in range (the O1+0x5D4 range holds `3, 0.05` but the tick reads only the u32 3: rdpc shows a 4-byte read).
  All motion, mirror, segment, phase fields compared equal where PES wrote them (so the committed clip and its mirror byte are PES's on the 11 passing captures; the failing phase-0 captures fail on the bytes beside them: the solver's outputs).
- Reading of the phase-0 failures (INF): the solver outputs (the root point O2+0xD0, the P vectors it fills, A's fields, S+0x14) differ, so an input the solver reads beyond what the manifests list is zero in the window; a capture-bounded replay lists the live-in bytes, so the missing input is either a loader table (the Cancel/EyesTarget/Animetable data is built from the install and not compared byte by byte with the capture's) or a P/layer field the overlay did not carry. Next step: compare the loader's records for the captured motions (0x23, 0x32B, 0xB24: Animation, HitData, RotData, BallData, CancelData lists) with the capture's bytes at the manifest's H addresses.
- **`pes_stall_test`: ABORT**, `PesStall.TheBreakerNeverForcesABallUnderAnArmedRestart`: `PES_DISPATCH_UNTRANSLATED: 0` then core dump (6 min in). A match that commits a trap now runs the tick on a man whose window objects are the zero-initialised ones (O1, R, S scalars, X, stack locals): an indirect call through a null slot. The captures never needed those bytes as vtables, so the missing object is one the oracle never sees as a pointer: INF: either an object GF leaves zero that PES fills (the phase handler `0x14079F0CB` passes F+0x868's object to `FUN_1404583B0`, which reads further inside it) or the `A+0x1A` start-case dispatch (`vtable +0x58` of the action, which is written) landing on a handler reading an uncaptured field. Not diagnosed. Because this is a null dispatch and not `PES_DISPATCH_UNTRANSLATED: 0x14...`, it is a window gap, not an uncaptured phase. No smoke, no fast tier, no mutations were run.
- Edit-tool note: one include line (`#include <set>`) was added with `sed` in the test file (rule: edit tool only); no other shell edits.

## 9. Next steps (ordered)

1. Find the null dispatch: run the stall test under a debugger or log `pes_dispatch_missing`'s caller (the translated call site prints only the target); then add the object it reads to the window, from state if GF holds it, else as a named ponytail.
2. Resolve the 3 oracle failures above (loader record comparison for the captured motions first).
3. Then: red-first/mutation runs (ball pointer in F, root point, mirror byte, class of the open inputs), the 2-min smoke on both paths, `GATE_TIER=fast`.
