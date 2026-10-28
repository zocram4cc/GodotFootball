# pes-port-OwnerPickup2: the refusing gate on the slow-loose-ball pickup is the GF trap-clip picker, not PES's TRAP path (28-10-26)

**Status.** `pes_stall_test` `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` is still RED: 147 live breaker firings (seed 7, 2-min `pes_loop` match), unchanged from `b095afa4` and `8eee7bfb`. Nothing landed. Branch `StoppageFlow` is still at `8eee7bfb`, the tree is clean (`git diff` on `src`, `tests`, `apps` is empty), no commit, nothing to push. The temporary probes are reverted.

**Named gate (CONFIRMED by probe).** In `_TrapCommand` (`playercontroller.cpp`, PES block), `pes::PickTrapClip` returns no clip, so no TRAP command is queued. `PickTrapClip` is the GF port of the row picker `FUN_14081FF70`. The PES reaction entry 3 TRAP branch does not call that picker: its clip comes from the Trap action's `+0x68` slot, whose chain calls the frame race `FUN_14079FBC0`, not the row table. Forcing a TRAP for the presser when the picker refuses removes every live firing (test PASSED, 16,830 forced issues; probe, reverted). So the gate is necessary for this stall. A faithful port of the PES clip choice is not done (§5).

## 1. Gate-by-gate on the presser (step 1, instrumented, reverted)

Probe: every `_ReceiveCandidate` / `_ReactionCommands` / `_TrapCommand` call for the loose-ball presser (`IsLoosePresser`, PES loop) with ball speed < 0.2 m/s. Seed 7, 2-min match, `PES21_ROOT=<workdrive>/PES21`. Probe 1 and probe 2 both give 7779 such ticks and 147 firings, so the run is deterministic here. Counts are per tick.

| gate (GF, file) | PES counterpart | fails | passes |
|---|---|---|---|
| G1 `WithinBallDistanceTable` (`pesreceive.cpp:276`) | `FUN_1409FA400` (10 m table) | 415 | 7364 |
| G2 `GetBallRetainer() == 0` (`playercontroller.cpp:387`) | none in entry 3 (INF) | 1379 | 5985 |
| G3 restart taker refused (`PesLoop` block) | gate 4d, MI+0x11C8/+0x11CC: NOT ported (GF stand-in) | 0 | 5985 |
| G4c `RefusedAsDesignatedOwner` (`d_b.c:91`) | `FUN_1409FA520`, `MA+0x2600C == me` (ported) | 0 | 5985 |
| G5 designation (`IsLoosePresser` / designated / team designated) | none: entry 3 has no designation gate (`pes-re-first-touch.md` §7.2); GF stand-in | 0 | 5985 |
| G6 `RetouchAllowed` (`FUN_1409FA720`) | `FUN_1409FA720` (ported) | 42 | 5943 |
| G7 `ShotRunsThrough` | shot-through rule (ported) | 0 | 5943 |
| reaction choice = kReceive (`ChooseMarkedReaction`) | entry 3 `FUN_1409F8220` | — | 5903 of 5943 (40 chose slide) |
| first-touch kind (`ChooseFirstTouch`) | `FUN_1409F8A50` | — | kind 0 (control) on every sampled entry |
| **clip: `PickTrapClip` (`_TrapCommand`)** | `FUN_1406B4580(anime,7)` → Trap `+0x68` (§3) | **no clip on 10,624 stall-speed TRAP evaluations; clip issued in 317 blocks** | — |

Caveat (not used as evidence): for about half the logged refusal rows, the 2-D distance on the G1 line is inconsistent with the clip-space ball point on the TRAP line (`sqrt(x²+z²)` ≠ dist). The probe attributes lines by player key and sequence, so per-row geometry is not reliable. The gate-level counts above are reliable.

Refused-TRAP geometry: the ball point is mostly 0.4–10 m ahead, outside the row windows. Row windows (`pes-re-trap-windows.md` §2, implemented as `ActionRow` in `pesreceive.hpp:42`): x in [xb, xe), z in (zb, ze), 0x547 at x [−0.25, 0.25), z (−0.557, 0.218).

## 2. Forced-trap probe (temporary, reverted)

On `!clip` for a presser, queue `Special`/TRAP clip 1351 (0x547) instead. Result: `PesStall.ALiveBall…` PASSED, 16,830 forced issues. The census has zero live firings. So the picker window is the refusing gate, and a TRAP issued to the presser does produce possession (the existing touch path works once a trap is issued, §4).

## 3. PES reaction entry 3, step by step (dec.sh headless; Ghidra bridge down)

Files: `gf-OwnerPickup2-scratch/dec_entry3.txt`, `dec_trap68.txt`, `dec_kind0.txt`.

