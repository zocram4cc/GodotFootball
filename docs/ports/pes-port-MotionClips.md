# MotionClips: the locomotion inputs and the six clip-record readers translated and proven; PES's readers run in GF over a PES-layout motion table; the movestep cutover is NOT done (MotionClips, 03-11-26)

Branch `MotionClips` (worktree `gf-MotionClips`, scratch `gf-MotionClips-scratch`, PES-derived, never committed). Pushed heads, confirmed by `git ls-remote origin MotionClips`: `b5123907` (phase 1), `f56a25d9` (final). Image `pes_full.bin`. Tags: CONFIRMED = tool output this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Item | State |
|---|---|
| Phase 1: translate each root, prove bit-exact on its live2 captures, results set, regenerate, `--check` | **DONE, CONFIRMED.** 8/8 roots, 24 real calls, 0 mismatches (rax, xmm0, every written byte). `results_motion.json` + `closure_motion_<VA>.json`, `SETS` entry `("results_motion.json","motion")`, layer regenerated (4,841 functions: 3,727 recorded, 1,114 closure), **`gen_translated.py --check` rc 0, byte-identical.** |
| Phase 2: movestep stand-ins replaced by the translated readers | **PARTLY.** Built and tested: the PES-layout motion table GF files from `Mbinfo` and the four translated readers running on it inside GF (section 3). NOT done: the `MoveHost` cutover, `AbilityByte`, `EntryAimFlag`, TurnStop's stand-ins and the 0.85 divisor (sections 4-5 give the exact PES semantics read this session and the numbered work left). |

## 1. Phase 1: proofs (CONFIRMED; `frame_trace.py recorded` then `snapdiff.py --bounded`, scratch `tr.sh`, `sd.sh`; hook cfg `PES21/rendezvous/hooks.cfg`)

All captures are `live2/pid296049/` (No Mod launch), slim (replayed as they are; nothing unpacked, so nothing to pack again).

| Root | Hook | Captures | Own-block | Bounded closure |
|---|---|---|---|---|
| `0x1406E6660` ability byte | mot_ability_6E6660 | seq017 seq038 seq056 | 4/4 = 1.0 | 4 (`0x1406EAAF0` = the `ap+0xAB8` interface slot 0, `0x140A489F0`, `0x14124DAD0`) |
| `0x1406BA7D0` entry aim flag | mot_entry_aim_BA7D0 | seq025 seq1315 seq1393 | 21/23 = 0.913 | 10 (`0x1406EA810`, `0x1406EAAF0`, `0x140A3B320`, `0x140A3E190`, `0x140A489D0`, `0x140A60530`, `0x140A62E40`, `0x140A6CE00`, `0x141E532D0`) |
| `0x1406B10D0` clip length | mot_cliplen_6B10D0 | seq022 seq1352 seq4405 | 9/13 = 0.6923 | 2 (+`0x141E532D0`, the frame-rate getter) |
| `0x1406B11E0` tick length | mot_ticklen_6B11E0 | seq021 seq1357 seq4626 | 0.6923 | 2 |
| `0x1406AF620` blend | mot_blendrec_AF620 | seq014 seq015 seq023 | 8/13 = 0.6154 | 2 |
| `0x1406AF8E0` cancel | mot_cancelrec_AF8E0 | seq005 seq026 seq030 | 0.6154 | 2 |
| `0x1406B1D00` RotData end | mot_rotend_6B1D00 | seq031 seq035 seq039 | 0.6154 | 2 |
| `0x1406B1F00` RotData start | mot_rotoff_6B1F00 | seq044 seq1295 seq1311 | 0.6154 | 2 |

Coverage is recorded, never gated. The uncovered blocks of the readers are the record-absent arms (count byte <= index, null pointer, motion >= 0x1125 reading `table[1]`) and the `rate <= 0` / frame-rate `<= 0` arms. Main armed those arms' predicates in rvwatch50 (new launch `live2/pid1037286/`, cap 6 each, sampled every 2000th call, so they may arrive slowly); when they land, add them to `results_motion.json` (`sd.sh`-style rerun) and regenerate. No synthetic cases were used. Note the roots 0x1406B10D0 / 0x1406B11E0 read kind 4 (count at `+4`, list `+0x90`) and have no GF consumer yet.

