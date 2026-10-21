# PES 2021 — the missing referee / injury systems: DOGSO, the abandon rule, injury resistance (and the two referee negatives)

> **Main note 22-10-26:** MissingRE, checker 95/95, 7 mutations; three cited sites spot-read by Main (below). DOGSO and injury resistance found; the minimum-players abandon rule was reported as dead code, but that is REFUTED by the owner's live evidence (endless-match runs cancelled and voided 3-0 by it): the rule runs live and its writer is being re-traced; handball and time-wasting rules do not exist.

Scope: the three inventory rows still marked "missing" in
`docs/plans/pes-inventory-trackA.md` (**K9 DOGSO**, **K11 minimum-players
abandon**, **E15 injury-resistance scaling**) plus the two referee negatives
(**K7 handball**, **K12 time-wasting / late-game rules**).

Read-only RE. Image `pes_full.bin` (base `0x140000000`), Ghidra bridge
`http://127.0.0.1:8080` (`decompile_function`, `disassemble_function`,
`xrefs_to`), plus local capstone decoding of the raw bytes. **Every claim
below is re-checked against the bytes, not against the decompile** (the
decompile mis-renders signed branches and some protected constants).

Tags: **CONFIRMED** = read in the bytes/decompile this session with the VA
named; **INF** = inference, confidence given; **OPEN** = not settled, blocker
named.

Scratch: `<workdrive>/gf-MissingRE-scratch/` —
`dec.py` (capstone decoder with rip-relative constant resolution),
`callscan.py` (rel32 call-site scanner), `dispfind.py` (memory-operand
displacement finder), `rawscan.py` (raw byte-pattern scanner),
`check_missing.py` (the checker below).

---

## 0. Headline

| item | verdict | one-line rule |
|---|---|---|
| **K9 DOGSO** | **CONFIRMED** | `DOGSO(A,B) = dir(A)·spot.x > dir(A)·MA.backLine[team(B)]` **and** `\|spot.z\| < F[7]` (20.12 m). No distance-to-goal test, no ball-control test, no "defenders between" count. |
| **K9 card** | **CONFIRMED** | A DOGSO makes a sub-kind-1 tackle a **straight red** (level 4). In the box: a **non-DOGSO** yellow/red is dropped to a plain foul (level 1); a **DOGSO red** is lowered to a **yellow** (level 3) — the double-jeopardy rule. A DOGSO also adds **+30** foul points. |
| **K11 abandon** | **NOT FOUND — OPEN** | Stop reason 10 has exactly **one consumer** (`0x14044eb4e`) and **no writer**: the whole image contains **zero** stores of `10` to `MI+0x1574` and **zero** stores of `0x0b..0x0f` to the mode byte `MI+0x1775`. The abandon path exists in the dispatcher but nothing in this build produces it. |
| **E15 injury resistance** | **CONFIRMED** | The stat is read at `ability+0x2f` (0=C, 1=B, 2=A). It sets **two** thresholds: severity is halved with probability **30 % / 50 % / 70 %** (C/B/A) after the contact severity is computed, and a contact whose severity > 85 is promoted to **200** with probability **5 % (C) / 2 % (B) / never (A)**. |
| **K7 handball** | **CONFIRMED negative** | PES 2021 has **no handball offence**. Only three producers of a pending foul exist and none is a hand/arm contact. |
| **K12 time-wasting** | **CONFIRMED negative** | No 80′/85′ rule, no time-wasting instruction, no keep-ball philosophy. The only "leading late" behaviour is the CPU strategy switch to attack level 1. |

---

## 1. K9 — DOGSO (denial of an obvious goal-scoring opportunity)

### 1.1 The decision function: `FUN_1405b7390` (CONFIRMED, raw bytes)

Entry **VA `0x1405b7390`**. Signature (from the call sites):
`bool DOGSO(ctx, victimId, foulerId)`.

Raw bytes (capstone, `dec.py`):

```
0x1405b7390: mov      qword ptr [rsp + 8], rbx
0x1405b73a8: mov      r14, qword ptr [rcx + 0x18]        ; MA = *(*ctx)+0x18  (match-analysis object)
0x1405b73af: mov      rcx, qword ptr [rip + 0x313ca12]    ; 0x1436f3dc8 = G
0x1405b73bb: mov      rbp, qword ptr [rcx + 0x2aa8]       ; F  = FieldInfo
0x1405b73d7: call     0x140a48a30                        ; team of the FOULER: is_11..21 -> team 1
0x1405b73ea: movzx    r15d, byte ptr [rax + 0x254]        ; dir = team->+0x254  (s8 attack sign, +-1)
0x1405b73f2: call     0x1405b6870                        ; spot = FUN_1405b6870(ctx, victim, fouler)
0x1405b73f7: mov      ecx, 0x40                          ; line offset for team 0
0x1405b7408: mov      eax, 0x54                          ; line offset for team 1
0x1405b7410: movsx    eax, r15b                          ; dir as int
0x1405b741e: mulss    xmm1, dword ptr [rsp + 0x20]        ; dir * spot.x
0x1405b7424: mulss    xmm0, dword ptr [r14 + rcx]         ; dir * MA.backLine[t]
0x1405b742a: comiss   xmm1, xmm0
0x1405b742d: jbe      0x1405b7451                        ; not beyond the line -> false
0x1405b7438: andps    xmm0, xmmword ptr [rip + 0x1fae8f1] ; abs mask @0x142565d30
0x1405b7443: movss    xmm0, dword ptr [rbp + 0x1c]        ; F[7] = penalty-area half width
0x1405b7448: comiss   xmm0, xmm1
0x1405b744b: jbe      0x1405b7451                        ; |spot.z| >= F[7] -> false
0x1405b744d: mov      al, 1                             ; true
0x1405b7451: xor      al, al                            ; false
```

