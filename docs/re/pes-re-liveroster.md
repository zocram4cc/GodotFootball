# PES live roster / frame advance / S4 + ball writer (LiveRoster, 26-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (VA offset 0x140000000, file-offset = VA − 0x140000000 for 0x140Axxxx code; validated by E8 rel32 at 0x140A032D8→0x140A03BE0 and the 36.0 float anchor). Scratch: `<workdrive>/gf-LiveRoster-scratch/`. PES pid 53052 throughout; read-only `process_vm_readv`, SIGSTOP snapshots ≤ 10 s (CONTed, logged). No PES bytes quoted.

## 0. Headline verdicts

| # | Ticket item | Verdict |
|---|---|---|
| 1 | S4 writer `0x140A04429` containing function + computed-base reduction to `MA+0x22E4C+200·i` | **CONFIRMED (static)** — but the site is an OUT-PARAM classification writer, not the MA-list builder. Candidate reclassified: consumer, not writer. MA-list writer remains OPEN. |
| 2 | Ball writer | **NARROWED, OPEN.** Zero `movss`/`movups` stores to `MA+0x2482C/+0x24834/+0x2601C` image-wide (CONFIRMED re-scan); only zeroing + index writes found. Same computed-base phenomenon as S4. |
| 3 | Live-match snapshot with full roster (all 22 bound) | **CONFIRMED.** `live2` snapshot: MI `0x1bc04608`, frame 43508, phase 1 (play), roster table `0x1bc02354` with 24 slots `[0,1,-1×22]` (22 bound), E ring contains a fresh type-13 with `[0x31A]=1`. |
| 4 | Emulated advance: roster gate → `0x140A128E4`, N frames, MI+0x1198 + positions advance, 3× deterministic | **NOT ACHIEVED.** Precise reason: at every SIGSTOP, `[G+0x180]` (the roster-link used by `0x140A115A0` as `r13`) is 0; the gate scans `0+k·0x74+0x10` (unmapped/zeroed) and returns `0xFF`/`0`, exits at `0x140A1282C`. The link is populated only during the ~17 ms frame window and torn down before any STOP lands. |
| 5 | With write-hooks: S4 writer confirmed + ball writer named in advancing run | **BLOCKED by (4).** Static elimination + reader map only. |

## 1. S4 — containing function and computed-base reduction (CONFIRMED)

**Function bounds (CC-delimited, verified by prologue `mov rax,rsp; mov [rax+8],rbx; … push rbp; lea rbp,[rax-0x48]; sub rsp,0x110`):**
- `0x140A03BE0` … `0x140A04616` (epilogue `pop r15…rbp; ret`), plus fall-through tail `0x140A04617` … `0x140A05AA0` reusing the same `0x110` frame (internal `jmp` targets `0x140A045C1`/`0x140A045CA`).
- `0x140A04CC0` … `0x140A04DB8` is a *separate* function (own prologue `mov rax,rsp; … sub rsp,0x110`), entered only via `call` from `0x140A03356`.

**Callers (CONFIRMED E8 scan):**
- `0x140A03BE0` ← `0x140A032D8` (inside `0x140A03030`, the analyzer) ← `0x140A02AF6` (inside `0x140A02A40`) ← `0x140A01D1E` (inside `0x140A01BE0`) ← `0x1409F4D65` (lobby/match-gated, `MI+0x116C==2` play-phase gate at `0x1409F4B61`).
- `0x140A04CC0` ← `0x140A03356` (internal, inside the analyzer).

**Computed-base reduction (CONFIRMED bytes):**
At `0x140A03FF6–0x140A0400F` and `0x140A043F5–0x140A0440E`:
```
imul rax, r13, 0xC8
add  rax, 0x22E4C
mov  rdx, [rsp+0x40]       ; MA base
add  rdx, rax              ; rdx = MA + 0x22E4C + 0xC8·r13   (r13 in 0..0x15)
cmp  r14d, 2; jge ...
movsxd rax, r14d
imul   rcx, rax, 0x58
add    rdx, rcx            ; +0x58·side   (side 0/1)  -> MA + 0x22E4C + 0xC8·i + 0x58·side
mov    eax, [rdx]          ; read id at row's slot 0   (0x140A04022, 0x140A04421)
```
`r13` is the nearest-index `i` (0..0x15, `0x140A03C5B`/`0x140A03CB7` map `esi`→`r13` via `cmovl`/`0xba2e8ba3` mul-shr), and `r14d` is the side flag (0/1, toggled via `xor r14d,1`). This is exactly `MA+0x22E4C+200·i` plus `0x58·side`, matching S4 doc §2 (`{id,d²}×11` at `+0x00`/`+0x58`, counts at `+0xB0/+0xB1`, rank at `+0xB2+id`). The computed base is the MA base from `[rsp+0x40]` (the `rbx`-derived MA pointer, set `mov r12, [rbp+0x70]` then `rbx = [r12+0x20]`).