`0x1406AF620`..`0x1406B1F00`'s old unproven pilot records in `results_core.json` ("corpus empty") are superseded by the `results_motion.json` records (layer files moved from `closure/` to the subsystem directories).

## 2. What the roots do (CONFIRMED by decompile this session)

- `FUN_1406E6660(ap, idx)`: `slot = (s8)ap[0x2458]`; calls `ap+0xAB8` interface slot 0 (result unused); `rec = FUN_140A489F0(G, slot)` = `*(G + (slot+0xF3)*0x28)` (slot < 0x16, else `*(G+0x25F8)`); returns the byte at `FUN_14124DAD0(rec) + idx` = `rec + 0x70 + idx`. GF already owns that block (`pesworldrecord.hpp` `kSlotAbilityOffset = 0x70`, `StoreLiveAbility`); what is missing is `P` (`ap`) with `+0x2458` and a bound interface vtable at `+0xAB8`.
- `FUN_1406BA7D0(ap)`: false unless the `+0xAB8` interface's vt+8 (action id) < 0x16 and `MI+0x116c == 1` (in play); then takes the recent ball event `FUN_140A3E190(*(vt0()+0xE0), 0)` (ring `G+0x1900/+0x1904`, 800-byte entries) and answers true when `(MI+0x1198 - event[4]) < int(tps*DAT_14259BE2C/DAT_14259A764 + 0.5)` and either the event's anime row differs from the interface's own (`FUN_140A62E40` / `FUN_140A489D0`) with `FUN_140A6CE00` (kind 9..14) true, or the event kind is 0xD or 0xA.
- Readers (all `(table, motion, rate[, idx])`, `v` the field below): `int(2v / rate * (tps/60) + 0.5)`, `rate <= 0 -> 1.0`, tps = `FUN_141E532D0` = cell `0x14351AE54` (54.0 on the install). Entry = `table[0] + motion*0x390` (`motion < 0x1125`, else `table[1]`); count byte `entry[kind]`, pointer list at the kind's offset, zero-word fallback record when the index is past the count or the pointer is null. `0x1406AF620`: kind 0, list `+0x18`, `u32[1] >> 10 & 0x3FF`. `0x1406AF8E0`: kind 5, list `+0xA0`, `u32[0] >> 13 & 0x3FF`. `0x1406B1D00`: kind 6, list `+0xC8`, `(u32[1] & 0x3FF) + 1`. `0x1406B1F00`: kind 6, `(s16)((u32[1] >> 10 & 0x3FF) - 0x205)`. `0x1406B10D0`: kind 4 (count `+4`, list `+0x90`), `((u32[0] >> 13 & 0xFF) + 2)`. `0x1406B11E0`: kind 4, `u32[0] >> 21 & 0xFF`.

## 3. Phase 2, done and tested: PES's readers inside GF

