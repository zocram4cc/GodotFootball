# PassLane2 (run 3): the aim is P+8, the man's newest ring point; the writer of P+8 is still not found; Integrate merged locally (not pushed); nothing landed

**Hooks (top of report, as requested by Main, all armed by Main; my asks are unchanged):** H1 `aimw_140514940` (`0x140514940`, EVERY 20, cap 12) is live and filling in `live2/pid254401` (5 `aimw` + 2 `ringc2` at the time of reading). The ring-writer candidates `ringc1_14056CB00`, `ringc2_140927D50`, `ringc4_140473990` are armed (cap 6, EVERY 50). I need no further hooks for section 1. For section 3 I need **a per-frame ring trace of one man**: one hook on the aim accessor's caller chain is enough (see section 4).

**Status:** `origin/PassLane` is still `7104ed38d` (ls-remote confirmed). Local `PassLane` has a merge of `origin/Integrate e154ed687` (`27aee1b94`), **not pushed**: `gen_translated.py --check` did not finish in 1500 s on this machine (timeout, rc 124, log `gencheck.log`), and the brief bars pushing a tip whose check has not passed. No port, no test change, no measurement this run. ALiveBall, PesStallQuick, census: not run (no behaviour change to measure).

CONFIRMED = read this run (capture bytes, asm, decompile, or test output). INF = inference. OPEN = blocker named.

## 1. The man's ring, read from captures (CONFIRMED)
Read in place from `live2/pid254401` (`seq106..112`; `tools/ring_read.py`, `tools/capmem.py`). Man record `M = G.table[slot]` (`0x140A48A10`), accessor `0x140A48A10` answers `G+0x1DD8 + 40*slot` as in the image.
- `M+4` (byte) is a **count**: 0 in `seq106`, 2 in the other six captures. Read as a dword at `M+4` the words are `0x00000000` and `0x00000002`.
- `M+8` is the first 12-byte point. In all six captures the point at `M+8` **equals the man's own GF position within 0.5 m** but lags it: e.g. `seq108` slot 1 `M+8 = (-9.559, 0, 8.481)`, man `(-10.097, 0, 8.941)`, frame 170; `seq110` slot 1, frame 172: `M+8` unchanged `(-9.559, 0, 8.481)`, man `(-10.106, 0, 8.947)`. **The ring did not change between frames 170 and 172 for the same man, while he moved 0.01 m.** So the writer does not run every frame for every man (INF on cadence from 2 frames; not a proof).
- `seq106` (count 0): `M+8 = (9.5, 0, 8.5)`, which is the man's position `M+0x55C` exactly. So at count 0 the point is the current position (CONFIRMED equality, one sample).
- With count 2 (six captures), `M+0x24` (the `0x140A76240` answer for n=2: `M + 12*(n+1)`) holds the **same** triple as `M+8` in every capture: `seq108 (-9.559,0,8.481)`, `seq109 (-8.159,0,-1.369)`, `seq111 (-5.428,0,14.332)`, `seq112 (17.986,0,-11.667)` (CONFIRMED by dump). So the answer of `0x140A76240` for count 0 and 2 is the point at `M+8`.
- Layout (INF on the names, CONFIRMED on the bytes): `M+0x00` word `0x01000000`, `M+4` count, `M+8` point A (x, 0, z) in the dumps; words `M+0x0C..M+0x1C` hold further floats (for example `(0.701, -10.091, 0)`), not yet named. The ring stride is not established: the accessor's `12*(n+1)` lands off the 12-byte grid from `M+8` (for n=2 it is `M+0x24`, which is `M+8` by value, not by grid). OPEN: the entry grid.

## 2. The PASS_GET aim is `M+8` (CONFIRMED on the 6 captures above; INF beyond them)
- The finalize `0x1405141F0` copies `uStack_90` into `R+0x894`, and `uStack_90` comes from `0x140514940` (`dec_514940.txt`), which copies `*0x140A76240(man)` when its flag `this+0x20` is set.
- So for the observed counts, **`R+0x894` = the man's `M+8` point**. The probe's frozen aims (`probe5`: slot 9 (-1.77, 14.35) for 60 ticks; slot 20 (-3.57, 32.83) for 40) are `M+8` values that the writer did not refresh.
- Consequence: the 43 m red is a stale `M+8`, not a wrong sample and not a wrong accessor. The fix is the writer that refreshes `M+8` (and the ring) every frame as PES does. INF that PES refreshes it every frame: the ball-to-aim test passed on the port's per-frame run (`probe4`, aim tracked the ball), but the port also changed the cadence of the tick, so the chain is not isolated.

