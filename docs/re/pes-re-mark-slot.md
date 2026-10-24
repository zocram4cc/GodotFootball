# PES mark slot (B7D8) + action word (pl+0x5D0): RE report (MarkSlotRE, 24-10-26)

> **Main audit 23-10-26 (bytes):** `0x140547811 mov [rsi+0xB7D8], r14d`, `0x14054743B mov [rsi+0xB7D8], 0xFF`, `0x140549E32 mov [rdi+0xB7D8], ecx`: match.

Title: which player each B7D8 writer stores and when (CONFIRMED), what `FUN_140A6CA10`
writes and when (CONFIRMED), the {1, 0x3F} value provenance (INF, one scan left),
plus `FUN_1404F0E50` inner scoring weights (INF detail, CONFIRMED skeleton/contract).

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`
(`decompile_function?address=`, `disassemble_function?address=`, `xrefs_to?address=`),
one request at a time.
Scratch: `<workdrive>/gf-MarkSlotRE-scratch/`
(`546FB0.dec`, `549D30.dec`, `6CA10.dec`, `dec_*`/`dis_*` fetches,
`fetch.py` sequential fetcher, `check_markslot.py` self-checking checker).
Prior: `pes-re-mark-table.md` (pair writer, T7a rule, 7.0 correction),
`pes-re-mark-cone.md` (B7D8 word CONFIRMED, cone, clear gate),
`pes-re-mark-gate.md` (count gate with 12.0 correction, head/action meanings),
`pes-re-mark-readers.md` (R1/R2/R3 payloads, ramp base CLOSED as FieldInfo F5),
`pes-re-team-helpers.md` (T6b/T9/T10 payloads, `FUN_1404F0E50` spine + contract §6).

## 0. Constant table (script-read from the image, never typed)

```
0x14259BDF0 f32=7.0 bits=0x40E00000   (T7a ramp base; NOT 8.0)
0x14258C264 f32=3.0 bits=0x40400000   (T7a ramp top add)
0x14259BE0C f32=15.0 bits=0x41700000  (T7a cone offset)
0x1425C48E4 f32=1369.0 bits=0x44AB2000 (37^2 hard-exit ceiling)
0x1425C48D0 f32=105.0 bits=0x42D20000 (wrap test 1 cap)
0x142596E6C f32=80.0 bits=0x42A00000  (wrap tests 2+3)
0x14259BE5C f32=90.0 bits=0x42B40000  (cone deg)
0x14259BE04 f32=12.0 bits=0x41400000  (cone outer len / count-gate radius)
0x14259BE44 f32=64.0 bits=0x42800000  (count-gate folded v; 64/2 = 32 band)
0x142565600 f32=0.5 bits=0x3F000000   (cone half-scale)
0x142565604 f32=2.0 bits=0x40000000   (count-gate width)
0x1425795E4 f32=6.2831854820251465 bits=0x40C90FDB (2pi)
0x14259BE40 f32=57.2957763671875 bits=0x42652EE0 (180/pi)
0x142565D30 bits=0xFFFFFFFF (abs-mask low word, NOT a value)
0x14259BDD0 f32=0.6944444179534912 bits=0x3F31C71C (0.6944 rate)
0x14259858C f32=360.0 bits=0x43B40000 (fold)
0x142598580 f32=180.0 bits=0x43340000 (wrap base)
0x14259DA9C f32=1.7000000476837158 bits=0x3FD9999A (4F0E50 gate)
0x14259BDDC f32=1.5 bits=0x3FC00000 (margin family)
0x1425A2670 f32=400.0 bits=0x43C80000 (d2 gate 400 = 20^2)
0x14259D5D8 f32=1.399999976158142 bits=0x3FB33333 (pick-blend base)
0x1425CB000 f32=1.6399999856948853 bits=0x3FD1EB85 (pick scale)
0x14259D60C f32=9.0 bits=0x41100000  (4F0E50 0x1404F3A3F COMISS XMM10)
0x1425A9464 f32=36.0 bits=0x42100000 (4F0E50 0x1404F3A4F MOVSS XMM0)
0x1425C4890 f32=0.0 bits=0x00000000  (4F0E50 0x1404F3ADB COMISS)
0x142588200 f32=0.10000000149011612 bits=0x3DCCCCCD (4F0E50 0x1404F3A9D MULSS)
0x14259D5C8 f32=0.3499999940395355 bits=0x3EB33333 (4F0E50 x0.35 arm)
0x14257A194 f32=0.699999988079071 bits=0x3F333333 (4F0E50 x0.7 arm)
```

Checker asserts every row above. 44 checks green; mutations 7/7 killed
(`ramp8/cap4/mod10/ceil1368/exitcont/gate65/half06` each fail exactly one check,
reverted).

## 1. `T+0xB7D8` (markSlot): which player's slot each writer stores, and when — CONFIRMED

Meaning (inherited, re-verified): per-team within-side slot word, range 0..10,
`0xFF` = empty. NOT the designation id. Both readers build
`tgt = side*11 + B7D8`; `FUN_140A06B50` returns 0 immediately if `B7D8 == 0xFF`.

Record model used below (from the decompiles, §§1.1–1.2): `param_1` is a per-man
concept record with `+0x18` = phase, `+0x1C` = own global slot candidate A,
`+0x20` = own global slot candidate B, `+0x24` = peer global slot candidate;
`param_2` = the MA/team context (`*param_2` = TA, `param_2[1]` = side);
`param_3` = the team block T. Slot words are global 0..21; the `%11` folds
(`MUL 0xBA2E8BA3; SHR 3; IMUL 11; SUB`) are within-side folds. All five store
sites opcode-verified from the image (checker §5 table):

| VA | bytes | site |
|---|---|---|
| `0x14054743B` | `C7 86 D8 B7 00 00 FF 00 00 00` | 546FB0 match arm `= 0xFF` |
| `0x140547811` | `44 89 B6 D8 B7 00 00` | 546FB0 live arm `= uVar10` |
| `0x1405497FB` | `C7 86 D8 B7 00 00 FF 00 00 00` (no REX.B) | 549D30 match arm `= 0xFF` |
| `0x140549E0B` | `C7 87 D8 B7 00 00 FF 00 00 00` | 549D30 clear arm `= 0xFF` |
| `0x140549E32` | `89 8F D8 B7 00 00` | 549D30 live arm `= uVar2` |

### 1.1 `FUN_140546FB0` (full decompile CONFIRMED, `546FB0.dec`)

Phase dispatch on `[param_1+0x18]` (`0x14054707C TEST; 0x140547084 SUB; 0x140547087/090`):
`0 → match arm`, `1 → mid arm`, `2 → late arm`, else return (B7D8 untouched).

- Match arm (`[param_1+0x18]==0`, `0x1405473CC` region): `iVar4 = FUN_1405969B0(TA, side)`
  (`0x1405473D1 CALL`); if `[param_1+0x1C] == iVar4` then `[param_1+0x18] = 1` AND
  `T+B7D8 = 0xFF` (`0x14054743B`). So: **when the record's own-slot A matches the
  resolved man for this side, the phase advances 0→1 and the mark slot is
  cleared** (fresh engagement starts with no marked man).
- Live arm (`[param_1+0x18]==0` but A mismatched, falls to `0x140547452` region):
  after the `44CC0`-refresh gate, the `4C9A0`-`429A0/42A40` blocks and the
  `627E0/5969B0` holder/duel wiring, `T+B7D8 = uVar10` (`0x140547811`) where
  `uVar10 = [param_1+0x1C] % 11` (`< 22`, else 0; head `0x140546FDA–140547003`).
  So: **while engaged, B7D8 continuously tracks the record's own-slot A folded
  to the side** — i.e. the within-side slot of the man this concept record is
  driving. It is the man the defender is assigned to handle on this concept,
  expressed as his own row index, not the opponent he marks.
- Mid arm (`[param_1+0x18]==1`): match sub-arm (`[param_1+0x20] == 5969B0(...)`)
  sets `[param_1+0x18] = 2` (no B7D8 store on that path); live sub-arm refreshes
  via `545560` and rewrites the `42940/42A40` blocks. Tail falls through to the
  common epilogue: `T+B7A0[uVar10] = uVar9` (`0x14054781B`), where
  `uVar9 = [param_1+0x20] % 11` — the sibling within-side slot word B7A0 tracks
  the peer slot B while B7D8 keeps slot A.
- Late arm (`[param_1+0x18]==2`, `0x140547096` region): stoppage-gated
  (`A48A30` + `T+0xA2A0[uVar9]==0x1E`) take/commit blocks; on the `5461A0` gate
  the duel wiring runs; tail writes `T+B7A0[uVar10] = 0xFF` (`0x14054718F`) —
  the sibling word is cleared while B7D8 keeps the live-arm value.

Condition summary for 546FB0: B7D8 = 0xFF exactly on the 0→1 phase advance
(A matches the resolved side man); otherwise B7D8 = own-slot-A % 11 every pass
through the live arm. No attributes, no RNG, no ball on this path.

The 5969B0 resolver (CONFIRMED, `dec_0x1405969B0.txt`): on a non-play restart
(`MI+0x1554==5`, `MI+0x1558==side`) it returns the first of the two
`MI+0x173C` slots whose role class passes `6E320` (keeper-class veto); else the
holder (`627E0`, else `MA+0x2483C`) when he belongs to this side; else the
`T+0x78` fallback (`A48A30`, `< 0x16`). So the "match" that clears B7D8 is:
**own-slot A names the same man the resolver picks for this side** (stoppage
designee, holder, or `T+0x78` fallback, in that order).

### 1.2 `FUN_140549D30` (full decompile CONFIRMED, `549D30.dec` + full disassembly)

Single caller found (`xrefs_to`: `0x1405494CA [UNCONDITIONAL_CALL]`; bridge has
no function body at the call site — inside unanalysed bytes, OPEN who calls).
Structure mirrors 546FB0 with different record offsets:

- Match arm (`[param_1+0x20] == FUN_1405969B0(...)`, `0x140549DA1/DA6`):
  `[param_1+0x18] = 2`; duel wiring on the `uVar6` (holder-fold) arm;
  `T+B7D8 = 0xFF` (`0x1405497FB` shape, `0x140549E0B` second `= 0xFF`), then
  immediately `T+B7D8 = uVar2` (`0x140549E32`) where
  `uVar2 = [param_1+0x24] % 11` (`0x140549E15–E32`). Net on the match arm:
  **clear-then-set: B7D8 ends as the peer-slot (`+0x24`) fold** — the OTHER
  record slot, not own-slot A. (The `546FB0` match arm only clears; the set
  comes on its live arm. Here both happen on the match arm.)
- Live arm (`[param_1+0x20]` mismatched, `0x140549E71` region): the
  `A48A30`/`A48A10` aim-project blocks (`8F7C0` project with `7.0@[0x14259BDF0]`,
  `DB20` window, `42A40` commit), `T+0xB748/T+0xB774` verdict words, duel
  wiring on the holder-fold arm. No B7D8 store on this path — B7D8 keeps the
  match-arm peer value.
- Tail (both arms): `T+B7F8[uVar5] = 4` (`0x140549FFE`) where
  `uVar5 = [param_1+0x24] % 11` (0 when `≥ 0x16`) — the B7F8 row follows the
  same peer slot the match arm stored to B7D8.

Condition summary for 549D30: B7D8 = peer-slot (`+0x24`) % 11, written once on
the match arm (clear-then-set); the live arm leaves it. So the two writers
cover the two record slots: **546FB0 publishes own-slot A (continuous), 549D30
publishes the peer slot (one-shot on match)**.

### 1.3 Port spec (markSlot)

GF has no `T+0xB7D8` carrier (grep for `B7D8/markTable` in `src/`+`tests/`
empty on the branch; the live stand-in is
`TeamAIController::TickMarkTable`'s designated possession player, marked
`// ponytail:` naming `FUN_140546FB0/549D30`).

