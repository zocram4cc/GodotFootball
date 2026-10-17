# PES team/player tick order: TeamOrder decode

Scope: read-only static decode (Ghidra bridge and dec.sh on PESLive, capstone sweeps of `pes_full.bin`) plus read-only live samples of PES2021.exe (pid 2510857, `/proc/<pid>/mem`, O_RDONLY). Nothing written to the game or the repo. Scratch: `<workdrive>/gf-TeamOrder-scratch/` (scanners `scan_all.py`, `scan_rip.py`, `scan_pub.py`, `scan_code.py`, `scan_disp.py`; live `live_order.py`, `live_order2.py`, `live_window_dump.py`, `live_probe.py`; outputs `scan_all.tsv`, `scan_pub.tsv`, `rip_registry.tsv`, `order*.json`; decompiles in `out/`). Image base 0x140000000, file offset = VA − base. Confidence: CONFIRMED (decompile, asm or live read), INF (inference, level given), OPEN (what blocks it).

Verdicts up front:
- Item 1 (22-player slot-2 loop and its place relative to H): **PARTIAL**. Loop body and effect CONFIRMED; the loop's caller is OPEN; order vs H is INF (high).
- Item 2 (per-tick bind/clear of G+0x270 and G+0x180): **PARTIAL**. Co-bind of MI, roster, ball and E CONFIRMED live; the writer and clearer are OPEN.
- Item 3 (200-stride builder of MA+0x22e4c): **OPEN**.
- Item 4 (hu+8 publisher, bit-27 cap): **PARTIAL**. Per-tick window binding of the hu table CONFIRMED live; bit-27 values CONFIRMED live; the writer of hu+8 is OPEN.

## 0. Corrections to existing docs

- `pes-re-gaps-anime.md` §549 calls `FUN_1409f1bb0` the "per-player tick loop". Decompile (`out/loop22.c` 1–124) shows a message handler: it returns unless `*param_2 == 0x14`, then runs a loop of `0x3c` (or 1) iterations of `FUN_141eb5e70(P)` on `P = *(this−0x178)`. It is not the 22-player loop.
- `pes-re-clock-record.md` names `matchCtx+0x10` in `FUN_140925750`. The counter is at the **player object** (PM+0x10), not the match context (decompile of `0x140925750`, `param_1[2]++`).
- `pes-re-gaps-match.md` §9.1b–9.2b and `pes-re-tick-order.md` §1–3 stand: H = `0x14041ecf0`, side tick `0x1404f6c30`, MA build inside `0x140535c60`.
- MI null fraction: this session saw 84% null over 1 s (4-read samples) and 97.9% null in a 20,000-sample run. The brief's 91% sits between the two; it depends on read cost per sample.

## 1. Objects and globals used below (CONFIRMED)

| Name | Address | Evidence |
|---|---|---|
| G (match ctx) | `*(0x1436f3dc8)` = 0x371d5090 live | decompiles, live |
| MI | `*(G+0x270)`, non-null only inside a tick window | live (windows at 52–60/s) |
| M | `*(0x1436f3f78)`; H = `*(M+0x788)` = 0x372284e0 live | `pes-re-tick-order` §1, live |
| Match-player registry | `0x1436f3fc0 + 8·k`, k = team·11+slot, k = 0..21 | `FUN_14091e9f0` @0x14091e9f0 writes `[(team·11+slot)·8 + 0x1436f3fc0]`; live: 22 entries, vtable `0x1426329a8` |
| match::Player vtable | 0x1426329a8; slot 2 (+0x10) = 0x140925750 | live (vtable qword of each registry entry) + `pes-re-dead-code` §5.2 |
| PM+0x10 | per-player tick counter; `FUN_140925750` increments it | decompile `0x140925750`; live: all 22 equal the frame value once (1912355) |
| P (AnimePlayer) | `*(PM+0x7d8)` | decompile `0x1408bfcf0` |
| hu(i) | `*(G+(i+0xa5)·0x28)`, bound only inside a tick window | `FUN_140a48910` @0x140a48910; live (null outside windows) |

## 2. Item 1: the 22-player slot-2 loop

### 2.1 Body of slot 2 (CONFIRMED, decompile)

`0x140925750` (`out/tick_0x140925750.c`):
1. `PM+0x10 += 1` (`param_1[2]++`).
2. `func_0x0001408bf090()` (body `0x1408bf090`), then `FUN_140925470(PM)` (`0x140925470`).
3. `FUN_1408bfcf0(PM, &msg, (PM+0x10) % 0x19 == *(uint*)(PM+0x18))` — call at `0x140925793`. Cadence gate: 25-tick cycle, phase `PM+0x18`.
4. `(*PM)+0x40(PM)` — slot 8 virtual call on the same object.

