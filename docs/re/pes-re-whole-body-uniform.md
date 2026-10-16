# PES 2021 whole-body uniform parts — evidence report

Scope: how uniform parts are chosen and drawn for whole-body players (live W
players are whole-body boots-pack models: 42 bones, `dsk_hip`), and where the
markings land on them. Read `pes-kit-assembly.md` (§3 assembly, §8.4 draw
lists) and `pes-re-markings.md` (§1 data flow) first; names below reuse theirs
(`FUN_141a89120` = the uniform assembly, `FUN_141a8b7b0` = the marking/binding
pass, `FUN_141a86310`/`FUN_141a85fe0` = the shirt/pants binders).

Legend: **OBS** = observed (decompile, bytes, or examined frame, and how);
**INF** = inferred, with confidence. Image base 0x140000000; decompiles below
are from the PESLive full-image project via `Unpacker/dec.sh` (read-only),
outputs in this task's scratch dir.

---

## 0. Headline

1. **OBS. PES builds uniform parts for every outfield player, whole-body or
   not.** `FUN_141a89120` (1,225 decompile lines) reads only the kit config,
   the per-player appearance struct and the sleeve/tail/captain state. It
   contains no boots-pack consult, no bone-name test and no BootsUnit
   cross-check: a full-text scan of the decompile finds no `boots`, `glove`,
   `face`, `head` or `dsk_hip` reference of any kind.
2. **OBS. The marking pass binds by part category, over the UniformUnit part
   list only.** `FUN_141a8b7b0` iterates `param_1+0x38..0x40` (the UniformUnit
   parts): shirt marking (`FUN_141a86310`) where the part's category
   (`+0x40`) is `< 2`, pants (`FUN_141a85fe0`) where it is `== 3`. It never
   touches BootsUnit. Had PES skipped uniform parts for whole bodies, the
   marking textures would bind nowhere.
3. **OBS. PES draws a name and back number on W backs** (decoded OBS frame,
   this report §4). By (1) and (2) those markings can only arrive through the
   normal uniform shirt part — so the shirt part is built and drawn for W.
4. **OBS. The `dsk_hip` string referrers are animation code, not a part
   gate.** The two code references to the `dsk_hip` bone-name strings
   (`0x14020c4fc`, `0x14020cb89`) sit in SSE pose/physics code, not in any
   character-assembly path.

Consequence for GF (B5): skipping `SelectUniformParts` for whole bodies is
wrong. The fix builds the uniform parts for every player and keeps the boots
body and the face-pack head. No importer change is needed: the boots fmdl
already parses (42 bones incl. `dsk_hip`) and the Suit UVs already sample the
kit atlas.

---

## 1. The assembly reads no boots state (OBS)

`FUN_141a89120 @ 0x141a89120`, body
`[[0x141a89120, 0x141a89575] [0x141a89956, 0x141a8b5f7]]`:

- Head (decompile l.115–200): `iVar12 = *(int *)(param_1 + 0x68)` (unit kind;
  0 = outfield path); kit node via `FUN_1419a4c50/5da0/5150/5690`; sleeve
  pairs via `FUN_141963790/61730/637b0/61750` selected by `+0xc0` (long-sleeve
  flag); glove presence via `FUN_141a80010 → +0xd6`; captain via
  `FUN_141a9a5750`; preview via `FUN_141a7ff50`; shirt/pants flags via
  `FUN_141a7ff90` (`bVar33` = shirt worn, `uStack_204` = pants state).
- Slot loop (l.298–418): dispatches on the 15-slot table hashes
  (`0x506226d207df` collar, `0x7575fa4e9626` shirt_in, `0xb69167e9ca5b`
  shirt_out, `0x1158d590d986` sleeve, `0x1efca60661a9` sleeve_cap,
  `0x335cade007ee`/`0x582a734dcac9` secondary pair, `0xf3fdfc929c53` socks,
  …), each gated only on the flags above plus referee kind (`+0xb8`) and
  tight kit (`+0xd5`). Every path-built slot goes through the same
  open-or-empty-slot load (kit-assembly §8.9); on failure the slot is
  abandoned with no substitute.
- Callee census (34 distinct `func_0x…` targets): kit getters, sleeve-name
  builder `FUN_1419637c0` (via `FUN_141a83820`), cloth constructors,
  arm/thigh selectors. No boots (`FUN_141a8e8d0` family), glove, face or head
  loader appears — boots/gloves load through the separate per-unit path
  (`FUN_141ec3a20` → `FUN_141a8e8d0`, BootsUnit/HeadUnit), never from this
  function.

The per-character units are independent (kit-assembly §8.4): UniformUnit,
ArmUnit, HandUnit, ThighUnit, BootsUnit, HeadUnit coexist on every character.
Nothing gates UniformUnit creation on BootsUnit content.

## 2. The marking binders touch UniformUnit parts only (OBS)

