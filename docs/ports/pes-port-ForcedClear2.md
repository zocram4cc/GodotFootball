# Carrier walk 0x14210F110 now decides the carrier's unit in GF (forced clear included); window BP at PES offsets to 0x4300; walk equal to PES on 8 real + 2 synthetic calls on GF's window; the GF stand-in walk deleted (EndClear, ForcedClear2, 29/30-10-26)

Branch `EndClear`, head `f644e0eb` (confirmed by `git ls-remote origin EndClear`); first push of this session `3f3faadc`; merge of `origin/main` pushed first (`c3a97223`). Worktree `gf-EndClear`, scratch `gf-EndClear-scratch` (oracle dirs `wo/`, synthetic states `syn/`, `prove.sh`, `rconsts.py`; PES-derived, never committed). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled.

## 0. Verdict

| Step | State |
|---|---|
| 1 worktree, merge `origin/main`, push | DONE (`c3a97223`). |
| 2 BP to 0x4300 at PES offsets; GF's unit table into PES's list and answer-record layout; walk inputs from real GF state | DONE for the list, records and the inputs below; stand-ins named `ponytail:` (section 3). |
| 3 call the translated walk from GF; its decision replaces `WalkContext::forcedClear`; delete the stand-in | DONE and wider than asked: the walk's return decides the whole carrier arbitration. `WalkCarrierUnits`, `WalkContext`, `FirstTimeAccept/RestartAccept/SetPieceAccept/OwnerAccept/SafetyAccept/PassVeto/FindSlot`, `ForwardIsInstantReturn` and their 20 tests are deleted (clean cutover; nothing else used them). |
| oracle | `pes_walk_oracle_test`: the walk on GF's window equals PES on all 8 real walk calls and both synthetic Clear-arm states, with and without the capture's out-of-window bytes (section 2). |
| 4 smoke determinism | both paths deterministic; the pes-loop path moved as expected (section 5). |
| real danger-predicated `carrier_clear_2109EB0` captures | **none exist yet**: `live2/pid132854/` holds no `carrier_clear` capture (hooks present: judge, trap, cut_anchor, goal_pick, fixdemo, one `carrier_walk` seq5796 that is not a Clear-arm call). The real Clear-arm oracle stays OPEN 1. |

## 1. Layout (CONFIRMED by the oracle passing; sources decompile `0x14210F110`, `0x14211EF40/EF20/EF80`, `0x142119DF0`, `0x142119D20`, `0x14210F090`, `0x142119E20`, `0x14212A700` tail)

