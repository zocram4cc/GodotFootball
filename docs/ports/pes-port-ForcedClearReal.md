# The forced clear's first REAL oracle: 12 danger-predicated carrier_clear captures replayed 12/12 bit-exact; run forward to the carrier walk, 6 of 12 reach the Clear block 0x14210F4F2 with PES's own (unpatched) kind-1 type 5; GF's window run equals PES on all 22 walk states; two window gaps named (ForcedClearReal, 30-10-26)

Branch `ForcedClearReal` (worktree `gf-ForcedClearReal`, scratch `gf-ForcedClearReal-scratch`: oracle dirs `wo/`, `uo/`, synthesized states `real/`, `syn/`, PES-derived, never committed). Pushed head `4563bd5a` (confirmed by `git ls-remote origin ForcedClearReal`). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Continues `pes-port-ForcedClear.md` section 3, `-ForcedClear2.md` Left open 1, `-ForcedClear4.md`.

## 0. Verdict

| Step | State |
|---|---|
| 1 worktree from origin/main (`40b218be`), godot-cpp symlink, push | DONE, pushed first at `40b218be` (ls-remote confirmed), then WIP `ad512200`. |
| 2 replay the 12 `carrier_clear_2109EB0` danger captures of `live2/pid483669/`; `snapdiff` on `0x142109EB0` | **12/12 bit-exact**, own-block **4/4** (the danger-1 arm: store type 5, call the angle, store the float, is now proven; EndClear3 had 2/4 on danger-0 only). Union with the two older captures still on disk (`pid407792` seq019, seq047): 14/14. |
| 2 `synth_walk.py` WITHOUT `--patch-clear` from each call's return to the walk entry | **12/12 reach the walk's entry**; every state holds PES's own kind-1 record, type 5 (no patch). **6 of 12 reach the Clear block `0x14210F4F2`**; the other 6 stand down at the last gate before it (section 2). |
| walk `snapdiff` on the 12 new states | 22/22 bit-exact with the 10 earlier calls (8 real, 2 synthetic): `calls=22 replayed=22 mismatches=0 own_block=0.5182` (185 of 357; was 0.4146 / 148). |
| 3 oracle dirs, GF's window run = PES | **`pes_walk_oracle_test` 8/8** on 22 dirs (8 earlier real + 12 new real + `syn_seq4046`, `syn_seq4444`), `GF_PES_UNIT_ORACLE` on the 12 new dirs green too. No red from GF's side: no GF writer needed a fix. One red from the TEST's side (section 3). |
| 4 (none would reach) | n/a: six do; the six that do not have a precise gate reason each (section 2), and Main gets the situation and a `--when` (section 5). |
| 5 mutations / smoke / fast tier | Two mutations run and killed (section 4). No GF behaviour code changed: no smoke needed. Fast-tier line in section 6. |

New finding (section 5): **GF's window never writes the dribble-proposal record `BP+0x4204` or the real `BP+0x3380`, the inputs of the very gate that stands the clear down.** With the record at zero (as GF leaves it) the window answers kind 1 on all six stand-down states where PES answers 6, 0x19 (x4) and 0xB. Not fixed: the writer `0x14211D710` is untranslated and has no spec.

## 1. The 12 captures and the forward run (CONFIRMED)

