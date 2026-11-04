# PES port: KickRoot: the kick geometry test's root-motion reader FUN_141AFDF10 proven bit-exact (75/75); GF builds PES's anime chain for it (clip database, key table, layer, pose slot) and matches PES on every captured kick; the pes-loop cutover is NOT wired: GF's real orders leave the request builder's bounded closure (0x140A510F0), and clips that turn the mover leave the reader's (0x1423BAEB0) (04-11-26)

Branch `KickMotion` (worktree `gf-KickMotion`), head pushed (confirm with `git ls-remote origin KickMotion`). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-KickWindow.md`, `pes-port-KickChooser.md`, `pes-port-MotionClips2.md`, `docs/re/pes-re-portmap-motion.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. prove `FUN_141AFDF10` and its closure on the existing captures | **DONE**: 75/75 traced calls bit-exact (section 1). |
| 2. decode the chain at `P+0xA08`; give GF that chain in PES layout; same vector as PES | **DONE for the vectors PES returns on the captured kicks and 21 other motions (all the identity vector); NOT proven for a clip that turns the mover** (section 3: `0x1423BAEB0` is outside the translated closure). The chain is built from the install (`BuildAnimeChain`), no capture bytes. |
| 3. wire `KickOrderFor` + `RunKickRequest` before `RunKickChooser`, delete `KickRequestFor` | **NOT DONE.** Wired once and run: the pes-loop smoke (seed 7, 2 min) aborted with `PES_DISPATCH_UNTRANSLATED: 0x140A510F0`, reached from `FUN_1406DDA60` inside the request builder's 80-member closure, on a path none of the 15 kick_header captures takes (section 4). Wiring reverted, `KickRequestFor` and its test stay. No `[kickmotion]` change, no hash move to explain. |
| 4. merge `origin/main`, census, `GATE fast` | merged (`aa4e051d`; conflicts below). Census: not run (batch BE not on main when I started; nothing moved a seeded match). `GATE fast`: **NOT RUN** (budget). |

## 1. The reader, proven (CONFIRMED)

The call is `0x14082B02F CALL 0x141AFDF10` in `FUN_140829E80` (the brief's `0x14082AFF3` is the `MOV RAX,[R12+0xA08]` that loads the chain: `RBX = *(A+0xF0)`). Arguments: `rcx` = B, `rdx` = 0 (no second output), `r8` = the output vector (`RBP+0x250`), `xmm3` = seconds (`esi` / `FUN_141E532D0`), `[rsp+0x20]` = the motion `FUN_14071E570` answers, `[rsp+0x28]` = a float (0).

`frame_trace.py recorded` with the new root spec `tools/dev/diffharness/specs/kickroot/141AFDF10.json` (`closure.py --members-file`, root only) traced the reader's entries inside every chooser case; `snapdiff.py --bounded --only 0x141AFDF10`:

| run | cases | calls | result |
|---|---|---|---|
| 1 | real `kick_chooser` live2/pid615629 seq15863 (24), seq15864 (12); live2/pid1513193 seq1151 (18), seq1170 (12); SYNTHETIC chooser state h4766 (6) | 72 | `calls=72 replayed=72 mismatches=0 own_block=1.0 (13/13) proven=True` |
| 2 | real pid615629 seq16063 (1), seq16064 (2) | 3 | `3/3`, same closure |

(two runs because at most four full captures are unpacked at a time). Of the 64 chooser cases only these seven enter the reader; the 15 kick_header captures reach it only through the synthetic h4766. The 21-function closure (`closure_kickroot.json`; the lock functions `0x1403DB2C0/410/590/280/210`, `FUN_141AF2C20`, `FUN_141AF32C0`, `FUN_141B034B0`, `FUN_141B03660`, `FUN_1415BB730/BBA50`, `FUN_1423A3FD0`, `FUN_1423A5750`, `FUN_1423A5BC0`, `0x1423B8FB0`, `0x1423B9030`, `0x1423B90E0`, `0x1423BA9D0`, `0x1423BB360`, `0x1423BE8C0`) was already in the layer as kickkind closure; `results_kickroot.json` + SETS `kickroot` make the reader a recorded member (moved to `animation/`). Layer regenerated.

## 2. What the reader reads (CONFIRMED by decompile + a read-set replay of seq15864; offsets are bytes)

Notation: P = the kicker's actor (`rdx` of the chooser), A = `*(P+0xA08)` (the anime player; the window's physique object A), B = `*(A+0xF0)` (`B+0xC0` = G, `B+0xC8` = A).

