# PES 2021 — LACC incident-light metering and `isLightAdaptationFromLACC` (RE report)

## 0. Summary

- Live byte `DAT_14373e951` (`isLightAdaptationFromLACC`) in the running match (pid 2510857, st018 dusk) = **0x00**. CONFIRMED.
- The byte is a **debug/render-settings toggle**, not a scene-driven flag. It is written by `FUN_141683280(param_1)` (stores param_1), called from:
  - `FUN_1416ca920` — registered as the console command `SetLightAdaptationFromLACC` (`FUN_1416c94e0` registers it, string `s_SetLightAdaptationFromLACC_142a08800`).
  - two init paths (`~0x141824230`, `~0x14194da5f`) that copy bit 9 of a 32-bit flags field of a settings/tonemap object into the byte.
- The string `isLightAdaptationFromLACC` is registered as a tweakable property in two property panels (`FUN_14182393e`, `FUN_14194da5f`). No fox2 rig or dt18 data file contains the name; it is code-side.
- Default/observed value in normal match play = **0 (untaken branch)**. At st021 15:00 (normal match) the flag is therefore expected **0**. Scene-dependent caveat: only verified on st018 dusk; the init paths key off bit 9 of a settings object whose value has not been traced per scene, but no normal-match scene is known to set it.
- Consequence: PES in normal play meters the **lit frame** (`Tonemap_Lum`), NOT the LACC incident-light buffer. The LACC hypothesis does **not** explain GF's EV-floor gap, and switching GF to an LACC meter is not warranted for fidelity to normal play.

## 1. Live read

| Item | Value |
|---|---|
| pid | 2510857 (match in progress, st018 dusk) |
| VA | 0x14373e951 |
| file offset | 0x373e951 (VA − 0x140000000) |
| byte | 0x00 |
| method | read-only `open('/proc/.../mem')` seek+read, 1 byte |

## 2. Static decode of the setter

- Writer: `FUN_141683280(undefined1 param_1)` → `DAT_14373e951 = param_1`. (decompiled, CONFIRMED)
- Reader: `FUN_141684180()` → `return DAT_14373e951` (decompiled, CONFIRMED).
- Consumer: `FUN_141805840` ("Generate Luminance Texture") branches on the byte: `if (byte == 0)` uses `Tonemap_Lum`, else `Tonemap_Lum_Lacc` (survey §2.3, CONFIRMED).
- Callers of writer:
  1. `FUN_1416ca920` — console command `SetLightAdaptationFromLACC` (registered in `FUN_1416c94e0` via `FUN_1402a1060(..., "SetLightAdaptationFromLACC", FUN_1416ca920)`). It reads slot 2 of a container (`FUN_140381540(u,2)`); if slot id == 1 it sets the byte from `FUN_140381300(u,2)` (nonzero state). This is a debug toggle.
  2. Init path `~0x141824230`: sets byte = bit 9 of `(settings+0xa4)` flags (also writes an adjacent flag from bits 0/1). Automatic, runs during setup.
  3. Init path `~0x14194da5f`: sets byte = bit 9 of `(settings+0x88)` flags. Automatic, runs during setup.
- No automatic path in normal match play has been observed setting bit 9; shipped default is 0 (live read).

## 3. LACC meter decode (reference only; not taken in normal play)

From `docs/re/pes-re-lighting.md` §1 and `pes-render-Survey.md` §2.3:

```
// Tonemap_Lum_Lacc.ps (1,476 B)
uv  = (ndc·(0.5,-0.5)+0.5)·cb5[11].zw       // sub-rect (note: cb5[11], not cb5[12])
lum = depth(uv) > 0 ? max(dot(lacc.rgb, Y709), 0) : cb5[11].x   // sky/far (reversed-Z 0) -> constant
```

- `inImage` t0 = **LACC light buffer** (incident light, before albedo).
- `inDepth` t1 = depth.
- Sky/far pixels (reversed-Z == 0) → constant `cb5[11].x`.
- Meters **incident light** (albedo-free), so pitch colour does not affect exposure.
- Ignores the LACC alpha multiplier (`max(16a,1)` in the debug view); pixels lit above 1 are clipped (only `.xyz` read).
- Inputs to adaptation law otherwise identical (`FUN_1416c7930`): EV clamp [minEV,maxEV], Lmeas·2^-comp, half-error step, speed, key 0.18.

## 4. Predicted effect on GF

- GF currently meters the **lit frame** (`docs/render/pes-render-ComposePath.md`, PostPort) — matches PES normal play (flag 0). No meter change is required for fidelity.
- **If** GF switched to an LACC meter: LACC is albedo-free incident light, sitting about `log2(1/mean albedo) ≈ 3–4 EV` from a lit-frame meter (grass albedo ~0.026–0.046 → ~4.5–5.3 EV; paint ~0.35–0.41 → ~1.3–1.5 EV). So an LACC meter would shift the auto-exposure target by roughly **+3 to +4 EV** at p12 relative to the lit-frame meter.
- **Grass/paint split is unchanged by the meter alone**: LACC meters incident light, which is the same for grass and paint, so exposure is uniform across materials; the split (measured 1.87× with FIN_NET) is a material/albedo phenomenon, not an exposure phenomenon. (Say so: the split does not change.)
- Does it close the EV-floor gap? **No.** Because PES itself does not use the LACC meter in normal play (flag 0, live-confirmed). The EV-floor gap must come from elsewhere: reduction type (arithmetic vs log mean, undecoded), the `0.5·` half-error in the target, `comp`/key, or the EV clamp — not from LACC.

## 5. Port spec

- **No port change.** Keep GF metering the lit frame; this already matches PES normal play. Do not add an LACC compositor pass.
- If a future debug/experimental mode wants LACC metering: add a compositor pass that reduces `dot(lacc.rgb, Y709)` over a sub-rect with reversed-Z sky→constant, feeding the existing adaptation law (`FUN_1416c7930`). Label it `// RE-NEW` / experimental, not the default, since PES ships with flag 0.
- Record in ledger: `EV-LACC-0001` CONFIRMED (live byte 0x00, normal play uses lit-frame meter). `EV-LACC-0002` CONFIRMED (LACC shader decode). `EV-LACC-0003` OPEN (which settings object / bit 9 controls the init paths; not needed for port, but unresolved).

## 6. Status

- CONFIRMED: live byte = 0x00; setter is a debug toggle + two bit-9 init paths; LACC shader decode.
- INF: init paths set the byte from bit 9 of a settings flags field; default 0 in shipped configs.
- OPEN: exact settings object/bit-9 semantics per scene; reduction type in GPU copy chain (pre-existing, unrelated).