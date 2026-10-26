# Registry-inclusive open-play captures and the frame run: reach stays 1/50 (26-10-26)

Branch `RegCapture` from `origin/Emulate` `9659e0e6`. Pushed `e2cb1f63` (`tools/dev/translate/capture_frame.py`, `tools/dev/translate/frame_trace.py`); `ls-remote` confirms `refs/heads/RegCapture` at `e2cb1f63`. Scratch (captures, banks, runs) is PES-derived and not committed: `<workdrive>/gf-RegCapture-scratch/`.

## Result

- **Registry in the capture: done (CONFIRMED).** The registry is now a root. 18 accepted captures across four sets, 4 open-play captures in each set; every capture has the registry object, its 234-slot table, every non-null E, and E+8.
- **Frames run per capture: 1 of 50 requested, for all 15 captures run.** Every run stops in frame 0 with an unmodelled read inside the team tick `1404F6C30`.
- **Determinism: 3 identical runs** (PYTHONHASHSEED 1, 2, 3) on `cap6/f04`: same record md5 `2d9703ee9c`, same stop.
- **Impure reach: 1 of 50** (union 1 of 50, `1404F6C30` only). Own-block coverage: not measured (the tick is `unmodellable` for the driver, see the earlier Emulate report; not re-run here). Translated 0 of 1; bit-exact 0 of 1.
- **Comparison with the canonical union (15/50 floor, median 0.30): not comparable.** Frame-route reach is limited by the tick's first frame, not by the trace hooks.
- **Acceptance not met:** no multi-frame run, so no traced calls into the 50 impure functions. The stops are below.

## 1. Registry capture (item 1)

Layout, from the disassembly of `0x141E5D4C0` → `0x141E5CA20` → `0x141E5DE80` (CONFIRMED):

```
REG  = 0x1437F4808  (image .data; test rcx,rcx at 0x141E5D4C0)
obj  = *REG         (0x2976B10 in f06)
table= *obj         (obj+0 is the table pointer, not a vtable; 0x2976B60 in f06)
count= 0xEA         (0x141E5DE80 returns the constant 234)
E    = table[183]   (0xB7, 0x2977118 in f06)  -> E = 0x393BFAD0 (phase p3)
tick reads [E+8]    (0x141E5CA52)
```

E's first 16 bytes are `0x0`, then the +8 pointer, then `0xB7` at +0x10 (index 183). The registry object and its table are stable across the captures. E differs by match phase, which matches the earlier phase table.