`0x1408bfcf0` (`out/loop22.c` 127–144): `P = *(PM+0x7d8)`; if `(P+0xa07 == 0 || P+0xa06 != 0)`: if `PM+0x7c8 != 0`, `FUN_141eb5e70(P, *msg)`; if `PM+0x940 != 0 && PM+0x7c8 != 0`, `FUN_141ebc820(PM+0x940, *msg)`.

Direct callers of `0x1408bfcf0` (raw `E8` scan): `0x140925793` (inside slot 2) and `0x140a367a3` (inside `0x140a36760`, a second slot-2-shaped body reached only by vtable dispatch; `E8` scan found no direct caller of `0x140a36760`).

### 2.2 Live: the burst (CONFIRMED live, read-only)

- All 22 registry entries have vtable `0x1426329a8` (k=0..21); k=22 has `0x1426420f0` (not a player).
- Counters `PM+0x10` change by +1 once per tick, all 22 together. Across the captures (`order.json` 0.5 s, `order_b.json` 0.4 s): each burst starts at an `MI_ON` edge (first change in the first sample after the edge). Burst span as sampled: 0 µs in one run (all 22 in one ~50 µs sample), 0.6–1.3 ms in the other (changes spread over several samples). The span difference is a sampling artefact that I could not rule out; treat the span as 0–1.3 ms.
- Within-burst order is NOT resolved. The sampler reads k=0..21 in fixed order within each sample, so order inside a sample is artefact. The rotated start indices seen in one run (0, 19, 17, 2, 5) are not trusted.
- Phase gate (CONFIRMED live): with `MI+0x116c == 2` (phase 2, `MI+0x1168 == 1`, `MI+0x1554 == 1`) the windows keep running at ~52/s but the 22 counters do not move (1921545 frozen over 1 s). With phase 1 they advance. So the 22-player slot-2 burst is gated on in-play phase.
- Caller of the burst: **OPEN**. The 22 calls are indirect (`call [reg+0x10]`); the registry is read by `FUN_142161c06` (position gather over 11 slots × 2 teams, `out/reg3.c` 1–82) but that fragment has no slot-2 call. Scans: `scan_all.tsv` has 2,591 `call [reg+0x10]` sites in `.text`, 0 direct calls to `0x140925750`; `rip_registry.tsv` has 3 RIP-relative references to the registry (`0x14091ed11` writer, `0x140920036` rewriter, `0x142161c06` reader).

### 2.3 Position relative to H (INF, high)

- H (`0x14041ecf0`) reads `MI` at its start (`*(G+0x270)+0x1168 != 10`), so it runs only after MI is bound.
- The burst starts at the MI bind edge, and the window then stays open 0.27–1.56 ms (live window lengths across captures). So the burst is the first observed in-window action. H's side tick runs after the bind; that H runs after the burst is INF (medium-high): the burst is first in the window, but H's entry was not directly observed.
- Not measured: the exact time of H's side-tick entry. Attempt: the frame stamp `FUN_140a42580` (`0x140a42580`, writes `block+0x21c`, called at `0x1404f6d48`) was not observable because `*(team+0x6d50)` reads 0 outside side ticks (sampled 0 across 0.6 s).

## 3. Item 2: per-tick bind and clear of G+0x270 (MI) and G+0x180 (roster)

### 3.1 Co-bind (CONFIRMED live)

Read-only, 1 s, four reads per sample: `(MI, roster G+0x180, ball G+0x40, E G+0x2c0)` non-null together in 15.6% of samples; mixed states (some fields non-null, others null) occurred in under 0.05% of samples. So the four fields are bound and cleared as one unit per tick, not MI alone.

### 3.2 Writers and clearers (OPEN)

- Writer search (`scan_all.tsv`, `.text` linear sweep): stores to disp 0x270 are dominated by stack-frame and other-object writes. No `G`-based bind found by function-level check.
- Clearers seen: `0x140af30c3`, `0x140af30d3` (`mov qword [rbx+0x270], 0`) live in `FUN_140af3020` (`out/bind.c` 49–98). That function clears `G+0x270` only when mode `*(p+0xa0) ∉ {7,8}` (match exit path), not per tick. Not the per-tick clear.
- Reader of the four fields: `FUN_1404566a0` (`out/bind.c` 941–986) copies `*(G+0x270)`, `*(G+0x2c0)`, `*(G+0x2aa8)`, `*(G+0x40)`, `*(G+0x180)` into a record (snapshot; reads only).
- `FUN_140a0cd60` is a constructor (vtable writes), not the bind.

### 3.3 Verdict

Co-bind per tick: CONFIRMED live. Writer: OPEN. Blocker: the bind is a store of a runtime-derived object pointer into G, which the displacement scan cannot isolate. Next step: a RIP-relative scan for `0x1436f3dc8` (G) loads, intersected with stores to disp 0x270/0x180/0x40/0x2c0 in the same function.

