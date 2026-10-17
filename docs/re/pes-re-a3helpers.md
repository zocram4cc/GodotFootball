# A3 MOVESTEP blockers: decode (A3Helpers)

Decode only. No `src/` edits, no commits, no game writes. Live reads: `sudo -n` `/proc/2510857/mem`, `pread` only (PES21 PID 2510857 still alive). Scratch: `<workdrive>/gf-A3Helpers-scratch/` (`dec/` decompiles and raw asm, `tools/pe.py` PE/capstone helper with `.pdata` containing-function lookup and E8/E9 call-site search, `tools/bit27b.py`, `tools/live_read.py`).

Verdicts: **CONFIRMED** = decompile, raw asm, install bytes, or live read (VA given). **INF** = inferred. **OPEN** = blocked, with the blocker.

Method notes: Ghidra's `(int)` casts on req words were wrong twice (see §2). Word and byte offsets: `req[w]` = byte `4w`. Ghidra's `int*`-typed and `ulonglong`-typed views disagree on the offsets, so the raw asm governed.

---

## 0. Corrections to the brief and the wave-1 notes

1. **`FUN_141af28f0` is not the yaw adjuster reset.** It takes the physique object (P+0xa08) and writes its `+0x88` value (mode 0: `FUN_1423a2a90`; mode 1: `FUN_141b11280`), with a scale of 300.0 (`0x1425a266c`). It is called from `FUN_1406e5f70(P, v)` as `FUN_141af28f0(P+0xa08, 1, v)`. CONFIRMED (decompile of both).
   The yaw reset in OutOfPlayPreempt is `(P+0x4d0).vt+0x08`. The yaw adjuster's vptr is `0x142b67950`; its slot +0x08 = `0x141eb33c0`, +0x10 = `0x141eb1a10`, +0x18 = `0x141eb1a50` (CONFIRMED, `pe.py vt`).
2. **D1's slot is `vt+0x20`, not `FUN_1406eab00`.** On the `+0xac0` anime table (vptr `0x1425e35b0`), slot 0x20 is at VA `0x1425e35d0` and holds `FUN_1406ea110` (km/h from the root displacement; CONFIRMED, `pe.py vt`). `FUN_1406eab00` is slot 0x40 (`0x1425e35f0`), the Acceleration τ, consumed by the rate step `FUN_1406e66d0`. See §4.
3. **The ramp's rate is a float, and the phase-3 table is not int.** `req[0x4a]` is read as float bits (`movss xmm9,[r15+0x128]`), and `req[0x1d]` = `3.6 × curve` (`DAT_14259ce38` = 3.6 on the install, not 55). Ghidra's `(int)` casts at ramp lines 280 and 71 are artifacts (CONFIRMED, raw asm `1406cc0f9`/`1406cc0f2`/`1406cc133`).
4. **The ring index is constant, not rate-driven.** Stage 1's displacement index is `int(54·4/60 + 0.5) = 4` (the 54 is the tick rate, `FUN_141e532d0`), not `int(rate·…)`. CONFIRMED (Ghidra lines 54-55; asm 1406cbf70+0x... `call 0x141e532d0` before the divide).
5. **Class word.** The phase-3 class and the dwell index are the same word: byte `0x7c` = `req[0x1f]` (both `[r15+0x7c]` in the ramp asm and `*(byte*)+0x7c` in the dwell gate). The repo's `kFootBandSide = 0x7c` is named as a word index, which I could not reconcile with this (INF; owner of `FUN_1406c2f80` to confirm).

---

## 1. OutOfPlayPreempt `FUN_1406ce550`

**Verdict: CONFIRMED** (decompile 1406ce550, sole caller `FUN_1406ba900` at `0x1406ba9b9`, the dispatcher; pdata-verified).

