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
| 6 | Live: a bit-15 player and when `+0x3ef8` names him | **OPEN.** 17-10-26 experiment (§6.4): bit 15 on one CB was reverted by the game within ≈0–3 s every time, so no player held bit 15 long enough to matter. No full attack in ~7 min, so `+0x3ef8` stayed 0 (not 15; the earlier 15 was a wrong pointer, §6.1). Blockers and next steps in §6.5. |

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

Kind meaning (INF): `kind 0` = keeper (local_158 is "the last slot with kind 0", L71–73, and step 6 writes it to ordinal 0). Kinds 1 = CB, 2 = FB per FC860's `{1,2}` test and the role-writer §5 class map; higher kinds are the same class map (P aliases S at init). _Corrected 19-10-26 from pes-re-role-writer.md §6: old "Other kinds are not named here (OPEN; the classes are P+0x3FB8, 22 ints)"._

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
3. **Taker loop** (L37–91): the three sheet takers `FUN_1408C5060(T+0x8E08, k)` for k = 0, 1, 2 (`T+0x8E08+0x1454+4k`; roster id mapped to a slot by `FUN_140A3B000`). Each taker: eligible, not in kind-8 table. Score `FUN_140A6DF00(slot, team+0xF8, 2)` (reach). **+0.2 if the candidate holds style bit 15 (`+0x48`) and bit 15 of `DAT_142613D20[B[slot]]`, set only for class 1 (CB)** (L62–65, `0x142598578` = 0.2, live; gate via `FUN_140A64030(slot,0xF,…)` → `FUN_1408CB0B0`). _Corrected 19-10-26 from pes-re-community-qa.md §6 C1: dec FUN_1405FC860/FUN_140A64030/FUN_1408CB0B0 + table decode; old reading "+0.2 if bit 15"._
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

## 6. Live (read-only probes; one-word write experiment, owner ruling 17-10-26)

### 6.1 Chain (corrected 17-10-26; supersedes the first §6.1)

+ `G = [0x1436F3DC8]`; `MI = [G+0x270]`; `MI+0x116C` (1 play, 2 dead ball, 3 set piece). `MI+0x1168/0x1574` never touched.
+ Team posture `T_s = [G+0x1928+0x28s]`: `T+0xB8A2` full attack, `+0xB8A3` full defence, `+0xB8A4` gauge (CONFIRMED live).
+ Roster `P_s = [G+0x1978+0x28s]`. Local slot `k` (0..10) → roster record `r = P_s[4+4k]`. Gate word (the word `FUN_1408CB0B0` reads through `FUN_140A3B110`) = `P_s+0xE0+0x48+r·0x128` (CONFIRMED raw; the candidate store and F63C0/F7D00 gates use this roster block, via `FUN_140A64030`).
+ Live copy of global player `i`: `copy = [G+(i+0xF3)·0x28]`; `copy+0xB8` equalled the gate word in every side-0 sample (11/11).
+ Set-piece/team-data object: `SP_s = [H+0x450+8s]+8` (`H = [0x1436F3F98]`; `SP_s+0` = side int, read 0 and 1), `TD_s = [SP_s+0xBA8]`. `TD+0x3EF8` = designated slot (`P+0x3EF8` in §3), `TD+0x7C80/0x7C94/0x7CDC` (count and list, mostly zero).
+ Ordinal list `SP_s[5..14]` (int index into `SP`, i.e. `SP+0x14..+0x38`; entries are local slots, `0xFF` = empty). `SP[4]` is a frame counter, not a list entry.
+ **Correction:** the first §6.1 read `P = [brain+0xBA8]` with `brain = [H+0x450+8s]`. That is the wrong base (it gave one shared `0x37228968`). The `slot3ef8 = 15` in the first §6.3 is therefore invalid. The correct value is `TD+0x3EF8 = 0` for both sides.

### 6.2 Live constants (unchanged; read from `/proc/2510857/mem`)

| VA | Value | Use |
|---|---|---|
| `0x142598578` | 0.2 | FC860 +0.2 |
| `0x1425A8A28` | 0.05 | tie window; F63C0 +0.05 |
| `0x142565D30` | `0x7FFFFFFFFFFFFFFF` | sign mask (abs test) |
| `0x142588200` | 0.1 | F7D00 window |
| `0x14259D674` | −100.0 | ranking penalty, kind 0/1 |
| `0x14256AE34` | 1000.0 | ranking bonus, kind > 5 |
| `0x14352AC10` | rows 0–6 = bits 0–6 | COM table |

