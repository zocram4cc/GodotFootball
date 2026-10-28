# pes-port-TrapReplay: the kind-0 trap chain is replayed, not yet proven; GF's trap is unchanged (28-10-26)

**Status: NOT CLOSED.** The chain is not proven (no translated binary was built), GF's trap still runs `pes::PickTrapClip`, and no mutation or `pes_*` sweep was run (no GF code changed). What landed: the branch merge, every capture replayed through PES's own bytes, one harness false-pass caught and reported, and the entry register roles pinned. Branch `StoppageFlow`: merged `origin/main` at `9d7875fb` to `16a99947` and pushed (`git ls-remote origin StoppageFlow` = `16a9994730b0214c4e16b1c18c72036df6f45e10`). Scratch: `<workdrive>/gf-TrapReplay-scratch/`.

Tags: **CONFIRMED** = read from a capture or a harness run in this session; **INF** = inference (reason given); **OPEN** = not settled (blocker named).

## 1. Merge and push (step 1)

- `git merge origin/main` clean (no conflicts; the godot-cpp gitlink is untouched; `git -c status.submodule=none` needed because of the symlink). The merge brought the docs the brief cites (`docs/ports/pes-port-TrapClipPort.md`, `pes-port-OwnerPickup2.md`, `docs/re/pes-re-record-ids.md`, `docs/re/pes-re-record-layouts-chains.md`). **CONFIRMED** pushed.
- `git diff c4a659f6 HEAD -- src tests apps CMakeLists.txt` is empty: the existing `build/` is current, so the stall binary `build/tests/sim/pes_stall_test` is the HEAD binary.

## 2. Replay verdicts: 12 of 12 captures return cleanly (CONFIRMED)

Captures: `<workdrive>/gf-Rendezvous-scratch/live2/pid463316/` (full, not slimmed). Replay: `frame_trace.py recorded SNAP --hooks <workdrive>/PES21/rendezvous/hooks.cfg --budget 50000000 [--out DIR]` (PES_EXE = `<local>/ghidra-projects/scratch/Unpacker/pes_full.bin`). `err: null` = the emulated PES call returned to its recorded return address with rsp balanced. Each call's rip at return: `0x1407A03EE` (kind-0, return into the dispatcher) and `0x14079A8C1` (commit, return into kind-0 at the commit call site: INF from the return address).

| capture | hook | rax low byte (return) | written bytes (own) | nested traced calls (spec-dir run) |
|---|---|---|---|---|
| seq4310 | trap_kind0_79A470 | 0x00 (0) | 1950 | 14079A470, 14079AFD0, 140A57650 |
| seq4329 | trap_kind0_79A470 | 0x00 (0) | 1940 | same three |
| seq4351 | trap_kind0_79A470 | 0x00 (0) | 1989 | same three |
| seq4402 | trap_kind0_79A470 | 0x00 (0) | 1411 | 14079A470, 14079AFD0 |
| seq4469 | trap_kind0_79A470 | 0x01 (1: issued) | 2063 | 14079A470, 14079AFD0 |
| seq4531 | trap_kind0_79A470 | 0x00 (0) | 1921 | 14079A470, 14079AFD0 |
| seq4273–4278 (6) | trap_commit_79AFD0 | 0x00 (0) each | 35 each | 14079AFD0 only |

