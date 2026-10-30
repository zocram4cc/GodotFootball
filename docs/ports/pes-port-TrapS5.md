# pes-port-TrapS5: the Trap action tick 0x140797CD0 translated on a capture-bounded closure and proven bit-exact on the 14 captured ticks; GF wiring NOT done (30-10-26)

Branch `StoppageFlow`. Commits (confirm with `git ls-remote origin StoppageFlow`): `b92caf71` merge of `origin/main` (item 1, pushed), `32b45d56` snapdiff `--bounded` + loud tail jumps + tests, `8703e5b1` closure.py `--members-file` + test, `31f6b040` the translated tick, specs, record, regenerated layer. Image `pes_full.bin`, base `0x140000000`. Tags: **CONFIRMED** = a tool run this session; **INF** = inference; **OPEN** = blocker named.

## 0. Verdict

| Item | State |
|---|---|
| 1. merge `origin/main`, push; `sqrtf` model | merge DONE (`b92caf71`). `sqrtf`: the model equals the export on every argument except NaN and negative non-zero ones; whether the captured arguments include such a value is NOT checked: gap named in section 1 |
| 2. capture-bounded closure of `0x140797CD0`, bit-exact via snapdiff | **DONE, CONFIRMED**: 154 members (the unbounded build was 1,033), **14 of 14 replayed calls bit-exact** (rax, xmm0, every written byte), own-block 20/27 = 0.7407; every call outside the 154 aborts loudly |
| 3. anime-layer objects in GF's window; the P+0x2472 writer | writer TRACED (section 3); the window objects are **NOT written from GF state** (not started) |
| 4. GF calls the tick for the Trap action; stand-in deleted; red-first GF test; mutation | **NOT DONE.** The play-at-once stand-in in `PlayerController::_TrapCommand` is unchanged. `pes_stall_test` not re-run (no GF code changed). Closure tooling tests have two mutations (section 2) |
| 5. 2-min smoke lines, `GATE_TIER=fast` | **NOT RUN** (load 30+; the budget went to the proof, which compiled a 154-unit closure twice) |

The task said "24 action-tick captures". There are **14**: `live2/pid483669/seq6634..6678` (12) and `live2/pid615629/seq126, seq4824` (2). The watcher log shows the later hooks released "without snapshot: disk under 400 GiB" (04:46-04:48): no more arrive until disk is freed (`df` showed 397 GB free on the capture drive).

## 1. Item 1: the `sqrtf` import model (the gap, named)

- The game's sqrt thunk jumps through `0x1425301F0` to the ucrtbase export. Its code read from the capture (`pid483669/seq6634`, slot value `0x6ffffec86620`): it spills the argument, tests the exponent (`0x7f800000`): NaN goes to a handler (`0x6ffffec858b0`); otherwise, **non-zero with the sign bit set** (a negative number, -Inf included) runs the domain-error path (returns the default NaN `0xffc00000`, sets errno); every other argument, +0, -0 and +Inf included, runs `sqrtss`.
- The model in `import_stubs.json` (`code_hex f30f51c0c3` = `sqrtss xmm0,xmm0; ret`) equals the export on all of those, and for a negative argument `sqrtss` also yields `0xffc00000`; the two differ only in the errno side effect and in the NaN handler's result (OPEN: the handler at `0x6ffffec858b0` was not decoded).
- **Not checked: the arguments the 14 captures pass.** The translated build inlines `pes_model_sqrtf` (`std::sqrt`), so the harness side never shows them. Fix: recompile the units that call it with a logging wrapper (about 10 min on a quiet machine).
- The replays are bit-exact on all 14 calls, which already shows the model agrees with the real export on every sqrt the captured ticks perform (the harness runs `sqrtss`, PES ran the export: the written bytes match). INF, since the proof compares the translated run against the harness model, not against the live export.

## 2. Item 2: the capture-bounded closure (CONFIRMED)