- BP `kBpBytes` `0x32C0 -> 0x4300`; MI `0x1770 -> 0x1780` (MI+0x1775); MA `0x24900 -> 0x26040` (MA+0x2601C); no `#deps 0` object appeared (build from clean tree).
- The priority list `BP+0x3E20`: u32 count, u32 kinds, signed rank bytes at `+0x68+kind` (0xFF = unlisted), 0x1C-byte answer records at `+0x84+0x1C*kind`. The walk's rdx is the window's unit object `{unit info VA, ctx VA}` (already laid out for the Clear unit).
- Answer record (u32 words): `+0` type, `+4` id (0x19 none), `+8` a (a pass's receiver LANE slot 0x14212A700 `param_2[2]`; a shot's sub-kind 0x14210F090; a dribble's sub 0x142119E20; the Clear unit's corridor angle f32), `+0xC` b (a pass's PassKind 0 short / 1 long / 2 centering / 3 through, equal to GF's `PassKind`), `+0x10` byte one-touch flag. `EncodeAnswerRecord` writes it; `FillUnitTable` now carries the lane slot (`CarrierAnswer::target`) so the table is filled BEFORE the lane-to-global conversion.
- Rank bytes: PES's own list can name a kind twice (seq5878 lists kind 0 at two positions, CONFIRMED by the round-trip test run); GF's `BuildUnitList` never does. `SetUnitList` writes rank = last position of a kind.
- Side record (the walk's skills read, `0x14063E960` -> `0x140A489D0`, `0x140A3AF90`, `0x141B59E20`): `+0` side id, `+4+4*k` slot-in-team -> info index, player info at `+0xA8+0x128*j`, u64 skill mask at `+0x78` of it (bits 0xB one-touch shot, 0xC one-touch pass, 0xE pinpoint cross); `+0x7C` the designated taker (`0x14210F110` kicker test). Height stat 0x31 (`0x14063D8A0`: slot record `+0x70+0x31` = `+0xA1`, the byte `kSlotByteA1Offset` already named; cm). Both written by `SetCarrierProfile`.
- Zone record at `BP+0xAB8` (`0x140699820` IsClose reads `+0x16C/+0x170/+0x178/+0x17C`; note the all-zero record reads CLOSE, so the window must write it): `SetCarrierZone`.
- Image bytes read through a register-held address: the tier table `0x142645A40` (0x22 rows x 0x20: `0x140A603C0`, the walk's capability gate `0x140A604D0`) is now bound by `LoadPesImageTables` beside the class table, the sine table and the frame cells (rconsts replay of the synthetic call: the only other register-held reads were the already-bound class table).

## 2. The oracle (CONFIRMED, run)

Tools: `tools/dev/translate/danger_oracle.py ... walk` (new mode: rcx = BP, ctx = `*(rdx+8)`, BP 0x4300, MA 0x27000, a `unit` kind for rdx's 16 bytes); `tests/pes_oracle_world.hpp` (the fixture both oracle tests share, moved out of `pes_danger_oracle_test.cpp`); `tests/pes_walk_oracle_test.cpp`. Directories: the 8 real walk calls (`pid1965411` seq035, seq627; `pid1981120` seq3914, 4518, 5130, 5878, 6294, 6695), and two SYNTHETIC states `syn_seq4046`, `syn_seq4444` (`synth_walk.py --patch-clear` on the danger-builder captures: PES's state with the kind-1 record's type word set to 5; labelled `syn_` in the test and here). `snapdiff` on the same ten (re-run): `calls=10 replayed=10 mismatches=0 own_block=0.4146 proven=True`.

`pes_walk_oracle_test` (4 tests, all PASS):
- `EveryCapturedWalkCallOnGfsWindowReturnsPesKind`: the window's walk return equals PES's `rax` on all ten, and again with none of the capture's out-of-window bytes (GF's production run). Real returns: 0x19 (no kind) on seven, 3 on seq5878; synthetic: 1 (PES keeps the clear) on both.
- `TheSyntheticClearArmKeysOnTheClearRecordsType`: zeroing the kind-1 record's type word moves the synthetic result off 1.
- `TheListGfWritesReproducesPesKindOnEveryCapture`: PES's list read back from each capture, rewritten with `SetUnitList` over a zeroed region: count, kinds, single-kind rank bytes and every listed record equal PES's bytes, and the walk still returns PES's kind.
- `CarrierWalk.ListedKindsThatAnswerNothingReturnNoKind` (zero world).
- Red-first: the test did not compile on `origin/main` (no `RunWalk`/`SetUnitList`); the first window run was red (both synthetic cases returned 0x19) until MA+0x2601C (the ball owner id `0x140A627E0(ctx,0)`, outside the old MA span) and the tier table were bound.
- `pes_carrier_test`: new `TheAnswerRecordCarriesTheFieldsTheWalkReads`; two table tests rewritten to assert the table (target lane slot) instead of the deleted walk.

Mutations (run, rebuilt, reverted; revert confirmed by diff): M1 `SetUnitList` rank byte always 0 -> `TheListGfWrites...` FAILED; M2 records written one slot up -> `TheListGfWrites...` FAILED. NOT run (budget): the unit-object rdx, `EncodeAnswerRecord`'s angle offset, `SetCarrierProfile`.

Not covered by any oracle (the real GF inputs, section 3): the oracle feeds PES's own input bytes; GF's writers for the stand-in fields are tested only by the smoke.

## 3. Inputs GF writes (elizacontroller.cpp, one block before `RunWalk`)

Real GF state: MA ball owner (`GetBallRetainer` slot, 0xFF none), BP+0x32B8 intention, the unit info and ball copy (`SetCarrierInputs`, already), the owned byte, the skill mask and height stat, the lists and pressure record the builder writes.
Stand-ins (each carries a `ponytail:` naming the PES site):
- MI+0x116C: PES's match FSM; GF writes `3 set piece / 1 in play / 2 stopped` into the period record's play-state word.
- MI+0x1775: OPEN, 0. Decoded this session: one immediate writer `0x140A3CCF0` (`mov [rcx+0x1775], dl`), called once, from `0x141FB1880` (at `0x141FB1903`, value 3; `0x141FB1880` is called from `0x141FD5200`), plus a copy `0x1404144DB/0x1404144E2` (value copied from another MI): 9 and 10 have no immediate writer. GF has no match-FSM state to map to it, so open play = 0.
- BP+0x60 = 500 (tier 5, the live setting, `pes-re-A4Carrier.md`); BP+0xC8, BP+0x32AD = 0; BP+0x41E4 = 0x19 (the previous decision; GF keeps none); BP+0x3380 = the carrier's facing bearing (writer undecoded); the zone record from GF's `IsClose`; `T+0x7C` = the carrier's slot.
- A kind-4/9/7 answer's receiver is whatever index GF's pass code holds (a global id, not a lane slot): only the set-piece pass gate and kind 6 read `+8`.
- A shot's aim vector and a dribble's direction (record `+0xC..`) stay zero.

## 4. Behaviour

GF's carrier decisions now come entirely from PES's walk, so rules the old stand-in encoded on GF's reading of the decompile (rule 3 first-time gate off, rule 5 restart vetoes, rule 7 forced clear never, rule 8 instant return, rule 10 owner) are PES's own. Visible effects in the 2-min pes-loop smoke: passes 70-28 (was 42-25), shots 2-4 (was 2-0), clearances 0-1; the `pes_movestep_wire_test` staging held (28/28, `[movestep-approach] first 4.38 closest 0.16`, unchanged). No test was re-staged or weakened.

## 5. Verification

- Binaries run (direct, no gate): `pes_walk_oracle_test` 4/4 (`GF_PES_WALK_ORACLE`, `PES21_ROOT` set), `pes_carrier_test` 171/171 plus the new layout test (189 before; 19 walk tests deleted), `pes_tick_a1_test` 26/26, `pes_world_test` 6/6, `pes_movestep_wire_test` 28/28, `pes_danger_oracle_test` 2 run / 3 skipped (no `GF_PES_DANGER_ORACLE` dirs this session; the shared fixture it now includes compiles and the zero-world tests pass).
- `gen_translated.py --check`: no layer file changed (see the final line).
- Smokes (2 min, seed 7, `--pes21-root`, each run twice, identical): switch-off `[determinism] 11c83698afc83d13` (unchanged from before this change: the carrier is not on that path), pes-loop `[determinism] b3a69571af761d23` (was `81a8586df698e5bf`; moved by the cutover). No crash, hang or zero passes/shots.
- Fast-tier interim line: `GATE fast (interim): GREEN: pes_walk_oracle_test 4/4, pes_carrier_test 171, pes_tick_a1_test 26, pes_world_test 6, pes_movestep_wire_test 28, pes_danger_oracle_test 2 (+3 skipped, no oracle env); smoke both paths deterministic; gen_translated.py --check: final line`. `GATE_TIER` was not run (`tools/dev/gate.sh` fast tier not exercised).

## 6. Open items

1. Real Clear-arm oracle: no danger-predicated `carrier_clear_2109EB0` capture exists (`live2/pid132854/` has none). When one lands: `synth_walk.py` without `--patch-clear` (source = that capture), then `danger_oracle.py ... walk`, and drop the dir in `GF_PES_WALK_ORACLE` (a name without `syn_` counts as real). Both synthetic Clear-arm states return 1 (the clear stands); no state yet shows the chain replacing a clear (3/4/9/6), so that branch of the Clear arm is covered only by the translation's snapdiff proof, not by a GF-window case with a type-5 record.
2. MI+0x1775 mapping from GF's match state (writer chain above).
3. The undecoded unit-updater fields (BP+0x60, +0xC8, +0x32AD, +0x41E4, +0x3380, zone builder `0x140698340/830`, taker `T+0x7C`).
4. Receiver lane slots for kinds 4/9/7 (GF's pass code holds global ids).
5. Mutations not run: rdx unit object, `EncodeAnswerRecord`, `SetCarrierProfile`.
6. Cleanup: `gf-EndClear-scratch` (oracle dirs, traces, `sdbuild`, `rc*`), worktree `gf-EndClear`.
