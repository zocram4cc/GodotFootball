# PES directive word + counter-target arm (raw-disassembly verdicts)

Scope: (1) writer of team directive word `T+0x80A0` (u16) and per-player copies `T+0x80E0[k]`; (2) counter-target arming feeding the protected-forward fallback arm at `0x1406830B0` in `FUN_140682770`. Image `pes_full.bin`, base `0x140000000`. Tags: CONFIRMED = raw disassembly or image bytes this pass; INF = inference; OPEN = named blocker. Decompiler cited only for orientation, never as evidence.

## 1. Directive-word writer: `FUN_1405DB000` (+ value fn `FUN_1405E4460`)

### 1.1 Writer VAs (raw asm)
Byte-pattern scan (66 89 /r mod=10, SIB-aware) over `.text` for disp `0x80A0`/`0x80E0` (T-relative), `0x7458`/`0x7498` (S-relative), `0xBA8` re-pointing. Deduped 66-prefixed rows (bare-89 rows at +1 are misaligned decoys of the same bytes):

| VA | bytes | owner | effect | tag |
|---|---|---|---|---|
| `0x1405DB0DC` | `66 89 b3 a0 80 00 00` | `FUN_1405DB000` | `[RBX+0x80A0] = SI (0)` early exit A | CONFIRMED (dis `dis_5DB000.txt:54`) |
| `0x1405DB177` | `66 89 b3 a0 80 00 00` | `FUN_1405DB000` | `[RBX+0x80A0] = SI (0)` early exit B | CONFIRMED (dis `:92`) |
| `0x1405DB323` | `66 44 89 bb a0 80 00 00` | `FUN_1405DB000` | `[RBX+0x80A0] = R15W` (computed phase) | CONFIRMED (dis `:200`) |
| `0x1405DB34C` | `66 89 b3 a0 80 00 00` | `FUN_1405DB000` | `[RBX+0x80A0] = SI (0)` value-fn returned 0 | CONFIRMED (dis `:208`) |
| `0x1405DB7B5` | `66 89 b3 a0 80 00 00` | `FUN_1405DB000` | `[RBX+0x80A0] = SI (0)` tail fallback | CONFIRMED (dis `:488`) |
| `0x1405E4B38` | `66 44 89 af a0 80 00 00` | `FUN_1405E4460` | `[RDI+0x80A0] = R13W` loop-clear inside value fn | CONFIRMED (dis `dis_5E4460.txt:401`) |
| `0x1405E165F` | `66 89 9a a0 80 00 00` | `FUN_1405E15C0` | `[RDX+0x80A0] = 0` full reset (param_3 != 0) | CONFIRMED (dec + dis) |
| `0x140A413F0` | `66 44 89 91 a0 80 00 00` | `FUN_140A411C0` | `[RCX+0x80A0] = 0` init (with `0x80D0 = 0`) | CONFIRMED (dis `dis_A413F0.txt:98`) |
| `0x14215714A` | `66 89 8e a0 80 00 00` | `FUN_142156E80` | `[param_1+0x80A0] = record word` snapshot load | CONFIRMED (dec `dec_215714a.txt:124`) |
| `0x1405D2668` etc (4 sites) | `66 41/42/44 89 ... e0 80 00 00` | `FUN_1405D1E60/2C00/3550/DA3A0` | `[base+0x80E0+k*2] = 4/3/1/5` per-style copy sets | CONFIRMED (raw bytes + dec) |
| `0x1405DAB14` | `66 89 84 7e e0 80 00 00` | `FUN_1405DA3A0` | second `0x80E0` store in same fn | CONFIRMED |
| `0x1405E1196` | `66 41 89 9c 50 e0 80 00 00` | `FUN_1405E1110` | `[R8+RDX*2+0x80E0] = BX (0)` per-slot reset | CONFIRMED (dis `:29`) |
| `0x1405D4F71` | `66 44 89 bc 75 e0 80 00 00` | `FUN_1405D4B00` | `[base+0x80E0+k*2] = R15W (sVar8+8)` copy follow | CONFIRMED (dis `:227`, dec `:181`) |

