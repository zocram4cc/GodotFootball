# Close the three opens in pes-re-stadium-cloth-vfx.md (read-only)

Scope: decode only. No engine code, no commits, no writes to the game, `docs/re/` not edited.
Tags: **CONFIRMED** = decompile, raw bytes or live read at a VA; **INF** = inferred; **OPEN** = not resolved, blocker stated.
Image: `pes_full.bin`, base 0x140000000. Live: PES2021.exe pid 2510857, read-only `pread` of `/proc/<pid>/mem`, base 0x140000000 confirmed in maps.
Scratch: `the scratch dir/` (dec1..dec10 decompiles, live scripts).

## 0. Summary

| item | result |
|---|---|
| 1. valid-stadium set + match path | **Partly closed.** Match-side resolver is now found and its rule is CONFIRMED. The **source list of the set is still OPEN**. The match loader does **not** read the team stadium from `DAT_1437050c8`; the earlier premise was wrong. **Roma (54 vs 29) is NOT settled.** No Roma match is live. |
| 2. cloth flag +0x14 | **Mostly closed.** Slot caller chain and gate predicate CONFIRMED. The root caller of the activation (player vtable slot +0x18) is **OPEN**. Normal-match outcome is stated as a conditional. |
| 3. vfxdb reader | **Reader found** (CONFIRMED): loader by name prefix `vfx`, parser `FUN_14161c8b0`. Header and instance count CONFIRMED at the header level. Per-instance record layout **partly decoded**; the schema resource is OPEN. Route 2 for B7 **not proven** (reader of `effect_config.xml` not located). |

Correction to the prior doc (`pes-re-stadium-cloth-vfx.md` §1.5): `FUN_141f5b030` gets its stadium from `FUN_141f500d0()+0xf0`, a 0x52E0-byte singleton at `_DAT_1437f5d50` (CONFIRMED, `dec2` FUN_141f500d0, `dec1` FUN_141f5b030 case 5 line 1633). `DAT_1437050c8` is a separate 0x18-byte team-slot object whose `+8` is the team id (CONFIRMED, `dec2` FUN_1414a8120/FUN_1414a8420).

---

## 1. Valid-stadium set and the match path

### 1.1 Match-side resolver (CONFIRMED)

`FUN_1414f5120` @0x1414f5120 (body [0x1414f5120, 0x1414f6acb]) builds the per-team output records. For each team record `puVar33` (team table, record stride from `FUN_14125f290`):

1. `sVar23 = (short)puVar33[0xf]` = u16 at **Team record +0x3C**.
2. `if (sVar23 == 0) sVar23 = 0x1d` (29).
3. `cVar7 = FUN_1414fe9a0(param_1 + 0x28, sVar23)` (membership in the set at **owner+0x28**; call site 0x1414f5c5d per xref).
4. `if (cVar7 == 0) sVar23 = 0x1d`.
5. store u16 at output record `+0x294` (`puVar30 + 0xa5`), and the packed team key `id << 14` at output `+0` (`uVar11 = puVar33[2] << 0xe`).

So for each team the **match-side resolved stadium** is `(team+0x3C) if that id is in S(owner+0x28), else 29`. This is CONFIRMED as a rule. The owner object of `S` is `param_1` of `FUN_1414f5120`; its callers are OPEN (xref 0x1414f6cca, unnamed function).

### 1.2 Set structure (CONFIRMED)