Verdicts: **12 of 12 clean replays (`err: null`, exit 0)**. Return values are the low byte of `rax` (INF: the return type is bool, from the doc's `return 1 = issued`). Every kind-0 capture also runs the commit `0x14079AFD0` inside its own call (CONFIRMED, trace `calls/`), so the chain's step 9 is reached in 6/6. The 6 standalone commit captures are all the same refusal shape: return 0, 35 bytes, no traced callees (INF: the early refusal `vt+0x28` / `FUN_140798A70` gate of `pes-re-trap-kind0.md` §5; not decoded here).

## 3. Proof attempt: commit `0x14079AFD0` (6 calls) is NOT proven (CONFIRMED false pass, reported)

`snapdiff.py --only 0x14079AFD0` over the 6 commit captures (spec `<workdrive>/gf-TrapReplay-scratch/specs/14079AFD0.json`, a minimal snapshot-only spec; result `sd_commit.json`):

- `calls=6 replayed=6 mismatches=0` **but `translated: false`, note `translate 0x140A4EC7F: no instructions`**. The translated side was never built, so the zero mismatches are a false pass. Not counted as proven.
- Cause (CONFIRMED): the closure contains `0x140A4EC7F`. It is not a replayed indirect site (the six `trace.json` files name only `0x14079B049 -> 0x1406E66B0` and `0x14079B057 -> 0x1406EA810`), and no spec, stub or addrs file lists it. It enters through `drv.runnable_extra(reached_seed)` (`snapdiff.py:50-52`). In the image it is mid-instruction: bytes at `0x140A4EC7F` are `c6 0f 28 74 24 20 48 83 c4 30 5b c3`; the epilogue `0f 28 74 24 20` starts at `0x140A4EC80`. The translator returns no instructions, and the build aborts after the replay has already counted its matches.
- Sent to ImportModel4 (harness owner): the walker should not admit a non-boundary address, or the build should record an out-of-set abort. **OPEN** until they land a fix.
- A second blocker is structural: `0x14079A470` and `0x14079AFD0` are not in `tools/dev/ledger/universe.json` (key form `140001000`; the trap chain is reached through the Trap vtable `+0x68`, so the direct-call walk never visits it). `specgen generate --set` cannot produce these specs; the minimal spec was hand-written (scratch only).

## 4. Translation of the kind-0 entry (CONFIRMED: the translator runs; not proven)

`xlate.translate_one(image, '', '0x14079A470', [])` emits 603 lines, no notes; `0x14079AFD0` emits 367 lines, referencing only itself. Output is scratch (`<workdrive>/gf-TrapReplay-scratch/xl_79A470.cpp`), not committed. The kind-0 closure (`140A57650`, `14079AFD0` and the callees of §5 in `pes-re-trap-kind0.md`, including `14084DF20`, `140857250`, `14085B3E0`, `140798A70` (280 lines) and `14079F920`) is not translated or proven. **OPEN.**

## 5. Entry register roles (CONFIRMED against the 6 kind-0 and 6 commit captures)

`pin_fields.py` (scratch) reads the entry registers and fields from each capture's own memory.

| reg | kind-0 entry (14079A470) | commit entry (14079AFD0) | evidence |
|---|---|---|---|
| rcx | the Trap action; `[+0]` = `0x1425E1ED0` (the Trap vtable in `pes-re-trap-kind0.md` §1) | same | seq4310, seq4273: `[rcx]` = image vtable; `+0x1` class `0x1E` (both) |
| rdx | the player record `P` (anime role per the chain docs; INF); `[+0]` = `0x1425E34D8`, `[+0xAB8]` = `0x1425E3508` (= vt+0x30, the interface in `pes-re-record-ids.md` §P), `[+0xC40]` = `0x1425E34C8` (image) | same | seq4310, seq4273: both pinned |
| r8 | the ball object; the same address `0x033F8B04` in all 12 captures (INF from constancy and the `(FUN_140A6CF70(ball,t))` use) | same | the object's bytes change between captures |
| r9 | the request on the stack (`rsp`-relative); `+0x1C` flag; `+0x8` kind word (not pinned to a value; INF) | the commit's query/out block (INF) | seq4310: `r9+0x1C` = 0; seq4273: `r9+0x40..` |

Field readings on `rdx` (P): `+0x2752` (s16) = `0xFFFF` in both pinned kind-0 and commit captures, which matches the kick doc's kind short at `P+0x2752` (CONFIRMED value; meaning INF). `+0x3B18` (f32) = `1.0` / `1.02778` (CONFIRMED); `+0x22` u8 = `0xE8` / `0x35`; `+0x1C` u8 = `0x74` / `0xC2`. **Not confirmed:** the doc's anime `+0x70` (state), `+0xDC` (candidate index), `+0x19/+0x18/+0x1A` (flags) were read on `rdx` in 2 captures only (all `0`), which is not enough to pin them; they stay OPEN. My earlier reads of `+0x70`/`+0xDC` on `r8` were on the ball object and are void.

**G-table check (CONFIRMED negative):** none of rcx/rdx/r8/r9 falls in the G anime table (`G+0x19C8`, stride `0x3F40`, 26 entries), so the chain's `anime` is not a `pesworldrecord` anime block. `P` is the record the kick port already pinned as "not a G block" (`pes-re-record-layouts-chains.md` §3). OPEN: the record's owner and the `+0x70/+0xDC` offsets.

## 6. GF state and the picker (not done)

- No GF change. `pes::PickTrapClip` is still reached only from `_TrapCommand` (`playercontroller.cpp:510`), and `src/sim/onthepitch/pesreceive.cpp:212` / `pesreceive.hpp:285` define it. The unit tests `tests/sim/pes_receive_test.cpp:445-662` use it.
- **Red oracle:** `pes_stall_test` `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: see §8 (baseline run in progress at the time of writing).
- No mutation was run (no new code; the rule is one mutation per record binding once bindings exist). No `pes_*` sweep was run.

## 7. Unfinished (numbered; each names the blocker)

1. **Proof of the commit `0x14079AFD0` (6 calls):** blocked by the harness closure defect (§3). Settled by an ImportModel4 fix, then `snapdiff.py` must report `translated: true` and the 6 calls bit-exact.
2. **Proof of the kind-0 entry `0x14079A470` (6 calls) and its callees** (`140A57650`, `14084DF20`, `140857250`, `14085B3E0`, `140798A70`, `14079F920`, `140799440`, `14079FBC0`): not started beyond the translator run (§4). Needs specs for the trap chain (not in the ledger universe, §3) and the vtable stubs for the candidate tests.
3. **Struct layout for the chain's records** (rdx `P`, the action at `P+0xC40 -> +0x1528+8*id`, the query `+0x6C/+0x20`): partially pinned (§5). The `+0x70/+0xDC/+0x19/+0x1A/+0x1C` meanings and the `P` owner are OPEN. Needs the writers (kick doc §3 writer list) and more captures with distinct states.
4. **GF's `_TrapCommand` on the PES path and deletion of `pes::PickTrapClip`:** not done (depends on 1–3).
5. **Red unit test on one captured call's inputs:** not written (no chain to test; it needs fixtures that cannot be committed, since captures hold PES bytes; the test must read the install at run time or pin a relation).
6. **Mutations per record binding:** not run (depends on 4).
7. **`pes_*` sweep:** not run (no code change). Baseline: the OwnerPickup2 sweep (95 green, 1 red).
8. **Disk:** the watcher released seq26788 onward "without snapshot: disk under 450 GiB" (`live2/watch.log`). Captures after seq26787 are lost until disk is freed (Main).

## 8. Stall baseline (red oracle)

Command: `PES21_ROOT=<workdrive>/PES21 tools/dev/simslot.sh timeout 900 nice -n 19 ctest -R ALiveBall --output-on-failure` from `build/` (the HEAD build, unchanged code). Log: `<workdrive>/gf-TrapReplay-scratch/stall_baseline.log`. Result (CONFIRMED): `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` **FAILED**, `c.liveFirings` = 147 (`tests/sim/pes_stall_test.cpp:122`), 135.8 s, `stall rc=8`. Red at HEAD, matching OwnerPickup2 §7.


## 9. Verification run

- Replays: 12 `frame_trace.py recorded` runs (above), 12 `err: null`.
- `snapdiff.py`: 1 run (commit, 6 calls): `translated: false` (§3).
- `xlate.translate_one`: 2 runs (§4).
- Tests: none written or run beyond the stall baseline.
- Mutations: none.
- Harness report: ImportModel4 told (§3).
