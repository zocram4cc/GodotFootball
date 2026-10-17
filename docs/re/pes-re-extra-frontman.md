# PES 2021 Extra Frontman (style bit 15, engine OVERLAP): read-site decode

Scope: the six read sites named in the assignment (`FUN_14051B8B0` L106–108, `FUN_1405F63C0` L157–158, `FUN_1405F7D00` L56, `FUN_1405FC860`, the normaliser `FUN_141B620D0`, dt18 key 0x28), plus a read-only live probe of the running match. No engine edits, no live writes.

Tags: **CONFIRMED** = decompile or raw disassembly read in this pass, or a live read (named as such). **INF** = inferred from naming, shape or neighbouring docs. **OPEN** = blocked; the block is named.

Method: Ghidra bridge (`decompile_function`, `list_functions`); raw bytes of `pes_full.bin` via capstone (file offset = VA − 0x140000000); live reads through `/proc/2510857/mem` under `sudo -n` (read-only, `pread` only). Scratch: `<workdrive>/gf-FrontmanDecode-scratch/` (`dec/`, `scan/disp.py`, `scan/calls.py`, `scan/contain.py`, `live/probe.py`, `live/consts.py`, `live/watch1.log`).

---

## 0. Answers

| # | Item | Verdict |
|---|---|---|
| 1 | `FUN_14051B8B0` L106–108 store | CONFIRMED. Fills the CPU corner/set-piece designated-ordinal list; a bit-15 player is stored unconditionally (no score). The ranking after the store is decoded (§1.3). |
| 2 | `FUN_1405F63C0` L157–158, `FUN_1405F7D00` L56 | CONFIRMED gates. F63C0: CPU candidate list is bit-15-only. F7D00: CPU kinds 0–3 are bit-15-only. Neither branch changes a constant with bit 15; the +0.2 is only in FC860. Output consumers OPEN. |
| 3 | `FUN_1405FC860`, slot `+0x3ef8` | CONFIRMED mechanics (taker loop, +0.2, tie rule, fallback thresholds). Football meaning of the slot: INF. Full reader list in §3.3. |
| 4 | Normaliser `FUN_141B620D0`, mask `0x01000000` at `+0x4C` | CONFIRMED: the rule has **two** arms (`+0x4C` and `+0x5C`), not one. `+0x4C` bit 24 is not one of the seven COM cards (bits 0–6); its identity is OPEN. Why it clears bit 15: OPEN (only the code path is decoded). |
| 5 | dt18 key `0x28` has no caller | CONFIRMED (dead arm). 13 direct call sites rescanned; no absolute pointer to `FUN_1405C0F00` in the image. |
| 6 | Live: a bit-15 player and when `+0x3ef8` names him | **OPEN / blocked.** The match is running, but in 120 s of in-play samples no player on either side had bit 15, and `+0x3ef8` stayed at 15 (not an Extra Frontman). Nothing to record. Probe ready (§6). |

---

## 1. `FUN_14051B8B0` (candidate store and ordinal ranking), VA 0x14051B8B0

Caller: team tick `FUN_14051D9E0` (`0x14051D9E0`), at its two call sites `0x14051DD16` and `0x14051DDA0` (CONFIRMED by raw E8 scan). Set-piece placement, corner ordinals (doc `pes-re-setpiece-choreography.md` §11.6 and `pes-re-setpiece-zones.md` §3.2, OBS).

Names: `SP` = the set-piece/brain object passed as `param_1` (`SP[0]` = team index, `SP[4..14]` = ordinal list); `P` = `*(SP+0xBA8)` (`0x2EA*4`; `FUN_1405EC600` loads the same pointer as `[r15+0xBA8]`, CONFIRMED raw); `MI` = `param_2` (`MI+0x116C`, `MI+0x1558`).

### 1.1 The store (L106–108)