### 6.3 Baseline, no writes (`live/watch_baseline.log`, 200 s, 1042 samples)

+ `MI+0x116C` took 1, 2 and 3 (95 samples at 3). Both teams: `b8a2 = 0`, `b8a3 = 0`, gauge = 2 for the whole window: **no full attack**.
+ `TD+0x3EF8 = 0` on both sides throughout. Not `0xFF`: `FUN_1405FC860` writes `0xFF` when `b8a2 == 0`, so it did not run in this window (its caller `FUN_1405EC600` path was not reached).
+ Ordinal list `SP_0[5..14] = (1,0,255,255,1,7,3,4,5,10)`, `SP_1[5..14] = (1,0,255,255,2,7,3,4,10,5)`. Constant across all 200 s (no placement rewrote it). Slot 1 (our CB, side 0) is already in the side-0 list with no bit-15 write.
+ `TD+0x7C94` holds mostly zeros and a float `-1.0` at index 5. It is not an ordinal list in this layout (OPEN).
+ `brain+0xB8B4` reads 0 for side 0 and a float (`1.0`) for side 1, so it is not the strategy mode for both sides (OPEN). `FUN_1405F7D00` is reached only for strategies 1 and 2, so its gate is unobservable unless the mode is read correctly.

### 6.4 The write experiment (one roster word + one copy word; bit 15)