- `FUN_141a8b7b0 @ 0x141a8b7b0`, body `[[0x141a8b7b0, 0x141a8c069]]`:
  after the kit-change checks (shirt job `FUN_141a82a70`, leg job
  `FUN_141a829c0`), the per-part loop reads each UniformUnit part's category
  at `part+0x40`: `< 2` → `FUN_141a86310` (shirt: `Marking_Tex_LIN`
  0x16f09b71 + `BNumber/FNumber/NameScaleXParam` + badge matrices), `== 3` →
  `FUN_141a85fe0` (pants: `Number_Tex_LIN`). The part-model handle comes from
  the part's own vtable (`+0x60`); the BootsUnit model (`BootsUnit+0x78`) is
  never read here.
- `FUN_141a86310 @ 0x141a86310`, body `[[0x141a86310, 0x141a866c3]]`: bails
  (binds null) only when the unit has no kit (`*param_4 == 0`), no kit node,
  or `param_3 != 0` (the `+0x1a8` referee-kind/number gate, markings §1).
  Whole-body state appears nowhere.
- Param-hash byte scan of the unpacked image: `0x16f09b71`
  (`Marking_Tex_LIN`) occurs exactly 3× — `0x141a85b77` (in `FUN_141a85b30`),
  `0x141a8605d` (in `FUN_141a85fe0`), `0x141a969bf` (in the menu/replica
  binder `FUN_141a96d50`). No fourth binder exists for a body mesh.

## 3. What a W boots body carries (OBS, this install's VFS)

Player 76020's save look (re-hashed owner save: long sleeves, socks 2, tail
in, glove 1578, boots 1595) loads `boots/k1595/boots.fpk`:

- `boots.fmdl`: 42 bones incl. `dsk_hip`; 6 meshes — `Skin` (2721 verts),
  `Suit` (4869), `boots` (690), `Patches` (195), 2× `Outline` (dropped, the
  non-render pass).
- `Skin`/`Suit` are `fox3DFW_ConstantSRGB_NDR` (unlit); `Suit` Base is the
  `dummy_kit` placeholder, which GF rebinds to the team's kit atlas
  (`u0760p1`, 2048² DXT5, opaque). `Suit` UV0 spans u∈[0.005,0.994],
  v∈[−0.975,−0.045] — the full atlas with negated v (the mesh's DX
  top-origin convention; Godot wraps it, landing on the same navy torso the
  PES frame shows).
- The face pack stays the head: `hair_high` (604 verts, the 0.6×0.4 m monitor
  box at y∈[1.50,1.90]); `face_high` is degenerate (all verts within 1 cm at
  y≈1.55) and contributes nothing visible.

With uniform parts restored, the shirt (`uni_shirts`, opaque) covers the
Suit torso and carries the markings; the Suit's visible contribution is the
skin arms (long-sleeve look builds no `arm` parts, kit-assembly §8.4), the
shorts region (`pants_016` is a 4cc degenerate stub) and boots/patches.

## 4. Visual confirmation (OBS)

- Decoded OBS match frame (BNT 34 – 55 W, 228:02): a W player from behind
  wears a navy shirt with a visible back number and name, a red/orange hem
  band, white shorts and navy socks, with the monitor head. Markings on a
  whole-body back are therefore drawn by PES — only reachable through the
  uniform shirt part (§1–2).
- Viewer (this branch): team 760, kit 0, face 76020 with the save look —
  before (uniform skipped): navy Suit torso, no number or name; after
  (uniform built): shirt with number 20 and name on the back, matching the
  PES frame's placement. Frames: `wmark` (before, body only) vs the fixed
  build; the PES reference is the decoded crop `pes_w_back.png`.

## 5. Killed hypotheses

- **"PES skips uniform parts when the boots pack carries a body skeleton"** —
  killed. No boots consult exists in `FUN_141a89120` (§1), the marking pass
  cannot reach a body mesh (§2), and PES draws markings on W backs (§4).
- **"The Suit's `dummy_kit` means PES binds nothing there"** — refined. PES
  binds kit-atlas-family textures only into `uni_*` materials (§2, kit
  report §8.6: the binder iterates the part's own material list). What PES
  draws on the ConstantSRGB Suit is still OPEN — but GF rebinding it to the
  team atlas reproduces the PES navy torso (§4), so the question does not
  block the fix.
- **"The white torso in the viewer is a lighting fault"** — killed. It was a
  wrong-subject artefact: without the EDIT save, boots id 0 resolves to the
  `k0062` default (vanilla boots), no body loads, and the vanilla shirt
  renders untextured-white. With the save look the torso is navy.

## 6. Open

- What PES binds to the whole-body Suit's `dummy_kit` (uniform binder skips
  non-`uni_*` materials; the Suit still shows the kit pattern in PES
  frames). INF (high): the atlas, as GF does — visually verified here.
- Whether PES draws the Suit torso under the opaque shirt (z-order/fight at
  close range). Not visible at match distance in either frame.
- The `u####p0` → current-kit swap in 4cc face/boots packs (kit-assembly
  §8.6) — unchanged by this fix.

Scratch: `dec_89120.txt` (assembly), `dec_builders.txt` (`FUN_141ec3a20`,
`FUN_141a80b80`, `FUN_141a8e8d0/670`), `dec_units.txt`, `dec_boots.txt`,
`dec_callers.txt`, atlas/dummy PPMs and viewer/PES frames.
