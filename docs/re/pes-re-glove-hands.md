# PES glove-replaces-hands rule (GloveRule)

Result: Code and tests Done (branch `GloveRule`, `61e56f9`). Visual acceptance:
outfield MET (one hand set, connected); keeper NOT MET (glove geometry and wrist
connection unverified); whole body unframed.

Commits (`git ls-remote origin GloveRule` = `61e56f9`):
- `64dd3eb` Red: tests only (GloveReplacesTheBareHands, test_player gloved check, glove_id_of binding).
- `61e56f9` Green: uniform.cpp (hands only when no glove), pes_character.cpp (glove pack for whole bodies).

## Decode (exe, image base 0x140000000)

Legend: OBS = decompile or xref read; INF = inferred, reason given.

1. **OBS. The uniform builder has no hand slot.** `FUN_141a89120` (0x141a89120) slot
   loop has no `hand_l`/`hand_r` hash. It sets `+0xD6` (glove present) from
   `FUN_141a80010` (0x141a80010, reads `A+0x34`, the glove id). Decompile l.153-158.
2. **OBS. The appearance applier passes the glove id to the glove loader.**
   `FUN_141a1e6e0` calls `FUN_141a8e8d0(unit, func_0x141a03a50(param_2), 0, mode)`.
   Call site VA **0x141a1ed1e** (xref). `func_0x141a03a50` returns
   `*(int*)(*(param_2+0x10)+0x168)`. INF (high): `+0x168` is the appearance glove id.
   Not traced to its writer. The live glove folders match (see frames).
3. **OBS. The loader makes the glove the hand's source.** `FUN_141a8e8d0`
   (0x141a8e8d0): when the id is nonzero it appends `g%04d` (format string
   `0x142aadc54`) to the hand's model name, then stores it under key
   `_DAT_143517c50` via `FUN_141a8e370` (0x141a8e370). Id 0 keeps the bare name.
   One slot, two sources: glove folder or bare hand. Its other callers are
   `FUN_141a71030` (0x141a7125f, 0x141a71279).
4. **OBS. Arms switch to glove variants by `+0x9e`.** `FUN_141a8d530` (0x141a8d530):
   `arm_glove` = hash `0xe257a221bc51`, `arm_glove_cap` = `0xf522d97f4720`, `arm_half`
   = `0x2825fb8e75b`. Its 4-entry loop at `0x143517c20` covers arm/arm_cap and the
   secondary pair (gate `+0x9f`), not the hands.
5. **OBS. Gloves are not gated by the body.** The glove id is the only id that
   reaches `FUN_141a8e8d0` from `FUN_141a1e6e0`. No boots accessor is among its ~40
   appearance reads. INF (medium): boots are loaded on a separate path
   (`FUN_141ec3a20`, per kit-assembly 8.3). So a whole body wears its glove too.
   The GF gate `!wholeBody_` on the glove pack was GF's own and is removed.

Live cross-check (kit-assembly 8.4, State 4): GK 15 with glove 1400 draws
`glove/g1400` `glove_l`/`glove_r` in place of the hands, and a missing glove folder
leaves no hands (no fallback). The port keeps that.

## Code

- `src/pes/uniform.cpp` `SelectUniformParts`: `hand_l`/`hand_r` only when
  `look.gloveId == 0` (comment cites the decode).
- `src/gdext/pes_character.cpp`: glove pack loaded for every player with a glove
  id, whole bodies included. Missing pack still leaves the slot empty.
- `src/gdext/pes_library.{hpp,cpp}`: `glove_id_of(face_id)` binding (test input,
  mirrors `boots_id_of`).

## Tests

- `pes_uniform_test` `SelectUniformParts.GloveReplacesTheBareHands` (new).
  Red (old code, 64dd3eb): `FAILED` with `hand_l.fmdl`/`hand_r.fmdl` present for
  `long=false` and `long=true`.
  Green: `pes_uniform_test` with `PES21_ROOT=<workdrive>/PES21`: 37 passed, 0
  failed (SelectUniformParts 7/7).
- Mutations (each rebuilt, run, reverted):
  - M1 `if (look.gloveId == 0)` -> `if (true)`: `GloveReplacesTheBareHands` FAILED.
  - M2 -> `if (false)`: `BuildsTheShortSleeveDrawList` and
    `LongSleevesUseTheLongSlotAndWinterCollar` FAILED.
- `godot/tests/test_player.gd` gloved-look check (inside the EDIT-save block; the
  look is player 70301, team 703, kit 0, glove 1):
  - Red (old code, `git restore --source=64dd3eb`): exit 1,
    `player 70301 (glove 1) draws no bare hand: [... hand_l, hand_r ...]` and
    `wears its glove hands` failed.
  - Green: exit 0.
  - Build: `godotfootball` target, `godot/bin/libgodotfootball.so` rebuilt, GDEXT_EXIT 0.

## Frames (viewer, EDIT save, team 752 kit 0; Vulkan on RADV NAVI21, log clean)

Not like-for-like with a PES reference. The viewer overexposes outfield players
(pes-re-whole-body-uniform.md §7). Judged on hand shape and wrist joint only.

- **75215** (glove 1400, short sleeves, tail out; the doc's State-4 "15"):
  before: bare `hand_l`/`hand_r` as flat paddles at each arm end; after:
  `glove_l`/`glove_r` with fingers, joined to the same cuff. One hand set, no bare
  hands. Wrist continuous at 3x. **MET** by eye.
  Files: `frames/before_75215`, `frames/after_75215`, `frames/zoom_75215.png`.
- **75201** (glove 1398, boots 1377; the doc's team-752 keeper, tight kit):
  after: glove hands replace the bare ones, but the glove geometry is fragmented
  (boxy, wire-like fingers), the right glove renders as a dark block, and a dark
  rectangle sits at the left wrist junction. Connection **not established**.
  Files: `frames/keeper_75201.png`, `frames/zoom_75201b.png`.

## Open

1. Keeper glove render (75201): fragmented, dark glove geometry and an unconfirmed
   wrist junction. Check the glove pack's materials (`basic_L_fx` on `glove_l`/`glove_r`,
   `skin_color` on the hands) and whether PES binds those textures or the arm skin
   on the glove. Not resolved here.
2. Whole body with a glove (W players): decode-based change, not framed. Check
   whether the W body's Skin already carries hands before the glove pack adds
   them.
3. INF: `+0x168` is the appearance glove id (see decode 2). Trace its writer to close.
4. Stale doc text, not edited (repo rule: `docs/re/` is written only via
   `archive_re.sh`): `docs/re/pes-re-whole-body-uniform.md` §7 gloves bullet
   (l.182-184) says PES's glove rule is "not implemented"; `docs/re/pes-kit-assembly.md`
   §8.4 l.175 says `hand_l/r` selection "not traced (INF)". Both now decoded above.
5. The gate was not run (Main gates). Only `pes_uniform_test` and `test_player.gd`
   ran. `extern/godot-cpp` symlink makes `git status` and path-limited `git checkout`
   fail (use `git -c status.submodule=none restore`).
