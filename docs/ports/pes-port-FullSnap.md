# FullSnap: complete SIGSTOP snapshots, whole-process emulation, and the first 50-frame tick run (26-10-26)

Branch `FullSnap` (from `origin/RegCapture` `e2cb1f63`), pushed `fccd8471`; `ls-remote` confirms `refs/heads/FullSnap` at `fccd8471`. Not merged with main; the gate was not run (Main gates). Scratch (snapshots, runs, banks) is PES-derived and not committed: `<workdrive>/gf-FullSnap-scratch/`.

## Result

- **Snapshots: 3 complete open-play snapshots kept.** Pauses (SIGSTOP to CONT, logged in `pauses.jsonl`): snap1 **1.844 s**, snap2 **3.077 s (over the ~2 s limit; see §1)**, snap3 **1.992 s**. Each is 2366 readable mappings, 3.94 GiB, 241 hole pages (the `/dev/dri` device mapping, which the kernel will not read).
- **Frames: snap1 runs 50 of 50 requested frames with no stop**: 100 ticks (both sides, 2 per frame), 139 s. Snap2 and snap3 were not emulated (budget).
- **Determinism: 3 identical runs** on snap1 (frame records, impure call counts and stop, md5 `eaf80cdbbb28`).
- **Stop causes (snap1), classified:** three stops, all in the first frame; each fixed on the harness side; no real divergence reached in 50 frames. Details in §3.
- **Impure reach: 20 of 50** on snap1 over 50 frames (one snapshot, one run). **Own-block coverage and bit-exact on the translated side: not measured** (no driver run; budget). The canonical union (15/50 floor, median 0.30) is a union over captures; the 20/50 here is one snapshot's reach, so the two numbers are not directly comparable.
- **Traced cases: capped at 1 instance per spec** (20 bank files, one per reached spec). With the reservoir at 32 the run died twice with `MemoryError` under the 2 GB cap (§4).

## 1. Snapshot tool (`tools/dev/translate/full_snapshot.py`)

