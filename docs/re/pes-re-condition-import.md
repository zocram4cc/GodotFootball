# PES condition import: how the band byte `editrecord+0x147` gets filled (ConditionImportRE, 23-10-26)

Parents: `docs/re/pes-re-condition-writer.md` (writer chain) +
`docs/re/pes-re-condition-bands.md` (band edges, prep reads, RNG) +
`docs/re/pes-re-condition-roll.md` (copier verdict, veto, mixer, no-Form proof).
This file closes the last link the ticket named: roster/save load into `+0x147`.

Image: `~/ghidra-projects/scratch/Unpacker/pes_full.bin`, file offset = VA −
0x140000000. Bridge one request at a time; floats/instructions script-read.
Checker: `<workdrive>/gf-ConditionImportRE-scratch/check_condition_import.py` —
41 byte/table/data asserts, GREEN 23-10-26; mutation run (0x46→0x47 at
0x140AF2EB1) goes RED on `band.lt46` only, then reverted.

## 0. Script-read constants (pasted tool output, owner ruling 19-10-26)

```
0x140B159D8 888347010000 (MOV [RBX+0x147],AL — the roster-load copier)
0x1404163FF 888747010000 (MOV [RDI+0x147],AL in FUN_1404161E0 — carrier #2, new)
0x14074A94F 888147010000 (MOV [RCX+0x147],AL in FUN_14074A6B0 — carrier #3, new)
0x1418E2246 888547010000 (MOV [RBP+0x147],AL, EAX=0 init-zero block)
0x14215DA70 41888647010000 (MOV [R14+0x147],R8B — JSON/option-field filler)
0x141FE3973 440fb68047010000 (MOVZX R8D,[RAX+0x147], short prep path)
0x141FE397B 4183e007 (AND R8D,7)    0x141FE3984 CALL 0x141FC1380
0x141FE4100 440fb68647010000 (MOVZX R8D,[RSI+0x147], long/gameplan path)
0x141FE4108 4183e007              0x141FE4113 CALL 0x141FC1380
0x141FC1386 bb02000000 (MOV EBX,2)  0x141FC13E1 bb04000000 (band 0 -> 4)
0x141FC138D(bb03000000 band 1 -> 3) 0x141FC13B9(bb01000000 band 3 -> 1)
0x140AF2EAC 83f841 (CMP EAX,0x41)  0x140AF2EB1 83f846  0x140AF2EBD 83f84b
0x140AF2EC9 83f850  0x140AF2ED5 83f855  0x140AF2EE1 83f85a  0x140AF2EED 83f85f
0x141F0FC60 85d2750a 8b4134 c1e816 83e007 (TEST; JNE; MOV EAX,[RCX+0x34]; SHR EAX,0x16; AND EAX,7)
0x143529930 table entry 32 = {id 0x2D, fn 0x141F0FC60, 0} (Form accessor slot)
0x142B47AD0 id table: 0C 09 0B 0A 05 06 08 07 04 03 02 01 00 (no 0x2D/Form)
0x142B47B10 WROW: 14..36 (23 weight rows)
```

## 1. Which functions, in what order, from which source — CONFIRMED

The `+0x147` byte has **no computing writer anywhere in executable code**.
A precise opcode-level scan (`88 /r, mod=10, disp 0x147`, REX-aware) finds
exactly six code sites image-wide (data-region hits above 0x144000000 excluded):

| VA | bytes | function | role |
|---|---|---|---|
| `0x140B159D8` | `88 83 47 01 00 00` | `FUN_140B158D0` | 0x690-record struct copy (roster/load path) |
| `0x1404163FF` | `88 87 47 01 00 00` | `FUN_1404161E0` | struct copy (menu/team-data path), NEW this session |
| `0x14074A94F` | `88 81 47 01 00 00` | `FUN_14074A6B0` | struct copy (menu path), NEW this session |
| `0x1418E2246` | `88 85 47 01 00 00` | init block | EAX=0 zero-fill, not a value |
| `0x14215DA70` | `41 88 86 47 01 00 00` | `FUN_14215DA70` region | JSON/option-field filler, not an edit record |

