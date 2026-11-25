# PressLane10: the player driver 0x140925470 and its wrapper 0x140925750. Wrapper proven on real captures (M5), pushed 96566fe69; no GF wiring

Branch `PressLane4` (worktree `<workdrive>/gf-PressLane4`), base `3d9ca8ba9` (Integrate `3ed0f35d9` merged), pushed tip **96566fe699e1998b548503e6816eadcf3e2c0933** (`git ls-remote origin PressLane4` confirms). Scenario claimed: **M5** (translation record: prove_bounded output, one RO window declared). **No GF behaviour change; nothing wired.** Wiring did not land because the driver needs PES's owner writer, which GF does not run (section 3).

## 0. Verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | Translate driver 0x140925470 and wrapper 0x140925750; decode RO tables | Wrapper proven; driver is a member of its closure (968). RO window 0x142632CA8 (0x800) declared. |
| 2 | Prove on real captures; regen; gen --check | Wrapper proven 4/4 bit-exact. Regen 11,268 functions. `gen_translated --check`: **byte-identical, rc 0** on 96566fe69. Driver has no own capture (it runs inside the wrapper; `pdrv_140925470` got 0 captures). |
| 3 | Wire GF per-slot call; delete RunDecisionFrame / RunFrameBuilder / RunPress* / pressMan | **Not landed (blocked).** Driver dereferences X+0x7D8 as the trap actor P; GF's X holds the side number there (section 3). |

## 1. Measurements (unchanged tip 3d9ca8ba9 = baseline; after the regen 96566fe69)

Build: `pes_press_approach_test`, `pes_restart_delay_test`; `PES21_ROOT=<workdrive>/PES21`, `PES_EXE=Unpacker/pes_full.bin`; sims via `tools/dev/simslot.sh`.

| Measure | Baseline (3d9ca8ba9) | After regen (96566fe69) | Bound |
|---|---|---|---|
| ALoosePresserCommandNamesTheActionPressTarget | red: 21 of 10,022 PRESS-man movement commands unnamed | not re-run (no behaviour change) | red on Integrate |
| ADesignatedManWithNoHolderIsCommandedToTheRestingBall | red: 2 of 369 designated-man ticks unnamed | not re-run | red on Integrate |
| PesStallQuick | green, forced_live=53 | green, forced_live=53 | 53 |
| PesStall.ALiveBall* | not run | not run | 2229 |
| PesPassTarget.* | not run | not run | — |

No behaviour change, so ALiveBall, census and PesPassTarget are not due. Main's note (PASS_GET aim via 0x142106110 → 0x140A76440 ← press tick) applies to any future wiring: PesPassTarget.* must be measured then.

## 2. The wrapper proof (CONFIRMED, real captures)

- Captures: `live2/pid3798748/` (relaunch 5), hook `pdrvw_140925750` (entry 0x140925750, EVERY 50, cap 12). Copies used (originals untouched): `seq075`, `seq079`, `seq084`, `seq088` (calls 3701/3901/4151/4351).
- Hooks: launch's `hooks.cfg`, copied to scratch (`RV_HOOKS`).
- Command: `prove_bounded.py 0x140925750 <work> --tag pdrv --real … --merge` (frame_trace recorded, then snapdiff --bounded).
- Result: proven=True, calls 4, replayed 4, excluded 0, mismatches 0 (return value and every written byte). Wrapper own block 5/5.
- Bounded closure: 968 members, including the driver 0x140925470 and the press tick 0x140928FC0 (`closure_pdrv.json`).
- Coverage: the driver's own blocks are covered only as far as the 4 calls enter them; the record's coverage field holds the wrapper's count only.
- Record: `results_trapaction.json` (0x140925750). Specs: `specs/trapaction/140925750.json`, `140925470.json`. Universe rows: 2 added. Set file: `specgen/sets/closure-pdrv.txt`.

## 3. The owner writer (CONFIRMED by decode; blocks wiring)

Headless decompile (bridge down; `dec.sh`): `dec1.txt` (driver FUN_140925470 and FUN_1409211E0), `dec2.txt` (candidate writers).