S-relative: `0x7458`/`0x7498` have ZERO true stores — the only raw hits (`0x14180B084`, `0x1408F6340`) are `E8 rel32` call-offset bytes, not stores. `0x745C` likewise one call-offset decoy. So no writer reaches the word/copies through the S base; all use the T base directly.
P-relative (`[ctx+0xBA8]` re-pointing): 12 true `MOV [reg+0xBA8],reg` stores; only `0x14051C84B` (`4C 89 89 a8 0b 00 00`, in `FUN_14051C840` called once at tick `0x1404F6D02` with `LEA R9,[R8+0xC48]`) is the tick init aliasing P to `T+0xC48`. The other 11 are ctor/snapshot/menu-context setups (`0x1404DDBF5`, `0x1408ADF36`, `0x140FF1344/9B/94`, `0x1411817D5/82D50/83E6A/855BC/8F934`, `0x1421403E0`), none on the live tick path. So during the team tick P == S; no separate C-like array exists for the directive word.

### 1.2 Base map of `FUN_1405DB000` (raw entry + cross-reads, CONFIRMED)
Entry raw (`dis_5DB000.txt:11-22`): `RSI=RDX(p2)`, `RDI=RCX(p1)`, `RBX=R9(p4)`, `R15=R8(p3)`; `R13 = FUN_140A48A30(G, [p3+0x110])` = side T (`0x1405DB024-046`).
- `RBX` is the side T: the same slot index family written as `[RBX+slot*4+0x7ED4]` is read as `[R13+slot*4+0x7ED4]` equivalent (`0x8B1C-0xC48 = 0x7ED4`; `R13` side-T read at `0x1405DB299` `MOV ECX,[R13+R9*4+0x8B1C]`). All five `0x80A0` stores land on `RBX` = T.
- `R15` is the `local_458` array: `[R15+0x110]` = side idx, `[R15+0x28]` = MI base (`+[0x116C]` in-play test `0x1405DB4EF`, `+[0x1198]` frame `0x1405DB335`), `[R15+0xF8]` = ctx (a63950 arg), `[R15+0x118]` = local-struct ptr (EC600, see §2), `[R15+0x128]` = role-class array (EC600 `RBP+0x2A0`, see §2).
- `RDI` = per-side struct (`+0x39C` from `5DB800.p1`); `RSI` = match struct (`[RSI+0x40C]` counter vs `0x15` gates at `0x1405DB0F3/18E`).

### 1.3 Values written and from what (raw)
- `0` (four sites): entry rejects — `FUN_1405E19F0` false (`0x1405DB04E-0DC`); `[match+0x40C] > 0x15` (`0x1405DB0F3-177`); value-fn returned 0 (`0x1405DB31D-34C`); tail no-advance fallback (`0x1405DB79A-7B5`).
- Computed phase (`0x1405DB323`): `R15W = EAX = FUN_1405E4460(...)` return (`0x1405DB2FF-315`), then `[RBX+0x80D0] = [MI+0x1198]` frame stamp (`0x1405DB32B-335`).
- `FUN_1405E4460` value logic (raw `dis_5E4460.txt`): proposals are small-int bit patterns looped as `local_124` count then first-accepted return. Entry (`0x1405E44BF-4FC`): `local_124 = 6`; if `T+0xB8A4 != 0` then `12`; if `MI+0x116C != 1` then `1`. So **mentality `T+0xB8A4` sets the proposal budget (6 vs 12; 1 when stopped)** — CONFIRMED raw. Per-iteration gates: old word `[RDI+0x80A0]` must equal proposal (`0x1405E4604-606`); timing gate `frame - [RDI+0x80D0]` vs `RNG(0x141E532D0)+0.5/1.0` scaled (`0x1405E462B-648`, `0x1405E467A-6B2`, `0x1405E46E6-72A`, `0x1405E4732-76F`); instruction id 6 test `FUN_140A63950(ctx,side,6,0xFF)` at `0x1405E45EE` (result gates the `0xD/8` cases); role-class B reads `[RDI+slot*4+0x3FB8]` filter proposals `0xB/0xC` (need class 2) and `0xF` (need class 8/5) at `0x1405E47ED-86D`. Special returns observed raw: `0xE` (`0x1405E4A22`), `0x11` (`0x1405E4A31`), `5` (`0x1405E4A40`), proposal value, `0.0` (`0x1405E4F20`). Proposal ids seen as float-bit cases: {1,2,5,6,8,10(0xA),13(0xD),14(0xE),15(0xF),16(0x10),17(0x11)} — INF mapping float-bits to phase ids (bit-exact, `struct.pack` checked), semantic labels OPEN.
- Ball zone / possession: enter via the `local_458` float window (`FUN_140687FB0`/`FUN_1406855F0` fill at `682770:62/83`, same family) and the `[RBP-0x48]` extent compares in `5E4460` (`0x1405E46CC-6F7`); possession enters through the latch-gated caller chain (§1.4), not a direct `T+0x255` read in either function (zero `0x254/0x255` hits in both raw disassemblies — CONFIRMED absence).
- No `T+0x134`/`T+0x210`/`T+0x8E08` reads in `5E4460` raw (grep zero hits — CONFIRMED absence). Strategy/plan do not enter the value directly.

