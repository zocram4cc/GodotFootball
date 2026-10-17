# PES kick and touch writers, spin pump, mode index, goal-frame accumulator (A1Kick)

Read-only. Image base 0x140000000. Sources: Ghidra bridge decompile and raw disasm of the functions named; the live PES process (read-only `/proc/<pid>/mem`, pid 2510857, match in replay for most of the session). Scratch: `<workdrive>/gf-A1Kick-scratch/` (`d_*.c`, `dis_*.txt`, `live/*.py`).

Tags: CONFIRMED = disasm/decompile/live value at the VA. INF = inferred, reason given. OPEN = blocked, blocker named.

## 0. Summary

1. Kick chain: the kick writer `FUN_140870a40` copies a candidate from `FUN_140874860` and gets the foot response from `FUN_140875580`. The SpinRunaway claim that the kick writes R's ω_out is wrong: `FUN_140875580` discards it. The kick's spin comes from a 2π·v_R term (uncapped) and, on one branch, from `FUN_140875a10`, which has the 8 rev/s cap. Kick spin magnitude live: OPEN.
2. Y spin pump (`0x1408770a5..0x1408771cd`): CONFIRMED. `raw_Y ← raw_Y·(1 − blend)`. The doc's `|ΔrawSpinY|` in the blend is `u = clamp01(|Δ|/0.01)`.
3. Mode writer `FUN_1408723d0`: CONFIRMED index is `T[3·i32[cfg+0x7c] + i32[cfg+0x78]]`, not `pitch·3+weather` with pitch = `[cfg+0x78]`. The doc transposed the axes. Which field is pitch and which is weather: OPEN. Table bytes live-verified.
4. Goal-frame path: CONFIRMED. Kind-10 records enter the same record loop of `FUN_14086d6f0` and the same summed-ω 8 rev/s cap (`0x14086efbd`).
5. Ball velocity field: OPEN. `B+0x94` is refuted (it is the last-contact position). No float triple beside any live pos copy equals 54·Δpos.

## 1. Kick application chain (item 1)

### 1.1 Callers (CONFIRMED by a direct `E8/E9` scan of the live code, `0x140001000..0x15b960000`)
- `FUN_140870a40` (kick application): one direct caller, `0x14042583d` (container not resolved; Ghidra bridge has no function record there).
- `FUN_140874860` (candidate state builder): 34 direct call sites. It is a general candidate builder, not kick-only.
- `FUN_140875580` (foot response wrapper R): 2 call sites, `0x140812af8` and `0x140874a2a` (the second is inside `FUN_140874860`).
- `FUN_140875a10` (spin helper): 2 call sites, `0x140874e1d` (inside `FUN_140874860`) and `0x140875875` (container not resolved).
- `FUN_140875170` (per-record response): 12 call sites, including `0x14086e688` and `0x14086eac8` inside `FUN_14086d6f0`, and `0x1408756e8` inside `FUN_140875580`.
- `FUN_14086d6f0` (accumulator): no direct caller (CONFIRMED; reached through the vtable, as the collision doc says).
- `FUN_1408723d0` (mode writer): 2 call sites, `0x14087000e` and `0x140872c61` (containers not resolved).

### 1.2 `FUN_140870a40` (kick application; `param_1` = ball object, `param_2` = kick kind, `param_3` = kick vector, `param_4` = second vector, `param_5` = kicker object, `param_6` = intended flag)
CONFIRMED from the decompile:
- `param_2` is used only for bookkeeping: `+0x40 = 1` and `+0x44 = param_2` (if `< 0x16`), `+0x288 = param_2`, the per-player slot index, and `+0x28c = param_2` (if `< 0x16` and the kicker flag `+0x10d & 2` is set). It does not enter the velocity or spin arithmetic.
- Velocity and spin come from `FUN_140874860(cand, kicker, mode_struct, ball+0x64)` and then `FUN_140731bb0(ball+0x64, cand)` (copies pos, vel, raw spin). In `FUN_140874860`, the caller's stack args are `local_208 = param_3` (kick vector), `local_200 = param_4`, `local_1f8 = 1`, so `param_7 = 1` on every call from this function.
- Spin magnitude written to `ball+0x20`: `|ω| / 2π` (`DAT_1425795e4` = 6.2831855, CONFIRMED live), where `ω = 492.181 · raw(ball+0x7c)` (`FUN_140875550`, `DAT_14349f780` = 492.181 runtime value, CONFIRMED live).
- Flags: `+0x298 = 1` (no param_6), or `+0x294 |= 4`, `+0x298 = 2` (param_6). Then `+0x294 |= 0x13`.
- No 8.0 cap (`0x14259bdf4`) in `FUN_140870a40`.