So the byte **arrives with the record** and is carried verbatim by the
`FUN_140B158D0`-family copiers. The roster-load chain that fills it
(all CONFIRMED at the bytes/decompile this session):

1. **Install + save → edit records.** Install `Player.bin` (312 B records;
   stock = `Data/dt10_x64.cpk` WESYS blob 14, 5,547,984 B = 17,782 players;
   VFS winner on this install = 4cc `download/4cc_01_base.cpk`, 4,830
   placeholder records) is overridden per-id by the edit save
   `EDIT00000000` (same 312 B layout; decrypted with GF's own
   `src/pes/editsave.cpp` cipher — desc/logo/serial SHAs match, data-block
   digest is stale and kept, exactly as the game does). Proof of override:
   same ids (70101..), winner Form = 3 for all 4,830 vs save Form varied
   (§4). The save is therefore **the** source for every 4cc-league player.
2. **Edit-data build** (`FUN_140AF22F0` dispatcher → `FUN_140AF1400`
   0x17C-array loop + `FUN_140AF0E50` checksum-gated single build →
   `FUN_140AF2DA0` per player): cond byte =
   `FUN_1414E2120(rec, 0xD, 2)` — `0x46` default when the record key matches
   the install tag (`DAT_14351AE30/34`) or id == 0, else mixer
   `FUN_141E5B540` over abilities, clamped `0x28..0x6D` (bytes
   `41 80 F8 28 / 41 B0 28 / B0 46` re-verified); band = 7-edge ladder
   (§0) → field `0x30` (`param_1[6]` bits 28–31); `+0x179` = cond byte;
   `+0x178` = field `0x2D` (`param_1[7] & 0x3F`); overall
   `FUN_14150EC40` → `+0x13C` bits; `FUN_140AF06D0` copies the 0x17C record
   into the sorted edit array (binary-searched by `FUN_1414B80F0`).
3. **Roster load** (`FUN_1413FAC60`, called from the team-setup state
   machines `FUN_140ACBC70`/`FUN_140BEAD80`): `FUN_140B158D0(editctx+…,
   teamrec)` header copy, then per squad player `FUN_140AF06D0(roster+…,
   1414BC9A0(editctx, playerkey))` 0x17C copies. This is the ticket's
   "caller used at roster load".