### 1.4 Cadence / call chain to the tick (raw E8 scans + raw call sites)
`FUN_1405DB000` has exactly 1 caller: `0x1405DC1A6` in `FUN_1405DB800` (`MOV`-free `CALL`, E8 scan CONFIRMED). Chain upward (each link E8-scan CONFIRMED, call-site VA raw):
- `FUN_1405DB800` callers (2): `0x1405F4CE8` (in `FUN_1405F4AF0`) and `0x14067B8EE` (in `FUN_14067B640`).
- `FUN_1405F4AF0` caller (1): `0x1405ECEC7` in `FUN_1405EC600`.
- `FUN_14067B640` caller (1): `0x1405ECC2A` in `FUN_1405EC600` (else-branch; `0x1405ECC28 JZ / 0x1405ECC2A CALL / ... / 0x1405ECC44 CALL 0x140681FF0` raw).
- `FUN_1405EC600` callers (2): `0x14051DE03` (head, arg 0) and `0x14051E264` (tail, arg 1) in `FUN_14051D9E0` (role-writer §3.1 CONFIRMED dec).
- `FUN_14051D9E0` runs in the team tick `FUN_1404F6C30` on the restart path (`0x1404F6EED`, step 26a) and via `FUN_1404F9DC0` on parity frames (step 37).
Full chain: team tick `FUN_1404F6C30` (steps 26a/37) -> `FUN_14051D9E0` (role rebuild head/tail, gated `MI+0x1775==0`) -> `FUN_1405EC600` (row dispatch: row 0 -> `FUN_140681FF0` -> `FUN_140682770` protection pick; either row -> `FUN_1405DB800` via `FUN_1405F4AF0` always / `FUN_14067B640` on else-row) -> `FUN_1405DB000` (team word) + `FUN_1405D4B00` (copies, called at `0x1405DC1CB` right after, same `5DB800` frame). Per-tick cadence is gated by the `51D9E0` `0x1775` gate and the `5DB800`-internal `param_2+0x40C <= 0x15` + `param_1+0x84 < 0x16` + `B8A4==0`-reject gates — every-tick when in normal play (INF cadence; frame-counter gate `param_1+0x164` 3-frame throttle in sibling `FUN_1405DABE0` is a different dispatcher, not this path).

### 1.5 How the copies follow (raw)
- `FUN_1405D4B00` per-slot (`dis` + dec `:181`): on the won slot, `[T+0x80E0+k*2] = sVar8+8` (proposal + 8), `[T+0x80A4+k*4] = [MI+0x1198]` frame stamp, plus `+[0x6EE]`/`+[0x6E3]` flags; on ineligible slots the copy is left stale unless `FUN_1405E1110` runs.
- `FUN_1405E1110` (7 callers incl. `0x1405D25A1/33D6/C4C75/E1476`): per-slot invalidation — if copy was 4/5 clear `+[0x7FD7]`, then copy = 0, `[0x80A4] = 0` (raw `:22-30`).
- `FUN_1405E15C0`: full reset incl. word = 0 when param_3 != 0 (dec `:38`).
- `FUN_140A411C0` init: copies = 0 (loop `LEA R9,[RCX+0x80E0]` + `MOV [R9-2],CX`, raw `:17/79`), word = 0 (`:98`).
- `FUN_142156E80` snapshot loader: word and all 11 copies restored from records (`dec :124, :441`).
- Per-style writers set individual copies without touching the word: `5D1E60 -> 4`, `5D2C00 -> 3`, `5D3550 -> 1`, `5DA3A0 -> 5` (raw disp bytes CONFIRMED; guard semantics INF — each sits in a different support/run scorer arm).

