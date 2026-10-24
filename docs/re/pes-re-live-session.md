# PES live-session RE: P5 monitor-UV decode + one capture script

Status: read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(VA = fileoff + 0x140000000), Ghidra bridge `http://127.0.0.1:8080` (one request
at a time), install VFS `<workdrive>/PES21` (read-only), live PES pid 2510857
(read-only `sudo -n` pread on `/proc/<pid>/mem` only).
Scratch: `<workdrive>/gf-LiveSessionRE-scratch/` (absolute output paths; nothing
written to the repo). Budget: ~40 requests used.

## 1. P5: the monitor-UV rectangle — DECODED static (CONFIRMED file + mesh bytes)

Question from `docs/render/pes-render-Static.md` §S2b/P5: the face-76020 monitor
shows one anime character in PES but GF renders the whole `head.dds` group image
(five figures under a cloudy sky). Binding is the same file both sides
(`Base_Tex_LIN = …/76020/sourceimages/head.dds`, re-verified this session via
`pesfmt_dump fmdl`, `hair_high.fmdl` material `Head [fox3DFW_Constant_NDR]`).
So the difference is the sampled sub-rect.

Findings (all script-read from the install, pasted):

- `hair_high.fmdl` Head mesh: 604 verts / 460 tris; UV0 bbox
  `u=[0.000002,0.937012] v=[0.146973,0.999512]` (`uv_probe.cpp` output).
- Triangle-weighted 8x8 occupancy (`uv_cells.cpp` output): 312/460 tris in
  `v[1..2]/8`, 304 in `u[1..2]/8` — i.e. the face geometry samples
  **u in [0.125,0.375], v in [0.125,0.375]** (stored-v convention).
  At 2048² (`head.ftex` pf=4 DXT5, 2048x2048, 1 mip): **x 256..768**,
  y 256..768 (D3D v-down) = y 1280..1792 (GL v-up).
- The `fox3dfw_constant_ndr` VS (`GrModelShaders_dx11.fsop` → `fsop_extract.py`
  → `vkd3d-compiler`, `ndr/` in scratch) ends `mov o0.xy, v6.xyxx`:
  **UV passthrough, no UV transform**. So there is no shader-side rect to port —
  the "rect" IS the mesh UVs, which GF already imports verbatim.
- Therefore the PES-vs-GF picture difference is NOT a missing UV rect on the
  Head mesh. Remaining candidates (both GF-side, no PES read needed):
  (a) GF samples the texture with the wrong v-flip/colourscheme (LIN vs SRGB:
  PES role is `Base_Tex_LIN`; GF's `MakeTechniqueMaterial` has no
  `fox3DFW_Constant_NDR` entry — `SHADERS[]` covers Skin/Blin/Eye/Hair2 only —
  so the Head material falls to the generic `MakeMaterial` path, which keys
  `BASE_ROLE = "Base_Tex_SRGB"` and may bind/flip differently);
  (b) the decoded `head.ftex` DXT5 block map both sides assemble differs
  (chunked-zlib assemble order).

Port spec: **no UV-rect port** (mesh UVs already verbatim; VS passthrough
CONFIRMED). The item becomes: route `fox3DFW_Constant_NDR` (role
`Base_Tex_LIN`, unlit, opaque) to the constant shader path and check the
v-convention against one viewer frame — GF-side work, no live match needed.
P5 CLOSED as a static decode; re-filed as a GF material-routing fix.

GF counterpart: `src/gdext/pes_character.cpp` `MakeTechniqueMaterial`
(`SHADERS[]`, no NDR entry) + `MakeMaterial` generic path (`BASE_ROLE =
"Base_Tex_SRGB"`); `src/gdext/pes_model.cpp` `ShaderFor` (NDR→nullptr→Blin
fallback with warning).

Reproduce: `pesfmt_dump fmdl "Asset/model/character/face/real/76020/#Win/face.fpk"`
→ `uv_probe.cpp` / `uv_cells.cpp` (link `build/libpesfmt.a -lz`, `-std=c++20`)
→ `pesfmt_dump cat shaders/dx11/GrModelShaders_dx11.fsop` → `fsop_extract.py
grmodel.fsop ndr fox3dfw_constant_ndr` → `vkd3d-compiler -x dxbc-tpf -b d3d-asm`.

Lua corroboration (script-read): `PesShaders_dx11.lua` has no `fox3DFW_Constant_NDR`
technique entry with params (only the `_Bill` variant); `GrModelShaders_dx11.lua`:
`fox3DFW_Constant_NDR` = `{params={}, packing={}, textures={"Base_Tex_LIN"}}`
vs `fox3DFW_ConstantSRGB_NDR` = `{"Base_Tex_SRGB"}` — the LIN/SRGB split is
by technique name, CONFIRMED.

## 2. The script: `<workdrive>/gf-LiveSessionRE-scratch/live_session.py`

One read-only script for every remaining live-only item. Waits for a match
(polls until MI non-null), samples per recipe, writes one log.
`sudo -n python3 live_session.py [--dur SEC] [--out LOG] [--pid PID]`.