- New per-team state: `markSlot` word, 0..10 + `0xFF` empty (1:1 with B7D8).
- Writers (port when the concept-record layer lands; both writers live inside
  it — neither maps to a GF concept record today):
  - own-slot path (546FB0 §1.1): on the 0→1 phase advance
    (`ownA == 5969B0(TA, side)`) write `0xFF`; on every live-arm pass write
    `ownA % 11`. Resolver 5969B0: stoppage designee → holder (same side) →
    `T+0x78` fallback.
  - peer-slot path (549D30 §1.2): on the match arm
    (`ownB == 5969B0(TA, side)`) write `0xFF` then `peer % 11`; live arm
    leaves it. Sibling `T+B7A0[ownA % 11] = peer % 11` / `= 0xFF` tails
    (546FB0 epilogue, 549D30 match/live tails) when the record layer lands.
  - teardown: bulk `= 0xFF` + `markTable = 0` on the `FUN_14057A1F0/3E0`
    path (gate `FUN_140579E10`, per mark-cone doc §4).
- Until the record layer lands, the stand-in stays: owning man's within-side
  slot. It is the 546FB0 live-arm value (own-slot A fold), NOT the 549D30
  peer-slot value and NOT the marked opponent. Direction of the error: with
  the stand-in, T7a's `tgt` skip excludes the engaging man himself (correct
  only when own-slot A is the engaging man); the peer-slot writer has no
  stand-in. Keep the existing `// ponytail:` (it already names both writers).
