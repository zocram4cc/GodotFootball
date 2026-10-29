# pes-port-TrapGF: the translated kind-0 trap chain runs on GF's window and equals PES on the captured calls; the in-match wiring is NOT done (29-10-26)

Branch `StoppageFlow` (`git ls-remote origin StoppageFlow` is the head after the final push). Tags: **CONFIRMED** = read in a tool run this session; **INF** = inference; **OPEN** = not settled (blocker named). `origin/main` and `origin/EndClear` merged (EndClear `92837342`, main `bbd5dc7c`).

## 0. Verdict against the acceptance

| Acceptance | State |
|---|---|
| GF's reaction trap runs PES's translated chain on GF memory | **PARTLY.** `pes::PesWorld::RunTrapKind0` runs `pes_fn_14079A470` on the world window with P, the ball, MI, slot, man and anime records as window state (CONFIRMED by the oracle test, §3). `_TrapCommand` does not call it. |
| `PesStall.DISABLED_...` re-enabled and green | **NOT DONE.** Still disabled. |
| `PickTrapClip` gone from the reaction path | **NOT DONE.** `playercontroller.cpp:510` still calls it; the G5 stand-in is untouched. |
| Oracle: a captured trap call rebuilt on GF state gives PES's output | **DONE** for 3 captures (seq019, seq4469 = the issued call, seq4531): `pes_trap_oracle_test` 2/2. |

Why the wiring is not done (§5): the chain's inputs include objects GF has no state for and no decoded loader: the constant registry (`0x1437F4808`), the anime-table manager (`0x1436F2378`, `0x1436F23C0`), the motion table's records (`0x1406B20E0`'s 0x1125 x 0x390 block) and P's own writers. Without them the chain refuses every call, so deleting `PickTrapClip` would remove the only clip source. I did not delete it.

## 1. Merge and two defects found on the way

- `git merge origin/main`, then `origin/EndClear`: clean. **Merge defect (CONFIRMED, build error):** main's `Match::RecomputePesDesignatedOwner` (`match.cpp:817`) read `pesEvents`, which EndClear deleted (the store is a view over the window). Fixed: it reads `GetPesEvents()`. Main will hit the same when it merges both.
- **Layer defect (CONFIRMED):** the first window run died with `PES_DISPATCH_UNTRANSLATED: 0x1406eaaf0`. That is vtable slot 0 of P's interface (`0x1425E3508`, P+0xAB8). `gen_translated.py` closes over direct callees only, so the vtable methods that snapdiff's replays reached were in the proof's build but not in the committed layer: the committed trap chain was not self-contained. Fix: `closure_trapchain.json` (the 347 functions snapdiff built for 0x14079A470) is referenced by the record (`closure_file` in `results_core.json`); `gen_translated.closure_members` seeds the layer closure with them. The layer is regenerated: 4,201 functions (3,686 recorded, 515 closure, +159 closure files); `gen_translated.py --check` result in §6.

## 2. The caller's stack block (step 2), decoded (CONFIRMED, `dec.sh`/bridge decompile of 0x1407A01C0 and 0x1409F8640)

The "caller-built stack block" is not an array the reaction code builds. The chain's live-in set on the captures is 1,734 bytes (§4); its caller-frame part is **the dispatcher's own frame**:
- Chain: `0x1409F8640` (`FUN_1409f8640(reaction obj, ..., plan param_4, float param_5)`, call site `0x1409F8792`) builds a 0x24-byte request `local_1d8` from its plan struct `param_4` and calls `FUN_1406B4580(anime = *(obj+0x40), 7, ball = *(G+0x40), &request, out[28])`. `0x1406B4580` forwards to vtable `+0x68` of the Trap action (`0x1407999F0` = `jmp 0x1407A01C0`) with rcx = action, rdx = P, r8 = ball record, r9 = request.
- Request words (the caller, CONFIRMED by decompile): +0 = plan[8], +4 = plan[0], +8 (kind) = plan[1], +0xC = param_5 (clamped to >= 0), +0x10..+0x18 = plan[10..12], +0x1C = 0, +0x1E/+0x1F/+0x20/+0x21/+0x22 = plan bytes (+0x34/+0x36/+0x37/+0x38/+0x3A). On a refusal it calls `FUN_141794410(anime, 7, out[0])`, and retries once with kind 0 when `out[0]` is 3 or 7, the kind was 4 and plan byte +0x35 is 0.
- Dispatcher `0x1407A01C0` (kind <= 0): stores request bytes +0x1D and +0x21 into the action at +0x3D/+0x3E, copies word 0 to its frame (r9 of the chain), and passes word 1, the out pointer, word 3, 0, bytes +0x1E, +0x1F, +0x20, +0x22, +0x1C as stack arguments 5..13 (entry rsp + 0x28 ... +0x68). The live-in confirms: stack bytes `S+0x28`..`S+0x68` and `S+0xA8` only.
- GF builds exactly this in `PesWorld::RunTrapKind0` (frame constants in `pestrapworld.hpp`). Not decoded: the plan struct's producer `0x1409F8A50` (GF's `pes::ChooseFirstTouch` is its port; its field-to-request mapping is not checked).