Flow:
1. Returns 0 if `MI+0x116c == 1` (in play), if anime phase `P+0x245c == 0x17` (state 23 is excluded), or if `req[0x14]` is outside `[0x22,0x29] ∪ [0x2f,0x32]`.
2. `v = FUN_141eb50b0(P, buf, 1)` (the displacement from last tick); `speed_kmh = |v|·54·3.6`. Compared against `DAT_1425a1ffc` = 13.5.
3. Motion from `req[0x14]` (switch): 0x23 → 0x179 / 0x1f9 (fast); 0x24 → 0x1c1 / 0x1d4; 0x25 → 0x140 / 0x1e1; 0x26,0x2f → 0xf9 / 0x127; 0x27,0x30 → 0x17a / 0xc3; 0x28,0x31 → 0x1c0 / 0x1d1; 0x29,0x32 → 0x1a4 / 0x1cd; default → 0x1c4 / 0x1d2. Fast variant when `speed > 13.5`.
4. Issue: `(P+0xac0).vt+0x10(req, motion, FUN_1406ebf30(P,0x32), 0, 0.3, DAT_1425a3590, 0.5)`.
5. Request fields: `req[0x1a] = 0`; `req[0x1b] = min(FUN_1406af620(motion,1.0)/54, 2.0)`; `req[0x2d] = 0`; `req[0x2e] = 0.3`; `req[0x1e] = req[0]`; `req[0x38] = 1`; `req[0x39] = 0x17` (state 23); `req[0x3c] = 0` (byte).
6. Yaw reset: `(P+0x4d0).vt+0x08(P+0xa08)` = `0x141eb33c0`.
7. `FUN_1406e5f70(P, 0.2)`: `FUN_141af28f0(P+0xa08, 1, 0.2)` (the physique value, §0.1), then the layer clear (below), then `(P+0xab8).vt+0x08` = `FUN_1406ea810` (match index; no effect).
8. Returns 1.

**Layer state (P+0x27d8 / 27e4 / 27e6), CONFIRMED.** Per `pes-re-anim-blend.md` (OBS), these are the upper-body layer timer, motion and mirror. `FUN_1406e5f70` is the clear (`DisableLayerMotion`): `P+0x27d8 = 0` (u32, float timer), `P+0x27e4 = 0xffff` (u16), `P+0x27e6 = 0` (u8).

Writers (pdata function, VA of the store): `FUN_1406e5f70` (`1406e5f90`, `1406e5fa1`, `1406e5fab`); `FUN_1406f0aa0` (`1406f0b96`, `1406f0b7e`, `1406f0b85`; the layer issue); `FUN_1406ee6a0` (`1406ee899`, `1406ee77f`, `1406ee88f`); `FUN_1406eb670` (constructor: `1406eb747`, `1406eb764`, `1406eb74e`); `FUN_140748550` (`140748633` float write); `FUN_1407743f0` (`140774606`); `FUN_1407a2762` (`1407a28d1`, `1407a3455`); `FUN_140808...` (`140808ce2`); `FUN_14044a...` (`14044a129`, `14044a135`: mirror 27e6); `FUN_140a571f0` (`140a5723c`).

Readers (pdata function): `FUN_1406b895e` (`1406b8ae4`, MOVESTEP entry); `FUN_1406f8518` (27e6, `1406f8561`, `1406f85b4`); `FUN_1406f5ec8`; `FUN_140748550`; `FUN_1407743f0`; `FUN_1407a2762`; `FUN_1407750e3` (`140775194`, `1407753ee`); `FUN_14080d210` and `FUN_14080e850` (`14080d405` … `14080e8e1`, many); `FUN_140a18b6c` (`140a18da8`); `FUN_14084e930` (`14084ebef`); `FUN_140449faf` (`14044a1..`).

**Who fills `req[0x14]`: CONFIRMED.** `FUN_1406b7560(P, req, cmd)` (MOVESTEP request filler), line 311 of its decompile: `req[0x14] = cmd[0x18]`, i.e. the action record's word 0x18 (byte `cmd+0x60`); line 310 `req[0x13] = cmd[0x14]`; line 396 (case 1) `req[0x50] (byte) = P+0x27b1`. It runs from `FUN_1406b8340` line 80, before the dispatcher.

Port note: OutOfPlayPreempt has no RNG, no install constants beyond `0.3` (`0x142590d98`), `13.5` (`0x1425a1ffc`), `300` (`0x1425a266c`), `2.0`, `0.5`, `0x1425a3590`. The motion-table values above are hard-coded in the decompile and must be read from the install by VA or by their switch, not copied (per the brief rules).

---

## 2. AccelerationRamp `FUN_1406cbf70` (stage 1, phase-3 class, post-step closing cap)

**Callers:** sole caller `FUN_1406b8340` at `0x1406b8568` (CONFIRMED). The MOVESTEP entry runs it for states 3 and 13 only.

Raw asm of the phase-3 block (`dec/ramp.asm`, CONFIRMED): the speed term reads `xmm9 = req[0x4a]` (`1406cc026`), the class call is `call [rax+0x68]` with `rcx = P+0xac0` and `xmm1 = xmm9 = rate` (`1406cc0f9`).

**Stage 1a: speed term (INF on the use).** `idx = int(54·4/60 + 0.5) = 4`; `FUN_141eb50b0(P, buf, 4)` gives ring entry k = min(4/10, 5) = 0 at `P+0x2fc` (stride 12). The 3-D length `|ring[0]|` is computed (`sqrtf` through `*(0x1425301f0)`), and it feeds a compare against an install constant with a rate cap `xmm9 = xmm15` (`1406cc0c7`…`1406cc0e7`). The ring writer is not decoded (OPEN). The cap constant behind `xmm15` and the constant at `0x1ebc141` (rip) were not resolved (OPEN).