| item | chain from G | sample / rate | settles |
|---|---|---|---|
| T8b (blocked-slices §1) | `T_s=*(G+(s+0xA1)*0x28)`, `T_s+0x8C1F` 11B + `T_s+0x8CE8` u16 | per MI change (~20 Hz poll) | first nonzero → writer-watch tick |
| T12b (blocked-slices §1) | `H=*(0x1436F3F98)`, 48 qwords `H+0x62B8+off` (OFF48 in script) | once + on change | vtable addrs → ≤48 bridge decompiles |
| T6a (blocked-slices §1) | `M=*(0x1436F3F78)`, `W=*(M+0x118)`, `MA=*(W+0x10)+8`, `MA+0x24D50/54` | 1 Hz | nonzero = pusher hunt; zero all match = inert |
| PA (publisher-link §1) | breakpoint-only; script logs the gap, no fake sample | — | `dst` vs `FUN_140A48910(G,id)` at `FUN_140430500` |
| CAM (Static S1) | `MI+0x44` mode byte + `MI+0x11B0` + T8b line | per MI change | `+0x44` writer by value change on mode switch |
| STEP (restart-step §4) | `MI+0x11B0` byte + `+0x1554/+0x116C` gates | per MI change | step values across 2–3 restarts |
| METER (Survey §5.1) | exposure-group inputs (registry value object walk — open) | logged as open | live rect → activates P2 static inputs |

METER/comp-block `param_3+0x8c` and the InplayCamera vtable-scan pose are
logged as open shapes, not sampled: the comp caller is table-driven
(`0x142a383f0` [DATA], Survey §2.5) and the plugin address is unstable across
restarts (camera-live §1: re-find by vtable scan each session). The script does
not fake them.

## 3. Static validation of the chains (script-read image bytes, pasted)

```
OK T8b reader 0x1404F9308 CMP [RAX+RSI+0x8C1F] = 80bc301f8c000000
OK STEP mirror 0x1404227F9 MOVUPS XMM0,[RSI+0x728] = 0f1086280700000f
OK STEP mirror2 0x140422800 MOVUPS [RBX+0x11B0],XMM0 = 0f1183b01100000f
OK W1 0x140458912 MOVZX EAX,[R15+0x11B0] = 410fb687b0110000
OK T6a gate 0x140537AE0 head = 4881c3504d02008b
OK cam ctor store 0x14087e965 MOV [RDI+8],EAX = 894708c7470c0000
OK copy blob 0x140450ff0 MOVSD head = f20f104113f20f11
OFF48 count = 48
```

T-chain `*(G+(s+0xA1)*0x28)` per `FUN_140A48A30` (holder-leaves, first-touch,
lane-core/leaf/post — five docs agree); validated live: `T0=0x1d471e04
T1=0x1d48ff14` read clean.

## 4. Dry run at menu state (no match running)

`sudo -n timeout 20 python3 live_session.py --dur 15` → rc=0, 75 lines in
`dry_menu2.log`: waits on MI-null, samples when MI flickers non-null mid-tick
(MI is null for part of every tick — polled, never pinned), logs
`step11B0=0x16` parked (restart-step §4: stopped parks at 0x16), T8b all-zero,
T6a count 0, T12b 24/48 nonzero (float `-1.0` init pattern `0xbf800000` +
runtime pointers — the row vtables are live heap values even at menu).
No match started during the run, so no in-match capture; the script was not
run during a match.

## 5. How to run it (owner / Main)

A match must be running (exhibition). Then:
`sudo -n python3 <workdrive>/gf-LiveSessionRE-scratch/live_session.py --dur 3600 --out <workdrive>/gf-LiveSessionRE-scratch/live_session_$(date +%s).log`
Leave it for the match; grep the log for `T8b=` (nonzero), `T12B nz=48/48`,
`T6A count!=0`, `step11B0=` transitions, `mode44=` changes.

## 6. Open / left

1. In-match capture (needs a live match; script ready).
2. PA identity (breakpoint, not pread — named in script, unsampled).
3. METER rect + InplayCamera pose scan (open shapes, script logs the gap).
4. P5 follow-up is GF-side (NDR routing + v-check + viewer frame), no PES read.

## Capture 1 (Main, 23-10-26): one hour of the owner's endless match

`live_session.py --dur 3600` against pid 2510857: 20,180 lines, frames 7,251,065 → 7,452,928 (~62 min at 54 fps). **Play never stopped:** every MI sample reads `play=1 restart=0 step11B0=0x16 mode44=0x00` (the owner's endless match, one outfield player per side, stayed in open play for the hour).

- **T6A: confirmed inert live.** The kick-list count `MA+0x24D50` read 0 in all 2,933 samples, as the static verdict (`pes-re-blocked-slices.md`) said.
- **T12B: the recipe's "48 row pointers" is wrong for at least half.** 24 of 48 slots are non-zero and constant through the hour, and their values are floats, not pointers (`0xBF800000BF800000` = (−1.0, −1.0) pairs, `0xBF0D78C800000000` ≈ −0.553). Re-decode the slot table's element type before reading it as a pointer table.
- **T8b, the camera writer, the restart step, the PA identity, the metering rect: unsettled.** Each needs a stoppage or a camera change, and this match had neither (T8b's flags read all zero; `mode44` stayed 0; the step stayed 0x16). Rerun during a normal exhibition match with goals and set pieces.
