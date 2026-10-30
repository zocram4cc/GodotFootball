# Carrier walk 0x14210F110: Clear block unreached by any live call; synthetic Clear-arm states prove the translated walk 10/10; forced clear NOT ported (named blocker); movestep test green on the merged tree (ForcedClear, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`, push worktree `gf-EndClear-push`), head `f3a3d75e` (confirmed by `git ls-remote origin EndClear`). Scratch merge branch `EndClear-merge` (EndClear + `origin/StoppageFlow`, merge `4202a1ef`, NOT pushed). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = blocker named.

## 0. Verdict

| Step | State |
|---|---|
| 1 worktree, merge `origin/main`, push | DONE (`7e3a44ee`). |
| 2 which walk captures reach the Clear block `0x14210F4F2` | **NONE of the 27 I replayed does** (8 proven in MatchInfo6 + the brief's 14 + 5 new in `pid132854`; MatchInfo6 had already found 3 more of `pid2580963` that miss). Main deleted the 14 and the 5. Entry conditions decoded (§2). |
| 3 forced clear from PES code, delete `forcedClear=false` | **NOT DONE: named blocker** (§4). No LIVE oracle exists; a synthetic one does (§3) and proves the translated walk, but GF cannot call the walk yet. `WalkContext::forcedClear` stays. |
| 4 movestep test on EndClear + StoppageFlow | **NOT REPRODUCED: green, 28/28** (§5). |
| 5 `gen_translated.py --check`, fast tier, smokes | §6. |

## 1. Replay of every carrier_walk_210F110 capture (CONFIRMED, unicorn PC trace of the recorded call, `wpc.py`-style, scratch)

- Checked PCs: `0x14210F4F2` (Clear block), `0x14210023A` (replacement chain), `0x142110267` (return). All 14 captures of the brief (`pid2580963` seq1997, 3197; `pid2621613` seq065, 846, 1367, 1904, 2394, 2972; `pid2670420` seq040, 1683, 2096, 6782, 7145, 7701) return at `0x142110267` with `eax = 0x19` (no kind) after 191-587 instructions and never touch `0x14210F4F2`. The new full captures of `pid132854` (seq040, 1053, 2258, 3313, 4770) likewise (seq1053 returns kind 3, same shape as the proven seq5878). `pid91171` held no walk capture (EVERY 2000 sampling).
- (A note on method: `0x14210F226` is a constant load in the prologue, not the skip gate; an earlier probe marker on it is meaningless.)
- Why (CONFIRMED by dumping each call's list at BP+0x3E20, record layout below): in all of them every listed kind's answer record has type 0 (kind 1's too); only seq5878 / seq1053 have one valid record (kind 3, type 2).

## 2. What produces the Clear block (CONFIRMED decompile `0x14210F110` and `0x142110370`/`0x1421105B0`/`0x142110D30`, Ghidra bridge; INF where marked)

Walk argument: rcx = BP (carrier block), rdx = `{unit info BP+0x1C, ctx}`. List at BP+0x3E20: u32 count, u32 kinds, rank bytes at `+0x68+kind`, answer records at `+0x84+0x1C*kind` (u32 type first). The Clear arm (`LAB_14210F4F2`) runs only for a listed kind whose record has **type 5**, in open play (MI+0x116C != 3), after the gates at the top of the loop (kind != 0x16; `BP+0xE4 != 0` or `0x140A604D0(BP+0x60, 0x14)` or the stat-0x31 test; MI+0x1775 not in {3, 9, 10} for type 5; MI+0x116C != 2).

Type 5 is written by exactly one unit: kind 1 (the Clear unit, `0x142109EB0`, EndClear.md §1: `cmp byte [[r8+0x28]+0x31C1], 0; je`), so **the game situation is: the carrier's danger byte imageA+0x31C1 is set AND the kind-1 unit is evaluated and enabled that frame.**
- Danger byte: written only by the builder `0x142119390` (tail rule `0x142119714-0x142119770`: attack-signed x inside the own defensive third, press count `P+0x30 != 0`, no list entry blocking). In English: a ball carrier in his own third with an opponent pressing him. In the 12 builder captures of the oracle set one has danger 1 (seq6161).
- Evaluation: `0x142110370` walks the list; kind 1 sits at odd list position 3, and the loop evaluates kinds by position parity against the refresh bits BP+0x18 (bit 3 / bit 4; `0x142117350`, state machine `0x142117370`), INF that kind 1 refreshes on frames with bit 4 and keeps its old record otherwise. A kind's unit then runs only when `0x142111610` enables it (unit state +0x38 == 1), for kind < 2 gated by BP+0xE4/BP+0xE6, the events' first kind in {1,2,3,0x10} with the carrier, and the ripeness delay BP+0xB4 against `0x141E532D0`. In seq6161 the forward run (below) stops at `0x140A6E320` of the unit's anime class (the carrier is in a skip state), i.e. even the one danger-1 builder capture is not a walk call.
- The walk is called from `0x142110D30` only if the unit's class gates pass, BP+0x18 & 0x10, and `0x142117350(BP+0x14, 3 or 4)` (the refresh frames). Net: Clear is rare in a testbot match (defender carrier, own third, pressed, on a refresh frame), which is why 33 samples missed it.

## 3. Oracle: synthetic Clear-arm states, translated walk 10/10 (CONFIRMED, run)

`tools/dev/translate/synth_walk.py` (new): takes a FULL capture of a real call inside the unit step before `0x142110370` (a `danger_builder_2119390` or `carrier_clear_2109EB0` capture), runs PES's own bytes from that call's return until the walk's entry (import models for libm/lock/atexit; Python float32 libm, not Wine's), and writes the state as a rendezvous capture. With `--patch-clear` the kind-1 record's type is set to 5 (the Clear unit's output); that is the only patched word, and those two states are SYNTHETIC.
- From `pid1981120` seq4046 and seq4444 (danger_builder; reached the walk in 7 of 20 oracle captures; the others hit holes or skip states): replay reaches `0x14210F4F2`.
- `frame_trace.py recorded` + `snapdiff.py --spec-dir specs/core --only 0x14210F110` over 8 real + 2 synthetic calls: `calls=10 replayed=10 mismatches=0 own_block=0.4146 proven=True` (was 8, 0.1092; reached 148 of 357 blocks). `results_walk.json`, `results_core.json` updated; no source or layer change (the translation is unchanged).
- The synthetic dirs live in `gf-EndClear-scratch/syn/` (PES-derived, never committed); regenerate with the tool.
- Not reached even now: the chain's inner returns and the replacement path past the Clear arm (209 blocks); coverage is recorded, not a gate.

## 4. Forced clear in GF: NOT DONE (OPEN 1)

Blocker, exactly: GF cannot call the translated walk because the window has no PES-layout answer table. Needed, in this order:
1. BP block to `0x4300` (`kBpBytes` is `0x32C0`, `pesworldrecord.hpp:310`; the walk reads lists at BP+0x3E20, answer records `0x84+0x1C*kind`, BP+0x41E4/0x4204/0x420C/0x4210, BP+0x32B0/0x32AD/0x3380/0xAB8; a layout change needs the `#deps 0` object cleanup).
2. `PesWorld::RunWalk(side, table)` writing GF's `FillUnitTable` result into that table (count, kinds, ranks, records), the `{BP+0x1C, ctx}` argument, and MI+0x116C/0x1775, MA+0x7C, BP+0xE4/0x60/0xBC/0xC8 as real state (each is a `// ponytail:` stand-in in `WalkContext` today).
3. Decision shape: the walk's return replaces `WalkCarrierUnits` entirely (clean cutover, the ~20 `pes_carrier_test` walk tests become oracle tests), or a probe that runs it with only kinds {1,3,4,9,6} listed and reads "returned a chain kind" as `!forcedClear`.
4. Oracle for the red-first test: the 2 synthetic states (§3) plus real ones from `carrier_clear_2109EB0` captures with the danger predicate (Main, `live2/pid132854/`, full, not slimmed; none had arrived when I stopped): forward-run with `synth_walk.py` (no `--patch-clear`) gives an unpatched real state whose walk result is the expected value.
I did not start step 2: with a build of 4,296 objects under load 45 it did not fit the budget, and writing it unbuilt would be unverified.

## 5. The red movestep test: not reproduced (CONFIRMED, run)

Tree `EndClear-merge`: `origin/main` + EndClear (`f3a3d75e`'s content) + `origin/StoppageFlow` (merge `4202a1ef`; two conflicts, both resolved by keeping both sides: `tests/CMakeLists.txt` (both new tests) and `tests/pes_trap_oracle_test.cpp` (took main's shared `pes_oracle_snap.hpp` with StoppageFlow's extra includes)). `pes_movestep_wire_test`: **28/28 PASSED**, `[movestep-approach] first 4.38 closest 0.16`, seed 1003 as staged. Same numbers as EndClear alone. So MatchInfo6's suspicion (the kickoff taker's carrier decision on the exact builder) is not borne out; Main confirms the red came from StoppageFlow `255df4ec` before later merges. No re-staging done, 0.65 m untouched.

## 6. Verification

- Built (merged scratch tree, `nice -j5`): `pes_movestep_wire_test`, `gfsim_run`. No C++/layer file changed on EndClear in this session (tools + results json only), so no `pes_*` binary links a changed file; ran `pes_movestep_wire_test` 28/28.
- `gen_translated.py --check`: **GREEN on both** the merged tree and EndClear alone (`f3a3d75e`): 4346 functions (3694 recorded bit-exact, 652 closure) regenerate byte-identical, rc 0 (the merged run printed the held-out closure notes only).
- Smokes (2 min, seed 7; merged tree, each run twice, identical): switch-off `[determinism] 11c83698afc83d13`, pes-loop `[determinism] 81a8586df698e5bf` (same hashes as MatchInfo6, as expected: no behaviour change); passes/shots non-zero (switch-off passes 13-10, shots 0-6; pes-loop passes 42-25, shots 2-0).
- Fast-tier interim line: `GATE fast (interim): GREEN: no pes_* binary links a changed file (tools + results json only); pes_movestep_wire_test 28/28; gen_translated.py --check GREEN`.

## 7. Open items

1. Forced clear (§4); `WalkContext::forcedClear` (`pescarrier.hpp:898`) stays.
2. Real unpatched oracle: wait for `carrier_clear_2109EB0` danger-gated captures (`live2/pid132854/`), forward-run, add to the walk snapdiff.
3. Walk coverage 0.4146 of 357 blocks.
4. Window runs other than the danger builder map no install tables (MatchInfo6 §1).
5. Cleanup: `gf-EndClear-scratch` (traces, `syn/`, `tr/`, `sdbuild/`, `wpc.py`, `lst.py`, `cont.py`, `runsyn.sh`, `prove.sh`), worktrees `gf-EndClear`, `gf-EndClear-push`, branch `EndClear-merge` (local only).