- Per snapshot: poll read-only until `MI != 0` and `play == 1`; then `kill -STOP`, read `/proc/<pid>/maps`, and read every readable writable or shared mapping with `process_vm_readv` (4 threads, 64 MiB chunks) into `mem.bin` in maps order; then `kill -CONT` in a `finally`. The index (mappings, offsets, holes, anchors) is written after CONT.
- **Deferred read.** The private read-only mappings (code and rodata) are read after CONT, to keep the pause short. `verify_snapshot.py` checks that this is harmless (§5). Wine's `memfd` views stay in the pause (fixed after the three snapshots were taken, see §5).
- Device mappings (`/dev/dri/renderD128`, 351 MB) are holes, not reads. Reading them page by page was what pushed run0 past 4 s.
- **Acceptance check on the dump.** A snapshot is kept only if `MI != 0` and `play == 1` in the dump itself. Run0's first try was rejected this way (MI null at the STOP, a step-window race).
- **Abort (added after Main's ruling).** `--max-pause` (default 2.0 s) aborts a dump that passes it: the game is CONTed in the `finally` and the snapshot is discarded. The three kept snapshots were taken before this option existed.
- **Overruns, reported:** run0 (discarded): snap1-141655 **4.257 s** (rejected, MI null in the dump) and snap1-141727 **5.287 s** (kept, then discarded). Kept snap2 **3.077 s**, taken while my emulator smoke test ran on the same machine. Not re-taken.
- Owner rule: the snapshots' pauses were logged; reads were pread-only through `/proc/<pid>/mem`; no other signal, no writes, no debugger.

## 2. Emulator on the full snapshot (`frame_trace.py --snapshot DIR`)

- **Lazy whole-process mapping (`LazyMap`).** The snapshot is never loaded whole. Pages are mapped into unicorn on first touch from `mem.bin`, in 256 KiB blocks clipped to the mapping. On snap1, 534 faults and 33,205 pages mapped for 50 frames. Holes stay unmapped, so touching one is a stop.
- The flat image is no longer pre-mapped in snapshot mode; the game image comes from the snapshot (`PES2021.exe`'s code is in anonymous mappings in the live process, and those match `pes_full.bin`, see §5).
- The `run.py` `hook_bad` change is the only edit to the shared harness, and it only applies when `uc._lazy` is set. The default path is unchanged.

## 3. Stop causes (snap1), classified

| # | Where | Stop | Class | Fix (harness side) |
|---|---|---|---|---|
| 1 | `0x1403FF444`: `call [0x14252F340]; xor eax,eax; add rsp,0x28; ret` | fetch in `ntdll.dll!RtlEnterCriticalSection` | **OS import, needs a model** | `import_stubs.json`: returns 0. The thunk discards the value. Assumption: single thread, lock never contended. |
| 2 | `0x14159FB8C`: `mov r11,gs:[0x10]` (stack-probe prologue) | read of `0x10` | **environment: TEB not modelled** (gs base unset) | `map_teb`: TEB at `0x7FF00000`, `StackBase` = emulated stack top, `StackLimit` = 0 (no guard pages, so the probe takes its no-probe path). Other TEB fields are zero: any other gs read will stop. |
| 3 | `msvcrt.dll!memcpy` (call chain from `0x140A790BB`) | fetch in an OS export | **OS import, needs a model** | `import_stubs.json`: Win64 `memcpy`, `rep movsb`, rdi/rsi saved. Assumption: non-overlapping (the memcpy contract). |

- After the three fixes: **no stop in 50 frames.**
- **Divergence: none observed.** The RegCapture stop at `0x141B52392` (a text-as-pointer read) is **not reached** in these 50 frames. This is not a resolution: that stop came from a partial walked capture, and this run is a different state.
- Any OS export without a model is `int3`; the run stops with the export's name (`unmodelled OS export ...`), so new imports show up as named stops, never as silent behaviour.

## 4. Reach, traced calls, memory

- Impure entries reached on snap1, 50 frames: 20 of 50, with call counts from 50 to 3,350 (`record.json`, `impure_calls`). Largest: `14057A3E0` (3,350), `140529490` (850), `140687310` (450), `14052A190` (350).
- `banks/`: 20 files, one instance each (`cap 1`). Cap 32 was attempted and died twice with `MemoryError` at the end of the run (the seed artifacts are 2 MiB per region, so the banks overran the 2 GB cap). Bank writing is now streamed per instance, but the reservoir itself holds every instance in memory. The fix is to store instances on disk (open item 3).
- The per-run cap 2 GB (`ulimit -v`) was respected for every emulator run.

## 5. Snapshot premise (`verify_snapshot.py`, three snapshots)

- Read-only private mappings: 1334 identical across the three snapshots, **2 differ**: `/memfd:wine-mapping` at `0x830000+0x148000` and the KUSER_SHARED_DATA page `0x7ffe0000+0x1000`. These were read after CONT, so they are not pause-consistent: the time fields change. The tool now keeps `/memfd:` mappings inside the pause (new snapshots only). Snapshots 1–3 carry this.
- Writable mappings: 660 identical, 135 differ (state moved, as expected).
- Executable image mappings: 4 equal `pes_full.bin`; **1 differs**: `0x143D14000+0x17C3E000`. Unexplained (not investigated; may be runtime-written or decoded data inside an executable mapping). Open item 4.
- 21 mappings skipped (not present in every snapshot, or unreadable).

## 6. Caveats

- **Mid-step snapshot.** A snapshot is taken while `MI != 0`, i.e. inside a match step. The replay starts H's step from its entry on that partially advanced state; this was not checked against a whole-frame boundary.
- **The bank corpus is capped (cap 1).** Reach numbers come from the call counts, not the banks.
- **Not measured:** own-block coverage and bit-exact against the translated side; snap2 and snap3 emulation; the 0x141B52392 stop with full memory.

## 7. Open items (numbered)

1. Coverage and bit-exact of the 20 traced entries against the translated side (driver `--seed-bank` over `banks/`).
2. Emulate snap2 and snap3 for 50 frames (and check the two-snapshot reach).
3. Full-size bank corpus (cap 32): store instances on disk, not in the reservoir.
4. The executable mapping `0x143D14000+0x17C3E000` that differs from `pes_full.bin`.
5. Other gs fields (TLS, PEB at `gs:[0x60]`) are zero; a read of one stops the run. Model when it stops.
6. Import models: only the three above exist. Any new import stops by name; model it with its evidence in `import_stubs.json`.

## 8. Files

- `tools/dev/translate/full_snapshot.py` (new): snapshot tool, deferred read, `--max-pause` abort, `SnapMem` reader with PE export resolver.
- `tools/dev/translate/verify_snapshot.py` (new): deferred-read premise check.
- `tools/dev/translate/import_stubs.json` (new): OS import models with evidence.
- `tools/dev/translate/frame_trace.py`: `--snapshot` mode, `LazyMap`, `map_teb`, `load_import_models`, streamed bank writes.
- `tools/dev/diffharness/run.py`: `hook_bad` routes unmapped accesses to `uc._lazy` when set (no change otherwise).
- Tests: none new (harness tooling). Checks run: `ast.parse` on all changed files; 3-frame smoke; 50-frame run ×3 (identical); verify_snapshot. Gate not run (Main gates).


## Main's audit

Main audited all three snapshots from `mem.bin` through `index.json`: each is complete open play (G's MI slot set, play 1,
period 3; both team records, 22/22 slot records and 25/26 anime and man records readable; the registry's 227 entries
readable). The differing executable mapping at `0x143D14000` is `.impdata`, which ImageDump already found self-patched
at 15 one-byte sites between dumps (`docs/ports/pes-port-ImageDump.md`).
