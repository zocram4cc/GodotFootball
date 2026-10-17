# Keeper glove g1398: does PES draw it solid? (KeeperGlove, round 3)

Result: **not resolved against PES.** No PES reference of g1398 was obtainable. Data-level finding: the g1398 fmdl triangles leave an enclosed see-through region in the palm plane. Any renderer that draws this mesh shows it, culling aside. So PES is hollow in the same view if it draws the same list triangles with no extra geometry. That is [INFERENCE, high]. It is not visually verified. No GF code changed; `GloveRule-held` is not merged.

## 1. PES reference: none obtainable

- Main confirmed no PES frame of g1398 is on disk.
- Live PES (OBS `PES-vk`, decoded, `now0_dec.png`, `now1_dec.png`): HUD reads BNT 0–1 W at 52:30. Field figures are about 40–60 px tall at 1280×720, so a glove is under about 5 px. No keeper close-up was on screen in two captures. The team-752 keeper is not identified in this match. Wearer ids (glove ids of BNT and W keepers) were not read.
- Verdict: a glove close-up needs a replay or a camera change. Neither is available read-only. The PES side stays open.

## 2. Data: g1398 has an enclosed hole in the palm plane

Method (scratch `KeeperGlove-scratch/cover.py`, `enclosed.py`, `raycast2.py`, `glovescan/`):
- Decode via `libpesfmt` (`ParseFmdl`), with the list topology the parser uses. The index count is a multiple of 3 (3441 for the main mesh, 1147 tris). Adjacent triangles share edges in 22 mm quads, so list reading is coherent.
- Project the rest-pose vertices onto the two largest-variance axes, looking along the thinnest axis. Rasterise. Flood-fill uncovered cells from the border. Cells not reached are enclosed holes.

| Mesh (rest pose, thin-axis projection) | verts | tris | enclosed hole, % of silhouette |
|---|---|---|---|
| g1398 `glove_r` main (and `glove_l` main, same) | 1515 | 1147 | **13.0%** (N=200), 12.9% (N=120) |
| g0001 vanilla `glove_r` / `glove_l` | 553 / 555 | 794 | 0.0% |
| g1578 W glove (solid in GF) | 1848 | 3193 | 0.0% |
| g1400 outfield glove | 514 | 702 | 0.0% |

- Ray test (`raycast2`): 40 of 40 random hole cells have zero triangles along the thin axis, through either face. Covered cells have 1–2+ hits. Culling cannot reveal a triangle that is not there.
- Population (`glovescan`): 802 main meshes in 410 glove packs. Median enclosed fraction 0. Above 1%: 12 meshes. Above 10%: 4 meshes, all g1398 (×2) and g0887 (×2, 12.5%, 20000-tri mesh, not checked). g1398 is the largest hole among the common 4cc gloves.
- Open undirected edges (edges used once): g1398 1431, g0001 296. The earlier doc's directed metric gave 689 vs 148. Same ~2× ratio.
- Small meshes: `glove_l` mesh 1 of g1398 is 30 verts, bbox diagonal 9.4 mm. It is not palm.
- Hole location in the rest pose: centre at 0.23 of the long axis from the wider end. It spans 0.09–0.85 of the length and is enclosed. Anatomy (palm vs back) is not bone-verified.
- Asymmetry (open, not explained): `MESH_INFO` `+0` (u16) is 0 for `glove_l` main and 416 for `glove_r` main. 416 also marks the 30-vert cluster in vanilla. Its meaning is unknown and it is not a cull flag by evidence.

## 3. Cull: Blin is back-face culled by naming; decode not completed

- Technique: `basic_L_fx` binds `fox3DDF_Blin` → shader `fox_3ddf_basic_nrmuv` (fmdl dump). No `_NC` variant exists.
- `pes-re-shaders.md` §3.1: `_NC` = no cull. Non-NC techniques cull back faces (`pes_character.cpp:346`). [INFERENCE, high; not decoded from the RasterizerState]
- So GF's default back-face cull matches the naming. `cull_disabled` (prior round) is a what-if, not PES-faithful.
- Hash decode (`StrCode64`, `libpesfmt`): `fox3DDF_Blin` = `0x19223e37b2de2311`, 48-bit `0x3e37b2de2311`. It appears in a 48-bit table at VA `0x143600240` (file offset `0x3600240`, rw data). Its one code xref is `FUN_1423c46d0` (returns the table). Callers: `FUN_141844630` (resource lookup, generic) and `FUN_141905530`. The RasterizerState decode was not reached. Stopped: the table is a generic registry, and the cull path needs more budget.

## 4. GF path

- No code change. No worktree created. No test run (no change; `test_player.gd` not run).
- The palm hole is in the asset data. Closing it in GF means a weld or fill with no PES site. That is not PES-faithful and is not done.
- Owner decision (unchanged from round 2): keep bare hands for the keeper, or accept the hollow glove, or add a PES-cited fill once a PES frame exists.

## 5. Acceptance status

- PES reference (frame or decode) of g1398: **not met.**
- GF after-frame with solid connected gloves: **not met** (no code change; the data is open).
- Proven "PES draws it hollow too": **not met.** Data-level proof only (§2), plus the naming cull (§3). The PES side needs a close-up frame.

## 6. Open items

1. A PES close-up of team 752 GK (glove 1398). Needs team 752 on pitch or a close-up replay.
2. Decode `fox3DDF_Blin`'s RasterizerState (cull) from the render path. Starting point: table `0x143600240`, `FUN_141844630`.
3. Confirm PES uses `TRIANGLELIST` for the fmdl index buffer (the draw call). Data strongly favours list (multiple-of-3 counts, shared-edge quads).
4. `glove_r` main `MESH_INFO +0 = 416`: meaning unknown.
5. `g0887` (12.5% enclosed, 20000 tris): not checked. Could be a second data-level hollow glove to compare in PES if one is ever seen.

## Tests and mutations

None run. No code changed.
