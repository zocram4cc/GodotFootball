# PES in-play plan selector (FUN_140500B90) and the start-plan / restart-kind / tick-order decodes

> **Main audit 23-10-26 (bytes):** `0x140500FA9 cmp eax, 2; je …; 0x140500FB2 mov edx, 2` (plan 2); the team tick calls the decider `0x1404F6DB8 call 0x14051E910`, the drift `0x1404F6DD3 call 0x1404F8350`, the selector `0x1404F6EA8 call 0x140500B90`, in that order: match.

Image `pes_full.bin` (file offset = VA − 0x140000000). Labels: **CONFIRMED** = read from bytes or a bridge decompile/disassembly and byte-checked; **INF** = inference; **OPEN** = not decoded, blocker named. Checker: `gf-PlanSelectorRE-scratch/check_plan_selector.py` (26 byte anchors; RESULT OK; one mutation `CMP EAX,2 -> 3` detected).

Bridge note: `/xrefs_to?address=` works; `/get_xrefs_to` and `/decompile_function?address=` 404 on this bridge. All decompiles below via `/decompile_function_by_address?address=`, all asm via `/disassemble_function?address=`.

## 0. Byte-anchor table (script output, pasted)

| site | VA | bytes |
|---|---|---|
| `sel_cmp_state1` | `0x140500F12` | `83 be 6c 11 00 00 01` = `CMP [RSI+0x116C],1` |
| `sel_jz_state1arm` | `0x140500F19` | `0f 84 9d 00 00 00` = `JZ 0x140500FBC` (in-play arm) |
| `sel_call_restartteam` | `0x140500F22` | `e8 99 f7 55 00` = `CALL FUN_140A606C0` |
| `sel_cmp_state2` | `0x140500F2F` | `83 be 6c 11 00 00 02` = `CMP [RSI+0x116C],2` |
| `sel_jnz_notstate2` | `0x140500F36` | `0f 85 ed 01 00 00` = `JNZ return` |
| `sel_state1_119c` | `0x140500FBC` | `83 be 9c 11 00 00 01` = `CMP [RSI+0x119C],1` |
| `sel_cmp_plan2` | `0x140500FA9` | `83 f8 02` = `CMP EAX,2` |
| `sel_jz_keep` | `0x140500FAC` | `0f 84 77 01 00 00` = `JZ return` (keep default) |
| `sel_mov_edx2` | `0x140500FB2` | `ba 02 00 00 00` = `MOV EDX,2` (plan 2) |
| `sel_bcc_load` | `0x140501076` | `8b 83 cc 0b 00 00` = `MOV EAX,[RBX+0xBCC]` |
| `sel_bcc_neg` | `0x14050107C` | `85 c0 78 7a` = `TEST EAX,EAX; JS +0x7A` (skip when < 0) |
| `sel_plan2_b8a3` | `0x1405010DF` | `48 8b 83 50 6d 00 00 33 ff 40 38 b8 a3 b8 00 00` = `MOV RAX,[RBX+0x6D50]; XOR EDI,EDI; CMP [RAX+0xB8A3],DIL` |
| `sel_write` | `0x140501121` | `e8 fa 4c 3c 00` = `CALL FUN_1408C5E20` (tail write) |
| `reader_head` | `0x1408E16D0` | `4c 8b c1 83 fa 02 7c 03 33 c0` = side>2 refuses to 0 |
| `reset_side0/1` | `0x1408ADEA5/0x1408ADFAD` | `89 bb 34 0b 00 00` / `89 bb dc 0b 00 00` = `MOV [RBX+0xB34]/[RBX+0xBDC],EDI`, EDI=0 (`33 ff` at `0x1408AD0CB`) |
| `tick_decider` | `0x1404F6DB8` | `CALL FUN_14051E910` |
| `tick_wordcopy` | `0x1404F6DBD` | `MOV RCX,[RDI+0x6D50]; MOV EAX,[RDI+0xBCC]; MOV [RCX+0x134],EAX` (17 B) |
| `tick_drift` | `0x1404F6DD3` | `CALL FUN_1404F8350` |
| `tick_selector` | `0x1404F6EA8` | `CALL FUN_140500B90` |
| `rows_neg/cmps` | `0x1404F5F28/38/42` | `41 f6 d9` (`NEG R9B`); `41 83 b8 6c 11 00 00 01` (`CMP [R8+0x116C],1`); `41 83 b8 54 15 00 00 01` (`CMP [R8+0x1554],1`) + `0f 44 ee` (`CMOVZ EBP,ESI`) |
| `gate_period_anchor` | `0x14050C3E0` | `40 53 48 83 ec 20` |

Neighbour-operand rule (§constants rule): every `CMP [R+disp],imm` above pairs the disp (`0x116C`, `0x119C`, `0x1554`, `0xBCC`, `0xB8A3`, `0xB34`) with the instruction's own address; checked in the disassembly, not the decompile.

