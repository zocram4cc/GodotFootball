# PES cutscene camera: how a `.fdc` Camera record becomes the camera pose (CameraPose, 29-10-26)

Read-only reverse engineering. Image: `pes_full.bin` (offset = VA − 0x140000000). Decompiles come from headless `dec.sh`. Operand-VA/constant pairs come from capstone through `consts.py`. RTTI class names are read from the image (`rtti.py`, `vtname.py`). CRT thunks are resolved through `docs/re/pes-re-import-slots.md`. Scratch: `<workdrive>/gf-CameraPose-scratch/`. It holds `dec_*.txt`, `consts_a.txt`, `consts_b.txt`, `example.py` and `example_out.txt`, and an extracted `ent_001_view_st000_cam2.fdc` (PES-derived; never commit it). Labels: **CONFIRMED** (decompile plus asm, image read or script output, with VA), **INF** (inference; the reason is given), **OPEN** (the blocker is named). No live reads, no repo writes, no branch.

## 0. Headline: the previous chain was not the camera (CONFIRMED, RTTI)

`docs/re/pes-re-open-decodes.md` §1 and `pes-port-FdcCamera3.md` treat the 0x578-byte node with vtable `0x142BB1048` as "the camera node". The decodes behind them cover `0x141F7E510`, `0x141F7ECC0`, `0x141F7DE00` and the anime sampler `0x141EB9640` with its 4 slots at `+0x50/+0x80/+0xB0/+0xE0`.

- The vtable `0x142BB1048` → its COL → TypeDescriptor gives **`.?AVFixDemoHuman@@`**.
- Node `+0x48` (vtable `0x142612F98`) is **`.?AVHuman@util@draw@@`**.

So `0x141F7E510` is `FixDemoHuman`'s update, and `0x141F7DE00` builds a **human model**, not a `.canm` loader. Its input comes from `0x141EC2890`, called from the cut pump `0x141F577B0`. `0x141EB9640` with its 4 slots is that human's motion/anime blend, and `0x122D` is a motion-id bound. None of it poses the camera, so the open-decodes §1 chain (§1.1–1.5) is wrong in full, not on two points only.

The real camera classes:

| class (RTTI, CONFIRMED) | vtable | role |
|---|---|---|
| `FixDemoCamera` | `0x142BB0D90` (13 slots) | per-cut camera. ctor `0x141F7AE60` (0x228 B), dtor `0x141F7AFF0` |
| `match::camera::demo::ktCameraAnimation` | `0x142BB0D80` | CANM player, `FixDemoCamera+0x08` (0xB8 B), dtor `0x142139A10` |
| `match::camera::demo::ktAnimationBase` | `0x142BF11C8` | CANM channel sampler (0x10 B, ctor `0x142139E50`), `ktCameraAnimation+0x98` |

`FixDemoCamera` vtable slots used here: `+0x20` Update `0x141F7B770`, `+0x40` SetCanm `0x141F7B5E0`, `+0x48` SetTime `0x141F7B4A0`, `+0x50` SetProcedural `0x141F7B630`, `+0x58` procedural update `0x141F7BFE0`, `+0x60` SetCutOffset `0x141F7B700`.

The FixDemo player owns the camera at `player+0xC68`. It is created in `0x141F2F6B0` and `0x141F31490` (call `0x141F7AE60`) when `player+0x7C != 0`.

## 1. Item 1: pose consumer, CANM load, key → pose (CONFIRMED)

### 1.1 CANM load: `ktCameraAnimation::SetData` `0x142139D60` (via `FixDemoCamera` slot `+0x40` `0x141F7B5E0`)
- `+0x98` = new `ktAnimationBase(data)`.
- `+0xA0` = `data + u32(data+0x10)`: the track.
- `+0xA8` = `(float)s16(data+4)`: the length in frames.
- `+0xAC` = fps: `(float)i32(data+8)` if `data[0xC] & 1`, else `30.0` (`0x142139E30 movss [0x142591140]` = 30.0).
- `0x141F7B5E0` then sets `cam+0x128 = 0` (CANM mode), `+0x214 = 0`, `+0x21C = 0`, `+0x209 = 0`, `+0x220 = +0x222 = 1`.
- The `.canm` pointer passed in is the resource at `node+0x128 → +0x88` (or loaded from `+0x80` via `0x1414A8B70`), in `0x141F39CC0`.

