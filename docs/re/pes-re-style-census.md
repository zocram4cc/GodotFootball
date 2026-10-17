# PES 2021 playing-style reader census (StyleCensus)

Read-only decode. Scratch: `<workdrive>/gf-StyleCensus-scratch/` (`xref_*.txt`, `dis/*.txt`, `sites.tsv`, `census.tsv`, `census_*.py`, `sweep_hits.tsv`).
Evidence tags: **CONFIRMED** = raw disassembly (Ghidra text `disassemble_function`, or raw bytes of `pes_full.bin` via capstone; file offset = VA − 0x140000000, verified on 0x14063E890) or live decompile where stated; **INF** = inferred; **OPEN** = blocked, with the block named.
Bit→name: `pes-re-playing-styles.md` §0 (engine order, unchanged here).

## 0. Method and wrapper ABI (CONFIRMED, raw)

- Call sites: Ghidra xrefs (`xrefs_to`, `limit=300`): 213 → `FUN_140A64030`, 13 → `FUN_140A63FE0`, 2 → `FUN_14063E890`, 0 → `FUN_141465180` (last bullet of this section). 18 of the 213 lie in code Ghidra does not define; they were decoded from raw bytes (call-aligned, §1.1).
- **The BIT of each caller is the register named below at the call, decoded from the raw instruction stream.** The decompile is not used for bit values.
- `FUN_140A64030(ECX=playerIdx, EDX=BIT, R8=block owner, R9=team)`: `EDI=EDX`; slot `EBX=team+0x4C00[ECX%11]` (or `team+0x4C00` when ECX≥11); `RCX=player block`, `R8D=EBX`, `EDX=EDI`, `JMP 0x1408CB0B0`. **BIT is passed through unchanged.**
- `FUN_140A63FE0(ECX, EDX=BIT, R8, R9)`: identical tail (`MOV EDI,EDX` … `MOV EDX,EDI; JMP 0x1408CB0B0`). BIT = EDX.
- `FUN_14063E890(RCX=team, EDX=playerIdx (≤0x15), R8D=BIT)`: `ESI=R8D` is the BIT; `EBX=EDX` is the player index; `CALL 0x1408CB0B0(RCX=block+8, EDX=ESI=BIT, R8D=team+0x4C00[idx%11])`. **The doc's "(block, bit, slot)" label for this function is wrong: arg2 is the player index and arg3 is the bit.**
- Gate `FUN_1408CB0B0(block, BIT=EDX, slot=R8D)`: `((block+0x48) >> BIT & 1) AND (0x142613D20[slot] >> BIT & 1)`. `SHL EDX,CL` has no range check.
- `FUN_141465180`: the BIT is `int(float [obj+8])` (`CVTTSS2SI EDI,[RBX+8]`), the player index is `[obj+0x2C]` (≤0x15), and the gate is reached by `JMP 0x1408CB0B0`. Its only references are one 8-byte pointer in `.rdata` at `0x142953168`, inside a 7-record `(fn,1,ptr)` table at `0x1429530D8`. A RIP-relative search of the whole image (disp32 landing on `0x1429530D8`, `0x142953168`) finds no code reference. Direct and indirect callers: **OPEN** (dispatcher not located).

## 1. Census by bit (CONFIRMED, 228 call sites accounted)

Totals: 212 `FUN_140A64030` + 1 shared tail + 13 `FUN_140A63FE0` + 2 `FUN_14063E890` = **228 sites**. 0 unresolved. The single shared tail `1405C0F9F` is reached with BIT 0, 1, 9, 10, 15 or 16, chosen by the switch in `FUN_1405C0F00` (§1.2).

