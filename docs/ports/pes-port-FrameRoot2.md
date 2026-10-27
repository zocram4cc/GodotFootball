# FrameRoot2: one level above the leaves (camera, HUD, stack candidates), H-leaf replay (26-10-26)

Branch `FrameRoot` (on `origin/NoFloor`), pushed: `58634146` (`git ls-remote origin FrameRoot` confirms). Commits: `fc03afd5` (FrameRoot, previous report), `58634146` (this change: default root h, `--root entry`). Gate not run (Main gates). Scratch is `<workdrive>/gf-FrameRoot-scratch/` (PES-derived, not committed). No new SIGSTOP snapshot was taken; all runs read existing snapshots. No live reads.

## Result

- **Default root is H** (`--root h`). The pump root is kept as `--root frame`, documented PARKED in the code and here (docs/re/pes-re-liveroster.md).
- **Camera: the update runs from its live object and gets past 0x1408A2169.** Entry `0x14087F280` (InplayCamera::update, vtable `0x142609C28` slot 4, CONFIRMED) with `rcx` = the live InplayCamera `0x34BE44E0` (FullSnap snap1-142012: vtable matches, `+8` = mode 5, in-play). Two frames, no stop. The context `rdx` is a live pump record (`0x7FF5FFDFAB50`, type 2): with `rdx = 0` the run stops at the same fault as before, `0x1408A2169`. **Impure reach from the update: 0.**
- **Camera caller (the one-level-up caller): found, but its gate is closed in the snapshot.** The update has no direct callers (vtable only). Its slot-4 call sites in the camera module: `0x140861C48` (in `0x1408614C0`), `0x14087408F` (in `0x140873FBA`, an array loop), `0x140886814` (in `0x140886780`, the in-play holder path; callers `0x140886760` and `0x140886890`). `0x140886780` calls the update only when the holder's `+8` mode is 1..6 (CONFIRMED by disassembly). The holder at `0x360B9FD0` (the only `holder+0x10 = camera` candidate) has `+8 = 0x33`, so in that snapshot the caller's update path is not live. `rdi`/`rsi` are not inputs: they are set inside the caller from `rcx` and `rdx`; the update's only external input is `rdx`.
- **HUD: the radar widget's own update runs two frames clean from its live widget.** Radar widget `0x1F713E70` (vtable `0x142BDFEA0` slot 0 = `0x1420E2910`, CONFIRMED static and live); scorebug `0x34BFB900` (vtable `0x142BE70B8` slot 17 = `0x1420F0EB0`). With TLS modelled, no TLS stop. **The owner's update was not reached** (open 2).
- **Stack evidence (item 3):** 10 cutscene and scene-birth snapshots with a `mem.bin` were scanned; 26 directories have no `mem.bin` and were skipped. Classified frames: 4 unique (snapshot, class, function) (§3).
- **Replay, H leaf (16 functions reached in run-h-snap4): 14 PROVEN** (every replayed call bit-exact), **142 calls, 0 mismatches**. Two are not replayable (§4).

## 1. Camera

| Item | Value | Label |
|---|---|---|
| Update | `0x14087F280`, vtable `0x142609C28` slot 4; the only data qword equal to it is the slot `0x142609C48` | CONFIRMED |
| Update's input | `rcx` = this (`mov rbp, rcx`); `rdx` = `r15`, passed as the rdx of `0x1408A1A70` (`0x14087F38D`/`0x14087F392`) | CONFIRMED |
| Fault mechanism | `0x1408A1A70` does `mov rsi, rdx`; for mode 2 or 4 it jumps to `0x1408A214B`, which stores `[rsi+0x30]` at `0x1408A2169`. With `rdx = 0`, `rsi = 0` and the write faults. Mode 5 takes the same `rdx` through the same function | CONFIRMED (disassembly); stop reproduced |
| Live object | `0x34BE44E0` in FullSnap snap1-142012 (vtable scan: one hit) | CONFIRMED |
| Context `rdx` | live pump record `0x7FF5FFDFAB50` (ring slot at `idx = 18`, type word `0x400002`) | INF: which record PES passes is not shown |
| Result | 2 frames, no stop; impure reach 0 | CONFIRMED (run) |