- `FUN_1414fe9a0(set, id)` @0x1414fe9a0: membership, map keyed by u32 at node+0x1c (MSVC tree; node+0x19 nil flag).
- `FUN_1414feb50(set)` @0x1414feb50: builds the map lazily from `X = *(set+0)`. `X+0x48` = byte count of the list (`FUN_1414a8c70`), `X+0x50` = u32 array pointer (`FUN_1414a8b70`). Both return 0 if the global byte at `*(_DAT_143704e40)` is set (`FUN_14149ab50`, `FUN_1414a8ce0`/`FUN_1414feab0` gate).
- Each u32 entry: **low 16 bits = stadium id**, **bits 16..31 = four 4-bit flags** (flag k = nibble k; a flag is true iff nibble == 1). Node payload `+0x20+k` holds flag k (CONFIRMED, loop at `dec1` 1344-1352).
- `FUN_1414fea20(set, id, k)` @0x1414fea20: returns flag k of id (CONFIRMED).
- `FUN_1414f4a80` @0x1414f4a80 reads the four flags for each stadium row it iterates (`FUN_1414f0200` list, row name at +0xBD, consistent with Stadium.bin `+0xBD`). Used by the stadium list UI and match setup (CONFIRMED callees, purpose INF).

### 1.3 Source of X: OPEN

Tested (all NOT the id|flags<<16 layout):
- `common/etc/pesdb/StadiumOrder.bin` (440 B, 8-byte rows `(seq, stadium id, ...)`, filtered by a 2-bit mode field at +4 bits 7-8 in `FUN_1414f4a80`).
- `common/etc/pesdb/StadiumOrderInConfederation.bin` (1760 B): `(u16 id, u16 seq)` rows.
- `common/etc/pesdb/StadiumWeight.bin` (3760 B): 8-byte rows, u16 fields.
- `common/match/constant/constant_stadium.bin` (8074 B, WESYS + zlib at +0x10 → 38832 B inflated): sub-table directory, no aligned id|flags entries in range 1–60.
- `common/etc/StadiumEditParam.bin` (12280 B): u32 index/param rows, no id|flags pattern found in the first 128 B.

Live memory (read-only, `liverun2.py`): 661 aligned runs of u32 with low16 in 1..60 and nibble-only high half. Most are small `(id, 0x0010)` DB-style tables. One family is interesting but **not confirmed as X**: 26-entry runs for ids 4..29 and 30..55 (together 52 entries, the vanilla Stadium.bin row count), with flag nibble1 = 1, repeated at a 0xC3C stride (e.g. 0x1c9cdd90, 0x1c9cde08, 0x1c9cea44, 0x1c9cf67c, 0x1c9d02ac). Runs containing 54 exist at 0x1c9cde08, 0x1c9cea44, 0x1c9cf67c, 0x1c9d02ac, 0x1eebc4f0, 0x1fe1b150, 0x3745a8c4. **Not linked to X**: a pointer search for the run starts (`ptrfind.py`) found no qword referencing them, so the object owning them is unknown. Verdict on X's source: **OPEN**. Candidates to test next: the object that owns `S` (owner of `FUN_1414f5120`), and the loader that fills `X+0x48/+0x50`.

### 1.4 Match loader consumes the singleton without validation (CONFIRMED)

`FUN_141f5b030` @0x141f5b030, case 5 (dec1 1590-1652): `lVar4 = FUN_141f500d0()` (singleton `_DAT_1437f5d50`, 0x52E0 bytes, created by `FUN_141f4c950`); copies team-side fields from `lVar4+0x50..+0xe8`; `uVar2 = *(u32*)(lVar4+0xf0)`; passes `uVar2` straight to `FUN_141eabf70(stadium_obj, &cfg, uVar2, …)`. **No call to `FUN_1414fe9a0` on this path.** So the stadium the match uses is whatever was written to `lVar4+0xf0`.

### 1.5 Writer of singleton+0xf0: OPEN

`FUN_141f500d0` has about 100 callers (xref dump; e.g. 0x141f5f0eb, 0x141f5b069, 0x141f57398). Not enumerated. The `+0xf0` writer is **OPEN**.

### 1.6 Live read (read-only), current process

| quantity | value | source |
|---|---|---|
| `*(0x1437f5d50)` singleton ptr | 0x1a753f70 | live |
| singleton `+0x50` (team id) | **21** | live |
| singleton `+0xf0` (stadium) | **0** | live |
| `*(0x1437050c8)` slot ptr | 0xa3cec30 | live |
| slot `+8` (team id) | 21 | live |

