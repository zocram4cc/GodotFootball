# PES RE: kick kinds (`ap+0x2752`) and the situation factor Q (KickKinds, 29-10-26)

Read-only RE. Image `pes_full.bin` (flat, VA - 0x140000000). Decompiles: headless `dec.sh` (`d_w3456.c`, `d_sel.c`,
`d_types.c`, `d_sem.c` in `<workdrive>/gf-KickKinds-scratch/`). Constants and jump tables: script-read from the image,
operand VAs from capstone. Replays: `kind_sweep.py` (scratch) on KickSoftMin's `frame_trace.run_recorded`.
Tags: **CONFIRMED** (decompile + asm/image read, or replay), **INF** (inference, reason given), **OPEN** (blocker named).

## Verdict

- The kind is **not a static GF-action table**. PES picks it per kick in `FUN_1408364D0` from the request
  *(action id, subtype)* by testing an ordered candidate list against geometry (`FUN_140829E80` = try kind k; it
  stores `*result = k` at its success path, decompile line `*puVar30 = uStack_404`). CONFIRMED.
- **Q depends on the kind only through three table/predicate facts: class (row byte 13), subclass (row byte 12)
  and the gate predicate set.** Replay sweep (all 150 kinds × 6 captures, 906 PES runs): Q(kind) = Base(kind) ×
  G(class, subclass, geometry). Base(kind) reproduces PES bit-exactly for all 150 kinds in 4 captures (600/600);
  in the other 2 the residual G is constant within each (class, subclass) group. CONFIRMED (replay).
- All 8 scatter captures: Q = Base(kind) × G, with G = 1.0 exactly in 6/8 (Q bit-exact from the kind alone),
  0.99625 (seq031, first-time window active) and 0.9 (seq036, header). CONFIRMED.
- Writers: correction to `pes-re-record-ids.md` §1. **W2 and W3 are resets (write -1), not kind-sets.** The kind
  is set only by **W5** (`Kick` action start) and **W6** (`Kick` action update/re-pick); **W4** writes 0x96. CONFIRMED.

## §0 Constants (script-read; operand VA paired)

```
0x14258C260 f32=0.8999999761581421 bits=3F666666  0x14083DF76 MOVSS XMM6 (class 0 base; sub 7/0xB mult; gate class 2)
0x1425DDF70 f32=0.8799999952316284 bits=3F6147AE  0x14083DFA4 MOVSS XMM13 (class 1 base)
0x1425EB850 f32=0.8600000143051147 bits=3F5C28F6  0x14083DF99 MOVSS XMM13 (class 2/3 base)
0x14256AE1C f32=1.0 bits=3F800000  0x14083CF1D MOVSS XMM12 (class 4 base: XMM13=XMM12 at 0x14083DF7E)
0x1425AB124 f32=0.949999988079071 bits=3F733333  0x14083DB62 MOVSS XMM11 (sub 1 mult 0x14083DFCE; gate class<2 0x14083E05E)
0x142572348 f32=0.800000011920929 bits=3F4CCCCD  0x14083DF54 MOVSS XMM8 (sub 5 mult 0x14083DFD5)
0x1425AB120 f32=0.8500000238418579 bits=3F59999A  0x14083DF67 MOVSS XMM9 (sub 6 mult 0x14083DFDC)
0x14257A194 f32=0.699999988079071 bits=3F333333  0x14083DFE3 MULSS XMM13 (sub 8 mult)
0x1425EF9DC f32=0.925000011920929 bits=3F6CCCCD  0x14083E04C MULSS XMM13 (gate class 3)
0x14259858C f32=360.0 bits=43B40000  0x14083E0BA MOVSS XMM4 (heading wrap before the turn delta; the window `< 360` gate reads the same constant, its instruction VA not paired here)
```
Subclass jump table `0x1408400A0` (13 dwords, `cmp ebx,0xc; ja` at 0x14083DFB3; >12 = no multiplier):
```
sub 0 -> 0x14083DFF3
sub 1 -> 0x14083DFCE
sub 2 -> 0x14083DFF3
sub 3 -> 0x14083DFF3
sub 4 -> 0x14083DFF3
sub 5 -> 0x14083DFD5
sub 6 -> 0x14083DFDC
sub 7 -> 0x14083DFEE
sub 8 -> 0x14083DFE3
sub 9 -> 0x14083DFF3
sub 10 -> 0x14083DFF3
sub 11 -> 0x14083DFEE
sub 12 -> 0x14083DFF3
```
targets: 0x14083DFCE ×0.95 (XMM11), 0x14083DFD5 ×0.8 (XMM8), 0x14083DFDC ×0.85 (XMM9), 0x14083DFE3 ×0.7, 0x14083DFEE ×0.9 (XMM6), 0x14083DFF3 none.