- Replay: `tools/dev/cutscene/replay_verdicts.py --hook 0x142109EB0` over `live2/pid483669` (`frame_trace.py recorded` then `snapdiff.py --hooks hooks.cfg --spec-dir specs/core`): `counts {"PROVEN": 12}`, each `1/1 calls bit-exact`; result `own_block 4 of 4`. Captures: seq8889, 11497, 13209, 14088, 15049, 17213, 18365, 20019, 20021, 20023, 20026, 20029.
- Forward run: `synth_walk.py CAPTURE OUT --hooks hooks.cfg` (no `--patch-clear`). All 12 wrote a walk-entry state. Each holds the same list shape (14 kinds: 23, 22, 0, 1, 17, 14, 11, 12, 2, 3, 10, 4, 7, 6; BP+0x18 = 0x1F) and kind 1's record is `type 5, id 1, angle word` written by PES's Clear unit (read from each state's memory by `walkinfo`-style dump: type word 5 on all 12). Kind 6's record is type 2 (a pass) on seq11497 and seq15049, type 0 elsewhere.
- Caveat (INF): the forward run's libm calls (the angle's atan2f etc.) use Python float32 models, not Wine's ucrtbase, so the Clear record's angle word can differ from a live run in the last ulp. The walk reads the record's type word only on the paths seen here, and calls no libm; the walk `snapdiff` and the oracle compare the translated walk with PES's bytes on the SAME state, so the proof is unaffected.
- Walk replay per state: `tools/dev/translate/walk_gates.py` (new; PES's bytes in unicorn, reads the gate inputs at their instructions). Output below is its output (angle = `0x140481B00(BP+0x3380, BP+0x4210)`; `P` = pressure record `0x14069A3E0(BP+0x2210, 0xFF)`).

## 2. Per capture: reaches the Clear block? (CONFIRMED, `walk_gates.py` + PC trace)

Gate chain for a listed kind 1 with type 5 (decompile `0x14210F110`, asm `0x14210F436..0x14210F4F2`; all 12 pass every gate before `0x14210F436`: kind != 0x16, play state, capability and stat tests, MI+0x1775):
1. `P+0x30 != 0 && P+0x34 > 1` (`0x14210F455..45F`, `JA 0x14210F4F2`) -> Clear block.
2. else, if `BP+0xE4 == 0`: event store's first kind (`0x140A3E190(ev, 0)`) in {1, 2, 3, 0x10} AND its side word != BP+0x24 (`0x14210F46D..49D`, `JNZ 0x14210F4F2`) -> Clear block.
3. else: `0x141052E30(BP+0x32B0) == 0`, or the dribble proposal `0x142119DF0(BP+0x4204)` invalid, or `BP+0x4204 != 3`, or `BP+0x420C <= 2` -> Clear block (`0x14210F4AB..4D1`).
4. else `0x140481B00(BP+0x3380, BP+0x4210) < 60.0` (`0x14210F4E8 COMISS XMM13,XMM0; JA 0x142110212`; 60.0 = `0x14259A764`, read from `pes_full.bin`) -> **stand down** (kind 1 skipped, loop continues). Otherwise -> Clear block.

| seq | P+0x30 / +0x34 | BP+0xE4 | BP+0x4204 / 420C | angle | Clear block | walk returns | why |
|---|---|---|---|---|---|---|---|
| 8889 | 1 / 3 | - | - | - | **yes** | 1 | gate 1 (pressure count 3) |
| 13209 | 0 / 2 | 0 | - | - | **yes** | 1 | gate 2 (event first kind in {1,2,3,0x10}, other side) |
| 14088 | 1 / 1 | 1 | 3 / 4 | 122.6 | **yes** | 1 | gate 4 passes (angle >= 60) |
| 15049 | 1 / 2 | - | - | - | **yes** | **6** | gate 1; the arm then runs the chain (below) |
| 17213 | 0 / 1 | 0 | 3 / 4 | 73.7 | **yes** | 1 | gate 4 passes (angle >= 60) |
| 18365 | 1 / 2 | - | - | - | **yes** | 1 | gate 1 |
| 11497 | 0 / 1 | 0 | 3 / 4 | 38.0 | no | 6 | stood down (angle < 60); kind 6 (type 2 pass) wins later in the list |
| 20019 | 1 / 1 | 0 | 3 / 4 | 33.0 | no | 0x19 | stood down |
| 20021 | 1 / 1 | 0 | 3 / 4 | 30.5 | no | 0x19 | stood down |
| 20023 | 1 / 1 | 0 | 3 / 4 | 28.0 | no | 0x19 | stood down |
| 20026 | 1 / 1 | 0 | 3 / 4 | 25.5 | no | 0x19 | stood down |
| 20029 | 1 / 1 | 0 | 3 / 4 | 24.1 | no | 0xB | stood down; kind 11 wins later |

- The five 2002x states are consecutive frames of one carrier (BP `0x3368C8B0` in all five): a dribbler under a pressing opponent with the proposal's heading inside 60 degrees of his facing, the angle shrinking 33.0 -> 24.1. PES keeps standing the clear down while he runs the line.
- seq15049 is the first real state in which the Clear arm hands the kind to the replacement chain: after the arm (`0x14210FA25..FAD1`), kind 4 answers nothing, kind 9 is unlisted, kind 6's record is valid (type 2), `BP+0xE4 == 0`, and `0x140A638C0(ctx, record+8)` is false, so it returns 6 (`JZ 0x14211024F; MOV EDI,6`). ForcedClear2's open item "no state shows the chain replacing a clear" is closed: this one does, on a real state, and the window reproduces it (section 3).
- The Clear arm is reached on 13209 by a different gate than the pressure-count one; all three gate families (1, 2, 4) are now covered by real states.

## 3. The oracle on GF's window (CONFIRMED, run)

Dirs: `danger_oracle.py SNAP CASE TRACE hooks OUT walk` (CASE = snapdiff's `gen/14210F110/snap_<label>`), 12 new dirs named `pid483669_seqN` (real: no `syn_` prefix) plus the 10 earlier ones regenerated from their captures (`pid1965411` seq035, seq627; `pid1981120` seq3914, 4518, 5130, 5878, 6294, 6695 slimmed but replayable; `syn_seq4046`, `syn_seq4444` regenerated with `synth_walk.py --patch-clear`, still labelled synthetic). `pes_walk_oracle_test` with `GF_PES_WALK_ORACLE` = the 22 and `GF_PES_UNIT_ORACLE` = the 12 new:

- `EveryCapturedWalkCallOnGfsWindowReturnsPesKind`: window run and production run (no out-of-window bytes) equal PES's `rax` on all 22. New assertions: among the real states whose kind-1 record is PES's type 5, PES keeps the clear on some (1) and does not on others (6, 0x19, 0xB); real set still holds 0x19 and chain kind 3.
- `TheClearArmKeysOnTheClearRecordsType` (was `TheSyntheticClearArmKeysOnTheClearRecordsType`): for every state with a type-5 record where PES returns 1 (6 real + 2 synthetic), zeroing the record's type word moves the window off 1. The test now asserts `syntheticChecked > 0` AND `realChecked > 0`; the `syn_` rows stay labelled as the synthetic ones, the real rows are the proof of the arm.
- `TheListGfWritesReproducesPesKindOnEveryCapture`: **red on the first run** (seq15049: window 0x19, PES 6). Root cause was the TEST: it read PES's list back from the WINDOW, which holds only the bytes the walk read, and the Clear arm's chain reads kind 6's record without ever reading kind 6's list position, so the window's position 13 was 0 and `SetUnitList` dropped the record. Fixed by reading the list from the capture's own memory (`Read(o, bp + 0x3E20 + at)`); rank bytes of single-listed kinds are now asserted exactly (the `|| captured == 0` tolerance is gone). `SetUnitList` itself was right; green after the test fix.
- `UnitOracle.UnitInfoAndStateFromTheWindowEqualPesOnEveryCapture` and `...DangerBuilderOnTheWindowWritesTheWalksDangerByteAndZone`: PASS on the 12 new states (these are complete states, so they serve `GF_PES_UNIT_ORACLE`): GF's unit-info/state writers and danger builder equal PES on 12 danger-set carriers (unit info, state, danger byte, zone, BP+0x1C..0xEC every byte except the two documented counters).
- Walk `snapdiff` (the translation itself): 22/22, `results_walk.json` and `results_core.json` (record `0x14210F110`) updated; `results_endclear_clear.json` replaced by the 14-call run.

## 4. Mutations (run, rebuilt, reverted; revert confirmed by the edit tag)

No writer was changed, so none was owed; two on `PesWorld::SetUnitList` (the writer these states exercise) were run:
| # | Mutation | Killed by |
|---|---|---|
| 1 | rank byte written as `position + 1` | `TheListGfWritesReproducesPesKindOnEveryCapture` (280 rank EXPECT failures) |
| 2 | record written one kind slot up | `TheListGfWritesReproducesPesKindOnEveryCapture` (new real `pid483669_seq11497` returns 7, PES 6, among others) and `CarrierProfile.TheWalkReadsTheTaker...` |

## 5. What GF's window still lacks; the situation and a `--when` for Main

1. **Gap (CONFIRMED by a temporary probe, removed): the stand-down gate's two inputs.** `BP+0x4204` is the dribble proposal record, written every unit step by `0x14211D710` (`0x142110D30` decompile: `FUN_14211d710(BP+0x32C0, ctx, BP+0x1C, BP+0x78, imageA, BP+0x32B0, ..., BP+0x4204)`; it is not the previous decision: `BP+0x41E8..` holds the adopted answer's copy). `BP+0x3380` is written inside its sub-builders `0x14211E2D0/E210/E650` (ForcedClear4 4.1; GF stands in the carrier's facing, `elizacontroller.cpp:3352`). GF writes no `BP+0x4204` (grep of `src/sim`: no writer; INF that the window's block stays zero there). Probe: `Build(...)` of each state, `memset(BP+0x4204, 0, 0x20)`, `RunWalk`: PES vs GF-without-the-record: seq11497 6 vs 1, seq20019/21/23/26 0x19 vs 1, seq20029 0xB vs 1; the other 16 states (8 earlier real, 6 new real that reach the block, 2 synthetic) are unchanged. **So in a real GF match a dribbling carrier whose heading is within 60 degrees of his facing is cleared where PES lets him run on (6 of the 12 real danger states).** Not fixed here: `0x14211D710` is untranslated and `snapdiff` says "no spec" for it (tried: `replay_verdicts.py --hook 0x14211D710` on a state written with the new `synth_walk.py --entry 0x14211D710 --hook-name dribble_prop_21D710` from `pid1981120/seq4046`: the state was written, `NOT VERIFIED no spec`, `calls=0`). A spec for `0x14211D710` (outputs: BP+0x4204..0x4224 and BP+0x3380) and its translation are the next step; real call states for it come from any FULL `danger_builder_2119390` capture (its return precedes `0x14211D710` in the unit step), no new hook needed.
2. **A direct walk hook is not needed.** If Main wants live (not forward-emulated, real-libm) walk states with a Clear record: `--when 'carrier_walk_210F110=d(rcx+0x3EC0)==5'` (kind-1 record at `BP+0x3E20+0x84+0x1C = BP+0x3EC0`, type word 5; `d()` as in the `judge_commit` predicate; INF that the expression syntax accepts `rcx`). The pressure gate cannot be a predicate (P comes from a function call), so filter at replay with `walk_gates.py`.
3. The situation that reaches the Clear block, in English: a defender carrying the ball in his own third with the danger byte set (builder `0x142119390`) and an opponent pressing him, on a refresh frame, AND (a) the pressure record counts at least two pressers (3 of 6), or (b) the last event is a kind 1/2/3/0x10 event by the other side (1 of 6), or (c) his dribble proposal heads at least 60 degrees from his facing (2 of 6). Otherwise PES lets him dribble on.

## 6. Verification

- Replays: `replay_verdicts.py` for `0x142109EB0` (12/12 and 14/14), `0x14210F110` (12/12, 10/10, 22/22 union). `walk_gates.py` on the 12 (output in section 2).
- Test binaries run (direct, `GF_PES_WALK_ORACLE` = 22 dirs, `GF_PES_UNIT_ORACLE` = the 12 new, `PES21_ROOT` set): `pes_walk_oracle_test` **8/8**. Other `pes_*` binaries: none links a changed file (only `tests/pes_walk_oracle_test.cpp`, tools and results json changed; `pesworldrecord.cpp` was mutated and reverted, its tag `ED76` is unchanged).
- Build: first full build of the worktree, `pestranslated` 4,543 objects at load 25-37: 35 min.
- `gen_translated.py --image pes_full.bin --check` (run after the results json edits, on `4563bd5a`): `4346 functions (3694 recorded bit-exact, 652 closure) regenerate byte-identical`, rc 0.
- Smoke: not run (no GF engine code changed; Main gates).
- Fast-tier line: `GATE fast (interim): GREEN: pes_walk_oracle_test 8/8 on 22 walk dirs + 12 unit dirs; no pes_* binary links a changed file; gen_translated --check identical (4346 functions). GATE_TIER not run (Main gates).`

## 7. Open items

1. `0x14211D710` (and `0x14211E2D0/E210/E650`): spec + translation + window writer, so GF's Clear arm stands down like PES's (section 5.1). Next highest value: it changes which carriers clear in a real match.
2. The Clear arm's other inputs GF does not write from state: `BP+0x32B0` intention path, MI+0x1775, BP+0x41E4 (unchanged from ForcedClear4).
3. The walk's remaining 172 blocks (own-block 0.5182): the set-piece branch (play state 3), `0x14210FB27..FD3A` region cases.
4. The three older Clear captures (pid407792 seq076, 095, 116) of EndClear3 are gone from disk; their 5/5 danger-0 proof stays in git history.
5. Cleanup: `gf-ForcedClearReal-scratch` (`real/`, `syn/`, `wo/`, `uo/`, `rv*/`, `walkroot*/`), worktree `gf-ForcedClearReal`.