- `FUN_1405969B0` resolver ports with the writers (stoppage-designee /
  holder / `T+0x78` order above); `T+0x78` writer is lane-leaves §4
  (CONFIRMED, independently ported).

Status: writers CONFIRMED (bodies + all five store opcodes + phase gates +
resolver). Meaning CONFIRMED (own row slot, not the opponent).

## 2. Action word `pl+0x5D0` writer `FUN_140A6CA10`: values and when — CONFIRMED

Writer body (CONFIRMED, `6CA10.dec` verbatim):

```c
void FUN_140a6ca10(longlong rec, int newVal) {
  int old = *(rec + 0x5D0);
  *(rec + 0x5D4) = old;     // previous value preserved
  *(rec + 0x5D0) = newVal;
  if (old == newVal) *(rec + 0x6B8)++;
  else *(rec + 0x6B8) = 0;  // repeat counter
}
```

Init `FUN_140A6C550` zeroes `+0x5D0/+0x5D4/+0x6B8` (same fn — record identity
fixed, per mark-gate doc §3). Exactly 2 CALL sites (bridge `xrefs_to`
CONFIRMED — no third site):

- `0x14092D607` in `FUN_14092D580`: `6CA10(R8rec+0xC8, [R8rec+0xB5AC])`
  (disassembly CONFIRMED: `0x14092D5F3 MOV EDX,[RSI+0xB5AC]`,
  `0x14092D5F9 LEA RCX,[RSI+0xC8]`, `0x14092D607 CALL 0x140A6CA10`).
  The target `R8rec+0xC8` is the player record at outer-record `+0xC8`
  (`A48910`-side record reach, decompile line 63 context). So **every
  `92D580` pass copies the outer record's action word `+0xB5AC` into the
  player record's action word `+0x5D0`** (with `+0x5D4` = previous, `+0x6B8`
  = repeat count). The value written is whatever `+0xB5AC` currently holds —
  the live {1, 0x3F} path is this site with `B5AC ∈ {1, 0x3F}`.
