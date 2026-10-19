# Open-play kick approach: horizontal alignment (S1Watch, round 7)

> **Main audit 19-10-26:** `FUN_1406B10D0` operands re-read: `movss xmm6,[0x14256AE1C]`=1.0 at `0x1406B1133`, `divss xmm1,[0x14259A764]`=60.0 at `0x1406B1148`, `addss xmm0,[0x142565600]`=0.5 at `0x1406B116B`: match. PARTIAL: the approach planner is not found; live sample n=6 (INF). The move target is `cmd+0x20` (ClipWords correction), not `cmd+0x14`. The per-frame ball address `0x3562E904` is session-specific.

Status: **partial**. The kick writer's ownership of translation is settled (static). The clip tick count is decoded (static). The live approach geometry is measured on 6 kicks (17 detected; 11 rejected by the run-up filter). **The planner that places the kicker's approach point is not identified.** Tags: **CONFIRMED** = asm/decompile or a live read with its source; **INF** = inference; **OPEN** = blocker.

## 1. Clip tick count: `FUN_1406B10D0` (CONFIRMED, asm `0x1406B10D0..0x1406B1160`)

`int FUN_1406b10d0(longlong *motion, uint id, float rate, uint track)`

| step | VA | instruction | meaning |
|---|---|---|---|
| record | `0x1406B10E1..F8` | `cmp $0x1125,%edx; imul $0x390,%rax,%rcx; add (%r8),%rcx` | `rec = motion[0] + id·0x390` (id < 0x1125), else `motion[1]` |
| track count | `0x1406B10FB` | `movzbl 0x4(%rcx),%eax` | `n_tracks = byte(rec+4)` |
| track header | `0x1406B1107` | `mov 0x90(%rcx,%rax,8),%rdx` | `hdr = *(rec+0x90+8·track)`; if out of range, `hdr = &DAT_1436F23D8` (`0x1406B1114`) |
| frame word | `0x1406B111B..20` | `mov (%rdx),%ebx; shr $0xD,%ebx; add $0x2,%bl` | `n = byte((w>>13)+2)` |
| fps | `0x1406B1123` | `call 0x1E532D0` | game speed (54) |
| rate | `0x1406B112E..33` | `xorps; comiss; movss 0x256AE1C` | `rate ≤ 0 → 1.0` (`0x14256AE1C`) |
| result | `0x1406B1148..` | `divss 0x259A764`; `addss 0x142565600` | `trunc( (2n/rate)·(fps/60.0) + 0.5 )` |

Constants (script-read, `pes_full.bin`): `0x14259A764` = 60.0 (`0x42700000`), `0x14256AE1C` = 1.0 (`0x3F800000`), `0x142565600` = 0.5 (`0x3F000000`), `DAT_1436F23D8` default record.

Caller in the kick band: `FUN_1407302B0` calls `(motion, rate, 0, 0)` → **track 0, rate 1.0**, i.e. the whole clip, not a hit frame.

## 2. The kick writer does not write the horizontal root (CONFIRMED)

- Caller chain: `FUN_14072F3B0` (111 lines) → `FUN_1407302B0` (757 lines) when `local_44 < (f(P,7)+f(P,8))·0.5` (`FUN_1406E6A40(P,7/8)`) and `FUN_140A6E450(P+0xACB) == 0`. Other branches: `FUN_14072F670`, `FUN_140730000`.
- `FUN_1407302B0` adjusts: `P+0x498` (height, `FUN_141EB3830` + `FUN_141EB3AE0`, guarded by `iVar25 ≥ 1`), `P+0x4D0` (yaw, `FUN_141EB35A0`, `round(54·12/60)` ticks). It does **not** call `FUN_141EB39A0` (`AdjustTransXZ`).
- `FUN_141EB39A0(adj, target, ticks, …)` (50 lines): zeroes the adjuster, then per-tick XZ delta `(f(target) − f(cur))/ticks` (`0x141EB39A0`, decompile lines 12–40). So XZ root motion to a target over `ticks` is issued elsewhere (58 image sites).

## 3. Live measurement (CONFIRMED method; sample small)

**Ball identification.** Three ball sources were tried and rejected:
- `G+0x40` (`*(G+0x40)` ball copy): changed in 44 of 2,563 ticks.
- `pes_state.find_ball` (signature + velocity): a copy that changes every 7 frames (`ball_addr.txt`), and later stale.
- The chosen ball: **`0x3562E904`** (found by the full signature scan `cands2.txt`, then per-frame change in play: 85% of frames; 4,599-frame recording: 48% of frames within 2 m of a player, 2 teleports > 3 m, 847 steps of 0.2–0.5 m/frame). Post-contact flight is smooth (steps decay 0.28 → 0.22 m/frame over 12 frames, contact at idx 1237). **INF(high): this is the physics ball.** Its address is not stable across matches.

Players: `G+0x1DD8+0x28·i → +0x55C/+0x560/+0x564` (x, y, z), changing on every frame (checked: no 7-frame cadence). Frame counter `MI+0x1198`, +1 per recorded tick (`rec4.json`, 4,599 ticks, 90 s, in play `MI+0x116C == 1`).

**Kick detection:** ball step ≥ 0.2 m/frame, step jump ≥ 0.10, previous step < 0.15, kicker = nearest player (≤ 1.5 m) at the frame before.

**Results** (`kickan3.py`; 17 detected, 6 analysed: 11 rejected because the 12-frame approach had no net displacement):

| quantity | value (mean; median; min..max; n) |
|---|---|
| ball kick speed (m/s) | 15.5; 15.0; 10.8..21.7; 6 |
| approach speed, 12 frames before contact (m/s) | 2.5; 2.4; 1.1..3.8; 6 |
| contact offset along the approach direction (m, ball ahead of root) | 0.31; 0.55; −0.25..0.72; 6 |
| contact offset lateral (m, signed) | 0.07; 0.08; −0.56..0.53; 6 (median \|lat\| 0.27) |
| run-up angle, approach vs kick direction (deg) | 78; 73; 3..144; 6 |
| frames from root-speed peak to contact | 18.3; 17.5; 14..24; 6 |

The root is the player's `+0x55C` position; the foot is **not** measured (no skeleton read), so the lateral offset is a root-to-ball figure, not a foot offset.

## 4. What is not established

1. **The planner of the kicker's approach point and of the foot offset.** Not found in the kick writer (§2). The candidates are the carrier's move target (`cmd+0x14`, AS §1.1) and an `AdjustTransXZ` issued before the kick (58 sites; the dribble-touch band `0x14073D…–0x14074…`). Neither is decoded.
2. **The arrival tick vs the clip's hit frame.** The clip (`FUN_1407302B0` track 0) gives `trunc(0.5 + 2n·54/60)` ticks, but the hit frame of that clip is not read, so the two cannot be compared yet.
3. **Run-up consistency.** In the 6 analysed kicks the run-up angle ranges from 3° to 144°; the approach speed is 1–4 m/s. These are not running kicks. Either the sample contains dribble touches or the approach is curved; a larger sample is needed.

## 5. Next steps

1. Decode the carrier's move-target writer for the kick (`cmd+0x14`), starting from `FUN_14072F3B0`'s branches and the `FUN_1406E85B0` root-target solver (46 call sites).
2. Read the kick clip's hit frame (the track's contact frame) and compare with the tick count of §1.
3. Record ≥ 40 kicks (with the same ball identification) to fix the run-up distribution and the lateral offset.