- `src/sim/onthepitch/pesclipreaders.{hpp,cpp}`: `PesClipReaders(const Mbinfo&)` files `Mbinfo`'s raw words into a PES-layout table (object, 0x1125 x 0x390 entries, default entry, record pool) held as one `PesMem::window` (no per-call copy), and `Read(reader, motion, rate, index, ticksPerSecond)` runs `pes_fn_1406AF620/AF8E0/B1F00/B1D00` on it (frame-rate cell `0x14351AE50` as a segment, zero-word fallback objects at `0x1436F2430 / 0x1436F2500 / 0x1436F23E8`, a stack segment).
- `src/pes/mbinfo.{hpp,cpp}`: `MotionRecord::blendWords / cancelWords / rotWords` (raw `u32[2]` per record, same caps and order as the decoded lists).
- `tests/pes_clip_readers_test.cpp` (registered in `tests/CMakeLists.txt`): 3 tests. Each expectation is a recompute from the decoded record of the same raw words (relation, no pinned value): synthetic words (blend, two cancels, two RotData; motions with records, without, 0x1124, 0x1125, 0x7FFF), a 7-record cancel list past its cap, and the whole install table (every 7th motion, all four readers, indices 0..3). Rates {1, 0.85, 2.5, 0, -1}, tps {54, 60}, indices 0..5 on the synthetic cases.
- Mutations (each applied, built, run, reverted): RotData list offset `0xC8 -> 0xD0` killed 2 of 3 (synthetic and install); frame-rate cell `+1` killed 3 of 3. Both reverted (`pes_clip_readers_test` 3/3 after the revert, head `f56a25d9`).
- Run: `pes_clip_readers_test` 3/3, `pes_mbinfo_test` 9/9, `pes_movestep_test` 114/114, `pes_movestep_run_test` 100/100, `pes_movestep_step_test` 46/46, `pes_movestep_turn_test` 55/55 (all rebuilt after the `mbinfo` change, install present). `GATE fast`: NOT RUN (Main gates).
- INF: the fallback records at `0x1436F2430 / 0x1436F2500 / 0x1436F23E8` are zero in PES. Mbinfo documents the readers' fallback as the zero-word record, and the test agrees, but the live `.data` bytes were not in any capture (slim, never read by the replayed arms). The captures Main armed (record-absent arms) settle it.
- `PesClipReaders` has no GF caller yet (see 5): it is the table and the proven bridge, not yet the cutover.

## 4. PES semantics found for the stand-ins (CONFIRMED, decompile of `0x1406CAB20` TurnStop and `0x1406BCD20` Decelerate, the bridge)

1. **The 0.85 rule is `P+0x2468 < DAT_1425AB120 (0.85) ? 1.0 : P+0x2468`**, in both functions (`cab20.c` lines 52, 93; `bcd20.c` line 46). GF's `movestep_run.cpp:1447` (Accelerate) already has it; `movestep_run.cpp:1774` (Decelerate) and `movestep_step.cpp:1436` (TurnStop) use `std::max(RecordF2468(), 0.85)`: wrong for any value under 0.85 (reads 0.85, PES reads 1.0). Invisible today only because `RecordF2468()` is the stand-in 1.0 (`pesmovestep.cpp:302`).
2. TurnStop part 1 (PES lines 51-80): `elapsed = vt+0x38 of the +0xAB8 interface` (GF `CurrentClipFrames()`), `start = 0x1406B1F00(motion, 1.0, 0)`, `end = 0x1406B1D00(motion, 1.0, 0)` (motion = vt+0x18); if the `+0x4D0` flag is clear and `start <= elapsed < (start + 2*end)/3`: `stamp = req[0x18]`, `ticks = int((end - elapsed)/divisor + 0.5)`, and when `|FUN_14043A0A0(span, stamp)| / ticks < DAT_14259BE30` it writes `req[0x3D] = stamp`, byte `req[0x3C] = 1`, `req[0x3E] = ticks`, byte `req[0x3F] = 0`. GF's `movestep_group_a.cpp:315-330` (`IdleTurn`, FUN_1406BFE70) already has this exact shape; TurnStop's block at `movestep_step.cpp:1432-1448` is the empty stand-in (`footFrames = cancelFrames = 0`, `(void)frames`).
3. TurnStop part 5 (PES lines 317-330): `blend = 0x1406AF620(motion, 1.0)`, `cancel0 = 0x1406AF8E0(motion, 1.0, 0)`; replay dwell = `blend / FUN_141E532D0()` (= blend / tps); otherwise `stopNow` when `req[9] > 1`, or `req[0x1F] < 3` (decompile text), or `(cancel0 + blend)/2 <= elapsed`. GF's `movestep_step.cpp:1634-1650`: (a) uses `cancelFrames` where PES has the blend for the replay dwell and `(one/tick) * x` where PES divides by tps (`ClockStep` = 1/tps, so GF multiplies by tps: wrong dimension); (b) compares `(foot + cancel)/2 <= int(divisor)` where PES compares against `elapsed`; (c) tests `kClassWord > 2` where the decompile says `req[0x1F] < 3` (OPEN: the asm compare of word 0x1F at `0x1406CB…` was not read; confirm before changing); (d) the foot reader `FUN_1406AFBC0(.., 9)` at `cab20.c` line 372 is the FootType count (GF `Motion().FootCount()`), not a cancel/foot-frame pair.