- `0x140A36D6F` in `FUN_140A36B30`:
  `6CA10(param_1+0x19, MI+0x116C==2 ? 0x72 : 1)` gated on
  `param_1[0x578+val] != 0` (decompile lines ~124–132). Match-phase script
  values (1 in play, 0x72 = 114 in phase 2). This site never writes 0x3F on
  the observed paths.

Value set (CONFIRMED reader + writer shape; provenance INF): T7a admits
`+0x5D0 ∈ {1, 0x3F}` (`0x1404F6484 CMP 1 / 0x1404F648F CMP 0x3F`). The {1, 0x3F}
provenance is the `+0xB5AC` feed at site (a): direct `disp32 0xB5AC` scan finds
no writer (split displacement, like the mark pair rows) but 9 byte-hits name
the reader/producer neighbourhood (`0x1409293F1/2BC54/2BE54/2C36A/2D5F5/2D6D6/
2E63A/2FFD2` + data-region `0x15AC797E8`): the `B5AC` source chain is
`A85270`-init → `A85820`-fill → 0x10-dword `MOVUPS` copy into `+0xB5AC`
(`0x1409293EE–41A`, CONFIRMED bytes) with guards comparing `+0xB5AC` against
`3` (`0x14092BC54`), `{0x0A, 0x49, 0x5A}` (`0x14092BE54`), `4` (`0x14092C36A,
0x14092FFC2`) and `0x36` (`0x14092D6D4`, `0x14092BC00` writer arm sets
`+0xB60C = 0x1E` when `+0xB5AC == 3`). Full value vocabulary still OPEN —
one scan left (the split-displacement writer hunt). What is CLOSED: **the
writer copies; the vocabulary lives upstream in `+0xB5AC`**.

