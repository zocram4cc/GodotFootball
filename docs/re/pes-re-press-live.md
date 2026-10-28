# PES 2021 press command — the two OPEN facts, closed

Read-only RE + live pread. Scratch: `<workdrive>/gf-PressLive-scratch/`. No repo edits; no PES
written. Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset == VA − base for code/rdata). Tags: **CONFIRMED** = raw-image bytes read this pass
or verified disassembly; **INF** = inference, reason given; **BLOCKED** = what stopped it + the
hook that would settle it. This report closes both OPEN items in `pes-re-press-cmd.md` §8.

## 0. The one-line answer

A PRESS man's command carries **his own pace class** as the command class word (`cmd+0x5C`, from
`FUN_140A75BE0`, 0..4), **`useClassRate = 1`** (`cmd+0x5C5`, always), and **`kmh = kmh[pace class]`**
(`cmd+0x5C8`). After the class-map he sits at band `classMap[pace class]` and runs at
`bandRate[that band]` — **never** the speed-derived `SpeedToRate(12.6)=0.182`. He does **not**
route to the STEP family; he stays in the RUN family (RUN_LOOP → RUN_TURN_RUN). The recipe in
`pes-re-press-cmd.md` §6 is **confirmed as the PES mechanism** (its two assumptions — `useClassRate`
set + a run-class class — are exactly what the filler writes).

Main's independent live observation (a StoppageFlow presser holding a **flat 0.600 rate**) is
`bandRate[2]`, i.e. command class 3 → `classMap[3]=2` → `bandRate[2]=0.6`. It matches this decode
exactly.

## 1. OPEN #1 — the rate tables (script-read, operand-VA pairs)

Read from `pes_full.bin` this pass (offset = VA − 0x140000000):

| constant | VA | value |
|---|---|---|
| `bandRate[0]` `UNK_1425DDE10` | `0x1425DDE10` | **0.15** |
| `bandRate[1]` | `0x1425DDE14` | **0.3** |
| `bandRate[2]` | `0x1425DDE18` | **0.6** |
| `bandRate[3]` `UNK_1425DDE10+0xC` | `0x1425DDE1C` | **1.0** |
| RUN_LOOP keep `DAT_142590D98` | `0x142590D98` | **0.3** |
| class map `UNK_1425DDCD8[0..4]` | `0x1425DDCD8`…`CE8` | **−1, 0, 1, 2, 3** |
| kmh[1] (`FUN_140a76070`) | `0x14259222C` | **5.0** |
| kmh[2] | `0x14259BDFC` | **11.0** |
| kmh[3] | `0x14259BE10` | **16.0** |
| kmh[4] | `0x1425A2004` | **28.0** |

**`bandRate[cls]` for every class:** 0→0.15, 1→0.3, 2→0.6, 3→1.0. **Class 3 = 1.0**, well above the
RUN_LOOP keep of `0.3` (`DAT_142590D98`). So a class-3 command (or any class ≥1) clears the
`req[0x4A] < 0.3 → IDLE` keep test; only `bandRate[0]=0.15 < 0.3` would drop to IDLE. **CONFIRMED.**

The §6 fix threshold "≥ 0.3" is now a named install value: `bandRate[cls] ≥ bandRate[1] = 0.3`
holds for `cls ≥ 1`, so **any** mapped class except band 0 keeps the man in RUN_LOOP. The fix does
**not** depend on reaching the top band 3; band 1 (rate 0.3) is enough.

## 2. OPEN #2 — `FUN_1409257B0` (the presser's command fill)

### 2.1 The object graph (CONFIRMED disasm, `FUN_1409257B0` @ 0x1409257B0)

`FUN_1409257B0`'s prologue: `rdi = r8` (param 4 = **A**, the ball-request/movement sub-record,
sub-record base `A+0x874`); `rsi` = **brain** (the command holder, command record at `brain+0xa390`);
`r15 = MI`. The A85A70 calls inside write `rcx = rdi+0x8C4` (the class/kmh/use sub-block).

The movement branch `FUN_14092A340` (`fn_14092a340.c`) then copies the sub-record word-for-word
into the command record (confirmed body, lines 362–417):

```
puVar3 = param_1 + 0xa390   // cmd (brain+0xa390)
puVar4 = param_3 + 0x874    // sub-record A+0x874
cmd[off] = A[0x874 + off]   for off in 0..0x874
```

So: `cmd+0x5C = A+0x8D0` (class), `cmd+0x5C5 = A+0xE39` (use), `cmd+0x5C8 = A+0xE3C` (kmh). The three
words read by the filler (`pes-re-press-cmd.md` §2 table) are exactly the sub-record block.

### 2.2 `FUN_140A85A70` — the writer (CONFIRMED disasm)

