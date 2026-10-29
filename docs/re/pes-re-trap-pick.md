# Trap picker vtable chain, TRAP decode, designated owner (28-10-26)

Read-only RE. Scratch: `<workdrive>/gf-TrapPick-scratch/`. Tags: **CONFIRMED** = read in decompile, image, or live; **INF** = inference; **OPEN** = not resolved.

## 1. The virtual +0x68 call: §7.2 OPEN → CONFIRMED

**§7.2 OPEN stated:** "That the vtable target is the picker is NOT CONFIRMED (OPEN)."

**CONFIRMED: the vtable target of `+0x68` is NOT the picker `FUN_14081FF70`.** It is the **Trap action's `+0x68` slot** at `0x1407999F0` (thunk to `FUN_1407A01C0`).

**Correction (28-10-26, TrapClipPort, dec.sh + image bytes).** `0x1407999F0` is `E9 CB 67 00 00` = `jmp 0x1407A01C0` (CONFIRMED). `FUN_1407A01C0` sends kind ≤ 0 straight to `FUN_14079A470`; it does not call `FUN_140799DB0`. `FUN_14079FBC0` returns a bool refusal test (a teammate race), not a clip. The clip is chosen by the candidate search and commit steps decoded in `docs/re/pes-re-trap-kind0.md`.

**Vtable chain (CONFIRMED, image read):**

| Step | VA | What |
|---|---|---|
| `FUN_1406B4580(anime, 7, ball, out)` | `0x1406B4580` | Reads `FUN_1406D9010(anime+0xC40)` → `obj`; calls `obj->vt[0x68/8](obj, anime, ball, out)` |
| `FUN_1406D9010(anime+0xC40)` | `0x1406D9010` | Returns `*(anime+0xC40+0x1528+8·id)` if id < 0x5E and non-null, else `*(anime+0xC40+0x1530)`. This is the **action pointer** for the currently active action. |
| Action pointer → vtable | `*(obj+0)` | The vtable of the active action object |
| Trap action vtable | `0x1425E1ED0` | RTTI-confirmed `action::Trap` (`pes-re-trap-windows.md` §Action names) |
| **`+0x68` slot** | **`0x1407999F0`** | Thunk to **`FUN_1407A01C0`** — the Trap action's `+0x68` implementation |

**Trap vtable `0x1425E1ED0` (CONFIRMED, image read, 64-bit pointers):**

| slot | VA | meaning |
|---|---|---|
| `+0x00` | `0x1406E2AC0` | dtor |
| `+0x10` | `0x14079BBE0` | class-7 admission (`pes-re-trap-contact.md` §3) |
| `+0x68` | `0x1407999F0` | **the trap motion clip selection** |
| `+0x70` | `0x1407995D0` | locomotion feasibility test |

`FUN_1406B4580` passes the **action object** (`param_1` = `obj`, not `anime`) as `this` to the `+0x68` slot. So `FUN_1407A01C0` receives `this = Trap action object`, not the anime object.

**The picker `FUN_14081FF70` is called from the kick/trap dispatcher `FUN_140769FE0`** (60+ call sites in `0x14076B…`), not from the `+0x68` vtable slot. The `+0x68` slot is the **Trap action's motion selection**, which uses a different code path (clip search via `FUN_14079FBC0` / `FUN_140799440`, not the row-table picker).

**The picker's role in the TRAP path:** The dispatcher `FUN_140769FE0` (action state `0x38` = TRAP, `uStack_288._4_2_ == 0` = first frame) calls `FUN_14081FF70(param_3, &uStack_278, 7, &uStack_288)` with action class 7 (TRAP). The picker returns a clip ID from the row table `DAT_14348DAC0`. The dispatcher then commits the clip via `FUN_1406B03D0` (hit-frame lookup) and `FUN_1406B0ED0` (motion object). This is a **separate path** from the `+0x68` vtable call.