Capture changes (`capture_frame.py`, commit `e2cb1f63`):
- `registry_spans` reads the object, the table and every non-null E (4 KB each), and E+8 as an object. Gather-time, so the BFS budget cannot starve it.
- Consistency gate: `registry_head` (obj, table bytes, each E's first 16 bytes) must be identical before and after the reads, with the MI frame advance in `[1, frame_max]`.
- `--root VA:BYTES` (repeatable) reads a named live object after expansion.
- `--max-objs`, `--deep-win` exposed (budget 600 used; see §4).
- A gate fix: `ok_frame = 0 <= df` accepted stale state (the game stops stepping and the frame never advances); now `1 <= df`. Three captures taken on a stale state were deleted.

## 2. Captures (item 1)

| set | captures (label) | phase | frames | roots | note |
|---|---|---|---|---|---|
| cap2 | f06, f07, f09, f12, f13, f19 | p3 | 24141–29677 | registry only | 600-object budget; patched 78 |
| cap6 | f02, f04, f05, f06 | p3 | 32142–35572 | registry + 4 named | 2 with `patched=0` |
| cap7 | f07, f08, f11, f12 | p3 | 42048–46186 | registry + 5 named | MI/G blob not patched |
| cap8 | f02, f04, f07, f11 | p1 | 185–3677 | registry + 5 named (0x20000 window) | new match |

Each capture: `frame_delta=2`, `headers_same`, `registry_same`, `slots_same` all true. Every set's captures are at least 20 s apart, so they are at different moments (open play, `play==1`). Main was messaged after the first good capture.

PES was live for the whole session, read-only `sudo -n` pread of `/proc/53052/mem`. It had a between-match gap (MI=0) for about 15 min, and a stoppage (`play=2`, `per=10`) that did not capture. No write, pause, signal or attach.

## 3. Frame runs (item 2)

All captures run with `frame_trace.py --frames 50`, 1 frame each. Stops (CONFIRMED, `record.json` `stop`):

| capture | stop RIP | access | state at stop |
|---|---|---|---|
| cap2 f06/f07/f09 | `0x140A3E8E0` (`mov eax,[rcx+0x271F0]`) | read `0x7FF5DFDE7208` | `rcx=0x7FF5DFDC0018`, the pointer stored at `0x2C2A2CE8` in the capture |
| cap6 f02/f04/f05/f06 | `0x141B5238F` | read `0x0A3FED20` | `rcx=0xA3FED20`; the 0x100-byte root `0x0A3E6A10` is too small (needs +0x18310) |
| cap7 f07/f08/f11/f12 | `0x141B5238F` | read `0x0A3FED20` | root `0x0A3E6A10:0x2000` too small (same offset) |
| cap8 f02/f04/f07/f11 | `0x141B52392` | read `0x0944006F666EB1` | `[0xA3FED20]` holds the text `Info\0Dir`; the tick reads it as a pointer |

Chain, CONFIRMED by disassembly and capture bytes:
- The tick reaches `0x140A3E8E0` after the registry E+8 read (cap2). The accessor reads `[rcx+0x271F0]`. With `rcx=0x7FF5DFDC0018` (a pointer that lives in the heap, outside the STACK_LO filter, in an anonymous rw region of 480 MB that starts at `0x7FF5DFDC0000`), the read is `0x7FF5DFDE7208`. Fixed by root `0x7FF5DFDC0018:0x28000` (cap3+).
- The tick's `this` is `0x2C293F90`. `this+0x6D50` is stored from `r8=[rsp+...]` and is `0xC148` only because `[rax+0x10]` was 0 when `rax=[rbx+8]=0x1BC71DD0` (P). P's live head is non-zero (`+8=0xA`, `+0x10=0x1BC71E00`); the 600-object BFS dropped P and `0x1BC8FEE0` (both present in the 1500-budget cap1 captures). Fixed by roots `0x1BC71DD0:0x100` and `0x1BC8FEE0:0x100`.
- `0x1405013DF` `mov rcx,[rax+0x10]` with `rax=[rdi+0x10]` (`rdi=0x2C29ACE8`) = `0x0A3E6A10` (live-verified). Root `0x0A3E6A10` added (0x100, then 0x2000).
- `0x141B5238F` reads `[0x0A3FED20]`, 0x18310 past `0x0A3E6A10`. The 0x20000 root covers it (cap8): the value at `0x0A3FED20` is the capture's `Info\0Dir` bytes, identical to live memory at the same address. The tick then reads that text as a pointer at `0x141B52392`.

The last stop is not a missing object: the captured bytes match live memory. The emulated control flow reached a state where a field that PES holds as a pointer is text. Likely causes, NOT verified: (a) an earlier named root is wrong or stale (its +0x10 or +8 comes from a different object than the live path uses), (b) a non-object field is read with a pointer-typed offset because an earlier branch was taken from a state our 600-object capture does not fully reproduce. Next step: record the stop's register and stack state (`rets` is empty in every stop; the stack-return scan found no image return addresses, so the diagnostic is not working yet), then trace which object gives `rcx=0xA3FED20`.

## 4. Why the frame window is the constraint

`frame_max=2` at ~57 frames/s gives a 35 ms window. A 600-object capture takes 36–50 ms under current machine load (loadavg ~30 on 16 cores). A 1500-object capture took 80–120 ms and was accepted in about 1 of 600 tries in the cap1 run. This is why the BFS was cut to 600, and why the 600-object BFS misses objects. Two fixes that were not applied: (a) a targeted expansion from the tick's `this` object instead of the BFS over every heap pointer, (b) moving the candidate scan (`heap_ptrs` in numpy) out of the window. The vectorised `heap_ptrs` took the per-round cost from ~0.1 s to ~0.02 s; the rest is per-read overhead.

## 5. Changes (commit `e2cb1f63`)

- `capture_frame.py`: `REG_GLOBAL`, `REG_SLOTS`, `registry_spans`, `registry_head`, `--root`, `--max-objs`, `--deep-win`, `ok_frame = 1 <= df <= frame_max`, `named_roots` in meta, `heap_ptrs` batched over all buffers.
- `frame_trace.py`: `stop` records the registers (`regs`) and, when a stack scan finds image return addresses, `rets` (currently empty: see §3, the scan is not working yet).

Checks run: `ast.parse` on both files; frame runs above (15 captures, 3 identical runs). The gate and `tools/check_pii.sh` were not run (Main gates). The unit tests and the driver were not run.

## 6. Open items (numbered)

1. **Multi-frame run (blocks reach).** Name the object behind `0x141B52392` (the text at `0x0A3FED20`): record its path from `rcx` (the call chain, `rets` fix first).
2. **`rets` stack scan is empty.** `stack_images` reads 256 qwords from RSP; no image return address found. Check the emulated stack mapping and the `UC_X86_REG_RSP` value at the stop.
3. **Stale-state gate.** `live` (`FRAME_LIVE`) starts at `frame_t=now` on the first loop iteration; `df>=1` now rejects it. A deeper fix (initial `frame_t=-inf`) is still open.
4. **Between-match gap.** Captures only come with a match in progress (MI≠0). Plan for a long-running capture and an open-play window (about 5–15 min per capture in this session).
5. **Budget and window.** See §4. Targeted expansion from the tick's `this` (`0x2C293F90`) is the cheapest next step.
6. **Coverage and bit-exact of the tick.** Not measured. The tick is still `unmodellable` for the driver (import slot `14252FDE0`, earlier Emulate §5).


## Main's audit

All 25 RegCapture captures (cap2-cap8) pass Main's byte-level audit as consistent open play (both teams whole, 22/22 slot records, H captured), and cap2's registry is complete (227 entries, each +8 target captured). The blocker is completeness beyond the walked graph, not consistency: the owner has since allowed brief SIGSTOP snapshots of the whole process.
