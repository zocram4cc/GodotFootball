> **SUPERSEDED (Main, 29-10-26): §1 decodes `FixDemoHuman` (a cutscene human's animation), not the camera.** The camera is `FixDemoCamera`; see `docs/re/pes-re-camera-pose.md` (RTTI checked). Items 2 and 3 stand.

> **Correction (Main, 28-10-26; see `docs/ports/pes-port-FdcCamera3.md`):** item 1 is wrong on two points: `0x141EBA8C0` is a generic anime time-track sampler (u16 keys at +0x2a/+0x2c, values +0x2e/+0x30, each /30, one float per active key plus fade weights), not a per-component quaternion lerp; and the 0.001 / 0.05 constants in `0x141F7E510` are clip-end time bounds, not near/far clamps. The camera pose consumer is still undecoded.

# Open decodes: CANM camera sampler, function 0x5D, sanction inputs

Read-only RE. Image `<local>/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000). Ghidra GUI bridge down → headless `dec.sh` + capstone on the image + operand-VA constants script-read. Scratch: `<workdrive>/gf-OpenDecodes-scratch/`. Labels: **CONFIRMED** = dec.sh decompile or image read this pass; **INF** = inference, reason given; **OPEN** = blocker named.

## Item 1 — Cutscene camera sampler (CANM)

### 1.1 Tick unit — SETTLED (CONFIRMED, with one correction)

The CANM sampler divides every key time by **`DAT_142591140 = 30.0f`** (CONFIRMED image read, `dec_chan.txt` lines 48/54/56: `fVar17 = (float)iTime / DAT_142591140`). So **CANM key times are at 30 fps** (30 keys/s). The cut-table record `+0x04` startCutTime is in **1/60 s ticks** (`pes-re-fdc-format.md` §3). Therefore the record tick → CANM-frame relation is **÷2** (120 ms per 1/60 tick maps to 2 ticks per 30 fps key). The cut pump `FUN_141f577b0` (`dec_batch1.txt:115-126`) walks the camera track list comparing `record.+0x04` against the track node key to activate the camera object; the per-frame sampling then runs in the anime-data sampler in 30 fps time.

**Correction to the GF reader:** `camera_cut.py` `Channel.sample`/`Canm` uses `frame_rate = 30` but the per-frame **tick→frame** mapping in the cut pump is the 1/60 tick unit; the two must not be conflated. The sampler itself is 30 fps.

### 1.2 Sampler chain — SETTLED (CONFIRMED, full VA chain)

The camera node does not sample CANM keys itself. It wraps a generic **anime-data object** and the sampler is the anime-data per-frame function:

| Step | VA | role |
|---|---|---|
| cut pump `FUN_141f577b0` | `0x141f577b0` | allocates 0x578 camera node (`FUN_141f7d010`), loads `.canm` (`FUN_141f7de00`), inserts into the track list by `record+0x04` vs track-node key (`dec_batch1.txt:111-136`) |
| camera node ctor | `0x141f7d010` | vt `&UNK_142BB1048`; 0x578 bytes; 8-channel header at `node+0x370`; near/far as channel floats at `+0x5b`/`+0x5c` |
| `.canm` load | `0x141f7de00` | stores 8 qword header at `node+0x370`..`+0x3d0`, frame-rate/zoom at `+0x3f0`..`+0x410`; builds the anime-data object at `node+0x48`/`node+0x58` (vt `&UNK_142612f98`) |
| **per-frame sampler** | `FUN_141eb9640` `0x141eb9640` | dispatches by channel kind: < `0xf0f` (0x12) and < `0x122d` (4653); calls the four per-channel samplers |
| ch sampler 1 (position, 3-comp) | `0x141ebac80` | linear lerp |
| ch sampler 2 (rotation, 4-comp) | `0x141eba8c0` | linear lerp (per-component) |
| ch sampler 3/4 (scalar fov / near / far / aspect) | `0x141ebb160` / `0x141ebb640` | linear lerp |

The per-frame sampler `FUN_141eb9640` is called from the camera node's vfunc `FUN_141f7e510` (`dec_camsample.txt:266-269`) as `FUN_141eb9640(anime_data, kind, time30fps, flags)`.

### 1.3 Interpolation rule — SETTLED (CONFIRMED, **corrects the GF reader**)

All four per-channel samplers use the **same linear lerp**; there is **no slerp / no trigonometric quaternion slerp** in the native code. The lerp, for channel value between key `k0` (time `t0`, value `v0`) and key `k1` (time `t1`, value `v1`), sampled at time `s` (both in 30 fps units):

```
f = (s - t0) / (t1 - t0)        # guard: if (t1 - t0) <= 0 use DAT_1425e49f0
out = v0 + (v1 - v0) * f          # dec_chan.txt:78-79 (ch1), :270-271 (ch2), same shape :380-585
```

Key constants (CONFIRMED image read):
- `0x142591140` = 30.0f (key-time scale, 30 fps)
- `0x14256ae1c` = 1.0f (max t clamp; `f` clamped to [0,1], negative→0)
- `0x1425e49f0` = 9.9999997e-05f (divide-by-zero guard)

**Correction to the GF reader:** `camera_cut.py:367-384` uses `slerp(q0, q1, t)` for the rotation channel. PES 2021's CANM rotation channel (`0x141eba8c0`) is a **per-component linear lerp of the quaternion components** (4-comp, `ncomp=4`), not a slerp. The port must match the per-component lerp to be bit-faithful; slerp will diverge on any rotating shot. (Both readers treat position and fov as linear, which matches.)

### 1.4 near / far application — SETTLED (CONFIRMED)

- near / far are CANM **channel scalars** (`CH_NEAR`=3, `CH_FAR`=4), constant per shot (single-key), read at `node+0x5b`/`node+0x5c` and pushed to the render camera object by `FUN_141f7ecc0` (`dec_camsample.txt:263-266` → `FUN_141ebf690`/`FUN_141ebfab0` with the `+0x5b`/`+0x5c` bytes). They are not lerp'd per frame (single key).
- The camera node's vfunc `FUN_141f7e510` (`dec_camsample.txt:210-292`) clamps the far plane against the near with **`0.001`** (`0x1425a9668`) and a **`0.05`** floor (`0x1425a8a28`) before applying zoom (`FUN_141af7a90`). Confirmed constants: `0x1425a9668` = 0.001, `0x1425a8a28` = 0.05.

### 1.5 What is still OPEN (precisely)

- **The exact per-frame tick that drives `FUN_141eb9640`'s `time30fps` argument.** The 30 fps key scale is CONFIRMED, but the value passed per simulation tick (the `param_3` float) comes from `FUN_141f7e510`'s caller chain and the cut pump's clock read (`FUN_141af3100`/`FUN_141af3400`); the precise tick→(0..1 over the shot) mapping is not isolated. Blocker: decode the camera node's update caller in the cut pump (the `FUN_141f7e510` call site) and read the clock read. Mechanically it is the anime-data "current time" accessor, not a new constant.
- **Which of the 6 channels is which in the raw `.canm` file** is settled by the GF reader's channel indices (CONFIRMED by install-byte coverage, `pes-re-fdc-format.md`), but the native `FUN_141eb9640` dispatch compares the channel id against `0xf0f` and `0x122d` (4653); the id→channel-index map (0 rotation,1 position,2 fov,3 near,4 far,5 aspect) is INF from the GF reader, not re-derived from the image this pass.

## Item 2 — Function 0x5D (ANIME_ANIME_DATA_DRIVEN)

### 2.1 Action id 0x5D — CONFIRMED
- Id 0x5D = `ANIME_ANIME_DATA_DRIVEN` (anime name table; `pes-re-trap-pick.md` §3 / OwnerPickup2 §4, image read `0x1434A5470`).
- The 0x5D **accessors** (CONFIRMED, `dec_5d_path.txt`):
  - `FUN_1406d9010(anime+0xC40, id)`: returns `*(anime+0xC40+0x1528+8·id)` if `id < 0x5e` and non-null, else `*(anime+0xC40+0x1530)` (the default action). For id 0x5D the slot is `anime+0xC40+0x1528+8·0x5D` = `+0x16D88`.
  - `FUN_1406b4580(anime, 0x5D, ball, out)`: `obj = FUN_1406d9010(anime+0xC40, 0x5D)`; `out = obj->vt[+0x68/8](obj, anime, ball, out)`. This is the clip-selection call. **The `+0x68` slot is virtual and the 0x5D action's vtable is filled at runtime** — so the clip family cannot be read statically. This is the blocker.

### 2.2 The 157.5° facing gate — SETTLED (CONFIRMED, fresh decompile, enough to port)

`FUN_1409f8220` (reaction entry 3) 0x5D branch, `dec_batch1.txt:253-299`. The facing gate applies **only when `cmd+0x8D0 != 0`**:

```
facing  = wrap360( *(float*)(cmd+0xE48) )                       # dec_batch1.txt:268,270-272
table   = FUN_140439f90(anime, playerSlot, playerId)            # :273-274
target  = wrap360( table )
d       = facing - target
if (d > 180)  d -= 360          # _DAT_142598598 = 360  (normalize to (-180,180])
if (|d| > 157.5)  → skip 0x5D  (goto FUN_1409f8538, fall to TRAP)   # :292-295
```

`FUN_140439f90(anime, slot, id)` (`dec_batch1.txt:160-175`) = `wrap( table[id] + 180 )` where `table = FUN_140a71260()` (the per-player table; body not read this pass). Wrap helpers: `+180` = `_DAT_142598580` = **180.0** (CONFIRMED), `+360` = `_DAT_14259858c` = **360.0** (CONFIRMED), `+1` = `_DAT_14256ae1c` = **1.0** (CONFIRMED), and the threshold `_DAT_1425cca3c` = **157.5** (CONFIRMED).

Gate order in entry 3 (CONFIRMED): distance `FUN_1409fa400` → `*currentAnimeId != 0x5D` → `FUN_1409fbb40` (action/stage) → `FUN_1409fbba0` (ball height ≤ 0.2174 m at 10 ticks + rel speed < 38 km/h) → **facing gate** → `FUN_1406b4580(anime,0x5D,…)`; success sets `+0x874=3`, `+0x878=0x5D`, returns (no contact frame, no `+0xE6C`).

**Constants (CONFIRMED image read this pass):** `0x142598580`=180.0, `0x14259858c`=360.0, `0x142588218`=-1.0, `0x1425cca3c`=157.5, `0x14256ae1c`=1.0. (Ball gate: `0x1425a9bd4`=0.2173719, `0x14263e398`=38000.0, `0x142598590`=3600.0, `0x14351ae50`=54 (gs), `0x14259bdd0`=0.6944444 — per OwnerPickup §2.3, not re-read this pass.)

### 2.3 What is still OPEN (precisely) — the clip family / action class

- **The 0x5D action's vtable and clip family are not statically decodable.** The slot `anime+0xC40+0x16D88` is written at runtime by a scaled-index store into the action table; the writer was not isolated this pass (the 8 per-slot `disp32` stores to `+0x1528` found by image scan belong to unrelated objects at `0x1406d9010`/`0x1408acd2e`/`0x140e1b64c`; the 3 `[reg+reg·8+0x1528]` scaled stores are all inside the accessor `0x1406d9010`). **Blocker:** find the function that allocates the `anime+0xC40` action-table object and runs the per-id init loop (a loop `for id<0x5E: table[id] = makeAction(id)`), then read the `id==0x5D` arm. That arm's constructor sets the vtable; the vtable's `+0x68` slot is the clip picker and its clip id(s) are the clip family. This is the single remaining decode for item 2 and it gates wiring the branch into `elizacontroller.cpp kReceive`.
- The facing gate, action/stage gate, and ball gate are all ported and tested (OwnerPickup §2.3); only the clip is missing. The gate is enough to port **the refusal logic** now; issuing the clip waits on the action-class decode.

## Item 3 — Sanction inputs

### 3.1 Event recorder `0x140a3dc40` → ring push `0x140a3dac0` — SETTLED (CONFIRMED, fresh)

**`FUN_140a3dc40(M, record*)`** (`dec_recorder.txt`, body `0x140a3dc40..0x140a3e13c`):
- `M` (param_1) = the large match event object (0x75000+ bytes); `record*` (param_2) = the 0x320-byte event record.
- **Dispatch key = `record+0` (param_2[0])** (`dec_recorder.txt:38`). Two branches:
  - **kind 0x1f (31)**: writes a **per-player secondary ring** at `M+0x27400 + record+0x5a·0x26ac8` (100-slot, 0x640 B stride) and a **timestamp ring** at `M+0x271f0`/`+0x271f4` (0x40 = 64 slots, 8 B). `record+0x5a` = the player index; `record+0x5b` = a "use global table" flag (0 → copy from `0x1436f3a40`, else from `M+0x4b18 + record+0x641c·800`). Copies the 0x320 record into the slot (loop `:99-131` and `:196-249`).
  - **all other kinds**: looks up `kind` in a **kind table at `UNK_142642e60`** (`dec_recorder.txt:269`: `iStack_678 = [UNK_142642e60 + kind·4]`). If `table[kind] < 10` → **ring push** `FUN_140a3dac0(table[kind]·0x1908 + M)` (`:271`); else → `FUN_140a3db80(M+0xfa50)`. Then writes the **main timestamp ring** `M+0x271f0`/`+0x271f4` (64 slots, 8 B: `{tick, kind}`) and records `{tick, kind}` into it (`:288`). kind 7 has extra per-field copies (`:290-300`).

**`FUN_140a3dac0(base, record*)`** (`dec_ringpush.txt`, body `0x140a3dac0..0x140a3db78`) — the 0x320-byte **ring push**:
- `base` = `M + kind·0x1908`. Layout per ring:
  - `base+0x1900` = **write counter** (saturates at 8 → max 8 live slots);
  - `base+0x1904` = **read/write index** (saturates at 7 → wraps to 0);
  - **record ring** = `base + index·0x320` (800 B stride, `:27`), 0x320 bytes = the source record copied verbatim (`:28-65`, 100 qword moves = 0x320 B).
- Returns the new index (`base+0x1904`).
- **So the kind-4 record's `+0x20` (the source record's byte 0x20) is copied verbatim into `ring[kind4] + index·0x320 + 0x20`.** The ring is a last-8 FIFO of the 0x320-byte source record. The register meaning: param_1 = ring base (M + kind·0x1908), param_2 = the 0x320-byte source record.

### 3.2 Card-history state `0x140a81230` — SETTLED (CONFIRMED, with one correction)

`0x140a81230(A1, kind, side, r9d)` reads the **card-level** from A1's card table. Body (fresh, `dec_cardtable.txt` / `dec.sh` this pass):
- **side-1 path** (param_3==1): → `0x140a80ef0(A1, kind, key, &out)` → `0x140a80d10(E0, G+0x2AD0, kind, key, &out)` (no-`.pdata` fragment).
- **side-0 path** (param_3==0): → `0x140a80cf0(A1, kind, key=5, r9d)`.
- `0x140a80cf0` is a **thunk**: `mov rcx,[rcx+0xe0]; jmp 0x140a80d50`. The real byte-reader is **`0x140a80d50`** (CONFIRMED disasm this pass): loops `i=0..1` over the two sides, `base = FUN_140a3e8a0(X) = X+0x750D0`, reads `byte(base + i·0x59650 + key)` via `0x1408c9df0(base, key)`; then `result = count[side] − count[side^1]` (with `r9d` added if nonzero).

**Card-count byte tables — CONFIRMED, corrects the prior sanction doc:**
- `X` = A1 (the ANALYZE object); `base = 0x140a3e8a0(X) = X + 0x750D0`.
- **side-0** count = byte read at `X + 0x750D0` (key).
- **side-1** count = byte read at `X + 0x750D0 + 0x59650` = **`X + 0xCE720`** (key).

The prior `pes-re-sanction-switch.md` item 1 listed the two card locations as `E0+0x750D0` and `E0+0x59650`. **Correction:** the second is `E0+0x750D0+0x59650` = **`E0+0xCE720`**, not `E0+0x59650`. The `+0x750D0` accessor `0x140a3e8a0` is applied to **both** reads (the decompile shows `lVar5 = 0x140a3e8a0(E0)` then `0x1408c9df0(lVar5 + 0x59650, key)`).

- **Who writes it:** the builder `0x140480910` writes A1's card **table** (entries at `A1+0xC+0x18·i`, kind 0x1B, at `+0x10`/`+0x14`; sets `A1+0x10`; and `req+0x18`/`+0x24` = the `A1+0x18` block with 0, **not** `A1+0x38` — see 3.3). The **card-count byte tables** at `A1+0x750D0` and `A1+0xCE720` are a **separate** small per-key byte store; the builder does not write them. **OPEN (precise):** the writer of the card-count bytes at `A1+0x750D0`/`+0xCE720` (and `G+0x2AD0` for side-1) — no store to those offsets found in the builder; it is a different subsystem (card state accumulation). Blocker: scan for stores to `+0x750D0`/`+0xCE720` on the ANALYZE object, or trace the card-award path.
- **r9d of the side-0 call:** `0x140a81230` side-0 calls `0x140a80cf0(param_1, param_2, 5)` — only 3 args, so **r9d = param_4 of `0x140a81230`** (unmodified 4th reg). At the tree call site `0x14047f754` the tree does **`xor r9d, r9d`** before `call 0x140a81230` (`tree.asm:378`), so **r9d = 0** → no adjustment to the count. **CONFIRMED.**

### 3.3 Kind-4 record `+0x20` meaning and writer — SETTLED as a precise blocker (CONFIRMED negative)

- The kind-4 record is the **0x320-byte event record** for kind 4, copied verbatim into `ring[kind4]+index·0x320` by the ring push (3.1). `+0x20` is byte 0x20 of that source record.
- **The sanction builder `0x140480910` does NOT write the kind-4 `+0x20` field** (A1+0x38). Confirmed by capstone scan of `0x140480910..0x14048101e`: zero memory ops with base register and displacement 0x20 or 0x38. The builder's only A1-region stores are the card table (`[rsi+0x10]`,`[rsi+0x14]`,`[rsi+0x18]`,`[rsi+0x24]`,`[rsi+0x28]`, indexed by `[rsi+rdx]` with `rdx=kind·0x1C`) and `req+0x18`/`+0x24` (= `A1+0x18` block) set to 0.
- **Meaning:** not the fouler, not the victim slot (FoulOffside4 §3.3); values 0/1 across captures. The only direct `qword store [reg+0x38]` in `.text` is at `0x1415e6170` (INF: a different object; not on the sanction path).
- **Writer:** OPEN (precise). The kind-4 `+0x20` is written by the ANALYZE event-dispatcher path that builds the kind-4 record before it reaches the recorder `0x140a3dc40` (the recorder only copies). **Blocker:** identify the function that assembles the 0x320-byte kind-4 record (the `param_2` passed to `0x140a3dc40` for kind 4) — it is the caller in the event pipeline, not the sanction builder. A write-watchpoint on `A1+0x18+0x20` across one kind-4 event, or a full decompile of that record assembler, resolves it.

## Corrections to prior docs (this pass)

1. **`pes-re-fdc-format.md` / `camera_cut.py`:** the native CANM rotation channel is a **per-component linear lerp**, not a slerp (`0x141eba8c0`, 4-comp). The GF reader's `slerp()` at `camera_cut.py:367-384` is a divergence from PES 2021.
2. **CANM tick unit:** the sampler's key-time scale is `0x142591140 = 30.0f` (30 fps). The record `+0x04` is in 1/60 s ticks (record domain) — do not conflate the two.
3. **`pes-re-sanction-switch.md` item 1 (card tables):** second card-count location is `A1+0xCE720` (`0x750D0+0x59650`), not `A1+0x59650`. The `0x140a3e8a0(X)=X+0x750D0` accessor is applied before the `+0x59650` in both reads.

## Recommended next steps (ranked)
1. **Item 2 clip family:** find the `anime+0xC40` action-table allocator + per-id init loop; read the `id==0x5D` arm → vtable → `+0x68` clip picker + clip id(s). Gating item for wiring 0x5D into `kReceive`.
2. **Item 1 tick:** decode the `FUN_141f7e510` caller in the cut pump to isolate the per-tick `time30fps` value (the 0..1-over-shot mapping).
3. **Item 3 card-count writer:** scan for stores to `A1+0x750D0`/`+0xCE720` on the ANALYZE object (card-award path).
4. **Item 3 kind-4 `+0x20`:** decompile the 0x320-byte kind-4 record assembler (the `param_2` caller of `0x140a3dc40`) or a write-watchpoint on `A1+0x38`.