## 1. Stoppage pre-branch (CONFIRMED)

Function `FUN_140500B90` (VA `0x140500B90`, 329-line decompile, 352-line disassembly). After the sheet-copy prologue (l.42–218) and the minute/second latch (l.227–231), the state split:

- `MI+0x116C == 1` (l.232, `CMP [RSI+0x116C],1` at `0x140500F12`, `JZ 0x140500FBC`): the in-play arm (§2).
- Else the stoppage arm (l.291–320):
  1. `FUN_140A606C0(uVar2)` must be non-zero (l.292, `CALL` at `0x140500F22`, `TEST AL,AL; JZ return` at `0x140500F27-29`). `uVar2 = *(DAT_1436F3DC8+0x1A8)`, the same object the reader `FUN_140A3E8A0` resolves as `G+0x750D0` (decompile: `return param_1 + 0x750d0`). `FUN_140A606C0` itself is one byte: `return *(G+0x750D0+0x8D)` (decompile, 7 lines). So the gate is **that object's byte `+0x8D`**. Its meaning is OPEN (37 callers, sheet/AI/match uses; e.g. `FUN_1408C2760` l.301/324 gates play-value writes on it) — GF has no carrier for it.
  2. `MI+0x116C == 2` required (l.296, `CMP …,2` at `0x140500F2F`, `JNZ return` at `0x140500F36`).
  3. `*(G+0x1F8)+0x10 != 0` (l.299), then `FUN_140A3FE50(*(G+0x1F8)+0x24)` non-zero (l.302).
  4. No in-play override: `FUN_140A55CF0(G+0x180, side)` must be 0 (l.306). (`FUN_140A55CF0` = match-phase gate: 0 unless the phase index reads 10.)