Module slot-4 call sites: `0x140861C48` (`mov rcx,[rsi+8]; add rcx,0xAC0; mov edx,5; call [rax+0x20]`: a slot-4 call with `edx = 5` on a sub-object at `+0xAC0`, not the in-play object), `0x14087408F` (array loop over `[r14+rax*8]`, count in `edi`; a plugin list, not shown to contain the camera), `0x140886814` (the holder path, the in-play caller).

Stack evidence for the camera: cutscene snap1-213436 thread 972 has camera return addresses `0x14088AEE2` (in `0x14088AEC3`), `0x140889685` (in `0x140889640`), `0x14088C03E` (in `0x14088C010`). They sit inside the pump's record dispatch (`0x141B3DEB2` in `0x141B3DE00`, via `0x1403FF490`). That supports a pump-dispatch context. They are not the in-play update: that snapshot has no live InplayCamera (vtable scan: 0 hits), and the nearby calls are cookie checks and a vtable slot-5 call.

Live objects by snapshot (vtable scans, 8-byte-aligned qwords in readable memory):

| Snapshot | InplayCamera `0x142609C28` | VerticalCamera `0x142609F10` | HUD radar `0x142BDFEA0` | HUD scorebug `0x142BE70B8` |
|---|---|---|---|---|
| FullSnap snap1-142012 (open play) | 1 (`0x34BE44E0`) | 0 | 1 (`0x1F713E70`) | 1 (`0x34BFB900`) |
| MainSnap snap4-163107 (play 2) | 0 | 0 | 1 (`0x37FF0620`) | 1 (`0x3D7B3A80`) |
| Cutscene snap1-213436 | 0 | not scanned | 1 (`0x333B3540`) | 1 (`0x2BEB3E50`) |

## 2. HUD owner (open)

- Radar widget referenced at `0x2C11A5B0` and scorebug at `0x2C11A478` (FullSnap snap1, ptr scan: one and seven hits). Both lie in a region of stride-`0x40` records whose `+0` qword is `0x142608738`.
- `0x142608738` is **not a class vtable**: slot 3 is the ASCII string `CAMERA_…`. Functions that reference it: `0x14086CA60`, `0x14089AEE0`, `0x14089B110`, `0x14089B280`, `0x14091E9F0`, `0x14217C4B0`, `0x14217D340`. The HUD owner is therefore not identified: OPEN, not inferred.
- So "rerun the HUD widget from its owner's update" was not done; the widget's own update was run instead (§1, HUD).

## 3. Stack evidence (item 3)

Scan of every thread stack (`scan_stacks.py`): each TEB's live `[StackLimit, StackBase)`, image return addresses, containing pdata function, range class.

| Snapshot | Thread | Return address (function) | Class | Status |
|---|---|---|---|---|
| cut/snap1-213436 | 972 | `0x14088AEE2` (`0x14088AEC3`) | camera | live frame, under pump dispatch |
| cut/snap1-213436 | 972 | `0x140889685` (`0x140889640`) | camera | live frame, under pump dispatch |
| cut/snap1-213436 | 972 | `0x14088C03E` (`0x14088C010`) | camera | live frame, under pump dispatch |
| MainSnap snap4-163107 | 1092 | `0x141F73C0F` (`0x141F73707`) | fixdemo | **candidate only**: `0x141F73707` is not named in any doc, and this thread's other stack words are rdata pointers (stale), so liveness is unverified |

- No HUD frames on any stack in the 10 scanned cutscene and scene-birth snapshots.
- Pump-dispatch frames (class `pump`) appear in 21 threads across those 10 snapshots: the dispatch is live in every scanned snapshot; the camera is not.
- The `call [reg+0x20]` pattern appears on several threads; not used as evidence (it matches any vtable slot 4).

## 4. Replay (snapdiff, H-leaf trace run-h-snap4, 16 reached functions)

Owner rule: PROVEN = every replayed call bit-exact; coverage recorded, not a gate.