Tooling (general, committed with tests):
- `snapdiff.py --bounded`: the build is the root, the reached indirect targets, and each direct callee or tail target **whose entry the replays executed** (the union of the harness's executed addresses); a callee the harness ran is built even when `stubs.json` calls it `unmodellable` (the harness executed its bytes: `0x140A58550`, the hash-checked integer reader, was the 4 failures of the first run: `PES_DISPATCH_UNTRANSLATED 0x140a58550`, fixed); a modelled callee stays inlined; every other call is the translator's `pes_dispatch_missing`. The member list goes to `<out>/gen/<VA>/members.json`.
- `xlate.py`: an unresolved out-of-range tail jump is now `pes_dispatch_missing(target); return;` instead of a silent `return;` (the same line as `origin/NativeSeam`'s change, so the two merge). Ten committed closure files changed on regeneration.
- `closure.py --members-file`: the closure is exactly the listed members; others stay loud aborts. Specs go to a new set `specs/trapaction` (154), set file `specgen/sets/closure-trapaction.txt`, record `results_trapaction.json` (`proof: snapdiff`, counts only), `closure_trapaction.json`, and `gen_translated.SETS` gained `("results_trapaction.json", "trapaction")`.
- The layer was regenerated (`gen_translated.py`): **4,327 functions** (3,691 recorded, 636 closure; +68 files incl. `trapaction/pes_fn_140797CD0.cpp`). Held out (pre-existing, never committed): `0x1403BB670`, `0x1405549C0`, `0x14202BA30`.

Proof run (harness Python): `frame_trace.py recorded` on the 14 captures (root-only spec as the traced set), then `snapdiff.py --bounded --only 0x140797CD0 --hooks hooks.cfg` over them: `calls=14 replayed=14 mismatches=0 own_block=0.7407 proven=True`.

Tests (`tools/dev/translate/test_closure_walk.py`, 22 OK; harness Python): 4 new (a bounded closure leaves an unentered tail target to the loud abort; builds an entered one; builds an entered callee that stubs.json calls unmodellable while the unbounded walk does not; `crawl` with `allowed` walks only the members). **Mutations, run and reverted**: M1 bounded ignores the executed set -> the unentered-tail test FAILS; M2 bounded refuses stub-table callees -> the unmodellable test FAILS; M3 `crawl` ignores `allowed` -> the crawl test FAILS.

Coverage of the 154 (INF from the captures): the phase machine entered cases 0 (start, `0x140795A40` -> `0x140795AB0` -> **`0x1406F0F70` SetMotion**), 1 and 3 (`0x140796CE0`). **Not entered, so NOT in the closure and loud aborts in GF if reached**: `0x140795BC0`, `0x1407964A0`, `0x1407965E0` (the start handlers for action byte `A+0x1A` = 3, 4, 5, 9), cases 4-6 (`0x140796A40`, `0x140796B30`, `0x1407976D0`), the root solver `0x1406E85B0`, the mode setter `0x1406F02F0`, the command slot getter `0x1406D9010`. Captures cover phase 0 (2), 1 (4), 3 (8). A GF match that runs a trap past phase 3 would abort until captures of those phases exist (the hook is FIRST 1 EVERY 3, cap 12: it needs a higher cap and free disk).

## 3. Item 3: what writes P+0x2472 (CONFIRMED decompile; the GF side is OPEN)

- `FUN_1406F0F70` (SetMotion(P, motion, startTime, **mirror**, blend, p6, p7)): `*(P+0x2472) = mirror` (store at `0x1406F10DA`), sets `P+0x3B50` (motion), `P+0x3B54`, `P+0x3B58` (mirror), `P+0x3758` (blend), `P+0xAD0` bit 36 (mirror) and the anime layer's motion (`P+0xAE0`, vtable `+0x10`). The action start handlers write the byte inline too (`0x140795CE0`, `0x1407966E5`). The phase-0 handler `0x140795A40` (vtable `+0x58` of the Trap action `0x1425E1ED0`) switches on `A+0x1A`; the captured phase-0 ticks entered `0x140795AB0` and SetMotion, so **the translated tick itself writes P+0x2472** when it starts the committed plan. Nothing else needs porting for the byte.
- Captured values (capture dump): phase-0 ticks `seq6634` (P+0xAE0 = 0x23, plan `A+8` = 0x9C4, P+0x2472 = 1 before the tick) and `seq126` (0xB24, plan 0xA6F, 1); phase-3 ticks `seq6666` (0x32B, 0x9DC, 1) and `seq4824` (0xD50, 0xB4E, **0**): PES plays this trap mirrored in three of the four sampled ticks and unmirrored in one.
- GF side (not done): P already holds the anime layer's vtable (`P+0xAB8`, `pestrapactor.cpp`); the tick additionally reads P+0x2471 (segment), P+0x2472, P+0x2704.. (modes; `11000000 11000000...`, then `0c000000`), P+0x2460 (flags), the action object P+0xC80 (state from the commit chain, persists across ticks), `*r8 + 0x870`, and the command slots at P+0xC40. The live-in manifests of two captures are in `<scratch>/oracle/` (made with `trap_oracle.py`, 83-93 lines: 17-18 P ranges, ~30 heap ranges, 5 image globals). `HumanoidBase::PlayIssuedClip(motion, playback, mirror)` already plays an issued clip authored or mirrored: the GF consumer of P+0xAE0 / P+0x2472 after a tick.

## 4. Open items (numbered, for the next run)

1. GF wiring (items 3, 4): add `PesWorld::RunTrapTick(slot, ext)` beside `RunTrapKind0` (rcx = `PVa + kPActionOffset`, rdx = P, r8 = a stack slot holding the frame pointer whose `+0x870` the tick reads: **decode that object first**, it is not in `PesWorld`), write P+0x2471/0x2472/0x2704.. from the world window state, call it each tick a man's action byte is TRAP (7), feed P+0xAE0 and P+0x2472 to `PlayIssuedClip`, delete the Special stand-in in `_TrapCommand`. Red-first: a test over `trap_oracle.py` directories of these 14 captures (the existing `pes_trap_oracle_test.cpp` `Build()` is the pattern; copy the action object bytes from the capture as previous-tick state, never P's derived fields).
2. More captures of phases 2, 4-9 (and the start handlers for `A+0x1A` = 3, 4, 5, 9): hook cap above 12 and `--hook-max trap_action_tick_797CD0=N`; disk below the 400 GiB floor stops all captures now.
3. `sqrtf` argument check (section 1) and the NaN handler `0x6ffffec858b0`.
4. Run `gen_translated.py --check` (the regeneration just written is byte-identical by construction; not re-run) and the layer build (only `g++ -fsyntax-only` was run on `pes_fn_140797CD0.cpp` and `pes_fn_14047BCA0.cpp`: ok).
5. Merge note: NativeSeam changes the same `xlate.py` line (comment text differs, the code line is the same).
6. `pes_stall_test`, the 2-min smoke lines on both paths and `GATE_TIER=fast GATE_SLOTS="3 2"`: not run, Main gates.

## 5. Verification run

- `test_closure_walk.py` 22 OK (3 mutations, each run and reverted); `snapdiff.py --bounded` 14/14 bit-exact; `closure.py --members-file` and `gen_translated.py` ran clean; syntax check of two layer files.
- Scratch (PES-derived, not committed): `<workdrive>/gf-TrapS5-scratch/` (`rv4/` the proving build, `rv4.json`, `members.json`, `oracle/`, `dumpcap.py`, `scan_disp.py`, `sd.sh`, `rv.sh`).
