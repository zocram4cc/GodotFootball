# FrameRoot: snapshot TEB as gs base, pump frame root, reach and replay (26-10-26)

Branch `FrameRoot` (from `origin/NoFloor` 7136fbb1), pushed as `fc03afd5`; `git ls-remote origin FrameRoot` confirms `refs/heads/FrameRoot` at `fc03afd5`. Gate not run (Main gates). Scratch is `<workdrive>/gf-FrameRoot-scratch/` (PES-derived, not committed). No new SIGSTOP snapshot was taken this session: all runs read the existing `snap4-163107` (pid 53052, MainSnap, play=2, per=3, frame 36836). No live reads.

## Result

- **TLS: modelled.** gs base is the snapshot's own main-thread TEB: `0x3A000`, Wine thread id 36 (process 32 + 4). Its TLS slots (`TEB+0x1480`) and `ThreadLocalStoragePointer` (`+0x58`) are the game's real values. No new snapshot page was needed: the TEB page is in the snapshot's rw mappings.
- **Frame root: partly.** The per-frame root is the pump's `Frame` method: vtable `0x142AC1D38` slot 4 (`0x141B3D8E0`, CONFIRMED disassembly: gate `0x141B3F140`, full pump `0x141B3E060` with the frame delta in xmm1, then `[pump+0x417C]++`). The pump object is at `0x12F19C0` in this snapshot (vtable scan, CONFIRMED); its queue container `0x7FF5FFDE0010` matches the decode. **The caller of that Frame method is not found** (see Open 1).
- **Frame-root run, 50 frames, snap4: no stop, but reach 0/50.** No team tick (0 entries), no impure entry. The drain never dispatches: the loop at `0x141B3E146` exits only on a nonzero wait result, and the record path needs three signalled waits (`pump+0x4198 >= 3`). The replay has no producers to signal them, so nothing is dispatched. The camera (`0x14087F280`) and HUD (`0x1420E2910`) frame-0 stops are therefore **not reached** in this run, so they are not evaluated here.
- **Leaf run (H step), 3 frames, snap4 (`--root h`): reach 16/50, 6 team ticks, 151 impure calls, no stop.** Not like-for-like with the old runs (different snapshot and frame count): FrameTrace 1/50 (missing TEB, frame-0 stop), FullSnap 20/50 (snap1, 50 frames, TEB stubbed).

## 1. Changes (commit `fc03afd5`, 5 files)

| File | Change |
|---|---|
| `tools/dev/translate/frame_trace.py` | `main_teb(sm)`: TEBs are pages whose `+0x30` equals their VA; the process is the `ClientId.Process` most TEBs share; the main thread is its lowest tid (`main_teb` returns `(0x3A000, 36)`). `map_teb(uc, lazy, teb, stack_top)` uses the snapshot TEB as gs base and sets `StackBase` to the emulated top and `StackLimit` to 0. Fixed constant `TEB_BASE` removed. `CallDelta(..., teb)` takes the fixed TEB page. `--root frame` (default: pump `Frame`, `--pump 0x12F19C0` checked against its vtable, `--frame-dt 17544` µs, INF from pes-re-advance) or `--root h` (leaf). `record.json` carries `teb`, `teb_tid`, `root`. New `qpc` model kind. |
| `tools/dev/translate/import_stubs.json` | Models with evidence: `WaitForSingleObject` = 258 (WAIT_TIMEOUT); `GetCurrentThreadId` = 36; `QueryPerformanceCounter` = fixed 0 (**model, not evidence**); `TlsGetValue`/`TlsSetValue` on the TEB slots (indices < 64). |
| `tools/dev/diffharness/run.py` | `mu._icount = 0` at build. The code hook increments it, and it was never set: every run stopped on its first instruction. **Shared harness: the gate must run.** |
| `tools/dev/diffharness/snapcase.py` | Replay sets gs base from the trace's `record.json` `teb` (was `ft.TEB_BASE`). Traces without `teb` must be regenerated. |
| `tools/dev/translate/snapdiff.py` | Tool bug 1 and bug 2 below. |

No TEB capture was added to `full_snapshot.py`: the snapshot already holds the TEB page.

## 2. Pump frame root: why reach is 0

- `0x141B389D0` (wait helper, CONFIRMED): `WaitForSingleObject(handle, 0)`; result 0 returns 0, which **loops** the drain at `0x141B3E146`; any nonzero result exits (`0x18000001` for `WAIT_TIMEOUT`).
- With the first model (return 0), the drain ran 2,499,997 `WaitForSingleObject` calls in one frame and hit the instruction budget (`did not return`). So 0 is the wrong value for a single wait. The earlier pes-re-pump §5 note that suggested 0 is wrong for this path.
- With 258, the drain exits at once: the record path then runs only if the pump has three signalled waits. In 50 frames `WaitForSingleObject` was called 50 times (once per frame), all `WAIT_TIMEOUT`, and nothing dispatched.