| bit | engine name (UI name) | A64030 | A63FE0 | E890 | total | sites (host: count) |
|---|---|---|---|---|---|---|
| 0 | GOAL_POACHER (Goal Poacher) | 28 | 3 | – | 31 | 14053b900, 14053bb90, 14053bdc0, 14053bf10, 140563040, 140595ab0, 1405a5b20, 1405c0f00, 1405d8b70, 1405dc350×3, 1405df1f0, 1405e6e80×6, 140601d80, 140604490, 140607bc0×2, 1406087a0, 140662a10, 140667480, 1406768e0, 1409502f0, 1409512e0, 140952a30, 14096dfc0 |
| 1 | DUMMY_RUNNER (Dummy Runner) | 4 | 4 | – | 8 | 14053bb90, 14053bf10, 140607bc0×3, 140667310, 1406725f0, 1406768e0 |
| 2 | FOX_IN_THE_BOX (Fox in the Box) | 21 | 1 | – | 22 | 14053ba10, 14055f590, 14056df90, 140591d70, 1405c1580, 1405d1e60, 1405d8b70, 1405dc350, 1405def90, 1405df1f0, 1405e6e80×3, 14060bdd0, 140644f10, 140663970, 1406718f0, 1406725f0, 1406768e0, 140680980, 14099d650, undef×1 |
| 3 | POST_PLAYER (Target Man) | 20 | – | – | 20 | 14053ba10, 1405571c0, 140596aa0, 1405d8b70, 1405dc350, 1405e6e80×4, 1405fc500, 140603d00, 1406046b0, 140610a30, 14061fa40, 140644f10, 140663970, 140664ee0, 1406718f0, 1406725f0, undef |
| 4 | CHANCE_MAKER (Creative Playmaker) | 13 | – | – | 13 | 14053bdc0, 14055f590, 1405d8b70×2, 1405dc350, 1405def90, 1405df5a0, 1405e2c90, 1405e6e80×2, 140608ac0, 14066b980, undef |
| 5 | WING_STRIKER (Prolific Winger) | 11 | 1 | – | 12 | 14053bf10, 14055f590, 1405a0a70, 1405d8b70, 1405dc350, 1405e6e80×2, 1406087a0, 14066b980, 1406725f0, 14067d6d0, 140680980 |
| 6 | INSIDE_PLAYER (Roaming Flank) | 10 | – | – | 10 | 1405d8b70, 1405dc350, 1405df5a0, 1405e6e80×3, 140663440, 14066b980, 140680980×2 |
| 7 | CROSSER (Cross Specialist) | 6 | 1 | – | 7 | 14053ba10, 1405dc350, 1405e6e80, 14067bc10, 14067d6d0×2, 140680980 |
| 8 | CLASSIC_NO_10 (Classic No. 10) | 11 | – | 2 | 13 | 14053bdc0, 14055f590, 1405c1620, 1405e6e80, 140600e40, 14067d4a0, 140681a20, 140958d00, 140a53530, **142125c80, 142132660** (E890), undef×2 |
| 9 | FREE_ROAMING (Hole Player) | 18 | 2 | – | 20 | 14053bb90, 14059ba60, 1405a6910, 1405c0f00, 1405e6e80, 140603d00, 14060c390, 14060cb50, 14060d210, 14060f9f0, 14060fe20, 1406121e0, 140667480, 14066e130, 1406725f0×2, 140681a20, 140691a80, undef×2 |
| 10 | BOX_TO_BOX (Box-to-Box) | 9 | – | – | 9 | 1405def90, 1405df1f0, 1405e6e80×4, 1406121e0, 14061b590, 1406725f0 |
| 11 | ANCHOR_MAN (Anchor Man) | 15 | – | – | 15 | 14055f590, 1405c1620, 1405d3550, 1405da3a0, 1405dc350, 1405e2c90, 1405e6e80×3, 14067b470, 140681a20, 14095cb90, undef×3 |
| 12 | ENFORCER (The Destroyer) | 1 | – | – | 1 | 1405c0f00 (key 0x30/0x34 path) |
| 13 | PLAY_MAKER (Orchestrator) | 11 | – | – | 11 | 1405df5a0, 1405e6e80×3, 140603d00, 140608ac0, 14060fe20, 140611140, undef×3 |
| 14 | BUILD_UP (Build Up) | 3 | – | – | 3 | 14053b900, 1405e6e80, 140610a30 |
| 15 | OVERLAP (Extra Frontman) | 6 | – | – | 6 | 14051b8b0, 1405f63c0, 1405f7d00, 1405fc860×3 |
| 16 | OFFENSIVE_SB (Offensive Full-back) | 7 | – | – | 7 | 1405df5a0, 1405e6e80, 140640cc0, 140641690, 14067ecc0, 140994090, undef |
| 17 | DEFENSIVE_SB (Defensive Full-back) | 8 | 1 | – | 9 | 14056df90, 1405a0d20, 1405da3a0, 1405df5a0, 1405e6e80, 140641690, 14067c050, 14067ecc0, undef×1 |
| 18 | INNERLAP (Full-back Finisher) | 10 | – | – | 10 | 1405dc350, 1405e6e80, 140608ac0, 140640cc0, 140641690, 14067ecc0, 140994090, undef×3 |
| 19 | OFFENSIVE_GK | 0 | 0 | 0 | 0 | keepers read directly (§5, R6) |
| 20 | CLASSICAL_GK | 0 | 0 | 0 | 0 | keepers read directly (§5, R6) |