### 1.6 Port statement (implements docs/re/pes-re-team-layer.md §1)
Insert between step 27b (`FUN_14051E350` designation publisher) and step 28 (`T+0x80` restart word): new step 27c `FUN_1405DB800` block — (i) `T+0x80A0 = propose(T+0xB8A4, old word, stamp, instruction-6, class-B filter, zone floats)` with `T+0x80D0 = frame` on accept else `T+0x80A0 = 0`; (ii) per-slot `T+0x80E0[k] = phase+8` + stamp for the won slot, `= 0` on invalidation (`FUN_1405E1110` sites) — implementer ports the five `5DB000` store sites verbatim (0 + computed + 0), the `5E4460` budget ladder (1/6/12 from `116C`/`B8A4`), and the `5D4B00` copy-follow; phase-id semantics stay numeric (labels OPEN).

## 2. Counter-target arm feeding `0x1406830B0`

### 2.1 What the arm tests (raw `dis_682770.txt:490-521`)
Pick loop `0x140682FC0-0A8` per slot (slot index EBX): on-pitch byte (`0x140682FDF`), line id `ESI = [RDI+RAX]` nonzero (`0x140682FFA`), class jump table at `0x140683D60` arms only classes 6/7/8 (AMF/SS/LWF/RWF per role-writer class map; classes 0–5 fall to `0x140683099` skip — CONFIRMED), then:
- `0x140683032 CALL FUN_140A63950(ctx, side, 0xF, slot)` — instruction id 15 = READY_COUNTER = Counter Target (tactics §6.2 enum CONFIRMED by tester bound + per-slot test census). `0x14068303F JNZ 0x1406830B0`: **Counter-Target-armed slot -> fallback arm takes it**.
- `0x140683041 MOV RAX,[R12+0x118]`; `0x140683049 CMP byte [RAX+0x129],1`; `0x140683050 JBE 0x1406830B0`: **advance byte <= 1 -> fallback arm takes it**.
- Else (both pass): `0x140683052 MOVSS XMM6,[R15]` line value, `+30.0` if ESI==9 (`0x14068305C`), `[RAX+0x128]==2` gate (`0x140683064-6B`: skip advance bonus unless ==2), `0x140683080 CALL FUN_1406405C0` predicate, `−10.0` (`0x140683089`), then dead `COMISS/CMOVNC` pick (team-layer §2 V5 CONFIRMED).
Fallback `0x1406830B0: MOV R13D,EBX` — takes the current slot unconditionally (last-wins shape CONFIRMED).
No card/sheet/role read in `0x140682FC0-0B3` except the class jump (role class via line-id table, not the player card): calls are only `a63950/a6405c0/a63950` (raw CONFIRMED). **The player card (COM style) is NOT a source at this site.**

### 2.2 The `[R12+0x118]` object and its bytes (raw)
- `R12` = `682770.param_1` = `681FF0.param_2` = EC600 `local_458` (raw call args `0x140682195` + EC600 `0x1405ECC1D`; bfa0 layout CONFIRMED dec).
- `[R12+0x118]` (= local_458 slot 0x23) written once at `0x1405ECB06: MOV [RBP+0x98],R9` with `R9 = LEA [RBP+0xD0]` (raw `:263-265`; `RBP-0x80+0x118 = RBP+0x98` arithmetic). So the test object is EC600's `RBP+0xD0` struct (the `local_308` shared with `5FBCB0/5F9CD0/5FBE10/681FF0` — CONFIRMED).
- `[R12+0x128]` (= slot 0x25) written at `0x1405ECA0A: MOV [RBP+0xA8],RAX`, `RAX = LEA [RBP+0x2A0]` (raw `:209-212`) — the 11-dword role-class array copied from `[R8+0x3FB8]` at `0x1405EC9E7` (`MOV EDX,[R8+0x3FB8]` loop). So `5F9CD0`'s `MOV RAX,[R14+0x128]` dereferences the role-class array (CONFIRMED).
- Writer of bytes `+0x128/+0x129` of the `RBP+0xD0` struct: OPEN — narrowed to the `5FBCB0/5F9CD0/5FBE10`-via-`5FA990/5F4F50/5EFBD0` family (all direct scans of `5F95A0/5FA7C0/5FBCB0/5F9CD0/5FBE10/5FC860/5FC500/681FF0` raw show no `+0x128/+0x129` store; EC600 body has none either). The `+0x129` advance byte is therefore neither the Counter-Target instruction (tested separately via a63950) nor the card.

