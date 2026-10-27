# PES21 rendezvous snapshots: loader route, hook design, tested prototype, emulator start

Status: prototype built and tested under Wine on our own test exe. **Nothing was loaded into PES21**, and PES (pid 53052) was not read, signalled or written during this work. Prototype and all outputs: `<workdrive>/gf-Rendezvous-scratch/` (see §9).

Labels: CONFIRMED (seen in files or in a run here), INF (reasoned, not run), OPEN (needs an experiment).

## 0. Summary

- **Loader (recommended): sider module.** sider already runs in the game. A Lua module in `modules/` calls `ffi.load` on `rv.dll` and `rv_init`. Two new files, one `sider.ini` line, fully reversible. Sider must stay enabled. Not yet verified in sider (§1, INF).
- **Hook:** 14-byte `jmp [rip+0]` at a function entry, into a per-hook trampoline. It saves all 16 GPR, 16 XMM, MXCSR and RFLAGS, then samples (first N calls, then every K-th). A sampled call parks the thread in a spin until a release file appears or a timeout passes.
- **External side:** SIGSTOP → read the shared page and the whole process → SIGCONT → read code pages → release. The parked thread stays parked until the snapshot is complete. Pause ≈ 51 ms and game stall ≈ 99 ms per sample for a 50 MiB test process (median, 82 samples).
- **Correctness (tested):** every variant ends with the same final state hash as the no-hook run. Each of 82 snapshot calls, replayed in unicorn from its snapshot, reproduces the game's own RAX and result word (82/82).
- **Crash recovery (tested):** the game was stopped for 3.0 s (longer than the 1.0 s DLL timeout), then continued without a release. The DLL timed out 2.2 ms after the continue, and the game finished with the correct hash.
- **Emulator start:** `frame_trace.py` (origin/NoFloor) already has the register layout (`entry_record`, `<49Q>`), lazy snapshot memory and call-time deltas. It lacks a "run this recorded call at its real rsp" entry point. Sketch in §7.

## 1. Loading into PES21

### 1.1 What the install has (CONFIRMED)