| VA | ledger tag | calls | replayed | mismatches | own-block cov |
|---|---|---|---|---|---|
| 1404405D0 | none | 3 | 3 | 0 | 0.216 |
| 1404F6C30 | AI (team tick) | 6 | **0** | - | - (no replayable case) |
| 1404F8C70 | none | 6 | 6 | 0 | 0.011 |
| 1404FA720 | none | 6 | 6 | 0 | 0.091 |
| 1404FB2A0 | none | 6 | 6 | 0 | 0.375 |
| 1404FE9C0 | none | 6 | 6 | 0 | 0.046 |
| 1404FEDC0 | none | 6 | 6 | 0 | 0.082 |
| 140509A40 | none | 3 | 3 | 0 | 0.060 |
| 14051B8B0 | none | 6 | 6 | 0 | 0.050 |
| 140529490 | none | 21 | 21 | 0 | 0.696 |
| 14052A190 | none | 28 | 28 | 0 | 0.333 |
| 14052A820 | none | 6 | 6 | 0 | 0.534 |
| 14053B110 | none | 6 | 6 | 0 | 0.259 |
| 14057BA50 | none | 6 | 6 | 0 | 0.546 |
| 1405CE4F0 | none | 3 | **0** | - | - (no replayable case) |
| 140687310 | none | 33 | 33 | 0 | 0.844 |

- **PROVEN: 14 of 16, 142 replayed calls, 0 mismatches.** Not replayed: 1404F6C30 (the team tick, 6 calls) and 1405CE4F0 (3 calls): every case excluded by the driver; its note gives no reason. Open 4.
- Per subsystem: the ledger tags only the team tick (AI). The other 15 carry no subsystem tag in `docs/re/LEDGER.md`, so a presentation / cutscene / AI-core split cannot be stated from the ledger. The camera and HUD entries reached 0 impure functions, so they contribute nothing to replay.
- Own-block coverage over the 14: four are at or above 0.5 (140529490 0.696, 14052A820 0.534, 14057BA50 0.546, 140687310 0.844); the rest are below. Coverage is not a gate.

## 5. Changes (commit `58634146`, `tools/dev/translate/frame_trace.py` only)

- `--root` is now `h` (default) / `entry` / `frame` (PARKED: the pump's Frame needs producers, docs/re/pes-re-liveroster.md).
- `--root entry --entry VA --reg name=value` (repeatable): calls `VA` once per frame with explicit registers, on the snapshot's live memory. Register names are checked against the emulator's register table.
- Earlier commit `fc03afd5` (FrameRoot): TEB, import models, `run.py` `_icount`, snapcase TEB, snapdiff fixes.

## 6. Verification

- Camera update, `rdx = 0`: stop at `0x1408A2169` (reproduced). `rdx = 0x7FF5FFDFAB50`: 2 frames, clean.
- Radar update `0x1420E2910`, `rcx = 0x1F713E70`: 2 frames, clean.
- H-leaf replay: 16 functions, `snapdiff.py` with the impure spec directory; 570 s.
- Scans (scratch): `scan_stacks.py` (10 snapshots), `vt_find.py` (5 snapshots), `ptr_scan.py` (2), `callgraph.py` (158,648 pdata functions), `stack_chain.py`.
- PII: grep for machine paths and machine paths over `frame_trace.py`: clean.
- Not run: the gate, unit tests, mutations, `check_pii.sh` from the main checkout.
- Note: detached (`( … &)`) background processes were killed when their shell ended; the scans and replays were re-run as tracked jobs.

## 7. Open items (numbered)

1. **Camera context `rdx` (PES's real value) is OPEN.** The live pump record is an assumption (INF). In FullSnap snap1 the in-play holder `0x360B9FD0` had `+8 = 0x33`, so `0x140886780`'s update path was closed. Next: a snapshot where that holder is in mode 1..6, then emulate `0x140886890` (the holder dispatcher, `rcx = holder`).
2. **HUD owner OPEN:** the object holding the radar and scorebug (`0x2C11A5B0`, `0x2C11A478` in FullSnap snap1) is not identified. `0x142608738` is a camera data table, not the HUD class. Next: the region's stride-`0x40` records and their writer.
3. **`0x141F73707` (FixDemo candidate, stack of snap4 thread 1092):** unnamed; liveness unverified. Next: its static callers and a cutscene snapshot where it is on a stack.
4. **1404F6C30 and 1405CE4F0: not replayable** (driver exclusion; reason not in the note). Next: read the driver's exclusion for these two.
5. **Subsystem tags:** the ledger tags only the team tick; presentation / cutscene / AI-core PROVEN counts need tags on the 15 untagged functions.
6. **Pump root parked** (`--root frame`): unchanged.
7. **Gate:** `run.py` and `frame_trace.py` changes touch the shared harness; Main runs the gate before merge.
