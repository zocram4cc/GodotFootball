# PES port AIRuns: seven AI roots (run writers, lane line, forward pull, Cross unit, receive entry) translated and proven bit-exact on their captures; GF's Cross unit stand-in replaced by PES's own 0x142129590 (03-11-26)

Branch `AIRuns` (head `2efd2fd4`, `git ls-remote` confirmed; worktree `<workdrive>/gf-AIRuns`, Release dir `build-rel`). Results set `tools/dev/translate/results_airuns.json`, closures `closure_airuns_<name>.json`, specs `tools/dev/diffharness/specs/airuns/`, set file `specgen/sets/closure-airuns.txt`. CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 1. Phase 1: the roots (CONFIRMED)

Captures `live2/pid296049/` (3 per hook, the PortMapAI hooks) and, for SecondLine, `live2/pid1037286/` (Main re-armed `ai_secondline_60D210` at `EVERY 20` with a valid-point predicate after my request; five captures landed). Each traced with `frame_trace.py recorded` (rc 0, 23/23) and replayed with `snapdiff.py --bounded`: PES's bytes in the harness vs the translated closure on the same snapshot, rax, xmm0 and every written byte compared.

| root | VA | captures | replayed / mismatches | own-block | bounded closure |
|---|---|---|---|---|---|
| ChanceSpaceRun | `0x140606060` | pid296049 seq027, seq11229, seq15389 | 3 / 0 | 0.087 | 1 |
| SecondLine | `0x14060D210` | pid1037286 seq946, seq4246, seq6204, seq9750, seq13263 | 5 / 0 | 0.575 | 26 |
| PostPlay | `0x14061F940` | pid296049 seq032, seq12200, seq6773 | 3 / 0 | 0.4706 | 33 |
| forward pull | `0x140682770` | seq007, seq1371, seq4710 | 3 / 0 | 0.4422 | 17 |
| lane line | `0x140681FF0` | seq006, seq1370, seq4709 | 3 / 0 | 0.8913 | 82 |
| Cross unit | `0x142129590` | seq036, seq17107, seq7090 | 3 / 0 | 0.3038 | 27 replay members + 113 from the static direct-call closure (translated, linked, not replay-entered) |
| receive entry 3 | `0x1409F8220` | seq050, seq1284, seq1302 | 3 / 0 | 0.1905 | 6 |

`gen_translated.py` writes 4888 functions (3726 recorded bit-exact, 1162 closure) and `--check` regenerates byte-identical (run after the last source change).

- **ChanceSpaceRun's coverage is 0.087 and says so.** All three PortMap captures took the head return (`float y < 0` of the man's run point at `param_1 + 0x60 + 12*(man % 11) + 4`; -100.0 is the "no point" value in all three), so only the root ran. Main armed `ai_chance_606060` with `--when 'd(rcx+0x64+(((r8&0xffffffff)%11 if (r8&0xffffffff)<0x16 else 0))*12)>>31==0'`, cap 9. OPEN: no valid-point capture had landed (live2/pid1037286 held none); the closure grows when one does (the `0x140609970`, `0x1406065B0`, `0x140A48A10` calls are not yet entered).
- **The Cross unit's extra 113 members** were added because the first window run aborted with `PES_DISPATCH_UNTRANSLATED 0x142125AB0` (the receiver picker, past the gates the three captures never passed). They are translated from the image like every closure member but were not replay-entered, so a branch that reaches them is covered by the translation only (the precedent is PressAI cc4b7eeb). A capture that reaches the picker would prove them: request below.

## 2. Phase 2: the Cross unit cutover (CONFIRMED: built, tests run)

Before: `elizacontroller.cpp` built a `pes::CrossInput` from GF values (facing standing in for the carry bearing, `owned = possession > 0`, the tallest mate) and `pes::CrossUnit` answered. Now `PesWorld::RunCrossUnit(side, slot)` runs PES's `0x142129590` on the carrier window (rcx = the kind-7 object at BP+0x3848, rdx = the 0x1C-byte answer record at BP+kCrossAnswerOffset pre-set to the driver's `00000000 19000000 0 0 0 0 ffffffff`, r8 = the unit object whose +0x20 now holds the state BP+0x78; offsets read from the three captures' registers), the walk's list takes the record for kind 7 on the rethink, and the pass target is the record's receiver slot mapped back to the man's GF id. Deleted (clean cutover): `pes::CrossInput`, `pes::CrossUnit`, the `cross` argument of `FillUnitTable`, `pes::lane::CrossLane`, the four `cross*` lane constants, and their stand-in tests (two `PesCarrierCross`, three `CrossLane` lane tests; the lane scan keeps a literal-lane test).

INF (decompile of `0x142129590`): an AI carrier (`K+0x6C == 0`) answers only while the volatile-ball byte `K+0x6E` is set, where GF's stand-in answered for any owned carrier. GF crosses by AI carriers therefore fall in number: PES's behaviour, not tuning. Not measured: no smoke was run (Main gates).

## 3. Tests and mutations (Release, `build-rel`)
- `pes_carrier_test` 171/171, `pes_lane_test` 22/22 (run directly, no match).
- `pes_tick_a1_test` (sim slot, filter `PesA1Sim.TheCross*`): `TheCrossUnitRunsOnTheWindowAndAnswersWithAMateOrNothing` (every slot of both sides: either the driver's initial record or type 2 / sub-kind 2 naming a mate of the same side) and `TheCrossUnitStaysSilentWhileItsStateGateIsClear` (24 x 50 ticks, never answers with K+0x71 clear; with it forced open the unit runs through the picker without leaving the layer). 2/2. Red on `origin/main` by construction (`RunCrossUnit` does not exist).
- Mutation M1 (`RunCrossUnit` drops the NoId prefill): killed by the first test (`pes_tick_a1_test.cpp:516`, rebuilt, run, reverted; `git diff -- src tests` empty afterwards). Not run: M2 (returning the Clear answer area instead of the Cross one, expected killed by the same assertion) and a mutation of the walk wiring.
- OPEN: **no window state made the unit answer** (gates forced open over 24 x 50 ticks: zero answers), so the "answers with a mate" arm of the first test is vacuous today and the elizacontroller call site (kind 7 wins the walk, record to pass command) is guarded by compile only. A capture whose entry reaches the receiver picker would settle both: request `ai_cross_unit_2129590` with `--when 'b(rcx+0x3d)==0 and ...'` (entry registers rcx = kind object, rdx = answer, r8 = unit array; K = `q(r8+0x20)`; K+0x71 != 0 and (K+0x6E != 0 or K+0x6C != 0)).
- Gate not run (Main gates).

## 4. Not done (numbered)
1. `ProcessRunManager` chance / secondline / postplay, `UpdateLaneLine` (`0x140681FF0`), Block A's forward pull (`0x140682770`) and `_ReactionCommands`' receive (`0x1409F8220`) still run GF's stand-ins. Each needs a window for its record family: the team record T (run state `T+0xB774`, the lane and rank words at ctx+0x58/+0x5C, planObj P at ctx+0xBA8) for the first four, the per-man think context and command record (`param_1 +4/+0x28/+0x38/+0x40`, `param_2 +0x874..`) for the receive. The translated functions are in the layer and proven; the wiring is the remaining work, one window per family.
2. A ChanceSpaceRun capture with a valid run point; a Cross-unit capture that reaches the picker (section 3).
3. M2 and the wiring mutation of the Cross cutover; a smoke on the merged tree.
