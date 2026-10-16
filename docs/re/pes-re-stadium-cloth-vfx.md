# B6 / B7 read-only decode (Track B), round 2

Scope: decode only. No engine code, no commits, no branches, no game writes, nothing visual.
Tools: `pesfmt_dump` (install reads), scratch C++/Python in `the scratch dir` against `build/libpesfmt.a`, Ghidra HTTP `decompile_function_by_address`, MCP xrefs, and `dec.sh` (one call at a time) for undefined addresses.

Evidence tags: **OBS** = observed in bytes or decompile (method given). **INF** = inferred. **OPEN** = not resolved.

VAs are in the unpacked image (base 0x140000000). The on-disk `PES2021.exe` is protected (its `.text1` is a 0x1600 stub), so code searches used `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − base).

---

## 0. Summary

| item | status |
|---|---|
| 1. team → stadium | **Resolved (reader found, VA-cited).** Open: the source of the valid-stadium set; why Roma's record stores 54. |
| 2. cloth vs nocloth | **Partly resolved.** Both part objects, their vtables, the draw list, and the flag mechanism are OBS. Open: the predicate in the enclosing function that chooses the cloth (0xD8) or nocloth (0x80) builder. |
| 3. vfxdb | **Not resolved.** Header read; record count and layout not decoded; the reader is not found. Route 2 is **not proven** (see §3). |
| 4. §3 corrections | **Done.** Table in §2.5, with constructor citations. |

Corrections to earlier premises (from round 1, still valid): the install's effective `Team.bin` is the 4cc download override (210 records, +0x3C = 29 for all); `vfx_class_db.vfxdb` is not the effects §5 table.

---

## 1. Team → stadium (item 1)

### 1.1 Table and record (OBS)

- Registry global `DAT_1436fed18`; accessors `FUN_14125f2b0` (registry), `FUN_14125f2c0(reg, id)` (count, container +0x38), `FUN_14125f290(reg, id, idx)` (record = data + idx·stride, via `FUN_14125f580`).
- Table ids (name table `0x1434d6630`, names at `0x1428bae78 + …`): `Stadium.bin` = **19** (stride `0x188` = 392), `StadiumOrder.bin` = 20, `StadiumOrderInConfederation.bin` = 21, `StadiumWeight.bin` = 22, `Tactics.bin` = 23, `TacticsFormation.bin` = 24, **`Team.bin` = 25** (stride `0x5FC` = 1532), `PlayerWeekly.bin` = 26, `TeamWeekly.bin` = 27. Stride table at `0x1428bb100`, index 19 = `0x188`, index 25 = `0x5fc` (read from the exe).
- Team record (vanilla, 663 records, unwrapped from WESYS): +0x08 u32 team id, +0x170 English name, +0x372 short name, **+0x3C u32 whose low u16 is the home stadium id** (`(short)puVar5[0xf]`, see 1.2).

### 1.2 Reader (OBS, decompile)

- **Reader: `FUN_141eeeea0` @0x141eeeea0** (body `[0x141eeeea0, 0x141eef26]`, headless `dec.sh`). It is slot 16 (+0x80) of the vtable at `0x142b9a9c8`, whose RTTI is `.?AVManager@edit@@`. So this is the **edit-menu** team reader (`edit::Manager`), not the match loader. The body includes 0x141eeeecb, which the round-1 `dec.sh` call reached as a fragment.
- Steps, in order:
  1. `puVar5 = FUN_14125f290(reg, 0x19, i)`, match when `puVar5[2] == key >> 14` (team id).
  2. `stadium = (short)puVar5[0xf]` (u16 at record +0x3C).
  3. `if (stadium == 0) stadium = 0x1d` (29, Konami Stadium).
  4. `if (!FUN_1414fe9a0(this+0x1e8, stadium)) stadium = 0x1d`.
  5. write `stadium` to the output struct at tag `0x10` (`FUN_140c20910(out, stadium, 0x10)`).
- **What the value indexes (OBS):** the Stadium.bin row whose **u16 at +4 equals the id** (`FUN_141eeed70` @0x141eeed70 is the lookup: table `0x13` = 19 (Stadium.bin), `*(u16*)(row+4) == id`). It is an **id, not a row index** (Lazio = 6 → row 2 is `ローマ・オリンピコ`; the id 54 has no row 54 in a 52-row table).
- The name written to the output: row+0xBD (second name, 181 B), or row+8 when `DAT_1434fce64 == 5` (`FUN_141eeed70`, `FUN_141ef4090(out, row+0xbd|8, 0x79, 2)`).
- **`FUN_1414fe9a0(this+0x1e8, id)` = membership test** (bool, a tree find on key at node+0x1c; OBS). It returns 1 when the id is in the set held at `this+0x1e8`. Its gate `FUN_1414feab0` (OBS: `*(this+0x1e8) != 0 && !FUN_1414a8ce0()`). The set is built by `FUN_1414feb50` @0x1414feb50 from a u32 list (OBS): the list comes from `FUN_1414a8b70` / `FUN_1414a8c70` (object +0x50 / +0x48 of `*param_1`, empty when `FUN_14149ab50()` is set). Each u32 has the stadium id in the low 16 bits and four 4-bit flags (bits 16–31). **Source of that list: OPEN.**

### 1.3 Roma → 54 against the install's stadium list (OBS)

- Roma: team **125**, English `ROMA`, short `ROM`, Chinese 罗马. Its +0x3C = **54**.
- Stadium.bin (vanilla, 52 rows): id **54** = row 35, `ナショナル・スタジアム` (folder `st054`). `ローマ・オリンピコ` (Olimpico) is **id 6**, row 2.
- The same byte is an id: **Lazio (team 122) = 6 = Olimpico** (correct).
- Who else stores 54 (9 teams): Italy national team (12), América de Cali (1257), Zürich (1957), Junior (2285), Wuhan Zall (4095), Hebei CFFC (5170), Henan Jianye (5171), Shenzhen Kaisa (5184), and Roma (125). Stadium 29 (`コナミ・スタジアム`) is the default (0 or invalid → 29) and appears in 175 records.
- **Explanation (INF, medium):** the value is read as a Stadium.bin id, but it is only used if `FUN_1414fe9a0` accepts it (step 4). For Roma, 54 is stored in the file, and the menu/match code writes 54 only when 54 is in the valid-stadium set (1.2). If it is not, the code writes 29. Whether 54 is in that set, and which list builds it, is **OPEN** (1.2). I did not confirm the in-game home stadium for Roma.

### 1.4 Installed effective file (OBS)

- The 4cc override (`download/4cc_*`) provides `Team.bin` for this install: 210 records, team ids 701–910, **+0x3C = 29 for all 210**. It is a pack value; the reader must take it from the file at runtime (AGENTS.md), never pinned.

### 1.5 Match-side writer (OPEN)

- The match stadium path: `FUN_141f5b030` @0x141f5b030 → `FUN_141eabf70` @0x141eabf70 → `FUN_141eacd80` (reads obj +0x10 = stadium id, builds `light_st%03d_{af,ar}.fpk` via `FUN_141ed2f60` @0x141ed2f60). `FUN_141f5b030` takes the id from `*(DAT_1437050c8 + 0xf0)`; the slot's team id is at +8 (`FUN_1414a8420`). **OPEN:** whether the match path calls `FUN_141eeeea0` (the edit-manager reader) or has its own copy of 1.2. I did not find a match-side writer of +0xF0.

---

## 2. Cloth / nocloth (item 2) and §3 corrections (item 4)

### 2.1 Two part objects (OBS)

| | object A (0x80 B) | object B (0xD8 B) |
|---|---|---|
| allocator | `FUN_1403fb410(…, 0x80, 0x10)` in the caller (`FUN_141a8b0d5`) | `FUN_141a828b0` @0x141a828b0 (`0xD8`) |
| ctor | `FUN_141a81c70` @0x141a81c70 | `FUN_141a816e0` @0x141a816e0 (via `FUN_141a828b0`) |
| vtable | `PTR_FUN_142aad500` (0x142aad500) | `PTR_FUN_142aad5e0` (0x142aad5e0) |
| draw/sim list | the caller's draw loop iterates **RDI+0x50** (A's insertion point not checked) | **RDI+0x38** (inserted via `FUN_141a8c300(RDI+0x38, …)`; not in the draw loop) |
| literal path prefix | `…/uniform/nocloth/` (0x142aab668), `…/nocloth/shirt.fmdl` (0x142aac848), `…/nocloth/` (0x142aac958) | `cloth/` and `nocloth/` pairs (below) |

### 2.2 Object A, field by field (OBS, ctor `FUN_141a81c70`)

- `+0x58` ← `FUN_141696b50(path = param_4+8)` (ref via `FUN_14031e700`). `param_4+8` is the caller's nocloth path string (the caller appends `nocloth/` and `collar_%03d.fmdl`/`referee_collar_%03d` or `shirt.fmdl`, §2.4).
- `+0x20` ← `FUN_1416934f0(param_4)` when `param_4+0x10 != 0`, else null (`FUN_1403cf7e0(param_1+4, …)`). **This is the model the draw loop draws**: caller draw loop `lVar11 = *(plVar9 + 0x20); if (lVar11) FUN_1416982d0(lVar11, …)`.
- `+0x78` ← `*param_5` (ref, `FUN_1403cf7e0(param_1+0xf, …)`). Caller's `param_5` = the cloth-related string/ref from `FUN_141a45ca0`. **Not identified as cloth.**
- The caller calls A's vtable `+0x58` (count) and `+0x60` (get i) for each object; A's `+0x60` is not decoded here.

### 2.3 Object B, field by field (OBS, ctor `FUN_141a816e0`; slots decompiled)

- `+0x58` ← `cloth/shirt_out.fmdl` (0x142aad6c8) when `param_4 == 0`, `cloth/shirt_in.fmdl` (0x142aad648) otherwise.
- `+0x88` ← `nocloth/shirt_out.fmdl` (0x142aad708) / `nocloth/shirt_in.fmdl` (0x142aad688).
- `+0x20`, `+0x78`, `+0xa8` ← null (ctor, `FUN_1403cf7e0(…, 0)`). `+0x40` (`*(u32*)(param_1+8)`) = 1.
- Flags: **`+0x14` byte** is set to 1 by slot `+0x20` = `FUN_141a88c80` (`*(u8*)(p+0x14) = 1; (*p)[+0x30]()`), and **`+0x15` byte** is set by slot `+0x38` = `FUN_141a826e0` (`*(u8*)(p+0x15) = arg; (*p)[+0x30]()`), where the arg is the caller's `unaff_SIL` (`(**(code**)(*plVar9+0x38))(plVar9, unaff_SIL)`).
- Slot `+0x30` = `FUN_141a85620` (selector):
  - if `+0x14 == 0 || +0x15 == 0`: take `+0x20` as-is → `FUN_14168c960` (direct);
  - else: `FUN_141a83d20(p, &+0x20)` → if `+0x08` (cloth sim object) and the model are non-null, `FUN_14168cbc0(+0x08, model)` (bind to cloth sim), then clear `+0x20`.
- Slot `+0x58` = `FUN_141f05c10` → returns **2** (two models). Slot `+0x60` = `FUN_141a82cd0`: `i==0 → +0x78`, `i==1 → +0xa8`.

### 2.4 Caller and the branch (OBS for the parts, OPEN for the predicate)

- `FUN_141a8b0d5` @0x141a8b0d5 (headless `dec.sh`, body `[0x141a8b0d5, 0x141a8b5f7]`). The body starts mid-function, so the **enclosing function is undefined in Ghidra**; the branch that picks object A's builder (`FUN_141a81c70`, path `nocloth/`) versus object B's builder (`FUN_141a828b0`, path `cloth/`+`nocloth/`) is in that enclosing function. **OPEN.**
- Caller literals that build the nocloth path for A: key `0x506226d207df` appends `…/nocloth/` + (`referee_collar_%03d` if `RDI+0xb8 ∈ {1,2}` else `collar_%03d`) + `.fmdl`; key `0x841be197b47b` appends `…/nocloth/shirt.fmdl` when `RDI+0xd5 == 0`, `RDI+0xb8 == 0`, `R12B != 0` and `RBP-0x58 != 1`. Key `0x7575fa4e9626` = shirt_out part id, `0xb69167e9ca5b` = shirt_in part id (the constructor writes them to `param_1[9]`).
- `RDI+0xb8 == 0` gates the normal-player branch (`FUN_141a82a70`, `FUN_141a829c0` run only then).
- **Which path a normal match draws (OBS for each link, INF for the total):**
  1. Object A (nocloth) is in the draw list RDI+0x50, and is drawn through `+0x20` whenever its builder ran.
  2. Object B (cloth) is in RDI+0x38. It emits a model only through slot `+0x30`, and only when **both** `+0x14` (set by slot `+0x20`, whose caller is not identified) and `+0x15` (= the caller's `SIL`) are non-zero. With `+0x14` clear, `+0x20` is null and B draws nothing.
  - So **the drawn cloth-free shirt in the observed path is A (nocloth)**, consistent with the round-1 finding (`nocloth` in the draw list). **INF (medium):** the cloth-sim path is active only if some caller sets B's `+0x14`; the setter's caller is OPEN. The predicate `uStack_1d0` of `pes-kit-assembly.md` §3 is consistent with the caller's `RDI+0xb8 == 0` gate but is not verified here.

### 2.5 Corrected table for `pes-re-cloth-nets.md` §3 (item 4)

Replace the §3 paragraph ("`ClothShirtParts` (0x80 B) … +0x20 nocloth, +0x78 cloth") with:

| object | size | ctor | vtable | field | content (ctor) | role |
|---|---|---|---|---|---|---|
| A | 0x80 | `FUN_141a81c70` | 0x142aad500 | +0x58 | nocloth model ref (path `param_4+8`) | ref |
| A | 0x80 | | | **+0x20** | model from `FUN_1416934f0(param_4)` or null | **drawn** (draw loop, `FUN_1416982d0`) |
| A | 0x80 | | | +0x78 | `*param_5` ref | not identified as cloth |
| B | 0xD8 | `FUN_141a816e0` (via `FUN_141a828b0`) | 0x142aad5e0 | **+0x58** | **cloth** model ref (`cloth/shirt_{in,out}.fmdl`) | cloth ref |
| B | 0xD8 | | | +0x88 | nocloth model ref (`nocloth/shirt_{in,out}.fmdl`) | ref |
| B | 0xD8 | | | +0x08 | cloth sim object (`FUN_141a83d20`) | sim |
| B | 0xD8 | | | +0x14 / +0x15 | flags: set by slot +0x20 / slot +0x38 (SIL) | gate |
| B | 0xD8 | | | +0x20, +0x78, +0xa8 | null at ctor; get(0)=+0x78, get(1)=+0xa8 | model holders |

Correction, in one line: **the cloth fmdl is at +0x58 of the 0xD8 object (B); the drawn nocloth fmdl is at +0x20 of the 0x80 object (A); +0x78 is not the cloth model.** The round-1 claim "+0x78 = cloth" is refuted.

Cite: `FUN_141a81c70` (ctor A), `FUN_141a816e0` (ctor B), `FUN_141a85620` (+0x30), `FUN_141a826e0` (+0x38), `FUN_141a88c80` (+0x20), `FUN_141f05c10` (+0x58), `FUN_141a82cd0` (+0x60), `FUN_141a83d20` (sim hand-off), caller `FUN_141a8b0d5`.

---

## 3. vfxdb (item 3)

- **Header (OBS):** `vfdb` at +0; u16 `0x001F` at +4, u16 `0x0051` at +6, then u32 words (`0xE7707950` at +8, `0x02000007` at +12, `0x00010000` at +16, `0x00100001`…).
- **Record count and layout: not decoded.** The `00 10 00 01 00 10` pattern recurs at offsets 19, 66, 1028, 1598, 1766, 2033, 2192 (spacings 47, 962, 570, 168, 267, 159): **not a fixed-stride array**. Name hashes (StrCode32/64 of the five models and 12 emitter names) do not appear in the file.
- **Reader: not found.** The string `vfxdb` at `0x142590808` is an entry of the file-extension table (`fclo`, `clo`, `lng`, `uig`, `uil`, `uif`, `uia`, `fnt`, `utxl`, `uigb`, **`vfxdb`** (index 10 from `0x143445c90`), `rbs`, `aia`, …). The table's DATA xref is only that slot. No RIP-relative code reference to `0x143445c90` or `0x143445ce0` was found (scan of `.text`), so the index is computed. No inline `0x62646676` (`vfdb`) immediate exists in code.
- **Route 2 (effect_config.xml + five models are all B7 needs): NOT PROVEN.** The effect *placement* data (effects §1.3, §1.7) is consistent with it, but I could not cite the reader that proves the vfxdb is unused by the effect path. B7 should treat the vfxdb as undecoded.

---

## 4. Verification

- Install reads (no game write): `pesfmt_dump ls/mounts/cat`, `Team.bin` (download override), vanilla `Team.bin`/`Stadium.bin` (`dt00`, `dt10`), `common_effect.fpk` (vfxdb member).
- Ghidra: `FUN_14125f290/2c0/2b0/310/480/670`, `FUN_1414f23d0`, `FUN_141eeeea0` (dec), `FUN_141eeed70`, `FUN_1414fe9a0`, `FUN_1414feab0`, `FUN_1414feb50`, `FUN_1414a8b70`, `FUN_1414a8c70`, `FUN_141a8b0d5` (dec), `FUN_141a81c70`, `FUN_141a816e0`, `FUN_141a828b0`, `FUN_141a85620`, `FUN_141a826e0` (dec), `FUN_141a88c80` (dec), `FUN_141f05c10`, `FUN_141a82cd0` (dec), `FUN_141a83d20`, `FUN_1414f0560`, `FUN_1414f6e20`, `FUN_1413f68b0`.
- Not run: no build, no ctest, no frames.

## 5. Open items (ranked by value)

1. **Valid-stadium set** (`this+0x1e8`): find the list at `object+0x48/+0x50` (`FUN_1414a8b70/8c70` callers) and check whether 54 is in it for Roma. Decides the Roma question.
2. **Match-side team → stadium writer**: the caller of `FUN_141eeeea0`/`FUN_141eeed70` on the match path, or the write of `DAT_1437050c8+0xf0`.
3. **Cloth predicate**: the enclosing function of `FUN_141a8b0d5` (undefined in Ghidra; find its start), to pick A vs B, and the caller that sets B's `+0x14` (slot `+0x20`).
4. **vfxdb**: find the code that indexes the extension table (computed base) to reach the vfxdb loader, then the record layout.
