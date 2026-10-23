# PES condition roll: the +0x147 writer, the arrow's randomness, and the GF port (BandByteRE, 23-10-26)

Parent: `docs/re/pes-re-condition-writer.md` (writer chain) + `docs/re/pes-re-condition-bands.md`
(band edges, prep reads, RNG mechanism). That file's §6 narrowed the one unknown to the writer of
the edit slot record's band byte `+0x147`. This file closes it, plus the remaining opens it could
reach within budget: the own-team shift's `+0x20 == 3` gate, the `/100 == 6` predicate, the Form
mirror reader, and the exact GF port.

Image: `~/ghidra-projects/scratch/Unpacker/pes_full.bin`, file offset = VA − 0x140000000.
Bridge one request at a time. Floats below are script-read from the image (owner ruling 19-10-26).
Checker: `<workdrive>/gf-BandByteRE-scratch/check_condition_roll.py` — 60 byte/float asserts,
GREEN 23-10-26; one mutation run (0x46→0x47 at 0x140AF2EB1) goes RED on `band.lt46`, reverted.

## 0. Script-read constants (pasted tool output)

```
0x140AF2EAC 83f841            0x140AF2EB1 83f846            0x140AF2EBD 83f84b
0x140AF2EC9 83f850            0x140AF2ED5 83f855            0x140AF2EE1 83f85a
0x140AF2EED 83f85f            0x1414E22FC 4180f828 (CMP R8B,0x28)
0x1414E2302 41b028 (MOV R8B,0x28)   0x1414E2320 b046 (MOV AL,0x46)
0x141FE3973 440fb68047010000 (MOVZX R8D,[RAX+0x147])
0x141FE397B 4183e007 (AND R8D,7)    0x141FE3984 e8f7d9fdff (->0x141FC1380)
0x141FE4100 440fb68647010000 (MOVZX R8D,[RSI+0x147])
0x141FE4108 4183e007              0x141FE4113 e868d2fdff (->0x141FC1380)
0x14092B4D6 8b5864 (MOV EBX,[RAX+0x64])   0x14092B4D9 e8e2561100 (->0x140A40BC0)
0x14092B4EF 83f803 (CMP EAX,3)    0x14092B500 e8cb561100 (->0x140A40BD0)
0x14092B50D e8ce4d1300 (->0x140A602E0)     0x14092B505 ba06000000 (MOV EDX,6)
0x14092B516 4183fc027205418bd6eb0641 (CMP R12D,2; JC; MOV EDX,R14D/JMP; MOV EDX,R12D; XOR EDX,1)
0x14092B536 e885561100 (->0x140A40BC0)     0x14092B53B 83f803 (CMP EAX,3)
0x14092B57B 83fb04 (CMP EBX,4)    0x14092B580 ffc3 (INC EBX)
0x14092B595 899f88c70000 (MOV [RDI+0xC788],EBX)
0x14092B59B 83fb05 (CMP EBX,5)    0x14092B5A0 83fb02 (CMP EBX,2)
0x14092B5A8 488d150578d001 (LEA RDX,[0x142632DB4] tier list)
0x14092B5D4 8b848b702d6302 (MOV EAX,[RBX+RCX*4+0x2632D70], base 0x142632D70)
0x140B159D8 888347010000 (MOV [RBX+0x147],AL — the ONLY +0x147 writer, a copy)
0x140B159CB 888346010000 (MOV [RBX+0x146],AL — same copier)
0x141FC1386 bb02000000 (MOV EBX,2)  0x141FC13E1 bb04000000 (MOV EBX,4, band 0)
0x141FC13CD bb03000000 (band 1 -> 3)  0x141FC13B9 bb01000000 (band 3 -> 1)
0x141FC13A8 33db (XOR EBX,EBX, band 4 -> 0)
0x1408C7710 895158c3 (MOV [RCX+0x58],EDX; RET)
0x141FC1190 48895c2408574883ec204863
0x141FBA4B1 e8da6c0000 (->0x141FC1190)  0x141FBA4C5 e80680a8fe (->0x140A424D0)
0x141030B40 8b4120c3 (MOV EAX,[RCX+0x20]; RET)
0x140A924E0 4d63c80f57c941b8134500000f57c04269448908e3b2222d (RNG; no call in prep chain)
0x14259BE5C 0000b442 = 90.0    0x14253EE30 0000c842 = 100.0
0x1426550D8 00000030 = 4.6566129e-10 (1/2^31)   0x14259BE30 00003442 = 45.0
0x1425AA1C0 00809d43 = 315.0
0x142632D70 row0: 1.12 1.06 1.00 0.92 0.84 (PES index 0=Top..4=Terrible)
```