Per-site record (all 228, with raw writer VA and the instruction text): `census.tsv`. Host function names are Ghidra `FUN_<hex>` (listed as `<hex>` above). `undef` = call site in code Ghidra does not define (18 sites, all in `FUN_140A64030`; list in `census.tsv`).

### 1.1 Undefined-host sites (CONFIRMED raw)
Call-aligned by capstone; BIT from the last `MOV EDX,imm`/`LEA` before the call:
`140601a10→16, 140607552→17 (cmp ebp,2 path), 140607734→9, 14060f219→13, 14060f247→9, 1406206b4→8, 1406206ce→11, 1406206e8→13, 14062073c→18, 140620757→4, 14062087c→3, 140620892→2, 140613b12→8, 140613b2d→11, 140613b48→13, 1405d52ba→18, 1405d6d02→18, 1405d7817→11`.

### 1.2 Shared tail and quality gate `FUN_1405C0F00` (CONFIRMED raw jump table)
`FUN_1405C0F00(playerIdx, key=R8D)` has a switch on key. The raw table (`0x5C1084` index bytes, `0x5C1060` rel32, base `0x140000000`) maps each key to a handler that sets `EDX` then jumps to the tail `1405C0F97`:

| key | bit(s) | how |
|---|---|---|
| 0x1D, 0x1F | 9 | `MOV EDX,9` → tail |
| 0x1E | 0, then 9 | `XOR EDX,EDX` → call `1405C0FBD`; else `MOV EDX,9` → call `1405C0FD3`; if bit 9 passes, `FUN_140A4C9A0` (not a style read) decides |
| 0x20, 0x2F | 0 | `XOR EDX,EDX` → tail |
| 0x21 | 1 | `MOV EDX,1` → tail |
| 0x27 | 16 | `MOV EDX,0x10` → tail |
| 0x28 | 15 | `MOV EDX,0xF` → tail |
| 0x30, 0x34 | 12 | `MOV EDX,0xC` → direct call `1405C1029` |
| 0x3A | 10 | `MOV EDX,0xA` → tail |
| other | none | returns 0 |

Callers of `FUN_1405C0F00` (13, all CONFIRMED by raw `MOV R8D,imm` or `LEA R8D,[R15+0x1E]` with R15=0):