## 3. Ring-writer candidates (CONFIRMED eliminations; no writer yet)
| candidate | what it writes | verdict |
|---|---|---|
| `0x14056CB00` (`ringc1`) | a per-man state machine: `param_1+0x10`, `+0x20` (state id 1..5), `+0x24` (count), `+0x38/+0x3C`; no writes at `+4` or `+8` of the man record | NOT the ring |
| `0x140927D50` (`ringc2`) | decision flags into `param_3[1..4]` (the caller's output bytes); reads the man position, writes no ring | NOT the ring (2 real calls, `pid254401/seq108,110`) |
| `0x140473990` (`ringc4`) | writes 28-byte records (`piVar1 + 7k`), a different table | NOT the ring |
| `0x140A6C550` | the man-record reset (zeroes `+0x55C`, `+0x564`, `+0x574..`, `+0x5D0`, `+0x6B8`) | NOT the writer (reset) |
| `0x140A76589` | a record writer: `+0` short, `+4` byte index, `+8` a point copied from its input, `+0xC` a stamp, and it copies the man's `+0x55C` into its own `+0x53C`. It has **no direct caller** (no E8, no E9 into its range) | UNRESOLVED: a different record shape; reached some other way (indirect or inlined into a caller) |
| `0x140651430` | 2 direct callers (`0x140656C14`, `0x14065800B`, both in `0x140656270`); not decoded | OPEN |
| `0x140A1CC64` | inside a larger function, not an entry | OPEN |

Scan notes: the static search (`tools/scan_ring*.py`) is weak for this (558 hits under the width-agnostic version, `scan_ring4.out`). I did not reduce it.

## 4. What finds the writer (asks for Main)
- A **per-frame trace of one man's `M+4` and `M+8`** over ~30 frames with no touch: one hook at the entry of the **frame-level man update** (I do not have its VA; the `aimw` caller chain is the nearest known) with `--when 'd(q(q(0x1436F3DC8)+0x40)... )'` is not workable. Simplest request: hook the aim accessor's caller `0x140514940` (H1, already live) **every call** for one slot and keep all captures (the ring at each entry), and hook the candidate `0x140A76589` by ENTRY with a predicate `ret` not needed: its entry (`0x140A76589`, any call, cap 24, FULL). If `0x140A76589` never runs for a man, the writer is elsewhere.
- The writer's VA follows from which of those captures shows `M+8` changing between two consecutive calls and what `rsp+8` (its caller) is.

## 5. Task 1: team.cpp:445 (not changed this run)
Still false: the samples 1..249 are written by `Ball::CalculatePrediction` (`ball.cpp:135-267`), so the comment ("stay zero") must go in the landing commit, as Main asked.

## 6. Next steps (exact)
1. Main: keep H1 live; add the `0x140A76589` entry hook (any call, cap 24, FULL) and the `0x140651430` entry hook (any call, cap 12, FULL). Both are needed to find the writer.
2. Me: once captures land, read `M+4`/`M+8` before and after each call of `0x140A76589` and `0x140651430` for the same man, decide which writes `M+8`, decode it, and port it with the per-frame cadence, red-first on `ThePassGetAimIsTheBall...`.
3. Resolve `gen_translated.py --check` timing (1500 s not enough here; the merge's push waits for it), then push `PassLane` (merge of `e154ed687`) on its own.
4. Task 3 (TRAP reach gate): not started.

## Artifacts (scratch, `<workdrive>/gf-PassLane-scratch`, not committed)
`tools/ring_read.py` (ring dump of H1 captures), `tools/capmem.py`, `tools/scan_ring*.py` (+ `scan_ring*.out`), `dec_76589.txt` (0x140A76589, 0x140651430), `dec_ring.txt` (0x14056CB00, 0x140927D50, 0x140A6C550, 0x140A76589), `dec_ring2.txt` (0x140473990, 0x140A1CC64), `gencheck.log`.