- Key table: `A+0x180` -> {`+0x58` count (0x1125 motions), `+0x60` -> entries, 16 bytes, key first}. `FUN_141AF32C0(table, motion)` returns `entries + 16 * motion`.
- Clip database: global `DAT_1437CEAA0` (`FUN_141B034B0`) -> {`+8` lock word (`0x1403DB590`: compare-exchange 0 -> 1, zero = free), `+0x18` pack count, `+0x20` -> pack records (0x40 B, pack object at `+0x18`)}. A pack object has `+0x40` == 2 (loaded), `+0x60` -> **the loaded `.mtar` file verbatim** (`FUN_1415BB5C0`: magic `0xBFFA89C`), `+0x90` -> its entry array (0x40 B each, `FUN_1415B8F40`: `+0` key, `+0x10` pack, `+0x18` the gani file, `+0x20` the clip, `+0x30` = 3). `FUN_1415BB730` binary-searches the file's own sorted table (`+4` count, `+0x20` 16-byte entries); **the key is `index << 32 | StrCode32(name)`** (the u64 at the entry's start; capture: motion 3983 -> `0x32F195D8048D`, mtar entry index 13041 = 0x32F1, hash `0x95D8048D`). Returns `entry`; `entry+0x20` = **the gani's MOTION chunk data** (`FUN_14036EDE0(chunk list at file+4, 0xC6E937B9)` + its `+0xC`): the file bytes verbatim, no runtime copy.
- The pose slot: `FUN_141AF2C20(A-chain, .., 0, 1)` walks `A+0x88` -> L1, `L1+0x10` -> L2, `L2+0xF8` / `+0x100` -> two containers {2 entries of 0x20 B (`+0` pose set, `+0xC` count), `+0x48` current index, `+0x4C` state, ready when `& 3 == 1`}; the pose set has `+4` the offset of its float weights, `+0xC` (u16) their count, `+0x30` one offset per layer; the first layer whose weight is non-zero is taken; `layer+0x98` -> the slot table {`+8` first track, `+0x14 + (track - first) * 8` the slot's offset, slots 0x60 B}. `FUN_1423A3FD0` saves the 24 dwords of the slot of each unit of type 5 (output at `r8`) or 6 (output at `rdx`), evaluates the unit into it (`FUN_1423A5BC0`, `FUN_1423A5750`), copies dwords 0x14..0x17 (type 5) out and puts the slot back.
- Output: a 4-float vector. On both captured kicks (motions 3983 `kick_mid_3_3_adjust_y0_000`, 2447 `kick_mid_0_0_adjust_y0_000`, 1/3 s) PES returns (0, 0, 0, 1). PES's own bytes asked motion 100 at 0.5 s return a yaw-quaternion-shaped vector (0, 0.148, 0, 0.989) (INF: the type-5 unit is the mover's rotation, not a position).

## 3. GF's chain (CONFIRMED by tests; the builder is new)

`src/sim/onthepitch/pesanimechain.{hpp,cpp}`, `BuildAnimeChain(exe, vfs)`: reads the ten `body_anime_file*.mtar` through the VFS winner and puts them in the window verbatim as PES's packs (one segment at `kAnimeChainVa` 0x2200000000, ~35 MB, lent to calls without copying), builds the 4,389-entry key table from `MotionNames` (the mtar entry's key), the database record array, each pack's entry array (the MOTION chunk found by **translated `pes_fn_14036EDE0`** run on a `PesState`) and the layer chain. `PesWorld::BindAnimeChain(slot)` (called by `WriteActorP`) links P+0xA08, A+0xF0, A+0x88, A+0x180, B+0xC8. `PesTrapLoaderData::AddTo` carries it. `PesWorld::RunRootMotion` runs the translated reader on a slot's B.