+ Target: CB, side 0, local slot 1 (global `i=1`, roster `r=14`, formation role 1 = CB, compat row `0xD000` contains bit 15). Old value `0x1000` (bit 12, ENFORCER). Gate word `0x1D47B830`; copy word `0x1D4A3160` (`copy = 0x1D4A30A8`). Writes went to `0x9000` (bit 15 OR'd in). Log: `live/exp_writes.log`.
+ Every write logged with address, old and new value. 7 write events in total (`11:17:33`, `11:21:10`, `11:25:31`, `11:26:41`, and 3 revert probes at `11:28:31/33/35`), 14 `WRITE` lines and 14 `RESTORE` lines. Final readback: `W = 0x1000`, `C = 0x1000`.
+ **Observation 1 (watch, `live/watch_bit15.log`, write at ≈t=12 s, 230 s window):** the gate word read `0x9000` from t=14.67 s to 16.91 s; the copy read `0x9000` (and once `0x1000`) to ≈16.9 s; both were back to `0x1000` by t=17.6 s, at the `MI+0x116C` 2→3 transition. I had not restored yet. The revert was made by the game, not by me.
+ **Observation 2 (`live/mech_mech2.log`, 2 ms logger, write at t≈7 s):** the logger never sampled `0x9000` in 60 s. The word was reverted before the first sample after the write.
+ **Observation 3 (`live/revert_probe.py`, 3 writes):** the readback immediately after the write already gave `0x1000` for both words, and the first 1 ms sample was `0x1000` (revert at ≈1.1 ms).
+ Revert latency therefore ranged from ≈0 ms to ≈2.7 s across writes, and is not explained by `MI+0x116C` alone (set-piece and play samples both show reverts).
+ Ruled out as the reverter: `FUN_141B620D0` (the normaliser). Its cache word at `block+0x10` (`W-0x38`) stays `0xA07F0000` while the version global `DAT_1437F0AF4` (`FUN_141B5B9C0`) ticks every ~18 ms, so no refresh runs. Its source pointer `block+0x20` (`W-0x28`) is `0x4D014D4D4D4D4D4D`, not an object pointer, so this word is not that normaliser's block. **Reverter: OPEN.**
+ Effect on the item-6 observables during the held windows: `SP` ordinal list unchanged; `TD+0x3EF8 = 0`; `b8a2 = 0` (no full attack, so `FUN_1405FC860`, the only writer of the +0.2 and of the designated slot, did not run). **No change versus baseline was measured.** The CB did not appear in any new list entry.

### 6.5 Item 6 verdict

**OPEN (not closed).** Two blockers:
(a) The bit-15 write on a centre-back does not persist: the game reverts it within ≈0–3 s. Holding it needs a repeated write (a pin). The owner ruled the pin on 17-10-26; it ran 15 min (§6.6), held the word 87% of samples, and found no full attack.
(b) The team did not reach full attack (`b8a2 = 1`) in about 7 minutes of play, so the +0.2 and the `P+0x3EF8` write never ran. The designated-slot observation with a bit-15 player therefore has no data.
(c) **Second live-copy mirror not located.** The owner ruling asked for both mirrors. Only one copy-table pointer exists (`G+0x2620`, `find_mirror.py`). A 32-byte header scan of the rw heap (`live/sibling.py`, 793 regions) found no sibling buffer with the same header; the 16-byte key matched only the executable image (noise). The experiment therefore wrote one roster word and one copy word, not "both mirrors".

### 6.6 Pin run (owner ruling 17-10-26: pin bit 15 on the same CB, cap 15 min)

+ Log: `live/pin_run1.log`, script `live/pin.py` (rewrites only the roster word `0x1D47B830` and copy word `0x1D4A3160`; never touches `MI+0x1168/0x116c/0x1574`).
+ **PIN_START 11:35:44; PIN_END 11:50:44 (cap, 900 s). No full attack was reached, so no early stop.**
+ Counts: 413,952 samples; 106,872 rewrites; the game reverted the roster word 53,020 times. Bit 15 was held at the sample point in 87.0% of samples.
+ **Restore:** both words back to `0x1000` at 11:50:44, readback `0x1000 / 0x1000`, VERIFY ok.
+ **Full attack (`b8a2 = 1`): never seen** in 15 min for either side (`b8a2` read 0 whenever the team pointer was valid; `gauge` 2). So `FUN_1405FC860` never ran during the pin, and `TD+0x3EF8` stayed 0 (`des_0 = des_1 = 0` in every valid sample). **The `P+0x3EF8` and +0.2 observation the owner asked for therefore did not happen; the experiment did not reach its goal.**
+ **F63C0 / F7D00 outcomes:** not observable. Their outcome is a candidate-list write, and the candidate-list call sits on the full-attack path that did not run. Nothing was logged for them.
+ **Candidate list (`SP[4..14]`) against baseline:** the normal list was the same as the baseline list (`(1979|2047, 1, 0, 255, 255, 1, 7, 3, 4, 5, 10)` and `(…, 2, 7, 3, 4, 10, 5)`). Slot 1 (our CB) is in the side-0 list only as the baseline value, and no change to the list was caused by the pin.
+ **Unexplained difference:** in 3,214 of 56,233 change-rows the lists were all `255` (cleared). The 200-s baseline never showed a cleared SP list. This is confounded (run length, set-piece restarts) and was not isolated, so it is **not** attributed to the pin. Needs a controlled comparison.

### 6.7 Status

+ Item 6 (the bit-15 player and when `P+0x3EF8` names him): **OPEN**. The pin held bit 15 on a CB for 87% of 15 minutes, but the team never reached full attack, so the designated-slot observation still has no data.
- Next: (1) a full-attack phase must occur while the word is held; the pin cap has been used, so this needs a new ruling for a longer or repeated window; (2) FrontmanSource's revert-writer finding would let the pin be replaced by a clean hold.
- **Executed 17-10-26 (EFPin), see §6.8.** Side-0 `team+0xB8A2` pin (control), pin plus bit 15 on the CB (run B), and two diagnostics. `P+0x3EF8` never left its initial value in any sampled frame, so no designated slot was observed in either run. The +0.2 is therefore unattributed. Reason and blocker: §6.8.
- Read-only verify at report time: CB roster word `0x1000`, copy word `0x1000` (`exp.py verify`); no pin is active.

Superseded by §6.6 and §6.7.

### 6.8 EFPin runs: `b8a2` pin, then bit 15 on the CB (17-10-26, owner ruling: pin `b8a2`=1; bit 15 on CB roster and copy words)
**CORRECTION (17-10-26, see §6.9): every `P+0x3EF8`, ordinal-list and `FUN_1405FC860` read in §6.8 used the wrong base.** The probe took `TD = [brain+0x8+0xBA8]` = `[brain+0xBB0]`. The team tick's ctx is `brain+0x10`, so the designated-slot object is `P = [brain+0xBB8]`. The `0`/`0x30`/`0xFF` values in §6.8 are from `[brain+0xBB0]`, a different object that FC860 never writes. Those "no designation" results are void. The sentinel and `0xFF` writes went to that wrong object and were restored.


Scratch: `<workdrive>/gf-EFPin-scratch/efpin_run.py`; logs `efpin_control.log`, `efpin_cb.log`, `efpin_des.log`, `efpin_probe.log`. Written words: side-0 `T+0xB8A2` at `0x1D47D6A6`; CB roster gate word `0x1D48B230` and CB copy word `0x1D4B3110` (new match, so different VAs from §6.4). `MI+0x1168/0x116C/0x1574` never written. Every write restored and read back: `b8a2` 0x00, roster and copy 0x1000, `TD+0x3EF8` 0x00, all `ok=True`. No pin is active.

- **A, control** (`control`, 300 s): pin `b8a2`=1 only. 130,657 samples. Pin rewritten 1,848 times; all pins held in 6.9% of samples. `P+0x3EF8` = 0 in all 9,226 valid samples, both sides. Set-piece entries: 5.
- **B, cb** (`cb`, 300 s): pin `b8a2`=1 and bit 15 on CB. 129,034 samples. Rewrites: `b8a2` 1,658, roster 1,636, copy 1,658; all pins held in 5.9%. `P+0x3EF8` = 0 in all 9,247 valid samples, both sides. CB bit 15 visible at 1 of 14 set-piece entries; `b8a2`=1 at 2 of 14.
- **Diagnostic D** (`des`, 150 s): pin `b8a2`=1, write `TD+0x3EF8`=0xFF once (VA `0x1D481E40`). 0xFF stayed in all 5,303 valid samples. In frames with `b8a2`=1 this shows no winner was written.
- **Diagnostic P** (`probe`, 180 s, no `b8a2` pin): write `TD+0x3EF8`=0x30 (sentinel) once. Stayed 0x30 in all 6,297 valid samples, `b8a2_0`=0 throughout. CONFIRMED (decompile): FC860 with `b8a2`==0 writes 0xFF at its first gate. So FC860 did not run on side 0 in ~3 min of play, dead ball and set piece.

**Verdict on the owner's questions (CONFIRMED by live read unless marked):**
- Does `P+0x3EF8` name the CB in B and not in A? **No, in neither.** It was 0 in every valid sample of A and B, and 0 at every set-piece entry (0 of 19 entries). Count of CB designations: 0.
- +0.2 effect: **not attributable.** No designation was written, so the FC860 +0.2 branch (L62–65) was never observed to matter.
- Candidate list (`FUN_14051B8B0`, SP[4..14]): side 0 read `(2047,1,0,1,2,3,6,4,5,8,9)` in every run, with 181 cleared (all 255) rows in A and none for side 0 in B. Side 1 changed in B (SP[4] 2047↔143, SP[14] 8↔9). INF: the CPU store (`FUN_140A55CF0`==0 branch) runs for side 1 and not side 0, so side 0 may be the human side. Not verified: `FUN_140A55EF0` reads a side-type table I did not decode live.
- `F63C0` / `F7D00` outcomes: **not observed.** Their outputs (candidate array, return slot) are not written to memory, and I used no breakpoints. OPEN.

**Decode added (CONFIRMED, raw decompile of `FUN_1405FC860` L1–L28):** gate 1, `b8a2`==0 → `des=0xFF`, return. Gate 2, `MI+0x116C`!=1 → `des=0xFF`. The recompute runs only if `0x15 < des` (the `0x15 < *(uint *)(param_2+0x3ef8)` test). So an initial `des`=0 can never recompute, and the pin alone cannot produce a designation. The one caller is `FUN_1405EC600` (its FC860 call sits at the end of an 11-slot loop). `FUN_1405EC600` is called from team tick `FUN_14051D9E0` in the MI==1 branch and the MI∈{2,3} branch. The probe shows the chain does not reach FC860 for side 0.

**Blocker (OPEN):** why the team tick (or `FUN_1405EC600`) does not reach FC860 for side 0. The tick's own entry gate, before its `MI+0x116C` branch, is not decoded. Next: (1) decode that entry gate; (2) run the sentinel probe on side 1's `TD+0x3EF8` to see whether any side reaches FC860; (3) only then repeat the CB pin, in a state where FC860 runs.

Side-0 HJ readout (`copy+0xA1` H, `copy+0x9C` J, as `block+0x31`, `block+0x2C`): values 175–189 and 57–96, in the range the reach formula clamps. CB H=185. Plausibility only (INF on the field identity).

### 6.9 Side 1 (CPU side) run, and the tick entry gate (17-10-26, Main ruling)

**Corrected base (CONFIRMED by decompile and live read).** `FUN_1404F6C30` is called from `FUN_14041EC80` at `0x14041EDD7`, for each side with `brain_s = [H+0x450+8s]` (`H = [0x1436F3F98]`). The loop has no side test; it runs whenever `brain_s != 0`. `FUN_1404F6C30` calls `FUN_14051D9E0` with `ctx = brain+0x10` (directly, or via `FUN_1404F9DC0(brain)` → `ctx = brain+0x10`). Inside the tick, `P = [ctx+0xBA8] = [brain+0xBB8]`. The ordinal list is `SP[4+j] = ctx+0x10+4j = brain+0x20+4j`. Live (read-only `ptr_probe.py`, 31,120 samples): `[brain+0xBA8]` is shared by both sides (`0x2C025888`); `[brain+0xBB0]` is per side and always has designation 0; `[brain+0xBB8]` toggles (null in ~94% of samples). Its +0x3EF8 read 0xFF in 734 samples (side 0) and 1,212 samples (side 1); `b8a2` was not read in that probe. 0xFF is the gate-1 signature of FC860 (`b8a2`==0 → `des=0xFF`). The side-1 runs below confirm FC860 recomputes for side 1 when `b8a2`=1.

**Entry gate (CONFIRMED decompile).** Nothing in `FUN_14041EC80`, `FUN_1404F6C30`, `FUN_1404F9DC0`, `FUN_14051D9E0` or `FUN_1405EC600` tests the side for the FC860 call. `FUN_14051D9E0` calls `FUN_1405EC600(…,0)` in the `MI+0x116C==1` branch, and `(…,1)` in the `MI∈{2,3}` branch (`0x14051E264`). `FUN_1405EC600` has one `return` (L497). The 11-slot loop calls FC860 on its 11th pass (`uVar20>10`). Side is set only in the callee tests.

**The side test `FUN_140A55CF0(G+0x180, side)` (CONFIRMED decompile, read live).** It returns 0 when `DAT_1434FC9C0[mode(+0xF0 of DAT_143704E38)]` = 10. Otherwise it scans 24 records of 0x74 bytes at `G+0x180` and returns 1 if a live record (`id(+0x10)!=-1 or +3!=0`, `+0xD==0`) has value `== side`, where value = 0 for type 0, 1 for type 2, 2 otherwise. Live state: `mode=8`, table=6 (not 10); two live records, rec0 (type 0, id 0) and rec1 (type 2, id 1). `FUN_140A55CF0(G+0x180, 0) = 1` and `(…, 1) = 1`. The CPU branch in `FUN_14051B8B0` (`==0`, L98, the only bit-15 store) is **not taken for either side** in this state. The **human-side reason is NOT confirmed**: the test does not separate the sides. `FUN_1405F63C0` calls the test with one argument (`param_2` undefined in the decompile), so its result is unreliable.

**Runs (CONFIRMED live; logs `gf-EFPin-scratch/win_A.log`, `win_B.log`; scripts `win_run.py`, `summarize.py`).** Side 1 target: `T1+0xB8A2` pinned to 1 (`T1 = G+0x1928+0x28`, VA `0x1D49B7B6`). B also pins bit 15 on the side-1 CB, local slot 1 = global 12 (roster gate word `0x1D4A9590`, orig `0x0`, target `0x8000`; copy word `0x1D4DE8D0`, orig `0x0`, target `0x8000`). "CB = slot 1" is INF (formation role from §6.4, not checked live). Designated object `0x1D49CCA0` (side 1). In every window the script wrote `0xFF` to `+0x3EF8` once if it was not already `0xFF`, then logged to close.
- **A (control, 300 s, b8a2 only):** 914 windows. `b8a2_1`=1 at all 914 opens. `P+0x3EF8` at open: 0xFF (831), 12 (83). At close: 12 (104), 255 (810). `FUN_1405FC860` recomputed to the CB (12) in 104 windows (12.5% of the 831 windows that started at 0xFF with no write).
- **B (cb, 300 s, b8a2 + bit 15 on CB):** 502 windows; `b8a2_1`=1 at all opens; bit 15 on CB roster word at all opens. Open: 0xFF (425), 12 (77). Close: 12 (69), 255 (433). Recompute to CB in 69 of 425 (16.2%).
- **Pooled:** rate A 12.5%, B 16.2%, z≈1.8 (two-sided p≈0.07). The bit-15 difference is not established.
- **Candidate list (SP[4..14] on side 1):** `(0,1,2,3,7,4,5,6,8,9,10)` in all 914 A opens and 495 of 502 B opens. B had 5 all-255 lists (cleared) and 2 rotated lists `(1,2,3,4,7,5,6,8,9,10,0)`. A had none. The cleared lists appear only in B; not attributed (5 of 502 windows).
- **Restore (verified):** `b8a2` 0x00, roster 0x00, copy 0x00 (readbacks `ok=True`); designated object `0x1D49CCA0+0x3EF8` restored to first-seen `0xFF` (readback `0xFF`). No pin is active.

**Answers to the ruling.**
- Does `P+0x3EF8` (side 1) name the CB in B and not in A? **No.** It names the CB (12) in A too, at a similar rate (12.5% vs 16.2%, not significant). So the bit-15 pin is not what makes him the designee.
- Side-1 candidate list: unchanged in A and in B (the CB is at index 1 in both). The bit-15 store (`FUN_14051B8B0` L106) is not reached (see the side test above), which matches the list not changing.
- +0.2 effect: **not attributable.** The CB is named in A without bit 15, so the +0.2 in FC860 cannot be isolated from the designation. Scores not logged.
- `F63C0` / `F7D00` outcomes: not observed (no breakpoints; outputs not in memory after return). OPEN.
- Human side: **OPEN.** The side test returns 1 for both sides live (above). Side 0 = human is not confirmed by this decode.

**Side-0 and side-1 windows show FC860 runs for both sides.** The earlier "FC860 never ran" finding (§6.8, diagnostic P) is void: it read the wrong object.

**Other void results (same base error):** the ordinal-list values and "cleared SP lists" in §6.3, §6.6 and §6.8 (`SP = [brain+0x8]+8`) are misaligned by 8 bytes. The list is `brain+0x20`. Re-derive before citing. The `pin_run1` pin (§6.6) wrote the same wrong object's list reads.

**Open (after §6.10):** (1) side-1 CB identity: now CONFIRMED by formation role (§6.10); (2) the +0.2 as a score: CONFIRMED (§6.10); (3) the human-side marker (`FUN_140A55CF0` returns 1 for both sides, §6.9): OPEN; (4) the live winner does not match the decoded fallback (§6.10): OPEN, the kind-8 exclusion is the unmodelled gate.

### 6.10 FC860 candidate scores, +0.2 measured from its inputs (17-10-26, Main ruling)

**Score decode (CONFIRMED by decompile and live constants).** Reach mode 2 = `C1 + K1·cJ + (C2−C1)·cH + D2`, with `cJ = clamp((J−40)/59, 0, 1)` and `cH = clamp((H−150)/60, 0, 1)`. Read live: `C1 = 1.935`, `K1 = 0.435`, `C2 = 2.335`, `D2 = −0.054343`, `J` lo 40 / hi 99 / span 59, `H` lo 150 / hi 210 / span 60, `DAT_142598578 = 0.2` (bit 15), `DAT_1425A8A28 = 0.05` (tie window). `FUN_140A6DF00(slot,…,2)` reads `block+0x31` (H) and `block+0x2C` (J) from `block = copy+0x70` (raw CONFIRMED, `disasm` of `0x140A6DF00`). So `H = copy+0xA1`, `J = copy+0x9C`, `copy = G[(slot+0xF3)·0x28]`.

**Bit-15 gate (CONFIRMED decompile of `FUN_1408CB0B0`, `FUN_140A64030`).** The gate is `roster_word(bit 15) AND DAT_142613D20[role] (bit 15)`, with `role = T+0x4C00[slot]` (the PES position enum: 0 GK, 1 CB, 2 LB/RB, 3 DMF, 4 CMF, 5 LMF/RMF, 6 AMF, 7 SS, 8 LWF/RWF, 9 CF). The row table is `0x180000, 0xD000, 0x70000, 0x3C00, 0x3700, 0x6D0, 0x712, 0x113, 0xF0, 0xF, 0, 0, 0`. **Only role 1 (CB) has bit 15 in its row**, so the Extra Frontman bit can only ever select a CB in this gate.

**Side-1 slot 1 is a CB (CONFIRMED live).** `T1+0x4C00[k]` = `(0, 1, 1, 2, 2, 3, 4, 6, 7, 7, 9)` for k = 0..10: slot 0 is GK (0), slot 1 is CB (1). Side 0 is the same shape `(0, 1, 1, 2, 2, 3, 6, 6, 7, 7, 9)`. The enum is INF from `pes-re-matchlogic.md` §352 and `pes-re-stamina-subs.md` (role classes 0..9, CB = 1).

**Fallback loop (CONFIRMED decompile, L102–166).** Taker loop skipped: `T+0x8E08` takers are `(255,255,255)` for side 1. Fallback over k = 0..10: eligible if `MI+0x1748+u != 0`, kind `P+0x3FB8[k]` ∈ {1,2}, not in the kind-8 table. Non-bit candidates are dropped when `P+0x7B5D < 2` and `rec+0x39 < 0xAF` and `rec+0x34 < 0x50` and `rec+0x25 < 0x50`. Score = reach + 0.2 if the gate bit. Winner: replace if `|Δ| > 0.05` or no winner yet and the score is higher; within 0.05, the higher `rec+0x25` wins. Write only if a winner exists. `u = 11·side + k`.

**Runs (CONFIRMED live; `gf-EFPin-scratch/score_run.py`, `score_A.log` 80 windows, `score_B.log` 138 windows, 90 s each, side 1, b8a2 pinned, 0xFF written once per window; B also bit 15 on the CB).**
- CB row, A: `bit 0, base 2.3131, score 2.3131` (every window).
- CB row, B: `bit 1, base 2.3131, score 2.5131` (every window). **The +0.2 is applied exactly (2.5131 − 2.3131 = 0.2000).**
- Gap to the next eligible candidate (kind 1 slot 2, base 2.2393): 0.0738 < 0.2. The other eligible candidates are 2.2355 (kind 2). Kind 6/7/9 players have higher base (2.43) but are not eligible (kind ≥ 3).
- `P+0x7B5D = 4` in every window, so the non-bit filter was inactive. The record fields `rec+0x25/0x34/0x39` read 0 for both sides (see the caveat below).
- Decoded winner: the CB in every window, A and B (the bit does not change the decoded winner here because the CB already has the highest eligible base). **Observed winner (`P+0x3EF8` at close): 255 in all 218 windows**, A and B. This does not match the decode.
- Earlier 300-s runs (§6.9) did show 12 in 12% of windows, so the live winner depends on state the decode does not model.

**Why the decode disagrees (OPEN).** The kind-8 exclusion (`FUN_140A63950(X, side, 8, u)`) is the one unmodelled gate. `X` is the FC860 local `+0xF8`; in `FUN_14051B8B0` the same test uses `SP+0xB98`, so `X` may be `ctx+0xB98` (`ctx = brain+0x10`), giving `[X+0x18] = [brain+0xBC0]`. That pointer is null in every live read, and `[brain+0xBB0]` gives float data, so the table is not located. Other suspects: the team record `FUN_140A3AF90` takes its flag branch on `T[0]` (side 1 `T[0]=0`, so every side-1 candidate gets the same record `T+0xD8`; the fields read 0), and the 0.05 tie branch was not exercised.

**Answers.**
- +0.2 measured: CONFIRMED in the computed score (2.3131 → 2.5131 with the gate bit read live). Not measured as a designation effect.
- Does the bit change the winner when the gap < 0.2? **Not answerable live.** Decoded: no (the CB is already top). Live: the winner is 255 whatever the bit, so the live designation does not follow the decode.
- Side-1 slot 1 is a CB: CONFIRMED (role 1 = CB, §6.10 above; INF on the enum name).

**Restore (verified):** `score_A`: `b8a2` 0x00, `P+0x3EF8` 0xFF (readbacks ok). `score_B`: `b8a2` 0x00, roster 0x00, copy 0x00, `P+0x3EF8` 0xFF (ok). No pin is active. `MI+0x1168/0x116C/0x1574` never written.

**Error log.** About 140–170 transient read errors per run (EIO) when the designated object pointer toggled; those windows were skipped. The windows that were logged are internally consistent.


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
3. Kind classes `P+0x3FB8` beyond kind 0 = keeper (INF) and 1 (−100 penalty): CLOSED 19-10-26 — the §5 class map (kind 1 = CB, 2 = FB per FC860's {1,2} test); P aliases S at init (role-writer §2, §5).
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
