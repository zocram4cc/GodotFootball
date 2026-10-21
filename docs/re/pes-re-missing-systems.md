# PES 2021 — the missing referee / injury systems: DOGSO, the abandon rule, injury resistance (and the two referee negatives)

> **Main audit 22-10-26:** K11 SOLVED and Main-audited at the bytes: FUN_140a23260: `0x140a23279 cmp [r12+0x1168],0xa` (period 10), `0x140a232c6 cmp byte [rbx+r12+0x1748],0` (on the pitch), `0x140a23308 cmp [rax+0x14],0` (no replacement), `0x140a2330e cmp [rax+8],4` (sent off), `0x140a23322 mov byte [rax+0x1b70],1` (forfeit flag), `0x140a23329 mov dword [rax+0x1b74],3` (the 3): match. The owner's live evidence (endless matches voided 3-0, subs depleted, clock --:--, no scorers) matches. The earlier "dead code" claim is withdrawn. DOGSO and injury resistance found; no handball or time-wasting rule exists.
> **Main note, round 2 (MissingRE2):** period 10 is post-match (the period table in §2.9a); the minimum-players rule is FUN_140447B50 case 6: a side with fewer on the pitch than E+0x17 (live: 7; `cmp al,[rdx+0x17]` at 0x140447E79) gets period 10 at the next stoppage (through the setter call `call 0x14102cad0` at 0x140447F0F; Main-checked: a call, not a direct store). player+0x18 is an injury marker, not a replacement count (corrects round 1). A second forfeit writer FUN_140A23380 exists.

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
| **K11 abandon** | **CONFIRMED: the minimum-players rule reaches period 10 without the clock; threshold `E+0x17` = 7 (live)** | `FUN_140447b50` case 6 (§2.13) counts each side's on-pitch players (`FUN_140a3bfc0`). A side under `E+0x17` gets period 10 (`DRV+0x2790 := 10`), restart 7 and stop reason 2, and is named in `DRV+0x2aa8`. The endless patch blocks only the clock gate, so an endless match reaches post-match (§2.9a). The 3-0 is written by `FUN_140a23260` (period 10; on-pitch `player+0x0c == 4` and `player+0x18 == 0`) or by `FUN_140a23380` (any period; score = `player+0x20`). Which one fired in the owner's runs is OPEN. |
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
> **MissingRE2 update (21-10-26):** the period-10 gate is now closed and the threshold is sourced. §2.9a gives the period table and the writers of period 10; §2.13 gives the minimum-players rule. Where the text below calls the period gate OPEN, §2.9a supersedes it.