Interpretation: the running session is team 21 with an **unloaded** stadium (0). No Roma match is live, so Roma 54 vs 29 is **not observed**. The live scan for team 125 keys (`0x1F4000` at +0x294) gave no record with 54 or 29, so the team-record output is not resident now (INF: the f5120 output buffer is transient).

### 1.7 Roma verdict

- Static: Roma team 125 has `+0x3C = 54` (prior doc §1.3; re-checked only by the prior report, not here).
- If the match path takes the f5120 output: resolved = 54 **iff 54 ∈ S** (CONFIRMED rule), else 29.
- If `+0xf0` is written straight from the team record: 54 (INF, depends on OPEN writer §1.5).
- **Verdict: OPEN.** Decided by: (a) the source X of S, and (b) the writer of singleton+0xf0. A live check needs a loaded Roma match.

---

## 2. Cloth flag +0x14 (item 2)

### 2.1 Enclosing function (CONFIRMED)

`FUN_141a89120` @0x141a89120, body `[0x141a89120, 0x141a89575] [0x141a89956, 0x141a8b5f7]` (dec1). It contains 0x141a8b0d5 and is the **kit/part assembler** (virtual slot of the player class, vtable 0x142aad448; xref DATA 142aad448). Function start found by the `CC` run before 0x141a89120 (raw bytes), prologue `48 8b c4 55 56 57 41 54 41 55 41 56 41 57`.

### 2.2 Build predicate (CONFIRMED)

`uStack_1d0` (dec1 169-193): **cloth gate = 1 iff**
- `iVar12 == 0` (kit kind 0; `uStack_1d0 = 0` otherwise),
- `*(int*)(player+0xb8) == 0`,
- `*(char*)(player+0xd5) == 0`,
- `cVar9 == 0` (computed from `func_0x0001403d26c0`, dec1 548),
- `!bVar35` (bVar35 from `uVar13`/`uStack_204`, dec1 169-176).

Effects of the gate:
- gate = 1: key `0x7575fa4e9626` (shirt_out) builds **B** (0xD8) via `FUN_141a828b0(…, 1, …)`; key `0xb69167e9ca5b` (shirt_in) builds **B** via `FUN_141a828b0(…, 0, …)`. Both are inserted in `player+0x38` (dec1 615-629, CONFIRMED).
- gate = 0: those keys `goto LAB_141a8b13d` (dec1 615-616, 621): **no B** is built; the nocloth path (object **A**, 0x80, `FUN_141a81c70`, path `nocloth/`) is the alternative (keys `0x506226d207df`, `0x841be197b47b`; prior doc §2.4, CONFIRMED by the same decompile lines 549-611, 646-658).
- `player+0xb8 == 0` also gates `FUN_141a82a70`, `FUN_141a829c0` (dec1 681-684).

### 2.3 The +0x14 setter and its caller chain

- Slot +0x20 of **both** part vtables is `FUN_141a88c80` (xrefs DATA 142aad520 = A vtable +0x20, 142aad600 = B vtable +0x20; CONFIRMED). It sets `+0x14 = 1` (dec 2257-2268).
- The only indirect `call [reg+0x20]` in the kit range 0x141a80000–0x141a92000 is at **0x141a88d3d** (raw scan). It lies in **`FUN_141a88c90`** @0x141a88c90 (body `[0x141a88c90, 0x141a88d5f]`, dec6). That function:
  1. marks the four holders at `player+0x210/0x218/0x220/0x228` dirty (`+0x60 |= 2`, `+0x9d = 1`);
  2. calls slot +0x20 (**+0x14 = 1**) on **every** part in `player+0x38` (B list).
- `FUN_141a88c90` is reached only from `FUN_141a83c60` (xref 0x141a83c66, conditional jump = tail jump). `FUN_141a83c60` @0x141a83c60 (dec7 1-67) is **player vtable slot +0x18** (xref DATA 142aad460 = 0x142aad448 + 0x18; CONFIRMED):
  - `player+0x2c = p2`;
  - if `p2 == 1`: **activation** = the loop above (B +0x14 = 1);
  - else: holders `+0x98 = 2` and slot +0x28 on every B (deactivation, INF).
