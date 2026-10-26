# PES per-frame pump emulation (Pump, 26-10-26)

Read-only RE. Image `pes_full.bin` (offset = VA - 0x140000000). No PES bytes quoted (VAs + hashes only).
Labels: **CONFIRMED** = disassembly/capture re-checked this pass, **INF** = inference, **OPEN** = blocker named.
Tooling lives in `<workdrive>/gf-Pump-scratch/` (own copy of `frame_trace.py` + probes `probe*.py`, `disp*.py`, `hub*.py`, `ring*.py`, `run*.py`, `mabuild.py`, `wtest.py`, `det3.py`, `stop*.py`; ~150 requests used).

## 0. What was decoded (entry, signature, queue)

The real per-frame entry is the pump object at **`0x12F19C0`** (vtable **`0x142AC1D38`**, CONFIRMED by init-site
`lea rax,[rip+0xF84542]` at `0x141B3D7EF`/`0x141B3D869` resolving to `0x142AC1D38`/`0x142AC1DD0`, both vtable-shaped in
the image), with its queue container at **`+0x10` = `0x7FF5FFDE0010`** (vtable `0x142AC1C68`).
Pump call under emulation: **`0x141B3EA30(rcx=pump, dl)`** (small pump: condvar wait, drain up to `edx` records,
at most one dispatch on this path) and **`0x141B3EEF0(rcx=pump, edx)`** (drain path: waits, then walks up to `edx`
ring slots via the mini-dispatch **`0x141B3CEB0(rcx=pump=hub, rdx=record)`**). The full-frame pump is
**`0x141B3E060(rcx=pump, rdx, xmm2, xmm3)`** (CONFIRMED prologue `mov rsi,[rcx+0x10]`, `mov r12d,[rcx+0x4198]`;
callers at `0x141B3D914/0x141B3D98C/0x141B3DCFD/0x141B3DD55`). The ticket's `0x141B3EA30` signature
`rcx=self, dl=flush-flag` is CONFIRMED by disassembly (`mov rbx,[rcx+0x10]`, `test dl,dl`, slot reads at
`[rbx+0x40204/0x40208]`, record fetch `[rax+rbx]` with `rax=((idx&31)+0x4000)*16`, dispatch call `0x141B3EDD0`,
free via `[rbx+rax*8+0x40008]` + `lock xadd [rbx+0x40208]`).

- Container ring (CONFIRMED): 32 slots of 16 bytes at `C+0x40000`, record ptr = `[C+0x40000+(idx&31)*16]`,
  `idx=[C+0x40204]`, `count=[C+0x40208]`. Snapshot: `idx=18`, `count=2`, `R18` = live record
  (`[R]=0x30700400002` low dword **0x02 = record type 2**, the match record; `[R+8]` ≈ 45.0 sane timestamp double;
  `[R+0x20]=0x200104`: dispatch word **type 4, channel 0**).
- Dispatch (CONFIRMED): `0x141B3EDD0(rcx=hub, rdx=record)` walks `rsi=rdx+0x20`: `lo7` = type,
  `bits7-16` = channel (0 -> `+0x1020`, 2 -> `+0x820`, else `+0x20`), stride `[rsi]>>0x11`, type-1 link-follow,
  type-3 end; per record one call on the channel table then one on **`+0x1820`** indexed by type. The drain's
  mini-dispatch `0x141B3CEB0` is the same walk with hub = pump (CONFIRMED `mov rbp,rcx`).