## §1 Writers of `ap+0x2752` (function starts from `.pdata`, chained unwind resolved)

| site | function | owner | what it writes | tag |
|---|---|---|---|---|
| W1 0x1406EB6D6 | `FUN_1406EB670` | called 0x1406E04D3 (init) | -1, window 720.0f | CONFIRMED (record-ids) |
| W2 0x1406F6B6B | `FUN_1406F6790` | called 0x1406E60F8 (per-tick player update) | **-1** (`0xffff`) when vt+0x28 action is not a kick action (`0x140A6E340` false) | CONFIRMED (d_w2f.c) |
| W3 0x140719DCF | `FUN_140719BB0` | `Kick` vtable slot 5 (`0x1425E2380`) | **-1**, window 720.0f, when Kick+0x2c == 0 (action exit) | CONFIRMED |
| W4 0x14071BB69 | `FUN_14071B350` | called 0x140719FD7 inside `FUN_140719F20` = `Kick` slot 4 | 0x96 (class 4, sub 13: Base 1.0) | CONFIRMED |
| W5 0x140724F26 | `FUN_140724CC0` | `Kick` slot 13 (`0x1425E23C0`) | kind = `FUN_1408364D0` result[0] (0x97 = none; then result[10]); +0x2754/55/56 = result[1]/[2]/[3]; +0x2758 | CONFIRMED |
| W6 0x140725FFF | `FUN_1407253D0` | `Kick` slot 15 (`0x1425E23D0`) | re-pick of the same shape (setup block `[rbp+0x110..]`) | CONFIRMED (site); re-pick conditions OPEN |

`Kick` = RTTI `.?AVKick@action@anime@match@@` (COL `0x142C9EA10`, vtable `0x1425E2358`, 28 slots). CONFIRMED.
No other of the 79 `match::anime::action` classes (CornerKick, FreeKick, GoalKick*, PenaltyKick, KickoffKicker,
Throwin*) owns a kind writer: set pieces leave -1 and Q's fallback remaps the action (0x33→0x2A, 0x3F→0x1C, 0x41→0x41,
else default; `d_83CC30.c` lines 102-123). CONFIRMED (writer scan) / INF (which action id is which set piece).

## §2 The request and the chooser

- Request built by `FUN_140833CD0(req, player, cmd)`: `req[0] = cmd[0]` = **anime action id** (W6 checks
  `vt+0x28() == order[1]` and passes it as the request type), `req[1] = FUN_1407204E0(cmd.ptr, type, cmd[7])` = subtype,
  `req[0xb] = FUN_140837FC0(player)` → result[3] → `+0x2756` (first-time byte). CONFIRMED.
- Kick action ids: only **8, 9, 10** have flag bit 16 in the action table `0x1426484F0` (getter `0x140A6E340`). CONFIRMED.
- Subtypes `FUN_1407204E0`: type 8: cmd[7]==0x15→8; byte cmd.ptr+0x149: 0→7, 3→6, else 5. type 9: +0x14A==2→4;
  +0x14C&0x10→2; cmd[7]==0x18→3; else 1. type 10: +0x14A==1→10, ==2→11, else 9. CONFIRMED (meaning of the bytes OPEN).
- **Type semantics (INF, high):** 10 = **shot** (no receiver target; `FUN_140726AD0` goal-frame adjust in the tester
  when `req[0]==10`; W5 records a per-player stat only for 10); 9 = **ground pass** (space-pass target
  `FUN_14080ECE0`: distance linear in power, tuning block 0x31); 8 = **lofted/long pass** (`FUN_14080EBA0`: power
  curve with exponent, tuning block 0x2C). Blocker for CONFIRMED: the order writer that sets cmd[0] from the pad command
  (`PAD_COMMAND_KIND_SHORT_PASS/LONG_PASS/THROUGH_PASS/SHOOT/CENTERING/CLEAR`).