- `sider.exe` 7.1.0 (`sider.log`: "Init DONE", `start.game = steam://rungameid/1259970`). `sider.dll` is in the game directory.
- `sider.ini`: `lua.enabled = 1`, `luajit.ext.enabled = 1`, and `lua.module = "<file>.lua"` entries. Each name is a file in `PES21\modules\` (CONFIRMED by listing).
- `sider.dll` contains the strings `LuaJIT 2.1.0-beta3`, `local ffi = require('ffi')`, `ffi.cdef`, and `hook cache` / `Hooking Present`.
- Existing sider modules get a `ctx` with `ctx.sider_dir` and `ctx.register`, plus globals `log` and `memory` (CONFIRMED in `modules/etrace.lua`, `tracer.lua`). `gc64test.lua` documents that `luajit.ext.enabled = 1` enables FFI.
- `sider.exe` imports `?setHook@@YAXXZ` from `sider.dll`. `sider.dll` imports `SetWindowsHookExW`. The game receives `sider.dll` through a Windows hook (mechanism INF; the imports are CONFIRMED).
- Anti-cheat: no EasyAntiCheat, BattlEye, Vanguard or similar file in `PES21\` (CONFIRMED by listing). `steam_api64.dll` is present (CONFIRMED). The install is already modified (`ALLSTYLEPES2021.exe`, `patch_pes.py`), which is CONFIRMED.

### 1.2 The game's import table

- On-disk `PES2021.exe` import directory (CONFIRMED with `objdump -p`): `advapi32, anselsdk64, api-ms-win-crt-*, bcrypt, crypt32, d3d11, dinput8, dsound, dxgi, gdi32, iphlpapi, kernel32, mfplat, msvcp140, msvcrt, ntdll, ole32, oleaut32, secur32, shell32, shlwapi, steam_api64, user32, vcruntime140, winhttp, wininet, winmm, wlanapi, ws2_32, xinput1_3`. `version.dll` is **not** imported by the game.
- `pes_full.bin` (unpacked image): the import directory at RVA `0x1B9625B8` does not parse (the first descriptor is garbage). So the import list comes from the on-disk exe, not from `pes_full.bin`. CONFIRMED that it fails to parse; why is OPEN (not checked against the unpacker's scripts).

### 1.3 Route A (recommended): sider Lua module

Files (scratch copy: `loader/rendezvous_sider.lua`):
1. `PES21\rendezvous\rv.dll`, `PES21\rendezvous\hooks.cfg` (from `rv_hooks.py` on the PES image; the expected-bytes field is checked before patching), and an empty directory `PES21\rendezvous\run\`. The DLL writes its capture, release and done files there and does not create it.
2. `PES21\modules\rendezvous.lua` (the sketch).
3. One line in `sider.ini`: `lua.module = "rendezvous.lua"`.

The module calls `ffi.load(ctx.sider_dir .. "rendezvous\\rv.dll")`, then `rv_init(cfg, run_dir, 5000)`. Undo: remove the `sider.ini` line, delete `modules\rendezvous.lua` and `rendezvous\`. Sider stays enabled; it is the loader.

- INF: `ffi.load` works inside sider. The test that settles it is an owner-approved load of a no-op `rv.dll` whose `rv_init` returns 0 and logs a line.
- Editing `sider.ini` changes the owner's config and needs approval.

### 1.4 Route B (fallback): proxy DLL in the game directory

Windows loads a same-named DLL from the exe directory first. The game imports `winmm.dll`, `dinput8.dll` and `xinput1_3.dll`. Under Wine this precedence is **CONFIRMED** in the prototype (`loader/winmm_proxy.c`: `timeGetTime=4242` with the proxy beside the exe, `timeGetTime=99195089` in the control run without it, and the marker file written). For PES21 it is INF.

A proxy has to forward every export the game uses. The set is OPEN (needs the imported-symbol list for the chosen DLL). It also changes a DLL in the game directory, not only the sider config. Use it only if sider cannot load a Lua module.

## 2. Hook mechanism (`src/rv.c`, prototype)

- **Patch:** `FF 25 00 00 00 00 <abs64>` (14 bytes) at the entry, to a per-hook trampoline. Whole instructions covering at least 14 bytes. `rv_hooks.py` (capstone) refuses a window that contains a call, jump, return, or rip-relative operand. It writes the expected bytes to the config. `rv_init` compares them with the live bytes before patching and refuses on mismatch.
- **Trampoline:** `push r11; movabs r11, ctx; call [slot]; lea rsp,[rsp+8]; <displaced bytes>; jmp [rip+0] -> entry+len`. `slot` holds `rv_common`.
- **`rv_common` (asm):** saves 16 GPR, 16 XMM, MXCSR and RFLAGS into a 416-byte frame, calls `rv_handle(ctx, frame)` (MS x64, 16-byte aligned), then restores everything and returns with rsp = entry rsp. The frame offsets are checked with `_Static_assert`.
- **Sampling:** `count` increments on every call. Sampled when `n <= first`, or `(n-first) % every == 0` (`every = 0`: no later samples). Never-sampled hooks cost one locked increment plus the save/restore.
- **Shared page** (`rv_page_t`, at the page VA written into the capture file): header with magic, state (0 idle, 1 captured, 2 released, 3 timeout), hook index, seq, hook VA, TEB (gs base), tid, MXCSR, RFLAGS, QPC frequency, QPC capture/release times, call count, then at `0x100` the 392-byte `CALL_REC` (`<49Q>`, the same layout as `frame_trace.entry_record`).
- **Rendezvous:** one at a time (a sampled call that finds another rendezvous in progress is not sampled). Files, all written with write-then-rename:
  - `capture_<seq>.txt`: seq, hook, VA, page, calls, tid, QPC capture time, QPC frequency.
  - the thread spins: `release_<seq>` present → released; `timeout_ms` elapsed (QPC) → timeout. Logs `TIMEOUT`.
  - `done_<seq>`: outcome and QPC release time. The driver reads this file. It does not read the shared page after release, because the next capture overwrites the single page (a bug found and fixed in testing).
- **Why files for the release, not writes into PES:** the owner's rule is read-only `/proc/<pid>/mem` plus SIGSTOP/SIGCONT. The external side never writes into the process; it creates `release_<seq>` in its own directory.
- **Limits in the prototype:** no exit hook (the return value is not captured by the DLL); x87 state not saved; one shared page, so one rendezvous at a time; the trampoline page is RWX.

## 3. External side (`src/full_snapshot_rv.py`, copy of `full_snapshot.py` + `rv_take`)

Per capture, the order is fixed:
1. `kill -STOP pid`; read the shared page; require state 1 (captured), else abort.
2. Read `/proc/pid/maps`; read every readable mapping except the deferred ones (read-only private code and rodata) through `process_vm_readv`. Unreadable pages become holes.
3. `kill -CONT pid`.
4. Read the deferred mappings. The hooked thread is still parked, so the process cannot exit while they are read. (A first version released before this step; the game reached its last call and exited during the reads. Fixed.)
5. Write `release_<seq>`, in a `finally`, on every path. The game is never left parked.
6. Write `mem.bin`, `index.json` (mappings, holes, pause, rv metadata) and `call.bin` (the `CALL_REC`).

The pause covers steps 1–3. Pause time and holes are logged to `pauses.jsonl`, as in the existing script.

## 4. Test setup (all under Wine, own prefix)

- Test exe `build/rv_test.exe` (mingw 16.2 / Wine 11.20). Fixed base `0x140000000` (`--disable-dynamicbase`). `sim_tick` (VA `0x140001580`) is a deterministic FP/integer loop with a 512K-entry result ring. `g_sim` is at `0x1400090A0`.
- Own `WINEPREFIX` (`scratch/prefix`). The owner's PES prefix (`<local> PES 2021`) was not used, and no wineserver was touched. Wine's own server socket still lives in `/tmp/.wine-1000` (Wine does not honour `TMPDIR`; this is Wine's location, not ours).
- Unicorn 2.1.0 from `<workdrive>/gf-DiffHarnessRE-scratch/venv` (the system Python has no unicorn).
- ptrace_scope = 1; the test process is our child, so `/proc/<pid>/mem` reads need no sudo.

## 5. Results (CONFIRMED, from `run/summary.json`)

| Check | Result |
|---|---|
| Final state hash, 400k calls: no-hook ×3 | `e9a09ecd724545b2` ×3 |
| Hooked, never sampled ×3 | same hash ×3 |
| Hooked, 82 samples, released | same hash |
| Hooked, 82 samples, no external (timeouts, 300 ms) | same hash; 82/82 outcome 3 |
| Emulation of sampled calls (unicorn, from snapshot, RAX and ring word vs game) | **82 of 82** match |
| Crash test: stopped 3.0 s past a 1.0 s timeout, then CONT, no release | timeout 2.2 ms after CONT; same hash |
| Loader precedence (proxy `winmm.dll` beside exe) | proxy loaded; control loads system winmm |

Timing (test process ≈ 50 MiB, 121 mappings, 6 holes = unreadable pages, not identified):

| Quantity | Median (min–max) |
|---|---|
| Pause, SIGSTOP → SIGCONT (82 samples) | 50.8 ms (46.9–53.6) |
| Game stall, capture → release seen (82 samples) | 98.7 ms (94.1–108.2) |
| Changed pages vs previous snapshot (81 pairs) | 0.24 % (0.13–0.28) |
| Compare time, consecutive pair (both dumps read from page cache) | 10 ms (9–11) |
| Unicorn replay per call | 2 ms (2–5) |
| 400k-call run, no-hook (3 runs, mean) | 633.9 ms |
| 400k-call run, hook never sampled (3 runs, mean) | 636.7 ms (+2.8 ms, ≈ 7 ns/call, within the ±6 ms spread) |

Stall = pause plus the deferred code read. Releasing before the deferred read would shorten the stall, but reopens the exit race; keep it parked.

**Reference from PES (CONFIRMED, existing logs):** `gf-MainSnap-scratch/stop/pauses.jsonl` shows full-process snapshots of 4.19 GB and 2374 mappings with a 1.5 s pause (written 2026-10-26). A rendezvous on PES would therefore stall the game about 1.5–2 s per sample, plus deferred reads. INF until measured.

**Changed-page fraction:** 0.24 % of pages change between consecutive 50 MiB snapshots. A delta dump would write about 1/400 of the bytes. The read cost (the pause) does not shrink, so delta storage only saves disk and post-processing time. Not implemented.

## 6. Risks

- **Anti-cheat / integrity:** none seen in the install (§1.1). The game is already modded and runs under sider, which already injects code. INF that no check would react to a sider-loaded module.
- **Timing effects on the bot:** each sample stalls every thread that the parked thread blocks. If the hooked function is on the main thread, the whole game stops for the duration (1.5–2 s on PES, INF). Keep the sample rate low (for example one per minute or less). The game's own clock may be affected if it compares wall-clock time: OPEN.
- **Crash recovery:** tested for "external stopped the game and then died" (the watchdog CONT path). After CONT, the DLL's clock has already passed the timeout, so it releases the thread at once and logs `TIMEOUT`. The game continues. A watchdog must still send SIGCONT when the external dies (a plain `kill` leaves the game stopped). If the game exits while captured, the driver logs a finding.
- **Install race:** patching the entry is a 14-byte write. Another thread executing that function at the same moment could execute a torn instruction. Install before the function can run, or while the process is stopped. Not tested on a multi-threaded target (OPEN).
- **Displaced code:** the check covers only the window at the entry. Branch targets from elsewhere into the window are not checked; that needs a whole-image scan (OPEN).
- **Holes:** the test has 6 unreadable pages per snapshot (not identified; contiguous in one mapping). PES dumps already have holes (`pauses.jsonl`). An emulation that touches a hole stops; that is the correct behaviour (`emu_call.py` refuses it, and an earlier run refused on a hole at the entry page).
- **Single-slot state:** one shared page and one rendezvous at a time. Hooks on several threads will lose samples while another is parked (skipped, not queued). Acceptable for a first pass.
- **Return values:** without an exit hook, the return value is not captured. The test gets it from its own ring. For PES, the next sample's dump shows memory after the call (with the other threads' changes in between, so it is exact only for single-thread state). Exit hook: not built (OPEN).
- **Memory protection:** the trampoline page is RWX. Acceptable for the prototype; for PES, use RX after writing (not done).

## 7. Emulator side: `frame_trace.py` on origin/NoFloor (sketch, not committed)

What exists (CONFIRMED by reading `tools/dev/translate/frame_trace.py` on origin/NoFloor):
- `entry_record(uc)` writes `CALL_REC = <49Q>`: 16 GPR (`rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15`), 16 XMM as lo/hi, return address at `[rsp]`. `rv_page_t.rec` in the prototype is the same layout, so `call.bin` feeds it directly.
- `fs.SnapMem` (lazy snapshot memory), `LazyMap` (maps pages on first touch), `CallDelta` (call-time memory delta), `map_teb`, and `run_entry(uc, entry, regs, ret, budget)`.
- `run_entry` is not enough: it rebuilds rsp (`stack_top - 8`) and writes its own sentinel return. A recorded call must keep its real rsp and return address.

Sketch:
```python
# frame_trace.py (origin/NoFloor) -- sketch
def run_recorded_call(uc, rec, entry, budget):
    """Start one recorded call at its entry with its real registers, rsp and return address.
    rec: the 49 values of call.bin (entry_record layout)."""
    names = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
    for i, r in enumerate(names):
        uc.reg_write(rn.reg_const(r), rec[i])
    for i in range(16):
        uc.reg_write(xc.UC_X86_REG_XMM0 + i, rec[16 + 2 * i] | (rec[17 + 2 * i] << 64))
    uc.emu_start(entry, rec[48], count=budget)   # stops at the real return address