4. **Match prep** (`FUN_141FE3830`, two paths): short path
   (`0x141FE3973`) and long/gameplan path (`0x141FE4100`) both do
   `[slot+0x147] & 7 → FUN_141FC1380` (inverse map `0→4,1→3,2→2,3→1,4→0,
   else 2) → provider `+0x58`. Zero RNG calls on either path (re-CONFIRMED).
   The gameplan screen reads the same byte for the icon
   (`FUN_1404DFFB0`: `(rec+0x144)>>0x18 & 7` = byte `+0x147`, 5 icons +
   bit-23 variant via `FUN_1408C7710`-family stores).

OPEN (narrowed): which store first puts the band value into the 0x188 slot
record's `+0x147` — formation-screen assignment or the save-import fill
(`FUN_1413FCxxx` 0x690 loops / `FUN_141FC5500` fill sites `0x141FC55CB/823`
via `FUN_140A81830` + sort `FUN_141FC4B30`). Not the match engine.

## 2. Weekly Form changes — answered: NO writer in this install (CONFIRMED)

- `PlayerWeekly.bin`: **size 0** in the VFS winner (no records at all).
- `TeamWeekly.bin`: 629 `(team, value)` pairs, **every value = 5**
  (script-read; `data.teamweekly.all5` assert). Neutral-constant, no signal.
- The cond path is deterministic (mixer, no RNG; prep RNG-clean), and Form
  provably does not enter the mixer (§3).
- Verdict: **exhibition arrows are fixed per data file** (install, overridden
  by the edit save where present). No weekly/mode-specific Form writer was
  found in the traced paths; a live-service weekly path may exist for online
  modes but leaves no trace in this install's data or the match-prep chain.
  Status: CONFIRMED for exhibition; online-weekly existence stays OPEN
  (low priority — out of the port's scope).

## 3. Form does NOT feed the condition byte — correction (CONFIRMED)

The ticket's presumed chain (Form → condition byte → ladder) is wrong at
step 2, proven three independent ways this session:

1. The mixer's input-id table `0x142B47AD0` (script-read, 13 ids
   `0C 09 0B 0A 05 06 08 07 04 03 02 01 00`) contains **no 0x2D/Form**
   (`mix.idt.no2D` assert).
2. The only in-match Form-accessor consumer found is the generic UI/data
   getter `FUN_141EEDAD0` (via table dispatch `FUN_141F10670`, single caller
   `0x141EEE978`) — no match-engine reader.
3. The prior docs' "`+0x178` Form mirror" label is wrong for on-disk layout:
   across all 4,830 save records, word-7 low 6 bits (field `0x2D`) differ
   from bit-438 Form in **4830/4830** cases (`data.save.word7.notform`).
   (`+0x178` may still mirror Form in the *runtime* 0x17C record if the
   blit puts it there — the blit itself is part of the OPEN item in §1.)

Form's only confirmed runtime sinks: `+0x178` (whatever the blit stores),
the gameplan/provider byte `+0x30` (`FUN_141FE9D00` writes field `0x2D`
there), and UI display (UI = raw + 1, matchlogic §1.2 fit 32/32).

## 4. Worked example — teams 701/702 (first two install teams), CONFIRMED data

Form = `Field(rec, 438, 3)`, raw 0..7 (UI = raw + 1). The game loads the
**save** values (same ids as the 4cc install file, which carries only
placeholders with Form = 3):

| team | pid | save Form (UI) | winner Form | cond→arrow (decoded chain) |
|---|---|---|---|---|
| 701 | 70101 | 5 (6) | 3 | cond `0x46` (default path) → band 2 → PES Normal / GF Normal |
| 701 | 70102 | 1 (2) | 3 | same chain (Form does not move cond) |
| 701 | 70103 | 0 (1) | 3 | same chain |
| 701 | 70104 | 5 (6) | 3 | same chain |
| 701 | 70105 | 1 (2) | 3 | same chain |
| 701 | 70106 | 5 (6) | 3 | same chain |
| 702 | 70201 | 5 (6) | 3 | same chain |
| 702 | 70202 | 1 (2) | 3 | same chain |
| 702 | 70203 | 5 (6) | 3 | same chain |
| 702 | 70204 | 5 (6) | 3 | same chain |
| 702 | 70205 | 1 (2) | 3 | same chain |
| 702 | 70206 | 1 (2) | 3 | same chain |

Distributions behind it: stock Form (17,782) `{0:1, 1:79, 2:931, 3:7606,
4:6000, 5:2559, 6:512, 7:94}` (peak 3–4); save Form (4,830)
`{0:61, 1:3660, 3:1, 4:105, 5:1003}` (editors set mostly raw 1). Both
script-read (`data.stock.formdistro`, `data.save.formdistro`).

Full cond→band→arrow table (ladder §0 + inverse map; GF index = 4 − PES):

| cond | band | PES arrow | GF arrow |
|---|---|---|---|
| `0x28..0x40` | 0 | Terrible | Terrible |
| `0x41..0x45` | 1 | Low | Poor |
| `0x46..0x4A` | 2 | Normal | Normal |
| `0x4B..0x4F` | 3 | High | Great |
| `0x50..0x54` | 4 | Top | Top |
| `0x55..0x6D` | 5/6/7 | Normal | Normal |

The `0x46` default (unedited-record path) lands band 2 → Normal on both
sides; the consumer then applies the team shift (own-team `+0xA298==3` →
index 0; opponent `min(arrow+1,4)`, `0x14092B57B CMP EBX,4 / 0x14092B580
INC EBX`).

## 5. Port spec (GF terms)

1. `src/pes/playerdb.*`: add `Form = Field(rec, 438, 3)` (Player.bin id
   `0x2D`, accessor `0x141F0FC60`, bit 438, 3 bits, raw 0..7; UI = raw + 1
   per matchlogic §1.2, fit 32/32) alongside `0x2E/0x0F` — same `Field()`
   helper. Extend `PlayerTraits{…, int form}` (raw). Read at runtime through
   the VFS (`build/apps/pesfmt_dump`, `src/pes/vfs*`); base = install
   `Player.bin`, override = edit-save records by id (the save wins —
   demonstrated §1). No table transcription.
2. `src/sim/data/formstate.*`: replace `ArrowForPlayer` splitmix draw with
   `pes = kInvMap[Band(condByte)]`, `Band(b) = b≤0x40?0 : b<0x46?1 :
   b<0x4B?2 : b<0x50?3 : b<0x55?4 : b<0x5A?5 : b<0x5F?6 : 7` (edges §0),
   `kInvMap={4,3,2,1,0,2,2,2}`; GF-space `{0,1,2,3,4,2,2,2}`. `condByte` =
   the persisted record's byte (`0x46` default path vs mixer output, §1),
   **never a kickoff RNG** — `FUN_141FE3830` is RNG-clean (CONFIRMED
   negative). Keep any interim draw behind `// ponytail:` naming
   `0x140B159D8` (copier) and `0x141FE3973` (reader).
3. `Team::InitPlayers` + apply path (`match.cpp`): store per-player arrow;
   replicate the shift — own-team `+0xA298==3 → index 0` (predicate
   `0x140A40BC0`; setter `0x140A424D0` from `0x141FB9FB0`; `// ponytail:`
   until edit+0x20==3 semantics land), opponent `min(arrow+1,4)`
   (`0x14092B57B/580`); skip stat writes for index 2 and ≥5
   (`0x14092B59B/5A0`); table base `0x142632D70`, index `5·tier+arrow`,
   PES index = `4 − GF_Arrow`.
4. Keep GF `kArrowTable` (row0 re-verified: 1.12/1.06/1.00/0.92/0.84).
5. Do NOT wire Form into the arrow (no such path exists). Form feeds UI
   display and (pending the §1 blit) `+0x178`/provider `+0x30` only.

## 6. Status ledger

- CONFIRMED: six code sites storing disp `+0x147` (2 copiers new:
  `0x1404161E0`, `0x14074A6B0`); no computing writer exists; full roster-load
  chain (`140AF22F0→140AF1400/140AF0E50→140AF2DA0→140AF06D0`,
  `1413FAC60` via `140B158D0` + per-player copies, `141FE3830` both read
  paths + `&7`, gameplan icon read of the same byte); install-vs-save
  override (same ids, placeholder vs edited Form); `PlayerWeekly` empty +
  `TeamWeekly` all-5s (no weekly writer); stock + save Form distros;
  `BallCondition.bin` shape (225 × 4-u16 records — per-ball data, not player
  condition); assignment table (team 701 = 23 players from 70101);
  save-decrypt round-trip (3/4 block SHAs match, stale data-digest kept as
  the game does); Form∉mixer (3 proofs); word7≠Form on disk (4830/4830).
- INF: `/100==6` team-code meaning (unchanged); `DAT_14351AE30/34`
  install-tag semantics (readers everywhere, writer not yet traced —
  peripheral).
- OPEN: (1) the first store of the band value into the 0x188 slot record's
  `+0x147` (formation assignment vs save-import fill — narrowed to
  `1413FCxxx` 0x690 loops / `141FC5500` fill sites); (2) the
  Player.bin→0x17C word-7 blit (whether runtime `+0x178` carries Form);
  (3) edit `+0x20==3` menu meaning + mode-10 meaning (unchanged);
  (4) online-weekly existence (no trace in this install).
