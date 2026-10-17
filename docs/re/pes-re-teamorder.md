# PES team/player tick order: TeamOrder decode

Scope: read-only static decode (Ghidra bridge and dec.sh on PESLive, capstone sweeps of `pes_full.bin`) plus read-only live samples of PES2021.exe (pid 2510857, `/proc/<pid>/mem`, O_RDONLY). Nothing written to the game or the repo. Scratch: `<workdrive>/gf-TeamOrder-scratch/` (scanners `scan_all.py`, `scan_rip.py`, `scan_rip_range.py`, `scan_pub.py`, `scan_loop22b.py`, `find_vec.py`; live `live_fine.py`, `live_all22.py`, `live_tail.py`, `live_tail3.py`; outputs `scan_all.tsv`, `rip_reg_range.tsv`, `loop22b.tsv`, `fine2.json`, `all22.json`, `tail3.json`; decompiles in `out/`). Image base 0x140000000, file offset = VA − base. Confidence: CONFIRMED (decompile, asm or live read), INF (inference, level given), OPEN (what blocks it).

## 0. Verdicts and changes in this revision

- **Item 1, caller of the 22-player slot-2 loop: OPEN.** Two things are now established: (a) the 22 objects are the array at `M+0x10` (not H's listener vector), and (b) the burst does **not** precede H. Its calls are interleaved with H's side-0 tick and H's tail call. "Burst precedes H" (the §2.3 claim in the previous revision) is **refuted**.
- **Item 2, bind/clear of G+0x270 and G+0x180: unchanged.** Co-bind CONFIRMED live; writer OPEN.
- **Item 3, MA+0x22e4c builder: unchanged.** OPEN.
- **Item 4, hu+8 publisher: writer still OPEN. The bit-27 consumer is now located and verified (§5).** The previous revision said the only consumer was the `FUN_140676010` veto. That was wrong: it came from a grep of the docs, not the image. `FUN_1406cbf70` (AccelerationRamp) reads hu(opp)+8 bit 27 at raw `0x1406cc399–0x1406cc3c9`, and `pes-re-teamctx.md` §4.2 is right.
- Correction to the Ghidra listing of `FUN_1404334e0` (`out/listen.c`): the listing shows an inline loop, but the raw bytes (`gate_asm.txt`) are a gate with tail jumps: `JMP 0x140a8f0f0` (slot-3 broadcast, `0x14043350f`) and `JMP 0x140a8eff0` (vector walk, `0x140433562`). The `FUN_140a8eff0` vector walk is the listener walk, and it does not contain the 22 players (§2.4).

## 1. Objects and globals (CONFIRMED)

| Name | Address | Evidence |
|---|---|---|
| G (match ctx) | `*(0x1436f3dc8)` = 0x371d5090 live | decompiles, live |
| MI | `*(G+0x270)`, non-null only inside a tick window | live |
| M (match object) | `*(0x1436f3f78)` = 0x2c6b2b20 live | live |
| H (team brain owner) | `*(M+0x788)` = 0x372284e0 live; also at global `0x1436f3f98` | live; `find_vec.py` |
| Player array at M+0x10 | 22 pointers, stride 8: `M+0x10+8k`, k = 0..21 | `find_vec.py`: H hit at M+0x788 has the 22 player pointers at −0x778+8k |
| Registry | `0x1436f3fc0 + 8k` holds the same 22 pointers | `FUN_14091e9f0` @0x14091e9f0 writes it; live |
| match::Player vtable | 0x1426329a8; slot 2 (+0x10) = 0x140925750 | live + `pes-re-dead-code` §5.2 |
| PM+0x10 | per-player tick counter; `FUN_140925750` increments it | decompile; live |
| brain0 | H+0x450 = 0x31c43390 live; brain1 H+0x478 = 0 in every sample | live |
| side-0 block | `*(brain0+0x6d50)`, set at the start of side tick `0x1404f6c30` (line 157 of `out/callers.c`), cleared at its tail (`*(param_1+0x6d50)=0`, near 0x1404f7aa) | decompile + live |
| H tail marker | `H+0x960`, non-zero only while H calls `FUN_1404405d0` on H+0x460 (`out/listen.c`, team tick decompile) | decompile + live |
| hu(i) | `*(G+(i+0xa5)·0x28)`, bound only inside a tick window | `FUN_140a48910`; live |

## 2. Item 1: the 22-player slot-2 loop

### 2.1 Body of slot 2 (CONFIRMED, decompile)

`0x140925750` (`out/tick_0x140925750.c`): (1) `PM+0x10 += 1`; (2) `0x1408bf090`, then `FUN_140925470(PM)`; (3) `FUN_1408bfcf0(PM, &msg, (PM+0x10) % 0x19 == PM+0x18)` at call site `0x140925793`; (4) `(PM vtable)+0x40(PM)`.

`0x1408bfcf0` (`out/loop22.c` 127–144): `P = PM+0x7d8`; if `P+0xa07 == 0 || P+0xa06 != 0`: if `PM+0x7c8 != 0`, `FUN_141eb5e70(P, msg)`; if `PM+0x940 != 0 && PM+0x7c8 != 0`, `FUN_141ebc820(PM+0x940, msg)`.

### 2.2 Live burst (CONFIRMED live, read-only)

- All 22 M+0x10 / registry objects have vtable 0x1426329a8. Counters change once per tick, only in phase 1 (`MI+0x116c == 1`). In phase 2 the windows still open at about 52/s but the counters and frame stay frozen.
- Fine timeline (`tail3.json`, 1,867 in-play windows, sample period about 10 µs with 4 reads; `all22.json`, sample period about 60 µs with 25 reads): the 22 changes are spread over the window, 0.08–1.9 ms after `MI_ON`. Within a window they run mostly in descending index (21, then 19–20, 14–18, 10–13, 6–9, 0–5 in the 60 µs capture), with a few windows starting mid-sequence.

### 2.3 Position relative to H (CONFIRMED live: the simple "precedes" claim is refuted)

Per in-play window, relative to `MI_ON` (`tail3.json`, 8 representative windows):

| event | times (µs after MI_ON) |
|---|---|
| side-0 block bound (side tick entered) | 38–58 (one window at 121) |
| player 21 counter changes | 79–199 (before or after side-0 clear, varies) |
| side-0 block cleared (side tick left) | 167–336 |
| H tail marker H+0x960 on | 283–433 |
| H tail marker H+0x960 off | 300–449 (about 17–20 µs long) |
| player 11 counter changes | 441–659 (after tail on in 7 of 8 windows; once together with tail off) |
| player 0 counter changes (last) | 696–1942 |
| window (MI) closes | about 0.5–1.8 ms |

Consequences:
- Side 0 and H's tail sit **inside** the player burst. The burst is not wholly before H.
- No player counter changed while `H+0x960` was set in any window where the change could be separated (the 3-counter capture resolves this to about 10 µs). The burst is therefore not inside H's tail call (`FUN_1404405d0`).
- H's end (after the tail, including `FUN_1405069a0`) is not bracketed, so "burst after H" is OPEN; "burst interleaved with H" is CONFIRMED live.
- H starts before the burst's bulk. H reads MI first and runs its side-0 tick at 38–58 µs, which is well inside the window.

### 2.4 Candidate callers checked (CONFIRMED negatives)

- `FUN_140a8eff0` (vtbl[2] vector walk, reached by the tail jump at `0x140433562`): its vector holds H (`0x1c481660`), and no player pointer is within ±0x800 of that entry (`find_vec.py`). Not the player loop.
- `FUN_1405140e0` (`out/walk.c` 1–46): dispatches one entity by id (`param_1[id]`, slot 1 and slot 2 calls, the id from `param_2+4`). Called per entity, not a 22-loop.
- `FUN_140873ee0` (`out/walk.c` 49–178): a driver with slots 1–5 over a byte-counted array (count at +0x50), calling the ball integrator `FUN_140876360` per step. Its array is not M+0x10.
- Pattern scan `scan_loop22b.py` (streaming, `.text` 0x140380000–0x140c00000): array walk `mov R1,[X+I*8]`, then `mov R2,[R1]`, then `call [R2+0x10]`. Three hits: the vector walk `0x140a8f029` (§2.4 first bullet), `0x14051416e` (`FUN_1405140e0`), `0x140873ff8` (`FUN_140873ee0`). None matches the M+0x10 array. The scan does not cover pointer-increment loops (`add R, 8`), so a pointer-walk caller is still possible.
- Stack-return sampling is not available: `kstkesp` is 0 for every PES thread (`/proc/<pid>/task/*/stat`), and attaching a debugger is out of scope (read-only, no stops).

### 2.5 Verdict

Caller: OPEN. Blocker: pointer-walk loops and calls through a register that is not `[R1]`. Next step: scan for `add R,8` walks over `M+0x10` (`mov R1,[R0+0]` with `R0` loaded from `M+0x10`), or find writers of the M+0x10 array (the two M-stores at `0x140412c21` and `0x1404133ea` in `rip_reg_range.tsv` are the first candidates).

## 3. Item 2: per-tick bind and clear of G+0x270 (MI) and G+0x180 (roster)

### 3.1 Co-bind (CONFIRMED live)

Read-only, 1 s, four reads per sample: `(MI, roster G+0x180, ball G+0x40, E G+0x2c0)` non-null together in 15.6% of samples; mixed states occurred in under 0.05% of samples. The four fields bind and clear as one unit per tick.

### 3.2 Writers and clearers (OPEN)

- Writer search (`scan_all.tsv`): stores to disp 0x270 are dominated by stack-frame and other-object writes.
- Clearers seen: `0x140af30c3`, `0x140af30d3` in `FUN_140af3020` (`out/bind.c` 49–98). That function clears MI only on match exit (mode not in {7,8}), not per tick.
- Readers: `FUN_1404566a0` (`out/bind.c` 941–986) copies the four fields into a snapshot record.
- RIP-relative references to the G-neighbourhood (`rip_reg_range.tsv`, 51 rows, `0x1436f3f00–0x1436f4100`): writes to 0x1436f3f78 (M) at `0x140412c21` and `0x1404133ea`; writes to 0x1436f4098 and 0x1436f4070 at several sites, not yet decoded.

### 3.3 Verdict

Co-bind per tick: CONFIRMED live. Writer: OPEN. Next step: the two M-store sites above, and the 0x1436f4070/0x1436f4098 writers.

## 4. Item 3: the 200-stride builder of MA+0x22e4c

- `scan_all.tsv`: 0 stores or RMW with displacement in `[0x22e4c, 0x22f14)` in `.text`. The table is written through a derived base.
- Readers in the cache: `idx·200 + 0x22e4c` (e.g. `14050f7e0.c:111`, `atk1.c:1073`); the ramp reads it at `0x1406cc32x` (`out/ramp.c` line 124, raw `IMUL RCX,RAX,0xc8; ADD RCX,0x22e4c` at 0x1406cc329).
- Candidate: `FUN_142161c06` gathers each registry player's `PM+0x624/+0x62c`. No MA store in its fragment. OPEN.

## 5. Item 4: hu+8 publisher and the bit-27 consumer (reconciled)

### 5.1 Window binding (CONFIRMED live)

`hu(i) = *(G+(i+0xa5)·0x28)` is null outside tick windows and non-null for all 22 slots inside them (2 s run: 2,142 window samples, all 22 slots bound).

### 5.2 Bit 27 values (CONFIRMED live)

`hu+8` bit 27 inside windows: slot 16 set in 67.7% of samples, slot 6 in 13.4%, all others 0%. Flag word changed 12 times in 2,142 window samples.

### 5.3 The consumer: AccelerationRamp `FUN_1406cbf70` (CONFIRMED raw, asm from the bridge)

`pes-re-teamctx.md` §4.2 is correct. Raw sequence (`ramp_asm.txt`, lines 232–260):

- `0x1406cc37c`–`0x1406cc390`: `hu(opp)` argument is `R12`, the rank-0 record `MA+0x22e4c+200·rank`, so `[R12]` is the opponent id (`EDX = EBX = [R12]`).
- `0x1406cc399 CALL 0x140a48910`: `RAX = hu(opp)`; `RBX = RAX`.
- `0x1406cc3a4 CALL 0x14124dad0`; `0x1406cc3a9 MOVZX ECX, byte [RAX+0x28]`: `PC(opp) = ai(opp)+0x98` (ability byte 0x28 of `ai+0x70`).
- `0x1406cc3ad MOV EAX, [RBX+0x8]`; `0x1406cc3b0 SHR RAX, 0x1b`; `0x1406cc3bd TEST AL, 1`; `0x1406cc3bf JZ 0x1406cc60c` (skip if bit 27 clear).
- `0x1406cc36c CVTDQ2PS XMM14` after `FUN_1406e6660(P, 0x28)` at `0x1406cc358`: `PC(self) = XMM14`.
- `0x1406cc3c5 COMISS XMM11, XMM14`; `0x1406cc3c9 JBE 0x1406cc60c` (skip unless PC(opp) > PC(self)).

So the gate is: proceed only if bit 27 of `hu(opp)+8` is set **and** `PC(opp) > PC(self)`. This matches the doc's skip rule (`bit27 == 0 or PC(opp) ≤ PC(own)`). The Ghidra decompile line 140 shows the same test.

### 5.4 The second consumer (CONFIRMED, previous session)

`FUN_140676010` (`pes-re-run-targets-b2` §10): a veto on candidate `c` when `a48910(c)+8` bit 27 is set. This is a separate use of the same flag, not a cap.

### 5.5 Writer (OPEN)

- `FUN_14092d580` (`out/pub.c`) publishes the AI record `ai(i)` (`G+(i+0xf3)·0x28`), not `hu`.
- Stores into `G+0x1a28..0x2190` (`scan_pub.tsv`, 1,657 sites) are dominated by other objects' fields. The per-slot publisher is not isolated.
- `FUN_1406e1ce0` (struct copy) and `FUN_140a47280` (0x1c-stride bulk copy, `out/pub_0x140a47280.c`) are copiers; their callers are not walked. `FUN_140a47290` is mid-function (NOFUNC in dec.sh).

## 6. Frame order (one in-play logic window, measured, with VAs)

Times are µs after MI bind; ranges are from §2.3. Items marked OPEN are not yet pinned to code.

1. **[OPEN writer] MI bind (t = 0).** MI, roster G+0x180, ball G+0x40, E G+0x2c0 and the per-slot tables become non-null together (§3.1). Writer OPEN (§3.2).
2. **Player 21 slot-2 call** (`0x140925750` via the M+0x10 array). Counter 21 changes at 79–199 µs. Caller OPEN (§2.5).
3. **H entered, side-0 tick entered** (`0x14041ecf0` → brain0 → `0x1404f6c30`). Side-0 block bound at 38–58 µs; stamp `block+0x21c` = frame (`FUN_140a42580` at `0x1404f6d48`). Decompile: `out/callers.c`.
4. **Side-0 work**, including the master tick `FUN_140535c60` at call `0x1404f7955` (`pes-re-gaps-match` §5). Player calls continue in descending index, interleaved with this work (§2.3).
5. **Side-0 tick left**: block cleared at 167–336 µs (`*(side+0x6d50)=0`, tail of `0x1404f6c30`).
6. **H tail marker on**: `FUN_1404405d0` on H+0x460 (`H+0x960` non-zero) at 283–433 µs, for about 17–20 µs. No player counter changes inside this window (§2.3).
7. **Remaining player calls** (players 11 and below, descending) at 441–659 µs (11) and 696–1942 µs (0). H's epilogue (`FUN_1405069a0`, `H+0x988` loop) is not bracketed; its position relative to these calls is OPEN.
8. **[OPEN writer] MI clear** at window end (0.5–1.8 ms). Co-clear CONFIRMED (§3.1).

Brain1 (H+0x478) is null in every sample, so only side 0 runs in these windows. The event hub (`FUN_141b3edd0`, `pes-re-gaps-match` §9.6) posts after H returns; that step is not bracketed.

## 7. Open items, with the next experiment that settles each

1. **Caller of the 22 slot-2 calls (item 1).** Next: scan for pointer-increment walks (`add R,8`) over `M+0x10`, or find the stores into the M+0x10 array (candidates `0x140412c21`, `0x1404133ea` writing M). Settle whether the descending order is an index loop or a vector.
2. **H's epilogue position (item 1).** Next: a read-only marker for `H+0x988` or `FUN_1405069a0`, to see whether the late calls (players 11–0) are inside H.
3. **Bind/clear writer of G+0x270/0x180/0x40/0x2c0 (item 2).** Next: the RIP-relative loads of `0x1436f3dc8` intersected with stores at 0x270/0x180/0x40/0x2c0 in one function; the `0x1436f4070`/`0x1436f4098` writers in `rip_reg_range.tsv`.
4. **MA+0x22e4c builder (item 3).** Next: stores through a register base after `imul …, 0xc8`.
5. **hu+8 publisher (item 4).** Next: callers of `FUN_140a47280`; a live capture of bit-27 flips (12 seen in 2 s).

## 8. Checks run

- Read-only live captures: about 15 short samplings (roughly 6 s on the fine captures, 40–85 s on the longer ones). No writes.
- Static: capstone sweeps of `.text` (`scan_all`, `scan_rip_range`, `scan_loop22b`, `scan_pub`); dec.sh decompiles of about 70 functions; bridge `disassemble_function` for `0x1406cbf70` and `0x1404334e0`.
- The `scan_code.py` sweep timed out and is unused. `scan_loop22.py` (relaxed) and an earlier `scan_loop22b` run were killed for memory (exit 137) and are superseded by the streaming `scan_loop22b.py`.
- No tests run (read-only task); no gate or smoke run.