**The rule, exactly:**

```
DOGSO(A,B) =  (dir(A) * spot.x  >  dir(A) * MA.backLine[team(B)])
            && (|spot.z| < F[7])
```

where

* `A` = the **victim** (the player fouled), `B` = the **fouler**;
* `dir(A)` = `team(A)->+0x254`, an `int8` attack sign (±1);
* `MA.backLine[t]` = the float at `MA + 0x40 + 0x14*t` — the **defending
  team's deepest back-line x** (`MA` = `*(*ctx)+0x18`, the match-analysis
  object). `MA+0x40` is the *back* line, `MA+0x3c` the front-mid mean and
  `MA+0x44` the team lineX (`pes-re-defensive-shape.md` §"MA line block",
  `pes-re-foundation-matchball.md` row D1). **INF(med)**: it is the
  *last-defender* line, i.e. the second-last-defender rule PES uses for
  offside is the same quantity family;
* `spot` = `FUN_1405b6870(ctx, A, B)` — the **contact spot**, computed in
  §1.2;
* `F[7]` = the penalty-area half width, a **runtime** FieldInfo value
  (20.12 m; `pes-re-fieldinfo.md` §1 — it is VM-computed and is *not* a
  constant in the exe, so a port must name it, not internalise an address).

### 1.2 The contact spot: `FUN_1405b6870` (CONFIRMED, raw bytes)

```
0x1405b6892: call     0x140a48a10        ; victim motion object
0x1405b68a0: movsd    xmm0, qword ptr [rax + 0x55c]   ; victim pos x,z (Fox metres)
0x1405b68ae: movsd    qword ptr [rdi], xmm0
0x1405b68b5: call     0x140a489f0        ; fouler AI record
0x1405b68ba: cmp      dword ptr [rax + 0x1478], esi   ; AI target == victim?
0x1405b68c2: cmp      dword ptr [rax + 0x1474], 0     ; and a target is set
0x1405b68d7..0x1405b68e4: copy AI+0x1480..0x1488 -> spot   ; predicted interception point
0x1405b6908: movss    xmm2, dword ptr [rip + 0x2015928]   ; 0x1425cc238 = 0.06  (script-read)
0x1405b691e: mulss    xmm1, xmm2
0x1405b6922: subss    xmm0, xmm1        ; spot.x -= dir(A) * 0.06   (6 cm back towards A's own goal)
0x1405b6932: comiss   xmm0, xmm1        ; sign of spot.z
0x1405b6941: movss    xmm0, dword ptr [rip + 0x1fb44d3]   ; 0x14256ae1c = 1.0
0x1405b6937: movss    xmm0, dword ptr [rip + 0x1fd18d9]   ; 0x142588218 = -1.0
                                                 ; spot.z += sign(z) * 0.06  (6 cm towards the touchline)
```

So the spot is the victim's position — or the fouler's **predicted
interception point** when the fouler's AI was targeting the victim — pulled
**6 cm** back towards the victim's own goal and **6 cm** towards the
touchline. Script-read constant: `0x1425cc238` = **0.06** (0.05999999865).

### 1.3 Where the DOGSO result is used (CONFIRMED, three call sites)

`xrefs_to 0x1405b7390` → exactly three callers:

| call site | function | effect |
|---|---|---|
| `0x1405b761d` | `FUN_1405b7470` (the level decider) | **straight red**: the fouler is in TACKLE (action `0xd`) with **sub-kind 1** ⇒ level = 3 (yellow), and **4 (red) if DOGSO**. Bytes: `0x1405b75ff cmp byte ptr [rax+1], 0xd` / `0x1405b7605 cmp byte ptr [rax+0x44], 1` / `0x1405b7624 mov ecx, 4` / `0x1405b7629 cmovne ebx, ecx`. |
| `0x1405b7d4d` | `FUN_1405b7470` (the box rule) | **double jeopardy**: if the spot is in the attacking box (`local_a8 & 0x800`), then a **non-DOGSO** card is dropped to a plain foul and a **DOGSO red** is lowered to a **yellow**. Bytes: `0x1405b7d5a lea eax, [rdi-3]` / `0x1405b7d5d cmp eax, 1` / `0x1405b7d62 mov edi, r13d` (level 1) and `0x1405b7d67 cmp edi, 4` / `0x1405b7d6a mov eax, 3` / `0x1405b7d6f cmove edi, eax`. |
| `0x1405b6f3c` | `FUN_1405b6d40` (`P_action`, the action-points term) | **+30 points**: the foul is a DOGSO **and** the **victim fell** (`r13b`, set at `0x1405b6e77 mov r13b, 1` — reached only when the victim's action id is `0x11` FALL_DOWN, `0x1405b6e26 cmp al, 0x11`) **and** the foul is in the victim's attacking box (`FUN_140a72e70`, bit 0x800 — the same box test as §1.4) ⇒ add 30 to the foul points. Bytes: `0x1405b6f06 call 0x140a72e70` → `0x1405b6f3c call 0x1405b7390` → `0x1405b6f43 je` (not a DOGSO ⇒ no bonus) → `0x1405b6f45 test r13b, r13b` → `0x1405b6f4a add sil, 0x1e` (0x1e = 30). |

### 1.4 The full card path (the "yellow vs red" decision)

The foul is scored as `points = P_angle + P_speed + P_action` (u8, no RNG)
and mapped to a **level** by thresholds (protected `0x143f71690` +
`0x143fa8760`, CONFIRMED from the raw bytes):

```
(T1, T2, T3) = spot in the penalty area A attacks ? (60, 80, 110) : (40, 80, 110)
```

Script-read proof of the in-box pair:
`0x143fa87f8 mov byte ptr [rdi + 2], 0x6e` ⇒ T3 = **110**;
`0x143fa8802 mov r15w, word ptr [rip + 0x430b5be]` loads the obfuscated word
at `0x1482b3dc8`, `0x143fa880a xor r15w, 0x50fc` decodes it
(`0x1482b3dc8` = `0x00c0` ⇒ `0x00c0 ^ 0x50fc` = **0x503c** ⇒ T1 = **60**,
T2 = **80**); and the out-of-box pair is the literal
`0x143fa8816 mov word ptr [rdi], 0x5028` ⇒ T1 = **40**, T2 = **80**.

```
level = points <  T1 ? 0                     # play on
      : points <  T2 ? (points > T1 + (T2-T1)/2 ? 2 : 1)   # 2 = foul + warning, 1 = foul
      : points <  T3 ? 3                     # yellow
      :                4                     # red
```

Then the overrides, in the order the bytes give them (`FUN_1405b7470`):

1. **Forced yellow / red before scoring** — the victim's flag bit 29 outside
   the box ⇒ level 3 (`0x1405b75e8 mov ebx, edx` with `edx = 3`,
   `0x1405b75dd test cl, 1`); the fouler in TACKLE with **sub-kind 1** ⇒
   level 3, or **level 4 if DOGSO** (§1.3).
2. **Forced red from a professional foul** — if the ball is > 5 m away
   (`DAT_14259be20` = 25.0 is the *speed* constant; the 5 m radius is
   `sqrt(25)`, `pes-re-duels-fouls.md` §2.5 step 5) and the fouler's intent
   flag is set and the victim fell ⇒ level ≥ 3.
3. **Box rule (double jeopardy)** — §1.3.
4. **Suppressions** — tutorial/training (`G+0x3b0`) or the card-suppression
   flag `*(G+0x2c0)+0x76` ⇒ level 3/4 → 1; online practice (`G+0x400`) ⇒
   any level → 1.
5. **Repeat offender** — a stored warning from ≤ 45 game minutes ago adds
   `⌊m/3⌋` points; if that reaches level 3 the level is capped at 3.

The level becomes the card: `JR+0x38` (`MI+0x158c`) = **1 foul, 2 foul +
warning, 3 yellow, 4 red**. On commit (protected `0x143f546f0`, raw bytes
read) `0x143f54865 mov ecx, dword ptr [rdi + 0x14]` / `sub ecx, 1` /
`je` → kind 2 (direct foul) ⇒ the level is written straight to `JR+0x38`;
kind 1 ⇒ 1, kind 3 ⇒ 3, kind 4 ⇒ 1.

**So: a DOGSO does not *create* a red. It (a) turns a sub-kind-1 tackle into
a straight red, (b) turns a red into a yellow inside the box, and (c) adds 30
points (which can push a borderline foul over the 110 threshold into a red
on its own).**

### 1.5 What GF would need to carry

GF has **no DOGSO symbol** in `pesjudge.hpp`/`referee.hpp` and **no
last-defender-line rule** (`pes-re-foundation-areas.md` row J-7). To carry
PES parity GF needs:

1. A **back-line x per team** (`MA+0x40+0x14*t`) — the deepest outfielder's
   world x. GF's `pesteamconcepts.hpp:172` already exposes a team line; the
   *back* line is the missing quantity (its writer is still OPEN — see §5).
2. A `DOGSO(victim, fouler)` predicate using the two inputs above plus the
   **contact spot** with PES's 6 cm / 6 cm shift, and the **attack sign** of
   the victim's team.
3. In the foul judge: the **straight-red-for-sub-kind-1-tackle** rule, the
   **box double-jeopardy** rule, and the **+30 points** term.
4. The **penalty-area half width** (20.12 m) as a named constant, not an
   address into FieldInfo.

Note the GF stand-in is *not* a constant: it is a rule that disappears once
the back-line and the spot are carried. Per the brief, GF should carry the
**rule**, and the stand-in should name the PES site (`FUN_1405b7390`).

### 1.6 OPEN

* The **writer** of `MA+0x40+0x14*t` (the back line) was not located this
  session. The reader is confirmed; the writer is the team-shape chain
  (`FUN_1405fbcb0` / `FUN_14051d9e0`, named in `pes-re-foundation.md` row
  MB-D but "not traced"). **INF(med)**: it is the same deepest-outfielder x
  the defensive-shape report calls "MA.lineBack".
* The exact **meaning** of `MA.backLine` (last defender? second-last? the
  whole back four?) is INF: the code only compares the spot against it.
* Whether `spot` uses the victim's *current* position or a *predicted* one
  when the fouler was targeting the victim — the code takes the AI's
  `+0x1480` interception point for the **fouler's** target, which is
  INF(med) "where the fouler expected to meet the victim".
* Whether the "6 cm back" is really "towards the victim's own goal": the
  code subtracts `dir(A)*0.06` from `spot.x`, and `dir(A)` is the attack
  sign, so yes — but the *sign convention* of `+0x254` is inferred from
  `pes-re-duels-fouls.md` §2.6, not re-proven here.

---

## 2. K11 — the minimum-players abandon rule (stop reason 10)

### 2.1 What exists (CONFIRMED)

| item | VA | evidence |
|---|---|---|
| stop-reason field `MI+0x1574` (`JR+0x20`) | setter `0x141b8b250` = `mov dword ptr [rcx + 0x20], edx; ret` | 43 call sites in the whole image |
| mode byte `MI+0x1775` | setter `0x140a3ccf0` = `mov byte ptr [rcx + 0x1775], dl` | **one** caller: `0x141fb1903` inside `FUN_141fb1880`, passing **3** (match-start reset) |
| **the only consumer of stop reason 10** | `0x14044eb4e` = `cmp dword ptr [r13 + 0x1574], 0xa` | in the foul/restart builder `FUN_14044eb10` |
| the mode test next to it | `0x14044eb37` = `cmp byte ptr [r13 + 0x1775], 0` | same builder |
| the dispatcher that branches on the mode | `0x1404526b7` = `movzx eax, byte ptr [r8 + 0x1775]` in `FUN_1404526a0` | `mode ∈ {9,10}` → inline programme; `mode ∈ {0x0b..0x0f}` → `FUN_14044ffd0`; `mode == 0x10` → `FUN_14044ffd0` + side 2 |
| the abandon builder | `0x14044ffd0` (`push rbx` at entry) → `FUN_140451fd0(param_1, 0xc)` then steps `[0x0f, 0x45, 0x32, 0x0f]` + tail | `FUN_1404526a0` calls it at `0x1404526db` and `0x1404526fb` |
| on-pitch flag array `MI+0x1748[0..21]` | setter `0x140a3cac0` = `cmp edx, 0x15` / `mov byte ptr [rcx + rdx + 0x1748], al` | 6 callers, **none of them a sending-off site** |

### 2.2 The negative result (CONFIRMED, exhaustively)

Three independent whole-image searches (raw byte patterns over all
`0x140001000..0x15b97c000`, i.e. `.text` **and** the protected RWX region):

1. **No store of `10` to the stop reason.** The pattern
   `C7 <modrm> 20 0a 00 00 00` (and the `0x1574` displacement variant) has
   **zero** hits in the whole image. The 43 `FUN_141b8b250` call sites pass
   immediates **1, 2, 6, 7, 8** only; the remaining sites pass record-sourced
   values (`[rax+0x1ac0]`, `[rax+0x1b0c]`, `[rbp+0xe0]`, `[rax] >> 0xe`).
   The `>> 0xe` nibble sites (`0x140d1daaf`, `0x140d1dc36`) can produce at
   most 3; the `+0x1ac0` / `+0x1b0c` sources are menu/network records, not
   match flow.
2. **No writer of the abandon mode bytes.** `C6 <modrm> 75 17 00 00`
   (imm8 store to `+0x1775`) has **zero** hits in the whole image;
   `88 <modrm> 75 17 00 00` (register store) has exactly **two** hits:
   `0x1404144e2` (the **snapshot copy** — `mov byte ptr [rbx + 0x1775], al`
   inside the record-copy routine reached from `0x14041b62b`) and
   `0x140a3ccf0` (the setter itself). So the only way the mode byte changes
   is `FUN_140a3ccf0`, whose only caller writes **3**.
3. **No player-count threshold.** A displacement scan of every `MI+0x1748`
   access in `.text` (**1582** sites) and in the protected region (**10**
   sites) found **no** `cmp` against 6/7/8/10/11 paired with an increment of a
   counter and a subsequent stop-reason write. The protected-region readers
   are all player-selection predicates (e.g. `0x143f24143 cmp byte ptr
   [rcx + r11 + 0x1748], 0` → a per-slot "is on the pitch" test inside a
   lineup picker).

**Conclusion:** the *consumers* of the abandon rule exist and are decoded
(the dispatcher branch, the builder `FUN_14044ffd0`, the programme
`[0x0f, 0x45, 0x32, 0x0f]`), but **nothing in this build of PES 2021
produces** stop reason 10 or the abandon mode bytes. The writer is either
(a) in the protected region behind a path that never runs in a normal match,
(b) reached through one of the record-sourced `FUN_141b8b250` sites whose
source record has not been identified, or (c) **not implemented in PES 2021
at all** — the abandon builder is dead code carried over from an earlier
title.

### 2.3 Why the "threshold 7" premise is not supported

The brief's premise ("players on the pitch < N", threshold 7) comes from the
Track A row, not from a PES source. The searches above found **no** compare
against 7 tied to a player count. The nearest numeric 7 in `.text` near a
`0x1748` access is `0x1404251d0 cmp dl, 7` — a **match-setup mode** byte
(`*(G+0x2ad0)`, the competition kind), not a player count.

### 2.4 What GF would need to carry

Nothing, if the negative holds. If GF wants the same *shape* as PES, it needs:

* a `StopReason::Abandon` (10) enum value, matching `MI+0x1574`;
* a match-mode byte matching `MI+0x1775` with the abandon values `0x0b..0x0f`;
* the abandon programme `[0x0f, 0x45, 0x32, 0x0f]`.

All three are **dead code in PES** as far as this session could establish.
Per the brief's "PES is king", the honest port is therefore: **GF should not
implement a minimum-players abandon**, and any GF rule of that kind should be
labelled `// RE-NEW:` (a game-specific addition, not a PES parity gap).

### 2.5 OPEN

* The four record-sourced stop-reason writers (`0x140d1d770`,
  `0x1410a7ace`, `0x1413e7bbd`, `0x1420bd7e5`) were not traced to a source
  record that could hold the value 10. **INF(low)**: they are menu /
  replay / online-flow records, not match flow.
* Whether a **live** PES match ever reaches the abandon path (e.g. after 5
  red cards) was **not** tested live this session — no live capture was
  performed, so the negative is a **static** finding, not an observed one.

---

## 3. E15 — injury resistance scaling

### 3.1 Where the stat is read (CONFIRMED, raw bytes)

The ability record getter is `FUN_14124dad0` = `lea rax, [rcx + 0x70]`
(`0x14124dad0`) — i.e. the live-stat block sits at `player + 0x70`, and the
resistance byte is at **`ability + 0x2f`**.

```
0x140481856: movzx    ecx, byte ptr [rax + 0x2f]   ; resistance 0/1/2
0x14048185a: test     ecx, ecx
0x14048185c: je       0x140481871                    ; C
0x14048185e: cmp      ecx, 1
0x140481861: je       0x14048186a                    ; B
0x140481863: mov      ebx, 0x46                     ; A -> 70
0x14048186a: mov      ebx, 0x32                     ; B -> 50
0x140481871: mov      ebx, 0x1e                     ; C -> 30
0x140481876: mov      edx, 0x64                     ; draw bound 100
0x140481882: lea      r8d, [rdx - 0x62]              ; r8 = 2  (the RNG slot)
0x140481886: call     0x140a924e0                   ; draw = trunc(state * 2^-31 * 100)
0x14048188b: cmp      eax, ebx
0x14048188d: jae      0x140481894
0x14048188f: mulss    xmm6, xmm9                     ; sev *= 0.5   (xmm9 = 0.5 @0x142565600)
```

**So the stat is the live stat id `0x2f`** — the *internal* id, matching the
`.rdata` ability-name pool `… STAMINA / FORM / RESISTANCE` at
`0x1425d80f8 / 0x1425d8100 / 0x1425d8108` (`pes-re-stamina-drain.md` §4).
`0` = C, `1` = B, `2` = A — the Player.bin ability id is `0x2E` with UI =
raw + 1 (`pes-re-matchlogic.md` §1.3).

### 3.2 The severity formula (CONFIRMED, raw bytes, `FUN_140481610`)

```
base = victim.aiState == 0x11 ? 100 : 50          ; 0x14253ee30 = 100.0
                                                  ; 0x14259be34 = 50.0
ua   = clamp(speedA / 25, 0, 1)                   ; 0x14259be20 = 25.0
uv   = clamp(speedV / 25, 0, 1)
sev  = base * (0.7 + 0.3*ua) * (0.7 + 0.3*uv)      ; 0x142590d98 = 0.3, 0x14257a194 = 0.7
sev *= attacker.aiState in 11..12 ? 1.0 : in 13..14 ? 0.8 : 0.5
                                                  ; 0x142572348 = 0.8, 0x142565600 = 0.5
sev *= speedA >= 1 ? 0.75 + 0.5*clamp(angle/180,0,1) : 0.2
                                                  ; 0x14259bdd4 = 0.75, 0x142598580 = 180.0
                                                  ; 0x142598578 = 0.2
sev  = (resistance roll) ? sev * 0.5 : sev        ; 0x142565600 = 0.5
sev  = clamp(sev, 0, 255)                         ; 0x14258b550 = 255.0
```

### 3.3 The two resistance gates — the answer to "how does it scale"

**Gate 1 — severity halving (1st RNG draw), in `FUN_140481610`:**

| resistance | threshold | effect |
|---|---|---|
| C (0) | **30 %** (`0x1e`) | severity × 0.5 |
| B (1) | **50 %** (`0x32`) | severity × 0.5 |
| A (2) | **70 %** (`0x46`) | severity × 0.5 |

**Gate 2 — the "jackpot" promotion to an immediate injury, in
`FUN_1404812b0` (the accumulator, raw bytes):**

```
0x1404814e3: call     0x140481610          ; severity of this contact
0x1404814eb: cmp      al, 0x55             ; severity > 85 ?
0x1404814ef: call     0x14124dad0
0x1404814f9: movzx    edx, byte ptr [rax + 0x2f]   ; resistance
0x1404814fd: test     edx, edx
0x1404814ff: je       0x14048150b          ; C
0x140481501: cmp      edx, 1
0x140481504: jne      0x140481534          ; A -> never
0x140481506: mov      r14b, 2              ; B -> 2 %
0x14048150b: mov      r14b, 5              ; C -> 5 %
0x14048150e: mov      edx, 0x64            ; draw bound 100
0x140481513: lea      rcx, [rbp + 0x1a70]  ; the Injury object's math::Random
0x14048151a: lea      r8d, [rdx - 0x62]    ; slot 2
0x14048151e: call     0x140a924e0
0x140481527: mov      edx, 0xc8            ; 200
0x14048152c: cmp      eax, ecx
0x140481531: cmovb    ebx, edx             ; severity := 200
```

| resistance | jackpot chance | result |
|---|---|---|
| C (0) | **5 %** | severity → **200** ⇒ immediate injury (state 3) |
| B (1) | **2 %** | severity → **200** |
| A (2) | **never** | — |

The severity then accumulates: `acc = min(255, acc + sev)`; `acc ≥ 150` ⇒
knock, `acc ≥ 200` ⇒ injured (`pes-re-stamina-drain.md` §6.2/§6.3, which is
consistent with the bytes read here).

**The RNG** is `match::Injury`'s embedded `math::Random` at `+0x1a70`:
`FUN_140a924e0(rng, M, 2)` = slot-2 LCG
`0x140a924ef imul eax, dword ptr [rcx + r9*4 + 8], 0x2d22b2e3` /
`0x140a924e6 mov r8d, 0x4513` / `0x140a924f8 sub r8d, eax` /
`0x140a924fb btr r8d, 0x1f` ⇒ `s = (0x4513 − s·0x2D22B2E3) & 0x7fffffff`;
the draw is `trunc(s · 2⁻³¹ · M)` with the scale constant
`0x1426550d8` = **4.656612873077393e-10** = 2⁻³¹.

### 3.4 The call path into the match tick

```
contact event kind 0xf
 └ FUN_140481bb0        (match::Injury entry)
    └ FUN_140481d70(inj, ev, victim, attacker)     # gates: MI+0x1775 == 0, MI+0x116c == 1,
                                                     # victim on the pitch, injuries enabled,
                                                     # victim in contact with that attacker
       └ FUN_1404812b0  (accumulate)
          ├ FUN_140481610  (severity, incl. the resistance halving roll)
          └ the jackpot roll (sev > 85 → 200)
       └ FUN_1404819a0 / FUN_14048202c  (state 3 → 4, the stretcher demo request)
```

Gates read in `FUN_1404812b0` (decompile + bytes): `MI+0x1775 == 0`,
`MI+0x116c == 1`, `*(ctx+0x2c0)+0x72+side != 0` (injuries enabled for that
side), `!FUN_140a3f570(settings)` (`settings+0x76 == 0`, the card/incident
suppression flag), the victim's AI state not `0x0f`, no teammate in AI state
`'X'`, and `victimMotion+0x610 == attacker` (real contact, not a proximity).

### 3.5 What GF would need to carry

GF has **no injury path at all** from a contact (`pes-re-injury-fatigue.md`
§4.3: the fatigue cause is missing, the contact cause is only partially
present). To carry the E15 rule GF needs:

1. A **live injury-resistance value** (0/1/2) per player, sourced from the
   Player.bin ability `0x2E` (UI = raw + 1) — GF's player database already
   carries the raw ability, but the *live* stat id `0x2f` is a separate
   live-stat block (`player + 0x70 + id`), not the raw ability byte.
2. The **severity formula** of §3.2 (all constants are named above; every one
   is a script-read float in the image).
3. The **two resistance gates** of §3.3: the halving roll (30/50/70 %) and
   the jackpot (5/2/never %).
4. A **per-player severity accumulator** with the 150/200 state thresholds.

GF must route the draws through its own `pesrandom.hpp` (per the brief) with
the **same slot-2 LCG** if bit-exactness matters; the Injury object's RNG
has its own default seed (193490) and is never re-seeded
(`pes-re-stamina-drain.md` §9), so GF can either reproduce that seed or
document a deviation.

### 3.6 OPEN

* The **fatigue** cause (`PLAY_DATA_INJURY_FATIGUE`) was not found in this
  object (`pes-re-stamina-drain.md` §6.2 already flags it as open) — the
  E15 rule covers only the **contact** cause.
* The mapping of the *attacker* action classes (`aiState` 11..12 vs 13..14)
  to "tackle" vs "slide" is INF (med) from the action id space.
* Whether the **GF** injury-resistance value should be the raw ability byte
  (`0x2E`, 0..2) or the live stat (`0x2f`) is a design choice; they coincide
  in stock data but the live stat can be modified in-match.

---

## 4. K7 — handball (CONFIRMED negative)

**PES 2021 has no handball offence.** The evidence, re-verified this session:

1. **Only three producers of a pending foul** (`FUN_14050ee40`, rel32 call
   scan over the whole image):
   * `0x1405b8282` — the **contact** judge `FUN_1405b8000` (direct foul,
     kind **2** — bytes `0x1405b8258 mov dword ptr [rsp + 0x38], 2`);
   * `0x1405b67d7` — the **offside** judge `FUN_1405b6660` (kind 1);
   * `0x1405b8982` — the **dive** judge `FUN_1405b83b0` (kind 3).
   None of them is a hand/arm contact.
2. **The whistle writer's callers.** `FUN_14050e770(judge, side, restart,
   reason, spot, foulKind, fouler, victim)` — the register/stack mapping read
   from the bytes is: `edx` = side, `r8d` = restart, **`r9d` = stop reason**,
   `[rsp+0x20]` = spot, `[rsp+0x28]` = **foul kind**, `[rsp+0x30]` = fouler,
   `[rsp+0x38]` = victim. Its callers (5 in `.text` + 2 in the protected
   region) pass:

   | caller | reason (`r9d`) | foul kind (`[rsp+0x28]`) | bytes |
   |---|---|---|---|
   | kick-off / training `0x14050cba2` | **0** | `r12d` (0) | `0x14050cb9c xor r9d, r9d`, `0x14050cb88 mov dword ptr [rsp+0x28], r12d` |
   | ball out (corner / goal kick) `0x14050e0f5` | **4** | **0** | `0x14050e0b9 mov r9d, 4`, `0x14050e0d4 mov dword ptr [rsp+0x28], 0` |
   | throw-in / goal `0x14050e2f2` | **3** | **0** | `0x14050e1fd mov r9d, 3`, `0x14050e215 mov dword ptr [rsp+0x28], 0` |
   | throw-in `0x14050e47d` | **4** | **0** | `0x14050e455 mov r9d, 4`, `0x14050e465 mov dword ptr [rsp+0x28], 0` |
   | PK shoot-out `0x14050ccbc` | **9** | **0** | `0x14050cc98 mov r9d, 9`, `0x14050cca6 mov dword ptr [rsp+0x28], 0` |
   | foul commit (protected) `0x143f54758` | **5** | **2** | `0x143f5473a mov r9d, 5`; the kind comes from the pending record |
   | period end (protected) `0x143f55a64` | **1** | **1** | `0x143f55a5a mov r9d, 1`, `0x143f55a50 mov dword ptr [rsp+0x28], r9d` |

   **The only non-zero foul kinds in the whole image are 1 (offside), 2
   (direct foul) and 3 (indirect / dive).** There is **no hand/arm kind
   and no "handball" restart anywhere**.
3. The **foul-type byte** (`JR+0x55`) is set from the fouler's action id
   (the `PLAY_DATA_FOUL_*` pool at `0x142b42d60` has **no** handball
   member): contact / sliding / block / tackle / tackle-shoulder / kick /
   keeper / face / force.
4. The `PLAY_DATA_*` name pool (`0x142b42d60..0x142b43648`, read this
   session) contains **no** `PLAY_DATA_FOUL_HAND*` entry.

This **confirms and strengthens** `pes-re-duels-fouls.md` §4 (which rated the
negative "INF hi") to **CONFIRMED**: the exhaustive producer and foul-kind
enumerations leave no room for a handball offence.

**What GF would need to carry:** nothing — but GF must **not** add one
without a `// RE-NEW:` label, because the brief's rule is "PES is king".