## 3. What landed

- `src/sim/onthepitch/pestrapworld.hpp` (new): P's layout as the chain reads it (offsets are live-in, names provisional where INF), the request, the frame, `PesTrapExternals`. `pesworldrecord.hpp`: `PesTrapImage trap` (P, 0x3B40 bytes) in the window between the Clear units and the scratch; **`kMatchInfoBytes` 0x15C0 -> 0x1760** (the chain reads MI+0x1748..0x175D, 21 flag bytes); `PesWorld::P()`, `PVa()`, `RunTrapKind0`. MatchInfo3 told before the edit.
- `tools/dev/translate/trap_oracle.py`: replays a capture under unicorn with a written-byte set and classifies the live-in bytes into window records (T), heap (H), image globals that differ from the file (I), pointers of P into other records (F) and the caller frame (S); writes the oracle directory (`GF_PES_TRAP_ORACLE`, PES-derived, never committed).
- `tests/pes_trap_oracle_test.cpp` (`pes_trap_oracle_test`): `EveryCapturedCallOnGfsWindowGivesPesOutput` rebuilds each capture in a `PesWorld` (P through its named fields, the other records at PES offsets, the request as the dispatcher took it) and compares the issued flag and the out word with PES's reference write log: **3/3 equal** (seq4469 issues, rc 1). `EachWindowBindingChangesTheCall` flips one bit of each of 17 window bindings (run in a forked child because PES's own code aborts on some values) and requires the call's path to change in some capture: **17/17**.
- Red first: before the layer fix the test aborted with `PES_DISPATCH_UNTRANSLATED`; before the window change it did not compile (`P()`, `RunTrapKind0` undeclared).
- Mutations (run, rebuilt, reverted): request word 1 and word 3 swapped in the frame (`RunTrapKind0`) -> `EveryCapturedCallOnGfsWindowGivesPesOutput` FAILS. The 17 bindings are each a mutation by construction. **Not claimed** (no remaining capture's call changes with them): P+0xB08, P+0x377C, MI+0x116C (play state), pitch F0, the anime class byte, ball+0x4, man B facing and +0x5B0. P+0x2458 cannot be flipped: any other value reaches a trap in `0x140A58550`.
- Captures: **seq4310, 4329, 4351, 4402 were removed from disk** while I worked (the oracle directories made from them have dangling links); the test skips a directory whose capture is gone. 3 captures remain (one issued). Two more kind-0 captures would be worth a hook sample on the next PES run.

## 4. The chain's true input set (CONFIRMED, seq4310 replayed before its removal; the other captures' manifests agree in kind)

Live-in = bytes read that the call had not written itself (the earlier "read set" of TrapChain2 counted the chain's own stack locals too, which is why it looked huge): 1,734 bytes in the heap/stack, plus image globals.
- **Window records (GF state, now in PES layout):** P (14 field ranges), ball record B (the sample table rows read as `x,y,z` and single heights; `+0x4`, `+0x74..0x93`, `+0x2944..0x2953`), MI (`+0x116C`, `+0x1198`, `+0x1748..0x175D`), pitch F0, anime class bytes and `+0x1A/+0x1C`, man pos/vel/`+0x554..` and `+0x5B0`, slot `+0x20, +0x88, +0x9B.., +0xB0, +0xC8..+0x124, +0x1438`, G's tables (all 22 anime/man/slot entries, side A/B entries).
- **Not GF state (stand-ins in the oracle, captured bytes at their VAs):** the constant registry `*(0x1437F4808)` (entry 0x1F: object fields `+0x128, +0x12C, +0x150, +0x18C..+0x1A0, +0x1CC`), the manager globals `0x1436F2378` / `0x1436F23C0`, four motion-table entries (0x390 stride in an arena at `0x7FF5DA8xxxxx`: per-kind count bytes then list pointers to raw 108-byte HitData records, the 12 floats at `+0x24..+0x54` match `HitRecord`'s `+0x24..+0x44`), P's pointer targets (`P+0xA08 -> +0xF0 -> f32 +0xC0`, `P+0x3AC0`), and the install's image (the vtables; `PesState::mem.flat`).
- Writers: P's fields have **no GF writer** and no decoded PES writer (the P constructor `0x1406D...` family is OPEN); the vtable pointers P+0xAB8/+0xAC0 are P's class constants.

## 5. Not done, numbered (each names its blocker)

1. **P writers.** P+0x8/+0x14/+0x20 (position, next point, heading), +0x2FC (velocity), +0x430, +0xA08 target, the interface bytes +0xAC8..+0xAE1, +0xB08, +0x2458, +0x377C, +0x3AC0/+0x3AD8, +0x3B18 need GF writers at their PES writers (INF: copies of the man record's pos/vel/facing, which GF writes already via `WriteManMovement`). Needs the P constructor/update decode.
2. **Motion table in PES layout** (`0x1406B20E0`: 0x1125 x 0x390 block; builders per file kind in `pes-re-gaps-anime.md`) and the **constant registry entry 0x1F** and the manager objects as window state, loaded from the install's bins at match start. GF's `pes::Mbinfo` parses the same bins; the window layout of an entry (count bytes, list pointers, raw records) is decoded only in part.
3. **Image access at run time:** `RunTrapKind0` needs the install's image for the vtables (`PesTrapExternals::image`); the Match has no `PeImage` (constants are read by the caller). Plumb a required image (or just the vtable runs) through `MatchSetup`.
4. **The request's producer** (`0x1409F8A50` plan struct -> request, §2) and the out array's consumer (`out[0]` = clip key; GF's command takes a clip by family and motion number).
5. `_TrapCommand` calls `RunTrapKind0`, `PickTrapClip` and its tests (`pes_receive_test.cpp:445-662`) are deleted, the G5 designation stand-in is removed, `PesStall.DISABLED_...` re-enabled: all blocked by 1-4.
6. Re-run `snapdiff.py` over more kind-0 captures; fold the 4 pid1792161 commit captures into `results_trap.json` (carried over from TrapChain3).

## 6. Verification run

- Gate not run (Main gates). Visual claims: none.
- Scratch: `<workdrive>/gf-TrapGF-scratch/` (live-in dumps `li_*.json`, `oracle/`, `peek.py`, `attr2.py`, logs). Remove once archived.
- `pes_trap_oracle_test` 2/2 (`GF_PES_TRAP_ORACLE` = 3 oracle dirs, `PES_EXE` set). `gen_translated.py --check`: 4,201 functions regenerate byte-identical (rc 0). Run in slots 2-3 (`GF_SIM_SLOTS=4`, `timeout 900`): 52 `pes_*` binaries PASSED with no failure (angle oracle 4, clock 51, dodge 45, events 39, movestep_{run 99, step 46, base 114, turn 55}, carrier 189, world 6, possession 24, and the rest in the final message); 4 SKIPPED tests are install-only. Then `pes_match_info_test` 2, `pes_involvement_test` 6, `pes_tick_a1_test` 25, `pes_receive_test` 49, `pes_scorerows_test` 12, `pes_period_end_test` 11, `pes_judge_test` 22 all PASSED. **Not run** (the first sweep hit its 3,500 s cap after `pes_holder_latch_test`): `pes_image_test`, `pes_keeper_test`, `pes_kick_error_test`, `pes_lane_test`, `pes_mark_table_test`, `pes_mentality_test`, `pes_mover_test`, `pes_movestep_wire_test` (EndClear2 §1: `TheKickoffTakerClosesToTheBall` fails there, not mine), `pes_stall_test` and the remaining sim binaries; `pes_stall_test` is unchanged (its test is still disabled).