**`FUN_1409F8220`** (entry 3, `dec_entry3.txt:1-157`, CONFIRMED):
1. refuse if `P+0x5D0 == 5` with `ai+0x1457 == 3` or `ai+0x1458 == 3` (lines 55–58);
2. refuse in a kick clip before its contact frame (lines 59–67);
3. `FUN_1409FA400` (distance, line 68);
4. refuse if the current anime id is 0x5D (`*pcVar8 == ']'`, lines 72–75);
5. **0x5D branch** (lines 76–128): `FUN_1409FBB40` and `FUN_1409FBBA0` (§4), then the facing gate (§4), then `FUN_1406B4580(anime, 0x5D, …)`; success sets `+0x874 = 3`, `+0x878 = 0x5D` and returns;
6. **TRAP** `FUN_1409F8640` (lines 131–155): `FUN_1409F8A50` (first touch), then `FUN_1406B4580(anime, 7, …)`; success sets `+0x874 = 4`, `+0x878 = 7`, `+0xE6C = 7`, and the contact frame `R[9] = FUN_1406B3D60(anime, 7)` (`d_1409f8640` line 405).

**`FUN_1406B4580`** (`dec_entry3.txt:255-267`): `FUN_1406D9010(anime+0xC40)` returns the action at `+0x1528 + 8·id` (`id < 0x5E`), then calls its `+0x68` slot.

**Trap action `+0x68`**: the Trap vtable `0x1425E1ED0` `+0x68` = `0x1407999F0` (thunk to `FUN_1407A01C0`) (CONFIRMED, image read). `FUN_1407A01C0` (`dec_trap68.txt:1-59`) dispatches on the request's kind word: `iVar3 = param_4[2]`:
- `iVar3 ≤ 0` (kind 0, control): `FUN_14079A470` (line 54). The presser's kind is 0 on every sample.
- 1–2 → `FUN_14079A310`; 3 → `FUN_14079B3D0`; 4 → `FUN_14079B3D0(…, 4)`; 5 → `FUN_14079AAE0`.

**Callee check (CONFIRMED):** the callees of `FUN_1407A01C0`, `FUN_140799DB0`, `FUN_140799440`, `FUN_14079A470`, `FUN_14079FBC0` contain no `FUN_14081FF70`. The row picker is not on the reaction's TRAP clip path. `pes-re-trap-pick.md` §1 already says the picker sits in dispatcher `FUN_140769FE0`.

**Kind-0 clip search** (CONFIRMED structure, INF on meaning): `FUN_14079A470` calls `FUN_140799440`, which walks frames `t` from `int(param_3 · DAT_1425CD060)` up to `param_3` and returns on the first `FUN_14079FBC0(anime, ball, t)` that passes. `FUN_14079FBC0` (lines 227–343 read; 344–403 not read) takes the ball sample at `t` (`FUN_140A6CF70`), the man's bearing (`FUN_140439E50`), his own arrival (`FUN_140A4D340`), and the team's men in the ring at `+0x22E4C` (stride 200 per anime slot, 2 × 11). It breaks when a mate's distance test (`(d + DAT_1425CB30C)` style) beats him. INF: this is a first-to-the-ball race test with frame-by-frame search, not a clip window.

## 4. 0x5D (step 2)

- **Id 0x5D = `ANIME_ANIME_DATA_DRIVEN`** (anime name table, VA `0x1434A5470`, entry `0x5D` → `0x142652CC0`) (CONFIRMED, image read). Cross-check: `0x4` ANIME_DRIBBLE, `0x7` ANIME_TRAP, `0x17` ANIME_KICKOFF_RECEIVER_LOOP, `0x4A` ANIME_DEMO_MOVE, `0x4F` ANIME_DEMO_OUTOFPLAY, all agree with the action names in `pes-re-trap-windows.md`.
- **Action class and clip family for slot 0x5D: NOT decoded (OPEN).** The slot is filled at runtime; I did not find its writer or vtable. Without a clip the branch cannot issue (`FUN_1406B4580` returns false and entry 3 falls to TRAP), so nothing is wired. No dead code landed.
- **Gates (CONFIRMED, `FUN_1409FBB40` lines 462–477; `FUN_1409FBBA0` lines 484–521):**
  - `FUN_1409FBB40`: if `ai+0x14 != 0`, the result is `stage == 5`; else `(byte & 0x7F) == 0x4A`. This is `KickBranchActionGate` (GF, `pesreceive.cpp:377`). Matches.
  - `FUN_1409FBBA0`: ball sample at 10 frames, height ≤ `0x1425A9BD4` (0.2173719); then 2-D squared distance of player sample (`FUN_140A6C6D0`, scale `0x14259BDD0 / gs`) to ball sample at frame 1 `<` `(0x14263E398 / (gs · 0x142598590))²` (38000 / (54 · 3600) m/frame). Matches `KickBranchBallGate` (GF, `pesreceive.cpp:385`).