- Chooser `FUN_1408364D0`: dispatch on req[0]: 8→`FUN_1408360F0`, 9→`FUN_140837670`, 10→`FUN_1408371A0`; action
  0x12→`FUN_14082DD20` (kind 118); no hit and req[1]==8 → retry with kinds 10..13 (`lea ecx,[rdx+8]`, `FUN_140829E80`
  variants 2/3). Candidate families by chooser (immediate kind args of the 294 `FUN_140829E80` call sites):
  `FUN_1408272A0` 1..27 (all class 3, the high-ball family, tried by every type), `FUN_14082DE40` 42..56,
  `FUN_1408314C0` 57..64,140,146, `FUN_140822C10` 97..104 (type 10 sub 10), `FUN_14082E370` (type 10 other subs),
  `FUN_1408262B0` 28..38,66,75,78,106..108, `FUN_1408293A0` 28,39..41,46,66,70,97,98 (type 9), `FUN_140824A30`/
  `FUN_140825610`/`FUN_140825D60` 121..134. Per-function lists in the kind table below. CONFIRMED (asm);
  geometric gates inside each chooser: OPEN (not decoded; each is a `FUN_140829E80` feasibility test).
- **Class 3 = header family** (kinds 1..27, 149): seq036/037 (kind 14) and seq113 (kind 1) have the ball at
  y = 2.54 / 2.58 m (ball ring `[G+0x40]+0x88`, y = height) and the kicker airborne (P+0xC 0.05/0.29); every
  ground kind (28, 42, 43, 57, 97, 98) has the ball at 0.11-0.12 m. CONFIRMED (captures) for the class, INF for the name.

## §3 Q's inputs (sources)