Named stand-in (`ponytail:` at the site in `pesanimechain.cpp`): the pose slot holds zero (PES's anime update `FUN_141AF7A90` keeps the player's pose there; SetMotion `FUN_141AF61C0` builds the table); one layer with weight 1 (PES's layer objects); the database holds the ten body packs only (PES holds eleven). Removing them needs the anime player's construction and update ported.

Tests (run, every capture of the two oracle sets, captures unpacked for the run and packed again):
- `pes_kick_chooser_oracle_test` on `oracle2` (4 real + 18 synthetic): 2/2, descriptor equal on every byte **with GF's chain in place of the capture's GX/GY**; on `oracle4` (pid1513193 seq1151, seq1170) `EveryCapturedCall...` passes (the `AGfBuiltRequest...` test fails there as before: lofted orders carry flags GF's `KickRequestFor` lacks).
- new `pes_kick_root_test`: PES's vector (rootmotion.tsv: the reader's real calls, recorded by `rootrd.py` over the captures) equals GF's on every real call (seq15863, 15864, 16063, 16064, 1151, 1170, h4766) and on 84 SYNTHETIC rows (PES's own bytes on seq15864's memory, 21 motions x 4 times).
- Mutations (each rebuilt, run, reverted): layer weight 1.0 -> 0.0 (both tests fail); key table without the index (`nameHash` only) (both tests fail).

**OPEN (blocks "same vector for any clip"):** every non-identity vector PES returned (131 rows, 48 motions) is for a motion whose clip reaches `FUN_1423BAEB0` (a segment evaluator called by `0x1423A5BC0`, `0x1423A5750`, `0x1423BB360`, `0x1423A5FE0`), which no recorded call has entered: it is not in the translated closure and the call aborts loudly (`PES_DISPATCH_UNTRANSLATED: 0x1423baeb0`). Of 72 motions sampled every 61, 21 run (all identity), 51 abort. A kick clip that turns the mover (the captured `kick_*_adjust` clips do not) would abort a match. Fix: widen the closure over modified-call traces (PES's bytes asked those motions; `rootsyn.py` produces the rows) with `snapdiff --bounded`, then add the 131 rows to the test.

## 4. Why the wiring is not in (CONFIRMED by a run)

`PesKickSituation` was changed to `KickOrderFor` (action, target, the team's pass target as receiver, launch heading, shot power) -> `RunKickRequest` -> `RunKickChooser`. The seed-7 2-minute pes-loop smoke aborted after ~8 minutes with `PES_DISPATCH_UNTRANSLATED: 0x140A510F0`. Its callers are `FUN_1405379C0`, `FUN_1405A4B90`, `FUN_1406DDA60`, `FUN_14060FE20`; only `FUN_1406DDA60` is in the builder's closure (`closure_kick.json`), so GF's order reaches a branch the 15 recorded builder calls never took. OPEN: a capture (or a synthetic header state) of `kick_header_833CD0` with an order on that branch, or the order record's missing bytes (`pes-port-KickHeader.md` section 4: +0x1C, +0x28, +0x5C/+0x5D, +0xB4, +0xCC, +0x149/+0x14A/+0x14C are zero in GF's order). Wiring reverted: `humanoid.cpp` is as on `KickMotion` before this task.

Smoke of the tree as pushed (wiring reverted, chain in the window; seed 7, 2 min, `--pes-loop`, through `simslot.sh`): **`[determinism] a2f78ca1b76a66e4`**, 46,254 ticks to full time, `[kickmotion] 79 kicks, no clip 79, descriptor 1 0, in-flight Q 0` (every kick still takes the zero-geometry request and answers none, so no kick has a clip). That is not main's `85c7d7c515cf92ae` (30,080 ticks): the branch carries the unlanded KickCutover / KickKind / KickChooser commits (the chooser already runs on `KickRequestFor` in `humanoid.cpp`), which main lacks. The ablation that would split it (chain bound vs not) was not run: INF that the chain is inert here (the zero request never reaches the reader, and the kick start's chooser call is its only caller), OPEN until that ablation runs. Switch-off smoke: not run.

## 5. Merge (CONFIRMED)

`origin/main` (`aa4e051d`) merged into `KickMotion` (`c2bcb768`). Conflicts: `pesworldrecord.{hpp,cpp}` (both sides add methods: `RunKickRequest` / `RunKickChooser` and `RunAbilityByte` / `RunEntryAim`, kept both), `gen_translated.py` (SETS keeps `kickkind`, `kick`, `aiconcepts`, `motion`; dead-noise table keeps both rows), `docs/re/LEDGER.md` and `universe.json` (taken from main, then the kick closures' rows re-added with `closure.py --no-specs --members-file` and the ledger regenerated: `ledger.py --check` current). Layer `--check` after the merge: 5,392 functions byte-identical (before the kickroot set).

## 6. Left (numbered)

1. Widen the reader's closure to `0x1423BAEB0` (and what it calls) over modified-call traces, then prove the 131 non-identity rows.
2. Cover the request builder's branch to `0x140A510F0` (a header capture on it, or synthetic states), then re-wire (the `PesKickSituation` change is in commit history of this branch: `KickOrderFor` + `RunKickRequest` + `RunKickChooser`), delete `KickRequestFor`, the zero-geometry request and the `AGfBuiltRequest...` test, smoke, ablation.
3. Census and `GATE fast` on the merged tree (not run).
4. Remove the scratch dir `gf-KickMotion-scratch` (synthetic chooser states ~50 GB, oracle dirs, `rootrd.py`, `rootsyn.py`, `trroot.sh`, `sdroot.sh`, `rmexp.sh`; the oracle dirs hold `rootmotion.tsv` / `rootmotion_synthetic.tsv`); all captures are packed again.
