# PES team-AI context read by AccelerationRamp (FUN_1406cbf70) and the RUN_LOOP jostle override (FUN_1406c0170)

Revision 2 (after Main's IRC of the second round). Changes are marked **[rev2]**.

Tags. **CONF-D** = Ghidra decompile. **CONF-R** = raw bytes of the install (`ALLSTYLEPES2021.exe`; `.trace` and `.rdata` file offset = VA − 0x140000A00), disassembled with objdump. **CONF-S** = store/call scan. **DOC** = taken from a `docs/re` report, not re-checked here. **A3S** = read on `origin/A3Salvage` (f9d0401). **INF** = inference. **OPEN** = not settled; the step that settles it is named.

Ghidra's decompiler drops float arguments and float returns (it printed `FUN_1406b9e80(P)` for a call that takes `xmm1 = req[0xa]`). For the target path the raw listing is authoritative, and every constant below was read from the install.

Scratch (read-only): `<workdrive>/gf-TeamCtx-scratch/` (`asm_ramp.txt`, `ramp_target_region.txt`, decompiles `d_*.c`).

---

## 0. Summary

1. **[rev2] X is a per-player context, not G or T.** `X = *(P+0x3ac0) = PM+0x7e0`. The player constructor `FUN_1408bd970` stores `PM+0x7e0 = FUN_140a45060(new 0x4b8-byte block)` at `0x1408bda60`. `MA = X+0x18`. The constructor zeroes `X+0x18`, and the setter of that pointer is OPEN (§1).
2. **[rev2] The target path is fully named.** §4.3 gives `target = max(0, min(x, rateMax·F))`, with `x` from the base rate, the turn slow, and the proximity cap, and `F` from one of five branches. Each xmm value is traced to its source and every constant has its install value.
3. **The `MA+0x22e4c` writer is still not found** (§2.4). Entries are `{u32 id, f32 d²}`, distance-sorted (A3S, INF high on the sort direction). The search space is narrowed and the next step is named.
4. **[rev2] `T+0xb8c4[slot]` writer = `FUN_140623750`** (CONF-D). `T+0x78` is still OPEN; there is one unidentified candidate (§4.2). `G+0x180` (roster) is still OPEN.
5. **Corrections to the earlier report** (§8): the turn slow applies in every phase, and in phase 13 its turn comes from the velocity heading. The `FUN_140a62690` skip is the side's keeper. The `+0x377c` correction stands.
6. **GF and A3S state.** `AccelerationRamp` is declared in GF `src/pes/movestep.hpp:686` with no body. On A3S it is still declared-only (`movestep_probe_stubs.cpp:86`), and A3S has `LateralDamping` (`movestep_ramps.cpp:34`) and the `LocomotionStyle` ladder (`movestep_style.cpp:65`). **Neither branch has the RUN_LOOP jostle override** (`6 / 7 / 0x10`): A3S `movestep_run.cpp` has no such branch either. That override is in `FUN_1406c0170`, §5.

---

## 1. The context `X` = PM+0x7e0 **[rev2]**

| Step | VA | Evidence |
|---|---|---|
| anime `vt+0x00` returns `*(P+0x3ac0)` | `0x1406eaaf0` | CONF-R: `mov 0x3008(%rcx),%rax; ret` |
| factory: `*(P+0x3ac0) = PM[0xfc]` | `0x1408be740` | CONF-D |
| `FUN_1408be840` calls `FUN_1408be270` (builds the 160-byte request block) and then the factory | `0x1408be840` | CONF-D. `FUN_1408be270` does not write `PM+0x7e0` |
| caller of `FUN_1408be840` | `FUN_140922d40` (`0x140922d40`) | CONF-D, xref |
| **writer of `PM+0x7e0`** | `0x1408bda60`: `mov %rax,0x7e0(%rbx)` in `FUN_1408bd970` | CONF-R. `rax = FUN_140a45060(new 0x4b8 block)` (CONF-D: `param_1[0xfc] = uVar1` after `uVar1 = FUN_140a45060(local_res18)`) |
| `FUN_140a45060` | builds the object: vtables `0x14259d590`, `0x142599550`, … | CONF-D |
| `X+0x18` (MA) | zeroed at construction (`FUN_140a45060`: `param_1[3] = 0`). **Setter OPEN** | the ramp dereferences it unguarded, so it is non-null by the time the ramp runs |

`FUN_1408bd970` is the match player constructor (its vtable `0x142612fb8`, and it creates `P` lazily through the factory). So `X` is **one object per player**, created at the same time as the player, and it is neither `G` nor the team record `T`.

**Consequences.** `FUN_140a627e0(X,0) = *(*(X+0x18)+0x2601c)` (carrier, `MA+0x2601c`). `FUN_140a62690(·,side)` ignores its first argument and returns the side's keeper (A3S `pes-re-attack-decisions.md:23`). `FUN_140a62960(·,side)` and `FUN_140a62d90` ignore `X` as well.

**OPEN:** the setter of `X+0x18`. Settle with a write watch on `X+0x18` at kick-off, or by tracing the match setup that constructs the per-player `X`.

---

## 2. The 200-byte list `MA+0x22e4c + 200·i`

### 2.1 Layout (CONF-D: `FUN_140a79b20`, `FUN_14045e030`, consumers)

| Entry offset | Type | Meaning |
|---|---|---|
| `+0x00 + 8k`, `k = 0..10` | `{u32 id, f32 d²}` × 11 | team 0 sorted by distance from entry owner `i`; `k` is the rank |
| `+0x58 + 8k` | `{u32 id, f32 d²}` × 11 | team 1, sorted the same way |
| `+0xb0`, `+0xb1` | u8 | counts of team 0 and team 1 |
| `+0xb2 + id` (`id` 0..21) | u8 | rank of `id` in its own team's sub-list; `0x0a` if absent |

The value at `+4` is a **squared** distance: A3S `pes-re-run-targets-s1.md:20` says `{id, d²}`, and `FUN_1404c8ea0` (A3S `pes-re-possession-latch.md:201`) reads `d²` from it. Both consumers of the ramp and of the jostle override read only the id.

### 2.2 What `i` indexes

`i` = `P+0x2458` (the AI record index, 0..21). The entry belongs to player `i`. For `i ≥ 0x16` the base is `0x22e4c` itself.

### 2.3 Opponent selection (both consumers, CONF-D + raw)

```
own  = A.vt+8()                          // P+0x2458
base = MA + 0x22e4c + (own < 22 ? own*200 : 0)
off  = (11 ≤ own ≤ 21) ? 0 : 0x58       // own team 1 -> opponents (team 0) at +0
opp  = *(u32*)(base + off)               // rank-0 opponent; use only if < 22
```

### 2.4 Writer and sort (OPEN; search state)

- **Excluded as writers:** `FUN_1405346e0` (MA dispatcher), `FUN_140531490`, `FUN_140531260`, `FUN_140531810`, `FUN_1405325e0`, `FUN_140536fc0`, `FUN_140535c60`, `FUN_14052f4f0`, `FUN_14052f720`, `FUN_140532f70`, `FUN_140530320`. Their `+0x22e4c` uses are reads, and `FUN_140530320` builds its copy on the stack (`&local_248`).
- **Excluded callees:** `FUN_140a62140` (an average of two `FUN_140a61f40`, a read); `FUN_1408c5020` (`*(this+0x1438)`, a read).
- **`FUN_140a79b20` (rank rebuild)** has callers `FUN_14045e0d0` (operating on `MA+0x24908`, the ball-arrival list, not this list), and `FUN_140530320` (on its stack copy). So the rank rebuild for this list is not called from any known writer.
- **`FUN_14045e030` (move one entry)** is called only by `FUN_14045e0d0`, on `MA+0x24908`. Its layout is the same 11-pair team block (`param_1 + (team·11+slot)·8`), so it could operate on this list too. The sort would be built from that primitive, if it is used here.
- **Direct stores:** a store scan over the image for disp `0x22e4c` (opcode-filtered) found no store that is an obvious writer. The `add`/`lea` sites with `0x22e4c` number 288; the disp users, 558. Too many to check by hand.

**Next step (cheapest):** a write watch on `MA+0x22e4c` during a live match, or the `FUN_14045e0d0` callers on the other `MA` lists to find a caller that passes `MA+0x22e4c+200·i`.

---

## 3. Keeper records (INF high on the struct; OPEN on one offset)

`pes-re-keeper-port.md` §2.2 reads the keeper's distribution pick from `u = R+0x22e4c + slotKeeper·200`. Its `u[0] < 0x16` and `u[1] ≤ 9.0` test is the keeper's own rank-0 entry (INF high). So the keeper's table is the same runtime struct, not install data as that doc says. The flag `(char*)(u+0xb6ea)` lies outside the 200-byte entry, so the keeper doc's pointer unit is wrong or the base differs (OPEN; settle by decompiling the keeper pick).

---

## 4. Consumer A: AccelerationRamp `FUN_1406cbf70` (`0x1406cbf70–0x1406ccd00`)

### 4.1 Interfaces and writes (CONF-S / CONF-R)

`A = P+0xab8` (anime; vtable `0x1425e3508`), `B = P+0xac0` (speed/rate; vtable `0x1425e35b0`).

Persisted writes (all others are frame locals):

| Store (raw VA) | Word | Condition |
|---|---|---|
| `0x1406cc104` | `req[0x24]` = old `req[0x1f]` | phase 3, class changed |
| `0x1406cc10f` | `req[0x22]` = old `req[0x1d]` | same |
| `0x1406cc116` | `req[0x1f]` = new class | same |
| `0x1406cc13e` | `req[0x1d]` = `B.vt+0x48(r9, G)·3.6` | same |
| `0x1406ccc74` | `req[0x4a]` = final rate | always |
| `0x1406ccce3` call `*0x10(A)` | `A.vt+0x10(DAT_1425ddcf0[req[0x50]·16 + band])`, sets `P+0xae0` (`0x1406ef320`) | phase 3 only |

### 4.2 The target path, every xmm named [rev2]

**Constants (install values, CONF-R):**

| VA | Value | Use |
|---|---|---|
| `0x142565600` | 0.5 | `K_HALF`; turn-slow threshold; phase-3 range low |
| `0x14257a190` | 0.6 | `K_06`; phase-3 snap value; cap entry threshold |
| `0x14257a194` | 0.7 | `K_07`; phase-3 range high; cap threshold base; band ≥ 0.7 |
| `0x142590d98` | 0.3 | `K_03`; phase-3 floor; closing-lateral limit |
| `0x1425656c0` | 4.0 | `K_4`; lookahead ticks numerator |
| `0x14259a764` | 60.0 | `K_60`; lookahead denominator |
| `0x14256ae34` | 1000.0 | `K_KMH`; speed = `|v|·gs·60·60/1000` km/h |
| `0x142588210` | 10.0 | phase-3 speed threshold (km/h) |
| `0x1425a1ffc` | 13.5 | phase-3 speed window low |
| `0x1425c48c0` | 22.0 | phase-3 speed window high |
| `0x1425ab124` | 0.95 | `K_095`; phase-3 cap and block-C cap |
| `0x142596e6c` | 80.0 | reserve window high |
| `0x14259be1c` | 20.0 | reserve window low |
| `0x1425c48fc` | −60.0 | reserve falloff divisor |
| `0x14253ee30` | 100.0 | `K_100` |
| `0x14259be34` | 50.0 | opp-PC window low; `xmm7` default in block A |
| `0x14259d620` | 49.0 | opp-PC window span |
| `0x142598578` | 0.2 | `K_02`; cap depth; block-C tail |
| `0x14259be2c` | 40.0 | `K_40`; PC-gap span |
| `0x1425cc680` | 11.25 | `K_TURN_MIN` |
| `0x14259be5c` | 90.0 | `K_90`; turn and angle full-weight edge |
| `0x1425cca3c` | 157.5 | angle zero edge |
| `0x1425dbdb8` | −67.5 | angle falloff divisor |
| `0x14256ae1c` | 1.0 | `K_1` |
| `0x14259be38` | 55.0 | DA window low |
| `0x14259be48` | 65.0 | DA window high |
| `0x14259be84` | −10.0 | DA falloff divisor |
| `0x1425ddf7c` | 0.93 | **F default** |
| `0x1425ddf40` | 0.05 | F-A step |
| `0x1425ddf70` | 0.88 | F-A target |
| `0x1425ddf38` | 0.03 | F-B step |
| `0x1425ddf3c` | 0.04 | F-C step |
| `0x14259ce38` | 3.6 | class-speed output scale |
| `0x142588200` | 0.1 | block-C rate floor |
| `0x142588218` | −1.0 | sign |
| `0x14259be7c` | −5.0 | block-C closing limit |
| `0x1425ddf54` | 0.225 | band 1 floor |
| `0x14259b89c` | 0.4 | band 2 floor |

**Pseudo-code, with each step's raw source.** Names: `rMax = B.vt+0x18() = P+0x377c` (`0x1406eadf0`). `gs = FUN_141e532d0()`. `n = trunc(gs·4/60 + 0.5)` (raw `0x1406cc411–0x1406cc430`).

```
STAGE 1 — phase-3 class side effect (only when A.phase == 3); raw 0x1406cc026..0x1406cc1e8, 0x1406cc0ef..0x1406cc13e
  r9 = req[0x4a]
  if 0.5 <= r9 <= 0.7:           r9 = 0.6                     // raw cc02f..cc043
  if r9 >= 0.3:                                                  // else skip to CLASS (r9 < 0.3 keeps r9)
     v = |FUN_141eb50b0(P, n)| · gs · 3.6                     // root velocity over n ticks, km/h  (raw cc053..cc0c2)
     if v < 10 and r9 > 0.3:             r9 = 0.3              // raw cc0cf..cc0e3
     elif 13.5 < v < 22 and 0.6 < r9 < 0.95·rMax: r9 = 0.6     // raw cc163..cc1af
  CLASS:  c = B.vt+0x68(r9)
          if c != req[0x1f]: shift req[0x1f]→req[0x24], req[0x1d]→req[0x22]; req[0x1f] = c;
                             req[0x1d] = B.vt+0x48(r9, G)·3.6, G = FUN_141af3180(P+0xa08)   // raw 0x1406cc0f9..0x1406cc13e
  xmm9 is then zeroed (raw 0x1406cc149): the snapped r9 never reaches the target.

STAGE 2 — base target (all phases); raw 0x1406cc144..0x1406cc1ed
  if req[0xb] != 0:                         base = B.vt+0x38(req[9])
  elif req[9] == 0 and req[0] in {3, 6}:    base = 0
  else:                                      base = FUN_1406b9e80(P, req[0xa])   // SpeedToRate; xmm1 = req[0xa] (the decompile drops it)
  x = max(0, min(base, rMax))                                      // raw 0x1406cc1d8..0x1406cc1e8

STAGE 3 — turn slow (only if x > 0.5); raw 0x1406cc1ed..0x1406cc2a9
  turn = (A.phase == 13) ? wrap(heading(FUN_141eb50b0(P,1)) − req[1]) : req[5]     // raw 0x1406cc209..0x1406cc24a
  a    = (|turn| < 11.25) ? 0 : clamp(|turn| / 90, 0, 1)                             // raw 0x1406cc256..0x1406cc298
  x    = 0.5 + (1 − a)·(x − 0.5)                                                     // raw 0x1406cc2a0..0x1406cc2a9

STAGE 4 — proximity cap (block A; only if own<22, x > 0.6, P+0x27b1 ∈ {6,7}); raw 0x1406cc2b7..0x1406cc626
  own = A.vt+8();  opp = rank-0 of §2.3; if opp ≥ 22: skip
  if bit27 of hu(opp)+8 == 0  or  PC(opp) ≤ PC(own): skip           // raw 0x1406cc3bd..0x1406cc3c9
     (PC(x) = FUN_1406e6660(P,0x28) for self; ai(opp)+0x98 for opp; both ability byte 0x28)
  predSelf = pos(own) + n·vel(own);   predOpp = pos(opp) + n·vel(opp)          // raw 0x1406cc411..0x1406cc4d8
  dist = |predSelf.xz − predOpp.xz|                                              // raw 0x1406cc4de..0x1406cc501
  fracOpp = clamp((PC(opp) − 50)/49, 0, 1)                                         // raw 0x1406cc52a..0x1406cc559
  THR = 0.7 + 0.2·fracOpp                                                          // raw 0x1406cc56e..0x1406cc577
  if dist < THR:
     ang = wrap(req[2] − bearing(predSelf → predOpp))                              // raw 0x1406cc516..0x1406cc525
     w   = (|ang| ≤ 90) ? 1 : (|ang| ≥ 157.5) ? 0 : (157.5 − |ang|)/67.5          // raw 0x1406cc52a..0x1406cc5e7
     d   = PC(opp) − PC(own)
     g   = (d > 0) ? clamp(d/40, 0, 1) : 0                                          // raw 0x1406cc569..0x1406cc5a5
     x  *= (1 − 0.2·w·g)                                                            // raw 0x1406cc5f4..0x1406cc605

STAGE 5 — factor F and the ceiling t (raw 0x1406cc626..0x1406cca83)
  t = rMax·F, unless branch (a) which uses t = rMax.
  (a) NO FACTOR, t = rMax, when any of these hold (each jumps to 0x1406cca88):
        • own == FUN_140a62690(X, side(own))  (the side's keeper)            raw 0x1406cc69d
        • T(side own)+0xb6e0 != 0                                              raw 0x1406cc6ab
        • T(side opp)+0xb6e0 != 0                                              raw 0x1406cc6ec
        • own ≤ 21 and FUN_140a62d90(X, own, 0) == 0x1c4b6                  raw 0x1406cc725
        • A.vt+0x28() (= P+0xac9) == 0x4a                                      raw 0x1406cc737
  (b) own > 21:  F = 0.93                                                      raw 0x1406cc743..0x1406cc751
  Otherwise let u2 = reserve weight:
        e = FUN_140a58550(ai(own))  (a byte; reserve percent, INF)
        u2 = (e ≤ 20) ? 1 : (e < 80 ? (80 − e)/60 : 0)                         raw 0x1406cc9c1..0x1406cc9e8 (same in all branches)
  Special-role test (c): any of
        carrier == own           (FUN_140a627e0(X,0))                          raw 0x1406cc7b8
        T(side own)+0x78 == own                                                 raw 0x1406cc7d6
        FUN_140479470(G+0x180, own) != 0   (route row with id==own, flag 0)    raw 0x1406cc808
        FUN_140a62960(X, side own) == own  (MATCH_UP man)                       raw 0x1406cc83d
        pl(own)+0x5d0 == 0x32  (COVER)                                          raw 0x1406cc855
        T(side own)+0x278 + 0x40·(own mod 11) == 0x32  (queued COVER)           raw 0x1406cc88d
        T(side own)+0xb8c4[own mod 11] != 0                                     raw 0x1406cc8d3
  (c) special: F = 1 − 0.04·u2                                                   raw 0x1406cca2a..0x1406cca7f
  (d) free, and T(side own)+0x255 == 0 (no possession latch):                   raw 0x1406cc8e1..0x1406cc9aa
        u1 = (DA ≤ 55) ? 1 : (DA < 65 ? (65 − DA)/10 : 0),   DA = FUN_1406e6660(P, 0x15)
        F  = (1 − u2)·(0.93 − 0.05·u1) + 0.88·u2
  (e) free, and T(side own)+0x255 != 0 (latch set):                             raw 0x1406cc9af..0x1406cca28
        F  = 0.93 − 0.03·u2
        (The raw re-tests T(own)+0xb6e0 here; it is always 0, because branch (a) already took every nonzero case.)
  t = rMax · F

STAGE 6 — select (raw 0x1406cca83..0x1406cca9f)
  target = max(0, min(x, t))

STAGE 7 — rate step (raw 0x1406ccab8)
  rate' = FUN_1406e66d0(P, target, req[0x4a])        // clamp target to [0, rMax]; τ = B.vt+0x40(target, rate, rMax) (FUN_1406eab00);
                                                       // rate' = rate + (1/gs)/τ·(target − rate) if τ ≥ 1/gs, else target
```

**Two things the raw settles that the decompile hid:** (i) `F` depends on the chain of conditions above, not on a single formula; (ii) the target is `min(x, t)`, not `x` alone.

### 4.3 Block C: proximity closing cap, after the rate step (raw 0x1406ccb00..0x1406ccc74)

Only when `A.phase == 3`, `rate' > 0.1`, the partner `pl(self)+0x610 = p` is `< 22`, `pl(self)+0x661 != 0` (touching), and `|sep|² > 0` (sep = `pl+0x64c, 0x650, 0x654`).
Then `d = pos(p) − P.root` rotated by `−P.facing` (raw `0x1406ccbc1` with `xmm1 = P+0x20 · −1.0`). If `|d.x| < 0.3` and `d.z > 0.3`, and the closing test `FUN_14042d470(·) < −5.0` (`0x14259be7c`) holds, then `rate' *= 0.95`.
The closing vector comes from `FUN_140581ce0` and `FUN_140591a10` (semantics OPEN; the doc calls it "closing faster than 5 km/h").

### 4.4 Band and motion key (phase 3; raw 0x1406ccc34..0x1406cccee)

```
band = (rate' < 0.225) ? 0 : (rate' < 0.4) ? 4 : (rate' < 0.7) ? 8 : 12
A.vt+0x10( u16 DAT_1425ddcf0[req[0x50]·16 + band] )        // writes P+0xae0 via FUN_1406ef320
```

---

## 5. Consumer B: RUN_LOOP jostle override in `FUN_1406c0170`

Decompile lines 114–215 (`d_1406c0170.c`); the VA is ~`0x1406c0650–0x1406c0b01` (DOC). Its inputs: `MA` list head (opp), `pl(own)`, `pl(opp)`, `P+0x20`, `FUN_1406e6660(P,0x28)`, `ai(opp)` ability `0x28`, `A.vt+0x28` (= P+0xac9), `req` words. It reads **no team field** (CONF-D).

### 5.1 Entry and order

1. Enter when `((req[8] ∈ {0,5}) ∧ req[0x4a] > 0.15) ∨ FUN_1406b70d0(P,req) == 0`, and `(req[0xe] ≠ 1 ∨ req[9] = −1 ∨ req[0x2d] < req[0x2e])`.
2. Return while `req[0x1a] < req[0x1b]`.
3. `base = LocomotionStyle(P, req[0x16], req[0x4a], req)` (`FUN_1406ba2f0`; GF stub returns 0; A3S ladder `movestep_style.cpp:65`).
4. Override (§5.2) only when `own < 22` and `opp < 22` (§2.3).
5. If the result equals `req[0x50]`, return; else write the style-change block (§5.3).

### 5.2 Threshold and style rule (CONF-D, corrected)

```
ab  = FUN_1406e6660(P, 0x28)                      // self PC
thr = 0.7 + 0.3·clamp((ab − 50)/45, 0, 1)         // DAT_14257a194, DAT_142590d98, DAT_14259be34, DAT_14259be30
if A.vt+0x28() == 0x4a: thr = 0.45                 // DAT_14256b140
n    = trunc(gs·4/60 + 0.5)                        // same n as §4
pred(x) = pos(x) + n·vel(x)
bearing = FUN_140439e50(pred(self), pred(opp));  ang = wrap(req[2] − bearing)
face    = |FUN_14043a0a0(P+0x20, opp.heading)|
if dist(pred(self), pred(opp)) < thr  and  |ang| > 11.25 (DAT_1425cc680)  and  face < 67.5 (DAT_1425cca34):
    if |ang| ≥ 112.5 (DAT_1425cb30c):
        cand = (|ang| ≥ 157.5 (DAT_1425cca3c)  or  (|ang| > 112.5 and req[9] < 2)) ? 0x10 : 7
    else: cand = 6
```

- Guards (CONF-D 197–211): each candidate applies only when `req[0x50] ≠ cand`, or when `(P+0x2472 ≠ 0) == (sgn(ang) < 0)`.
- **[rev2] Correction to the earlier report:** `|ang| = 112.5` goes to style 7, not 6 (the test is `≥ 112.5` for the 6/7 split). Style `0x10` needs `|ang| ≥ 157.5`, or `|ang| > 112.5` with `req[9] < 2`.

### 5.3 Writes on a style change (CONF-D lines 215–263)

`req[0x37] = cand`, `req[0x50] = cand`, `req[0x2f] = 1`, `req[9]` clamped to 0..3, `req[0x30] = DAT_1425ddcf0[band + 4·cand]`, `req[0x31]` mirror, `req[0x32]` = `DAT_1425ddf4c` (0.1333) or `DAT_142598578` (0.2) when returning to style 0, `req[0x33] = 0`, `req[0x35] = 1.0`, `req[0x36] = 2 + (mirror changed)`, `req[0x1a] = 0`, `req[0x1b] = 0.2`, `req[0x2d] = req[0x2e] = 0`, `req[0x1e] = req[0]`, `req[0x20] = 0`.

---

## 6. Field table for both consumers (with writers)

| # | Field | Type | Read by | Writer (VA) | Status |
|---|---|---|---|---|---|
| F1 | `X = P+0x3ac0` (= `PM+0x7e0`) | ptr | A | `0x1408bda60` in `FUN_1408bd970`; object from `FUN_140a45060` | **[rev2] CONF-R** |
| F2 | `X+0x18` = MA | ptr | A, B (via §2) | zeroed in `FUN_140a45060`; setter | **OPEN** |
| F3 | `P+0x2458` own id | i8 | A, B | `FUN_1406eb670` (`param_2`) | CONF-D |
| F4 | `P+0x245c` phase | u8 | A, B | `FUN_1406eb670` = 0; `FUN_1407344f0` = 1; MOVESTEP FSM tail (`FUN_1406ba900`) | DOC |
| F5 | `P+0xac9` anime action | i8 | A (`0x4a` skip), B (`== 0x4a` threshold) | anime action dispatcher | DOC (naming of 0x4a: §8.6) |
| F6 | `P+0x2472` mirror/side | u8 | B | anime vt+0x30 (`0x1406ec700`) | writer not located (OPEN) |
| F7 | `P+0x27b1` previous style | u8 | A (`∈ {6,7}`) | GF adapter copies `req[0x50]`; PES writer not located | OPEN for PES |
| F8 | `P+0x377c` rate ceiling (`rMax`) | f32 | A, B | `FUN_1406f99f0` (`StoreRateCeiling`) | DOC |
| F9 | `P+0x20` facing | f32 | B | AnimePlayer writer (not located) | OPEN |
| F10 | `pl(i)+0x55c/0x560/0x564` pos, `+0x574/0x578/0x57c` vel | f32 | A, B | PM motion writers | DOC |
| F11 | `pl(i)+0x554` heading | f32 | B | PM motion writers | DOC |
| F12 | `pl(self)+0x610` partner id | u32 | A block C | `FUN_1408c31c0` / `FUN_1408c48c0` | DOC |
| F13 | `pl(self)+0x661` touching; `+0x64c..0x654` sep | u8 / f32 | A block C | same | DOC |
| F14 | `hu(opp)+8` bit 27 | u32 | A block A | action dispatcher (A-CLS) | not decoded |
| F15 | `ai(i)+0x98` ability `0x28` (PC); `0x15` (DA) | u8 | A, B | ai-record builder (match setup) | writer not located |
| F16 | `FUN_140a58550(ai(own))` reserve byte | u8 | A (u2) | computed from the ai record (INF: reserve %); FNV-checked block | OPEN (identity) |
| F17 | `T(side)+0xb6e0` stoppage | u8 | A (a) | `FUN_14057ac20` (pair `+0xb6df`) | DOC |
| F18 | `T(side)+0x255` latch | u8 | A (d)/(e) | `FUN_140a428d5` via `FUN_1404fe9c0` | DOC |
| F19 | `T(side)+0x78` (designated/pass-target) | i32 | A (c) | **OPEN** | §4.5 |
| F20 | `T(side)+0xa2a0[k]` state (MATCH_UP = 0x3a) | i32 | A (c) | `FUN_140a42940` | DOC |
| F21 | `T(side)+0x278 + 0x40·k` queued state (COVER = 0x32) | i32 | A (c) | `FUN_140a42b80` | DOC |
| F22 | `T(side)+0xb8c4[k]` chain byte | u8 | A (c) | **`FUN_140623750`** (CONF-D: cleared at entry; set to `FUN_140622490(...)`) | **[rev2] CONF-D** |
| F23 | `pl(own)+0x5d0` AI action (COVER = 0x32) | i32 | A (c) | PM+0x5d0 dispatcher (base open) | DOC |
| F24 | `MA+0x2601c` carrier | u8 | A (c) | carrier AI (DOC) | DOC |
| F25 | `G+0x180` roster (24 × 0x74): id `+0x20`, flag `+0xd` | u32 / u8 | A (c) via `FUN_140479470` | **OPEN** | §4.5 |
| F26 | `MA+0x22e4c + 200·i` (both teams, ranks) | struct | A, B | **OPEN** | §2.4 |
| F27 | `req[0x4a]`, `[0x1d]`, `[0x1f]`, `[0x22]`, `[0x24]` | f32 / i32 | A (writes) | `FUN_1406cbf70` | CONF-S |
| F28 | `P+0xae0` motion key | u16 | A (phase 3), B (via `req[0x30]`) | `FUN_1406ef320` (`A.vt+0x10`) | CONF-R |

### 4.5 Open writers, with narrowed candidates **[rev2]**

- **`T+0x78` (F19).** Two cheap checks gave no clean writer. (i) Disp8 `0x78` stores in `0x140930000–0x140940000` and `0x14211e000–0x142130000` (11 sites). Most are byte or qword stores to other structs. (ii) The one int32 candidate is `mov %eax,0x78(%rbp)` at **`0x14093a089`**, in `FUN_140939cc0`; its base `rbp` is not identified, and the function works on `param_1+0x560..0x5b8`, which looks like a pass-decision context rather than `T`. A3S `pes-re-receiving.md:293` lists the same writer as open ("the kick path that aims a pass"). **Next step:** confirm the base of `0x14093a089`, then a write watch on `T+0x78` at a pass kick.
- **`G+0x180` roster (F25).** The reader is `FUN_140479470`. The writer was not found by the disp `0x180` scan (581 sites, too broad). The roster is 24 entries of `0x74`, with id at `+0x20` (`pesevents.hpp:470`). The builder is probably in match setup. **Next step:** a write watch on `G+0x180` at match start.
- **`T+0xb8c4` extra sites.** Four disp-only stores `c6/c7 81 c4 b8` (`0x1402c9dd6`, `0x1408b273d`, `0x140fa5dfd`, `0x141558f00`). Probably a per-team reset. Not decoded.

---

## 7. GF and A3S mapping

| Item | GF (`main`/worktree) | A3S (`f9d0401`) |
|---|---|---|
| `AccelerationRamp` (FUN_1406cbf70) | declared `movestep.hpp:686`, **no body** | declared only (`movestep_probe_stubs.cpp:86`); **not ported** |
| `LateralDamping` (FUN_1406cbc10) | declared `movestep.hpp:688`, no body | **ported** `movestep_ramps.cpp:34` (+ install scalars `movestep_common.cpp:152`) |
| `LocomotionStyle` (FUN_1406ba2f0) | stub returns 0 (`movestep_dispatch_seams.cpp:47`) | **ported** ladder `movestep_style.cpp:65`; **no 6/7/0x10 branch** |
| RUN_LOOP jostle override (FUN_1406c0170 lines 114–215) | missing | **missing** (`movestep_run.cpp` `RunLoop` at 1795 has no such branch) |
| `FUN_1406eab00` (τ) | declared; misnamed `AccelerationRamp` in a comment (`movestep_run.cpp:11`) | same comment |
| 200-byte list `MA+0x22e4c` | missing; `OppByDist` sorts per call (`pesoffball.cpp:548`) | docs only |
| `FUN_140a62690` (keeper of side) | present in some form (`peskeeper`) | A3S `pes-re-attack-decisions.md:23` |
| `T+0xb8c4` chain | field present; writer not ported | — |

**Port note for A4:** `LateralDamping` and `LocomotionStyle` on A3S give the port its helper signatures. `AccelerationRamp` (§4) and the RUN_LOOP override (§5) are not on A3S and need writing.

---

## 8. Corrections to existing docs **[rev2 additions marked]**

1. **`pes-re-gaps-anime.md`, vt `0x18` on `+0xac0`:** the target is `this+0x2cbc` (`P+0x377c`), not `P+0x297c`. (Raw: `movss 0x2cbc(%rcx),%xmm0; ret` at `0x1406eadf0`.)
2. **`pes-re-anim-select.md` §3.3:** the `112.5°` split is `≥ 112.5 → 6/7`, and `|ang| ≥ 157.5` or `(|ang| > 112.5 and req[9] < 2)` → `0x10`. (§5.2.)
3. **`pes-re-anim-select.md` §3.6 is incomplete.** Three corrections:
   - **[rev2]** The turn slow applies in every phase, not only phase 13. Only its source of `turn` differs: phase 13 uses the root-velocity heading; other phases use `req[5]`. The slow itself is `0.5 + (1 − a)(x − 0.5)`, applied only when `x > 0.5`.
   - **[rev2]** The target is `min(x, rMax·F)` with the five-branch `F` (§4.2). The doc gives only the clamp.
   - **[rev2]** The proximity cap is `x *= 1 − 0.2·w·g`, gated by `dist < 0.7 + 0.2·fracOpp`, with `w` (angle) and `g` (PC gap) defined in §4.2. The doc's "0.95 when right ahead" belongs to block C, after the rate step (§4.3).
4. **`pes-re-anim-select.md` §3.6** calls the tracked opponent "right ahead". It is the rank-0 opponent from §2.3, and block C uses the contact partner `pl(self)+0x610` with `+0x661` (touching).
5. **`pes-re-keeper-port.md` §2.2:** the keeper table is the runtime 200-byte struct, not install data (§3).
6. **Naming conflict:** the RUN_LOOP doc calls `0x4a` a "command". The code reads it from the anime action byte `P+0xac9` (`A.vt+0x28`). Confirm which.
7. **`pes-re-movestep-wave1.md`:** this file has six passes, not nine. The brief's "ninth pass" is on `origin/A3Salvage` (`f9d0401`), not on `main`.
8. **[rev2] `pes-re-dash-reserve.md:85`** says `team = [rsi+0x7e0]`. That is `X`, the per-player context (§1), not a team record.
9. **[rev2] Decompile artefacts:** `FUN_1406b9e80` (SpeedToRate) takes `xmm1 = req[0xa]`, which the decompile drops. `FUN_1406e66d0`'s return is a float (the decompile types it as an integer).

---

## 9. Open items, ranked

1. **`MA+0x22e4c` writer and sort (§2.4).** A write watch on `MA+0x22e4c` during a live match, or the `FUN_14045e0d0`-style caller search on the `MA+0x2470c` and `MA+0x24908` lists.
2. **`X+0x18` (MA) setter (F2).** Write watch at match start, or trace the per-player constructor's caller.
3. **`T+0x78` writer (F19).** Confirm the base of `0x14093a089` in `FUN_140939cc0`; then a live write watch on a pass kick.
4. **`G+0x180` roster builder (F25).** A write watch at match start.
5. **Reserve identity (F16):** confirm `FUN_140a58550`'s return value against the breath reserve percentage.
6. **Anime action `0x4a` identity (§8.6).**
7. **The A3S `LocomotionStyle` ladder** and the RUN_LOOP override: port the override into the A3S RUN_LOOP. Not verified against live.
8. **Block C closing vector** (`FUN_140581ce0`, `FUN_140591a10`): semantics open.

---

## 10. Method and budget

- Ghidra: `FUN_1406cbf70`, `FUN_1406c0170`, `FUN_1406e66d0`, `FUN_1408bd970`, `FUN_1408be270`, `FUN_1408be740`, `FUN_1408be840`, `FUN_140922d40`, `FUN_140a45060`, `FUN_140623750` (4 KB+ output; grepped only), `FUN_140939cc0`, `FUN_140a62140`, `FUN_1408c5020`, `FUN_1405346e0`, `FUN_140535c60` and the MA-build parts.
- Raw: objdump of `.trace` bytes (VA − 0x140000A00). Constants: read from `.rdata` with the same mapping (`0x142565600` … `0x14259b89c`, §4.2 table).
- Scans: disp-`0x7e0` stores (38), disp-`0x180` stores (581), SIB/disp `0xb8c4` (33 forms), `imul 0xc8` (401), `add/lea 0x22e4c` (288), disp `0x22e4c` (558), disp8 `0x78` in two ranges (11). Results in §2.4, §4.5.
- No code was changed. No live PES read or write. No GF or A3S build.
- Budget used in this round: about 75 requests (about 190 in total for the ticket, against the ~300 cap).