GF mapping: no 1:1 carrier (`TeamPressState.role[]` is press slots, not player
state; `pescarrier.hpp:1014 action` / `PlayerFacts.passGet` cover only
fragments). Needs new per-player `actionWord` (fed from the carrier/command
layer; `+0x5D4` previous + `+0x6B8` repeat counter ride with it for free).
Until then the stand-in stays: `passGet ? 1 : 0x3F`. Direction of the error:
T7a admits both, so the stand-in over-admits everyone who is neither the
receiver (true 1s are narrower) nor a 0x3F man. Keep the existing
`// ponytail:` (it already names `0x140A6CA10` + both call sites).

Status: writer + both call sites + init + value copy CONFIRMED. {1, 0x3F}
provenance INF (upstream `+0xB5AC` vocabulary; split-displacement hunt left).

## 3. `FUN_1404F0E50` inner scoring weights: skeleton + contract (INF detail)

Budget note: full dataflow trace of the 3424-line body exceeds this pass
(the brief allows it only "if budget allows"). What this pass adds to the
team-helpers doc §6 spine: callee census + scoring-region float-op map +
script-read constants, enough to name exactly what a port needs.

- Callee census (from `dis_0x1404F0E50.txt`, 3424 lines): `A3E190` ×47
  (record ring), `141E532D0` ×33 (global float cell), `1415A1780` ×33 (sqrt
  dispatch), `A57800` ×14, `141E532E0` ×13 (global counter), `A48A10` ×11,
  `A489F0` ×10, `43A0A0` ×9 (wrap diff), `A48910` ×8, `439E50` ×8 (bearing),
  `A6CF10/A92D40/A6E5E0/A6CE10/A62EA0` ×6–7, `A71540/A6E340/A6CF70` ×5,
  `A6E150` ×4 (need chain with `A6E580/A6E4B0/A6E050/A6CF50/A6CF30`),
  `A627E0` ×3 (holder), `A48A30` ×3, `481B00` ×3, `A76240` ×2,
  `A41C00`-family claim gates, `4F6110/4F4AB0/4F50F0/4C8E00` payload calls.
- Scoring region (around the `4F6110`/`4F4AB0` calls, `0x1404F3941–3B71`):
  `MULSS 0.1@[0x142588200]` rate arm, `9.0@[0x14259D60C]` COMISS,
  `36.0@[0x1425A9464]` gate, `0.0@[0x1425C4890]` COMISS, `3.0@[0x14258C264]`
  ceiling arm, `25.0@[0x14259BE20]` COMISS, `×0.35@[0x14259D5C8]` /
  `×0.7@[0x14257A194]` need-chain scales (all operand VAs pair-checked,
  constants script-read §0).
- Contract (unchanged from team-helpers §6): `FUN_1404F0E50(Y, i) → slot id`
  (the final computer; `T+0x78 :=` it or 0xFF). Inputs are Named 5/6/7/8/10
  (already listed there) — no new state beyond those. **The inner scoring
  weights stay INF detail; the word + writer path are ported independently.**
  Exact port BLOCKED on a register trace (~30 requests, one more pass).

GF mapping: none 1:1 for the weights; needs Named 5 (`T+0x78` id word, writer
lane-leaves §4 independently ported), Named 6 (record tables), Named 7
(geometry), Named 8 (MI words), Named 10 (class words).