### 1.2 Per-frame evaluate: `ktCameraAnimation` `0x142139AB0(this, frame)` (asm `0x142139AB0..0x142139D55`)
- **Time clamp.** If not looping (`+0xB0 == 0`; set to 0 by the ctor, nothing else seen writing it): `frame = clamp(frame, 0, +0xA8)` (`0x142139B3B..B51`). If looping, it wraps by `floorf`.
- **Integer frame.** `iframe = (int)floorf(frame)` (`0x142139B57` call `0x1415A1876` = `floorf`, then `0x142139B79 cvttss2si`).
- **Channel loop.** For k = 0..5 (`rdi = 8..0x1C`, `0x142139B7D`/`D02`), the channel is at `track + u32(track + 8 + 4k)`. The call is `ktAnimationBase vfunc +0x18 (0x142139EB0)(ch, frame, iframe, &out)` at `0x142139BC9`. The switch is on the channel's semantic byte (`out+4` = `ch[+6]`):
  - **0 rotation.** Quaternion `out+8..+0x14` (x,y,z,w; default (0,0,0,1)) → `0x140A91CA0`. This builds a column-vector rotation matrix with `s = 2/|q|²` (`0x140A91D24 movss [0x142565604]` = 2.0), so the matrix normalises the quaternion. **Columns 0 and 2 are then negated**: `xorps` with `[0x1425656D0]` = −0.0 at `0x142139CAB/CAE/CC3/CCE/CE0/CE8` on m00,m02,m10,m12,m20,m22. The result goes to `0x14088D2A0(this, M', 0)`. That stores the matrix at `+0x44`, sets `dir (+0x24) = column 2 of M' = −R·ẑ` and `up = column 1 = R·ŷ`, then derives yaw/pitch/roll (`+0x38/+0x34/+0x3C`). The camera looks down the quaternion's −Z.
  - **1 position.** `out+0x18..+0x20` → `this+0x18..+0x20` (`0x142139C31..C5A`).
  - **2 fov.** `this+0x10 = v·π/180` (`0x142139C19 mulss xmm8` with `xmm8 = [0x142578480]` = 3.1415927 loaded at `0x142139B67`; `0x142139C22 divss xmm9` with `xmm9 = [0x142598580]` = 180.0 loaded at `0x142139B70`). Also `this+0xC = 0`.
  - **3 / 4.** `this+0x84 / +0x88` (the CANM near/far). **These never reach the render camera** (§2.3).
  - **5 (aspect).** Ignored: no case.

### 1.3 Channel sampler: `ktAnimationBase::Sample` `0x142139EB0`
Channel header: `s16 +0` interp type, `s16 +4` key count, `u8 +6` semantic, `u32 +8` times offset (0 = one key per frame), `u32 +0xC` values offset. Offsets are relative to the channel.

- **Key search** `0x14213A210(ch, iframe)`. With a time table (u16 frames): binary search for the last key with `t ≤ iframe`. It returns `count−1` if `iframe ≥ t[last]`. Without a table: `min(iframe, count−1)`.
- **Segment.** Step forward past keys with equal time. `f = (frame − t0)/(t1 − t0)`: the float frame, not `iframe`. At the last key, `f = 0` and both indices are the last key.
- **Interpolator.** Taken from a 16-byte table at `0x143542620` (lea at `0x142139F63`; values read from the image): type 1/3 → kind 1 `0x14213A180`; 4/5/6/10 → kind 2 `0x14213A110`; 8 → kind 3 `0x14213A0E0`. The `0x14213A06B` load of `[0x14256AE1C]` = 1.0 is the default `w` of a rotation output.
  - **Rotation (kind 1, `0x14213A180`).** `f < 0.001` → q0 (`0x14213A184 movss [0x1425A9668]` = 0.001). `f > 0.999` → q1 (`0x14213A1AC comiss [0x1425E4D78]` = 0.999). Otherwise **slerp** `0x140A93210`:
    - `d = q0·q1`; if `d < 0`, negate d and sign = −1 (`0x140A9328F xorps [0x1425656D0]`).
    - If `1 − d ≤ 1e-5` (`0x140A932AA comiss [0x1425568D0]` = 9.9999997e-06), the weights are `(1−f, ±f)`, not normalised.
    - Else `θ = acosf(d)` (thunk `0x1415A1798` = acosf), `inv = 1/sinf θ`, `w0 = sinf((1−f)θ)·inv`, `w1 = ±sinf(fθ)·inv` (`0x140A932C1..0x140A93314`; `0x1415A1792` = sinf; 1.0 = `[0x14256AE1C]` at `0x140A93299`).
    - Output `q0·w0 + q1·w1`.
  - **Position (kind 2, `0x14213A110`).** `f·v1 + (1−f)·v0` per component; values are vec4 with stride 16 (1.0 at `0x14213A116 [0x14256AE1C]`).
  - **Scalar (kind 3, `0x14213A0E0`).** `(1−f)·v0 + f·v1`, stride 4 (1.0 at `0x14213A0E6 [0x14256AE1C]`).

