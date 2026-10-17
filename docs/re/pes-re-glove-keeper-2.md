# Keeper glove g1398: hollow render, round 3 (GloveRule)

Result: **Not fixed. No GF-path defect found that makes g1398 solid.** Acceptance
(solid, connected close-up) NOT met. Branch `origin/GloveRule` = `9c33567`
(resolver fix from round 2 kept). No experiment left in the tree (verified clean).

## What the frame shows (close-up, distance 2.0 m, target-y 1.3, glove only)

`frames/cu1_go` (baseline): palm is a hollow hexagonal ring, fingers are thin
spikes. Main's reading is correct. The glove reads as an outline, not a hand.

## Tested and ruled out (all with VA-free numbers from the same mesh)

1. **Bind / pose mismatch (ruled out).** Elbow and hand maps agree exactly at the
   render wrist: `M_elbow(render wrist) = M_hand(render wrist) = (0.770, 1.467, 0.054)`,
   gap 0.000 m (`python` check on the rig's `align`/anim/render values). Every
   finger joint's residual `(anim_f - anim_wrist) - W_wrist (render_f - render_wrist)`
   is 0.000 (`resid`). Per-bone centroids of the vertices each bone drives sit
   2–5 cm from the rig joint (`bonecent`), the same as the bare hand. The glove
   binds to the same joints with the same transforms as the hand.

2. **Helper bone `dsk_forearm_*` (real defect, not the cause).** The rig's name
   table maps `dsk_forearm_l/r` rigidly to the elbow (`rig_names.hpp` l.127–128,
   via name-miss). `hand_l.skl` (render pose) puts `dsk_forearm_l` at
   (0.533, 1.132, 0.050), which is the elbow-to-wrist segment at t≈0.65 on all three
   axes (checked). `dsk_forearm_t_l` sits at the elbow (correct). Re-placing the
   forearm helper on the segment (`helpvar` mode `seg`, t=0.65, position and slerp
   rotation) leaves the glove's long-triangle count unchanged (391 of 1147) and makes
   the bare arm worse (12 → 541 long tris). Not adopted: the anim pose of this helper
   is not in any PES file I read (`body_anim_skel.ask` has 20 `sk_*` bones only;
   `body_high_skel.ask` and `hand_anim_skel.ask` have no `dsk_forearm`), so a segment
   placement would be an inference. Left as an open item.

3. **Winding per island (ruled out).** Over all undirected edges, zero are traversed
   in the same direction by both neighbours (`orient`), for the glove and the controls.
   Winding is consistent with normals. The glove is not inside out.

4. **Back-face culling (contributing, not the whole).** `render_mode cull_disabled`
   on `pes_blin` fills part of the palm (`cu2_nocull`), but the hexagonal ring
   stays. So culling hides some faces, and the ring is a separate effect.

5. **Material alpha / shader (ruled out).** `str` base texture: min alpha 255,
   0 texels below 128 (`alpha`). Blin shader has no discard except `alpha_tested`,
   which is off here. The W glove `g1578` uses `pes_skin` (no `render_mode`) and
   renders solid, so cull mode and alpha are not the difference between the two gloves.

6. **Bare hand under the glove (ruled out).** Drawing `hand_l`/`hand_r` with the
   glove leaves the frame identical (`cu4_both`). The bare hand does not cover the palm.

## What differs in the mesh (the real finding, OBS)

- **Open boundary edges:** g1398 has **689** edges with no reverse partner;
  g0001 (vanilla) 148, bare hand 178, g1578 (W, solid) 246 (`orient`).
  So g1398 is an open surface, with about 2× the open edges of the other meshes.
- **Long triangles in rest space** (no skinning involved): g1398 **391 of 1147**
  have an edge over 30 mm, worst 113 mm; g0001 127 (56 mm); bare hand 0 (`lbones`
  with `REST=1`). Long triangles are fan-like (index positions 9, 12, 18, 24, …).
- Index buffer: all vertices referenced (`refd`); list reading gives fewer long
  triangles than strip reading (`stripdiag`: list 391, strip 1456).
- Skinned: 391 long triangles, worst 113 mm (same set as rest space).

Reading: the 4cc glove asset is an open surface with long fan triangles across the
palm. GF draws its triangles faithfully, and the palm reads as a ring because the
fans are thin. This is data, as far as the path above can show.

## Conflict with Main's premise

Main states PES draws g1398 solid with the same 27 islands. I have no PES frame of
this glove, and the live PES frame (`pes_now.png`) is too far off to show it. If PES
is solid, something in PES's path is different. I could not find it in the GF path.
Candidates that would produce a solid PES glove from this mesh, none confirmed:
- PES renders the glove two-sided and also fills the open edges through a mesh I have
  not identified (the 30-vert mesh 1 is a 3 mm cluster and cannot be the palm);
- PES's skin splits the fans differently (no evidence in the files I read);
- PES does not draw this glove on a keeper (the draw list in kit-assembly 8.4 shows
  `glove/g1400`, not `g1398`).

## What I need to proceed

1. A PES close-up of this exact keeper glove (team 752 GK, glove 1398) where the palm
   is solid, so I can see whether it is two-sided, what covers the open edges, and
   whether the glove is drawn at all for this keeper. Main has this if the PES
   reference exists; otherwise I can capture one only with a close-up camera in a
   replay (none found in the live match right now).
2. The owner's decision: if PES shows the 4cc glove hollow too, GF is faithful and
   the keeper should keep bare hands (or the glove should be restricted to outfield
   players), not get a weld. If PES is solid, the next step is the PES fill mechanism.

## Tests and state

- No code change this round. Experiments reverted (`pes_blin.gdshader`,
  `uniform.cpp`), rebuilt at head.
- Previous round tests still hold (resolver 3/3; `pes_formats_test` 25/26;
  26 pes binaries pass; GDScript player/texture/model/character exit 0).

## Frames

`frames/cu0_go` (distance 0.9, missed the hands), `frames/cu1_go` (baseline,
distance 2.0), `frames/cu2_nocull` (cull disabled), `frames/cu3_wire` (wireframe
capture, crop too low), `frames/cu4_both` (bare hand underlay). Note: the frame
crops are 1152×400 at (0,120); zoom with `magick -crop` around x 130–330 and 820–1020.