### 1.3 `FUN_140874860` (candidate builder)
CONFIRMED from the decompile and disasm. Gate structure:
- **Gate 1** (`0x1408749e5..0x140874a00`): the foot response R is used only if `param_7 != 0` and `kicker != 0` and `kicker+0xec > 0`. Otherwise the candidate velocity is the kick vector unchanged.
- **R call** `0x140874a2a`: `FUN_140875580(&v_R, mode, kicker+0xf0, kicker+0xfc, 1.1)`. Returns `v_R` only. Its ω output (`local_28` inside `FUN_140875580`) is written and never read. **This refutes the SpinRunaway §2 statement that the kick writes R's ω_out.**
- **Velocity, gate 1 true** (INF on the callee names, from the decompile): `d_in` = unit kick vector, `d_R` = unit `v_R`, `θ = arccos(d_in·d_R)` (callee `0x1415a1798`). If `-1 < d_in·d_R < 1`, the output velocity is `½(|v_in| + |v_R|) · rot(d_in, axis = d_in×d_R, angle = ½·kicker+0xec·θ)`. Otherwise it is `v_in` (the kick vector) unchanged.
- **Spin, two sub-branches** (CONFIRMED structure, INF units):
  - A′, if `kicker+0x108 ≤ 0` or (`param_7 == 0` and `kicker+0x10c != 0`): `ω = 2π · (v_R, or param_6-based terms)` via the cross products of `local_128`/`local_120`. Uncapped.
  - B′, otherwise: `ω = (2π·v_R-terms) + kicker+0x108 · FUN_140875a10(...)`. `FUN_140875a10` (below) is capped.
- Then `raw = ω · DAT_14349f740` (1/492.181, CONFIRMED live), written to `cand+0x18..0x20`. The velocity is written to `cand+0xc..0x14`.
- Extra terms when `kicker+0x2d8 != 0` (`+0x28c`, `+0x13c`) and when `kicker+0x2d9 != 0`. INF on their meaning.

### 1.4 `FUN_140875580` (R wrapper)
CONFIRMED: zeroes and returns `v_R`. It calls `FUN_140875170(v_R, ω_discard, n, n, u = 54·param_2-vec, n' = 54·param_3-vec, T = 0.75 (0x14259bdd4), E = 0.45 (0x14256b140), size = 1.1 (param_5), 0, w1 = 492.181·raw(…), 0, ω_discard)`. The velocity term is `(T)·u_tan − E·u_n` with `T = 0.75`, `E = 0.45`. `FUN_140875170` returns ω only through `param_2`, which is discarded here.

### 1.5 `FUN_140875a10` (spin helper for branch B′)
CONFIRMED: calls `FUN_140875170` once, then caps the result at 8 rev/s: `if |ω|/2π > 8.0 (0x14259bdf4): ω *= 8·2π/|ω|`. So the B′ term is capped. The A′ term is not.

### 1.6 Constants (CONFIRMED live reads from the image)
`0x1425795e4` = 2π (6.2831855); `0x142598958` = 0.10868593 (ball radius r); `0x142565600` = 0.5; `0x14256ae1c` = 1.0; `0x142578480` = π; `0x14259bdf4` = 8.0; `0x1425a3590` = 1.1; `0x14259bdd4` = 0.75; `0x14256b140` = 0.45; `0x142598578` = 0.2; `0x14349f780` = 492.181; `0x14349f740` = 1/492.181; `0x14259be5c` = 90.0; `0x142588218` = −1.0; `0x14257a190` = 0.6; `0x1425c4bf4` = 0.24; `0x14258c260` = 0.9.

### 1.7 Kick spin magnitude, live sample: OPEN
- The collision doc's chain puts the candidate at `simBall+0x64`, and `FUN_140870a40` writes `simBall+0x20 = |ω|/2π`. Locating `simBall` live failed. `B = *(G+0x40)` is not it: `B+0x20` holds `-1.0`, and `B+0x280..0x2a0` hold floats, not the `0x294/0x298` flags.
- The live ball copies (§2.3) carry pos at `+0`, and no velocity or spin triple with the expected relation.
- **Blocker:** the simBall struct, with its velocity and raw spin, is not located. Next step: find the `BallControl` (vtable `0x1425988b0`, slot 9 reads `[obj+0x20]` = simBall) and read `simBall+0x20` across one kick.

## 2. Ball state, velocity field (item 5): OPEN

### 2.1 `B+0x94` is refuted, with the reason (CONFIRMED live)
In a 240 s capture (9,881 frames, `live/cap1.bin`), the triple at `B+0x94` equals the ball position at the frames the ball lands (e.g. frame 1327063: `B+0x94 = (-37.312, 0.143, 2.947)` = pos). It holds that value until the next contact. So `B+0x94` is the last-contact point, not velocity.