Status: spine + callee census + scoring-region constants CONFIRMED; weights INF.

## 4. Port spec for MarkTablePort (delta vs the branch tip `6263981`)

The branch already wires: pair table (P1), T7a build (P2 with F5 ramp, cone,
count gate, side/action/head inputs), R1 exemption, R2 OR-leg, R3 skip,
plus the two `// ponytail:` stand-ins named below. What this report changes:

- **P0 — `markSlot` word**: keep the state; extend the `// ponytail:` at
  `TeamAIController::TickMarkTable` to cite the exact arms:
  `FUN_140546FB0` live arm (`0x140547811 = ownA % 11`, match arm
  `0x14054743B = 0xFF`) and `FUN_140549D30` match arm
  (`0x140549E0B = 0xFF` then `0x140549E32 = peer % 11`, live arm leaves it).
  Resolver `FUN_1405969B0` order: stoppage designee → same-side holder →
  `T+0x78` fallback. No code change (record layer absent); doc-only delta.
- **P2 — `actionWord`**: keep the state and the `passGet ? 1 : 0x3F`
  stand-in; extend its `// ponytail:` to cite the copy semantics
  (`FUN_140A6CA10`: `+0x5D4 = old`, `+0x5D0 = new`, `+0x6B8` repeat count),
  site (a) `0x14092D607` (`+0x5D0 := +0xB5AC` every pass) and site (b)
  `0x140A36D6F` (1 / 0x72 by `MI+0x116C == 2`). No code change; doc-only.
- **Do NOT port yet**: B7A0 sibling tails, `5969B0` resolver, `+0xB5AC`
  vocabulary, `4F0E50` weights (§3).

## 5. Checker with mutations (in scratch)

`<workdrive>/gf-MarkSlotRE-scratch/check_markslot.py` — self-checking, no PES
bytes in repo: asserts the §0 script-read constants (incl. the 7.0-vs-8.0
correction), helper arithmetic (`0x228D4+0x16A8 == 0x23F7C`,
`0x228D4+7*200 == 0x22E4C`, row stride `22 f32 = 0x58`, NL span ending at
`0x23F7C`), `%11` magic vs `%` operator, all five B7D8 store-site opcodes,
T7a cap-3 / `%11`-magic / hard-exit bytes, and the port rule on synthetic
geometry (zero-init, `id%11` commit, cap-3, `d² > 1369` hard-exit, cone
ahead/behind/outer, count-gate 12.5 m boundary, half-cone 49.5° boundary).
**44 checks green; mutations 7/7 killed** (`ramp8/cap4/mod10/ceil1368/
exitcont/gate65/half06` each fail exactly one check, reverted).

## 6. Status summary

- B7D8 writers: **CONFIRMED** (which slot each stores + phase conditions +
  all five store opcodes + resolver order). Meaning CONFIRMED (own/peer row
  slot, not the marked opponent).
- Action word: **CONFIRMED** (writer body + init + both call sites + copy
  semantics). Values/when CONFIRMED (every `92D580` pass copies `+0xB5AC`;
  match-phase 1/0x72 at the other site). {1, 0x3F} provenance **INF**
  (upstream `+0xB5AC` vocabulary; split-displacement hunt left).
- `FUN_1404F0E50` weights: **INF** (skeleton + census + scoring constants
  CONFIRMED; exact port blocked on a register trace).
- GF state: no 1:1 carrier for `markSlot` (record-layer state) or
  `actionWord` (carrier-layer state); each names its `needs <X>`. The two
  stand-ins stay with extended `// ponytail:` cites (§4).

## 7. Open questions / blockers

1. `140A92D40/2DC0` bit-exactness (lane-core O3, inherited).
2. `+0xB5AC/+0xB5B0` split-displacement writers ({1, 0x3F} provenance
   completion; port takes the action word as input).
3. `FUN_140549D30` caller at `0x1405494CA` (unanalysed bytes; which tick
   drives the peer-slot path).
4. `FUN_1404F0E50` register trace (~30 requests) for the exact weights.
5. `FUN_140A62780` window fill + caller row-buffer identity (team-helpers
   open items 1–2, unchanged).