- **Facing gate (CONFIRMED decode, NOT ported):** `dec_entry3.txt:85-119`. Applies only when `cmd+0x8D0 != 0`. `diff = wrap360(cmd+0xE48) − wrap360(FUN_140439F90(anime, player id))`, wrapped to ±180. Refuse if `|diff| ≥ 157.5°` (`0x1425CCA3C`). `FUN_140439F90` (`dec_entry3.txt:524-543`) = `table[id] + 180` wrapped, `table = FUN_140A71260()` (not read). Not ported because the 0x5D clip is undecoded; the gate alone would be dead code.
- **Doc conflict (OPEN):** `pes-re-receiving.md` §1.2 records zero live DRIBBLE-touch (0x5D) starts in 5896 frames and says the path needs a pad order. `pes-re-trap-pick.md` §3 says the designated owner takes slow balls through 0x5D. The image gate (stage 5 or order byte 0x4A) supports the first. Unresolved.
- Success of 0x5D sets `+0x874 = 3`, `+0x878 = 0x5D` and returns before TRAP. It sets neither `+0xE6C` nor `+0xE74`, and no contact frame. (CONFIRMED, lines 124–127.)

## 5. TRAP touch frame (step 3): already wired on this branch

- `animcollection.cpp:603-608`: for the trap family, `touchframe` = `pes::PesTouchAnimFrame(mbinfo.Hits(clip.motionNo)[0].frame)`. `HitRecord::frame` is u32 `+0` bits 13..20 (`src/pes/mbinfo.hpp:36`).
- `humanoid.cpp:2714-2718`: a Special trap command reads `touchframe` and sets `touchFrame_tmp` and `touchPos_tmp`.
- So `pes-re-first-touch.md` §7.6(b) ("GF sets touchFrame = −1 for PES clips") is stale for traps. The forced probe (§2) shows the contact path works once a trap is issued.
- NOT re-verified this session: the frame value for 0x547 (contact frame 18 from `pes-re-receiving.md` §6 is the live reference). PES's own contact frame comes from the Trap action `+0x98` (`0x1407981F0`, read from the image; body not decoded) via `FUN_1406B3D60`.

## 6. Tests and mutations (step 4)

- **Red-first:** no new test was written, since no code changed (the probes were reverted). The brief's red-first rule was therefore not exercised this ticket; stated plainly.
- **M4** (`DesignatedOwnerScan`, roster check removed: `ownerOf` → `return a`): `pes_receive_test` 1 FAILED (`PesReceiveOwner.AShotFromOutsideTheRosterTeamsIsNoOwner`), 48 passed. **KILLED.** (The previous run did not kill it.)
- **M10** (`KickBranchBallGate`, `<` → `<=`): 1 FAILED (`PesReceiveKickBranch.TheRelativeSpeedMustBeUnderTheLimitStrictly`), 48 passed. **KILLED.**
- Both reverted; tree clean.
- M1–M3, M5–M9, M11 were not re-run individually this ticket; the previous batch results stand.

## 7. Sweep (every `pes_*` binary, sim slots, timeouts)

- 96 binaries, a full `cmake --build build -j3` first, then two at a time through `tools/dev/simslot.sh` (`GF_SIM_SLOTS=4`, `timeout 1800`; 3600 for `pes_restart_delay_test`). Wall 2593 s. No timeouts.
- **95 exit 0. 1 red: `pes_stall_test`** (1 passed, 1 failed; 147 live firings).
- Test totals: 1992 passed, 1 failed. Gate not run (Main gates).
- Log directory: `gf-OwnerPickup2-scratch/sweep/`.

## 8. Open items

1. **Port the PES kind-0 TRAP clip choice** (`FUN_14079A470` → `FUN_140799440` → `FUN_14079FBC0`). Decode the rest of `FUN_14079A470` (226 lines), `FUN_14079FBC0` lines 343–403, and the helpers `FUN_140A4D340`, `FUN_140439E50`, `FUN_14079F920`, and the constants `DAT_1425CD060`, `DAT_1425CB30C`, `DAT_14259BE30`, `DAT_14259BE5C`, `DAT_142565600`, `DAT_142588218`, `DAT_142565D30`. Then replace `PickTrapClip` on the PES path. The forced probe is the oracle: the stall passes once the trap is issued to the presser. (Next ticket.)
2. **G1 table limit.** GF passed the presser at 14.5 m. The doc says FUN_1409FA400 is 100 m² (10 m). Check `ballDistanceTableMaxSq_m2` against the install constant.
3. **G5 designation** is a GF stand-in. PES entry 3 has no designation gate. Remove it on the PES path once item 1 lands.
4. **0x5D**: action class and clip family for slot 0x5D (`ANIME_ANIME_DATA_DRIVEN`), its writer, and `FUN_140A71260` (the per-player table behind `FUN_140439F90`).
5. **Correct `pes-re-first-touch.md` §7.6(b)**: the trap touch frame is already wired (§5).
6. **Probe geometry**: re-log with a per-tick id before using per-row geometry.