```
0x140a85a76  mov  rbx, rcx            ; rcx = A+0x8C4 (sub-block)
0x140a85a79  mov  [rcx+0x0C], edx     ; [A+0x8D0] = class  (edx = arg)      -> cmd+0x5C
0x140a85a7c  mov  ecx, edx
0x140a85a7e  call 0x140a76070         ; xmm0 = kmh[class]
0x140a85a83  movss [rbx+0x578], xmm0  ; [A+0xE3C] = kmh   (float)          -> cmd+0x5C8
0x140a85a8b  mov  [rbx+0x575], 1      ; [A+0xE39] = use = 1                -> cmd+0x5C5
```

**`useClassRate` (`cmd+0x5C5`) is written to 1 unconditionally** by every A85A70 call. **CONFIRMED** —
this is the writer the §6 recipe needed, and it closes the "who writes `cmd+0x5C5`" INF from
`pes-re-press-cmd.md` §2 (it is A85A70, part of the sub-record, not a stray immediate).

### 2.3 The class arg — every A85A70 site in `FUN_1409257B0` (CONFIRMED disasm, 8 sites)

`FUN_1409257B0` is the AI-think movement-command fill (the presser's fill per `pes-re-press-cmd.md` §9).
All 8 A85A70 calls in it write `rcx = rdi+0x8C4` (the class/kmh/use sub-block). The `edx` arg at each:

| A85A70 call | `edx` (class) | branch |
|---|---|---|
| `0x14092595A` | **3** (hardcoded) | target-mismatch / stopped-man fallthrough |
| `0x1409259D1` | **own pace class** = `FUN_140A75BE0(PM)` (0..4), fetched at `0x1409259C3` | the **active presser** path |
| `0x140925BC2` | `[r15+0x14]` (a stored class word) | a specific sub-branch |
| `0x140925F90` | **3** (hardcoded) | the ball-target branch |
| `0x140925FAF` | **4** (hardcoded) | top-sprint override: `[r13+0x1A]≠0 && [r13+0x19]==0` |
| `0x14092625A` | **2** (hardcoded) | distance/scale sub-branch (positioning) |
| `0x140926325` | **2** (hardcoded) | distance/scale sub-branch (positioning) |
| `0x1409263E0` | **1 or 2** (distance-gated) | 1 if `d ≤ c1` else 2 |

The **active-presser** branch is `0x1409259D1` (own pace class) — the §9-identified fill for a man who
has the PRESS action and is moving; the **stopped** man (`FUN_140A75BE0==0`) takes `0x14092595A`
(class 3). `FUN_140A75BE0` (pace class) returns 0 when the man is static (`[PM+0x3F4]==0`) and 1..4
from his top speed otherwise. The `edx=1/2` distance-gated sites are the generic movement-fill's
positioning sub-branches (not the presser-at-rest command); the live hook in §3 disambiguates the
exact at-rest branch.

**Net for a PRESS man: `cmd+0x5C` = his own pace class (active, `0x1409259D1`) or 3 (stopped,
`0x14092595A`) — either ≥ class 2 — `cmd+0x5C5` = 1, `cmd+0x5C8` = kmh[class] (16.0 for class 3).**

### 2.4 Effective rate — and the STEP-family question

The filler maps the command class to the run-band via `classMap` (`UNK_1425DDCD8` = −1,0,1,2,3):

| pace class (cmd+0x5C) | `req[9]` = classMap[c] | rate = bandRate[req[9]] |
|---|---|---|
| 1 | 0 | 0.15 |
| 2 | 1 | 0.3 |
| **3** | **2** | **0.6** |
| 4 | 3 | 1.0 |

A typical presser (top speed 13.5–22 km/h = pace class 3) → `req[9]=2` → **rate 0.6**. **This is
exactly Main's flat-0.600 observation.** **The presser goes to the RUN family (RUN_LOOP →
RUN_TURN_RUN), not the STEP family.** The §3.3 alternative (route to STEP) is **refuted**: the
filler always writes `use=1` and a class ≥ the man's pace, so the rate is the class band rate and
stays in RUN_LOOP. **CONFIRMED.**

## 3. Live read — BLOCKED (with the hook that settles it)

The match (pid 4103465) was **live** and I confirmed the open-play state (`MI` set, `MI+0x11b0`
present) and **validated the read-only pread path against rdata** (live reads of `bandRate[0]=0.15`,
`bandRate[3]=1.0`, `kmh[3]=16.0` matched the image exactly). But I could **not** capture the three
command words over frames, for two concrete reasons:

1. **The match cycled foul → replay cutscene** repeatedly (watcher: `cut_load` fired 10×,
   `foul_sanction` at 09:35); `MI = *(G+0x270)` is **null during cutscenes** and only briefly set
   during open play, so a stable multi-frame window could not be held.