This corrects `pes-re-open-decodes.md` §1.3 ("no slerp, per-component lerp"). The PES rotation **is** a shortest-arc slerp.

### 1.4 The pose consumer: `FixDemoCamera::Update` `0x141F7B770(this, params)` (CANM branch, `this+0x128 < 1`)
`this+0x168` holds a copy of the `ktCameraAnimation` pose block (copied by `0x1408914D0` in SetTime).

1. `M = 0x14088AD50(block, 0)`: the rotation is rebuilt from yaw/pitch/roll.
2. `p = block.pos + (0, this+0x20C, 0)` (`0x141F7B866 addss [rbx+0x20C]`).
3. If `this+0x164` (flip), mirror X: m01, m02, m10, m20 and p.x are multiplied by `[0x142588218]` = −1.0 (`0x141F7B87D`).
4. If `this+0x160 != 0` (rotY, degrees), `R_y(rotY·[0x1425A0C48])` is applied to M and p (`0x141F7B8FC`; 0.017453292).
5. `p += this+0x154/+0x158/+0x15C` (`0x141F7B9BF/9B7/9C7`).
6. Set the translation of M and call `0x14088D2A0(block, M, 1)`.
7. Write params:
   - `[0..2]` = target `= pos + dist·dir` (`0x14088AD10`)
   - `[3..5]` = eye
   - `[6..8]` = up (column 1)
   - `[0xC]` (`+0x30`) = fov in radians (`0x141F7BA1A`)
   - `[0xD]` (`+0x34`) = near ← `this+0x1EC` (`0x141F7B9FB/BA05`)
   - `[0xE]` (`+0x38`) = far ← `this+0x1F0` (`0x141F7BA0B/BA11`)

   The DOF branch (`record+0xEF`) and `0x141F7B250` (fov widescreen fix, below) follow.

The yaw/pitch/roll round trip is rebuilt as Ry·Rx·Rz (`0x140889B70` axis-angle ×3, `0x14088AD50`). That it reproduces the slerped orientation up to rounding is **INF**: the same matrix is decomposed and rebuilt, but it is not emulated bit-exact. The worked example uses the matrix before decomposition.

**fov widescreen fix** `0x141F7B250`. If `G(0x141F500D0)+0x522E` is set, the display has `!+0x7C` and aspect `< 16/9` (`0x141F7B294 [0x142609394]` = 1.7777778):
`fov = 2·atanf(tanf(fov·0.5)·(16/9)/aspect)` (`[0x142565600]` = 0.5 at `0x141F7B2A6`, `[0x14256AE1C]` = 1.0 at `0x141F7B2B3`, `[0x142565604]` = 2.0 at `0x141F7B2CC`; tanf/atanf = thunks `0x1415A17D4`/`0x1415A17AA`). This keeps the 16:9 horizontal FOV, so the CANM fov is **vertical fov at 16:9** (INF from this formula).

## 2. Item 2: start offset, per-tick time, near/far sink (CONFIRMED unless marked)

### 2.1 Record → camera (`0x141F39CC0(player, node)`)
The node is the map node of the record; node `+8` = record `+0`. `cam+0x130 = node+8` = record (`0x141F39D79`).

| record field | → | VA |
|---|---|---|
| `+0x0C` animePath (empty → slot `+0x50` procedural) | resource → SetCanm (`+0x40`) | `0x141F39DEB`, `0x141F39FC6` |
| `+0x8C` animeStartTime (+ function-number terms, `0x141F31F00`, record `+0xA8` u16×4) | `cam+0x21C` | `0x141F39CFA` load, `0x141F39E3D` store |
| `+0x90` nearClip | `cam+0x200` | `0x141F39DEE`/`0x141F39DFD` |
| `+0x94` farClip | `cam+0x204` | `0x141F39E05`/`0x141F39E14` |
| `+0x9D` loop | `cam+0x208` | `0x141F39E1C`/`0x141F39E2A` |
| `+0xA6` use cut offset | slot `+0x60` gets `player+0xA08..+0xA18` (pos, rotY, flip) or zeros | `0x141F39E56`, `0x141F39E89` |
| `+0xA0` height rate, `+0xB1` target | `cam+0x20C = (h − 180)·0.01·rate` (0 if rate is 0 or target < 0) | `0x141F39F10 subss [0x142598580]` = 180.0, `0x141F39F18 mulss [0x14256AE18]` = 0.01 |