| caller | key | style bit(s) | group |
|---|---|---|---|
| 14052f720 | 0x3A | 10 | _BoxToBox marking (dt18) |
| 140533bc0 | 0x3A | 10 | _BoxToBox marking block (`0x140533BC0`) |
| 1405aca50 | 0x1E | 0, 9 | off-ball run |
| 14060a8d0 | 0x1F | 9 | off-ball run |
| 140612660 | 0x1D | 9 | off-ball run |
| 14060d210 | 0x1D | 9 | off-ball / attack |
| 14053e9a0 | 0x1F | 9 | off-ball run |
| 140612fc0 | 0x21 | 1 | Diagonal / _dummyRunner selector |
| undef 140602651 | 0x1E | 0, 9 | off-ball run |
| undef 14060725f | 0x1E | 0, 9 | off-ball run |
| undef 14060b05c | 0x1F | 9 | off-ball run |
| undef 14061e180 | 0x2F | 0 | off-ball run |
| undef 140613cae | 0x21 | 1 | Diagonal / _dummyRunner |

## 2. Host functions grouped by system

System = the repo `docs/re` (`pes-re-*.md`, RunTargets*, OffBall*, Setpiece*, CarrierAI*) that cites the host (per-host doc list in `docmap.tsv`). Hosts with no `docs/re` citation are listed under "Uncited".

- **Off-ball runs / run targets / attack decisions** (cited in `pes-re-off-ball`, `pes-re-run-targets*`, `RunTargetsRE.*`, `pes-re-attack-*`, `OffBallSelectors`, `pes-re-chance-lanes`): 140591d70, 140595ab0, 1405a0d20, 1405a5b20, 1405c1620, 1405c0f00 (quality gate), 1405d8b70, 1405df5a0, 1405e6e80, 140600e40, 140601d80, 1406087a0, 140603d00, 140608ac0, 14060c390, 14060d210, 14061fa40, 140640cc0, 140641690, 140644f10, 140662a10, 140663440, 140663970, 140664ee0, 140667310, 140667480, 14066b980, 14066e130, 1406718f0, 1406768e0, 140691a80, 14060cb50.
- **Positioning / formation / team shape / tactics** (`pes-re-tactics`, `pes-re-foundation*`, `pes-re-gaps-match`, `pes-re-team-ai-leftovers`, `pes-re-constant-team`, `pes-re-transitions`): 1405d1e60, 1405d3550, 1405dc350, 1405df1f0, 1405e2c90, 1405c1580, 1406046b0, 140607bc0, 14060bdd0, 14067bc10, 14067d6d0, 14067ecc0, 140681a20, 140958d00, 14096dfc0, 140994090, 14099d650, 140a53530, 140610a30.
- **Set pieces** (`SetpiecePlacementRE`, `pes-re-setpiece-*`, `pes-re-throwin-choreography`, `pes-re-taker-map`): 14051b8b0 (placement), 140680980 (throw-in), 1405fc860 (taker map), 1405f63c0 (taker map / contact), 1405f7d00.
- **Carrier / pass receiver / shot target** (`pes-re-carrier-*`, `pes-re-A4Carrier`, `PlayStyles`, `SetplayWaitRE`): 142125c80 (short-forward / receiver), 142132660 (receiver scorer), 14053b900.
- **Offside / counter** (`OffsideAwarenessPort`, `offsideawareness`, `CounterTrigger2RE`): 14059ba60. 1405a6910 is uncited.
- **Contact / defence** (`pes-re-contact`): 1405f63c0.
- **Keeper**: no wrapper sites. Keeper readers are the direct R6 sites in §5.
- **Uncited** (no `docs/re` mention; grouping = INF from the surrounding wrapper callers): 14053ba10, 14053bb90, 14053bdc0, 14053bf10, 1405571c0, 14055f590, 140563040, 14056df90, 140596aa0, 1405a0a70, 1405a6910, 1405da3a0, 1405def90, 1405f7d00, 1405fc500, 14060f9f0, 14060fe20, 140611140, 1406121e0, 14067b470, 14067c050, 1409502f0, 1409512e0, 140952a30, 14095cb90, 140607552 (undef), plus the other 17 undef sites.

