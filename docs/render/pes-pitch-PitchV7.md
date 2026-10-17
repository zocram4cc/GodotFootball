# PitchV7: pitch VS fog terms (o7.z, o7.w) into specular and roughness

Branch: refs/heads/PitchV7 at dd38f95 (base origin/B4Grass 6bd1dc8). ls-remote confirmed.
Commits: 6b3753f (shader port + regex test), dd38f95 (test removed; test_model.gd back to base).

## Ported (asm lines in pes3ddf_pitch_2020 PS / VS, B4Grass scratch)
- PS asm 166 `r1.xy += v7.zw`: `ROUGHNESS = s1 + pes_v7.y` (asm 177); `specScale = m4*slope.r*match*(s0 + pes_v7.x)` (asm 176, 178).
- VS asm 107-119: h = max(-1.7*dy - log2(12*dy*P[1][1]/max(depth, 1/16)), 0), dy = (p.y - cam.y)/|p - cam|.
- VS asm 120-129: o7.z = sat(poly_z(h)+0.5)-0.5, o7.w = sat(poly_w(h)+0.5)-0.5.

## Test
- No numeric test exists. The regex source-text check (6b3753f) was removed in dd38f95, because it tested source text, not the maths.
- The change is guarded only by the shader compiling: `test_model.gd` and `test_stadium.tscn` run with PES21_ROOT set, exit 0, 0 SHADER ERROR (run directly, no flock, no ulimit).
- Mutation evidence from the regex-era run (superseded): dropping `.x` or `.y`, removing the vertex write, and a compile error (`pes_v7.z`) each produced the expected result. The compile error printed SHADER ERROR, so shader compilation is checked by the stadium and model runs.

## Not verified
- No rendered frame. No pitch-brightness or specular comparison against PES; the 0.145 B4 measurement was not retaken.
- No numeric check of the VS polynomial or clamp. A GDScript mirror would copy the implementation, and headless readback of GB2 was not attempted within budget.
- Assumed, not read from the install: cb2[5].y = PROJECTION_MATRIX[1][1]; depth = -view z; metres as units.
- Polynomial and scale literals are asm immediates, hard-coded in the shader's PS style, not read with ReadVa.
- log2 of a non-positive argument (camera below the surface) is undefined in GLSL; not guarded.
- ROUGHNESS is not clamped before output, as in PES asm 177; Godot's clamp unverified.
- docs/re/pes-re-pitch-grass.md §7.6 ("skip") is not updated.
- `tools/godot.sh --import` core-dumped on the fresh worktree; `|| true` masks it. Tests ran anyway.