### 2.2 Per-tick time (`0x141F42110`, the player's camera update)
This replaces the old question about `0x141F7E510`'s argument: that function is the human's update (§0).

- The player clock is `player+0x80`, in **1/60 s ticks**. It advances by `player+0x6C` each frame (`0x141F2F6B0`: `+0x6C = Δ`, `+0x68 = Δ/60.0` with `[0x14259A764]`, `+0x80 += +0x6C`).
- SetTime argument (`0x141F42191..0x141F421DC`):
  - `t = clock` if record `+0x9C == 0` and CANM mode.
  - Else `t = max(clock − record+0x04 startCutTime, 0)`: `0x141F421BB movss xmm0,[rbx+0xC]` (node `+0xC` = **record `+0x04`**), then `subss` at `0x141F421C5`.
  - `xmm2 = Δticks`.
- **Record `+0x04` enters here**, and as the switch threshold at `0x141F42294 comiss xmm0,[rcx+0xC]`: the next record is activated when `clock ≥ its startCutTime`. Activating a record calls `0x141F39CC0` (`0x141F422B4`).
- **SetTime** `0x141F7B4A0`:
  - `t += cam+0x21C` (`0x141F7B4FC`).
  - `frame = t · fps / 60.0` (`0x141F7B51A divss [0x14259A764]` = 60.0, then `mulss`).
  - If looping and `frame > len`, `frame -= (int)(frame/len)·len`.
  - Evaluate (`0x141F7B562`), copy the pose (`0x141F7B581`).
  - Return `frame < len`. When it is false and record `+0x9E` is set, `0x141F421E6` moves to the next record.

  So the time argument is **ticks**, not 0..1. One CANM frame = 2 ticks at 30 fps.

### 2.3 near/far sink
- SetTime overwrites the pose's near/far with the record's: `cam+0x1EC = +0x200` (`0x141F7B586/7B599`) and `cam+0x1F0 = +0x204` (`0x141F7B59F/7B5A5`). **The CANM near/far channels are discarded.**
- Update writes them to `params+0x34/+0x38` (§1.4).
- The camera controller calls `cam->Update(cam, 0x1437F4A40)` at `0x141EAF8B4 call [rax+0x20]` (params = global `0x1437F4A40`). Then:
  - `0x141EAF8E7 movss [0x1437F4A70]` (fov) → `0x141EAF5D0`: aspect scale, clamp to `[0, 2.4434609]` (`0x141EAF60C [0x142B672C8]` = 2.4434609 rad = 140°).
  - That result goes to `0x1419F80E0` (render cam `+0x40` fov, `+0x44` aspect).
  - `0x141EAF903 movss [0x1437F4A74]` (near) → `0x1419F80B0` (render cam `+0x34`).
  - `0x141EAF914 movss [0x1425CB498]` = 4000.0, then `0x141EAF91C maxss [0x1437F4A78]` (far) → `0x1419F7EE0` (render cam `+0x38`). **The render far is `max(4000, farClip)`.**
- That render cam `+0x34/+0x38` are the projection's near/far planes is INF (setter order; the projection build is not decoded).
- The camera is registered with the controller by `0x141F36000` → `0x141EB0570(owner+0x258, cam)`.

## 3. Item 3: worked example `ent_001_view_st000_cam2.fdc` (script `example.py`, float32)

The file was extracted from the mounted VFS with `pesfmt_dump cat`. Which archive wins the mount was not recorded. Fixdemo is not a dt18 constant file.

Camera records (script output): idx 4 key 20 and idx 5 key 21, both at start 540. Then keys 30–33 at 1200 and keys 40/41 at 1680. All have animeStartTime 0, near 0.5 (0.75 / 1.15 on some), far 400, `+0x9C = 1` (relative time), loop 0, `+0xA6 = 1`, height rate 0, function numbers 0.

`cam0021.canm` (external, through the VFS):
- 330 frames at 30 fps (header `+0xC` bit set, `+8` = 30).
- Channels: rot (type 1, 331 keys, times table), pos (type 4, 331), fov (type 8, 331), near / far / aspect (1 key each).

Pose at `clock = 540 + Δ` ticks: `frame = Δ·30/60`. The cut offset is taken as zero (see OPEN 2). PES render values: near 0.5, far = max(4000, 400) = 4000.