## 3. Per-style effect (what changes when the bit is set)

"Gate" = the wrapper result is taken as a branch input at the call. Effects marked OPEN are not decoded here.

- **0 Goal Poacher (31 sites):** Quality keys 0x1E and 0x2F (§1.2) read it at off-ball run selection (13 callers, §1.2). Effect beyond the gate: OPEN (which branch the key feeds is not decoded).
- **1 Dummy Runner (8):** Quality key 0x21 (`1405C0F00` ← `140612FC0`, undef `140613CAE`) reads bit 1 for the player. Per doc §6 this gate selects the Diagonal branch `0x140613CC0`, which uses the `_dummyRunner` constants. Gate CONFIRMED; the constants are from the doc and not re-verified here.
- **2 Fox in the Box (22):** Direct readers (CONFIRMED decompile):
  - `FUN_1402C0B30` at `1402C0C7D`: `(copy+0xB8)>>2 &1` → `FUN_1402C1570(copy,1)`.
  - `FUN_1418136C0` at `14181378A`: bit 2 is the loop test; while set, the chain walk `FUN_1402BFD00` continues.
  - `FUN_140E295D0` at `140E295DA`: `(sub+0x50)>>2 &1` set → `[+0x58]=1`, `[param_2+0x34]=0`, early return (skips the +0x30 distance update).
  - Twelve functions test copy+0xB8 bit 2 and pass it to `FUN_1402BFD00` (`141813E36`…`1419297A1`, raw CONFIRMED; behaviour OPEN).
  - Wrapper sites: 22 (§1). Effect OPEN beyond these.
- **3 Target Man (20):** wrapper sites only. The doc says the long-target rule keys on height, not the bit. Effect OPEN.
- **4 Creative Playmaker (13), 5 Prolific Winger (12), 6 Roaming Flank (10), 11 Anchor Man (15), 13 Orchestrator (11), 16 Offensive FB (7), 17 Defensive FB (9), 18 Full-back Finisher (10):** wrapper sites only. Direct decode: bit 11 is also read on copy+0xB8 by `FUN_141AAB320` (decompile line 126, `(copy+0xB8)>>0xB &1`), which selects between `FUN_141AA0520` and a plain compare loop. Effect OPEN for all of these.
- **7 Cross Specialist (7):** R3 `FUN_14211AB10`, `14211AC8D`: `mov eax,[rbx+0x48]; shr eax,7` (raw CONFIRMED) → Cross base-50 fallback (doc, com-styles §6.3; constants not re-verified).
- **8 Classic No. 10 (13):** Receiver scorers `FUN_142125C80` (`1421264C9`: `MOV R8D,8` → `1421264D5` gate, EDX=player idx) and `FUN_142132660` (`142132A23`: `MOV R8D,8` → `142132A2B`). Both return immediately on pass (doc R2a/R2b; call sites CONFIRMED raw, consumer behaviour from doc). Direct read: `FUN_141AAB320` tests copy+0xB8 bit 8 → skip compare loop. Other sites OPEN.
- **9 Hole Player (20):** Quality keys 0x1D/0x1E/0x1F (callers in §1.2: `1406126AB`, `14060D495`, `14060AA39`, `14053F17B`, `1405ACAAE`, `14060B05C`, `14060725F`, `140602651`) read bit 9. Effect OPEN beyond the gate.
- **10 Box-to-Box (9):** Quality key 0x3A from `14052F720` and `140533C10` (`FUN_140533BC0` = _BoxToBox marking block). Gate selects the `_BoxToBox` profile (doc §6). **Verdict changed (§4).** Other sites (1405E6E80 ×4, 1405DEF90, 1405DF1F0, 1406121E0, 14061B590, 1406725F0) OPEN.
- **12 The Destroyer (1 wrapper + direct):** Direct R5a `FUN_14078E5A0` at `14078E9B4` (raw CONFIRMED): `call 14124DAD0` → `mov esi,[rax+0x48]; shr esi,0xc` (copy+0x70 block). Direct R5d `FUN_140971BA0` at `140971F85` (raw CONFIRMED): `mov ebx,[rax+0x48]; shr ebx,0xc; and ebx,1`. Constants (11.25°/45°/0°, touch 0.075/0.1/0.125·gs, slide `4.0+0.8L`) are from the doc §7 and not re-verified here. **R5b `140790441` is not an instruction boundary in the raw decode; the doc VA needs fixing.**
_Corrected 17-10-26 from pes-re-style-effects.md §5/§7: the 0° is a register-zeroed literal (pesdefenderdecisions.cpp:91); VA `0x14259BE20` reads 25.0 and is not its source._
- **14 Build Up (3):** `1405E818E` (gate with EBX==1 → BIT 14, `LEA EDX,[RBX+0xD]`). Effect OPEN (`approachRateMax_buildUp` reader untraced, doc §6).
- **15 Extra Frontman / OVERLAP (6):** `FUN_141B620D0` (block+0x48 bit 15; if set and `[block+0x4C]&0x01000000`, clears bit 15 with `AND 0xFFFF7FFF`). This is a style-word writer/normaliser, not only a reader. Also `FUN_141B620D0` writes other players' style words (`…+0x48 |= (b>>7)<<bit`). Gate sites OPEN.
- **19 Offensive GK / 20 Defensive GK:** no wrapper sites. Direct R6 sites: `1409DDB69` (bit 20, `shr eax,0x14; test al,1`, raw CONFIRMED), `1409DED4A` (bit 19, `shr ecx,0x13; test cl,1`, raw CONFIRMED), `142116852` (bit 19, `shr ecx,0x13; test cl,1`, raw CONFIRMED). The remaining R6 sites (`1409DF7D0`, `1409E03BE`, `142115BB2`, `1421163F4`) were not re-swept (see §5 recall).