## 4. Item 3: the 200-stride builder of MA+0x22e4c

- `scan_all.tsv`: 0 stores/RMW with displacement in `[0x22e4c, 0x22f14)` in `.text`. The table is written through a derived base, as the docs say.
- Readers (docs and cache, `~/ghidra-projects/scratch/Unpacker/out/*.c`): `lVar = idx·200 + 0x22e4c` computed then `[MA + lVar + …]` (e.g. `14050f7e0.c:111`, `atk1.c:1073`, `carunits6.c:633`). No writes in the cache.
- Candidate: `FUN_142161c06` gathers each registry player's `PM+0x624/+0x62c` into stack arrays (11 × 2 loop) and calls `FUN_142172d80(stack, out)`. Its fragment has no MA store. OPEN.
- Verdict: **OPEN**.

## 5. Item 4: hu+8 publisher and the bit-27 cap

### 5.1 Window binding (CONFIRMED live)

`hu(i) = *(G+(i+0xa5)·0x28)` is null outside tick windows and non-null for all 22 slots inside them (2 s run: 2,142 window samples, all 22 slots bound). So the publisher of the hu table is a per-tick bind of the same family as item 2.

### 5.3 Bit 27 values (CONFIRMED live)

`hu+8` bit 27 inside windows: slot 16 set in 67.7% of samples, slot 6 in 13.4%, all other slots 0%. Flag word changed 12 times in 2,142 window samples (mostly stable within a window).

### 5.2 Writer (OPEN)

- Consumer found: `FUN_140676010` (`pes-re-run-targets-b2` §10): candidate `c` is refused when `a48910(c)[+8]` bit 27 is set. This is a veto, not a ramp cap. The brief's "ramp's bit-27 cap" consumer was not located; OPEN.
- Writer search: stores into `G+0x1a28..0x2190` (`scan_pub.tsv`, 1,657 sites) are dominated by other objects' fields; the per-slot publisher is not isolated. Also `FUN_14092d580` (`out/pub.c` 52–717) publishes the AI record `ai(i)` (`G+(i+0xf3)·0x28`), not hu.
- `FUN_1406e1ce0` (0x1c struct copy) and `FUN_140a47280` (0x1c-stride bulk copy, `out/pub_0x140a47280.c`) are copiers; their callers were not walked. `FUN_140a47290` is mid-function (NOFUNC in dec.sh).
- Verdict: **OPEN**. Blocker: publisher identity; next step is the caller of `FUN_140a47280` and the RIP-relative loads of `G` in its caller chain.

## 6. Frame order (one logic tick, numbered, with VAs)

Window = MI bound (≈52–60 per s in both phases; 0.27–1.56 ms long). Items marked OPEN are not yet pinned to code.

1. **[OPEN writer] Bind.** G+0x270 (MI), G+0x180 (roster), G+0x40 (ball), G+0x2c0 (E) and the per-slot tables G+(i+0xa5)·0x28 (hu), G+(i+0xbf)·0x28 (P), G+(i+0xf3)·0x28 (ai) become non-null together. Live: CONFIRMED (co-bind, §3.1; hu window binding, §5.1). Writer: OPEN (§3.2).
2. **[OPEN caller] 22 player ticks, in-play only.** For each registry entry k = 0..21 (`0x1436f3fc0 + 8k`), vtable slot 2 (`0x1426329a8 + 0x10`) → `0x140925750`:
   a. `PM+0x10 += 1`; `0x140925470(PM)`; `0x1408bf090`.
   b. `0x1408bfcf0(PM)` at call site `0x140925793`: `P = PM+0x7d8`; `FUN_141eb5e70(P)` (adjuster driver, per `pes-re-gaps-anime` §551) and `FUN_141ebc820(PM+0x940)`.
   c. `(PM vtable +0x40)(PM)`.
   Live: CONFIRMED burst of 22 at the MI edge, phase 1 only (§2.2). Within-burst order: OPEN (sampler resolution ~50 µs). Caller: OPEN.
3. **[CONFIRMED static] Team tick H** `0x14041ecf0` (vtable `0x142597580` slot 2). Reached by `0x140a8eff0` (vector walk, `call [rax+0x10]`) ← `0x1404334e0` (MatchSub gate: `(this+0x28)->vtbl[8]()` true → H, else slot-3 broadcast) ← `0x140433760` (MatchSub handler, type 2 → `vtbl+0x20`).
   a. `FUN_140a49c50(H+0x488)` (`0x140a49c50`, pending-flag resolver).
   b. If `MI+0x1168 != 10`: for side in {H+0x450, H+0x478}: `FUN_1404f6c30(side, H+0x488, side_record)`; `FUN_1404405d0` on H+0x460 if set.
   c. `FUN_1405069a0(H+0x18, H+0x488)`; clear H+0x938 flags.
