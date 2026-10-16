# PES kit comparison: BNT (708) and W (760), B5 Materials

Scope: the two test subjects the owner named. `BNT` is team 708, a stock cloth kit,
and this note uses kit 1 (the pale kit) with face 70821 (shirt 21). `W` is team 760,
a whole-body team, and this note uses kit 0 with face 76020 (shirt 20). Both are
drawn in the viewer at stadium 29 with the rig's defaults (day, fine, summer), which
is the stadium and weather of the live PES match these were paired with.

## 1. Brightness: the PES capture path is double-encoded

The OBS `PES-vk` capture is brighter than the owner's display by 1 to 2.2 stops,
depending on tone. Over five static regions (HUD, boards, pitch line, grass, stands),
the OBS value equals sRGB-encode(display value): decoding the OBS PNG with the sRGB
transfer (`magick -colorspace RGB`) reproduces the compositor capture of the same
frame to 0.001 in every channel of every region. Every PES frame used below is
decoded this way. The raw OBS frames serve only to measure that encoding; no
comparison uses one.

The Godot movie writer is display-correct. A 2D reference patch at 0.5 gives 128 in
the movie and on screen. A background at 0.5 gives 127 in the movie and on screen,
and an unshaded quad at 0.5 gives 127 on screen. So the viewer's frames need no
correction; the difference below is in the scene light.

## 2. W (760): whole body, unlit

The W body is `boots.fmdl` from the boots pack (42 bones, `dsk_hip` among them) with
four materials: `Skin` and `Suit` are `fox3DFW_ConstantSRGB_NDR` (unlit), `boots` is
`fox3DFW_Constant_NDR` (unlit), and `Patches` is Blin. The PES body is unlit too, so
the W comparison tests albedo and the kit atlas, not lighting.

- The navy GAINAX shirt, the orange-red hem band, the cream shorts, the navy socks
  and the skin arms agree in hue and value between the decoded PES frame and the
  viewer render.
- The head (`hair_high`, the monitor) agrees in shape. Its picture does not: PES shows
  one anime character, the render shows a group image. Not resolved (see §5).
- The kit's back number (20) and name are not drawn on the whole-body `Suit`
  (see §5).

## 3. BNT (708): stock cloth, lit

Measured on one patch of the pale shirt body, linear light, same atlas region:

| | pale shirt, linear RGB | ratio to PES |
|---|---|---|
| PES, decoded, replay close-up | 0.468 / 0.463 / 0.568 | 1 |
| Viewer render, same region | 0.020 / 0.018 / 0.023 | 0.043 (about -4.5 stops) |

The attribution is the scene light, not the material:

- The atlas sample at the chest is the pale lilac of the atlas (0.49 / 0.42 / 0.55,
  linear), so the atlas and its UV are read correctly.
- The weave term at the chest is 0.91 (linear), so the fabric pattern is bright.
- The final normal at the chest decodes to about (-0.10, 0.00, 0.99): it faces the
  camera, so it is not flipped.
- Only the chest was measured. The sleeves, shorts, socks and forearms also look dark
  in the same render, and the forearm texture is light (0.63 / 0.49 / 0.42, linear).
  A material fault would be specific to a part; the whole render is dark, which points
  to the light.

So the lit cloth is about 4.5 stops under the decoded PES frame, and this is the
viewer's fixed exposure from the rig. It is not a material result. Exposure
adaptation is the post chain's (B3) and the rig's (B2); this note does not change
either.

**Open for the materials, small:** the BNT kit atlases (`u0708p1`, `u0708p2`) are
DXT5 with a zero alpha channel in every block, while the W atlas (`u0760p1`, DXT5) is
opaque in every block. The same zero-alpha atlas exists in `4cc_45_uniform.cpk`.
Under the RE coverage rule, zero alpha takes the weave term (0.91 here) where an
opaque kit takes 0.85, so the albedo differs by about 7 percent. That is much smaller
than the 4.5 stops above. Whether PES reads the same zero alpha is not settled.

## 4. Verification

- `pes_uniform_test`: 35 of 35 pass, install tests included.
- `test_player.gd`: passes with the owner's save for W (whole body, face pack kept,
  no uniform, no bare arm or hand). For BNT the save-look check asserts a long-sleeved
  subject, and subject 70821 is short-sleeved in the save; that run is not a
  regression and is not used as a gate.
- Viewer renders: no SHADER ERROR and no SCRIPT ERROR for the W and BNT shots.

## 5. Open items

1. Lighting: the viewer's lit scene is about 4.5 stops under the decoded PES daylight
   (§3). Report to B2 and B3 with this measurement; the exposure is not changed here.
2. Whole-body markings: W's back number and name are not drawn on the body `Suit`.
   PES draws them, so the W kit is incomplete until the markings are placed on the
   body's UVs.
3. Whole-body head picture: the monitor's artwork differs from PES (§2). Check which
   texture the head material binds for these face packs.
4. Kit atlas coverage: zero alpha on the BNT atlases (§3). Confirm with the RE what
   PES reads there.
5. Shirt name: the back name in PES is the shirt name (`DANK`); the viewer was given
   the full name, so the render shows `LE DANK FORCED FACE`. Pass the shirt name for
   comparisons.
6. Reference frames: no PES frame of either team has a kit at one third of frame
   height yet. The closest are replay and celebration close-ups at 0.7 to 0.9 of the
   frame height; the comparisons above use those.