```
Plus, for a `--call SNAP_DIR` mode:
- read `call.bin` and `index.json["rv"]` (`entry_va`, `teb`, `flags`, `mxcsr`);
- set `GS_BASE` to the real TEB from the record and skip the synthetic `map_teb` (the real TEB is in the snapshot);
- set RFLAGS from the record;
- if the process holds a rendezvous patch at `entry`, write the original bytes first (the prototype does this from the config's expected bytes; PES has no hooks, so this step is not needed there).

`emu_call.py` in the scratch tree is the working version of this: a page-at-a-time lazy map, the same register load, `emu_start(entry, ret)`, and a check of RAX and the ring word. It ran 82/82 correctly. MXCSR is not set in unicorn (the test does not change it). OPEN: verify MXCSR in unicorn 2.1 before relying on FP rounding modes.

Ground truth for PES: the test uses its own ring. For PES, a return value needs an exit hook (§6), or a memory effect checked against the next dump (valid only for state that no other thread writes).

## 8. Open items and decisions for Main

1. **Owner approval, before any PES action:** (a) test load of a no-op `rv.dll` through sider (settles INF in §1.3); (b) `sider.ini` edit; (c) the sample rate.
2. **Hook targets for PES:** a list of entry VAs with their thread. The config generator (`rv_hooks.py`) and the expected-bytes check are ready; the VAs must come from repo docs with PES-cited evidence. No PES bytes are in this report.
3. **Exit hook** for ground-truth return values (OPEN).
4. **Whole-image branch-target scan** for displaced windows (OPEN).
5. **Install race** test on a multi-threaded target (OPEN).
6. **MXCSR in unicorn** (OPEN).
7. **Emulator change** in `frame_trace.py` (§7): not committed here; no repo edits were made.

## 9. Files (scratch: `<workdrive>/gf-Rendezvous-scratch/`)

- `src/rv.c`: hook DLL (`rv_init`, `rv_common` asm, `rv_handle`, shared page, files).
- `src/rv_test.c`: test exe (`sim_tick`, ring dump at exit).
- `src/rv_hooks.py`: config generator with the capstone window check.
- `src/full_snapshot_rv.py`: copy of `full_snapshot.py` plus `rv_take` (rendezvous snapshot). The copy's `seed` import is optional.
- `src/emu_call.py`: unicorn start of one recorded call from a snapshot.
- `src/rv_run.py`: driver (cases base, overhead, rdv, timeout; verification; delta stats).
- `loader/`: `winmm_proxy.c`, `proxy_check.c` (precedence test), `rendezvous_sider.lua` (sider module sketch, not installed).
- `build.sh` → `build/rv.dll`, `build/rv_test.exe`; `build/loader/`.
- `run/`: `summary.json` (full run), `hooks_sim.cfg`, `hooks_off.cfg`, per-case dirs, `rdv/snaps/` (82 dumps, about 4 GB; delete when archived), `crash/`.

Reproduce: `sh build.sh`; `python3 src/rv_hooks.py build/rv_test.exe run/hooks_sim.cfg sim_tick:3:5000`; `<workdrive>/gf-DiffHarnessRE-scratch/venv/bin/python src/rv_run.py --total 400000 --keep 100`. The Wine prefix is created with `WINEPREFIX=scratch/prefix wineboot -i`.