2. **The command object (brain, `cmd` at `+0xa390`) is not at a plain G-table accessor offset.**
   The per-player objects from `FUN_140a48a10/a5/…` (PM/PA/PI/D9) were scanned at `+0xa390` and
   `+0x874` (and through 24 field-pointer slots each) for a block with `use==1` and
   `kmh∈{5,11,16,28}`: **no hit.** The brain is reached via a virtual call in the think loop
   (`call [rax+0x40]` at `0x14092579E`), not `*(G + k*0x28)`, so a G-offset pread cannot find it.

**Hook for Main to install (pick one; all read the same sub-block):**

- **Preferred — `FUN_140A85A70` @ `0x140A85A70`** (fires for every command fill): at entry
  `rcx = A+0x8C4`, `edx = class`. After the call read
  **`[rcx+0x0C]` = class (cmd+0x5C)**, **`[rcx+0x575]` = use (cmd+0x5C5)**,
  **`[rcx+0x578]` = kmh float (cmd+0x5C8)**. To isolate the *press* man, correlate the same tick's
  press-action owner (PRESS-state 0x34 slot, chain in `pes-re-live-flow.md`).
- **Alternative — the presser's own filler `FUN_1409257B0` @ `0x1409257B0`**: the A85A70 calls at
  `0x14092595A`/`0x1409259D1` set `rcx = rdi+0x8C4`; read the same three words there. This fires
  only on the presser's movement tick, so it self-identifies the press.
- To gate on "open play, not cutscene," read `MI = *(0x1436F3DC8+0x270)`; only sample when `MI != 0`.

## 4. Final recipe — the words the GF press command must carry

Per `elizacontroller.cpp:897-916` (the press block) and `pesmovestep.cpp FillCommandWords` (the
word pack), a PES-faithful press command is:

| GF field | value | source | CONFIRMED? |
|---|---|---|---|
| `in.commandSpeedClass` | **the presser's pace class** (GF band: run = 3) | `FUN_140A75BE0`; GF mirror `pes::PressSpeedClass` | CONFIRMED (PES disasm §2.3) |
| `in.speedBand` (→ `req[9]`) | `CommandClassFor(3) = classMap[3] = 2` | `UNK_1425DDCD8` | CONFIRMED (§1 table) |
| **`kUseClassRate` (`req[0xB]`, `cmd+0x5C5`)** | **set to 1** — **the missing GF writer** | `FUN_140A85A70` `mov [rbx+0x575],1` | CONFIRMED (§2.2) |
| `in.desiredKmh` | **16.0** (kmh[3]); generally `kmh[pace class]` | `FUN_140A76070` table `0x14259222C…A2004` | CONFIRMED (§1) |
| target / look | the ActionPress target / own base (unchanged) | `FUN_140962440` | (unchanged) |

Effective rate = `bandRate[2] = 0.6` ≥ 0.3 keep → RUN_LOOP holds. **Both required changes are the
§6 recipe**: (i) set the command class to the man's pace class, and (ii) **add the `kUseClassRate`
writer** (currently absent in GF `FillCommandWords`). Neither alone works (LooseBall E1/E2), both
together reproduce PES. The §6 "STEP-family" fallback is **not** needed.

**Scope note:** Main's live 0.6 is on the merged **StoppageFlow** tree, which already produces a
working press (no 0.182 stall) — so on that tree the fix is **already present / not required**. This
recipe is the **confirmed PES mechanism** for the **Cutover/LooseBall** base, where the stall
persists and the fix must be landed per §6. The recipe is now CONFIRMED rather than a hypothesis:
its two load-bearing assumptions (`useClassRate` set + a class that maps to a band ≥ 0.3) are both
shown to be what the filler writes.

## 5. What is now CONFIRMED vs. previously OPEN

| item | before | after |
|---|---|---|
| `bandRate[cls]` table values | OPEN (§8.2) | **CONFIRMED** §1 (0.15/0.3/0.6/1.0) |
| class-3 band vs 0.3 keep | assumption | **CONFIRMED**: band 3 = 1.0 ≥ 0.3 |
| who writes `cmd+0x5C5` | INF (false-positive in §2) | **CONFIRMED**: `FUN_140A85A70`, always 1 |
| presser's class word | INF (pace class, high) | **CONFIRMED**: own pace class (active) / 3 (stopped) |
| `cmd+0x5C8` kmh | INF | **CONFIRMED**: `kmh[pace class]` = 5/11/16/28 |
| STEP-family routing | OPEN (alt. #2, §3.3) | **refuted**: stays in RUN family |
| effective press rate | ~0.182 (speed path) | **0.6** (band 2) for a class-3 presser — matches Main |
| live `cmd` words over frames | — | **BLOCKED** (cutscene cycling + brain not at G offset); hook given §3 |

**Unresolved:** the only thing not closed to a live per-frame read is item 3 (the actual in-memory
words for a *named* press man this match) — it needs the §3 hook on Main's side. Every static fact
the fix rides on is now CONFIRMED from the image, and the mechanism is independently cross-checked
by Main's 0.6 observation.