## 4. Corrections to `pes-re-playing-styles.md`

**Stands:**
- §0 bit→name table (engine order), §1 storage.
- R2a/R2b (`142125C80`, `142132660`): bit 8 CLASSIC_NO_10 (`R8D=8`). The gate arg order was mislabelled in the doc (§2 R1); the gate call itself is correct.
- R3 (bit 7, `14211AC8D`), R5a (bit 12, `14078E9B4`), R5d (bit 12, `140971F85`), R6b (bit 20, `1409DDB69`), R6c (bit 19, `1409DED4A`), R6g (bit 19, `142116852`): raw CONFIRMED.
- The five confirmed in-match effect styles (7, 8, 12, 19, 20) stand as the **decoded** effects. Other bits are in §3.
- §5 dead tables (`0x1436F2110`, `0x1436F2260`, `playStyle.o`): not re-checked here; no change claimed.
- §6 dt18 constants and §7 constants: not re-verified here.

**Retracted:**
1. **§6 `_dummyRunner` "selected by the Diagonal quality gate 0x1405C0F00(…,0x21) — NOT the style bit."** Retracted. The 0x21 key tests bit 1 for the player (§1.2, raw). The gate is a style gate.
2. **§6 `_BoxToBox` "selected … 0x1405C0F00(…,0x3A) — NOT the style bit."** Retracted. The 0x3A key tests bit 10 for the player (§1.2, raw). The `_BoxToBox` profile is selected by a style-gated branch (the `0x14052C440` role check still applies; the style bit is an additional input).
3. **§2/§10 "run choice never branches on the style mask" (off-ball sweep, 13/14 copy-uses).** Retracted. The 13 `FUN_1405C0F00` callers are style-gated (bits 0, 1, 9, 10, 12, 15, 16 via the tail).
4. **§3 "compat-only; effect INF" for bits 0–6, 9, 11, 13–18.** Retracted as "no in-match consumer". All bits 0–18 now have ≥1 in-match reader (§1), and bits 19–20 have direct readers. The effect for most remains OPEN (§3), but "16 styles do not branch in-match code" is wrong.
5. **§10 "other 16 bits really have no in-match consumer" and "direct-block readers CLOSED".** Retracted. The `[r+0x50]` sweep did not cover `copy+0xB8` (copy+0x70+0x48), which is how bits 2, 8, 11 are read directly (§3). Bits 1, 2, 8, 11, 15 have direct readers confirmed by decompile (§3); bit 1 has candidate readers only.
6. **§2 R1 gate label "(block, bit, slot)" for `FUN_14063E890`.** Corrected: `(team, playerIdx, BIT)`; BIT = R8D (§0).
7. **§2 R5b `140790441`.** Not an instruction boundary in raw bytes. OPEN until re-located.
8. **§2 R5c `14096FDA0` (`~0x14096FE??`).** Approximate VA in the doc; not found by the sweep. OPEN.
9. **§2 R6a/R6d/R6e/R6f.** Not re-verified by the sweep (recall gap, §5). OPEN.