- The driver dereferences `[X+0x7D8]` at entry: `*(owner+0x3AD8) = slot pointer` (0x140925511..0x14092552A), and tests it at 0x14092559A.
- GF's X at construction: ctor `pes_fn_14091E9F0` (GF: `pesworldrecord.cpp:102-111`, rcx = X, rdx = side, r8 = slot) calls `pes_fn_1408BD970` with rcx = X; that stores the sign-extended side into `X+0x7D0`, `X+0x7D8`, `X+0x7E0` (`pes_fn_1408BD970.cpp:54-55`; disassembly 0x1408BD9B6/0x1408BD9BD). **So GF's X+0x7D8 = side (0 or 1).** The driver would dereference it.
- PES writes P into X+0x7D8 in `FUN_1408BE740` (0x1408BE740..0x1408BE83E): `param_1[0xfb]` (= X+0x7D8) = P, allocated when zero (`FUN_1403FB410(0x18, 0x3BC0)`), constructed by `FUN_1406E03D0` (ConstructActorP); then `FUN_141EB5B30(P, X+0x624)`, `FUN_141EB56D0(P, X+0x61C)`, `P+0x3AC0 = X+0x7E0`, `P+0x3AD8 = result of X's vtable slot +0x90`, `FUN_1406F0360`, `FUN_1406F0350`; gate `FUN_1408BE3C0`.
- Callers of FUN_1408BE740: OPEN.
- INF: X+0x7E0 holds the side after the ctor and is copied to P+0x3AC0. PressLane9 expected PES's `CtxVa(side)` there. Not settled: needs the caller of FUN_1408BE740 or a live read of X+0x7E0 (seq075 has it).
- Counterexample for wiring now: the driver on GF's X (no FUN_1408BE740) reads the side number as a pointer.
- Rule 4 (port the writer, never set the field): the writer is a chain (allocation, ctor, two writers, vtable slot, gate). Not ported this round.

## 4. RO window (CONFIRMED by decode of the proof build)

- `pes_fn_14092AF10` (press tick per-entry reader; proof build `pw1/build/gen/140925750/pes_fn_14092AF10.cpp:501-502`) reads `rdf(0x142632CA8 + 8*byte)` and `rdf(0x142632CAC + 8*byte)`. `byte` comes from a record walk at 0x14092B3D0 (`word at X-4`, `byte at X`, `X = 0x142632CC4 + 8*i`, `gpr1 += 8`, count in gpr8).
- Declared in `ro_tables.json`: `0x142632CA8`, 0x800 bytes. INF: the walk stays within i < 0xFE; gen's lint would flag any read outside the window, and it did not.

## 5. Regen and check

- Regen 1 (no RO window): held out 0x140925750 (live RO read at 0x14092AF10) and `0x1409586B0 calls 1405C0B90, which is not committed` (1405C0B90 is in the wrapper's closure; its root was held out). GENRC=1, layer not written.
- Regen 2 (with the window): wrote 11,268 functions (4,162 recorded bit-exact, 7,106 closure), up from 11,191 / 4,161 / 7,030.
- `gen --check` on the staged tree (before commit): only 77 untracked drifts (the new closure files); content identical.
- `gen --check` on the committed tree 96566fe69: **11268 functions, byte-identical, rc 0.**

## 6. Verification

- prove_bounded (wrapper): proven, 4/4, 0 mismatches, 0 excluded.
- gen --check: rc 0 on 96566fe69.
- Build: both press test binaries built after the regen (ccache; the worktree's godot-cpp symlink was an empty dir and was restored for builds, swapped out for git).
- Tests run: PesPressApproach.ALoose* and ADesignated* (baseline, red); PesStallQuick.* (baseline and after regen, green, 53).
- Gate: not run (Main gates).
- PII: grep of staged files for `<workdrive>` and `<home>`: no hits. `tools/check_pii.sh` not run in the worktree (symlink trap).

## 7. Open items

1. Caller of FUN_1408BE740 (xrefs; bridge down). Settles the ctx value and the order of the man's creation.
2. Port the owner writer chain (FUN_1408BE740 and callees, each proven) so GF's X holds P before the driver runs.
3. Driver's own proof: needs a capture where the driver is entered alone. The wrapper's rendezvous holds it, so `pdrv` got 0 captures. Ask Main to arm `pdrv` on its own callers (or accept the wrapper proof as covering it).
4. Wiring (task 3) after 1 and 2, measured against ALoose, ADesignated, Quick, census, ALiveBall and PesPassTarget.
5. The static closure job (`closure.py --tag pdrv`, 2,567 members, 418 with neither spec nor layer translation) is still running (`closure_pdrv_full.log`); kept, not shrunk.

## 8. Rig

No new requests. `pressG_140961FB0` (seq123, seq207, seq226 of pid3798748) is unused.

## 9. Next exact step

Find the caller of FUN_1408BE740, then port its chain one function at a time, each with its own proof, before any wiring.
