# B5 Materials: report, update (branch `B5Materials`, head 8fd78e0)

Supersedes the earlier report where it differs. Full comparison: docs/render/pes-kit-compare-b5.md.

## Commits on origin/B5Materials
- 52ff83d, 6c35124, bd5dfa2, ba1ac18 (earlier kit, uniform, skin/hair/eye, viewer work)
- 143ab9e whole-body boots packs replace the base body (kit on the Suit)
- 8fd78e0 whole-body players keep their face pack; comparison note

## Brightness (owner's note)
- OBS PES-vk capture is sRGB double-encoded: +1.0 to +2.2 stops by tone.
  Decoding (`magick -colorspace RGB`) matches the compositor capture to 0.001 over five static regions.
- Godot movie frames are display-correct (2D 0.5 -> 128; 3D background 0.5 -> 127, on screen and in the movie).
- All PES references below are decoded.

## Findings
- W (760): whole body; Skin/Suit/boots materials are unlit constants in PES and in the viewer, so the W comparison tests albedo only. Colours agree with decoded replay frames. Monitor head now draws (face pack kept). Open: monitor picture differs from PES; back number and name not drawn on the body Suit.
- BNT (708): stock cloth. Pale shirt is 0.020 linear in the viewer vs 0.468 decoded in PES (-4.5 stops). Atlas sample, weave and normal are correct: the offset is the viewer's scene light (rig exposure, B2/B3), not the materials. BNT atlases carry zero alpha in every block (also in 4cc_45_uniform.cpk); W's are opaque. Effect on albedo is about 7 percent.
- Markings: the back name and number draw on cloth (BNT) and do not yet draw on the W whole body.

## One-third target
Not met. No captured frame has a kit at about one third of frame height. Closest: decoded replay/celebration close-ups at 0.7 to 0.9 (BNT, W) and a W back view at about 0.9. Live broadcast play is 40 to 100 px tall. 150 frames captured (live 90, live2 60), viewed as contact sheets.

## Checks
- pes_uniform_test: 35 of 35.
- test_player: green for 760/76020 with the owner's save. The 70821 save-look run asserts a long-sleeved subject; 70821 is short-sleeved, so that run is not a gate.
- Viewer renders for both teams: no shader or script errors.

## Open (for Main)
1. Lit-scene exposure about -4.5 stops vs decoded PES: B2/B3.
2. Whole-body markings (back name and number).
3. Whole-body monitor picture (texture binding check).
4. BNT zero-alpha kit atlas: confirm the PES rule.