> **CORRECTED 21-10-26 after the owner's live observation.** The owner's
> endless-match runs (the endless patch, `tools/dev/pes_endless_match.py`,
> which raises the clock bounds in `FUN_14050b9b0`'s `.rdata`) were
> **cancelled and voided 3-0 by the minimum-players rule**. So the abandon
> path **does run live**; the "dead code" reading below is **REFUTED**.
> The searches in §2.2 are still valid as *searches*, but the conclusion
> drawn from them was wrong: the writer is reached in a way those searches
> did not cover. See §2.6 for what the re-opened search established and
> §2.7 for the corrected status.

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

### 2.2 The negative result (CONFIRMED as *searches*, REFUTED as a conclusion)

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
   access in `.text` (**1026** sites, of which **1010** are the SIB-indexed
   `cmp byte [reg + reg + 0x1748], 0` predicate and **all** are `cmp`
   predicates, never a summation) and in the protected region (**10** sites)
   found **no** `cmp` against 6/7/8/10/11 paired with a counter increment and
   a subsequent stop-reason write.

**These searches are still factually correct** (the byte patterns really do
have zero hits), but the *conclusion* drawn from them — "the abandon path
is dead code" — is **REFUTED** by the owner's live observation. The
explanation is that the rule does **not** go through the paths searched:
the abandon is produced by a path that writes the match record through a
**pointer** (the match record is reached as `*(G+0x270)`, and the mode byte
is copied into it by the record copy `0x140414080`, not written in place),
and the count comes from a structure other than `MI+0x1748`.

### 2.3 The threshold is now sourced: `E+0x17`, live 7 (MissingRE2, 21-10-26)

The count rule compares the on-pitch count with a settings byte, not with a constant: `cmp al, byte ptr [rdx + 0x17]` at 0x140447e79, with `rdx = *(G+0x2c0)` (§2.13). The live value in the owner's match is 7. The first-pass search below found no compare against 7 tied to a player count. That stands for constants, and is kept as history. The nearest numeric 7 in `.text` near a
`0x1748` access is `0x1404251d0 cmp dl, 7` — a **match-setup mode** byte
(`*(G+0x2ad0)`, the competition kind), not a player count.

### 2.4 The live observation (CONFIRMED, read-only live read)

A PES 2021 process is running with the endless patch applied (pid 2510857).
Sampling `MI` every 0.2 s (read-only `pread` of `/proc/<pid>/mem`, no
writes, no pins) gives the live state of the referee fields:

```
f=1232176 per=1 play=1 restart=0 reason=0 kind=0 card=0
        | 1774=0 1775=0 1776=0 1777=0 | onpitch t0=10 t1=11
        on=1111111111011111111111
```

i.e. **team 0 already has only 10 players on the pitch**: slot 10 is the off flag (pid 4, `player+0x18 = 1`, an injury marker, not a red card; §2.12). Team 1 has 11. The mode byte `MI+0x1775` is **0**, the void flag `MI+0x1776` is **0** and the stop reason is 0. The minimum-players rule did not fire: the live threshold `E+0x17` is **7** (§2.13), so 10 is not short. The earlier "slot 9 is the red card" reading was wrong.

Scripts: `mi_read.py` (one-shot) and `mi_watch.py` (a change-logging
watcher) in the scratch dir. Both are strictly read-only.

### 2.5 What GF would need to carry

* a `StopReason::Abandon` (10) enum value, matching `MI+0x1574`;
* a match-mode byte matching `MI+0x1775` with the abandon values `0x0b..0x0f`;
* the abandon programme `[0x0f, 0x45, 0x32, 0x0f]`;
* a **minimum-players count** and the threshold, once located.

### 2.6 Re-opened search: what was found (and what was not)

| lead | result |
|---|---|
| the mode byte's JR/MI alias | `MI+X = JR+(X−0x1554)`, so the mode byte is `JR+0x221` and the void flag is `JR+0x222`. The A5 dispatcher `FUN_1404526a0` reads `MI+0x1775` = `JR+0x221` directly (`0x1404526b7 movzx eax, byte ptr [r8 + 0x1775]`, where `r8 = *(G+0x270)`). |
| the dispatcher's abandon branch | `0x1404526bf cmp eax, 9` / `0x1404526c8 cmp eax, 0xa` / `0x1404526cd cmp eax, 0xf` / `0x1404526d2 cmp eax, 0x10` → `0x1404526db call 0x14044ffd0` (the abandon builder). CONFIRMED. |
| writers of `JR+0x221` (the mode byte) | 6 sites: `0x140713042` (1), `0x14071307c` / `0x1407130de` / `0x1407131a8` (0), `0x141245866` (register), `0x141f7b7d0` (0). **None** writes the abandon values `0x0b..0x0f`. |
| **no writer of the mode byte exists at all** | An exhaustive operand scan of the whole image found **107** instructions with a `+0x1775` memory operand; **all** are reads except the record copy `0x1404144e2` and the setter `0x140a3ccf0`. A separate scan for **16-byte** stores (`movups`/`movdqu`/`movaps`) whose displacement covers `+0x1775` (disps `0x1766..0x1775`) found **zero**. So the mode byte is never written by a store of any width. |
| the record copy | `0x140414080` copies `0x22 × 0x80` bytes (0x1100) from `rdx` to `rcx` and **includes** `+0x1775` (`0x1404144e2 mov byte ptr [rbx + 0x1775], al`). It is reached through 22 thunk call sites (`0x14041b610` / `0x14041b620` / `0x14041b630`), and the *source* record is `*(obj+0x10) + 0x2380` (`FUN_14044bb40`). |
| writers of the void flag `JR+0x222` | 5 sites set it to 1: `0x141f7b232`, `0x141f7b246` (`FUN_141f7b1c0`), `0x141f7b314` (`FUN_141f7b2f0`), `0x141f7b61a` (`FUN_141f7b5e0`), `0x141f7b66a`; 2 clear it: `0x141f7af6f`, `0x141f7b7e4`. `FUN_141f7b5e0` also writes `JR+0x220` = 1 and clears `+0x128`, `+0x209`, `+0x210`, `+0x214`, `+0x21c` — it is a **"stop the match"** routine. Its vtable is `0x142bb0d90`; the slot is at `+0x40`. |
| the abandon *completion* | `FUN_141f7b770` (vtable slot `+0x20`) reads the mode byte and the void flag, **clears the mode byte** (`0x141f7b7d0 mov byte ptr [rcx + 0x221], 0`), clears the void flag (`0x141f7b7e4`) and sets `param_2+0x65 = 1` — the "the match is over" flag of the result object. |
| the on-pitch count | `MI+0x1748[0..21]` is read by **1010** SIB-indexed `cmp …, 0` predicates in `.text` and 10 in the protected region; **every** one is a per-slot predicate, **none** sums them. The count used by the subs machinery (`FUN_140521e40`, `FUN_140522300`) is built by *class* (GK/CB/… per `team+0x4c00`), not by a plain sum. |
| the "3-0" void outcome | the goal-sum record is read by `FUN_1408c9190` at `stats+0x20c+(period*9+slot)*4`; the writer was not located. `FUN_141fd82e0` is the match-result builder and reads the goal sum heavily. |
| the period-end writer | `thunk_EXT_FUN_143f559f0` (protected) is the period-end writer; its bytes are clean and contain **no** player-count test. |
| the time-up gate | `FUN_14050b9b0` is the clock gate the endless patch neutralises; its bytes contain **no** player-count test. |
| the team-change "no replacement" give-up | `FUN_140523110` (the red-card repair) returns **without queueing a substitution** when `FUN_140521930` finds no replacement (`iVar4 == 0xff` → `return`). `FUN_140521e40` / `FUN_140522300` return `0xff` when no eligible player exists. This is a real "a side has run out of players" condition, and it is the only one found. |

**So the rule is not in the referee, not in the period-end writer, not in the
clock gate, and it is not a sum over `MI+0x1748`.** Two things are now
certain that were not before:

1. **the mode byte has no store anywhere in the image**, so the abandon
   cannot be driven by `MI+0x1775` being set to `0x0b..0x0f` — those
   dispatcher branches are reached some other way (or the byte is set by
   code in the *protected* region through a pointer whose base is not
   `MI`, which the `+0x1775` displacement scan cannot see);
2. **the abort is a "stop the match" event** (`FUN_141f7b5e0` → mode 1, void
   flag 1 → `FUN_141f7b770` clears them and raises the result flag), i.e.
   the same object that carries the `FORFEITEDGAME_*` / `GIVUP_*` /
   `ABNORMALEND_*` outcomes (`0x142ad90a8`) and the `DiscontinueRequest`
   notice (`0x1425ab99d`).

The remaining place for the **condition** is the **team-change /
substitution machinery** (`match::ai::TeamMemberChange`, `FUN_140521000` /
`FUN_140521e40` / `FUN_140522300` / `FUN_140521930`), which is where the game
decides that a side has run out of usable players, and the **match-result
layer** (`FUN_141fd82e0`, `FUN_14216c680`), which is where the 3-0 void
score is written.

### 2.7 **THE WRITER — FOUND (CONFIRMED): `FUN_140a23260`, the 3-0 forfeit score**

The 3-0 forfeit score is written by **`FUN_140a23260`** (entry `0x140a23260`,
ends `0x140a23378`). Raw bytes:

```
0x140a23268: mov      rcx, qword ptr [rip + 0x2cd0b59]   ; 0x1436f3dc8 = G
0x140a2326f: mov      r13, rdx
0x140a23272: mov      r12, qword ptr [rcx + 0x270]        ; MI = *(G+0x270)
0x140a23279: cmp      dword ptr [r12 + 0x1168], 0xa       ; *** ONLY IN PERIOD 10 ***
0x140a23282: jne      0x140a23370                        ;     otherwise: return
0x140a232a1: xor      r14d, r14d                          ; r14d = side (0..1)
0x140a232a4: xor      ebx, ebx                            ; rbx  = slot (0..21)
0x140a232b0: mov      edx, r14d
0x140a232b3: call     0x140a489d0                         ; team record of side r14d
0x140a232bb: xor      edi, edi                            ; edi = slot within the side
0x140a232c0: cmp      rbx, 0x15
0x140a232c4: ja       0x140a23333
0x140a232c6: cmp      byte ptr [rbx + r12 + 0x1748], 0     ; *** IS THIS SLOT ON THE PITCH? ***
0x140a232cf: setne    cl
0x140a232d2: test     cl, cl
0x140a232d4: je       0x140a23333                        ;     not on the pitch -> skip
0x140a232d6: mov      edx, edi
0x140a232d8: mov      rcx, r15
0x140a232db: call     0x140a3ae30                         ; player id of (team, slot)
0x140a232e0: mov      esi, eax
0x140a232e2: cmp      eax, 0x28
0x140a232e5: jge      0x140a23333
0x140a232e7: mov      edx, eax
0x140a232e9: mov      rcx, r15
0x140a232ec: call     0x140a3af30                         ; the player object
0x140a23303: call     0x140a73e20                         ; rax = player + 4  (the match record)
0x140a23308: cmp      dword ptr [rax + 0x14], 0           ; *** player+0x18 == 0 ***
0x140a2330c: jne      0x140a23333                        ;     (a value there -> skip)
0x140a2330e: cmp      dword ptr [rax + 0x8], 4            ; *** player+0x0c == 4 ***
0x140a23312: jne      0x140a23333                        ;     (a different state -> skip)
0x140a23314: mov      r8d, esi
0x140a23317: mov      edx, r14d
0x140a2331a: mov      rcx, r13
0x140a2331d: call     0x1408c7210                         ; the per-team result record
0x140a23322: mov      byte ptr [rax + 0x1b70], 1          ; *** FORFEIT FLAG = 1 ***
0x140a23329: mov      dword ptr [rax + 0x1b74], 3         ; *** THE 3  (the forfeit score) ***
0x140a23333: inc      edi
0x140a23335: inc      rbx
0x140a23338: cmp      edi, 0xb
0x140a2333b: jl       0x140a232c0                        ;     next slot of this side
0x140a2333d: inc      r14d
0x140a23340: cmp      r14d, 2
0x140a23344: jge      0x140a23352                        ;     next side
0x140a23346: mov      rcx, qword ptr [rip + 0x2cd0a7b]   ; 0x1436f3dc8 = G
0x140a2334d: jmp      0x140a232b0
```

**The rule, in one line:**

> In period **10** (the endless / extra period), if **any** player who is
> **on the pitch** (`MI+0x1748[side*11+slot] != 0`) has
> **`player+0x0c == 4`** (the "sent off, no replacement" state) **and**
> **`player+0x18 == 0`** (nothing available to put in his place), then the
> team's result record gets **`+0x1b70 = 1`** (forfeit flag) and
> **`+0x1b74 = 3`** — the **3-0** forfeit score.

The per-team result record is `FUN_1408c7210(stats, side, period)` =
`stats + 0x64d8 + side*0x59650 + period*0x2134` (`0x1408c7210`), so the
forfeit lands in the record of **the side that owns the offending player**
— i.e. the side that ran out of players is the side that is **written 3
against** (a 0-3 loss for them, the "3-0" the owner saw).

**The reader that consumes the forfeit** is `0x140a105b5 cmp byte ptr
[rax + 0x1b70], 0` / `0x140a105be cmp dword ptr [rax + 0x1b74], 3` — the
same two fields, tested together: *forfeit flag set* **and** *forfeit score
== 3* ⇒ the team is counted as having **lost 3-0**; the clock and the
scorers are blanked by the same path (the owner's observation that the time
shows `--:--` and nobody gets a goal).

**CORRECTION (MissingRE2, 21-10-26): `FUN_140a23260` is not the only forfeit writer.** `FUN_140a23380` (case 0 of `FUN_140a22130`, record sub-kind 2) sets `+0x1b70 = 1` and `+0x1b74 = player+0x20` for every player who is on the pitch (`MI+0x1748`) or at `MI+0x175e` and has `player+0x18 == 1`. The writes are at 0x140a2361a and 0x140a23629 (CONFIRMED bytes; the gate is from the decompile). It has no period test. The writer above (`FUN_140a23260`) is gated on period 10. The reader needs score 3, so the second route yields 3-0 only when `player+0x20 == 3` (§2.13).

### 2.8 The call path into the match tick

```
per-team stats tick  FUN_140a0d190 (0x140a0d190)
 ├ FUN_140a0c3e0(param_1, *(param_2 + 0xe0))   : the record kinds of the other side
 ├ FUN_140a23c10(param_1, param_2)
 ├ loop over the record kinds:  FUN_140a3e8e0 / FUN_140a3e570
 │    └ FUN_140a22130(param_1, param_2, kind)     @0x140a0d33e
 │         switch (*kind):
 │           case 1: FUN_140a23260(param_1, stats, param_2, kind)   @0x140a22294   <== FORFEIT
 │           case 0: FUN_140a23380   case 4: FUN_140a236a0   case 6/7: ...
 └ if (*piVar4 != 0x37 && piVar4[5] == 10) *(uint *)(param_1 + 0x2048) = 1
```

`FUN_140a22130` is the **per-team statistics-record updater**; the "kind"
byte selects which part of the team record is recomputed, and **kind 1** is
the one that carries the forfeit. `FUN_140a0d190` is the per-team tick that
runs it, so the forfeit is evaluated **every tick**, not at a stoppage —
which is why the endless match voids mid-play.

### 2.9 Why this was invisible to the earlier searches (and what they still prove)

* the forfeit is **not** a stop reason and **not** the mode byte: it is a
  field in the **per-team statistics record** (`stats+0x64d8+…+0x1b70/0x1b74`),
  a completely different object from the match record `MI`;
* the "no replacement" condition is **per player** (`player+0x18 == 0`), not
  a count of the on-pitch flags — so no `cmp` against a player count exists
  (as the searches of §2.2 item 3 showed);
* the period gate is `MI+0x1168 == 10`.

### 2.9a Period 10 is the post-match phase; how an endless match reaches it (MissingRE2, 21-10-26)

**Plain answer.** Period 10 (`MI+0x1168`) is **full time / post-match**: the match is over. An endless match reaches it through the **minimum-players rule** (§2.13), not through the clock. The endless patch blocks only the time-up gate `FUN_14050b9b0`, and `FUN_140447b50` never calls that gate.

Period table. Writers are CONFIRMED by bytes at the VA unless marked; behaviour is from the decompile.

| period | meaning | writer (VA) |
|---|---|---|
| 1 | 1st half; clock runs (time-up threshold 162000) | match-setup function at `0x1404558b3` (`mov edx, 1`, CONFIRMED) |
| 2 | half-time break; judge tick skipped | switch 1→2, `0x140449925` (CONFIRMED) |
| 3 | 2nd half; clock runs | kick-off setup record `G+0x2a80`, `int` at `+0x60` (default 1 when 0), written by `FUN_1404514f0` event 0x14 (decompile) |
| 4 | break before extra time (tied, extra time enabled) | `FUN_1404493d0` returns 4 (decompile); `FUN_140443e30` runs on that path |
| 5 | extra time, 1st half; clock runs (threshold 54000) | same record `+0x60` (INF) |
| 6 | extra-time break; judge tick skipped | switch 5→6, `0x140449912` (CONFIRMED) |
| 7 | extra time, 2nd half; clock runs | same record `+0x60` (INF) |
| 8 | break before the shoot-out (tied, shoot-out enabled) | `FUN_1404493d0` / `FUN_1404499d0` return 8 (decompile) |
| 9 | penalty shoot-out; judge tick skipped | `FUN_1404a51b0` first state, `0x1404a5295` (CONFIRMED) |
| 10 | post-match / full time; judge tick skipped | four writers, next |

Writers of 10:
- **a. Normal switch.** `FUN_1404494e0` under `DRV+0x49c0 == 7`, with `MI+0x1775 == 0`, from period 3 or 7. `FUN_1404493d0` (from 3) returns 4 when tied with extra time enabled, 8 when tied with the shoot-out enabled, and 10 otherwise. `FUN_1404499d0` (from 7) returns 8 when tied with the shoot-out enabled, and 10 otherwise (decompile). Calls at `0x14044991c` and `0x140449909`; store at `0x140449931` (CONFIRMED).
- **b. Minimum players.** `FUN_140447b50` case 6, store at `0x140447f0f` (§2.13; CONFIRMED).
- **c. Step 0x43.** `FUN_140452e00` (caller `0x14044cbbc`), store at `0x140452e6e`, with UI event `0x1050054` (`mov edx, 0x1050054` at `0x140452e58`). It also sets restart 7 and stop reason 1, and queues steps 0x10, 0x26, 0x45. The builder that queues 0x43 (near `0x14044f94a`) is not named.
- **d. End-of-match state machine.** `FUN_1404a3fa0` (steps 0x29–0x31 through `FUN_14044a180`, call at `0x14044a22a`). State 2 runs `FUN_140447b10` (kind 0xb) at `0x1404a4089`, then writes 10 at `0x1404a409a`. State 3 writes 10 (decompile).

Why 10 is post-match and not a timeout: the judge tick is skipped in periods {2, 6, 9, 10} (mask `0x644`, `0x14050c3e0`). The period-end cutscene table maps 8 and 10 to the full-time scene `FUN_140486b10`, and `FUN_1404a3fa0` state 0 keys on the end-of-match scene id `0x7002b`. The period-10 programme holds step 0x26 (post-match UI) and 0x24.

Why an endless run reaches 10 (static chain):
1. The time-up whistle is `FUN_14050c3e0` → `FUN_14050b9b0` (gate call at `0x14050c425`) → the protected period-end writer, which sets restart 7 (`lea r8d, [r9 + 6]` at `0x143f55a60`, with `mov r9d, 1` at `0x143f55a5a`). The endless patch makes the gate false. The whistle also fires when `judge+0xdc != 0`; the only writer of that field I found is the judge reset `FUN_14050aa30` (decompile and store scan; INF that no other non-zero writer exists).
2. Route b does not need the gate. A side under `E+0x17` (7, live) makes the next restart programme that reaches step 0x13 write period 10.
3. Live (21-10-26, read-only): the owner's match is in period 1 with 10 and 11 on the pitch, so route b has not fired in that snapshot. The owner's 3-0 is consistent with route b plus a forfeit writer (§2.7, §2.12).

Correction to §4 (K7): the protected period-end call stores its foul kind from the incoming `r9d` at `0x143f55a50`, before `mov r9d, 1` at `0x143f55a5a`. The foul kind is therefore not 1, and the K7 table entry for that row is wrong.

Correction to the first pass: "period 10 is the endless period" was wrong, and so was "an endless run cannot reach 10". Route b reaches it without the clock.

Open: which route the owner's runs used (b, c or d); whether the step-0x43 and step-0x29 builders can run in an endless match (their triggers are not traced); the writers of `E+0x66` / `E+0x67` (the extra-time and shoot-out flags).

### 2.10 LIVE VALIDATION of the mapping (read-only, 21-10-26)

The mapping was re-derived **fresh** from the running process (pid 2510857,
no cached pointers, no writes, no pins — the brief forbids pinning
`MI+0x1168/0x116c/0x1574`):

| item | value read live | how |
|---|---|---|
| `G` | `0x1eb942c0` | `*(0x1436F3DC8)` |
| `MI` | `0x1d404608` | `*(G+0x270)` (it is **null in ~4-9 % of polls** — the record is torn down/rebuilt; every script retries) |
| period id | **1** | `int32 at MI+0x1168` — a **dword, not a pointer** (`0x14050b9fa lea rcx,[r8+0x1168]` then `0x14050ba0c mov eax,[rcx]`) |
| play state | 1 / 2 / 3 | `int32 at MI+0x116C` |
| on-pitch flags | `1111111111011111111111` | `MI+0x1748[0..21]` — team 0 has **10** on the pitch (slot 10 off, injured), team 1 has 11 |
| team record (the one the forfeit loop walks) | side 0 `0x1d471e04`, side 1 `0x1d48ff14` | `*(G + (0xa3+side)*0x28)` = `FUN_140a489d0`; the attack sign at `+0x254` read **+1 / −1**, which confirms the mapping |
| slot → player id | `[0,5,3,17,1,18,6,19,22,9,4]` (side 0) | `*(u32)(team + 4 + slot*4)` = `FUN_140a3ae30` |
| player object | `team + 0xa8 + slot*0x128` | `FUN_140a3af30` |
| `player+0x0c` | **0** for every on-pitch player | the per-player match state |
| `player+0x18` | **0** or **1** | the injury marker (§2.12): `FUN_140523910` queues a replacement for `!= 0` |
| `FORFEIT` | **empty** | no on-pitch player has `+0x0c == 4` — consistent with the owner's report that **nobody has been sent off yet** in the live match (only injuries) |

**So the live match satisfies neither gate**: no send-off (`player+0x0c` is 0
for every on-pitch player) and **period 1, not 10**. The live match is an
endless run, and the endless patch raises only the *clock* threshold, so
the period never advances and this rule cannot fire there. See §2.9a for the
conflict with the owner's observation and the next probe.

### 2.11 What GF would need to carry

* a **forfeit** outcome on the team result record (flag `+0x1b70` + the
  score `+0x1b74 = 3`);
* the per-player **"sent off with no replacement"** state
  (`player+0x0c == 4` and `player+0x18 == 0`) — GF needs the equivalent of
  "a red card and the bench cannot supply a replacement";
* the **period-10** gate, if GF is to reproduce the behaviour (GF's own
  endless mode would need the same period id);
* the result display: the clock shown as `--:--` and the scorers cleared
  (the owner's observation).

### 2.12 The K11 fields, closed or narrowed (MissingRE2, 21-10-26)

| field | meaning | evidence |
|---|---|---|
| `player+0x0c` (match record `+8`) | a removal marker. Values `> 1` make the red-card path try a kind-2 replacement. The forfeit tests the exact value **4** (`cmp dword ptr [rax + 8], 4`, 0x140a2330e). The writer of 4 is OPEN. Meaning of 4 beyond "a `> 1` removal state": INF(med) | `FUN_140523110` reads `mov ecx, dword ptr [rax + 8]` at 0x140523181 and tests `> 1` (decompile). Live: 0 for every on-pitch player |
| `player+0x18` (match record `+0x14`) | an **injury marker**, not a replacement count. `FUN_140523910` (injury scan) queues a kind-2 replacement for an on-pitch player with `+0x18 != 0` (CONFIRMED read at 0x140523981). Live (21-10-26): injured players show `+0x18 = 1`, on-pitch side 0 slots 4 and 7, side 1 slot 10; off-pitch side 0 slots 8 and 10. Writer: OPEN | `mov ecx, dword ptr [rax + 0x14]` at 0x140523981 |
| `MB+0x2048` (brain flag) | consumer = `FUN_140a0d190`. Value 0: the full brain tick, which runs the kind loop (`FUN_140a22130`, so both forfeit writers) and then sets 1 when record kind 10 is found. Value 1: a one-shot setup tick that sets 2. Values 2 and -1: no-op. So the forfeit evaluation runs only while the flag is 0; the earlier "every tick" wording holds only in that state | reads at 0x140a0d1df; writes 1 at 0x140a0d3d5 and 2 at 0x140a0d2e5 (CONFIRMED bytes). -1 at 0x140ce7c03 and `eax` stores at 0x140cecc45 / 0x141cc9de7 come from other owners, not traced |
| the other side's record | `FUN_140a23260` writes `FUN_1408c7210(stats, side, period)` for the side of the matching player only (`mov edx, r14d` at 0x140a23317, then the call at 0x140a2331d). The other side is written only if its own player matches. `FUN_140447b50` case 6 writes `DRV+0x2aa8` = the short side. Reader of `DRV+0x2aa8` for the 3-0: OPEN | 0x140a23317 (CONFIRMED bytes); 0x140447ec9 (CONFIRMED bytes) |

### 2.13 The minimum-players rule: `FUN_140447b50` case 6 (CONFIRMED bytes, 21-10-26)

The owner's "minimum-players abandon" is this rule. It runs inside the restart sub-machine, and it does not use the clock, so the endless patch does not touch it.

1. Entry: `FUN_14044c880` case 0x13 calls `FUN_140447b50` at **0x14044ca10** (checker `K11.mp_caller`). Restart-programme builders queue step 0x13: for example `FUN_14044f1c0` (stores at 0x14044f24b, template 0x16, 0x12, 0x13, 0x14, 0x15 ...), and the period-8 end programme `[0x13, 0x29]` from `FUN_1404505d0` (decompile). So the check runs at a stoppage whose programme reaches 0x13.
2. Gates: the function returns at once when `MI+0x1775 != 0` (the mode byte) or when the tutorial flag `G+0x3b0` is set (decompile). The count runs in sub-state 6 (`DRV+0x8f2`), gated by `cmp byte ptr [rbp + 0x1775], r14b` at 0x140447e4a (`MI+0x1775 == 0`).
3. Count: `FUN_140a3bfc0(MI, side)` at 0x140447e66 counts the nonzero bytes `MI+0x1748+11*side` over 11 slots (`mov byte` flags of the on-pitch array; decompile).
4. Threshold: `cmp al, byte ptr [rdx + 0x17]` at 0x140447e79, with `rdx = *(G+0x2c0)` = `E`. A side is short when its count is below `E+0x17`; the flag is stored at 0x140447e7e. **Live (21-10-26, `minplayers_read.py`): `E+0x17 = 7`**, with 10 and 11 on the pitch, so no trigger in that match.
5. Forfeit side: one flagged side is the short side (decompile: `esi = (flag0 == 0)`). Both flagged: decided by `MI+0x1558` (decompile, not byte-checked). Stored at `DRV+0x2aa8` (0x140447ec9), and `DRV+0x2aac` follows.
6. Period 10: `mov edx, 0xa` at 0x140447ef8, then `FUN_14102cad0(DRV+0x2790, 10)` at 0x140447f0f.
7. Restart 7 and stop reason 2 in the DRV copy: `mov edx, 7` at 0x140447f14 with `lea rcx, [rdi + 0x49c0]` at 0x140447f19, then the setter at 0x140447f20; `mov edx, 2` at 0x140447f25 (stop reason via `FUN_141b8b250`, decompile). `FUN_1404a7d60` then copies the DRV restart record into `MI+0x1554` (argument order CONFIRMED; the publish semantics are INF). With `MI+0x1574 == 2`, case 0x13 calls `FUN_1404505d0`, the period-10 end programme (steps 0x26 and 0x24, and 0x0f for stop reason 2).
8. The 3-0 then needs `FUN_140a23260` (§2.7): an on-pitch player with `player+0x0c == 4` and `player+0x18 == 0` (0x140a23308, 0x140a2330e).

What the owner saw fits this rule. An endless run has no time-up, so the clock never ends a half; the count rule still fires at the next stoppage once a side has fewer than 7 on the pitch. Subs depleted plus injuries (the `+0x18` markers) gives that count. Not verified: which stoppage fired it, and why the scorers were blanked and the clock showed `--:--` (INF: period 10 is the post-match phase, and its clock is hidden).

A second, un-gated route to the 3-0 exists (§2.7, `FUN_140a23380`, any period). If an on-pitch or flagged injured player has `player+0x20 == 3`, the reader would count the side as 3-0. Live `player+0x20` values for injured players were 1 and 2 (`score_read.py`, 21-10-26). Which route the owner used is OPEN.

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
   | period end (protected) `0x143f55a64` | **1** | **incoming `r9d`** (not 1; corrected 21-10-26) | `0x143f55a50 mov dword ptr [rsp+0x28], r9d`, stored before `0x143f55a5a mov r9d, 1`; restart 7 comes from `lea r8d, [r9+6]` at `0x143f55a60` |

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
| K11-1 | the **writers** of period 10 and of the forfeit result | **Period-10 writers: CLOSED** (four routes, §2.9a: switch 3/7→10, `FUN_140447b50` case 6, `FUN_140452e00` step 0x43, `FUN_1404a3fa0` states 2/3). **Forfeit-result writers: partly OPEN**: `FUN_140a23260` (period 10) and `FUN_140a23380` (any period; score = `player+0x20`) are both known; which one fired in the owner's runs, and the meaning of `player+0x20`, are not settled | live: log `player+0x20` of injured players across a run (`score_read.py`); decode the `player+0x20` writers; trace the reader at 0x140a105b5 and its use of `DRV+0x2aa8` |
| K11-2 | the **count and threshold** | **CLOSED**: count = `FUN_140a3bfc0` (nonzero `MI+0x1748` flags per side); threshold = `E+0x17`, read live as **7** in the owner's match (`minplayers_read.py`). **OPEN**: the writer of `E+0x17` (the settings loader; not located) | trace the settings loader that fills `G+0x2c0` |
| K11-3 | the 3-0 void score write | `FUN_140a23260` writes flag 1 and score 3 for the offending side (period 10). `FUN_140a23380` writes flag 1 and score `player+0x20` (any period). The reader at 0x140a105b5/0x140a105be needs flag and score 3. **OPEN**: which of these produced the owner's 3-0 | as K11-1 |
| K11-4 | whether the rule is "no replacement available" rather than "players on the pitch < N" | **Answered**: the period-10 rule is the count rule (K11-2). "No replacement" is a separate test: `FUN_140523110` (`player+0x0c > 1`) and `FUN_140523910` (`player+0x18 != 0`) queue a kind-2 replacement and return silently when `FUN_140521930` finds none | none |
| K11-5 | the writers of `player+0x0c == 4` and of `player+0x18` (injury marker) | **OPEN**: meanings partly decoded (§2.12); writers not located | trace the player-state writers: the injury machine (`FUN_1404812b0`, `FUN_1404819a0`) for `+0x18`; the card path for `+0x0c` |
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
124/124 checks passed
```

Mutations (each one overlays a mutated byte range **in memory only** — the
image on disk is shared with the Ghidra bridge and with other agents, so it is
never written — re-runs the whole suite and clears the overlay):

```
$ python3 check_missing.py --mutate K9.thr.in_box_xor
122/124 checks passed
MUTATION K9.thr.in_box_xor: 2 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate E15.sev.thr_A_70
119/124 checks passed
MUTATION E15.sev.thr_A_70: 5 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.consume_reason10
123/124 checks passed
MUTATION K11.consume_reason10: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate E15.jp.thr_C_5pct
122/124 checks passed
MUTATION E15.jp.thr_C_5pct: 2 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K9.judge.dj_to_yellow
123/124 checks passed
MUTATION K9.judge.dj_to_yellow: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K7.reason_foul_commit
123/124 checks passed
MUTATION K7.reason_foul_commit: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K9.paction.victim_fell
123/124 checks passed
MUTATION K9.paction.victim_fell: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.abandon_builder_call
123/124 checks passed
MUTATION K11.abandon_builder_call: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.ff_score_write
123/124 checks passed
MUTATION K11.ff_score_write: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.ff_state4_test
120/124 checks passed
MUTATION K11.ff_state4_test: 4 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.ff_period_gate
123/124 checks passed
MUTATION K11.ff_period_gate: 1 check(s) fired (expect >= 1)

$ python3 check_missing.py --mutate K11.ff_norepl_test
120/124 checks passed
MUTATION K11.ff_norepl_test: 4 check(s) fired (expect >= 1)
```

**Checker summary line: `162/162 checks passed`** (the 124 checks of the first pass plus 38 added on 21-10-26 for the period table and the K11 minimum-players chain). Each of the 38 new names has a `check_missing.py --mutate <name>` run; each fired 1–4 checks (`P.switch_call_1404493d0` 3, `P.switch_call_1404499d0` 4, `P.switch_setter` 1, `P.switch_from5_six` 4, `P.switch_from1_two` 2, `P.pk_period9` 2, `P.pk_period9_call` 1, `P.fulltime43_caller` 1, `P.fulltime43_uievent` 3, `P.fulltime43_p10` 2, `P.fulltime43_p7` 1, `P.judge_gate_call` 1, `P.whistle_period_read` 1, `P.whistle_restart7_lea` 2, `P.whistle_call` 1, `K11.mp_*` 1–4 each, `K11.sm_*` 1–4 each, `K11.redcard_reads_0c` 1, `K11.injury_reads_18` 1, `K11.ff2_flag_store` 2, `K11.ff2_score_store` 1, `K11.brain_flag_*` 1 each). The image file was not written: the checker only reads `pes_full.bin`, and each mutation overlays 32 NOP bytes in memory.

The checker is **capstone-only** — it does not need the Ghidra bridge or the
GUI's program to be open, so it keeps working while the Ghidra server is
being restarted.

The K11 rows re-verify the **whole** chain: the dispatcher that selects the
forfeit writer by record kind (`0x140a22294`), the writer itself
(`0x140a23260`: the period-10 gate, the on-pitch test, the "no replacement"
and "state 4" tests, and the two stores that set the forfeit flag and the 3),
and the reader that turns the pair into a 3-0 loss (`0x140a105b5`,
`0x140a105be`).

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
* K11 live scripts (all **read-only** — `pread` of `/proc/<pid>/mem` only,
  no writes, no pins):
  * `mi_read.py <pid> [n] [interval]` — one-shot dump of the referee fields
    (`MI+0x1168/0x116c/0x1198/0x1554/0x1574/0x1588/0x158c`,
    `MI+0x1774..0x1777`, the on-pitch flags `MI+0x1748[0..21]` and the
    per-side counts);
  * `mi_watch2.py <pid> [duration_s] [interval_s] [logfile]` — a
    change-logging watcher on those fields, with a 30 s heartbeat;
  * `forfeit_cond.py <pid> [duration_s] [interval_s] [logfile]` — the
    **forfeit-condition** watcher: it re-derives every pointer from `G` on
    every sample (no cached pointers) and logs the period, the on-pitch
    counts, every player's `+0x0c` / `+0x18`, and the **FORFEIT** set
    (the on-pitch players that satisfy `+0x0c == 4 && +0x18 == 0`);
  * `forfeit_probe.py <pid>` / `forfeit_read.py <pid>` — one-shot probes.
  * The `G` pointer is `*(0x1436F3DC8)`, the match record is
    `MI = *(G+0x270)`, the period id is the **dword** at `MI+0x1168` (not a
    pointer), the team records the forfeit loop walks are
    `*(G + (0xa3+side)*0x28)` (`FUN_140a489d0`), the slot→player-id table is
    `team + 4 + slot*4` (`FUN_140a3ae30`) and the player object is
    `team + 0xa8 + slot*0x128` (`FUN_140a3af30`); the per-player match
    record is that object **+ 4** (`FUN_140a73e20`), so the two fields the
    rule reads are `player+0x0c` and `player+0x18`.
  * `MI` is **null in ~4-9 % of polls** (the record is torn down and rebuilt);
    every script retries instead of treating that as an error.
  * Launched as
    `nohup nice -n 19 sudo -n python3 <script> <pid> 3600 0.2 <log>`.
* Period and minimum-players pass (MissingRE2, 21-10-26), all scratch, all read-only: `stores.py <disp> [lo] [hi]` (store-only displacement scan; wraps `dispfind.find`), `callargs.py <target> <needle> [lo] [hi]` (every `E8` call to `target` whose 48-byte argument setup contains `needle`), `minplayers_read.py <pid>` (live `E+0x17`, on-pitch counts, period, restart, stop reason, mode), `score_read.py <pid>` (live forfeit flag and score per side, and `player+0x0c/+0x18/+0x20` of on-pitch or flagged players). Live reads need `sudo -n` (`/proc/<pid>/mem`); the team base is `G+(0xa3+side)*0x28` (`FUN_140a489d0`), and a torn-down match shows as a null `MI` for a few polls. The Ghidra bridge endpoint for cross-references is `/xrefs_to?address=…` (`/get_xrefs_to` returns 404).
* K11 static scripts (scratch): `scan1775.py` (every instruction with a
  `+0x1775` memory operand — 107, all reads but two), `scan_prot_mode.py`
  (the same for the protected region: zero), `scan_mode16.py` (16-bit stores
  to `JR+0x220`), `scan_jrtail.py` (`JR+0x223/0x224/0x226`),
  `scan_count.py` (the `setne`+`add` counting idiom over `MI+0x1748` — 5
  candidates, all player-pickers, none a count), `scan1748fast.py` and
  `rawscan.py`.