### 2.3 Who arms Counter Target (instruction id 15)
Static writer of `MA+0x25CF8/0x25D38` (the a63950 backing): not found in image (tactics §6.1, still OPEN static). Live: only the two attacking records armed; defensive records gated off; GEGEN_PRESS-style episodes switch on/off in play (tactics §7.1 method). For the port: the arm reads the LIVE instruction state via `a63950(ctx,side,15,slot)` — implementer wires the existing instruction-active input, no new writer needed. Card-vs-instruction verdict: **instruction is the source (one OR leg); the card is not read at the arm**.

### 2.4 Port statement
In the `FUN_140682770` protection pick (team-layer step 44-adjacent support/forward logic, called via `FUN_140681FF0` from EC600 row-0 path): per forward-class (6/7/8) on-pitch slot, `if (instructionActive(side, slot, READY_COUNTER) || advanceByte[slot] <= 1) protected = slot;` iterated low-to-high so the last match wins; else compute the (dead) line value. `advanceByte` defaults to >1 until its writer is pinned (OPEN item 4 below; `// ponytail: +0x129 writer (EC600 RBP+0xD0 struct family)`).

## 3. Corrections to existing docs (doc:line, old, new, evidence)
| doc:line | old | new | evidence |
|---|---|---|---|
| positioning-phases:10 | writer OPEN (role part pinned) | team-word writer `FUN_1405DB000` + value `FUN_1405E4460` + copy-follow `FUN_1405D4B00`/`5E1110`/`5E15C0`/`A411C0`/snapshot `142156E80` (§1 table); S-relative stores do not exist | raw disp scan + dis this report |
| team-layer §5 item 5 | no store in either tick decompile | correct naming: the tick body has no T-relative store, but the writer runs BELOW the tick via `51D9E0 -> EC600 -> 5F4AF0/67B640 -> 5DB800 -> 5DB000` (not inline in either tick decompile) | E8 chain + call-site VAs §1.4 |
| team-layer §2 V5 + §5 item 4 | counter-target linkage OPEN | linkage CLOSED as instruction id-15 OR-leg at `0x140683038` (`a63950(...,0xF,...)` -> `JNZ 0x1406830B0`); card not a source (no card read in loop); `+0x129` writer still OPEN (narrowed) | raw `0x140682FC0-0B3` this report |
| tactics §6.1 | static writer of `MA+0x25CF8/0x25D38` not found (image-wide claim) | unchanged (still OPEN) — but the id-15 CONSUMER at the protection arm is now pinned (`0x140683032-03F`) | raw this report |
| role-writer §4 | P re-pointing after init OPEN (computed-base stores not excluded) | narrowed: 12 true `[+0xBA8]` stores enumerated; only `0x14051C84B` on live-tick path; no tick-path re-pointing found (computed-base still not excluded) | byte-pattern scan §1.1 |

## 4. Open items
1. `+0x129/+0x128` byte writers in the EC600 `RBP+0xD0` (`local_308`) struct family (`5FBCB0/5F9CD0/5FBE10` via `5FA990/5F4F50/5EFBD0`). Settles the advance-field semantics. Try: live watch on the struct during a match, or decode the three leaf callees.
2. `FUN_1405E4460` proposal-id semantics (numeric ids {1,2,5,6,8,10,13,14,15,16,17} bit-CONFIRMED; football meaning OPEN). Settles via the `PTR_FUN_143449e10/b0/e40` virtual tables (23/12/1-entry dispatch by budget).
3. `5DB800` gate inputs (`param_1+0x84 < 0x16`, `param_2+0x40C <= 0x15`, `B8A4==0` reject) event semantics. Settles via live `T+0x80A0` sampling across stoppages.
4. `FUN_140A6405C0` predicate (second arm condition at `0x140683080`) body. Needed for exact fallback-vs-bonus split.
5. Live confirmation of word/copy values across a phase change (read-only live reads `T+0x80A0`, `T+0x80E0[k]`).