### 2.2 No float triple in `B[0..0x400)` equals 54·Δpos (CONFIRMED, negative)
Over 7,845 in-play consecutive-frame pairs, the best fitted scale and R² for any offset is `R²=0.098` at `B+0x0c` (scale 57). The per-offset correlation over a rolling window (frames 3990–4029, `Δx ≈ 0.078 m/frame`) has no offset with `|r|` near 1 on all components.

### 2.3 Copies of the ball position in writable heap (CONFIRMED, one 90 s run plus one 60 s run)
A scan of 2.95 GB of writable heap for the 12-byte pos at frame `t` found 18 copies. Tracked over 3,399 frames, the only ones that keep pos equal to `B+0x88` are:
- `0xa3e8b8c` = `B+0x88` (control): ratio 1.000. Its `+0x0c` triple is a unit vector rotating, not `54·Δpos`.
- `0x12f7230`: ratio 0.945. `+0x0c` = `(1,0,0)` constant. `+0x18` = `(0,0,pos.x)`.
- `0x12f7250`: ratio 0.770. `+0x0c` = `(1,0,0)`. `+0x18` = `0`.

So no copy carries a velocity triple next to its position. Scripts: `live/track2.py`, output `live/survivors2.txt`.

### 2.4 Probe status
- `B+0x94` is the last-contact pos, and the velocity is not in `B`. The pointer-slot check (`B+k` holding a pointer whose block contains pos) found nothing.
- **Next step:** locate the BallInfo future buffer (`FUN_140870e90` pulls the next state; the collision doc says it is the source) or a double-precision velocity (the copies are float). Or read the velocity from the integrator's own state once `FUN_140876360`'s `r9` is located live.

## 3. Y spin pump, `0x1408770a5..0x1408771cd` (item 2): CONFIRMED with one correction

Disasm (`dis_876360.txt`, lines 718–781):
- Target: `X_t = (cap-scaled v/r) · (1/492.181)`, `Y_t = [rbp+0x98]`. CONFIRMED `[rbp+0x98] = 0` at `0x140876fd5` (XMM0 = 0·x − 0·y = 0, XMM7 = 0 from the prologue) and reloaded as `Y_t = 0` at `0x140877079`. So the Y target is 0.
- Δ: `ΔX = X_t − rawX` (`0x1408770a5`), `ΔY = 0 − rawY` (`0x1408770ab`), `ΔZ = Z_t − rawZ` (`0x1408770b0`).
- Blend `k` for X and Z (`0x1408770b6..0x1408770c0`): `k = 1.0` (the prologue XMM8) if `R12B` or `R15B` is set; otherwise `k = XMM14` (the rolling ramp, INF per the doc). `rawX += k·ΔX`, `rawZ += k·ΔZ` (`0x1408770c4..0x1408770e0`).
- **Y blend** (`0x140877125..0x1408771ba`): `u = clamp01(|ΔY| / 0.01)` (`0x14256ae18` = 0.01; `u` is 1 when `|ΔY| ≥ 0.01`, 0 when `ΔY = 0`). `vy_keep = 0.03 + 0.07 · clamp01((3.6|vy| − 0.1)/0.6)` (MinY `0x11c`, MaxY `0xec`, AddRateY `0x8c`, BaseY `0xbc`, m = mode). `blend = clamp01(vy_keep · (2.5 − 1.5·u))` (`0x14259bde4` = 2.5, `0x14253ee30` = 100, `÷100`).
- Store: `rawY = rawY + blend · ΔY = rawY · (1 − blend)` (`0x1408771ba..0x1408771cd`). **The Y pump decays raw Y toward 0 at rate `blend`. It does not touch `vy`.**
- **Correction to contact-2 §2.1:** the doc's blend used `|ΔrawSpinY|` directly. The code uses `u = clamp01(|Δ|/0.01)`, and `u` is 1 whenever `|Δ| ≥ 0.01`.

## 4. Mode index writer `FUN_1408723d0` (item 3): CONFIRMED index, semantics OPEN

Disasm (`dis_1408723d0.txt`):
- `call 0x140a3f560` (`MOVZX EAX, byte [rcx+0x85]`): if nonzero, mode = 3.
- `call 0x14234e700` (`MOV EAX, [rcx+0x7c]`) → `EDI` (row `a`). `call 0x141036a00` (`MOV EAX, [rcx+0x78]`) → `EAX` (column `b`). `cfg = [ [G] + 0x2c0 ]`, `G = [0x1436F3DC8]`.
- If `a > 2` or `b > 2`: mode = 1. Else mode = `T[3·a + b]` with `T = 0x14349f718`.
- The write goes to `[rsi+0x279d]` (byte) of `FUN_14046ddf0(rsi+0x4e0)`, plus cached `[rsi+0xdc]` and `[rsi+0x1e8]`. INF: `0xdc = 0x64 + 0x78` suggests the ball state base is `rsi+0x64` (not verified).