Stops before the models were settled, in order: `WaitForSingleObject`, `GetCurrentThreadId`, `QueryPerformanceCounter`, `TlsGetValue`. Each now runs without a stop for 50 frames. No new import stop appeared in 50 frames; `TlsSetValue` was never called.

## 3. Reach (snap4)

Leaf H run, 3 frames, `impure_reached` (16 of 50; ledger status column from `docs/re/LEDGER.md`; only the team tick carries a subsystem tag there, the rest do not):

| VA | ledger status |
|---|---|
| 1404405D0 | decoded |
| 1404F6C30 | AI (team tick, pes-re-teamtick.md §2) |
| 1404F8C70 | decoded |
| 1404FA720 | decoded |
| 1404FB2A0 | ported |
| 1404FE9C0 | ported |
| 1404FEDC0 | ported |
| 140509A40 | undecoded |
| 14051B8B0 | ported |
| 140529490 | decoded |
| 14052A190 | decoded |
| 14052A820 | decoded |
| 14053B110 | decoded |
| 14057BA50 | decoded |
| 1405CE4F0 | ported |
| 140687310 | decoded |

Pump root, 50 frames: 0 of 50. Subsystem split (presentation, cutscene, AI and core): not computed; no impure entry happened in the pump-root run.

## 4. Replay (snapdiff, leaf trace snap4)

Owner rule: a function is PROVEN when every replayed call is bit-exact; coverage is recorded, not a gate.

| VA | calls | replayed | mismatches | own-block coverage |
|---|---|---|---|---|
| 1404FB2A0 | 6 | 6 | 0 | 0.375 |
| 14052A190 | 28 | 28 | 0 | 0.333 |

PROVEN: 2 functions (34 calls, 0 mismatches, no instruction-class mismatch). Not replayed: 14 reached functions (budget) and the pump-root run (no entries). The 50-frame leaf run was not replayed.

## 5. Tool bugs (PresReplay)

1. **`build_closure` skipped callees of existing `pes_fn_*.cpp`.** Fixed: an existing file's text is scanned for callee `pes_fn_` references, as a fresh translation's is. Not exercised by a run: the new out-dir had no pre-existing files. No unit test added.
2. **`--only` rejected `0x` VAs.** Fixed (`norm_tag`). Verified: `--only 0x1404FB2A0 0x14052A190` ran both functions.

## 6. Verification

- `py_compile` on `frame_trace.py`, `snapdiff.py`, `snapcase.py`: pass. `import_stubs.json` parses.
- Runs: TEB scan (`teb_find.py`, `teb_threads.py`, `teb_ranges.py`, `thread_roots.py`, `stack_chain.py`, `vt_find.py`, `callgraph.py` over 158,648 pdata functions); frame-root smoke (1, 3 and 50 frames); leaf run (3 frames); snapdiff on two functions. All in scratch.
- PII: grep for machine paths over the five files: clean. `tools/check_pii.sh` not run from the main checkout.
- No new unit tests (harness tooling). No mutations run.

## 7. Open items (numbered)

1. **The frame root above the pump (the caller of `0x141B3D8E0`): OPEN.** No direct E8 caller of `0x141B3D8E0`; it is reached through vtable dispatch. Thread roots: main thread (tid 36) root `0x14159F150` (stack depth ~81, in UI/presentation-looking code, no pump or H frames at snap4); pump-worker family rooted at `0x141B376D0` (17 threads; its loop calls objects' slot 2/3, not slot 4). Next: find the object whose slot 4 is called (`call [reg+0x20]` sites in the pump-worker family or the main-thread chain), or trace it at runtime with a frame-stack watch.
2. **A pump drain model with signalled waits (design decision):** the pump needs three signalled waits per frame (`pump+0x4198 >= 3`, gate `0x141B3F140`). Options: a stateful wait model (counter, three signals per frame), or run the producers. This is the step that unblocks reach.
3. **QPC is a fixed clock (0): a MODEL, not evidence.** Frame timing from these runs is not PES's. Replace with a clock model when item 2 is decided.
4. **Camera and HUD: not reached.** The pump root does not reach them; the leaf H run does not either (no entry in its 16). FixDemo manager (`0x142BA6800`) is absent in snap4 (cutscene-time snapshot needed).
5. **TLS pointer target unverified:** `TEB+0x58` of tid 36 is `0x173950`; the game's TLS block is not identified. No stop from `gs:[0x58]` reads in 50 frames.
6. Replay the other 14 reached functions and the 50-frame leaf run; record the mismatches by instruction class if any.
7. **Gate:** `run.py` and the import models change the shared harness; Main runs the gate (`PES_EXE` and `GF_DIFF_PYTHON` set) before merge.