**What `0x140A04429` actually is (CONFIRMED):** `mov dword ptr [r12], r15d` where `r12` is reassigned at `0x140A03D98` to `[rbp+0x78]` — an **output pointer** (caller's `rbp+0x78` slot), not the MA list. The function writes the **classification result** (`r15d` ∈ {0,1,2,7,9,10,0x10,0x11}) into the caller's out-param and `rsi+0x50` (state byte), plus `rsi+0x14` (id read from the list at `0x140A04024`/`0x140A045BE`). Every store in the function is to `[r12]`/`[rsi+…]` — **no store through the MA-computed base**. All 23 LEA sites for the list are **reads** (S4 doc §3 readers: `0x140A25A94`, `0x140A26BFB`, `0x140A276C7`, `0x140A17F4A`, etc.).

**Conclusion:** `0x140A04429` is a **consumer/decision function** that *reads* the nearest list and writes a classification into an out-param. It is **not** the builder. S4's `S4-WRITER` remains OPEN; the builder is off-tick (pump/phase-change path), consistent with Advance §4 ("0 writes to MA+0x22E4C across 50 H steps"). Candidate eliminations re-confirmed: pump-consumer path, detector-consume path, `0x140A22130` (E-scratch only).

**Float consts anchored to instruction (per brief rules §94):**
- `0x140A040A8 COMISS XMM0,[0x1425A9464]` = 36.0 (nearest-distance gate)
- `0x140A042BC COMISS XMM0,[0x14259D620]` = 49.0
- `0x140A037DE MOVSS XMM1,[0x14259A764]` = 60.0
- `0x140A037F3 COMISS XMM0,[0x14259BDF0]` = 7.0
- `0x140A03E89 COMISS XMM0,[…]` = 2.25 (nearest-rank threshold)
- `0x140A03EE3 COMISS XMM0,[…]` = 1.0 (tie/abs guard)

## 2. Ball writer — NARROWED, OPEN

- Image-wide scan: **zero** `movss`/`movups`/`movsd` *stores* to `MA+0x2482C/+0x24834/+0x2601C` (CONFIRMED). Only `mov qword [rdi+0x2482C], rsi` at `0x140A6F29D` — a **zeroing** (rsi=0) init, not per-frame.
- Readers mapped (CONFIRMED): `MA+0x2482C` at `0x140A1E45E` (`movsd xmm0,[rdi+0x2482C]`) and `0x140A20902` (`movups xmm0,[rax+0x2482C]`); carrier `+0x2601C` at `0x140A627EC` (`mov eax,[rax+0x2601C]`); ball-index writer `0x140457259` (`mov [r13+0x2601C], edi`) writes the **carrier index**, not position.
- Same computed-base phenomenon as S4: positions are written through a computed base in the MA-build region (`0x140A6F000–0x140A70000`, entries from `0x140A5B319`/`0x140794517`; function-boundary mapping incomplete). The per-frame float writer is not yet named; candidate class = `movups`/`movss` stores in that region with a computed `MA+0x2482C` base. **Next step:** disassemble `0x140A5B300–0x140A5B400` and `0x140A6F200–0x140A6F300` for `movups`/`movss` to a computed base, or a write-hook run on a live-frame snapshot (still blocked by the r13 link).

## 3. Live-match snapshot (CONFIRMED)

- Polling: MI was 0 in 72+ lobby polls (S4/Type13 doc); a 5 Hz poller caught an advancing match (frame 23597→23627, phase 1) and took SIGSTOP snapshot `live` (4.71 s pause, CONTed, logged).
- `live` snapshot contents (verified via `SnapMem` + direct mapping reads):
  - `G=0x1F7F8490`, `M=0x1F230710`, `MI=0x1BC04608`, frame 4848, phase 1, `MI+0x116C=2`, `MI+0x1748` all-ones (open play).
  - Roster table `0x1BC02354`: 24 slots `[+0x10] = 0,1,-1×22` → **22 bound** (full roster). `0x140A0E690` against this table returns slot 0/1 (≤0x15), so the roster gate *should* pass.
  - E ring: fresh type-13 body at word 10 with `[4]=3509, [8]=4073, [0x24]=1, [0x31A]=1, [0x31B]=1` — the first live `31A≠0` record ever captured.
  - MA chain intact: `M+0x790 → 0x393D6CE0 → +0x18 → 0x2C8B7FB0 → +0xBD08 → 0x7FF5DFDC0018` (E), `W=*(M+0x118) vt 0x14259D4F0`, `blk=*(W+0x10)=0x7FF5FD3B0010` (but `blk+8=0` — the MA pointer is built at runtime from `blk+8` and is 0 in this snapshot; same null-link issue as r13).
- `live2` snapshot (second catch, frame 43508) is partial (MI object already torn down: `G+0x270=0`, E ring has only types 9/10/24/25, no type-13) — confirms the window is ~17 ms.

## 4. Why the emulated match does not advance (precise)

`0x140A115A0(D, E+0x180C60, rec, MI)` type-13 body needs, in one instant:
1. `r13 = [rax+0x180]` where `rax = [0x1436F3DC8]` (G global). In **every** snapshot (s0, rec0/1/2, live, live2) `G+0x180 == 0`. The roster table `0x1BC02354` exists and is correct, but the G object's `+0x180` slot is never populated at STOP time — it is set during the analyze steps (step 5, `0x140426760`) and torn down after, inside the ~17 ms frame.
2. `r12 = [r9+0xE0]` = analyze `+0x20` struct (`MI+0x68/+0xE0`) — also 0 in every snapshot.
3. With `r13=0`, `0x140A0E690` scans `0+k·0x74+0x10` → `0xFF` (no match) → exits at `0x140A1282C`. Even if r13 were seeded with `0x1BC02354` manually, `r12=0` faults in `0x140A0C3E0` on `[MI+0xE0]`.

**Determinism of what does run:** the clean-exit path is repeatable — same reached set `{0x140A115A0, 0x140A1281A, 0x140A12824, 0x140A0E690}`, same counters, across reruns (Records doc §4, 3+ identical runs). The *advance* criterion (MI+0x1198 moving) is not met because the transient bindings cannot be captured.

**Root cause (named):** snapshot timing, not missing emulation. The G+0x180 link is populated only during the analyze-listener step window; SIGSTOP (even at 5 Hz) never lands inside the ~17 ms window. A setup-window capture (S4 §5–§6) or a <1 ms cadence poller with CONT-on-STOP would be needed; neither is available under the read-only rule.

## 5. Recommended next step
1. Disassemble `0x140A03BE0`'s outer callers (`0x1409F4D65` region, play-phase gate `MI+0x116C==2`) for the *producer* of the S4 list (which function writes `MA+0x22E4C` with computed base) — static, no PES needed.
2. For the ball writer: disassemble `0x140A5B300–0x140A5B400` and `0x140A6F200–0x140A6F300` for `movups`/`movss` to a computed base, or seed r13/r12 from the live `0x1BC02354` table in the forge harness and run the advancing path to name the writer + call stack via write-hooks.
3. If a live-frame capture is required: poll `G+0x180` (not just `G+0x270`) at <1 ms with SIGSTOP-on-detect; the roster link populates for ~17 ms each frame.

## 6. Evidence IDs
- EV-S4-0001 (CONFIRMED): `0x140A03BE0` bounds + prologue + E8 caller chain.
- EV-S4-0002 (CONFIRMED): computed base `MA+0x22E4C+0xC8·r13+0x58·side` at `0x140A03FF6`/`0x140A043F5`.
- EV-S4-0003 (CONFIRMED): `0x140A04429` writes `[r12]` (out-param `[rbp+0x78]`), not the MA list; zero stores through the computed base.
- EV-BALL-0001 (CONFIRMED): zero `movss`/`movups`/`movsd` stores to `MA+0x2482C/+0x24834/+0x2601C`; readers `0x140A1E45E`, `0x140A20902`, `0x140A627EC`; zeroing init `0x140A6F29D`; carrier-index writer `0x140457259`.
- EV-LR-0001 (CONFIRMED): live snapshot `live` with MI `0x1BC04608`, frame 4848, phase 1, roster `0x1BC02354` (22 bound), type-13 word 10 `[0x31A]=1`.
- EV-LR-0002 (CONFIRMED): `G+0x180 == 0` in s0, rec0/1/2, live, live2; roster link absent at STOP.
- EV-LR-0003 (CONFIRMED): clean-exit path repeatable (same reached set, same counters) across reruns.


## Main's decision: frame-advance emulation is parked

Seven RE runs (FrameTrace, Emulate, RegCapture, Pump, Advance, Records, Type13, LiveRoster) took emulation from frame 0
to a clean 50-frame tick and decoded the pump, its records and the frame record, but the match never advances: the
roster link and the per-frame bindings exist only inside a ~17 ms window a SIGSTOP never lands in. Impure coverage no
longer depends on it (many snapshots plus call-time deltas give real situations), and the cutover does not need PES's
writers named in advance: from stage S7 the PES step itself runs on PES-layout state, so the writers run as translated
code. The S4 list and ball writers stay OPEN, to be found when S7 runs the step (a write-hook in the translated step).