**Table (live-verified bytes, `0x14349f718`, 9 × i32):**

| row a = [cfg+0x7c] \ col b = [cfg+0x78] | 0 | 1 | 2 |
|---|---|---|---|
| 0 | 2 | 1 | 1 |
| 1 | 2 | 1 | 0 |
| 2 | 1 | 0 | 0 |

**Correction to contact-2 §3:** the doc set `idx = pitch·3 + weather` with pitch = `[cfg+0x78]`. The disasm gives `idx = [cfg+0x7c]·3 + [cfg+0x78]`. The doc's table orientation is transposed.

Semantics: which of `[cfg+0x78]` and `[cfg+0x7c]` is pitch and which is weather: OPEN. The doc's "dry = mode 2 (weather = 0)" depends on it. The live `cfg` read returned 0 (the match context was not live: replay). **Next step:** read `i32[cfg+0x78]`, `i32[cfg+0x7c]` in an open-play match and compare with the Match Settings screen.

## 5. Goal-frame path shares the accumulator (item 4): CONFIRMED

`FUN_14086d6f0` (decompile, `d_14086d6f0.c`):
- The record loop covers every accepted record, kind 10 (`*(short*)(rec+0x1a) == 10`) included. Kind-10 normals come from the analytic frame (`pfVar15[0x10]`); the loop then jumps to the common response block (`LAB_14086e589`).
- The common block calls `FUN_140875170` at `0x14086e688` (INF: the record-loop call, by source order of decompile line 719) and adds `fVar35·ω_out` into `local_418/414/410` (the ω sums), weighted per record.
- After the loop, the aggregate response `FUN_140875170` at `0x14086eac8` (INF: decompile line 801) and the cap `fVar36 = |ω|·0.5/π; if fVar36 > 8.0 (0x14259bdf4): ω *= 8/fVar36` (decompile lines 912–919; `0x14086efbd` = 8.0) apply to the summed ω of all records, goal-frame included.
- Exception: when `bVar8` (intended touch, flags at `0x14086dad4`) is set, the response and cap block is skipped (only the position is written).
- So the goal frame does **not** have its own accumulator. It shares the 8 rev/s cap with bodies and posts. Both `FUN_140875170` call sites are inside `FUN_14086d6f0`.

## 6. Doc corrections

1. **SpinRunaway-2 §2:** "PES's kick writer sets the spin to R's ω_out, uncapped." Wrong. `FUN_140875580` discards ω_out. The kick spin is the candidate from `FUN_140874860`: a 2π·v_R-based term (uncapped; units INF) and, on the B′ branch, `kicker+0x108 · FUN_140875a10(...)`, which is capped at 8 rev/s. The conclusion "no clamp on the kick writer" holds only for the A′ term.
2. **Contact-2 §3.1:** table indexing transposed (see §4).
3. **Contact-2 §2.1:** the Y blend uses `u = clamp01(|Δ|/0.01)` (see §3).
4. **Contact-2 §7 / ball-track-live:** `B+0x94` is not velocity; it is the last-contact point (see §2.1).
5. **Collision doc §2 (`FUN_14086d6f0`) and §5:** the goal-frame [INFERENCE] of SpinRunaway-2 §3 is now CONFIRMED (§5 above).

## 7. Open items (numbered, with the named next step)

1. **Kick spin magnitude, live** (§1.7): locate simBall through the `BallControl` (vtable `0x1425988b0`); read `simBall+0x20` (= kick |ω|/2π) across one kick; compare with 8 rev/s.
2. **Ball velocity field** (§2.4): locate the BallInfo future buffer (`FUN_140870e90`) or the integrator state; expected relation `54·Δpos`.
3. **Mode pitch vs weather** (§4): read `i32[cfg+0x78]` and `i32[cfg+0x7c]` in open play and match against Match Settings.
4. **Callers** (§1.1): containers of `0x14042583d`, `0x140875875`, `0x14087000e`, `0x140872c61` are unresolved (the bridge returns "No function found" for these addresses). A REA session would resolve them, but REA has no app open.
5. **FUN_140874860 branches**: the A′/B′ split, the callee names `0x1415a1798`, `0x140a93070`, `0x140875a10` (verified as cap), and the `kicker+0xec/+0x108/+0x10c/+0x2d8/+0x2d9` meanings are INF from the decompile. A disasm-level walk of the R-branch would confirm the 2π·v_R units.

## 8. Verification

No code changed. No live write. Live reads were `/proc/<pid>/mem` `pread` only. Captures: `live/cap1.bin` (9,881 frames, `B` block), `live/cap2.bin` (partial; crashed on a bad pointer in its S read; not used), `live/survivors2.txt` (tracker run, control ratio 1.000). No test binaries were run (investigation only).