## 1. The +0x147 writer — CONFIRMED: a struct copy, not a roll

Full-image scan for every ModRM/SIB encoding that stores to displacement 0x147 (213 raw hits,
almost all in data/pack regions above 0x143000000) leaves exactly three code sites in the main
executable range:

| VA | bytes | what |
|---|---|---|
| `0x140B159D8` | `88 83 47 01 00 00` | `MOV [RBX+0x147],AL` inside `FUN_140B158D0` |
| `0x14215DA71` | `88 47 14?` family | `MOV [R14+0x147],AL` inside `FUN_14215CF10` region — JSON/option-field filler (param_2-driven `*(p+i)==2` gates), not an edit record |
| `0x1418E2246` | `88 85 47 01 00 00` | `MOV [RBP+0x147],AL` with EAX=0 init-zero block, not a roll |

(`0x151DFDE37` / `0x154C247AB` are data-region false positives of the naive `88 85 47 01`
pattern: no function contains them. `0x1412AED24/90/CE` are `MOV EDX,0x147` — field id 0x147 =
327 decimal to a string/getter call `FUN_1413374D0/141336CA0/141336E70`, not a byte store.)

`FUN_140B158D0` decompiles as a fixed-layout struct copier (~0x420 bytes): first word masked
`& 0x3FFF` exchange, then byte runs `0x23`, `0xB5`, `0x20`, `0x14` counts, carrying `+0x145`,
`+0x146`, `+0x147`, `+0x148` verbatim from source to dest:

```
*(dst+0x145) = *(src+0x145); *(dst+0x146) = *(src+0x146);
*(dst+0x147) = *(src+0x147); *(dst+0x148) = *(src+0x148); ...
```

23 callers image-wide (among them `FUN_140B15090`, `FUN_140B759F0`, `FUN_140B79770`,
`FUN_140B870E0`, `FUN_141066710`, `FUN_141EA1BF0` ×2). CONFIRMED at the bytes.

**Verdict: there is no instruction in the shipped image that computes a fresh `+0x147` value
at match setup.** The byte arrives with the edit record (edit-save data / install roster
import), is copied by `FUN_140B158D0`-family blits, and match prep only reads it
(`0x141FE3973` / `0x141FE4100`, both `& 7`, both calling `FUN_141FC1380`). The arrow's
"randomness" therefore does NOT enter through an RNG stream/seed at match time — the prep
chain `FUN_141FE3830` contains zero `FUN_140A924E0` calls (re-CONFIRMED: no RNG in its
795-line disassembly), and the band ladder `FUN_140AF2DA0` runs at edit-data build, not at
kickoff. The per-match variation PES shows comes from the condition byte the edit record
already carries (mixer `FUN_141E5B540` output clamped `0x28..0x6D`, §4) plus the two manual
edit inputs below. Any GF "roll at kickoff" is a stand-in: label it `// ponytail:` naming
`0x140B159D8` (copier) and `0x141FE3973` (reader) until the edit-save byte's upstream is
decoded from the save/import path.

### What writes the slot record's +0x147 upstream of the copier

The slot record is `FUN_1414C2170(base+0x35960, side, slot)` =
`base + ((slot + side·0x28)·0x188) + 0x1308` (CONFIRMED, decompile + byte-verified prologue).
Its `+0x147` is filled wherever the edit slot array is first populated — the fill sites in
`FUN_141FC5500` (`0x141FC55CB`, `0x141FC5823`) only pass the record pointer to
`FUN_140A81830` (a 3-dword × 0x28 store at `param_1 + slot·3 + 0x96/0x97`, slot-guarded) and
to the sort `FUN_141FC4B30`; neither computes a band byte. The genuine producer is the
edit-player build `FUN_140AF2DA0` chain (§4): condition byte → ladder → band field 0x30 →
copied into the 0x188 slot record by the edit-save import path that `FUN_140B158D0` serves.
That import (save-blob → slot array) is the remaining OPEN link, not a missing RNG: the
values it carries are the mixer's deterministic outputs plus persisted per-player condition
state.