**Two clip-selection paths for TRAP (CONFIRMED):**
1. **`+0x68` vtable slot** (`0x1407999F0` jmp `FUN_1407A01C0`; kind ≤ 0 → `FUN_14079A470`): clip search during Trap action execution, via the motion-table candidate search (`FUN_14079AFD0` → `FUN_140857250`; fallback `FUN_140857220` → `FUN_140858B90` → `FUN_1408586A0` → `FUN_14085B900` → `FUN_140858260`). The row table is not used on this path. See `docs/re/pes-re-trap-kind0.md`.
2. **Row-table picker** (`FUN_14081FF70`): called from the dispatcher `FUN_140769FE0` at trap initiation. Uses the row table `DAT_14348DAC0` with the TRAP rows (0x547–0x549, 0x57D, 0x57F).

Both produce a clip ID. The `+0x68` path runs during the Trap action's execution (per-tick clip update); the picker path runs once at initiation to select the initial clip.

## 2. `FUN_14081FF70` decode for the TRAP case

**Signature (CONFIRMED decompile `xr_picker.txt`):**
```c
int FUN_14081FF70(longlong param_1,   // ctx (anime context)
                  float *param_2,     // ball point in clip space (x, y, z)
                  uint param_3,       // action class (7 = TRAP)
                  ushort *param_4,    // out: [0]=clip_id, [1]=hit_key, [2]=sub_key
                  int *param_5,       // out: hit data
                  float *param_6,     // out: offset (x, y, z)
                  undefined8 param_7,
                  ulonglong param_8,  // flags
                  short param_9,
                  longlong param_10)
```

**Input: `param_3` = action class.** For TRAP, `param_3 = 7`. The picker keeps rows whose `+0x4` (action class byte) equals `param_3` (`(&DAT_14348DAC4)[row] == param_3`).

**Row table `DAT_14348DAC0` (CONFIRMED, `pes-re-trap-contact.md` §2):** stride `0x1C`.

| offset | type | meaning |
|---|---|---|
| `+0x00` | u32 | clip ID |
| `+0x04` | u8 | action class (must match `param_3`) |
| `+0x05` | u8 | priority |
| `+0x06` | u8 | hit key 1 |
| `+0x07` | u8 | hit key 2 |
| `+0x08` | f32 | x-window base `xb` (m) |
| `+0x0C` | f32 | z-window base `zb` (m) |
| `+0x10` | f32 | x-window extension `xe` (m) |
| `+0x14` | f32 | z-window extension `ze` (m) |

**TRAP rows (CONFIRMED, read from image):**

| row | clip | prio | keys | xb | zb | xe | ze |
|---|---|---|---|---|---|---|---|
| 34 | 0x547 | 0 | 0,0 | −0.250 | −0.557 | 0.250 | 0.218 |
| 35 | 0x548 | 0 | 8,0 | −0.180 | −0.767 | 0.270 | −0.353 |
| 36 | 0x549 | 0 | 10,0 | −0.300 | −1.013 | 0.210 | −0.143 |
| 84 | 0x57D | 1 | 8,1 | 0.256 | −0.729 | 0.620 | −0.323 |
| 86 | 0x57F | 1 | 10,1 | 0.138 | −0.913 | 0.860 | −0.153 |