- **Caller of player slot +0x18: OPEN.** Virtual dispatch; not found in the decompiles I hold (dec8 `FUN_1419c4260` has no `+0x18` call). The message dispatcher `FUN_1419c4260` (dec8) is the place to look next.

### 2.4 The +0x15 (SIL) setter

Slot +0x38 `FUN_141a826e0` (CONFIRMED, dec4 210-220) writes `+0x15 = arg` and calls slot +0x30. Its callers:
- `FUN_141a85a00` @0x141a85a00 (dec4 1-44): for each B/part in `player+0x38`, matches the type hash (`plVar1[9] & 0xffffffffffff`: `0x1158d590d986`, `0x335cade007ee` → B; `0x1efca60661a9`, `0x582a734dcac9`, `0xa4e48873bd5c` → other) and calls slot +0x38 with 1 iff (`player+0xd4 != 0` or the B-specific test) — CONFIRMED.
  - Callers: `FUN_141a89120` line 680 (`FUN_141a85a00(player, player+0xd4, 1)`), `FUN_141a1e550` (dec7 109), `FUN_141a86f20` (dec7 132-177, the variant selector; `player+0xc0` vs part key), called from `FUN_141a1fee0` (dec8 1956-2020, when `FUN_141a80150(player) != p2`).
- `FUN_141a1e550` @0x141a1e550 is the handler for **dispatch case 0x1c** of `FUN_1419c4260` (dec8 470-475): payload byte at message `+0x10`, player key at `+8`. So SIL can arrive as a message.
- At creation (dec1 627, 670): `(vt+0x38)(part, bVar29)` with `bVar29` from the kit path (INF).

### 2.5 Draw rule (from prior doc §2.4, unchanged)

A drawn: object A (nocloth) when its builder ran. B drawn only when **both** `+0x14` (activation) **and** `+0x15` (SIL) are nonzero (slot `+0x30` `FUN_141a85620`, dec4 182-207, CONFIRMED). Otherwise B's `+0x20` is null and nothing is drawn.

### 2.6 When does a normal match run cloth? (partly OPEN)

- B exists only if **gate = 1** (§2.2). Gate needs `player+0xb8 == 0`, `+0xd5 == 0`, `cVar9 == 0`, `!bVar35`, `iVar12 == 0`. Their values for a normal player are **OPEN** (need the player object or a live read).
- Activation (+0x14) only if `FUN_141a83c60(player, 1)` runs (vtable +0x18, caller OPEN).
- SIL (+0x15) only if set by `FUN_141a85a00`/message 0x1c/creation.
- **Verdict:** a normal match runs the cloth sim iff gate = 1 **and** activation ran **and** SIL = 1; otherwise object A (nocloth) is drawn and B is inert. The three factors are CONFIRMED; the values for a normal match are **OPEN** (activation caller, and live values of `+0xb8/+0xd5/+0xc0/+0xd4`).

---

## 3. vfxdb reader (item 3)

### 3.1 Loader by resource name (CONFIRMED)

- VfxBinaryFile vtable `0x1429ddcf8`; VfxEntityFile vtable `0x1429ddc88`; VfxResource vtable `0x1429e6580`. TypeDescriptors from RTTI names `.?AVVfxBinaryFile@fx@fox@@` (0x14366f3c8), `.?AVVfxEntityFile@fx@fox@@` (0x14366f398), `.?AVVfxResource@fx@fox@@` (0x14366ff90). COL→vtable resolved by MSVC x64 COL scan (raw).
- Factory `FUN_1415d3990` @0x1415d3990 (dec5 13-229): if resource name `pcVar1 = *(param_2+0x28)` begins with `v f x` → allocates **0xA0 bytes**, `FUN_1415d3240` (sets VfxBinaryFile vtable, dec9 242-256); else → **0x90 bytes**, VfxEntityFile vtable `0x1429ddc88` (dec5 97-151, 178-205). So the choice is by **name prefix**, not by the extension table index.
- Type code for the B-object base: `FUN_1402af9e0(obj, param_2, 0xe000d)` (dec5 49).

