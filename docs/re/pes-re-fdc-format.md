# FixDemo `.fdc` cut file: container and records, decoded to what Godot playback needs (CutsceneHybrid, 28-10-26)

Parser: `tools/dev/cutscene/fdc_parse.py` (committed, `57d95744`). Input: the mounted VFS through `pesfmt_dump` (`ls` and `cat`), or a directory of extracted files. Output: JSON in scratch (PES-derived, never committed). Run: `fdc_parse.py --pes21-root <install> --pesfmt <pesfmt_dump> --json <scratch>`. The oracle is the coverage check: every byte of a file must be claimed by exactly one parsed structure, with zero-valued alignment padding counted apart.

Labels: CONFIRMED = read from install bytes by script, or parser output; INF = inferred, reason given; OPEN = blocker named.

## 0. Result on the install (CONFIRMED)

- `ls` of `common/demo/fixdemo/<cat>/cut_data/` over the nine categories: **4,925 `.fdc` files**, all parsed without error.
- Records by class over all files: Cut 2,428; Env 1,444; Timing 3,507; Effect 5,881; Human 6,924; Object 4,830 (type 5); Camera 9,302; Function 624; HumanControl 3; Disabled (type 99) 15. The class counts match `pes-re-cutscenes.md` §3.2 except Object (see §3, size variants).
- Coverage: **4,910 of 4,925 files consume every byte.** 15 files do not (§5, OPEN).
- Padding: 39,509 zero bytes are alignment padding inside cut blocks (CONFIRMED by the pad rule of §2.3).
- Resources: 14,999 external (4-byte stub, loaded from the VFS), 13,031 embedded.

## 1. Container (CONFIRMED on 4,925 files)

```
u32  n
n x { u32 offset, u32 size, u32 nameOffset }     # offsets from the file start
```

- Entry 0 is the **cut block**. Its name is the file's own path (`cpk_dat/common/demo/fixdemo/<cat>/cut_data/<name>.fdc`). The name is a NUL-terminated string at `nameOffset`.
- Entries 1..n-1 are **resources**, named by path (`cpk_dat/…`). `size == 4` means external: the stub is read from the VFS with `cpk_dat/` stripped. Any other size is embedded: the bytes follow, with their own magic (`.gani`, `.seq`, `.canm`, `.ask`).

## 2. Cut block (CONFIRMED)

### 2.1 Table and records

```
block+0x00  u32 m
block+0x04  m x { u32 offset, u32 size, u32 typeOffset }   # offsets from the block start
```

