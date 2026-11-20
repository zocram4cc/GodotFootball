# pes-port-ReplayRuntime4: the trace-layer "divergence" was the instruction budget (fixed in the proof tool); the boot builder's proof is still running; slice 1 (recorder) NOT translated; the captures are now on disk for the next round

Branch `Presentation`, pushed `2c124031..ac9ec418` (`git ls-remote origin Presentation` = `ac9ec4182626`, confirmed). Commit `ac9ec418`: `tools/dev/translate/prove_bounded.py` (`--budget` passthrough, default unchanged) and `docs/re/pes-re-replay-system.md` (§10 correction of §7). No build dir was created this round (no test binary built), so none to delete.

Labels: CONFIRMED = a replay run, a decompile, a capstone read, an E8 scan, a git/ls-remote result. INF = inferred. OPEN = not settled.

## 1. The "trace-layer artifact" (CONFIRMED: it was the budget)

- Three variants of the trace layer on `pid950952/seq001` (boot builder `141EB8E80`), all with the default 50M budget: no trace (`trace=None`), trace with `install_hooks` replaced by a no-op, and the full `install_hooks`. All three stop at the same DLL address `0x6F769F9A19AC` with "did not return … after 50000000-instruction budget". The trace layer is not the cause.
- `frame_trace.py recorded seq001 --hooks … --budget 400000000` (full trace layer): returns `err null` in 113 s. rax = 1, rip = the recorded return `0x141FE4AD6`, 97 pages written, 12,125 bytes written. The boot builder is a long call (about 100M instructions; the DLL's name/CRT loops), not a spin.
- Fix in the tree: `prove_bounded.py --budget N` passes the budget to `frame_trace.py recorded`. (The first attempt I made, a per-spec `insn_budget` knob in `diffharness/run.py`, was reverted: the proof's replay side does not read it.)
- The replay side of the proof uses `diffharness/snapcase.py`, budget `GF_SNAP_BUDGET` (default 2,000,000; the repo's own comment says long calls need more). So the proof run needs `GF_SNAP_BUDGET=400000000` in the environment. That is what the current attempt uses.

## 2. The boot-builder proof (NOT finished at report time)

- Attempt 1 (default budgets): `did not return to 0x141fe4ad6 x1` (replay side, 2M). Attempt 2 (`--budget` set, replay side 2M): same. Attempt 3 (`GF_SNAP_BUDGET=400000000 prove_bounded.py 0x141EB8E80 … --real seq001 --spec-dir tools/dev/diffharness/specs/animelayer --budget 400000000`, no `--merge`): running at report time. Output: `<workdrive>/gf-ReplayRuntime-scratch/work-boot/prove_e8e80d.log`, results under `work-boot/e8e80d/`. It is a background shell started outside the job tracker; it will finish on its own. Its result is NOT recorded in any tracked file (I did not pass `--merge`).
- OPEN: the outcome of attempt 3. If it proves bit-exact, the next step is to translate `141EB8E80` (closure via `closure.py --no-specs`, root spec exists) and to merge the record. If it fails, the log names the mismatch.
- `1408BE3C0` (six calls, `pid950952` seq002–012): not run. The root spec is in `tools/dev/diffharness/specs/animelayer/` (untracked, with `141EB8E80.json`); the spec directory is not committed.
- Attempt 3 (`GF_SNAP_BUDGET=400000000`, `--budget 400000000`, no `--merge`) FINISHED: `0x141EB8E80 calls=1 replayed=1 mismatches=0 own_block=0.9043 proven=True` (log `work-boot/prove_e8e80d.log`; 540 s). **141EB8E80 is proven bit-exact on seq001** (one real call, 85 of 94 own-block functions reached). Not merged into `results_animelayer.json` and not translated into the layer (no `--merge`, no `closure.py` for `bootpath`): both are the next step.
## 3. Slice 1 (the recorder): NOT translated, NOT proven

- The captures that exist for the recorder on `pid1337341` (live2): `rec_1423E14D0` (6 calls, seq002–012, request fetch), `rec_141EDE9F0` (6 state-7 calls, seq107/197/201/205/209/291), `boot_141EB8E80` seq53749. Per Main, also `rec_140A88CE0` (seq53826–53832, 6), `rec_140A88E00` (seq53848/53898/53949/53998), `rec_140A89F20` (seq53849/53899/53950). **Not landed:** `rec_1408BED00` and `rec_1408CCBD0` (EVERY 1000).
- Not done: the translation of the writers and a bit-exact proof per writer. The budget ran out first (around 200 requests). Nothing was translated for slice 1, so `gen --check` is unchanged this round.
- Refused by the hook tool (per Main): `140927450` (call at +9), `140A89E00` (je at +0xD). Their callers need hooks instead.

## 4. What the new captures establish (CONFIRMED)

- The demo tick's caller: `141EDE9F0` is called through vtable slot 0 (`call qword ptr [rax]`) of a node object, from the tree walker `14149FD10` at return address `0x14149FDF2` (CONFIRMED: the captured return address; the disassembly at `0x14149FDE4..0x14149FDF2`). `14149FD10` is itself recursive (`call 0x14149FD10` on each child whose `+0x2C` is 0) and has no direct caller in the image; the node objects' vtables are not in the image as absolute pointers (zero hits), so they are built at runtime. The root of the walk is OPEN.
- `1423E14D0` (request fetch, decompiled, `FUN_1423e14d0`): a state dispatcher over the object's `+8` state: states 1–5 call vtable slots `+0x10`, `+0x18`, `+0x20`, `+0x28` (which stores the next state in `+8`), `+0x30` (state 5). It returns 0 for other states. The goal request id is inside those virtual calls (not decoded).

## 5. Checks

- `prove_bounded.py` `--budget` default path unchanged (no caller passes it except the boot proof).
- `gen --check`: last run at round 2 (10193 functions, byte-identical); no layer change since.
- `pes_anime_engine_test` 11/1 skip, `pes_native_service_test` 7/7, `pes_replay_human_test` 5/3 skip, `test_strlen_model.py` 4/4 (round 2): unchanged, nothing rebuilt this round.
- Decode doc: no machine paths (checked in round 3; §10 added here, text only).

## 6. Captures and scratch (cleanup status)

- `live2/pid950952/seq001` is UNPACKED and the proof (attempt 3) reads it. It must be repacked (`capz.py pack`) after the proof finishes. Not done (the proof is running). Other pid950952 captures are packed.
- `pid1337341` captures (live2) belong to Main's rig; not touched.
- Scratch `<workdrive>/gf-ReplayRuntime-scratch` ~3 MB plus `work-boot/` (the proof's trace dirs, ~tens of MB). Delete after the proof.
- Uncommitted, untracked: `tools/dev/diffharness/specs/animelayer/` (two root specs). Kept on disk for attempt 3; not committed. Remove or commit with the proof result.

## 7. Open items (numbered)

1. Attempt 3 outcome (section 2). Then merge or fix.
2. `1408BE3C0` proof over pid950952 seq002–012 (unpack first).
3. Slice 1: translate `1408BED00`, `1408CCBD0`, `140A88CE0`, `140A88E00`, `140A89F20` once captures exist (Main reports `rec_1408BED00` / `rec_1408CCBD0` pending); prove each bit-exact over its captures; `gen --check`.
4. Root of the demo tree walk `14149FD10` (section 4).
5. Goal request id: decode the virtual slots of `1423E14D0`'s object (section 4).
6. Repack `pid950952/seq001` and delete the work dirs after the proof.