- The 84-node circular list (`queue_order.json`, nodes `+0` next / `+8` prev, `+0x28` seq, `+0x20` target) is a
  **separate bookkeeping queue, NOT the dispatch's record stream** (INF): node seqs (`0x0402000d..`) as record
  words give type 13/channel 0/stride 513, whose first-listener call lands correctly (F2 body, reached under
  emulation with hub `0x2c694570`) but whose second-listener slot (`+0x1820[13] = 0x23`) is unregistered, and no
  hub shape validates all 49 match types against node targets (best score 4/49). The per-player records the ticket
  describes run through the **hub `0x2c694570`'s `+0x1820` table** only in the sense that that table holds the live
  registration (players 0-21 in `M+0x10` order at `+0x1820[29..50]` (CONFIRMED copy), H at `+0x1820[56]`
  (CONFIRMED), referee/analyze-shaped listeners around them); `0x2c694570` itself is a listener-registration hub,
  not the pump (its `+0x10` points at node/geometry data, not a container). Which hub the type-13-style records
  dispatch through remains OPEN (the `+0x1820` default for unregistered types is also OPEN).
- Stoppage snapshots (Main's `<workdrive>/gf-MainSnap-scratch/stop/`, snap1-162213 play=2 frame=22150): same pump
  VA/vtable, same container VA (`idx=0x200000008/0x100000002` low32s differ = mid-update), pump tables identical
  for type 2. No behavioral difference found (OPEN: deeper stoppage comparison not run).

## 1. How far the pump gets (message to Main: READ THIS)

**The pump runs cleanly but the match does not advance from it.** `0x141B3EA30(rcx=0x12F19C0, dl=1)` returns 0/None
with no listener call on this snapshot (ring has 2 pending type-2 records but this path's count logic drains
nothing observable). `0x141B3EEF0(rcx=pump, edx=2)` runs clean and dispatches the pending record(s) through
`0x141B3CEB0` to the type-2 first/second listeners (vtable `0x142AC17C0/0x142AC1800`, slot 2 **`0x140421F40`**, a
2-instruction leaf), once per call, `MI+0x1198` unchanged. Entering the mini-dispatch directly on the live ring
record reproduces the exact pair (`0x140421F40` x2 alternating first/second targets). The full-frame pump
`0x141B3E060` was NOT entered (needs `xmm2/xmm3` time-base args from `0x141B3F0D0/0x141B3F050`; OPEN).
No `MI+0x1198`, player-man, or MA-list change comes out of any pump path tried. **Frame advance is not there.**

**The H step DOES run the match tick (this is the working frame advance):** `0x14041ECF0(rcx=H)` from the complete
snapshot runs clean (`err=None`) through `140A49C50` (S init) -> `1404F6C30` x2 (team ticks, brains
`0x2c69ADD0/0x2c6A1BD0`) -> `140509170` x2 -> `1404405D0` (side planner) -> `1405069A0` -> `140A484F0` (teardown),
on both play=1 (frame 30381) and play=2 stoppage (frame 22150) snapshots. But it is **idempotent**: 3 back-to-back
iterations produce byte-identical player blocks, G header, list span, and `MI+0x1198` (deterministic, see §4).
Why it doesn't advance is OPEN (candidates: the `+0x28` second H-step arg — 0 on first call, `0x28` after —
selects a sub-frame; the H-step reads sequencing state (timers/counters) that the emulator leaves static; or the
real advance needs the full-frame pump's time-base first).

## 2. Frame advance: NOT achieved (honest status)

- `MI+0x1198`: 30381 before and after every run (play=1); 22150 ditto (play=2). Never advanced. (CONFIRMED reads)
- Player positions (`man+0x55C/+0x564` and full `0x4000` player blocks): byte-identical across iterations
  (hash `632de4f21c602f26` x3 runs). Note `man+0x55C/+0x564` read 0.0 in this snapshot for P0-3 (positions live
  elsewhere in the record; full-block hash covers the real fields whatever they are). (CONFIRMED)
- MA nearest-list span (`MA+0x22E4C`, 22x200): hash `fb15333c059f9bfd` before/after every H-step and pump run;
  **zero writes** with a write-hook on the span plus ball/rank/carrier neighbours, in both snapshots.
  The MA build (`0x140535C60` -> `0x140536FC0`) RUNS (both sides in play=1: brains `0x2c69EFD0/0x2c6A5DD0`;
  once in play=2) and MA consumers run (`0x140530320/0x140532F70`, `0x1405E6E80` x2), but the build only READS the
  list (24 read sites, 214 reads, top `0x1404F9C30` x86) and writes nothing in MA (`0x22E4C` span or whole
  `0x27000` MA range: 0 writes). The rank writer `0x140536FC0` runs yet writes nothing to `MA+0x24710`. The
  per-player driver (`0x140925750` for players 0-5, see §4) also writes nothing to the span. **S4's writer is not
  in any path exercised; no writer VA exists to name.** (CONFIRMED)
- EmuWatch's result (5 H-steps, 0 list writes) is reproduced exactly, including the list hash.

## 3. S4 writer: OPEN (not found)

Write-hook coverage: `MA+0x22E4C+4400` span + `MA+0x2482C` ball + `MA+0x2601C` carrier + `MA+0x24710+0x100` ranks
during H-steps (both snapshots), plus whole-`MA` (`0x27000`) watch: **0 writes, so no RIP/call-stack/frame-position
to report.** The build/consumer set that runs without writing: `140535C60/140536FC0/140530320/140532F70/
1406C0170(not reached)/1406CBF70(not reached)/140587100(nr)/1405B89F0(nr)/1405E6E80`. Next cheapest test (not run):
  single-step the MA build's rank-table store (`MA+0x24710`) with a read-hook to see if it reads-but-skips, and
  try the H-step with `rdx=0x28` on the FIRST call (the snapshot's live second-call shape).

## 4. Per-player chain: CONFIRMED running, in order, with a gate

`match::Player` slot 2 `0x140925750(rcx=player)` (vtable `0x1426329A8`, CONFIRMED) runs from the snapshot for every
player tried, reaching `0x140925470` (driver) -> `0x140928FC0` (AI decision) -> `0x1409257B0` ->
`0x140929A00` (gate `MI+0x11B0 == 0x16`) unconditionally (players 0-21 all reach `0x140929A00`, `err=None`).
Past the gate the chain is per-player: `0x14092A340 -> 0x14092C9F0 -> 0x1421019D0 -> 0x142110D30` (MainEval) for
players 3, 6(partial), 8, 9, 11, 12, 15 (deepest reach `0x142110D30` CONFIRMED for P3/P8/P9/P11/P12/P15); players
0, 1, 2, 4, 5, 7, 10, 13, 14 stop after `0x140929A00` (gate/stub-gated, no error). Player 15 then stops on
`stub_1415A17CE has unknown do=powf_xmm0` (a diff-harness `powf` libm stub the frame runner doesn't install;
harness-side stop, game-real code). In-order execution 0-21 was NOT demonstrated as a dispatched sequence (no
queue caller found; each player was entered manually in `M+0x10` order). No list write from any player leg.

## 5. Stops classified (harness-side = fixed in-scratch, not in repo)

1. `WaitForSingleObject` / condvar wait on the pump path: **engine call** (Wine threading). Model: return 0
   (`WAIT_OBJECT_0`). Evidence: pump prologue calls `0x1423D90E0`. Fixed in every `run*.py`.
2. `TlsGetValue/TlsSetValue`, `FlsGetValue/FlsSetValue`, `GetLastError/SetLastError`, `RtlEnter/LeaveCriticalSection`,
   `InitializeCriticalSectionAndSpinCount`, `FormatMessageA`, `WriteFile`, `RtlFreeHeap`: **engine calls**
   (return-0 models). Fixed in `run7.py` (+ descendants).
3. `VirtualAlloc` returning 0 -> later null-deref fault at `0x1402A51D4`/`0x1403435A8`: **harness-side** (model too
   weak). Fixed with a bump allocator at `0x5000000000` hooked at entry `0x6FFFFF9D1104` (maps + returns chunk,
   skips past the int3). Same for `RtlAllocateHeap` (`0x6FFFFFB9E9A0`, arena `0x5002000000`); without it the run
   spins 186k times and dies in an anonymous Wine trampoline (`0x6FFFFEC6CABF`, non-entry fetch refused).
4. TEB `gs:[0x58]` null-deref at `0x1402A51D4` (`mov rax,[rax+rcx*8]`): **harness-side** (TEB model only sets
   StackBase/Limit/Self). Fixed with a `TEB+0x58 -> P1 -> P2 -> P3` extension block (array + slot with
   `[P2+0x28] = P3`). ~30 `gs:[0x58]` sites in the image.
5. Hook-callback exceptions spamming 2.6 GB (`run14.log`): **harness-side** (list-append + `mem_read` inside
   MEM_WRITE hooks). Fixed: synchronous prints or counters only, stderr to `/dev/null`.
6. Unicorn dynamic-translator OOM on the 2nd fresh `build_emu` in-process: **harness-side** (emulator leak).
   Workaround: one run per process; determinism compared across processes (stronger anyway).
7. `stub_1415A17CE unknown do=powf_xmm0` (player 15 deep chain): **harness-side divergence from the diff harness**
   (its `stubs.py` installs libm `powf/logf` shims; the frame runner doesn't). Fix for the task agent: port the
   two libm models (`logf_xmm0/powf_xmm0`) into the frame runner or add exact-`powf`/`logf` import models and note
   the dependence. No code was changed in the repo (read-only rule); all fixes live in the scratch runners.

## 6. Determinism: CONFIRMED (3 identical runs, separate processes)

H-step x3 frames from snap1: players/man-block hash `632de4f21c602f26`, list hash `fb15333c059f9bfd`,
`MI+0x1198` = 30381, `MI+0x116C` = 1, run0 = run1 = run2 byte-for-byte (separate processes, so no shared-state
aliasing). (`det3.py`.)

## 7. Tooling changes (for a task agent to land)

All in `<workdrive>/gf-Pump-scratch/` (copies; TracedCases' files and the repo untouched):
- `run7.py` (working pump/H-step harness template): `LazyMap` + `map_teb` + TEB+0x58 extension block, return-0
  import models (list in §5.2), `VirtualAlloc`/`RtlAllocateHeap` bump hooks at `0x6FFFFF9D1104`/`0x6FFFFFB9E9A0`,
  `rn.install_hooks` stubs, guarded CODE/WRITE hooks (print/counter only).
- `run16.py` (H-step multi-frame + call log), `run17.py` (+ state snapshots), `run18.py`/`det3.py` (determinism
  hashes), `run19.py`/`run23.py`/`run24.py` (MA write-hooks), `run20.py`/`run21.py` (per-player chain),
  `mabuild.py` (MA build/consumer reach), `wtest.py` (list read-vs-write), `stop*.py` (stoppage probes),
  `disp*.py`/`hub*.py`/`ring*.py`/`up*.py`/`hscan.py` (decode trail for §0).
- To land: port the TEB+0x58 block, the two bump allocators, and the import-model list into
  `tools/dev/translate/frame_trace.py` (or a shared `frame_harness.py`); port `powf/logf` libm models from the
  diff harness's `stubs.py`; never add a model without the evidence line (a wrong model is a divergence).

## 8. Open items (ranked)

1. Frame advance: H-step idempotent (why `MI+0x1198`/positions never move; `rdx=0x28`-first and ordering vs the
   full pump). 2. MA-list writer (no write observed in any exercised path; build only reads). 3. Full-frame pump
   `0x141B3E060` entry (needs `xmm2/xmm3` time-base + waiter interplay). 4. Queue-role split (84-node list vs ring;
   which hub serves match types 13-61; `+0x1820` default). 5. Per-player dispatcher (queue -> player-driver caller
   unproven as a sequence). 6. `powf` libm in frame runner. 7. Deeper stoppage diff (beyond §0 tables/anchors).