---

## 5. K12 — time-wasting / late-game rules (CONFIRMED negative)

`pes-re-match-management.md` §4 already decoded this as negative. The
supporting evidence was re-checked here:

* The CPU **strategy decider** `FUN_14051e970` (entry `0x14051e970`,
  `push rbp`) reads only the score, the clock minute, the team-strength
  difference, the match-mode word and two RNG-held minute limits. The only
  "leading late" behaviour is **strategy 1 → attack level 1** (defensive)
  after `R[6] = 28 + rand(10)` minutes; there is **no** 80′/85′ boundary, no
  keep-ball instruction and no corner-camping target.
* The only minute constants in the management code are 10/20, 28, 14/15,
  24/25 and 100 — **no** 80 and **no** 85 (OBS, prior report §4).

**This is a confirmed negative, not a gap.** GF's own 80/85-minute rules are
GF-specific and must be labelled `// RE-NEW:` (or deleted at cutover if the
brief's clean-cutover rule is applied to them).

---

## 6. OPEN items, and what would settle them

| # | item | blocker | how to settle |
|---|---|---|---|
| K9-1 | the **writer** of `MA+0x40+0x14*t` (the back line DOGSO reads) | not traced | trace the team-shape writer chain (`FUN_1405fbcb0` / `FUN_14051d9e0`) to the float store |
| K9-2 | whether `MA.backLine` is the last or the second-last defender | semantics only | live read of `MA+0x40` vs the second-deepest defender's x in a match |
| K9-3 | the exact meaning of the fouler's `AI+0x1480` "predicted interception point" | INF(med) | read the AI record live during a through-ball |
| K11-1 | whether the abandon path can ever run in PES 2021 | the writer was not found | live, read-only: force 5 red cards (or read `MI+0x1574`/`MI+0x1775` every frame) during a match; or a live PES **disconnection** match, which is the most likely way to reach `FORFEITEDGAME` |
| K11-2 | the source records of the four record-sourced stop-reason writers | not traced | they are menu/online records; check `0x140d1d770` / `0x1410a7ace` / `0x1413e7bbd` / `0x1420bd7e5` callers |
| E15-1 | the fatigue injury cause | not in the Injury object | search the fatigue-drain path for a `PLAY_DATA_INJURY_FATIGUE` producer |
| E15-2 | the attacker action class mapping (11..12 / 13..14) | INF(med) | cross-reference the action id table |
| K7-1 | none — the negative is closed | — | — |
| K12-1 | none — the negative is closed | — | — |

---

## 7. Checker

`<workdrive>/gf-MissingRE-scratch/check_missing.py` re-verifies **every VA
and every constant quoted above** by reading the bytes at the named address
in `pes_full.bin` and comparing the *decoded instruction text* (capstone) or
the *float value* against the literal string quoted in this report. It needs
no PES tables, no network and no live process.

```
$ python3 check_missing.py
...
95/95 checks passed
```

Mutations (each one overlays a mutated byte range **in memory only** — the
image on disk is shared with the Ghidra bridge and with other agents, so it is
never written — re-runs the whole suite and clears the overlay):

```
$ python3 check_missing.py --mutate K9.thr.in_box_xor
93/95 checks passed
MUTATION K9.thr.in_box_xor: 2 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate E15.sev.thr_A_70
90/95 checks passed
MUTATION E15.sev.thr_A_70: 5 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.consume_reason10
94/95 checks passed
MUTATION K11.consume_reason10: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate E15.jp.thr_C_5pct
93/95 checks passed
MUTATION E15.jp.thr_C_5pct: 2 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K9.judge.dj_to_yellow
94/95 checks passed
MUTATION K9.judge.dj_to_yellow: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K7.reason_foul_commit
94/95 checks passed
MUTATION K7.reason_foul_commit: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K9.paction.victim_fell
94/95 checks passed
MUTATION K9.paction.victim_fell: 1 check(s) fired (expect >= 1)
```

**Checker summary line: `95/95 checks passed`; the seven mutations above each
fired ≥ 1 check (2, 5, 1, 2, 1, 1, 1) and the image file was never written
(md5 `516c7d9722094226cfe103c0f15bb2e3` before and after the whole session).**

The checker is **capstone-only** — it does not need the Ghidra bridge or the
GUI's program to be open, so it keeps working while the Ghidra server is
being restarted.

---

## 8. Reproduction notes

* Image: `~/ghidra-projects/scratch/Unpacker/pes_full.bin`, file offset =
  VA − `0x140000000`.
* Decode helpers (scratch):
  * `dec.py <VA> <nbytes>` — capstone disassembly with the rip-relative
    constant of every memory operand resolved **and its float value read
    from the image**;
  * `callscan.py <target-VA> <lo> <hi>` — every `E8 rel32` call to a target,
    with the 40 bytes before the call disassembled so the argument setup can
    be read;
  * `dispfind.py <disp> <lo> <hi>` — every instruction whose memory operand
    has a given displacement (scans for the `disp32` pattern, then decodes
    backwards; robust against linear-sweep desync);
  * `rawscan.py <value> <lo> <hi>` — raw byte-pattern search (used for the
    "no writer of 10 / no writer of 0x0b..0x0f" negatives);
  * `check_missing.py` — the checker of §7.
* Ghidra bridge: `http://127.0.0.1:8080/decompile_function?address=0x…`,
  `/xrefs_to?address=0x…`. The decompile was used to *locate* logic; every
  constant and every branch condition quoted above was re-read from the raw
  bytes.
