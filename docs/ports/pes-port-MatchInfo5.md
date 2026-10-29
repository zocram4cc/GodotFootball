# Danger builder 0x142119390 and pressure builder 0x14069A500 proven on real calls; GF's danger flag now comes from the translated builder (MatchInfo5, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`), merged `origin/main` then `origin/NativeSeam` (970a8d47); head: see `git ls-remote origin EndClear` (last report line). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled (blocker named). Gate not run (Main gates).

## 0. Verdict

| Step | State |
|---|---|
| 1 merge `origin/main`, `origin/NativeSeam`, push | DONE. Layer conflicts (17 generated files, `translated.hpp`, `pes_dispatch_gen.hpp`) resolved by taking main's side and REGENERATING (`gen_translated.py`); `gen_translated.py` SETS merged (`results_stoppage.json` + `results_fixdemo.json`). |
| 2 builder `0x142119390` | **PROVEN 10/10 bit-exact** (snapdiff: every real call, rax/xmm0/every written byte, harness vs translated), own-block 0.6275 (32/51). Pressure builder `0x14069A500` 16/16 (0.9697), writer `0x140699960` 20/20 (0.8023; the 10 calls inside the builder captures included). One builder case ends with danger 1 (seq6161: the writer's own store; no tail-rule store in any of the ten). |
| 3 layer records | `0x14069A500` and `0x142119390` are `proof: snapdiff` records in `results_core.json` (counts in `results_danger.json`); the builder carries `closure_file: closure_danger.json` (9 indirect targets the replays reached: 0x14063CD60, 0x14063EA50, 0x14159F670, 0x14159F974, 0x1415A18F4, 0x1415A18FA, 0x1421399A0/C0/E0). Layer regenerated: 4342 functions (3693 recorded, 649 closure); `gen_translated.py --check` byte-identical (last line). |
| 4 GF wiring | **DONE for the danger flag.** `PesClearUnitBlock` replaced by the carrier block BP (`PesCarrierBlock`); `elizacontroller.cpp`'s own-third stand-in deleted; GF calls the translated builder (which calls `0x14069A500` then `0x140699960` at its tail, and stores imageA+0x31C1). Red-first oracle: GF's flag equals PES's on all ten captured calls. |
| 5 forced clear | **NOT DONE (OPEN 1).** `forcedClear` stays false. |

## 1. Git and layer

- `git merge` fails on the symlink (`extern/godot-cpp`): removed it around every git call, recreated for cmake.
- NativeSeam's regeneration is not kept: its recorded `0x141FEE390` and `0x1403D2E60` decoded past their ends (15,500 lines of `__builtin_trap`, 561 closure members); main's bounded decode (StoppageFlow) wins on regeneration. After the merge the layer has 4342 functions; three records are held out by `gen_translated` ("closure reaches a mistranslated callee": 0x1403BB670, 0x1405549C0, 0x14202BA30; none is in main's layer either).
- `tools/dev/diffharness/specs/core/1415A17CE.json` removed (it was a spec of the powf thunk, now a model; snapdiff refused "a callee is one or the other").

## 2. The builder on its captures (CONFIRMED)

- Captures: `pid1792161` seq16184, 16456; `pid1965411` seq033, 495; `pid1981120` seq4046, 4444, 4891, 5314, 5769, 6161. Writers (10) and pressure builders (6) as in MatchInfo4 §2.
- Harness: `frame_trace.py recorded SNAP --hooks hooks.cfg --out TRACE --spec-dir specs/core` (all 26 exit 0), then `snapdiff.py --only 0x140699960 0x14069A500 0x142119390 --snapshot ...`. The harness now has powf (`1415A17CE`, 72 calls in seq4046) and sqrtf.
- **Allocator in the harness:** PES's own allocator bytes run (`1403FB410`: 11 allocations and 11 frees per call in seq4046, all balanced; the dlmalloc and pool code `1403FBxxx` is harness-only). The translated side of snapdiff runs the same bytes translated (not the native heap), so both sides agree by construction; the native heap is exercised by GF (§3).
- **powf caveat (INF):** both sides use the host libm model (XlateLoud: ucrtbase differs by 1 ulp on 0.15% of inputs). The proof is harness == translated, not PES == translated on powf's last bit.
- Danger outputs per case (store log on imageA+0x31C1): nine `[0]`, seq6161 `[1]`.

## 3. GF wiring and the oracle (CONFIRMED, test runs)

- `PesCarrierBlock` per side: BP at PES offsets (`kBp*`: flags +0x18, unit info +0x1C, state +0x78, imageA +0xEC, to +0x32C0), then the Clear unit object (`+8` ctx, `+0x28` imageA, `+0` unit info) and its answer. `PesWorld::RunDangerBuilder(side)`: PES's call frame (rcx imageA, rdx ctx, r8 unit info, r9 state, 5th/6th stack args = side record and BP+0x18), allocates through `PesHeap` (PES's allocator on GF memory, one per world), returns imageA+0x31C1. `RunClearUnit` reads the carrier slot from BP+0x1C. `SetClearDanger` stays only as a test poke for the Clear unit's own test.
- Window growth (read-set measured on the captures): MI 0x1760 -> 0x1770; side record 0x260 -> 0x4C30 (`+0x6D4` the writer's mode word, `+0x4C00` the unit words).
- `SetCarrierInputs(side, slot, owned)` writes from the window's own records (CONFIRMED relations on ten captures): unit info slot, slot % 11, side, the side's attack heading (+0x250) and sign (+0x254), the man record's position (+0x55C) and that times the sign, state ball = MA ball copy, owned. ponytail: unit info +0xC (side 1 differs from slot % 11), state +0x38/+0x3C/+0x44/+0x48, BP+0x64, BP+0xE5/E6 are written by the unit updater `0x14092C9F0 -> 0x1421019D0 / 0x142117370` (undecoded); BP+0x18 = 0x1F in all ten captures.
- Tooling: `tools/dev/translate/danger_oracle.py` (manifest of the call's live-in set classified against the window's records incl. ctx, MA, E, BP), `tests/pes_oracle_snap.hpp` (the oracle loader, shared with `pes_trap_oracle_test.cpp`), `tests/pes_danger_oracle_test.cpp`. The live-in set is small (about 2.5 KB per call).
- **Red first:** with the builder call neutralised: 3/3 tests FAILED (flag, side-mode, zero-world pressure marker). Green with the call: `DangerOracle.EveryCapturedCallOnGfsWindowLeavesPesDangerFlag` (all ten flags equal PES's, the danger-1 case included, and the same run without any capture bytes outside the window, as production runs), `SideModeSixAnswersDangerOnEveryCapturedCall` (the writer's `+0x6D4 == 6` fact), `ZeroWorldRunsAndPressureRecordNamesTheCarrier` (`*P = slot % 11`, 0x14069A500), `CarrierInputsAreTheWindowsRecordsTimesTheAttackSign`: **4/4**.
- Mutations (run, rebuilt, reverted): M1 BP flags argument 0 -> 3 tests failed (flag, side-mode, zero-world); M2 signed position without the sign -> `CarrierInputs...` failed. The native-heap path is covered by `pes_native_heap_test` 9/9.

## 4. Open items

1. **Pressure lane records not equal on two captures.** `DISABLED_EveryCapturedCallLeavesPesPressureLaneRecords`: imageA+0x2124..0x31C0 equal PES's on 8 of 10; on seq4046 and seq4891 the (slot id, f32) entries of the sorted lane lists differ (467 bytes). The flag is equal on all ten and the same captures replay bit-exact on the snapshot's own memory, so a list input the window does not bind yet differs (the manifest's live-in set shows no other record). This is the input of the forced clear.
2. **Forced clear (MatchInfo4 §4) not ported:** its inputs (N1, cls, the guards) read the builder's lists (imageA+0xB6C), the pressure record (item 1) and BP+0x3380/0x4204; the walk `0x14210F110` (87 members) is not translated, so there is no oracle for a C++ port. `forcedClear` stays false. Path: fix item 1, translate the walk (`closure.py --tag walk`), diff the C++ against it on the captured walk calls.
3. Unit info / state fields of `SetCarrierInputs` that stand in (above).
4. The danger writer's `[0x31C0]` goal-mouth byte is computed by the builder but GF still hard-codes `goalMouth = false` in elizacontroller (`ponytail: imageA+0x31C0`); read `Carrier(side)[kBpImageAOffset + 0x31C0]`.
5. Cleanup: `gf-EndClear-scratch` (traces, snapdiff builds, oracle dirs) once archived.

## 5. Verification

Ran (sim slot 4, `GF_SIM_SLOTS=4`): `pes_danger_oracle_test` 4/4 (+1 disabled, fails as item 1), `pes_trap_oracle_test` skipped (no oracle env), `pes_tick_a1_test` 26/26, `pes_world_test` 6/6, `pes_match_info_test` 2/2, `pes_team_tick_test` 26/26, `pes_carrier_test` 189/189, `pes_native_heap_test` 9/9, `pes_events_test` 39/39. `gen_translated.py --check`: see the last line of the final message. Not run: the full gate (Main), the other `pes_*` binaries.