- Then the plan-2 rule (§2, second half): `iVar7 = FUN_1421B7D40(sheet)` (the sheet's current plan word, `*param_1` — a trivial getter, 7 lines); if `setup+0x1FC == 0` return (keep); if `iVar7 == 2` return (already 2); else write plan 2 (`MOV EDX,2; JMP write` at `0x140500FB2-B7`).

GF carry: `src/sim` has no reader of any `G+0x750D0+0x8D`-like byte, no `G+0x1F8` chain, and no `setup+0x1FC` field (grep `0xBCC|0xbcc|0xB8A3|116c|1554` in `src/sim`: only `MI+0x116C/0x1554` restart codes in `pesclock/pespossession/referee`, and `T+0xB8A3/B8A4` in `pesteamtick/pesdefshape/pesoffball/pespress`). **Needed: one byte input (name TBD after `+0x8D` decode) + the two gate calls, or a named stand-in with a `// ponytail:` citing this section.** The `MI+0x116C == 2` test maps to GF's stopped-but-not-ready state (`MatchState1554`/`PlayStateFor` give 2 vs 3; `pesmovestep.cpp:320-323`).

## 2. In-play arm and the `+0xBCC` mapping (CONFIRMED mechanics, OPEN input)

- State-1 fast paths (l.232–270): `MI+0x119C != 1` + event `0x37` at `FUN_140A3E5D0(uVar1,6)` → return (l.233–236); the `FUN_140A55CF0` / `FUN_1408C5010` / 15-tick `+0x6D06..0x6D0C` latch (l.237–270, `CMP [RSI+0x119C],1` at `0x140500FBC`, `CMP EDI,0xF; JC` at `0x140500F27-30`, `CMP [RAX],0x37` at `0x140501048`) — the GamePlan press latch, not decoded further here.
- Mode rule (l.272–289): `uVar9 = default plan = FUN_1421B7D40(sheet)`; `iVar7 = *(param_1+0xBCC)` (at `0x140501076`):
  - `iVar7 < 0` → keep default; `iVar7 < 2` → 1 (`MOV EDI,1` at `0x1405010F5`); `iVar7 == 2` → `T+0xB8A3 != 0 ? 1 : 0` (`CMP [RAX+0xB8A3],DIL; SETNZ` at `0x1405010E8-EF`); `iVar7 ∈ {3,4}` → 0 (`XOR EDI,EDI` at `0x140501092`); else keep default (`JS/JA` at `0x14050107E/90`).
  - If it equals the current plan (`CMP EDI,EAX` at `0x14050110D`, `JZ return`), return; else `FUN_1408C5E20(sheet, uVar9)` writes it (tail at `0x14050111A-121`) and returns 1. L.253–257 is the same write with `0/ default` when the override says absent (`TEST EAX,EAX; JZ return; XOR EDX,EDX` at `0x140500FA9-DB`... `0x1405010D7-DB`).
- `FUN_1408C5010` (l.254) is the sheet-valid gate for the absent path; not decoded here.

GF carry: `+0xBCC` has **no GF field** (no `bcc/BCC` hit in `src/sim` outside this report's citations). The `== 2` arm reads `T+0xB8A3` = GF `TeamTick.b8a3` (`pesteamtick.hpp:44`, `WriteAttackFlags`: `b8a3 = b8a4 < 1`). The other arms need the mode word. **Needed: one int input `modeBCC` (−1..4+, −1/absent keep) with the table above; GF default −1 (keep) with a `// ponytail:` citing `0x140501076`.** The plan index then flows to `TeamSheet::SetPlan` (`pesteamconcepts.cpp:477`, `FUN_1408C5E20`) — GF already has the writer; `WriteSlotPoints` (`teamAIcontroller.cpp:3160`) still passes the setup constant 0 (see §3).

## 3. Start-plan writer (CONFIRMED: no non-zero writer; exhibition starts at 0)

- Reader `FUN_1408E16D0` (7 lines): `return side>2 ? 0 : setup+0xB34+side*0xA8`.
- Whole-image scan: the only SIB stores with disp `0xB34` in code are the reader family's loads (`8B` at `0x1408E1500/56E/5BE/6DE`) — no indexed non-zero writer exists. All `C7/89 … 0xB34` ModRM forms: zero hits. Raw `34 0B 00 00` hits resolve at instruction boundaries to stack/immediate false positives, except three same-struct (`param_1+0xB34`, word/byte, unrelated objects at `0x1411D317C/6FE7/97B9`) and the reset below.
- The only writer is the reset in `FUN_1408AD0B0` (called by `FUN_1408ACC00`): `MOV [RBX+0xB34],EDI` at `0x1408ADEA5` and `MOV [RBX+0xBDC],EDI` at `0x1408ADFAD` with `XOR EDI,EDI` at `0x1408AD0CB` — both sides to **0**.
- The `0x144089CD0` bytes (`44 89 84 0A 34 0B 00 00 C3` after `CMP EDX,2; IMUL RDX,RAX,0xA8`) sit in `.impdata` (VA `0x144089CC0`, file off `0x4089CC0`), not in any code section — a data blob, not a linked writer. No `E8`-relative caller targets it.
- Setup builder `FUN_141FC1590` only **reads** (`FUN_1408E16D0` at its l.101/145) and publishes via `FUN_1408C5E20(sheet, plan)`; it never writes `setup+0xB34`.

So a normal exhibition starts with plan **0 on both sides**, and nothing in play writes a non-zero start plan before the selector does. **Port spec: `kSetupPlan = 0` stays (already in `WriteSlotPoints`, `teamAIcontroller.cpp:3162`); close the `// ponytail:` by citing this section (`0x1408ADEA5/0x1408ADFAD`, `0x1408E16D0`) instead of "writer OPEN".** The 68 `FUN_1408E16D0` callers are the tactic/slider/role readers, not writers (xrefs list in scratch).

## 4. Period-end restart kind (CONFIRMED chain, OPEN byte)

- PES chain: judge gate `FUN_14050C3E0` (19 lines): skip periods in mask `0x644` (`10 < u || !(0x644 >> (u&0x1F) & 1)`), and when `(time-up FUN_14050B9B0 || judge+0xDC)` call `thunk_EXT_FUN_143F559F0(param_1)` — the protected thunk that raises restart 7 (reason 1; A5Judge §1.3/§4.8: `JR+0x00 = 0x1554` via `FUN_14102CAD0`, a trivial `*param_1 = param_2` setter; the `7` constant lives behind the protected thunk `0x143F5530B`, not in decodable code). Then builder `FUN_1404505D0` (CONFIRMED head: `FUN_140451FD0(param_1,8)` … emits `[3, 7, 0x45]` per A5Judge §4.8).
- What follows: the period-end programme runs the break/full-time steps (`0x45, 0x25, 0x24 …`); the next period's kick-off is a fresh restart 1 written by the normal award path (`FUN_1404A56C0` writes 2,3,4,5,6 per spot at `0x1404A59F3/5A45/5A9F/5B50`, and the `EDI+2` write at `0x1404A5BD2` with `EDI=1` gives 3) — i.e. PES does **not** re-read the 7; the 7 is consumed by the period dispatcher and the next kick-off starts at 1.
- GF: `Match::PesRestartKind` (`match.cpp:497-499`) maps the referee buffer (`RestartKindOf`: 1 kick-off … 6 penalty, 0 none; `pesteamtick.cpp:119-134`) and has no 7. At period end GF sets `buffer.desiredSetPiece = e_SetPiece_KickOff` (`referee.cpp:281-284` + `StopPlay` block) — so GF reads **1** where PES's JR holds **7** during the break.

**Port spec: add kind 7 (`kRestartPeriodEnd = 7`) to `RestartKindOf`/the `MI+0x1554` carrier, written when the period ends (GF's `clock.EndPeriod` site, `referee.cpp:301`) and held until the next award overwrites it with its own kind; no consumer yet — the rows rule `FUN_1404F5F10` (`f = 0` when `MI+0x116C != 1 && MI+0x1554 == 1`) only special-cases 1, and the selector's `MI+0x116C == 2` arm does not test the kind, so 7 changes no current branch.punt: name the constant after A5Judge §1.3 (`JR+0x00`), cite `FUN_14050C3E0` + `0x143F559F0`.** Status: chain CONFIRMED to the thunk; the `7` write itself OPEN behind protected code (blocker: `0x143F5530B`).

## 5. Decider-vs-drift order (CONFIRMED at the bytes)

`FUN_1404F6C30` disassembly:
- `0x1404F6DB8: CALL FUN_14051E910` (decider)
- `0x1404F6DBD-DCA: MOV RCX,[RDI+0x6D50]; MOV EAX,[RDI+0xBCC]; MOV [RCX+0x134],EAX` (attack-mode word copy to `T+0x134`)
- `0x1404F6DD3: CALL FUN_1404F8350` (drift)
- `0x1404F6E1C-58: B8A2 = (L>=4), B8A3 = (L<=0)` (inline flags)
- `0x1404F6EA8: CALL FUN_140500B90` (selector, l.237)

So PES runs **decider → mode-word copy → drift → flags → … → selector**. GF's `UpdateStrategy` (`teamAIcontroller.cpp:2826-2868`) runs drift + flags + restart word and never calls the decider-then-copy before them; the strategy moves on dead balls underneath via a different path. **Port spec: in GF's team tick, move `DriftAttackLevel`/`WriteAttackFlags` after the strategy-word source and insert the `+0x134 = +0xBCC` copy between them (GF needs a `restartWord134` field on `TeamTick` — new, cite `0x1404F6DBD`); the drift then reads the previous tick's word exactly as PES does. The drift mesh itself (`0x1404F8640-6C8`, `CMP ESI,EBP` at `0x1404F8663`, freeze `CMP EBX,EBP` at `0x1404F86C8`, clamp `CMP EBX,R14D` at `0x1404F86DE`, stamp `CMP EBX,EBP` at `0x1404F86ED`) is already ported (`DriftAttackLevel`/`ManualOverrideLevel`); only the dead-ball crossing arm past `0x1404F8675` (helpers `FUN_140A606C0`, `FUN_140A3FE50`, `MI+0x116C == 2`) stays OPEN — same helpers as §1.** Status CONFIRMED.

## 6. Port checklist (GF files)

1. `pesteamtick.hpp/cpp`: add `modeBCC` (−1 default) + `word134`; set 134 from BCC between decider-source and drift (§5).
2. `teamAIcontroller.cpp` (`UpdateStrategy`, `WriteSlotPoints`): thread the mode table (§2) and `SetPlan` from the selector instead of `kSetupPlan` (§3, close ponytail with §3 citation); order decider-copy → drift → flags (§5).
3. `pesteamtick.cpp` (`RestartKindOf`), `match.cpp` (`PesRestartKind`), `referee.cpp` (period end): add kind 7 (§4).
4. `pesmovestep.cpp:423` (`MatchState1554() == 0` stand-in), `team.cpp:463-466` (2-or-3 split): unchanged by this report, but the selector's `== 2` test (§1) needs the 2-vs-3 split to fire — note as dependency.
5. Tests: rows-gate (`FUN_1404F5F10`: `NEG/SBB/ADD` = `2-(F!=0)`, `CMOVZ` override), mode table (5 arms + keep), plan-2 (already-2 keeps, `1FC==0` keeps, else 2), tick order (copy before drift before selector), period kind held-then-overwritten.

## 7. Status

- §1 stoppage pre-branch: rule + constants CONFIRMED; the `+0x8D` byte's meaning OPEN (blocker: decode the `G+0x1F8`/`G+0x750D0` objects or name the byte from another caller).
- §2 plan-2 + BCC table: CONFIRMED; the `+0xBCC` writer OPEN (blocker: writer scan — no indexed/imm writer in image; likely set from UI/management layer outside the sim tick; GF default −1 stands).
- §3 start plan: CONFIRMED 0 (no non-zero writer; reset cites above).
- §4 period end: chain CONFIRMED to the protected thunk; the `7` store OPEN behind `0x143F5530B`; "what follows" CONFIRMED (dispatcher consumes; next kick-off is a fresh 1). GF delta: writes 1 where PES holds 7.
- §5 order: CONFIRMED (decider → copy → drift → flags → selector); GF inverted (drift before/under decider) — port item.