- Record `i` is the bytes `[block+offset, block+offset+size)`. Its class is the byte at `block+typeOffset`.
- Record classes: 0 Cut (64 B nominal), 1 Env (156), 2 Timing (92), 3 Effect (188), 4 Human (364), 5 Object (444), 6 Camera (284), 7 Function (324), 8 HumanControl (96), 99 Disabled (444, dropped by the loader's switch, `FUN_141f36200`).

### 2.2 Type pool (CONFIRMED)

The type bytes sit at the **end of the block**, after the records. Each record's type byte is referenced by its `typeOffset`. Between type bytes there are zero bytes that no record references. The pool begins at `min(typeOffset)`. The block is then padded to its declared size.

### 2.3 Padding rule (the coverage oracle)

An unclaimed byte is **padding** only if it lies in `[pool start, block end)`, is zero, and is not referenced by any `typeOffset`. Any other unclaimed byte is a **gap**. A referenced type byte that is not claimed is also a gap. The parser builds the referenced set from the table itself, so dropping a claim is caught (mutation, §6).

## 3. Record layouts (field offsets from `pes-re-cutscenes.md` §3.2; parser decodes types 0, 2, 4, 5, 6 with bounds checks)

Times are 1/60 s ticks. Positions are metres (Y up); angles are degrees.

| type | class | nominal size | counts (install) | fields Godot playback needs | layout status |
|---|---|---|---|---|---|
| 0 | Cut | 64 | 2,428 (28 files at 56 B, 9 at 60 B) | id, cutStartTime, minTime, maxTime, playSpeed, movingDistance, cameraPattern, offset, offsetPos, offsetRotY, offsetFlipX, humanHeightRange(+Center), functionNo | CONFIRMED by doc; size variants bounds-checked |
| 2 | Timing | 92 | 3,507 | word0 (high 16 bits = timing type, low 16 = id/order: INF med), startTime, endTime, targetNo, paramNo1/2, value, actionName (char[64]) | doc |
| 4 | Human (actor track) | 364 | 6,924 | slot (u16, binding), disp, position, rotY, animePath (.gani), animeFacePath (.seq), animeStartTime, correctRateForHeight, functionNo, IK targets and on/off times | doc; IK byte pairs INF med |
| 5 | Object (prop/actor track) | 444 | 4,830 (4,410 at 444 B + 420 at 440 B) | slot, disp, position, rotation quaternion, modelPath (.fpk), skeletonPath (.ask), animePath, animeStartTime, parent kind/no/bone (INF med) | doc; 440-byte variant lacks the last 4 bytes |
| 6 | Camera (camera track) | 284 | 9,302 | key (map key, not a start frame), startCutTime, originalCameraId, envDataId, animePath (.canm), animeStartTime, nearClip, farClip, nearClipScale, zoom[8] (ticks, −1 = off), targetKind/targetNo, selectByRate, DOF block (raw) | doc |
| 1 | Env | 156 | 1,444 | id, lightPath (char[128]); 39 flag bytes from 0x82 | PARTIAL: flag order is the doc's, offsets INF med |
| 3 | Effect | 188 | 5,881 | (not decoded) position, rotation, scale, type, timing, life, speed, colours | OPEN: field offsets not in the doc; kept as hex |
| 7 | Function | 324 | 624 | (not decoded) function-code arrays | OPEN: kept as hex |
| 8 | HumanControl | 96 | 3 | (not decoded) IK control floats | OPEN: kept as hex |
| 99 | Disabled | 444 | 15 | (dropped by the loader) | CONFIRMED that the loader drops it (doc §3.1); see §5 |

### 3.1 Size variants (CONFIRMED by parser output)

- Object: 420 records are **440 bytes**, not 444. The doc's table gives Object as 4,410 at 444 B; the install has 4,830 type-5 records in total. The 420 difference is the 440-byte variant. This corrects the doc's count, and the parser bounds-checks the last field.
- Cut: 28 at 56 B and 9 at 60 B (the doc's "37 older files", matching).

## 4. Godot playback: what each part of the file gives (INF where noted)

- **Layers.** A cut file is one layer. A variant (from the selection table, `pes-re-cutscenes.md` §4) names up to 10 layers that play together: base actors, camera, crowd, effects, props. The layer set comes from the selection, not from the file.
- **Actor tracks.** Type 4 (Human) and type 5 (Object) records. Binding is by `slot` (0 = subject, 11–21 away side, 22–25 officials, 26 coach, per the doc's §0 slot ranges). The slot-to-match-player remap is OPEN (`pes-re-cutscene-roles.md` §7 item 5).
- **Camera tracks.** Type 6 records, keyed by `key`, starting at `startCutTime` in ticks. The `.canm` at `animePath` is the camera path (CANM v1, 30 fps, six channels, per the doc's `camera_cut.py` reference, not re-derived here).
- **Timing.** Type 2 records give events: `startTime`/`endTime` in ticks, and `actionName` for sounds (e.g. `demo_goal_level3`). The timing type/id split in `word0` is INF med.
- **Asset references.** External resources (`size == 4`): the path with `cpk_dat/` stripped, read through the VFS, so a mod that wins the mount is the one read. Embedded resources (`.seq`, `.canm`, some `.gani`, `.ask`): their bytes are inside the file and the parser reports only their size and magic.

## 5. Open (OPEN, with what would settle each)

1. **15 files do not consume every byte** (all type-99 `Disabled` records, in `ent_009_*` and similar). For each, the type byte at the record's `typeOffset` is `0x63` ('c'), the first char of the file's own path, which begins 4 bytes before the entry-0 name pointer. So the name pointer reads `dat/common/…`, and the 3 bytes `pk_` are claimed by nothing. Settles by: a byte-level check of the block tail against the loader's name read (`FUN_141f36200`); the parser then needs either a rule for a type byte that aliases the name, or the loader's own name offset. Until then these 15 report `ok: false`.
2. Effect, Function and HumanControl field offsets are not in the doc. Settles by: decoding `FUN_141f3e050`, `FUN_141f3e530` and `FUN_141f3e6d0` (dec.sh) against the type-3/7/8 records.
3. Env flag offsets from 0x82 and the Camera DOF block: offsets are the doc's order (INF med). Settles by: the same decode as item 2 for `FUN_141f3e1f0` and `FUN_141f3dd00`.
4. The timing word split (`word0 >> 16`, `& 0xffff`) and the Human IK byte pairs are INF med.
5. Embedded `.gani`, `.seq`, `.canm` and `.ask` bodies are not parsed (out of this format's scope; the `.canm` reader is in `camera_cut.py`).

## 6. Checks run (CONFIRMED)

- `fdc_parse.py --selftest`: a file built from the container layout parses with zero gaps; three stray bytes appended are reported as a gap of 3.
- **Mutation** (run on a scratch copy, repo file untouched): the record type-byte claim removed. Selftest fails: `gap_bytes: 1, gap_offsets: [44]`. Reverted by never editing the repo copy.
- Install run: 4,925 files, 4,925 parsed, 0 parse errors, 15 coverage failures (§5.1), 719 s with three `pesfmt_dump` readers.
- Not checked: that decoded field values are right for playback (no replay oracle for `.fdc` yet). Cross-check with `scene_map.py`: its 485 `mode` references (21 distinct names) point at `.fdc` files the install does not mount; this parser never sees them.