| input | read | source (writer) | tag |
|---|---|---|---|
| kind | `ap+0x2752` s16 | W5/W6 (`FUN_1408364D0` result[0]); W4 0x96; reset -1 by W1/W2/W3 | CONFIRMED |
| +0x2754 byte | `ap+0x2754` | result[1] = `FUN_140838CA0(...)` float→int in the tester (default 0xC) | CONFIRMED (site), meaning OPEN |
| **uVar14** | `ap+0x2755` | result[2] = `FUN_140723DE0(signed turn angle, kind, req[0x24])` in `FUN_140829E80` (two sites); default 0x12. Captures 9, 13, 11, 2, 4 are its outputs (a direction bucket of the kick angle); Q's motion switch (0x9BE→0xF, 0xA20/0xA7D→0xD, 0x9B0/0x9B4/0xA41/0xA42/0xA45 with kind 0x2B/0x3A→9) overrides it after entry | CONFIRMED (writer), INF (bucket meaning) |
| firstTime | `ap+0x2756` | result[3] = req[0xb] = `FUN_140837FC0(player)` | CONFIRMED |
| class / subclass | row bytes 13 / 12 of `0x14344E380 + kind·16` (getters `0x1407205A0` / `0x14071E520`; out-of-range row `0x14344E620`) | install table | CONFIRMED |
| gate predicates | `0x1407240F0` (exclude), `0x140724590`, `0x140726610`, `0x140726640` | pure functions of kind (sets in the kind table) | CONFIRMED (asm) |
| window | `ap+0x2764` | 720.0 by W1 and W3; decreasing timer otherwise (writer of the countdown OPEN) | CONFIRMED / OPEN |
| AI bit | `ap+0x2460` bit 29 | writer OPEN (0 in 14/14) | OPEN |
| motion id | `*(u16*)vt+0x18(ap+0xAB8)` | live animation | CONFIRMED (read) |
| heading / body | `ap+0x247C`, `ap+0xBE4` | kinematics | CONFIRMED (read) |
| stamina-ish | `ap+0x281C` | `FUN_1406F6790` (W2's function) accumulator | CONFIRMED (writer), meaning OPEN |

Base(kind) = classBase[class] × subMult[sub] × (gate ? gateMult[class] : 1), gate = !P240F0(kind) && AIbit==0 &&
(P24590 || P26610 || P26640)(kind) && motion != 0x9F1. classBase: 0→0.9, 1→0.88, 2,3→0.86, 4→1.0; gateMult: class<2
→0.95, 3→0.925, else 0.9. G (speed, first-time window, turn delta; decompile lines 959-1040): OPEN as a closed form;
measured per capture below.

## §4 Validation on the 14 kick captures

Columns: seq | hook | kind | b4 | sub | class | gate | +0x2755 | firstTime | window | AI | ball y (m) | Base(kind) |
PES Q (soft-min call 1, x) | G = Q/Base | Q == Base bit-exact

| seq | hook | kind | b4 | sub | class | gate | 2755 | ft | window | AI | ball y | Base | Q | G | exact |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| seq017 | kick_scatter | 42 | 9 | 3 | 1 | False | 9 | 0 | 720.0 | 0 | 0.11 | 0.88 | 0.88 | 1.0 | True |
| seq018 | kick_postsca | 42 | 9 | 3 | 1 | False | 9 | 0 | 720.0 | 0 | 0.11 | 0.88 | None | None | None |
| seq022 | kick_scatter | 97 | 7 | 3 | 2 | False | 9 | 1 | 275.95 | 0 | 0.11 | 0.86 | 0.86 | 1.0 | True |
| seq031 | kick_scatter | 57 | 12 | 3 | 0 | False | 13 | 1 | 273.69 | 0 | 0.11 | 0.9 | 0.896625 | 0.99625 | False |
| seq036 | kick_scatter | 14 | 0 | 0 | 3 | True | 9 | 0 | 720.0 | 0 | 2.54 | 0.7955 | 0.71595 | 0.9 | False |
| seq037 | kick_postsca | 14 | 0 | 0 | 3 | True | 9 | 0 | 720.0 | 0 | 2.54 | 0.7955 | None | None | None |
| seq048 | kick_scatter | 28 | 6 | 2 | 2 | False | 9 | 0 | 155.71 | 0 | 0.11 | 0.86 | 0.86 | 1.0 | True |
| seq067 | kick_scatter | 98 | 9 | 3 | 1 | False | 9 | 1 | 236.6 | 0 | 0.11 | 0.88 | 0.88 | 1.0 | True |
| seq070 | kick_postsca | 42 | 9 | 3 | 1 | False | 13 | 1 | 71.52 | 0 | 0.12 | 0.88 | None | None | None |
| seq084 | kick_postsca | 42 | 9 | 3 | 1 | False | 9 | 0 | 167.29 | 0 | 0.11 | 0.88 | None | None | None |
| seq099 | kick_postsca | 97 | 7 | 3 | 2 | False | 9 | 0 | 78.47 | 0 | 0.12 | 0.86 | None | None | None |
| seq113 | kick_postsca | 1 | 2 | 0 | 3 | False | 4 | 0 | 720.0 | 0 | 2.58 | 0.86 | None | None | None |
| seq4471 | kick_scatter | 43 | 9 | 3 | 1 | False | 2 | 0 | 720.0 | 0 | n/a | 0.88 | 0.88 | 1.0 | True |
| seq4774 | kick_scatter | 97 | 7 | 3 | 2 | False | 11 | 0 | 312.99 | 0 | n/a | 0.86 | 0.86 | 1.0 | True |

- 8/8 scatter captures: Base(kind) from the captured kind explains Q; 6/8 bit-exact, 2/8 differ by the class-wide G
  (seq031 0.99625 = first-time window 273.7 < 360 with firstTime 1; seq036 0.9 = class-3 G). The sweep shows G is
  identical for every kind of the same (class, subclass) in both. CONFIRMED (replay, `sweep_*.json`).
- 6 post-scale captures have no Q (the post-scale does not compute it); their kinds are rows of the table and match the
  paired scatter P (seq018 = seq017, seq037 = seq036). Base shown for reference.
- **seq4471 and seq4774 are no longer on disk** (`live2/pid463316` holds 14 other seqs). Their row comes from
  KickSoftMin's `kick_oracle.json` (pre-call kind and PES soft-min args); not re-replayed.

## §5 Table for GF's port (GF action → PES request → kinds)

| GF `e_FunctionType` (+context) | PES request (action id, subtype) | kinds PES can pick | tag |
|---|---|---|---|
| Shot | 10, sub 9 (sub 10/11 from order byte +0x14A = 1/2) | `FUN_1408371A0` list: 0x69, 116/117, 113, 114, 112, 97..104 (sub 10) or `FUN_14082E370` set, 121..134/137..141, 1..27 (high ball) | INF (type), CONFIRMED (lists) |
| ShortPass | 9, sub 1 (2 when order flag 0x10, 3 when cmd[7]==0x18, 4 when +0x14A==2) | `FUN_140837670` list: 0x21, 0x24, 0x26, 0x6A, 0x6B, 0x6C, 0x82, 119/120, 113..115, 112, 28/39..41/46/66/70/97/98, then the type-8 chain, 1..27 | INF / CONFIRMED |
| LongPass, HighPass | 8, sub 5/6/7 (order byte +0x149: else/3/0) | `FUN_1408360F0`: sub 4..6 → 73, 116/117, 98..103, 28..38/66/75/78/106..108, 121..133/137/139/144, 1..27; sub 7 → 0x27, 0x42/0x2E, 0x41 | INF / CONFIRMED |
| (none in GF) | 8, sub 8 (cmd[7]==0x15) | 39/46/108/121..147 then 1..27, then 0x17/0x18 | GF has no counterpart |
| Header | no request of its own: any of 8/9/10 with a high ball lands in class 3 (1..27, `FUN_1408272A0`) | 1..27 | CONFIRMED (class), INF (selection) |
| Trap, BallControl, Deflect, Catch, Interfere, Trip, Sliding, Movement, Special | not a `Kick` action: kind stays -1 (W2 resets it) | — (Q fallback remaps action 0x33/0x3F/0x41) | CONFIRMED (W2) |
| Set pieces (kick-off, free kick, corner, goal kick, penalty, throw-in) | separate action classes, no kind writer; Q uses the -1 remap | 0x2A / 0x1C / 0x41 / default `0x1425E5B60` | CONFIRMED (remap), INF (which set piece is 0x33/0x3F/0x41) |
| Clear | PES has `PAD_COMMAND_KIND_CLEAR`, GF has no function type | — | GF gap |

What the port can do without inventing: Q needs only (class, subclass, gate) of the kind. Exact kind requires porting
`FUN_1408364D0` + `FUN_140829E80` (geometry tests); a GF table would be the invented mapping the brief forbids.

## Kind table `0x14344E380` (16 B/row; b0 = id, b4, b8, b12 = subclass, b13 = class; script dump). The last column lists only immediate `mov ecx,imm` call sites; kinds passed as `lea ecx,[reg+N]` (e.g. 14 in `FUN_1408272A0`, 0x48..0x4A in `FUN_1408262B0`) are not attributed here.

| kind | b4 | b8 | sub (b12) | class (b13) | Base(kind) (AI=0, motion≠0x9F1) | gate | predicates | tried (imm) in chooser |
|---|---|---|---|---|---|---|---|---|
| 1 (0x1) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 2 (0x2) | 0 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 3 (0x3) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 4 (0x4) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 5 (0x5) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  | 241A0 |  |
| 6 (0x6) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  | 241A0 |  |
| 7 (0x7) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 |
| 8 (0x8) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 |
| 9 (0x9) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  | 241A0 | 1408272A0 |
| 10 (0xA) | 2 | 0 | 0 | 3 | 0.7955000400543213 | yes | 24590 |  |
| 11 (0xB) | 1 | 0 | 1 | 3 | 0.7557250261306763 | yes | 24590 |  |
| 12 (0xC) | 2 | 0 | 0 | 3 | 0.7955000400543213 | yes | 24590 | 1408272A0 1408364D0 |
| 13 (0xD) | 1 | 0 | 1 | 3 | 0.7557250261306763 | yes | 24590 | 1408272A0 1408364D0 |
| 14 (0xE) | 0 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26610 |  |
| 15 (0xF) | 0 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26610 |  |
| 16 (0x10) | 0 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26610 |  |
| 17 (0x11) | 1 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26640 | 1408272A0 |
| 18 (0x12) | 1 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 |
| 19 (0x13) | 1 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26640 |  |
| 20 (0x14) | 1 | 0 | 1 | 3 | 0.7557250261306763 | yes | 26640 | 1408272A0 |
| 21 (0x15) | 1 | 0 | 1 | 3 | 0.8170000314712524 |  |  |  |
| 22 (0x16) | 1 | 2 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 23 (0x17) | 3 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 24 (0x18) | 3 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 1408360F0 |
| 25 (0x19) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 |
| 26 (0x1A) | 2 | 0 | 0 | 3 | 0.7955000400543213 | yes | 26640 | 1408272A0 |
| 27 (0x1B) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  | 1408272A0 |
| 28 (0x1C) | 6 | 0 | 2 | 2 | 0.8600000143051147 |  |  | 140822C10 1408262B0 1408293A0 14082E370 |
| 29 (0x1D) | 6 | 0 | 2 | 2 | 0.8600000143051147 |  | 240F0 | 1408262B0 |
| 30 (0x1E) | 6 | 0 | 2 | 2 | 0.8600000143051147 |  | 240F0 | 1408262B0 |
| 31 (0x1F) | 5 | 0 | 2 | 2 | 0.8600000143051147 |  |  | 1408262B0 |
| 32 (0x20) | 6 | 0 | 2 | 2 | 0.8600000143051147 |  |  | 1408262B0 |
| 33 (0x21) | 8 | 0 | 12 | 1 | 0.8799999952316284 |  |  | 1408262B0 140837670 |
| 34 (0x22) | 8 | 0 | 2 | 1 | 0.8799999952316284 |  |  |  |
| 35 (0x23) | 8 | 0 | 12 | 1 | 0.8799999952316284 |  | 240F0 | 1408262B0 |
| 36 (0x24) | 8 | 1 | 2 | 1 | 0.8799999952316284 |  |  | 1408262B0 140837670 |
| 37 (0x25) | 8 | 1 | 2 | 1 | 0.8799999952316284 |  | 240F0 | 1408262B0 |
| 38 (0x26) | 8 | 1 | 2 | 1 | 0.8799999952316284 |  |  | 1408262B0 140837670 |
| 39 (0x27) | 7 | 0 | 3 | 2 | 0.8600000143051147 |  |  | 1408223F0 1408293A0 1408360F0 |
| 40 (0x28) | 7 | 0 | 3 | 2 | 0.8600000143051147 |  | 240F0 | 1408293A0 |
| 41 (0x29) | 7 | 0 | 3 | 2 | 0.8600000143051147 |  | 240F0 | 1408293A0 |
| 42 (0x2A) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 43 (0x2B) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 44 (0x2C) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 45 (0x2D) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 46 (0x2E) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 1408223F0 140822C10 1408293A0 14082DE40 14082E370 1408360F0 |
| 47 (0x2F) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 48 (0x30) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  |  |
| 49 (0x31) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  |  |
| 50 (0x32) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 51 (0x33) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 52 (0x34) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  |  |
| 53 (0x35) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  |  |
| 54 (0x36) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  | 240F0 | 14082DE40 |
| 55 (0x37) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 56 (0x38) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 14082DE40 |
| 57 (0x39) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408314C0 |
| 58 (0x3A) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408262B0 1408314C0 |
| 59 (0x3B) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408314C0 |
| 60 (0x3C) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  |  |
| 61 (0x3D) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  |  |
| 62 (0x3E) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  | 240F0 | 1408314C0 |
| 63 (0x3F) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408314C0 |
| 64 (0x40) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408314C0 |
| 65 (0x41) | 4 | 0 | 4 | 2 | 0.8600000143051147 |  |  | 14082E370 |
| 66 (0x42) | 6 | 0 | 4 | 2 | 0.8600000143051147 |  |  | 140822C10 1408262B0 1408293A0 14082E370 |
| 67 (0x43) | 9 | 0 | 6 | 2 | 0.7310000061988831 |  |  | 1408217D0 140821A30 |
| 68 (0x44) | 9 | 0 | 5 | 2 | 0.6880000233650208 |  |  | 1408217D0 140821A30 |
| 69 (0x45) | 6 | 0 | 4 | 2 | 0.8600000143051147 |  |  |  |
| 70 (0x46) | 6 | 0 | 4 | 2 | 0.8600000143051147 |  |  | 1408293A0 |
| 71 (0x47) | 4 | 0 | 4 | 2 | 0.8600000143051147 |  | 240F0 | 14082E370 |
| 72 (0x48) | 4 | 0 | 4 | 2 | 0.8600000143051147 |  | 241A0 | 14082E370 |
| 73 (0x49) | 5 | 0 | 4 | 2 | 0.8600000143051147 |  |  | 14082E370 140838AB0 |
| 74 (0x4A) | 7 | 4 | 4 | 2 | 0.8600000143051147 |  |  |  |
| 75 (0x4B) | 10 | 0 | 9 | 1 | 0.8799999952316284 |  |  | 1408262B0 14082E370 14082F000 |
| 76 (0x4C) | 10 | 0 | 9 | 1 | 0.8799999952316284 |  |  |  |
| 77 (0x4D) | 13 | 0 | 9 | 0 | 0.8999999761581421 |  |  | 14082F000 |
| 78 (0x4E) | 13 | 0 | 9 | 0 | 0.8999999761581421 |  |  | 1408262B0 14082F000 |
| 79 (0x4F) | 13 | 0 | 9 | 0 | 0.8999999761581421 |  |  |  |
| 80 (0x50) | 13 | 0 | 9 | 0 | 0.8999999761581421 |  |  |  |
| 81 (0x51) | 8 | 0 | 9 | 1 | 0.8799999952316284 |  |  | 14082E370 |
| 82 (0x52) | 13 | 0 | 10 | 0 | 0.8999999761581421 |  |  | 14082F000 |
| 83 (0x53) | 10 | 0 | 8 | 1 | 0.6159999966621399 |  |  | 140828DA0 |
| 84 (0x54) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828DA0 |
| 85 (0x55) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828DA0 |
| 86 (0x56) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828DA0 |
| 87 (0x57) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828DA0 |
| 88 (0x58) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 89 (0x59) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 90 (0x5A) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 91 (0x5B) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 92 (0x5C) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 93 (0x5D) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 94 (0x5E) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 95 (0x5F) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 96 (0x60) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  |  |
| 97 (0x61) | 7 | 0 | 3 | 2 | 0.8600000143051147 |  |  | 140822C10 1408293A0 14082E370 |
| 98 (0x62) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 1408293A0 |
| 99 (0x63) | 8 | 0 | 9 | 1 | 0.8799999952316284 |  |  | 140822C10 14082E370 |
| 100 (0x64) | 7 | 4 | 3 | 2 | 0.8600000143051147 |  |  | 140822C10 140823010 |
| 101 (0x65) | 5 | 4 | 3 | 2 | 0.8600000143051147 |  |  | 140822C10 |
| 102 (0x66) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 140822C10 |
| 103 (0x67) | 8 | 4 | 9 | 2 | 0.8600000143051147 |  |  | 140822C10 |
| 104 (0x68) | 8 | 0 | 9 | 1 | 0.8799999952316284 |  |  | 140822C10 |
| 105 (0x69) | 6 | 4 | 4 | 2 | 0.8600000143051147 |  |  | 1408371A0 |
| 106 (0x6A) | 8 | 1 | 9 | 1 | 0.8799999952316284 |  |  | 1408262B0 140837670 |
| 107 (0x6B) | 10 | 1 | 12 | 0 | 0.8999999761581421 |  |  | 1408262B0 140837670 |
| 108 (0x6C) | 10 | 1 | 9 | 1 | 0.8799999952316284 |  |  | 1408223F0 1408262B0 140837670 |
| 109 (0x6D) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  |  | 140824010 |
| 110 (0x6E) | 7 | 0 | 3 | 2 | 0.8600000143051147 |  |  | 140824550 |
| 111 (0x6F) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828DA0 |
| 112 (0x70) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 1408306F0 |
| 113 (0x71) | 14 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140828A00 |
| 114 (0x72) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 140823570 |
| 115 (0x73) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 1408238D0 |
| 116 (0x74) | 8 | 0 | 11 | 1 | 0.7919999957084656 |  |  | 14082F6A0 |
| 117 (0x75) | 9 | 0 | 11 | 1 | 0.7919999957084656 |  |  | 14082F6A0 |
| 118 (0x76) | 13 | 0 | 9 | 0 | 0.8999999761581421 |  |  | 14082DD20 |
| 119 (0x77) | 9 | 0 | 3 | 0 | 0.8999999761581421 |  |  | 140821660 |
| 120 (0x78) | 10 | 0 | 8 | 0 | 0.6299999952316284 |  |  | 140821660 |
| 121 (0x79) | 9 | 0 | 7 | 2 | 0.6965999603271484 | yes | 24590 | 1408223F0 140824A30 140825610 140825D60 |
| 122 (0x7A) | 5 | 0 | 4 | 2 | 0.7739999890327454 | yes | 24590 |  |
| 123 (0x7B) | 12 | 0 | 4 | 0 | 0.8549999594688416 | yes | 24590 | 1408223F0 140824A30 140825610 |
| 124 (0x7C) | 8 | 0 | 3 | 2 | 0.7739999890327454 | yes | 24590 | 1408223F0 140825610 140825D60 |
| 125 (0x7D) | 8 | 0 | 3 | 2 | 0.7739999890327454 | yes | 24590 | 1408223F0 140824A30 140825610 140825D60 |
| 126 (0x7E) | 8 | 0 | 12 | 1 | 0.8359999656677246 | yes | 24590 | 140824A30 1408262B0 |
| 127 (0x7F) | 13 | 0 | 7 | 1 | 0.7523999810218811 | yes | 24590 | 1408223F0 140825610 140825D60 |
| 128 (0x80) | 13 | 0 | 7 | 1 | 0.7523999810218811 | yes | 24590 |  |
| 129 (0x81) | 9 | 0 | 3 | 1 | 0.8359999656677246 | yes | 24590 | 1408223F0 140824A30 140825610 140825D60 |
| 130 (0x82) | 13 | 0 | 10 | 0 | 0.8549999594688416 | yes | 24590 | 1408223F0 140825610 140825D60 140837670 |
| 131 (0x83) | 13 | 0 | 10 | 0 | 0.8549999594688416 | yes | 24590 | 1408217D0 140821A30 140825610 |
| 132 (0x84) | 12 | 3 | 5 | 0 | 0.6839999556541443 | yes | 24590 | 1408223F0 140824A30 140825610 140825D60 |
| 133 (0x85) | 12 | 3 | 5 | 0 | 0.6839999556541443 | yes | 24590 | 1408223F0 140824A30 140825610 140825D60 |
| 134 (0x86) | 14 | 4 | 10 | 0 | 0.8549999594688416 | yes | 24590 | 1408217D0 140821A30 140825610 |
| 135 (0x87) | 3 | 0 | 8 | 0 | 0.5985000133514404 | yes | 26610 | 140830B50 |
| 136 (0x88) | 14 | 0 | 8 | 0 | 0.5985000133514404 | yes | 26610 | 140830B50 |
| 137 (0x89) | 9 | 0 | 7 | 2 | 0.6965999603271484 | yes | 26610 | 1408223F0 14082E370 14082F930 14082FD90 140830380 |
| 138 (0x8A) | 10 | 0 | 3 | 1 | 0.8799999952316284 |  | 240F0,26610 | 1408262B0 14082DE40 |
| 139 (0x8B) | 12 | 0 | 3 | 1 | 0.8359999656677246 | yes | 26610 | 14082F930 14082FD90 140830380 |
| 140 (0x8C) | 13 | 0 | 3 | 0 | 0.8999999761581421 |  | 240F0,26610 | 1408314C0 |
| 141 (0x8D) | 13 | 0 | 3 | 0 | 0.8549999594688416 | yes | 26610 | 14082FD90 140830380 |
| 142 (0x8E) | 14 | 0 | 8 | 0 | 0.5985000133514404 | yes | 26610 |  |
| 143 (0x8F) | 9 | 0 | 3 | 1 | 0.8799999952316284 |  | 240F0,26640 | 1408262B0 14082DE40 |
| 144 (0x90) | 9 | 0 | 3 | 1 | 0.8359999656677246 | yes | 26640 | 1408223F0 1408318C0 140831A80 140831CA0 |
| 145 (0x91) | 10 | 0 | 9 | 1 | 0.8359999656677246 | yes | 26640 | 1408223F0 |
| 146 (0x92) | 12 | 0 | 3 | 0 | 0.8999999761581421 |  | 240F0,26640 | 1408314C0 |
| 147 (0x93) | 12 | 0 | 3 | 0 | 0.8549999594688416 | yes | 26640 | 1408223F0 140831A80 140831CA0 |
| 148 (0x94) | 13 | 0 | 10 | 0 | 0.8999999761581421 |  |  |  |
| 149 (0x95) | 2 | 0 | 0 | 3 | 0.8600000143051147 |  |  |  |
| 150 (0x96) | 15 | 5 | 13 | 4 | 1.0 |  |  |  |

## Open items

1. Order writer: which pad command / AI decision sets cmd[0] = 8/9/10 and bytes +0x149/+0x14A/+0x14C, cmd[7] (makes §2 types CONFIRMED). Blocker: decode of the order builder (callers of `Kick` slot 13 via the action manager).
2. Geometric gates inside each chooser (which candidate wins): `FUN_140829E80` body (~1650 lines), `FUN_140723DE0` (uVar14 buckets), `FUN_140838CA0` (+0x2754).
3. G closed form (decompile lines 959-1040: speed term with `ap+0x281C`, first-time window term, turn-delta term by subclass); the replay oracle per capture is in §4.
4. AI bit writer (`ap+0x2460` bit 29) and the window countdown writer.
5. seq4471/seq4774 snapshots missing from `live2/pid463316`; a new kick-scatter capture (`pid1467004`, hook live) would restore a 2nd-session replay.