### 3.2 Entry point and reader (CONFIRMED)

- VfxBinaryFile slot 9 = `FUN_1415d33e0` @0x1415d33e0 (vtable 0x1429ddcf8 +0x48 = 0x1415d33e0; dec9 54-146). It calls **`FUN_14161c8b0`** @0x14161c8b0 (body `[0x14161c8b0, 0x14161cdc0] [0x14161cf6c, 0x14161da0b]`, dec10), the parser that takes the buffer `param_5` and size `param_6`, and fails with `0x8b72dc02` if `size < 0x10`.
- VfxEntityFile slot 9 = `FUN_1415d35d0` @0x1415d35d0 (dec9 149-196): it calls `FUN_1402c4140` (resource id), `FUN_14161b6c0` (registry lookup keyed on `param_1+0x70`), then `FUN_14161ba20` (VfxResource registry insert, dec5 280-445). **Not** the file parser `FUN_14161c8b0`.

### 3.3 Extension table (CONFIRMED)

- `0x143445c90`: array of `const char*` with **8-byte stride** (not 16). Index 10 = `0x143445c90 + 0x50 = 0x143445ce0` → `"vfxdb"` (0x142590808). Index list: fclo, clo, lng, uig, uil, uif, uia, fnt, utxl, uigb, vfxdb, rbs, aia, …
- Only one qword in the image points at the table (the `"vfxdb"` pointer itself at 0x143445ce0). No qword stores the table base.
- No xref to `0x143445c90`/`0x143445c98`/`0x143445ce0` (MCP, empty). Raw RIP-relative scan (all REX, modrm mod=00 rm=101, imm 0/1/4) with targets in `[0x143445b90, 0x143445dd0]`: **0 code hits** (3 hits, two are `.rdata`).
- Verdict: **the table is not referenced by code in this image**. The loader does not use it (3.1). The `vfxdb` index-10 claim in the prior doc is correct as data but **irrelevant to the loader**.

### 3.4 Header and count (CONFIRMED / INF)

`common_effect.fpk` (14048 B, `pesfmt_dump cat`), member `vfdb` at byte 592 (raw). Header bytes:

```
+0  76 66 64 62            'vfdb'
+4  00 1f 00 51 50 79 70 e7   (u16 at +5 = 0x001F)
+8  07 00 00 02 00 00 01 00 10 00 01 00 10 03 00 ...
```