4. **[CONFIRMED static] Side tick** `0x1404f6c30` (per side). Key call sites (return addresses from the decompile's frame markers, `out/callers.c`): `FUN_140a0c3e0` (0x1404f6ce9), `FUN_140501260` (0x1404f6d0f), `FUN_1404f6720` (0x1404f6d25), `FUN_1404f7ad0` (0x1404f6d2d), **`FUN_140a42580` frame stamp, call 0x1404f6d48** (writes `team_block+0x21c`), `FUN_1405008e0` (0x1404f6d55), `FUN_140502190` (0x1404f6d5d), `FUN_1404fe9c0` (0x1404f6d65), `FUN_1404fedc0` (0x1404f6daa), `FUN_14051e910` (0x1404f6dbd), `FUN_1404f8350` (0x1404f6dd8), `FUN_1404fa840` (0x1404f6e05), `FUN_1404fa720` (0x1404f6e0d), `FUN_1404fbca0` (0x1404f6e15), `FUN_140500200` (0x1404f6e67), `FUN_1404ff8b0` (0x1404f6e6f), `FUN_1404f7ba0` (0x1404f6e77), `FUN_1404fd4b0` (0x1404f6e7f), `FUN_14053b860` (0x1404f6e9d), `FUN_1404f87d0` (0x1404f6ea5), `FUN_140500b90` (0x1404f6ead), then the stage/branch block (`FUN_14051d9e0` or `FUN_14051e350`, `FUN_140581980`, `FUN_14057ac20`, `FUN_140537ac0`), the 11-slot instruction loop (`FUN_140a429a0`, 0x1404f77f7), `FUN_14057bbb0`, `FUN_14053b110`, `FUN_1404fb2a0`, `FUN_14057b6b0`, `FUN_14057c190`, `FUN_1404f8c70`, `FUN_140500a00`, `FUN_1404fad30`, `FUN_140525530` (parity gated), `FUN_140538050`, the 11-slot `FUN_140a40820` loop (0x1404f78fe), **`FUN_140535c60` master tick, call 0x1404f7955**, then `FUN_1404fc720`, `FUN_140509170`, `FUN_1404fc4d0`, `FUN_14057ba50`, `FUN_14057d240`, `FUN_140580630`, `FUN_140520930`, `FUN_14053c6e0`, `FUN_140a408c0`, `FUN_140a42a70`.
5. **[CONFIRMED static, docs] Master tick** `0x140535c60` (`0x140535c60`–`0x1405360c3`): MA build (`FUN_1405346e0` dispatcher, `FUN_140536fc0` at `0x140536002`) in the branch `MI+0x116c == 2 && MI+0x11d4 == 0`; then the 11-player setters (`FUN_140a42940`, `FUN_14062af10`/`FUN_140a40820`). Source: `pes-re-gaps-match` §5 and §9.6.
6. H runs the second side (step 4 again, with side 1).
7. After H returns, control goes back through `FUN_1404334e0` and `FUN_140433760` to the hub `FUN_141b3edd0` (`0x141b3edd0`), which then posts the `0x1820` list (`pes-re-gaps-match` §9.6).
8. **[OPEN writer] Clear.** MI, roster, ball, E and the per-slot tables go null at window end (MI_OFF). Live: co-clear CONFIRMED (§3.1); writer OPEN.

Open placement: the position of step 2 relative to steps 3–8 is INF (medium-high) for "before 3" (§2.3); the position of the bind (step 1) relative to the hub is OPEN.

## 7. Open items, with the next experiment that settles each

1. Caller of the 22 slot-2 calls (item 1). Next: RIP-relative and `[reg+0x10]` walk from the registry readers (`0x142161c06`, `0x140920036`) or a stack-return sampler (read-only) for return addresses inside the burst.
2. Bind/clear writer of G+0x270/0x180/0x40/0x2c0 (item 2). Next: RIP-relative loads of `0x1436f3dc8` intersected with stores to disp 0x270/0x180/0x40/0x2c0 in one function.
3. MA+0x22e4c builder (item 3). Next: stores through a register base after `imul …, 0xc8`; check `FUN_142161c06`'s tail for MA writes.
4. hu+8 bit-27 writer and the ramp consumer (item 4). Next: the caller chain of `FUN_1406e1ce0`/`FUN_140a47280`, and a live bit-27 flip capture (only 12 flips seen in 2 s).
5. Phase gate on the burst (`MI+0x116c == 1`): live only; static check pending in the caller.

## 8. Checks run

- Read-only live captures: about 10 short samplings, totalling about 6 s. No writes.
- Static: capstone sweeps of `.text` (scan_all, scan_rip, scan_pub; `scan_code` timed out and is unused), dec.sh decompiles of about 60 functions, bridge queries. No tests run (read-only task); no gate or smoke run.