| Δ ticks | frame | eye (m) | forward (= −R·ẑ) | up (= R·ŷ) | fov (vertical, deg) |
|---|---|---|---|---|---|
| 30 (0.5 s) | 15.0 | (−55, 0.4, 9.7) | (0.4942, 0.2417, −0.8351) | (−0.1231, 0.9704, 0.2080) | 46.3980 |
| 91 (mid-key) | 45.5 | (−55, 0.4, 9.7) | (0.5285, 0.2447, −0.8129) | (−0.1334, 0.9696, 0.2051) | 46.3980 |
| 180 (3 s) | 90.0 | (−55, 0.4, 9.7) | (0.6259, 0.2512, −0.7384) | (−0.1624, 0.9679, 0.1916) | 46.3980 |

The camera is fixed and panning. The CANM's own near/far (0.01/400) are ignored. Δ = 91 runs the slerp between keys 45 and 46: f = 0.5, d = 0.9999995 (1 − d = 4.77e-07 ≤ 1e-5, script check), so it takes the **linear-weight branch**. The same table for `cam0022` (key 21) is in `example_out.txt`.

## 4. What this means for GF's port (`src/pes/fdc.cpp` on `FdcCamera`, not edited)
1. Rotation slerp is right in kind. Three things differ:
   - PES snaps to q0 for `f < 0.001` and to q1 for `f > 0.999`.
   - Its linear fallback starts at `1−d ≤ 1e-5`, not `d > 0.9995`.
   - It does not renormalise; the matrix's `2/|q|²` does that.
2. Lerps are `(1−f)·a + f·b`, not `a + (b−a)·f`. These are different float bits.
3. The key index comes from `floorf(frame)` with clamp to `[0, len]` before sampling.
4. Time is `frame = (clock − startCutTime [if rec+0x9C] + animeStartTime(+fn terms))·fps/60`, with the CANM's own fps.
5. The CANM near/far channels are never used. Near = record `+0x90`. Far reaches the render as `max(4000, record+0x94)`. GF's `kMinNearM = 0.1` clamp has no PES counterpart in this path.
6. The fov is clamped to 140° and widened for aspect below 16:9 under a flag.
7. Cut offset, flip, rotY and the height offset are applied after sampling (§1.4).

## 5. OPEN (numbered, with blocker)
1. **Which record of a same-start pair plays** (key 20 vs 21 at 540, 30–33 at 1200). The selection is upstream of `player+0x88` (the list built from the record map). Blocker: decode the builder of `player+0x88` and the `selectByRate` use (`FUN_141F3F5E0` is a candidate, not read).
2. **The cut offset at runtime** (`player+0xA08..+0xA18`: offset pos, rotY, flip) for the entrance. Record `+0xA6 = 1` applies it. Blocker: a live read of the FixDemo player while the entrance plays (the `player` object = rcx of `0x141F42110`; a new hook would be needed: ask Main) or a decode of its writer. The example assumes zero.
3. **Clock origin.** When `player+0x80` is reset at cut start, and what `+0xAA8`/`+0xAB0` are (INF: dt in seconds and a speed divisor; `0x141F2F6B0`). Blocker: the decode of the cut-start init (`0x141F3D370`/`0x141F3CFA0`, not read).
4. **Bit-exact Euler round trip** (`0x14088D2A0` → `0x14088AF20`/`0x14088AD50`). Decoded in structure and not emulated. The example's pose is the pre-decomposition matrix (INF that it is equal to within rounding).
5. **Render cam `+0x34/+0x38/+0x40` as projection near/far/fov** is INF from the setters `0x1419F80B0`/`0x1419F7EE0`/`0x1419F80E0`. Blocker: decode the projection build in the renderer.
6. **Doc corrections for Main.** `pes-re-open-decodes.md` item 1 (all of §1.2–1.5) and `pes-port-FdcCamera3.md` §1.4/§1.5/§5.1–5.3 point at `FixDemoHuman`, not the camera. Replace them with this report. Not edited here (read-only task).

No tests were run and no code was changed. Gate not run (Main gates).


## Main's check (29-10-26)

RTTI read from the image: vtable `0x142BB1048` = `.?AVFixDemoHuman@@`, `0x142BB0D90` = `.?AVFixDemoCamera@@`. `0x141F7B51A divss xmm0,[0x14259A764]` = 60.0; `0x142591140` = 30.0. `0x141EAF91C` is a `maxss` (the far clamp).
