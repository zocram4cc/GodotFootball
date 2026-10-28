# Rendezvous snapshots

A complete snapshot of the running PES taken **at the entry of a real call**, with that call's registers,
so the call can be replayed exactly in emulation (`tools/dev/translate/frame_trace.py recorded`) and the
translated function proven against it. Design and history: `docs/re/pes-re-rendezvous.md`,
`docs/re/pes-re-rendezvous2.md`.

How it works: `rv.dll`, loaded into PES by a sider Lua module, patches a 14-byte jump at each configured
function entry. On a sampled call the hook saves the full register file (GPRs, XMM0-15, MXCSR, RFLAGS, the
TEB/gs base, the thread id) to a shared page, writes `capture_<seq>.txt` and parks the thread. The watcher
SIGSTOPs the game, reads all of its memory plus the shared page, resumes it and writes `release_<seq>`;
the thread continues into the original code. Nothing is written into the game's memory beyond the hooks.

Snapshots and the hook config contain PES bytes: they stay outside the repo.

## Files

|file|what|
|---|---|
|`rv.c`|the hook DLL (pins itself on init so a collected `ffi.load` handle cannot unload it)|
|`rv_pin_test.c`|regression for that unload: exit 0 = still mapped after `FreeLibrary`|
|`build.sh`|builds both with mingw-w64|
|`rv_hooks.py`|image reader and the hook-window check (refuses windows with branches or rip-relative operands)|
|`hook_plan.py`|static plan: targets to entries, window check, scan for branches into each window, writes `pes_hooks.cfg`|
|`rendezvous.lua`|the sider module that loads the DLL and installs the hooks|
|`install_rendezvous.py`|installs/undoes the module, DLL, config and the `sider.ini` line (plan-only unless `--apply`)|
|`full_snapshot_rv.py`|the snapshot reader (`rv_take`)|
|`rv_watch.py`|the watcher: one snapshot per capture, everything chowned to the run dir's owner|

## Use

1. **Build** (needs `x86_64-w64-mingw32-gcc`):
   `sh tools/dev/rendezvous/build.sh OUT` and check the pin:
   run `OUT/rv_pin_test.exe` under Wine with three arguments (the Windows paths of `OUT/rv.dll`, an empty
   config file and a run directory ending in a backslash); it exits 0 when the DLL stays mapped.
2. **Plan the hooks** against your dump: add targets to `TARGETS` in `hook_plan.py`, then
   `python3 tools/dev/rendezvous/hook_plan.py $PES_EXE PLAN_DIR`. Every target must print `PASS` and
   `plan.json` must list no `real_branches_into_window`. Set the sample rate per line in
   `PLAN_DIR/pes_hooks.cfg`: `hook NAME VA LEN FIRST EVERY BYTES` takes calls 1..FIRST, then every EVERY-th.
3. **Check the live bytes** match the plan (read-only `/proc/<pid>/mem` of the running game) before
   installing; the DLL also refuses any window whose bytes differ.
4. **Install** (sider must have `lua.enabled = 1` and `luajit.ext.enabled = 1`):
   `python3 tools/dev/rendezvous/install_rendezvous.py --root <PES21 dir> --dll OUT/rv.dll --cfg PLAN_DIR/pes_hooks.cfg --module tools/dev/rendezvous/rendezvous.lua`
   prints the plan; add `--apply` to write it. `--undo` restores `sider.ini` byte for byte.
5. **Start the watcher** before the game:
   `sudo -n python3 tools/dev/rendezvous/rv_watch.py --run '<PES21 dir>/rendezvous/run' --out SNAP_DIR`
6. **Start the game.** sider loads at game start, so: the bot's pad daemon first (if a bot drives the game),
   then `sider.exe`, then `PES2021.exe`, all in the game's Wine prefix. `sider.log` shows
   `rendezvous: N hook(s) installed`; `rendezvous/run/rv_dll.log` shows `rv_init: N of N hooks installed`.
   A bot that watches for frozen frames needs to tolerate the pauses (rigbot: `RIGBOT_ALLOW_FROZEN_FRAMES=1`).
7. **Replay** a capture: each `SNAP_DIR/seqNNN/` holds `mem.bin`, `index.json` (its `rv` record names the
   hook) and `call.bin` (the registers); `frame_trace.py recorded SNAP_DIR/seqNNN` (on branch `Rendezvous`
   until it lands with its harness changes) runs the call from its real entry state.

Pauses take about 1-7 s each on a 4 GB process. The DLL releases the thread on its own after the module's
`TIMEOUT_MS` (60 s) if no watcher answers; the watcher aborts a read that passes its 60 s pause cap and
releases the game at once.

**Slim a capture** (after a replay has proven it): `python3 tools/dev/rendezvous/slim.py --hooks <run>/hooks.cfg`
(default: every seq dir of `live/` and `live2/` older than 10 minutes; or pass the seq dirs). It replays each capture's
recorded call, logs every `mem.bin` page read, and rewrites `mem.bin` as a sparse file of the same size and offsets that
keeps only those pages plus one page either side. The slim copy is replayed again and must read only kept pages and
return byte-identical output before the full file is deleted; otherwise the full capture is restored and reported.
Run it when a capture's hook has no open replay question. Caveat: the full file is deleted after verification, so a
later replay that reads a page outside the kept set cannot run on that capture. Keep the replay's `--out` trace as the
record of what the capture proved.