Path: `if (FUN_140A55CF0(...) == 0)` (L97–98), the non-sheet branch (L99–120), taken when `FUN_140A55CF0` returns 0. Per `pes-re-setpiece-choreography.md` §11.6 this is the **CPU** branch (INF: `FUN_140A55CF0` is the user-controlled-side test; the other branch reads the human team's sheet takers `FUN_1408C5060(T+0x8E08,k)`, L122–141, which has no bit-15 read).

Loop over the 11 local slots `uVar16` (global `uVar5 = 11·side + uVar16`):
- eligible: `MI+0x1748+uVar5 != 0` (L102);
- not in the instruction table at kind 8: `FUN_140A63950(SP+0xB98, side, 8, uVar5) == 0` (L103–104; `FUN_140A63950` tests `[ctx+0x18]+0x25CF8/+0x25D38`, a 4-entry (kind, id) table, CONFIRMED);
- **L106: `FUN_140A64030(uVar5, 0xF, …) != 0` → bit 15 set** (CONFIRMED gate, `pes-re-style-census.md` §0);
- L108–110: `SP[uVar23+4] = uVar16` (uVar23 starts at 10 and falls by one per store); `P+0x7C94[count] = uVar16`; L112: local-slot mask bit set; L113: `SP+0x3C = 1`; `count++`.

So a bit-15 player is written into the **top** free ordinal entries (10 downward), in slot order 0..10, with **no score and no kind test** at the store. Only `count` and the mask are updated.

### 1.2 What reads the store

- `SP[4..14]` and `P+0x7C94` (the ordinal list) are the corner/restart placement order (OBS, `pes-re-setpiece-choreography.md` §11.6: "ordinals filled from 10 downward … ordinal 0 = the keeper slot").
- Team tick `FUN_14051D9E0` after the call: `*(P+0x7C80) = SP[0x8A]` (L122/143 of the decompile; CONFIRMED read; the consumer of `P+0x7C80` is OPEN).

### 1.3 Ordering after the store (L146–331, CONFIRMED from decompile)

1. **Keeper chase** (L146–156): `FUN_140509890(...)` true → `SP[ordinal] = local_158` (the last kind-0 slot, §1.4), set `local_166 = 1`.
2. **Ranking score** per eligible slot (L206–256). Base `fVar24 = block+0x31 (height) + block+0x2C (jump) + block+0x1D (heading)` (CONFIRMED offsets; names from `pes-re-contact.md` §3.1 and `pes-re-matchlogic.md` §1.3). Then:
   - **designated slot** (`P+0x3EF8 == slot`, L225): score = `fVar24`, **no role penalty** (the designated slot is exempt);
   - **non-designated**: `fVar25 = fVar2` (−100) unless `local_166` (keeper chase) → `fVar24`; then if kind ∉ {0,1}: kind 2 or ≥10 → `0.0`; kinds 3–9 → `fVar24`;
   - **+1000** (`fVar1`) when kind > 5 (L247), for every slot including the designated one.

   Constants (live read, see §6.2): `0x14259D674` = −100.0; `0x14256AE34` = +1000.0.
3. Sort: `FUN_140A79640(&local_148, 1)` (L264; direction not decoded, INF: descending).
4. Fill (L269–303): walk the sorted list; take a slot if kind ≠ 0, not already in the mask, and `FUN_140A3C8F0(…)==0`; write `SP[count+4]`, `P+0x7C94`; stop when `count ≥ min(T+0x6D0, eligible)` (`T+0x6D0` = "men in box", `pes-re-setpiece-choreography.md` §11.6).
5. Remainder (L304–327): fill the rest from list `P+0x7D08` (count `P+0x7CDC`) downward.
6. Ordinal 0: if the keeper is not yet in the mask, `SP[4] = local_158`.

Kind meaning (INF): `kind 0` = keeper (local_158 is "the last slot with kind 0", L71–73, and step 6 writes it to ordinal 0). Other kinds are not named here (OPEN; the classes are `P+0x3FB8`, 22 ints).

### 1.4 Verdict

CONFIRMED: the bit-15 store is an unconditional, score-free inclusion of CPU Extra Frontman players into the top corner ordinals. INF (medium): these are the men sent into the box on CPU corners, placed before the score-based fill. Football description: "every eligible CPU Extra Frontman goes forward into the box at corners; the rest of the box is filled by height+jump+heading ranking, and the designated slot is exempt from role penalties."

---

## 2. `FUN_1405F63C0` L157–158 and `FUN_1405F7D00` L56

Callers (CONFIRMED raw E8):
- `FUN_1405F63C0` ← `FUN_1405F2280` (`0x1405F233E`). `FUN_1405F2280` picks a strategy wrapper from `brain+0xB8B4` (0 → F2A80, 1 → F2760, 2 → F2360, 3 → F32E0; CONFIRMED decompile) then always calls F63C0. `FUN_1405F2280` is called from `FUN_1405EFCC0` (`0x1405EFE00`).
- `FUN_1405F7D00` ← `FUN_1405F2760` (`0x1405F277B`, strategy 1) and `FUN_1405F2360` (`0x1405F238A`, strategy 2).
- The strategy family is the "tactics CPU strategy" of `pes-re-rng-slots.md` L129 (INF on the name).

### 2.1 F63C0 (`0x1405F63C0`)

- L147 `if (FUN_140A55CF0(…) == 0)`: the **CPU** branch (same test as §1; INF).
- **L157–158, the only bit-15 read in F63C0**: a candidate must satisfy `MI+0x1748 != 0`, `slot != MI+0x173C`, `slot != MI+0x1740` (restart takers), **`FUN_140A64030(slot,0xF) != 0` (bit 15 required)**, and `FUN_140A63950(…,8,slot) == 0` (not in the kind-8 table).
- Bit 15 **clear** → the slot is never a candidate in the CPU branch. Bit 15 **set** → candidate; score `fVar30 = FUN_140A6DF00(slot, ctx, 2)` (reach, mode 2), `+ fVar6` when `record+0x25 > 84` (L166–167; `fVar6 = DAT_1425A8A28` = 0.05, CONFIRMED live). No +0.2 here.
- Output: candidates are stored as (id, score) pairs in a local array and sorted (`FUN_140A79640`, L138). Consumer of the sorted array: OPEN.
- The human branch (L231 onward) reads the sheet takers; no bit-15 read there (grep: `0xF,` appears only at L157).

### 2.2 F7D00 (`0x1405F7D00`)

- Candidate gate, L44–47: `slot < 0x16`, eligible, `slot != FUN_140A627E0(team+0xF8,0)`, not `MI+0x173C/0x1740`.
- **L54–62**: kind `< 4` (`P+0x3FB8`) → **CPU** (`cVar7 == 0`): `FUN_140A64030(slot,0xF)` must be set (L56, the only bit-15 read); **human**: `FUN_140A63220(team+0xF8, slot)` (membership in takers 0..2). Kind ≥ 4 → not gated.
- Score: reach (`FUN_140A6DF00(slot,…,2)`, L64). Window: candidate must be within **0.1 m** of the running max (L65; `0x142588200` = 0.1, live). Accept if `|Δ| > 0.05` (L71; the `DAT_142565D30` mask is the sign mask `0x7FFF…`, live), else tie-break on `record+0x25` (strictly greater wins, L85–91).
- No +0.2 in F7D00. Returns the best slot (`0xFF` = none) to F2760/F2360.

### 2.3 Verdict

CONFIRMED: for the CPU team, bit 15 is a **required gate** in F63C0 (all candidates) and for kinds 0–3 in F7D00. Neither function gives Extra Frontman a bonus. Football meaning: INF: "on CPU attacking/defending set-piece plans, the only players who go up for the header are the Extra Frontman players." Consumers of the F63C0 list and the F7D00 return value: OPEN.

---

## 3. `FUN_1405FC860` (taker chooser, writes `P+0x3EF8`)

Caller (CONFIRMED raw): `FUN_1405EC600` at `0x1405ECBFE`, with `P = [r15+0xBA8]`. `FUN_1405EC600` is the defensive-shape/fore-check block of the team tick (`pes-re-team-plans.md`, `pes-re-defensive-shape.md`), called from `FUN_14051D9E0` (`0x14051DE03`, `0x14051E264`).

Args: `param_1` = a local set-piece ctx with `+0x10` = team T, `+0x28` = MI, `+0xE0`/`+0xF8` = team sub-objects; `param_2` = P.

### 3.1 Mechanics (CONFIRMED)

1. **Gate** (L26–32): `T+0xB8A2 == 0` (not full attack) → `P+0x3EF8 = 0xFF`, return. `MI+0x116C != 1` → `P+0x3EF8 = 0xFF`, then recompute.
2. **Recompute only if** `P+0x3EF8 > 0x15` (L35).
3. **Taker loop** (L37–91): the three sheet takers `FUN_1408C5060(T+0x8E08, k)` for k = 0, 1, 2 (`T+0x8E08+0x1454+4k`; roster id mapped to a slot by `FUN_140A3B000`). Each taker: eligible, not in kind-8 table. Score `FUN_140A6DF00(slot, team+0xF8, 2)` (reach). **+0.2 if bit 15** (L62–65, `0x142598578` = 0.2, live).
4. **Tie rule** (L67–86): replace the best if `|Δ| > 0.05` and higher; if `|Δ| ≤ 0.05`, the candidate wins only if `record+0x25` is strictly greater than the best's. (0.05 = `0x1425A8A28`, live; sign mask `0x142565D30`, live.)
5. **Fallback** (L92–170), only if no taker won: 11 slots, kind `P+0x3FB8` must be 1 or 2 (L107–112). For **non-bit-15** candidates (L113–130) skip if `P+0x7B5D < 2` **and** `record+0x39 < 0xAF` **and** `record+0x34 < 0x50` **and** `record+0x25 < 0x50`. Bit 15 exempts the candidate from that filter. Bit 15 also adds +0.2 (L132–135). Same tie rule as step 4 (L146–157).
6. **Write**: if a winner exists, `P+0x3EF8 = winner` (L93, L168). If none, the **previous value is kept** (no reset).

Reach formula (`FUN_140A6DE20`, reached by a thunk `FUN_140A6DF00` that reads `block+0x31` and `block+0x2C` and tail-calls with mode in `r8d`; raw CONFIRMED): `reach(H,J,mode) = 1.935 + 0.435·clamp((J−40)/59,0,1) + 0.4·clamp((H−150)/60,0,1) − 0.054343` for mode 2 (`pes-re-contact.md` §3.1, constants CONFIRMED there, not re-read here). The bridge decompile of `FUN_140A6DF00` (a 1-arg `void`) is wrong; use the raw disassembly.

### 3.2 Football reading

CONFIRMED: `P+0x3EF8` is the single **designated aerial attacker** of the team: the player who goes up for the header or attacks the box, picked by jump reach (height + jump). Extra Frontman adds 0.2 m to that reach, so it is worth one tie-break step or a near-tie win. A bit-15 player also skips the stat filter in the fallback.

INF: "a centre-back steps into midfield" cannot be confirmed from this function alone. In §1.3 the designated slot is exempt from role penalties, so a defensive-role player (kind 0–2) can be the designated attacker. Whether this is the "overlap" move the name suggests is OPEN.

### 3.3 Readers and writers of `P+0x3EF8` (CONFIRMED by raw disp32 scan, `scan/disp.py`; host by `scan/contain.py`)

Writers: `FUN_1405FC860` (0x1405FCA9D, 0x1405FCD3E); `FUN_142156E80` (0x142157BEB, L575: `P+0x3EF8 = record[2]` if the record's kind is 2 or 3, else 0; the same pattern writes `P+0x3ECC/0x3ED0/0x3EF4`, so it is the plan builder's designated-slot list (INF)).

Readers (behaviour summary; not all decoded):
- `FUN_14051B8B0` (0x14051BD3D): designated exempt from role penalty (§1.3). CONFIRMED.
- `FUN_1405FC500` (Target Man candidate, 0x1405FC761, L129): `slot == designated || kind > 3` (CONFIRMED).
- `FUN_1405F8880` (0x1405F8965, L30): `kind > 3 || slot == designated` (CONFIRMED). Same pattern as F7D00's kind gate.
- `FUN_1405F5FE0` (0x1405F605A–F6326, 9 reads): `slot == team*11 + k` for k = 0..7 (positive designated test).
- `FUN_140680980` (throw-in, 20 reads, L409–595): `slot != team*11 + k` exclusions.
- `FUN_1405E6E80` (off-ball run scorer, 6 reads, 0x1405E8162–0x1405E97AF): `!= slot` exclusions.
- `FUN_1405DC350` (0x1405DC651, 0x1405DC897): skip if equal to the designated slot (`P+0x40C` or `uVar39`).
- `FUN_14067C130` (0x14067C28C–3A6, L59–97): writes a per-slot value into `param_3[(3EF8 % 11)·3]`.
- Others (behaviour not decoded): `FUN_1405CE150`, `FUN_1405CE4F0`, `FUN_1405E0180`, `FUN_1405E9F50` (0x1405EA4F3), `FUN_1405F3840`, `FUN_14067E6C0`, `FUN_140A41CC0`, `FUN_14214E960`, `FUN_1405F8880`.

Verdict: `P+0x3EF8` is read by about 60 sites across set-piece, off-ball and defensive code. It is used as an "is this the designated man" gate in each.

---

## 4. Normaliser `FUN_141B620D0` (`0x141B620D0`) and mask `0x01000000` at `block+0x4C`

### 4.1 Code (CONFIRMED, decompile L22–78)

- `param_1` = `block` = in-match block (`copy+0x70`; `FUN_14124DAD0` = +0x70). Style dword `block+0x48` = copy `+0xB8`; COM dword `block+0x4C` = copy `+0xBC`; `block+0x5C` = copy `+0xCC` (CONFIRMED offsets).
- It refreshes a 256-bit card array from a 256-byte source (vtable +0x28 read of the object at `param_1+0x20`), writing dwords `+0x48 … +0x64` bit by bit (L58–69). Bit `i` of the array = `byte[i] >> 7`. So array bit 32+k = `+0x4C` bit k, and array bit 160+k = `+0x5C` bit k.
- **Clear rule (L70–74)**: if `(block+0x48 >> 15) & 1` **and** (`(block+0x4C & 0x01000000) != 0` **or** `(block+0x5C & 0x01000000) != 0`) → `block+0x48 &= 0xFFFF7FFF`.
- The census (`pes-re-style-census.md` §3, §5, `pes-re-style-effects.md` §6.8) describes only the `+0x4C` arm. **Correction**: there are two arms.
- Dispatch: no direct E8 call site; the function pointer is in a table at `0x142AC8450` (qword pointer scan), next to `FUN_141B59E20` at `0x142AC8440` (CONFIRMED pointer; the table's dispatcher is OPEN).

### 4.2 Which COM card is `0x01000000` at `+0x4C`

- `0x01000000` at `+0x4C` = bit 24 of the COM dword = **array (card) index 56**.
- The COM table `0x14352AC10` (live read, 12-byte rows) has seven valid rows: `(bit, field, UI) = (0,60,0), (1,62,2), (2,61,1), (3,65,5), (4,63,3), (5,66,6), (6,64,4)`, so only bits 0–6 of `+0x4C` are COM cards (`pes-re-com-styles.md` §2.3, CONFIRMED). Rows 7–8 are zero (`(0,0,0)`, `(0,0,37)`); the loop bound is INF (7).
- **Verdict: bit 24 is not one of the seven COM cards.** Its identity is OPEN. The field behind array index 56 is not decoded (the source object at `param_1+0x20` is not traced).
- Same for `0x01000000` at `+0x5C`: array index 184, not in the COM table, identity OPEN.

### 4.3 Why it clears Extra Frontman

CONFIRMED: Extra Frontman (bit 15) is removed whenever card 56 **or** card 184 is set.
OPEN: the game-design reason (mutually exclusive card, COM override, or something else). Not decodable from these functions alone.

### 4.4 Live evidence (§6)

- `+0x4C` bit 24 (card 56): never set in any in-play sample (0 of 22 players, every sample).
- `+0x5C` bit 24 (card 184): set on 14–21 of 22 players and it toggles between samples (e.g. players 0 and 11 at one sample, not the next). So card 184 is not a static card attribute in this match. INF: it is the state-dependent source.
- Bit 15 was clear in every sample for every player, so the clear path was never exercised live.

---

## 5. dt18 key `0x28`

- `FUN_1405C0F00` switch: key `0x28` → `MOV EDX,0xF` → tail (bit 15). CONFIRMED (`pes-re-style-census.md` §1.2).
- Direct call sites: rescanned all `E8 rel32` into `0x1405C0F00` (`scan/calls.py`): 13 sites, keys 0x3A (×2), 0x1F (×3), 0x1E (×3), 0x1D (×2), 0x21 (×2), 0x2F (×1). **No `r8d = 0x28`**, and no `lea r8d,[…+0x28]`.
- No 8-byte absolute pointer to `0x1405C0F00` in the image (qword scan).
- **Verdict: CONFIRMED dead arm.** Scope: direct calls and absolute pointers. An indirect call through a computed address is not excluded, but none was found.

---

## 6. Live (read-only)

### 6.1 Chain used (CONFIRMED; the values below were read live)

- `G = [0x1436F3DC8]`; `MI = [G+0x270]`; `MI+0x116C` (1 = in play, 2 and 3 seen).
- `T_s = [G+0x1928+0x28s]`; posture bytes `T+0xB8A2` (full attack), `T+0xB8A3` (full defence), `T+0xB8A4` (gauge).
- `H = [0x1436F3F98]`; `brain_s = [H+0x450+8s]` (`pes-re-teamtick.md` L45); `P = [brain+0xBA8]`; `slot = [P+0x3EF8]`.
- Player `i` (0..21): `copy = [G+(i+0xF3)·0x28]`; style `copy+0xB8`; COM `copy+0xBC`; `copy+0xCC`.
- Process `PES2021.exe` base `0x140000000` (`/proc/2510857/maps`).

### 6.2 Live constants (read from `/proc/2510857/mem`, `live/consts.py`)

| VA | Value | Use |
|---|---|---|
| `0x142598578` | 0.2 | FC860 +0.2 |
| `0x1425A8A28` | 0.05 | tie window; F63C0 +0.05 |
| `0x142565D30` | `0x7FFFFFFFFFFFFFFF` | sign mask (|Δ| test) |
| `0x142588200` | 0.1 | F7D00 window |
| `0x14259D674` | −100.0 | ranking penalty, kind 0/1 |
| `0x14256AE34` | 1000.0 | ranking bonus, kind > 5 |
| `0x14256AE1C` | 1.0 | (not used here) |
| `0x14352AC10` | rows 0–6 = bits 0–6 | COM table |

### 6.3 Watch (`live/probe.py watch 120`, log `live/watch1.log`)

- 120 s, in-play samples only. `MI+0x116C` took values 1, 2 and 3 (play, dead ball, set piece).
- Posture: `b8a2 = 0`, `b8a3 = 0`, gauge = 2 for both teams throughout. Neither side in full attack.
- `slot3ef8 = 15` throughout, `P = 0x37228968` for both sides (INF: one shared object).
- Bit 15: **no player** in any sample (`bit15_players = []`). The match has no Extra Frontman player in the copy records I can read.
- Because `b8a2 = 0` and `slot3ef8` never changed, FC860's early-out (which writes 0xFF) did not run for this `P` in this window. INF: FC860 is gated by a condition not decoded here, or the `+0x10` team pointer in its ctx is not the same `T`.

### 6.4 Item 6 verdict

**OPEN / blocked.** A match is running, so the probe works. But:
(a) no player has bit 15 in the running match; and
(b) the +0.2 path needs `T+0xB8A2 == 1` (full attack, gauge ≥ 4), which did not occur in 120 s.

To close item 6: wait for a match where a team has an Extra Frontman player and reaches full attack, then `cd <workdrive>/gf-FrontmanDecode-scratch/live && sudo -n python3 probe.py watch 600`. It is read-only and logs when `slot3ef8` changes, with the posture and bit-15 list. I did not send input or start a match.

---

## 7. Corrections to existing docs

1. `pes-re-style-census.md` §3, §5 and `pes-re-style-effects.md` §6.8: the normaliser's clear rule has two arms (`+0x4C` and `+0x5C`, bit 24 each), not one (§4.1).
2. `pes-re-style-census.md` §3 says bit-15 gate sites are OPEN. They are now decoded (§1–§3 here).
3. `pes-re-style-effects.md` §2 / §6: `FUN_140A6DF00` = reach (correct). The bridge's 1-arg decompile is misleading; the raw code is a thunk that reads `block+0x31`/`+0x2C` and tail-calls `FUN_140A6DE20` with mode in `r8d`.
4. `pes-re-style-census.md` §1.2, dt18 key 0x28: confirmed no caller (§5).

---

## 8. Open items

1. Identity of COM-dword bit 24 (`+0x4C`, card 56) and bit 24 of `+0x5C` (card 184): the field behind source index 56 and 184 (source object at `normaliser+0x20`, not traced).
2. Game-design reason for the Extra Frontman clear (§4.3).
3. Kind classes `P+0x3FB8` beyond kind 0 = keeper (INF) and 1 (−100 penalty): names not decoded.
4. `FUN_140A79640` sort direction (INF: descending).
5. Output consumers of the F63C0 candidate list and the F7D00 return value.
6. Who writes `P+0x3EF8` each tick in the live match (value 15 observed with `T.b8a2 = 0`; `FUN_142156E80` is the plan writer, INF).
7. `P+0x7C80` consumer (`FUN_14051D9E0` writes it from `SP[0x8A]`).
8. Item 6 (§6.4).

---

## Scratch and tools (all read-only)

- `scan/disp.py <disp>`: capstone scan for instructions with a given memory displacement in `pes_full.bin`.
- `scan/calls.py <VA>`: rel32 E8/E9 call sites into a VA.
- `scan/contain.py <VA>…`: containing function from the bridge's `list_functions` (nearest entry).
- `live/probe.py snap|watch SECONDS`: live reads through `/proc/<pid>/mem` (`sudo -n`), never writes.
- `live/consts.py`: live constant and COM-table reads.
- `live/watch1.log`: the 120-s watch.