## 5. Left (numbered, in order)

1. `MoveHost::ClipTicks(ClipReader, motion, rate, index)` (new, pure virtual), implemented by the sim adapter through a `PesClipReaders` the Match builds from its `Mbinfo` (`match.cpp:239`, tps from `TicksPerSecond()`); the four test fakes (`pes_movestep_{test,run_test,step_test,turn_test}.cpp`) implement it from their `Motion()` record. Then delete `movestep::RecordTicks` (`movestep.hpp:935`) and `FrameTicks` (`movestep_turn.cpp:99`) and move every caller (`movestep_group_a.cpp:19,73,260,315,462,493,553`; `movestep_run.cpp:229` `ReadClipHits`; `movestep_turn.cpp:99-116,470,3608`) onto it (clean cutover, no second path).
2. TurnStop (section 4.2 and 4.3) rewritten on `ClipTicks` with PES's elapsed/stamp/ticks/lock words; the divisor rule of 4.1 fixed in `movestep_run.cpp:1774` and `movestep_step.cpp:1436`. Red tests first: a TurnStop fixture where `start <= elapsed < (start + 2 end)/3` and the turn per tick is under 45 degrees must write `req[0x3C..0x3F]` (fails on main: the block is empty), and a divisor test with `RecordF2468() = 0.5` expecting 1.0.
3. `AbilityByte`: `PesWorld::RunAbilityByte(slot, idx)` = `pes_fn_1406E6660` with a `P` that holds `+0x2458 = slot` and an `+0xAB8` object whose vtable slot 0 is the image's (`0x1406EAAF0` is the translated target); the image vtable must be bound like `kBpVtablesVa` (`pesimagetables`). The ability bytes are already in the world window. Delete the `50` stand-in at `movestep_turn.cpp:123`. Also `elizacontroller.cpp:1051` (the stat-to-byte conversion) needs checking against the world block.
4. `EntryAimFlag`: `pes_fn_1406BA7D0` needs the `P` interface (vt+8 action id, vt+0x20..), `MI+0x116c/0x1198` (in the world window) and the event ring `G+0x1900/0x1904` (`pesevents.hpp` holds the judge's events; the 800-byte stride and entry fields `+0/+4/+0x10/+0x14` must be mapped onto it). Delete `movestep_turn.cpp:120`.
5. Cliplen/ticklen (`0x1406B10D0/0x1406B11E0`, kind 4): no GF consumer; `Mbinfo` does not build kind 4 (count `+4`, list `+0x90`; the builder feeding it is not identified in this report: OPEN). Needed with the start-time formula `clipTicks - rand - elapsed` (`sim-motion-mapping` section 14, item 3).
6. When the rvwatch50 captures of the record-absent arms land: rerun `sd.sh` over all captures, refresh `results_motion.json` / `closure_motion_*.json`, regenerate, `--check`; settle the INF in section 3.
7. Cleanup: `<workdrive>/gf-MotionClips-scratch`, `build/`, worktree `<workdrive>/gf-MotionClips`.

## 6. Method notes

- `closure.py --spec-set motion --tag motion <8 roots>` made the 19 specs (`specs/motion`); the capture-bounded rows came from `closure.py --members-file <union of the 8 members.json> --no-specs`, then a spec pass for the two new members (`0x1406EA810`, `0x1406EAAF0`).
- `closure_motion_<VA>.json` per root; `results_motion.json` records are in the `results_dribble.json` format (`proof: snapdiff`, so the gate's recorded re-check skips them: `select_recorded.checked`).
- Hook request sent to Main (arms of the six readers); Main armed them in rvwatch50. Nothing else was asked of Main.
- Not run: `GATE fast`, any smoke, any Godot run. No GF behaviour changed on the production path (`PesClipReaders` and the raw-word lists are additive and unused outside the test).