## 5. Re-sweep for direct style-word tests (`census_sweep.py`)

Method: raw bytes in `pes_full.bin`. Candidates = `[reg+0x48]` and `[reg+0x50]` (disp8, mod=01) and `[reg+0xB8]` (disp32, mod=10), 4.84M candidates. Capstone-verified load, then a constant `shr/sar/bt` within 4 instructions. Results: 5,673 hits in the image; 1,087 in the code range; 574 with an immediate-count consumer; 81 with a 32-bit load and bit index ≤20; about 40 of those have an `and/test 1` or `bt` consumer (BIT1 class).

**Recall limit:** caught 5 of 6 doc-verified direct sites (R3, R5d, R6b, R6c, R6g). Missed R5a (`14078E9B4`, confirmed by hand). This sweep is **not exhaustive**. It is a candidate list.

**Direct readers, CONFIRMED by decompile or raw:**

| bit | VA (raw) | base | function | effect (what the test does) |
|---|---|---|---|---|
| 2 | 1402C0C7D | copy+0xB8 | FUN_1402C0B30 | `FUN_1402C1570(copy,1)` if set |
| 2 | 14181378A (+11 more) | copy+0xB8 | FUN_1418136C0, 1419292C0, 141838480, … | chain walk via FUN_1402BFD00 |
| 2 | 140E295DA | sub+0x50 | FUN_140E295D0 | `+0x58=1`, `+0x34=0`, early return |
| 8 | 141AAB3E8 | copy+0xB8 | FUN_141AAB320 | skip compare loop when set |
| 11 | (decompile line 126) | copy+0xB8 | FUN_141AAB320 | alternate path `FUN_141AA0520` |
| 12 | 14078E9B4 | copy+0x70 block | FUN_14078E5A0 | R5a tackle entry |
| 12 | 140971F85 | block | FUN_140971BA0 | R5d slide reach |
| 15 | 141B6220F | block | FUN_141B620D0 | reads bit 15, clears it if `+0x4C`&0x01000000 |
| 19 | 1409DED4A, 142116852 | block | FUN_1409DE8C0, FUN_1421163F4 | R6c, R6g |
| 20 | 1409DDB69 | block | FUN_1409DD470 | R6b |

**Candidate (raw BIT1 consumer, behaviour not decoded; OPEN):** `1402C4EFD` (bit 2, `[rcx+0x48]`, in FUN_1402C4EE0); `140308F19` (bit 1, `[rdi+0x48]`, FUN_140308EB0); `14166D564` (bit 1, `[rsi+0x48]`); `14166D99A` (bit 1, `[rax+0x48]`); `140E2A748` (bit 1, `[rdi+0x50]`, FUN_140E2A520); `14174B73D` (bit 2, inverted, undefined code); `1402BD8AE` (bit 1, copy+0xB8, undefined code).