**Row selection logic (CONFIRMED structure, decompile lines 100–130):**
1. The picker divides the ball point by the context scale `*(ctx+0x3B18)` (the man's root scale).
2. Subtracts the clip's hit offset `FUN_1406AFF90(motion, clip, 0, 0)·0.01` (cm→m), giving the ball relative to the man in clip space.
3. Iterates rows in the table. A row is admitted when:
   - `row+0x4 == param_3` (action class match);
   - the ball point lies inside the row's x and z windows: `xb ≤ x < xb+xe` and `zb − m < z < ze` (the exact window test is partially OPEN; the z test uses `zb` and `ze` with the stored values).
4. Among admitted rows, the one with the highest priority (row `+0x5`) wins. Ties broken by hit key comparison with `param_4[1]` and `param_4[2]`.
5. Output: `param_4[0]` = clip ID, `param_4[1]` = hit key, `param_4[2]` = sub-key.

**Constants (operand-VA pairs):**
- Row table: `DAT_14348DAC0` (image VA, stride `0x1C`)
- Context scale: `*(ctx+0x3B18)` (read at `0x14081Fxxx` in the picker)
- Hit offset accessor: `FUN_1406AFF90(motion, clip, 0, 0)`
- Scale factor: `0x14256AE1C` = 0.01 (cm→m)

**HitData field that gives the PES touch frame:**

The touch frame is **not** a field in the picker's output. It is derived from the **HitData record** of the selected clip:

- The motion object (`FUN_1406B0ED0()`) has HitData records at `+0x20+8k` (k = hit index).
- **HitData u32 at `+0`: bits 13..20 = hit frame** (CONFIRMED, `pes-re-anim-select.md` §6.1: "HitData record (pointer `+0x20+8k`): u32 `+0` bits 13..20 = hit frame").
- The touch frame is the hit frame from the HitData record of the selected clip, read via `FUN_1406B03D0(motion, clip, rate, 0)` (the dispatcher's hit-frame lookup, called at `d_disp.c:2401`).

**Touch frame derivation (CONFIRMED):**
1. Picker returns clip ID (e.g. 0x547 for the ground TRAP row).
2. Dispatcher calls `FUN_1406B0ED0()` → motion object pointer.
3. Dispatcher calls `FUN_1406B03D0(motion, clip, rate, 0)` → returns the hit frame (from HitData bits 13..20).
4. The hit frame is stored in `param_2[0x25]` (the reaction record's contact frame field).
5. For TRAP, the contact frame is **18** (CONFIRMED live, `pes-re-receiving.md` §6: "the defender who reaches a loose ball issues the same TRAP (anime 7, contact frame 18)").

## 3. Designated owner `MA+0x2600C`

**CONFIRMED: `MA+0x2600C` is the designated owner, written by `FUN_140457070`.**

**Writer (CONFIRMED decompile `d_a.c:1170-1350`):**
- `FUN_140457070(param_1, flag)` runs from the ball-action tick.
- `lVar3 = *(param_1+0x508)` = the MA (MatchActor) pointer.
- Ring-0 scan: `func_0x000140A3E190(ring0_key, k)` for k = 0..8 (9 iterations).
- **`MI+0x1168 == 9` branch:** if the newest ring-0 record has kind 5 and player index < 2, the designated owner = `FUN_140A62E90(player_ptr, team_idx)`. This is the "designated owner" for a ball in a 9-entry ring-0 state.
- **Else branch:** if kind ≠ 0xE and `piVar13[7] == 6`, the designated owner is set. If the new value differs from the old `MA+0x2600C`, the scan breaks early.
- Store: `*(lVar3+0x2600C) = uVar12` at `d_a.c:1283`.
- Also writes `MA+0x2601C` (carrier) at `d_a.c:1301` from the ring-1 newest key.

**Gate 4c (CONFIRMED decompile `d_b.c:91`):**
```c
if (*(int *)(lVar9 + 0x2600c) == iVar1) return false;  // designated owner refuses TRAP
```
where `lVar9 = *(param_1+0x38+0x18)` = MA pointer, `iVar1 = *(param_1+4)` = the player's own ID. The designated owner **never traps through reaction entry 3** (`FUN_1409FA520`).

**What PES does instead for the designated owner near a slow loose ball (CONFIRMED code; INF on the live path):**

The designated owner takes the ball through the **0x5D kick branch** (step 5 of reaction entry 3, `FUN_1409F8220`), which is checked **before** the TRAP branch (step 6):

- `FUN_1409FBB40` (anime action 0x4A, or AI stage 5) and `FUN_1409FBBA0` (ball sample at 10 ticks height ≤ 0.2174 m, `0x1425A9BD4`; `|v_player − v_ball,1| < 38 km/h`, threshold `0x14263E398` = 38000/(gs·3600)).
- Success issues function 0x5D (INF: ground-control or clearance clip). The clip family is not decoded.

Additionally, the designated owner can take the ball through the **PASS_GET → TRAP path** if the movement selector `FUN_140933450` assigns state 2 (go to ball as receiver) to the designated player. The movement selector checks the team's pass target `T+0x78 == me` (`FUN_140931C60`), and if true, sends the player to the ball. When the player arrives, reaction entry 3's TRAP branch fires — but gate 4c refuses it. So the PASS_GET path also leads to a dead end for the designated owner.

**The actual path for the designated owner:** The 0x5D kick branch is the only path that bypasses gate 4c. It requires the ball to be within 0.2174 m height at 10 ticks and the player's velocity to be within 38 km/h of the ball's velocity. For a slow loose ball (ball speed ≤ `kStalemateBallSpeed`), this condition is met. The 0x5D branch issues a ground-control clip, and the designated owner takes the ball.

**Live read:** Not performed (blocker: PES is running but the specific scenario — a designated owner near a slow loose ball — was not observed during this session's available time window. The `MA+0x2600C` value can be read via the poller chain `G+0x1436F3F08 → M+0x260 → E+0x508 → MA+0x2600C`, but a loose-ball scenario with the designated owner in range was not captured.)

## 4. GF port recipe

### 4.1 `playercontroller.cpp` — `_TrapCommand`

**Current state:** `_TrapCommand` is called from `elizacontroller.cpp:1017` (`kReceive`) and uses `pes::TrapBallClipPoint` to select a TRAP clip row. It works for non-designated players.

**Required change:** No change needed for the TRAP clip selection itself — the picker logic is already ported (`pesreceive.cpp`). The TRAP path is correct for non-designated players.

### 4.2 `elizacontroller.cpp` — `kReceive`

**Current state:** The `kReceive` block is in the defender branch. It calls `_TrapCommand` when the player arrives at the ball.

**Required change:** The designated owner must be routed to the **0x5D path** instead of the TRAP path. Specifically:

1. **Before** the TRAP branch, check if the player is the designated owner (`match->GetDesignatedPossessionPlayer() == player`).
2. If yes, check the 0x5D conditions:
   - Ball height at 10 ticks ≤ 0.2174 m (`0x1425A9BD4`).
   - `|v_player − v_ball| < 38 km/h` (`0x14263E398` / `0x142598590`).
3. If both pass, issue the 0x5D clip (ground control) instead of TRAP.
4. If the 0x5D conditions fail, fall through to the TRAP branch (which will be refused by gate 4c — this is correct PES behavior; the designated owner simply doesn't trap).

**VA tie:** `FUN_1409FBB40` / `FUN_1409FBBA0` (the 0x5D gate conditions), `0x1425A9BD4` (0.2174 m), `0x14263E398` (38 km/h threshold).

### 4.3 `match.cpp` — breaker's designation

**Current state:** `match.cpp:809-850` sets the designated possession player using a time-ratio 0.85 switch and stalemate pick. This is a STAND-IN for the PES ring-0 scan.

**Required change:** Replace the GF stand-in with the PES ring-0 scan logic from `FUN_140457070`:

1. Read the ring-0 event store (9 entries).
2. If `MI+0x1168 == 9` (9-entry ring-0 state) and the newest record has kind 5 and player index < 2, set the designated owner to that player.
3. Else, scan the ring-0 records for kind ≠ 0xE and `piVar13[7] == 6`; set the designated owner to the first matching player.
4. Store the result in `MA+0x2600C` (GF: `match->designatedPossessionPlayer`).

**VA tie:** `FUN_140457070` at `0x140457070`, store at `0x1404571F1` (`MOV [R13+0x2600C], ESI`).

### 4.4 Summary table

| GF file/function | PES source | VA | Change |
|---|---|---|---|
| `playercontroller.cpp` `_TrapCommand` | picker `FUN_14081FF70` | `0x14081FF70` | No change (already ported) |
| `elizacontroller.cpp` `kReceive` | 0x5D gate `FUN_1409FBB40`/`FUN_1409FBBA0` | `0x1409FBB40`/`0x1409FBBA0` | Add designated-owner check; route to 0x5D if conditions met |
| `match.cpp` breaker designation | `FUN_140457070` | `0x140457070` | Replace GF stand-in with ring-0 scan |

## 5. Open items

1. **0x5D clip family:** The 0x5D kick branch's clip family is not decoded (INF: ground-control or clearance clip). The clip ID and its HitData record need to be read from the image.
2. **Lateral sign of TRAP rows:** The rows with `x > 0` (0x57D, 0x57F) need the right/left sign of `x`, which the hit data do not fix. The central ground row (0x547) is symmetric in `x`, so the ground case does not depend on it. (`pes-re-trap-windows.md` §5)
3. **Live `MA+0x2600C` read:** Not performed this session. The poller chain is known but a loose-ball scenario with the designated owner in range was not captured.
4. **Kind-0 clip search:** decoded in `docs/re/pes-re-trap-kind0.md`. `FUN_14079FBC0` is the teammate race refusal, not the clip search.