## 2. `FUN_141FC1190` (the +0xA298 veto) — CONFIRMED: a menu-mode gate, not an RNG

```c
ulonglong FUN_141fc1190(longlong param_1,int param_2){
  if (param_2 < 2) {
    uVar1 = FUN_14149a470();            // global context pointer
    in_RAX = FUN_14149f4f0(uVar1);      // mode = table[ctx+0xF0], clamped
    if ((int)in_RAX != 10) return *(byte *)(param_2 + param_1);
  }
  return in_RAX & 0xffffff00;            // nonzero (mode<<8 masked) = "veto"
}
```

`FUN_14149F4F0` = `EAX = [RCX+0xF0]; if (EAX > 0x3D) EAX = 0; return table_1434FC9C0[2*EAX]`
(byte-verified: `8B 41 F0; 83 F8 3D; 0F 47 C1; LEA RCX,[0x1434FC9C0]; ADD RAX,RAX;
8B 04 C1`). So the veto reads a small mode id out of the context object and maps it through
a 0x3E-entry table; mode value 10 disables the veto (returns the caller's byte), any other
mode returns nonzero.

Four callers, all menu/setup-time code: `0x141FC172F` (in `FUN_141FC1590`, formation setup),
`0x141FBA4B1` (in `FUN_141FB9FB0`, the `+0xA298` setter path), `0x141FBB5D7` (in
`FUN_141FBB530`, 2-slot compare), `0x141FCE6D9` (in `FUN_141FCE150`, setup with
`[RSP+0x58]` byte array). In `FUN_141FB9FB0` the use is:

```
uVar11 = FUN_141FC07B0(*(edit+0x20))   // identity map 0..3 else 0
... if (FUN_141FC1190(&local, slot)) uVar11 = 0   // veto zeroes the state
FUN_14140A424D0(team, uVar11)          // +0xA298 = state
```

So `+0xA298 == 3` ⇔ the edit record's `+0x20 == 3` **and** the mode veto is silent.
`+0x20` itself is read by the 1-line thunk `FUN_141030B40` (`8B 41 20; C3`).
`FUN_141FC07B0` is the identity map `0→0,1→1,2→2,3→3,else 0` (bytes above). All CONFIRMED.

What `+0x20 == 3` and mode 10 mean (which menu/setup state) is still OPEN — it lives in the
edit-data/menu layer (`0x1415xxxx` edit functions), not the match engine. The match engine
only consumes it.

## 3. `+0x20 == 3` gate — narrowed to the edit word, semantics OPEN

Chain (all CONFIRMED at bytes or 1-line decompile):
`FUN_141030B40` (`*(edit+0x20)`) → `FUN_141FC07B0` (identity 0..3) → veto `FUN_141FC1190`
→ `FUN_140A424D0` (`MOV [RCX+0xA298],EDX`) writes the team state; default
`FUN_140A415A0` sets word `0x28A6` (= `+0xA298`) to **4**; consumer `FUN_14092AF10` forces
arrow index 0 when `FUN_140A40BC0` (`MOV EAX,[RCX+0xA298]`) returns 3 — for the own-team
branch only after the `/100 == 6` predicate fails (§5), and for the opponent branch
unconditionally (`0x14092B536/53B/53E`).

Writers of edit `+0x20 == 3`: the edit setter is `FUN_1414C6E90` (field-indexed writer;
30+ callers) and the edit-save import path. Which menu action stores 3 there was not traced
within budget — OPEN, menu-layer.

## 4. Condition byte 0xD path — CONFIRMED deterministic, no RNG, no Form

`FUN_140AF2DA0` (`0x140AF2E8D`): `R8D=2; [RBP+0x50]=0xD; RCX=RDI; RDX=&...;
CALL 0x1415130F0` (thunk to `FUN_1414E2120`), then the 7-edge ladder on EAX
(`CMP 0x41/JC; 46; 4B; 50; 55; 5A; 5F`, `SBB/ADD` tail), store via `FUN_1414C6E90`
field `0x30`, then `+0x178` (Form mirror, field `0x2D`), `+0x179` (cond byte),
`FUN_14150EC40` overall → `+0x13C` bits, `FUN_140AF06D0` 0x17C-record copy. CONFIRMED.

`FUN_1414E2120` (field-0xD reader): checksum-gated (`DAT_14351AE30/34` match → default
`0x46` at `0x1414E2320`, bytes `B0 46`); else builds the ability array via `FUN_1414C66C0`
cases, maps ids via `FUN_1414E2060`, calls mixer `FUN_141E5B540`, optional scale for
`param_3` 3..4 (`MULSS [RSP+R14*4+...]` float table at `0x14297BE50`, `0x142588200`),
then clamp: `CMP R8B,0x28; JNC; MOV R8B,0x28` (`41 80 F8 28 / 41 B0 28`) low side;
`CMP R8B,0x6D; CMOVA` high side; mid-range default `MOV AL,0x46` only on the
checksum-mismatch path. No RNG call anywhere on this path. CONFIRMED.

Mixer `FUN_141E5B540`: ability-weighted sum over the 13-id table at `0x142B47AD0`
(script-read: `0C 09 0B 0A 05 06 08 07 04 03 02 01 00` — 13 ability ids, no 0x2D/Form),
`(Σw·v + 0x32)/100`, clamp `0x28..0x6D`, default `0x46`. CONFIRMED — Form does not enter
the condition byte through the mixer.

## 5. The `/100 == 6` predicate — CONFIRMED mechanics, code meaning INF

`FUN_140A602E0` = `return *param_1 / 100 == param_2` with `param_2 = 6`
(`0x14092B505 BA 06 00 00 00`). `FUN_140A40BD0` feeds it: `if (*(p+0xA29C)) return 0-arg;
else return *(p+0xA294)`. Both sides are tested: own-team (`0x14092B500/50D/512/514`:
if state==3 → index 0 anyway; else if `/100==6` → index 0; else opponent-state check)
and opponent (`0x14092B565/572/577`: if `/100==6` → apply the `min(arrow+1,4)` shift;
if not, keep). Side selection: `CMP R12D,2; JC` → `EDX = R12D^1` else `EDX = R14D`
(`0x14092B516` bytes). So `edx=6` is a per-team status code whose hundreds digit is 6.
Which team status that is (a competition/mode flag at `+0xA294`, zeroed when `+0xA29C`
is set) is INF — it lives in team setup, not the arrow path.

## 6. Form mirror (`+0x178`) reader — OPEN (no in-match reader found)

`FUN_140AF2DA0` writes `+0x178` = `FUN_1414C66C0(rec, 0x2D)` = `param_1[7] & 0x3F`
(dispatch case `0x2D`, CONFIRMED) and `+0x179` = the condition byte. Case `0x38` =
`param_1[7]>>0x16 & 7` (a second 3-bit view of the same word 7) and case `0x2E/0x2F` =
bytes 0/1 (CONFIRMED). The accessor-table index 97 thunk at `0x141F0FC60` is the Form
view (`C1 E8 16 83 E0 07`, byte-verified in the parent doc). No function in the
`0x1408xxxx–0x140Axxxx` match-engine range reads `+0x178` at match prep; the prep chain
reads only `+0x147` (band), `+0x146` (`MOVZX R12D,[RSI+0x146]; AND R12B,0x7F` at
`0x141FE4118`, stored to provider `+0x5D` at `0x141FE4240`), and the checksum words.
Verdict: Form reaches the 0x17C record but its in-match consumer (if any — possibly via
the `FUN_14150EC40` overall-rating input or UI-only) is unfound. OPEN.

## 7. Consumer + team shift — CONFIRMED (extends the parent doc)

`FUN_14092AF10`, `param_2` = record (`0x14092AF5F MOV [RSP+0x28],RDX`):
`0x14092B4D6 MOV EBX,[RAX+0x64]` (single plain-dword read); `0x14092B4D9 CALL 0x140A40BC0`;
`0x14092B4EF CMP EAX,3; JZ 0x14092B584` (own-team state==3 → `MOV EBX,R14D`, R14D=0);
own-team side path also requires `FUN_140A40BD0`→`FUN_140A602E0(...,6)` false and reads
the other side via `R12D^1`; opponent: `0x14092B536 CALL; 0x14092B53B CMP EAX,3; JZ shift`
then `0x14092B57B CMP EBX,4; JGE; 0x14092B580 INC EBX` (one step worse, cap 4);
`0x14092B595 MOV [RDI+0xC788],EBX`; skips `0x14092B59B CMP EBX,5; JGE` and
`0x14092B5A0 CMP EBX,2; JZ` (index 5+ and green ×1.0 write nothing). Table base
`0x142632D70` (row0 read: 1.12 1.06 1.00 0.92 0.84 — PES index 0=Top..4=Terrible);
index `LEA RAX,[R11+RCX*4]; ADD RCX,RAX` = `5·tier+arrow` with tier `MOVZX ECX,[RDX]`
/ stat-offset `MOVSXD R9,[RDX-4]` pairs walking `0x142632DB4` in 8-byte steps
(`(off,ability)` = (0,21),(0,22),(0,34),(0,35),(0,36),(0,37),(0,42),(0,23),(1,24),…).
Store `MOV [RDI+R9*4+0xC888],EAX`. All CONFIRMED at the bytes above.

## 8. Exact GF port spec

PES arrow index p: 0=Top … 4=Terrible; GF `Arrow`: 0=Terrible … 4=Top, so `g = 4 − p`.
Band→PES map `kInvMap = {4,3,2,1,0,2,2,2}`; band→GF map `{0,1,2,3,4,2,2,2}`.

1. `src/pes/playerdb.*`: add Form = `Field(rec, 438, 3)` (Player.bin id 0x2D, accessor
   `0x141F0FC60`, bit 438, 3 bits, raw 0..7; UI = raw+1 per matchlogic §1.2, fit 32/32)
   alongside 0x2E/0x0F — same `Field()` helper. Extend `PlayerTraits{…, int form}` (raw).
   No table transcription: read at runtime through the VFS (`build/apps/pesfmt_dump`,
   `src/pes/vfs*`).
2. `src/sim/data/formstate.*`: replace `ArrowForPlayer` splitmix draw with
   `pes = kInvMap[Band(condByte)]`,
   `Band(b) = b≤0x40?0 : b<0x46?1 : b<0x4B?2 : b<0x50?3 : b<0x55?4 : b<0x5A?5 : b<0x5F?6 : 7`
   (edges §0, `// ponytail:` naming `0x140AF2DA0`/`0x141FC1380` until inputs arrive through
   the VFS); GF-space: `band→{0,1,2,3,4,2,2,2}`. `condByte` input: the mixer output (§4).
   Until the edit-save `+0x147` import path lands, feed `condByte` from the persisted edit
   record, NOT from a kickoff RNG — a kickoff-time draw contradicts `FUN_141FE3830`'s
   RNG-clean prep (CONFIRMED negative). Keep any interim draw behind the same
   `// ponytail:` naming `0x140B159D8` (copier) and `0x141FE3973` (reader).
3. `Team::InitPlayers` + apply path (`match.cpp`): store per-player arrow; replicate the
   shift — own-team `+0xA298==3 → index 0` (predicate `0x140A40BC0`; setter `0x140A424D0`
   from `0x141FB9FB0`; `// ponytail:` until edit+0x20==3 semantics land), opponent
   `min(arrow+1,4)` (`0x14092B57B/580` `CMP EBX,4;JGE;INC EBX`); skip stat writes for
   index 2 and ≥5 (`0x14092B59B/5A0`); table base `0x142632D70`, index `5·tier+arrow`,
   note PES index = `4 − GF_Arrow` when reading it at runtime.
4. Keep GF `kArrowTable` (matches row0 re-verified here: 1.12/1.06/1.00/0.92/0.84 etc.).

## 9. Status ledger

- CONFIRMED: +0x147's only writer is the `FUN_140B158D0` struct copy (a carrier, not a
  roll); both +0x147 prep reads + `&7`; slot indexer math; band edges (§0; first edge
  `CMP 0x41 + JC`); inverse map + `+0x58` store; mixer determinism + no-Form-in-mixer;
  clamp bytes `0x28/0x6D`/default `0x46`; veto mechanism (`FUN_141FC1190` mode gate,
  `FUN_14149F4F0` table map, 4 callers); `+0xA298` predicate/setter/init-4/state-via-
  `+0x20` chain; consumer gates incl. `+0x64` read / `+0xC788` store / green+OOR skips /
  `inc` cap / side-xor / `edx=6` predicate wiring; no-table verdict; RNG mechanism bytes;
  row0 table floats; floats 90/100/2⁻³¹/45/315.
- INF: `/100==6` team-code meaning; Player.bin bit-438 → edit-word-7 blit site.
- OPEN: (1) edit-save → slot-array import that first fills `+0x147` (save/import path,
  not match engine); (2) edit `+0x20==3` menu meaning + mode-10 meaning; (3) `+0x178`
  Form-mirror in-match reader.