**Stage 1b: class side effect (CONFIRMED structure, raw asm).**
- Class = `(P+0xac0).vt+0x68(rate)` = `FUN_1406ba030`. Thresholds (4 floats, install, VA `0x1425dde20`…`0x1425dde2c`) = `{0.29, 0.59, 0.70, 1.0}`; ints (VA `0x1425dde00`…`0x1425dde0c`) = `{0,1,2,3}`. Result = `count(threshold ≤ rate)`, saturating at 3 (`1406ba030`…`1406ba067`; CONFIRMED).
- If class ≠ `req[0x1f]`: `req[0x24] = req[0x1f]`; `req[0x22] = req[0x1d]`; `req[0x1f] = class`; `G = FUN_141af3180(P+0xa08)`; `req[0x1d] = 3.6 × (P+0xac0).vt+0x48(rate, G)` (`DAT_14259ce38` = 3.6). Slot 0x48 = `FUN_1406b9aa0`, a 3-segment curve decoded in the gap report round 3 from raw bytes (no bridge function at that VA; not re-verified here).

**Stage 2 (proximity cap, reads hu(opp)+8 bit 27).** Ghidra lines 129-165 (asm not checked): opponent `hu = FUN_140a48910(G, opp)`; gate `(hu+8 >> 27) & 1` and own PC (`FUN_1406e6660(P,0x28)`) `< opp ability byte +0x28`; then predicted positions over `int(rate·…)` ticks and `FUN_140439e50`/`FUN_14043a0a0`. The hu+8 bit 27 question is §3.

**Post-step closing cap (rev2 §4.3), INF on semantics, from Ghidra lines 247-278.** `rate' = FUN_1406e66d0(P)` (the rate step, `τ` from `(P+0xac0).vt+0x40` = `FUN_1406eab00`). If phase 3 and `rate' > 0.1` (`0x142588200`), own record `lVar13 = FUN_140a48a10(G, match index)` with partner index `+0x610 < 0x16`, flag `+0x661 ≠ 0`, and `|(+0x64c,+0x650,+0x654)| > 0`: partner `lVar15 = FUN_140a48a10(G, partner)`; relative vector = partner pos (`+0x55c/+0x560/+0x564`) − P pos (`P+8/+0xc/+0x10`), rotated by `FUN_1404bcad0(vec, P+0x20 · −1)`. If `x < 0.3` (`0x142590d98`) and `0.3 < z` then `d = (+0x64c,+0x654)` rotated by yaw (`FUN_140581ce0`, `FUN_140591a10`); `speed = FUN_14042d470(d.z)` (= d.z·54·3.6); if `speed < −5.0` (`0x14259be7c`) then `rate' *= 0.95` (`0x1425ab124`). Stored: `req[0x4a] = rate'` (float). Semantics of `+0x610/+0x661/+0x64c` (partner and touch records) are OPEN (the partner/touch writer is not decoded).