- Reader (dec10 88-89): `uVar12 = *(ushort*)(buf+5)`, `pbVar14 = buf+0xF`. u16 at +5 = **31**. Body starts at `+0xF`. `uStack_18c = *(ushort*)(buf+7)` = 0x5051 (not a count; only copied into `uStack_190`).
- **31 = instance count (INF, from the reader's loop bound `if (uVar12 != 0)`, dec10 211-215).**

### 3.5 Record grammar (CONFIRMED from the reader; schema resource OPEN)

- Schema/class table `lStack_168` = `*(lStack_1d0+0x60)` where `lStack_1d0` is the object found by name hash **`0x4e0ffd4a54c66e8`** (`FUN_140289d40`, dec10 96-102). Section header: u16 at +5 = class count (`< 0x1000`), records from +7 (dec10 109-111, 126-158).
- **Class record (CONFIRMED)**: header 0x14 bytes = `u64 key` (+0), `u16` (+8), `u32` (+0xA), `u32` (+0xE; top 4 bits must be 0), `u16 child_count` (+0x12); then `child_count × 9` bytes of children, each `u8 kind` (cases 0..6) + 8-byte value (dec10 154-158, 265-283). Record size is therefore **0x14 + 9·n**, not fixed.
- **Instance (CONFIRMED)**: after the header, for each of the 31 instances: `u64` class key (8 bytes at body pointer), looked up in the class list (dec10 249-252), then `u8 n` (`bVar2 = *pbVar14`) and `n` values, each typed by the class child `kind` 0..6 (dec10 258-284). **Value sizes per kind are OPEN** (switch bodies at code_r0x00014161cdc1…ceb0, not read).

### 3.6 Count and layout, verdict

- **Count: 31 instances** (CONFIRMED value at +5; role INF).
- **Record layout: partly decoded.** Header and class record CONFIRMED; instance value encoding per kind OPEN. The raw file shows `00 10 00 01 00 10` patterns at several offsets (prior doc), consistent with `(u16,u16,u16)` fields but not verified.
- **Schema resource** (hash `0x4e0ffd4a54c66e8`) is **OPEN**: I have not located its file. It is presumably loaded from another resource in the install.

### 3.7 Route 2 for B7 (vfxdb not needed): NOT PROVEN

To prove it I need the reader of `effect_config.xml`. Findings:
- No literal `effect_config` string in the image (plain or UTF-16). `config.xml` appears at 0x142b8fe2d (`model/bg/bill/config/config.xml`) and 0x142c6cf3a (`character_config.xml`): not the effect config.
- `effect_st018_df.fpk` is a literal at 0x1437d1f64, and `st%03d` at 0x142b8fe2c area. So the effect path is **composed at runtime** from fragments. The XML name is a member of the fpk per the prior doc.
- Verdict: **OPEN**. Next step: find the function that formats `effect/#Win/effect_st%03d_%c%c.fpk` (search xref to the `st%03d`-style format string near 0x142b8fe2c), then the member reader that opens the XML and the `EffectSetting` parse, then check whether placement fields reference the vfxdb name hash.

---

## 4. Files and tools

- Decompiles: `dec1.txt` (cloth enclosing function, stadium list reader, match loader), `dec2.txt` (singleton/slot helpers, match builder), `dec3.txt` (flag builder f4a80, f6ae0), `dec4.txt` (SIL/selector helpers), `dec5.txt` (vfx factory, registry), `dec6.txt` (activation loop 0x141a88c90), `dec7.txt` (activation mode 0x141a83c60, SIL callers), `dec8.txt` (message dispatcher 0x1419c4260, variant caller), `dec9.txt` (vfx slots), `dec10.txt` (vfx reader 0x14161c8b0).
- Live scripts: `liveread.py`, `livescan.py`, `liverun2.py` (vectorized run scan), `ptrfind.py`. All read-only via `sudo -n` + `/proc/<pid>/mem` `pread`.
- Install reads: `build/apps/pesfmt_dump cat|ls` (no writes).
- No build, no ctest, no frames.

## 5. Open items for the next pass (ranked)

1. **X (set source)**: find the owner object of `S = owner+0x28` in `FUN_1414f5120` (its param_1, caller xref 0x1414f6cca in an unnamed function) and the loader writing `X+0x48/+0x50`. Then check 54 in S for Roma.
2. **singleton+0xf0 writer** (`_DAT_1437f5d50+0xf0`): find the match-setup writer. Candidates: callers of `FUN_1414f5120` output, or `FUN_141f500d0` callers that write `+0xf0`.
3. **Player vtable +0x18 caller** (activation): message dispatcher `FUN_1419c4260` cases `0x1d`…`0x2x`, and `FUN_141a1fb50`, `FUN_141a1fd20` (not decompiled).
4. **Schema `0x4e0ffd4a54c66e8`**: locate its resource and the child-kind value sizes (dec10 switch bodies), then decode `vfx_class_db` instances fully.
5. **Route 2**: `effect_st%03d` format-string xref → fpk member → XML reader.

Roma status: **OPEN**. Cloth status for a normal match: **conditional** (gate × activation × SIL, §2.6). vfxdb: reader **found** (`FUN_14161c8b0`), count 31, record grammar partly decoded, schema OPEN.
