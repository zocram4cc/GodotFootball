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

**Slim a capture** (footprint): `GF_CAPTURE_ARCHIVE=<archive root> python3 tools/dev/rendezvous/slim.py --hooks <run>/hooks.cfg --scratch SNAP_ROOT` (every seq dir of `SNAP_ROOT/live/` and `SNAP_ROOT/live2/` older than 10 minutes; or pass the seq dirs). Per capture it replays the recorded call with the GF stand-ins off (`frame_trace.py recorded --no-stubs`): PES's own image code runs from the hook entry to its return, and every page of `mem.bin` it reads is kept, with the identity pages and one neighbour either side. The replay must return cleanly (an unmodelled import, a fault or the step cap keeps the capture full). The slim copy must read only kept pages and return byte-identical output. Then the full `mem.bin` is packed, copied to the archive and verified byte for byte, and the packed name becomes a link to the archived copy. Nothing is deleted. `index.json` `slim` lists every dropped range and the archive-relative path of the full copy. A replay that reads a dropped page is refused, and the refusal names that copy and `slim.py --restore <capture>`, which unpacks it back.

**Keep set and expiry.** `auto_slim.py` never slims the first two in-play captures of each hook in a launch (in play: `MI+0x116C == 1`, cached in `index.json` as `in_play`), nor a hook or a `pidN/seqM` capture named in a keep file. `expire.py --live2 DIR --record 'GLOB' [--cited FILE] [--days 14]` lists the captures that no results record and no open lane cites and that are older than 14 days, with their bytes. It is a dry run and deletes nothing.

**Pack a capture that must stay full** (cited, or slim refused) but that nothing reads now:
`python3 tools/dev/rendezvous/capz.py pack DIR...` (a seq dir, or a pid dir for all of its captures). `mem.bin` becomes
`mem.bin.zst` (zstd -3, about 3.7x smaller, ~4 s per capture); the frame checksum is tested before `mem.bin` is removed.
Slim captures are left alone. A tool that reads a packed capture needs it unpacked first: `capz.py unpack DIR...` (sparse
output, a few seconds). It refuses an archived slim capture: its full copy comes back only through `slim.py --restore DIR`,
an explicit act. `capz.py status DIR...` prints packed / full / slim (an archived slim capture is slim).
`auto_slim.py --pack-after SECONDS` packs
captures still full that long after landing, so kept captures stop pushing the disk under the watcher's floor.