**Motion issue (phase 3):** `motion = DAT_1425ddcf0[req[0x50]·0x10 + band·4]`, band 0/4/8/12 from `rate` vs `0.225` (`0x1425ddf54`), `0.4` (`0x14259b89c`), `0.7` (`0x14257a194`). Issued via `(P+0xac0).vt+0x10`. Style = `req[0x50]` (word 0x50 = byte 0x140; repo's `kStyle` ✓).

---

## 3. A-CLS bit 27 at `hu+8` (main's added item)

**Meaning of `hu`: CONFIRMED** (per `pes-re-duel-sim.md`, `pes-re-contact.md` EV-CONTACT-0013): `hu(i) = FUN_140a48910(G,i) = *(G+(i+0xa5)·0x28)`, the **published anime-state record** that the Judge reads. It is a bulk copy of a player state block (`pl = Player+0xc8`, so `hu+8` ≡ `Player+0xd0`). It is not the AnimePlayer `P`.

**Writers found by exact-immediate scan (`.text`, disp8 = 8; OR `0x08000000`, AND `0xF7FFFFFF`, BTS/BTR 27): CONFIRMED for these two, nothing else.**
- `FUN_1406f6ce0`: writes bit 27 of **P+0xad0** (`lea r14,[rbp+0xac8]`; `or/and qword [r14+8]`, `1406f753a` / `1406f7530`). Sets it when `P+0x27b1 ∈ {6,7}` or `== 0x10`; clears otherwise. This is the own-player jostle-style flag (`P+0x27b1` byte); the same test is ramp line 113 and LateralDamping's drift exclusion. It writes the AnimePlayer's word, not `hu+8`.
- `FUN_140a1f1f0`: writes bit 27 of `puVar12[2]`, `puVar12 = FUN_140451250(...)` = `Q+0x44` (so the bit is at `Q+0x4c`), under flags `+0x5f`, `+0x16` and a counter `lVar29+0xbd08+0x1827f8 == 2`. Not `hu+8`.

**Clears:** only the `and` at `1406f7530` (`FUN_1406f6ce0`). Register-computed RMW (`or eax,…; mov [r+8],eax`) was not scanned.

**Writer of `hu+8` itself: OPEN.** The publisher is a bulk copy (`FUN_1406e1ce0` / `FUN_140a47290` per `pes-re-duel-sim.md` §3; not decoded here). Until it is decoded, the claim "the cap reads the opponent's bit 27 and that bit is the opponent's jostle-style flag" is **INF** (high): the setter condition in `FUN_1406f6ce0` is the style test used elsewhere, and the cap is only meaningful for a jostling opponent.

**GF today:** the stand-in never sets it, so the cap never fires (as Main noted). Port needs: the publisher (copy source/offset) and the style test, or a direct decode of `FUN_140a47290`'s copy list.

---

## 4. L4 (P+0x428 approach/strike) writers and the D1 slot

**Solver `FUN_1406e85b0` (CONFIRMED, decompile):** `(P, out, motion, target, yaw_ptr, mirror, k, flag)`. `local = FUN_1406e8b70(...)`: `local = 0.01·(FUN_1406aff90(motion,k,flag) − FUN_1406b05e0(motion,k,flag))·P+0x3b18`, x negated when mirror (`DAT_142588218` = −1). `c = FUN_140a92c80(yaw+90°) = cos(yaw)`, `s = FUN_140a92c80(yaw) = sin(yaw)`. Output: `out.x = target.x − (c·lx + s·lz)`, `out.y = 0`, `out.z = target.z − (c·lz − s·lx)` (CONFIRMED from the arithmetic).

**Adjusters on A = P+0x428 (CONFIRMED, decompile):**
- `FUN_141eb3910(A, target, cur, ticks, vel)` (approach/AdjustTransXZ setup): clears `A[1..3]`, `A+0x20/0x24`; `vt+0x10(A)`; `A+0x28 = cur`; `A+0x30 = target`; `A+0x38 = vel`; `A+0x10 = ticks` (u16); `A+0xc = 1`; `A+8 = 1`; **`A+0x6d = 2`** (mode 2).
- `FUN_141eb39a0(A, target, cur, ticks, vel, vel2)` (strike, `ticks ≠ 0`): velocity `A+0x30 = (target.x − pred.x)/ticks`, `A+0x34 = (target.z − pred.z)/ticks`, with `pred = FUN_141eb30c0(cur, ticks, vel, vel2)`; `A+0x48 = FUN_141eb3220(...)` (alpha); `A+0x10 = ticks`; **`A+0x6d = 1`** (mode 1).
- `FUN_141eb30c0`: `out = p0 + (vA+vB)·n/2 + (vA−vB)·(alpha−0.5)·(n−1)/2` per axis (CONFIRMED).
- `FUN_141eb3220` (alpha): `((Δp·(vA−vB) above)/n)/|vA−vB|² + 0.5`, clamped to `[0.25, 0.75]` (`0x1425dde38`… = `0.75`/`0.25`: `DAT_14259bdd4`, `DAT_142592228`); `0.5` when `|vA−vB| = 0` (CONFIRMED).
- `FUN_141eb3ae0(A)`: if `A+8 == 1`, `A+8 = 2` (finish).

**Who consumes A (per tick): CONFIRMED structure.** The owner tick `FUN_1408bfcf0` (per player, slot 2 of the 22) → `FUN_141eb5e70(P, tick)` → `FUN_141eb3af0(A)` → `(A.vt+0x18)(A, P, …)` = `FUN_141eb1bf0` (the root advance; mode 2 rotates the velocity by yaw and steps over `A+0xc..A+0x10` ticks). Corrected 17-10-26: `FUN_1409f1bb0` is a message handler (kind 0x14), not the per-player loop (pes-re-gaps-anime.md §1). Per-tick reset: `FUN_1406eb440` clears `A`'s state. The bridge has no function at `0x141eb1bf0` (raw only); its body is per the gap report round 4 and not re-decoded here.

**Gate bit `P+0x2460 & 0x40000000`: writer partly found.** `FUN_14073338c` is the tail of `FUN_140733000` (phase-1 continuation; the `or [rdi+0x2460],0x40000000` at `14073338c`). Callers: via vtable slot `0x1425df758` (table base unresolved). OPEN.

**D1 (dodge speed term): CONFIRMED slot, OPEN equality.** The dodge (`FUN_140736d30`) calls `(P+0xac0).vt+0x20(1)` = `FUN_1406ea110` = `|Δroot|·54·3.6` (km/h). The D1 open item is whether GF's physical speed equals that quantity (not shown). `FUN_1406eab00` is not on the D1 path.

**Approach/strike, L4 port order:** approach `FUN_1407344f0` (phase 0; solver and `FUN_141eb3910` only when bit 30 is set; `FUN_1406f0f70` issue; `P+0x245c = 1`), then strike `FUN_140733a30` (phase 1; solver, `FUN_141eb39a0`, yaw `FUN_141eb35a0(P+0x4d0)`), then phase 2 `FUN_1407335e0`. Consumer and gate writer as above.

---

## 5. RUN_LOOP entry gate (dwell) `FUN_1406b70d0`

**Verdict: structure CONFIRMED; branch semantics INF.** Callers (xref by `.text` scan, CONFIRMED): `0x1406c0424` (in `FUN_1406c03f8`: after `comiss` with `req[0x4a]` at `1406c0411`; `test al,al; je skip` at `1406c0429`, so return 1 lets the transition proceed), `0x1406c5664` (`FUN_1406c4f59`, STEP_LOOP_CHANGE), `0x1406c80f6` (`FUN_1406c7400`), `0x1406ca18c` (`FUN_1406ca030`), `0x1406cae72` (`FUN_1406cab20`).

Argument: `(P, req)`. The gate reads **byte** offsets (its `param_2` is `ulonglong`): `req` word = byte/4. Word map: byte `0x24` = `req[9]` (`== -1` → return 1 at once); `0x20` = `req[8]` (switch: 0,5 → 0; 1,6 and 2,7 use the dwell arms; default → 1); `0x38` = `req[0xe]` (precise, `== 1` test); `0x3c` = byte 0 of `req[0xf]`; `0x44`/`0x48` = `req[0x11]`/`req[0x12]` (distance thresholds); `0x58` = `req[0x16]` (`== 5`: the ball command); `0x7c` = `req[0x1f]` (class 0..2 indexing the tables below); `0x128` = `req[0x4a]` (rate); `0x18` = `req[6]`; `0x10` = `req[4]` (body direction; `FUN_14043a0a0`).

Constants (install, CONFIRMED by read): class tables `DAT_1425dde70 = {0.3,0.5,1.25}`, `DAT_1425dde80 = {0.7,1.25,2.0}`, `DAT_1425dde90 = {0.5,1.0,1.5}`, `DAT_1425ddea0 = {1.25,2.0,3.5}`. Scalars: `0.4` (`0x14259b89c`), `2.5` (`0x14259bde4`), `56.25` (`0x14259be3c`), `101.25` (`0x1425ddfac`), `112.5` (`0x1425cb30c`), `45` (`0x14259be30`), `25` (`0x14259be20`), `12` (`0x14259be04`), `90` (`0x14259be5c`), `11.25` (`0x1425cc680`), `33.75` (`0x1425cc688`), `13.5` (`0x1425a1ffc`).

Dwell arms: the arm for `req[8] ∈ {1,6}` adds a ramp `fVar16 = (rate − 33.75)/33.75` (clamped) to `class-table[0x1425dde70 or dde90]` and compares with the `req[0x11]/req[0x12]` thresholds; the arm for `{2,7}` uses `dde80/dde90/ddea0` with `(rate − 0.? )/45` and tests `req[0x24]`, `req[0x7c]`, `req[0x128] ≥ 0.4`. The clock/limit semantics (`req[0x1a]`, `req[0x1b]`, `req[0x2d]`, `req[0x2e]`) and which arm is the dwell proper are INF.

---

## 6. Action classes after MOVESTEP: port order

**MOVESTEP entry:** `FUN_1406b8340(P, cmd)` (CONFIRMED). Its callers are the per-action tick drivers, CONFIRMED by `.text` call-site scan:
- `FUN_1407824a0` (action tick; checks anime classes `0x52`/`0x53` via `FUN_1406b4580`, else MOVESTEP; `FUN_1406eebb0(P,…,0x5e)` on a class hit).
- `FUN_140789dd0` (class `0x53` gate, else MOVESTEP).
- `FUN_1407d1e60` (velocity integrator; MOVESTEP at line 221, `FUN_1406eebb0` on the stop flag).
- `0x1407347b6` (inside the kick approach region; no pdata function).

**Action setter and reset:** `FUN_1406eebb0` (sets action id `+0xac8`, sub-id `+0xac9`; `fox-registry`), `FUN_1406eb440` (per-action reset: clears phase, calls `A.vt+8` and yaw `vt+8`, clears `P+0x2460` bits, `FUN_1406fa7c0`).

**Layer-clear (DisableLayerMotion) callers, i.e. action classes that use the upper-body layer:** `FUN_140743b5a`, `FUN_140748550` (+`0x14074866a`, `0x140748697`), `FUN_1407a32b6` (`1407a33ac`), `FUN_1407e1b10` (`1407e1c28`), `FUN_1407e23ee` (`1407e2493`), `FUN_1407ebfe0` (`1407ec0fb`), `FUN_14080d1c0` (`14080d1d9`), and group B `FUN_1406ce5bf` (`1406ce7a4`).

**Proposed port order after MOVESTEP (INF; follows the foundation build waves):**
1. Glue: `FUN_1407824a0`, `FUN_140789dd0`, `FUN_1407d1e60` (MOVESTEP callers) with the action setter `FUN_1406eebb0` and reset `FUN_1406eb440`.
2. A-CLS (foundation wave 3): anime-class predicates (table `0x1426484f0`), slot tests, and the publish of `hu+8` (§3). Blocks the proximity cap.
3. Layer state owner `P+0x27d8/27e4/27e6` (`FUN_1406e5f70` clear, `FUN_1406f0aa0` / `FUN_1406ee6a0` writers) with the layer-using action classes above.
4. L4 kick family (foundation K-ERR, wave 8): approach `FUN_1407344f0`, strike `FUN_140733a30`, phase 2 `FUN_1407335e0`, kick starter `FUN_140733000`, dispatcher `FUN_140732e30`; with the solver and adjusters (§4).
5. Dodge / tackle / trap selectors (foundation D1, D2, D5, D9) and the dodge speed `FUN_1406ea110` (D1).
6. K-SEL (wave 7) and J-JDG (wave 8).

---

## 7. Live checks

- `freemove.o`: the registry lookup `list = *(0x1437f4808)` = `0x29856e0`; `rec = list[0x2e]` = `0x2c170ec0` (CONFIRMED, live). Runtime words: `rec+0x10 = 0x24`, `rec+0x14 = 0x1`, `rec+0x18 = 0x63` (as floats: 5.0e-44, 1.4e-45, 1.4e-43). **These do not match the `0x20, 0, 0x40` the wave-1 doc gives for freemove.o.** The record has no name field, so identity (`id 0x2e = freemove.o`) is unconfirmed. LateralDamping's `freemove.o` reads are **OPEN**: either the id map is off or the doc's values are stale. A check: the install's `constant_player.bin` record 19 bytes, against this live record.
- Slot 0x68 on both tables: CONFIRMED by `pe.py` (below). Live not checked (no caller traced live).
- `FUN_1406b9aa0`: raw bytes only (no bridge function); not re-verified.

## 8. Slot 0x68 on both anime interfaces: CONFIRMED (static)

| Interface (vptr) | Slot 0x68 VA | Target | What it is |
|---|---|---|---|
| `+0xac0` (`0x1425e35b0`) | `0x1425e3618` | `FUN_1406ba030` | rate → class 0..3 (thresholds `0x1425dde20`…, ints `0x1425dde00`…) |
| `+0xab8` (`0x1425e3508`) | `0x1425e3570` | `FUN_1406e7ea0` = `add rcx,-0xab8; jmp FUN_1406e7220` | `FUN_1406e7220(P, motion, scale)`: `0` if `motion ≥ 0x1125`, else `|root frame|·0.01·scale` (3-D length of `FUN_1406af6d0`'s result) |

Also verified: +0xac0 slot 0x18 = `FUN_1406eadf0` (P+0x377c, rate ceiling); slot 0x40 = `FUN_1406eab00` (τ); slot 0x48 = `FUN_1406b9aa0` (curve); +0xab8 slot 0x08 = `FUN_1406ea810`; slot 0x48 = `FUN_1406e7910` (P+0x34e4).

---

## 9. Open items (exact)

1. Stage 1a: the cap constant behind `xmm15` and the rip constant `0x1ebc141` (raw asm `1406cc0c7`…`1406cc0e7`); the ring writer for `P+0x2fc`.
2. Closing cap: partner record `+0x610`, flag `+0x661`, touch vector `+0x64c..0x654` writers (partner/touch records not decoded); the `−5.0` threshold semantics.
3. `hu+8` bit 27: the bulk publisher (`FUN_1406e1ce0` / `FUN_140a47290`) and its copy source; a register-computed RMW scan.
4. Dwell gate: which arm is the dwell proper and the meaning of `req[0x1a]`/`[0x1b]`/`[0x2d]`/`[0x2e]` against the gate's returns (decode the `req[8]` = 1,6 and 2,7 arms with the caller `FUN_1406c03f8`).
5. Gate bit 30 of `P+0x2460`: callers of `FUN_140733000`'s table slot `0x1425df758`.
6. `freemove.o`: id map or record identity (§7).
7. Layer-reader semantics (which readers consume `27d8/27e4/27e6`).
8. The class word: `0x7c` byte = `req[0x1f]` vs the repo's `kFootBandSide = 0x7c` (owner of `FUN_1406c2f80` to confirm).

## 10. Budget and tooling

About 80 requests used. Scratch: `<workdrive>/gf-A3Helpers-scratch/` (`dec/*.c`, `dec/ramp.asm`, `tools/pe.py`, `tools/bit27b.py`, `tools/live_read.py`). Nothing in `/tmp`, nothing in `src/`.

## Addendum (round 2, Main's order): corrections and the four items

Supersedes §0.5, §4 (closing cap, partner fields), §7 (freemove.o live) and §9 items 3, 6, 8 where they conflict. Scratch: `tools/bit27c.py`, `tools/copy_ad0.py`, `tools/table.py`, `tools/live_read.py`.

**Correction to §0.5 (class word).** The class word is byte `0x7c` = word `0x1f`. CONFIRMED: `FUN_1406c2f80(P, req)` reads `*(int*)(req+0x7c)` (a byte offset, `req` is `longlong`), tests `< 3`, and INIT writes `= 1` (`1406c2f80` line 123, 169). Ramp (`param_2[0x1f]`, `int*`) and dwell (`*(int*)(param_2+0x7c)`) agree. Repo mismatch: `moveword::kFootBandSide = 0x7c` is used as a WORD index (`MoveReq::U(word) = words_[word]`, `movestep.hpp:184`), so every reader reads byte `0x1f0`, not `0x7c`. The right word is `0x1f`. The name is also suspect: this word is the 0..3 class used by the dwell, ramp and step tables, and the repo labels it "foot band side". Its reader `FUN_1406c2f80` is the class test. Readers to fix: `movestep_run.cpp:2259,2396`, `movestep_step.cpp:111,174,333,361`, `movestep_turn.cpp:1258,1293,3145`. Verdict: the class word (PES byte 0x7c) is right; `kFootBandSide = 0x7c` is wrong. Whether the repo's foot-band use is a second field that shares this word is OPEN.

**1. `hu+8` publisher: bit 27 is NOT P+0xad0 by offset (INF-high, source OPEN).**
- Object chain: `hu(i) = G+(i+0xa5)*0x28` is the published copy of the match-player block `PM+0xc8` (EV-CONTACT-0013). PM is the match::Player (vtable 0x1426329a8); the AnimePlayer is `P = PM+0x7d8`. So `P+0xad0` is `PM+0x12a8`, not `PM+0xd0`.
- Writers of bit 27 at disp32 `0xd0` (or/and/bts/btr with the immediate): 0 functions (`tools/bit27c.py`).
- Functions touching both disp32 `0xad0` and `0xd0`: 25 (`tools/copy_ad0.py`). The nearest, `FUN_14072f670`, does `or/and qword [rsi+0xad0], rax` with `rax = 0x1000000000` (bit 36, the mirror flag). Its `[rbp+0xd0]` is a different local. No copy of bit 27 from `P+0xad0`.
- The two bulk copies in the cited doc are not the hu publisher. `FUN_1406e1ce0` (0x18-byte struct copy) has one caller, `FUN_140805203` line 2053 (`FUN_1406e1ce0(param_1+0x634, *param_3+8)`), a contact struct. `FUN_140a47250` (shift of 0x1e entries of 0x1c) has one caller in `FUN_140a46a80`, a ring shift.
- The `hu` publisher (the 0x28-stride store into `G+(i+0xa5)*0x28`) is NOT located. A register-based store to `+0xd0` would not show in the disp scan; that is the gap. Next step: find the function that writes `G+0x28*(i+0xa5)` (imul 0x28 with `+0x1668`), then read its source offset for `+8`.

**2. registry[0x2e]: the record is freemove.o on disk; the live path LateralDamping uses does NOT reach it (plain answer: main reads the wrong bytes).**
- Disk (CONFIRMED): `common/match/constant/constant_player.bin` (dump `gf-A3Salvage-scratch`, raw WESYS table, 36 objects). Object 19 is `freemove.o`, 80 bytes. Registry order is the match table (27 objects, 0..26) then the player table, so global `0x2e` = 46 = player index 19. Payload: `+8 = 0x20`, `+0xc = 0`, `+0x10 = 0x40`, `+0x14 = 1` as the doc says. Floats from `+0x20`: 42.0, 18.0, 69.5, 0, 50.0, 10.0, 1, 3.0, 0x30, 11.1.
- Live (CONFIRMED, `pread` only): `list = *(0x1437f4808) = 0x29856e0`. `FUN_141e5ca20(list, i)` returns `*(list[i]+8)`. For i in 0x1a..0x2f every `*(list[i]+8)` is a C++ object with a vtable in the exe (0x142b5xxxx). For `i = 0x2e` it is `0x42a2b9d0`, vtable `0x142b548c8`, with inline text `age`, `json#`, `lplayerSetplay3`.
- LateralDamping (`FUN_1406cbc10` line 48-52) reads that object at `+0x10, +0x14, +0x18` as floats: 9.3e-39, 1.85e28, 4.9e-44. These are not the freemove values. So the bounds it uses on main come from the wrong object. The earlier "live 0x24, 1, 0x63" was the node's own fields (`list[i]`), not the record. That reading was my error and is withdrawn.
- The freemove record IS live: an object at `0x42a2bbd0` whose word at `+0` is `0x2e` (its registry id), with the exact freemove payload floats at `+0x20..+0x44`. Its `+0x10` is a vtable (`0x142b55d58`), so the payload's +8/+0xc/+0x10 do not sit at runtime +0x10/+0x14/+0x18. That contradicts the doc's "runtime = payload + 8" for this object.
- Nothing in writable memory holds an 8-byte pointer to `0x42a2bbd0` or `0x42a2bbf0` (3.2 GB scanned). The registry chain does not reach it. INF: a 32-bit reference, or a staging copy.
- Verdict: the id-0x2e identity is CONFIRMED (disk name, index arithmetic, live id word, exact payload match). The access path main uses is WRONG on the live build: `FUN_141e5ca20(0x2e)+0x10..0x18` is not freemove's fields. Do not port LateralDamping's bounds from the doc's denormal values. Open: which pointer the freemove object is reached through, and where its bound fields sit (candidates: the `+0x20` block, which holds plausible numbers).

**3. Closing cap: the partner and touch fields (partly CONFIRMED).**
- Partner index `pl+0x610`: the first dword of the contact out-record (`local_b8`). CONFIRMED: `FUN_1408c48c0` line 124 gates `(uint)local_b8 < 0x16`; line 164 stores it: `FUN_1404313c0(PM+0x6d8, &local_b8)`; `pl+0x610 = PM+0x6d8` (contact doc). The previous record is kept at `PM+0x72c`.
- Flag `pl+0x661` = `PM+0x729` = record `+0x51`: CONFIRMED that `FUN_1408c48c0` line 169 writes 0 every tick (after the store). I found no `=1` writer in `FUN_1408c48c0` or in the `FUN_1408c31c0` offset scan. So either it is set inside the record copy or it is always 0 at the end of the tick. OPEN; that decides whether the cap ever fires.
- Vector `pl+0x64c/+0x650/+0x654` = `PM+0x714/0x718/0x71c` = record `+0x3c/+0x40/+0x44`. Its writer is the out-record of `FUN_1408c31c0`: the relative vector `local_1d8..1d0` (lines 209-240, pl-relative subtraction). The direct store offsets were not pinned. OPEN.
- Cap semantics (ramp lines 247-278, CONFIRMED structure): if phase 3, `rate' > 0.1`, partner index `< 0x16`, flag set, and the vector non-zero: `rel = partner pos − own pos` (`+0x55c/+0x560/+0x564`), rotated by `FUN_1404bcad0`; if `rel.x < 0.3` and `0.3 < rel.z`, the vector `(+0x64c,+0x654)` is rotated by yaw and `speed = FUN_14042d470(d.z) = d.z·54·3.6`; if `speed < −5.0` then `req[0x4a] *= 0.95`. The threshold semantics (why −5, why 0.3) are INF.

**4. `kFootBandSide` vs class word: decided above.** The class word, byte `0x7c` = word `0x1f`, is right. The repo constant is the wrong word and the wrong name.

**Open, exact next steps.**
1. `hu+8` bit 27: find the store into `G+0x28*(i+0xa5)`, then read its source offset.
2. freemove: find the 64-bit or 32-bit reference to `0x42a2bbd0` (scan `.data`/heap with a 4-byte pattern too), or the object that holds `*(list[0x2e]+8)` as a field. Until then, LateralDamping's constants are unverified.
3. Flag `PM+0x729` setter and the vector writer (`PM+0x714..0x71c`): decode `FUN_1404313c0`'s copy and `FUN_1408c31c0`'s out-record stores.
4. Repo: move `kFootBandSide` to word `0x1f` (after confirming the foot-band use is a separate word).