**Single-bit getters (raw CONFIRMED, not Ghidra functions):** `141E94410` (copy+0xB8 bit 1), `141E944D0` (copy bit 2), `141E94460` (block+0x48 bit 1), `141E94520` (block bit 2). They are referenced only from `.rdata` DATA records at `0x142B628E0`–`0x142B63E10` (stride 0x18/0x50). Their callers are indirect and **OPEN**: a table dispatcher is not located.

**Excluded (not style):** `1415244FC`/`141524582` (`shr ecx,9; and ecx,7`, a field); `141E59C80`/`141E59CE0` (`and 0x3F` fields); `142125C80:279` reads `[copy+0x70+0x40]` (**skill** word, bit 12), not style.

## 6. GF gap list

1. **Quality gate `1405C0F00` (keys 0x1D/0x1E/0x1F/0x20/0x21/0x2F/0x3A, plus 0x27/0x28/0x30/0x34 unused by callers):** GF has no style-gated run/marking quality test. Port the key→bit table (§1.2) and the 13 callers' keys (§1.2).
2. **Dummy Runner / Box-to-Box profile selectors:** GF must gate the Diagonal (`_dummyRunner`, key 0x21 → bit 1) and `_BoxToBox` (key 0x3A → bit 10) profile on the player's style. Port the selectors, not the constants (constants per doc §6).
3. **Classic No.10 receiver preference:** R2a/R2b (`142125C80`, `142132660`) return on bit 8 via `14063E890`. GF has no counterpart.
4. **Fox in the Box (bit 2) on copy+0xB8:** 12+ readers plus the `1402C0B30`/`1418136C0`/`140E295D0` effects. GF has no FOX read on the copy object.
5. **Extra Frontman (bit 15):** `141B620D0` both reads and clears bit 15 under `[block+0x4C]&0x01000000`. GF needs the normalisation or it must be documented as a deliberate omission.
6. **Keepers (bits 19/20):** R6 sites (§3) are in `elizacontroller.cpp`/`goalie_default.cpp` stand-ins (doc §8). Rewire to these VAs.
7. **Destroyer (bit 12):** R5a/R5d confirmed raw; R5b VA wrong; R5c approximate. Fix the VAs before porting.
8. **`FUN_141465180` (float-bit gate):** no static callers. Its dispatcher is **OPEN**; GF must not port it until found.
9. **18 undefined-host call sites (§1.1):** no GF counterpart; host functions are not in Ghidra's function list. Decode from raw bytes or leave as "dispatch unknown".
10. **Off-ball / positioning styles (bits 0, 3–6, 9–11, 13, 16–18):** 167 wrapper sites. Effects OPEN; GF's `GetPoacherTargetX`, `GetAttackingRunAffinity` etc. (doc §8) remain stand-ins with no PES site.

## 7. OPEN

- Effects of bits 3–6, 11, 13, 16–18 (wrapper sites only, §3): which branch each feeds.
- Dispatcher of `FUN_141465180` and of the 141E944xx getter table (`0x142B628E0`…). Static search found none.
- R5b (`140790441`) and R5c (`14096FDA0`) VAs.
- `1402BD8AE`, `14174B73D`: undefined code; behaviour not decoded.
- Recall of the re-sweep (§5): it did not catch R5a, and R6a/R6d/R6e/R6f were not re-checked.
- Gate `1408CB0B0` has exactly four callers (`14063E890`, `140A63FE0`, `140A64030`, `141465180`), all confirmed by xrefs; no other direct caller.

## 8. Verification

- Tests/builds: none run (read-only decode; no engine edits).
- Scratch and reproducers: `census_fetch.py`, `census_analyze.py`, `census_final.py`, `census_raw.py`, `census_riprel.py`, `census_sweep.py`, `census_docmap.py` in `<workdrive>/gf-StyleCensus-scratch/`.
- Request count: about 130 of 300.
