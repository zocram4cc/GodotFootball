# PES step 9 (T2b) decode: the `T+0x6a4 / B860` support-timer writers

> **Main audit 22-10-26 (bytes):** jump table `0x140502758` read from the image: kinds 1-5, 7, 9, 16 → `0x1405025BD` (FDD20, param_5 = 0), 10-14 → `0x1405025C0` (param_5 = 1), 6 → `0x1405025E5` (27A0), 8 and 15 → `0x140502615` (dead tail): match. Correction: `CMP 0xF` bounds the table at **16** entries; the 17th dword (`0x10CCCCCCC`) is padding, not an entry.

Batch: Team-tick step 9 (`FUN_140502190`, `docs/re/pes-re-tick-steps.md` slice T2b). Image
`~/ghidra-projects/scratch/Unpacker/pes_full.bin`, base `0x140000000`. Tags: **CONFIRMED**
= raw image bytes / disassembly this pass; **INF** = inference; **OPEN** = blocker.
Decompiles cited for orientation; every constant re-read from the image.

Step 9 = `FUN_140502190(T)`, called per team. `T` = team object; `T+0x62b4` = the
match-constant-registry ctx; `T+0x6d48` = the team list base; `T+0x6d50` = `T+0x8` (side
pointer, value = `T+8`); `MI = DAT_1436f3dc8+0x270` (match info, loaded to R15 at `0x1405021b5`).
The two tables written here:

- `T+0x6a4[11]` int32 = per-local-slot **reaction-delay counter** (GF: `teamRecord[+0x6a4+(id%11)*4]`,
  `src/sim/onthepitch/pesdodge.hpp:311`, "the AI reaction-delay counter", `humanoid.cpp:2261`).
- `T+0xb860[11]` int32 = the **brain copy** of the same table (`0xb860-0x6a4=0xb1bc` = the
  `-0xb1bc` in the zero-loops). 11 local slots, index 0..10.

**Not every write touches both tables.** Write-sites, by table (CONFIRMED, `dis_140502190.txt`):
- write **both** `T+0x6a4[s]` and `T+0xb860[s]`: the two payload writers (FDD20
  `d_1404FDD20.c:252-253,448-449`; 27A0 `d_1405027A0.c:435-436`), the FC0D0 sub-writer, the
  Block-B kickoff zero (`0x140502280/86`, via the `-0xb1bc` alias), the `116c≠1` zero
  (`0x140502730/36`), and the dead seed-10 tail (`0x1405026ef/6fa`).
- write **only `T+0x6a4[s]`**: the three per-tick **saturating-decay** loops
  (`0x1405024ac`, `0x140502526`, `0x140502707` — `LEA RCX,[R13+0x6a4]; MOV [RCX],EAX`, no
  `0xb860` sibling).

Consequence: `0xb860` is only ever (re)written by payloads/zeros, so it **lags** `0x6a4` by the
amount decayed since the last reset. A port that keeps a single array decays it once; a port that
keeps both MUST decay only the `0x6a4` one. See the OPEN question below.

---

## Piece 1 — `X+0x38` (the Block-B gate) — **CONFIRMED, enable = 1**

`X = FUN_141e5d4c0(T+0x62b4, 0xF)` = constant-registry entry **15** = `constant_match.bin`
record `pes15Test.o` (getter `FUN_141e5d4c0(ctx,index)`, `pes-re-constant-team.md:40`).

The gate, raw (`dis_140502190.txt`):
```
0x140502217: XOR ESI,ESI                     ; SIL = 0
0x140502245: LEA RCX,[RBP+0x62b4]
0x14050224c: MOV EDX,0xf                     ; registry index 15
0x140502251: CALL 0x141e5d4c0                ; X = pes15Test record
0x140502260: CMP byte ptr [RAX+0x38],SIL     ; CMP [X+0x38], 0
0x140502264: JZ 0x14050247e                  ; ==0 -> (skip Block B, go to event dispatch)
```
So **Block B (the per-player zero path) runs only when `X+0x38 != 0`**. The seed-10 tail at
`0x140502615` re-tests the same byte (`0x14050261a: CMP [RAX+0x38],0; JNZ 0x140502742`) and runs
only when it is **0**.

**What `X+0x38` is:** the `enable` flag of the `project1vs1` sub-block of `pes15Test`.
The deserializer `0x141e66030` writes it directly from the JSON bool (`dis_pes15deser.txt`):
```
0x141e660ea: CALL 0x140384750            ; read JSON bool
0x141e660f5: LEA RDX,[0x142b50af0]       ; key "project1vs1" (sub-block)
0x141e66103: MOVSS [RDI+0x34],XMM1       ; +0x34 decelerateThreshold (f32)
0x141e6611f: CALL 0x1403846d0            ; read JSON bool
0x141e6612b: MOV byte ptr [RDI+0x38],AL  ; ** +0x38 = enable  <-- THE GATE BYTE **
0x141e66148: CALL 0x1403846d0            ; +0x39 loseNoDodgeFlag
0x141e6616e: CALL 0x140384750            ; +0x3c pressCheckStepMoveDistance (f32)
```
The binary loader (slot 2) maps the same field to `blob[P_off+4]`, `P_off = blob[0x10]`.

**The byte value (live VFS winner, `constant_match.bin` = `4cc_06_gameplay.cpk` override):**
record `pes15Test.o` @ blob `0x3b1a` (winner) / `0x3b20` (stock), `P_off=0x20`:
```
P bytes 0..0xB: cd cc 4c 3e | 01 | 01 | 00 00 00 00 | 00 00 00 40
decelerateThreshold +0x34 = 0.2f   enable +0x38 = 0x01   loseNoDodgeFlag +0x39 = 0x01
pressCheckStepMoveDistance +0x3C = 2.0f
```
**`enable = 0x01` in BOTH stock (`dt18_all.cpk`) and the mod.** Therefore:
- **Block B (zero path) is LIVE.** It runs: on `matchState==3` (kickoff, `R14+0x291c`) it zeroes
  all 11 slots of both tables (`0x140502274-0x140502290`); otherwise (`!isCarrier`) it runs the
  per-player `FUN_1405027A0` zero-path for each on-pitch slot (`0x140502333` loop, call at
  `0x14050244f`).
- **The seed-10 tail (`0x140502615`) is DEAD CODE** on every install we can read (enable≠0).
  TeamT1 does NOT need to port it for fidelity; record it as dead-on-stock-and-mod.

> NOTE (brief vs. doc): `X+0x38` is NOT a generic "test enable" — it specifically gates the
> `project1vs1` (1-on-1) reaction-timer model. When `enable==0`, PES falls back to a fixed
> "seed-10" two-player distance routine instead of the per-player model. With `enable==1`
> (real install) the per-player model is what runs.

---

## Piece 2 — the two event payloads

Step 9's newest-event dispatch (after the in-play gate `MI+0x116c==1`):

```
0x14050247e: CMP [R15+0x116c],1 ; JNZ 0x140502726   ; not in-play -> zero BOTH tables (11 slots)
0x14050248c: EAX=event[0x1c]; TEST EAX,EAX ; JZ 0x140502549  ; event[0x1c]==0 -> frame test below
0x140502499: CMP [R15+0x1775],0x18 ; JZ 0x140502549  ; MI+0x1775==0x18 -> frame test below
0x1405024a7: CMP EAX,5 ; JZ 0x1405024cc              ; event[0x1c]==5 -> FC0D0 sub-path
0x1405024ac: decay 0x6a4-only (11 slots, floor 0); JMP end   ; other event[0x1c] (!=0, !=5)
0x1405024cc: FC0D0: if (MI+0x1198 - event[0x10]==1 && event[0x0]==5 && [T+0x6c40]!=3)
             CALL 0x1404fc0d0 ; [T+0x6c40]=3 ; JMP end ; else decay 0x6a4-only (0x140502526)
0x140502549: EAX=[R15+0x1198]; CMP EAX,event[0x10] ; JNZ 0x140502707  ; frame!=event.frame -> decay 0x6a4-only
0x14050255b: EBX=event[0x0] (kind); MOV DIL,1
0x1405025a0: DEC EBX (idx=kind-1); CMP 0xf ; JA 0x140502615  ; kind>16 -> dead seed-10 tail
0x1405025bb: JMP [0x140502758 + idx*4]                ; jump table dispatch
```

Jump table `0x140502758` (17 dwords, byte-verified this pass), `idx = kind-1`:

| kind | idx | target | handler |
|---|---|---|---|
| 1,2,3,4,5 | 0-4 | 0x1405025bd | **FDD20** `param_5=0` |
| 6 | 5 | 0x1405025e5 | **27A0** `param_5=param_6=SIL=0`, `param_7=0xff` |
| 7 | 6 | 0x1405025bd | **FDD20** `param_5=0` |
| 8 | 7 | 0x140502615 | seed-10 tail (no payload; dead on enable=1) |
| 9 | 8 | 0x1405025bd | **FDD20** `param_5=0` |
| 10,11,12,13,14 | 9-13 | 0x1405025c0 | **FDD20** `param_5=1` |
| 15 | 14 | 0x140502615 | seed-10 tail (dead on enable=1) |
| 16 | 15 | 0x1405025bd | **FDD20** `param_5=0` |
| (17) | 16 | 0xcccccccc | default (none) |

> **MATCHES the brief** ("FDD20 kinds 1-5,7,9,10-14,16; 27A0 kind 6") — verified against the raw
> 68-byte table dump (raw hex + two independent index scans agree). `param_5` is set by `DIL`:
> the `0x1405025bd` target does `XOR DIL,DIL` first (`param_5=0`); the `0x1405025c0` target
> keeps `DIL=1` from `MOV DIL,0x1` at `0x14050255f` (`param_5=1`), so **kinds 10-14 pass
> `param_5=1`, all other FDD20 kinds pass `param_5=0`**. The in-play `kind==5` sub-path at
> `0x1405024cc` (before the jump table) calls `FC0D0` — a third, smaller writer.

### `FUN_1404FDD20(param_1=T, param_2=side, param_3=side, param_4=regctx, param_5=0|1)`
`d_1404FDD20.c`. Loops 11 local slots (`local_154` 0..10, `local_148`=slot). Global member
index `uVar14 = param_3*11 + local_150`. **Writes:**
```
T+0x6a4 + local_148*4  = (int)(longlong) fVar
T+0xb860 + local_148*4 = (int)(longlong) fVar      (d_1404FDD20.c:252-253 and 448-449)
```
Two write sites (both = the per-slot reaction timer, int-truncated from a float):
1. **Early branch** (`param_5==0`, `local_118==local_f8`, event not kind-4, event kind in
   {0..2,16}, receiver != this slot): `fVar = fVar33 * fVar29` where
   `fVar33 = clamp((T+0x2940 - fVar30)/(fVar11 - fVar30), 0..1)` (a heading/stamina ramp),
   `fVar30 = 0.02f (0x1425c4894)` or `0.05f (0x1425c4898)` if kind 2. **CONFIRMED** structure +
   constants.
2. **Main branch** (else): a multi-step RNG-driven estimate — base `fVar29 = sqrt(dist2)`
   (`MA+0x22e50[rank+bucket]*8` if `param_5==0` else `MA+0x24710[...]`), then
   `fVar34 = ((reg47.+0x74 - reg47.+0x78)*(99f - fVar30)/100f + reg47.+0x78 + fVar9)`
   (`FUN_141e5d4c0(param_4,0x2f)` = registry entry 47), `* (rng/60f)`, then distance/role/
   GK-attribute multipliers, then `fVar = fVar29` (floor `fVar31`). **CONFIRMED** structure;
   the per-branch multiplier constants below are image-read.

Key constants (all image-read this pass, `repr`/bits):

| VA | value | use |
|---|---|---|
| 0x142565600 | 0.5f | general half-scale (fVar3) |
| 0x142565604 | 2.0f | (fVar31 default) |
| 0x1425656c0 | 4.0f | (fVar11 upper bound) |
| 0x142590d98 | 0.3f | early-branch RNG blend (fVar7) |
| 0x14259b89c | 0.4f | GK/side scale |
| 0x14259be04 | 12.0f | distance band |
| 0x14259be20 | 25.0f | 27A0 touch band |
| 0x14259be2c | 40.0f | 27A0 (fVar34) |
| 0x14259be34 | 50.0f | 27A0 card floor |
| 0x14259be5c | 90.0f | card upper (FDD20) |
| 0x14259be6c | 160.0f | long-ball add |
| 0x14259be80 | **-7.0f** | FDD20 main: `(fVar29-10f)/-7f` (`DIVSS [0x14259be80]` @ `0x1404fe7a2`, CONFIRMED real code, not a decompiler artefact) |
| 0x14259a764 | 60.0f | RNG scale |
| 0x142588210 | 10.0f | distance band |
| 0x142588214 | 48.0f | 27A0 (fVar6) |
| 0x14253ee30 | 100.0f | registry normalize |
| 0x1425c4894 | 0.02f | early-branch (kind≠2) |
| 0x1425c4898 | 0.05f | early-branch (kind 2) |
| 0x1425c48a4 | 0.22f | 27A0/FC0D0 RNG |
| 0x1425c489c | 0.09f | FC0D0 kind-2 RNG |
| 0x1425c48b8 | 14.0f | FDD20 main |
| 0x1425c48c8 | 73.0f | card→time offset |
| 0x1425c48cc | 99.0f | (fVar28) |
| 0x142591140 | 30.0f | (fVar27 default) |
| 0x14259d620 | 49.0f | 27A0 card denom |
| 0x142598578 | 0.2f | 27A0 (fVar8) |
| 0x142598580 | 180.0f | FC0D0 angle |
| 0x14259858c | 360.0f | FC0D0 angle |
| 0x142598598 | -360.0f | FC0D0 angle wrap |
| 0x14259bdd0 | 0.6944f | FDD20 (0.6944) |
| 0x14259bddc | 1.5f | FC0D0 ball-speed gate |
| 0x14259222c | 5.0f | FC0D0 offset |
| 0x14257a190 | 0.6f | 27A0 RNG |
| 0x14257a194 | 0.7f | 27A0 RNG |
| 0x142572348 | 0.8f | 27A0 (fVar5) |
| 0x14258c264 | 3.0f | (fVar8 alt) |
| 0x14256ae1c | 1.0f | (fVar4) |
| 0x142588218 | -1.0f | FC0D0 sign |

`param_5` semantics: **0** = "this side's own reaction estimate" (uses `MA+0x22e50` distance
table, the early-branch is reachable); **1** = "opponent/reaction-estimate" (uses `MA+0x24710`,
early-branch suppressed). **Kinds 10-14 pass 1, all other FDD20 kinds (1,2,3,4,5,7,9,16) pass 0.**

### `FUN_1405027A0(param_1=T, param_2=side, param_3=side, param_4=0, param_5=0, param_6=0, param_7=0xff)`
`d_1405027A0.c`. Loops 11 local slots. Gate to run the per-slot model:
`MI+0x116c==1 && (param_6!=0 || !isCarrier)`. **Writes:**
```
T+0x6a4 + lVar17*4  = (int)(longlong) fVar25     (d_1405027A0.c:435)
T+0xb860 + lVar17*4 = (int)(longlong) fVar25     (d_1405027A0.c:436)
```
Core (per on-pitch slot, `d_1405027A0.c:140-436`):
```
fVar25 = (int)(rng()*0.6f) - (int)(rng()*0.2f) ... (base, see OPEN-1)   (d_1405027A0.c:157-160)
         [param_5==0 -> *0.5 ; state3&<=1s -> *0.5 ; other-side touch -> + (rng()*0.2f+0.5f)]
         [>0 -> + (99f - min(card,50f)) / (49f / (int)(rng()*0.2f))   (card/pace term)]
         [opponent-touch & GK/keeper rules -> max with rng()*0.7f (same-slot) or rng()*0.2f]
         [touch-class / position / card / side / GK multipliers: *0.5 *0.4 *0.8 *0.2 ...]
fVar25 = clamp( fVar25, floor 0.5f, ... ) ;  fVar25 = max(fVar25, T+0x6a4[slot]+0.5f)   (monotonic non-decay)
[state 0x11 (17) -> fVar25 = (T+0x6b8[slot] + old) ; randomize down]
T+0x6a4[slot] = T+0xb860[slot] = (int) fVar25
```
`local_100`/`uStack_108` — **OPEN (see OPEN-1).** The decompile reads them as the ball's y/z and
claims a preloaded accumulator, but that is **not supported**: 27A0's prologue never reads XMM0
(`XORPS XMM0,XMM0` is its first XMM0 use, `0x140502870`), and the two step-9 call sites
(`0x14050244f`, `0x140502610`) do not preload XMM0 either. The raw shows `[RSP+0x100]` is a
**XMM9 spill** (`MOVAPS [RSP+0x100],XMM9` @ `0x140502a5f`; XMM9 then set to `0x142598578`=0.2f and
used in the `MULSS …,XMM9` terms; restored @ `0x1405033e5`), and `[RSP+0x110]` is the XMM8 spill —
so Ghidra is aliasing the spilled XMM9 low 12 bytes onto `local_100`/`uStack_108`. The decompile's
init `local_100=uStack_108=0` (`d_1405027A0.c:82-83`) is the only proven value; if it holds, the
base term is just `(int)(rng()*0.6f) - (int)(rng()*0.2f)` and the `* uStack_108 * uStack_108 + local_100`
factors are 0. `param_4` is unused in the step-9 calls (`0`; `MOVSXD R15,R8D` uses param_3=side).
`param_7=0xff` = "all 11 slots" (the `(0x15 < param_7 || param_7==uVar10)` slot gate is always true).

The per-player **zero-path** call (Block B, `0x14050244f`) and the **kind-6** call (`0x140502610`)
pass **identical** args `param_5=param_6=0, param_7=0xff` (SIL=0 at both; SI is never written
between `XOR ESI,ESI`@`0x140502217` and the call). So the two 27A0 call sites are the same model,
gated only by the surrounding flow (Block-B by `!isCarrier`, kind-6 by the jump table).

### `FUN_1404FC0D0` (the in-play kind-5 sub-writer, `d_1404FC0D0.c`)
Full gate (`dis_140502190.txt`, all CONFIRMED): `MI+0x116c==1` **and** `event[0x1c]==5`
(reached only because `event[0x1c]!=0` and `MI+0x1775!=0x18`) **and** `MI+0x1198 -
event[0x10]==1` (the tick *after* the event's own frame, `0x1405024cc-4db`) **and**
`event[0x0]==5` (kind, `0x1405024dd`) **and** `[T+0x6c40]!=3`. Writes `T+0x6a4[uVar11*4]` and
`T+0xb860[uVar11*4]` (`d_1404FC0D0.c:137-138`) where `uVar11 = memberIndex % 11`. Value = `0`
(the `(float)rng()*0.0` zero path) **or** `(int)(rng()*0.22f)&0xffff + sVar12` (a small RNG value,
`sVar12` from the facing/heading term `FUN_140a8f7c0`). Sets `[T+0x6c40]=3` on return (one-shot
latch, `0x140502517`). Call args: `RCX=T+0x6d48, RDX=T+0x6d50, R8=[RSP+0x40] (=T+0x68), R9D=T+0x8,
param_5=[T+0x6c40]`. **CONFIRMED** structure.

---

## Piece 3 — the kind-5 read of `MI+0x1554` at event time — **CONFIRMED: not in step 9**

`MI+0x1554` is the **restart-situation code** (0 = in-play, 1..5 = a restart/set-piece kind).
This is confirmed by the live sample in `docs/re/pes-re-live-teamplayer.md:50,52,57`:
> "in all 3,909 valid ticks, `MI+0x116c==1` occurs exactly when `MI+0x1554==0` ... Every
> 116c 2-or-3 tick has 1554 in 1..5."

**Step 9 does NOT read `MI+0x1554`.** Its full disassembly (`dis_140502190.txt`, 359 lines)
reads only `MI+0x116c`, `MI+0x1775`, `MI+0x1198` (grep for `0x1554`/`R15+0x1554` → no hit), and
neither `FUN_1404FDD20` nor `FUN_1405027A0` nor `FUN_1404FC0D0` reads `0x1554`.

The **confirmed** `MI+0x1554` read sites are:
- **Step 8** `FUN_1405008e0` (`d_step8.c:16`): `if ( [T+0x255]==0 && MI+0x116c != 1 && MI+0x1554 == 5 )`
  → the set-piece/formation trigger (writes `T+0x6c40=2`, walks 24 formation slots).
- **Steps 15-17** `0x1404FA9F9` / `0x1404FBCE2` (`pes-re-tick-steps.md` §3, DOC).

So the kind-5 path's *event-time* reads in step 9 are actually **`MI+0x1198`** (match frame
counter) and **`[event+0x10]`** (the event's own frame) — the `frame == event.frame` test — not
`MI+0x1554`. The brief's "kind-5 path reads `MI+0x1554`" is a **cross-reference** to the
step-8 restart gate (`==5`), the same stoppage code family. **Port the `MI+0x1554==5` gate from
step 8, not step 9.** For step 9's kind-5 path, port `MI+0x1198` and the event-frame compare.

---

## Port spec for TeamT1 (GF fields, order)

Order of operations inside step 9 (per team, per tick):
1. `X = matchConstRegistry[15]` (pes15Test). Read `X.enable` (`+0x38`, stock=1).
2. If `X.enable != 0` (always, on stock+mod):
   - If `matchState==3` (`match+0x291c`, R14): `for s in 0..10: react[s]=brainReact[s]=0`, then
     fall through to step 3.
   - Else if **this team is not the ball-carrying side** (`[T+0x255]!=0` and the carrier's side
     `!= T+0x8`, `0x140502205/23f`): `for s in 0..10 (on-pitch only): react[s]=brainReact[s]=
     T27A0(T, side, side, 0, p5=0, p6=0, p7=0xff, s)` ← the **FUN_1405027A0 zero-path** (live).
     Then **fall through** to step 3 (the per-player loop has no early exit; both run per tick,
     Block-B writes first, the dispatch then decays/overwrites).
3. In-play newest-event dispatch, only if `MI+0x116c==1` (else zero BOTH tables, `0x140502726`):
   **A.** If `event[0x1c]==0` **or** `MI+0x1775==0x18` (`0x14050248c/499`) → **the normal
       dispatch** (the frame test + jump table below). This is the main path (fresh-event
       dispatch fires when the newest event carries no 0x1c payload).
   **B.** Else if `event[0x1c]==5` (`0x1405024a7`, the FC0D0 sub-path):
       - if `MI+0x1198 - event.frame==1` **and** `event.kind==5` **and** `[T+0x6c40]!=3`:
         `react[memberIdx%11]=brainReact[memberIdx%11]=FC0D0(...)`; `[T+0x6c40]=3` (both tables).
       - else: decay `0x6a4`-only (11 slots), `0x140502526`.
   **C.** Else (`event[0x1c]!=0` and `!=5`, `0x1405024ac`): decay `0x6a4`-only (11 slots).
   **D.** Dispatch (from A): the frame test `MI+0x1198==event.frame` (`0x140502549`).
       - If **mismatch** → decay `0x6a4`-only (11 slots), `0x140502707`.
       - If **match** → jump table (idx = `event.kind-1`; table verified above):
         - kind 1,2,3,4,5,7,9,16 → **FDD20**(param_5=0); kind 10,11,12,13,14 → **FDD20**(param_5=1).
           Both: `for s in 0..10 (on-pitch): react[s]=brainReact[s]=FDD20(T, side, side, regCtx, p5, s)`
           (writes both tables, re-aligning them).
         - kind 6 → **27A0**(T, side, side, regCtx, p5=0, p6=0, p7=0xff) — identical args to the
           Block-B zero-path call; `for s in 0..10: react[s]=brainReact[s]=27A0(...,s)` (both tables).
         - kind 8,15 (and kind>16, unreachable) → seed-10 tail: **returns immediately on
           enable==1 (stock)** → skip for the port.
   (Decay = `react[s]=max(react[s]-1,0)` on **only the `0x6a4` array**; `0xb860` is NOT decayed.)

**Table asymmetry (the key port point):** the per-tick decay decrements **only `T+0x6a4`**.
`T+0xb860` is (re)written **only** by the payload writers (FDD20/27A0/FC0D0) and the two zero
paths — it is never decayed. So after a payload event the two arrays are equal, then `0x6a4`
counts down each tick while `0xb860` holds the last written value. GF currently keeps **one**
array (`pesdodge.hpp:311`, read by the dodge gate `humanoid.cpp:2261`) = the **`0x6a4` side**
(the decayed counter). That is the correct single-array model: keep `react[11]`, decay it per
tick, write payload values into it. Do **not** model a second `0xb860` array (no GF reader uses
it); if parity with PES-internal readers of `0xb860` is ever needed, add it and decay only
`0x6a4`. Flag: `// RE-NOTE: PES decay touches only 0x6a4; 0xb860 is payload/zero-only (docs/re/
pes-re-treact.md)`.

GF mapping (resolved): keep **one** array = the `0x6a4` side = GF's `teamRecord[+0x6a4 +
(id%11)*4]` (`pesdodge.hpp:311`, read by the dodge gate `humanoid.cpp:2261`). Decay it per tick;
write payload/zero values into it. The `0xb860` brain copy has **no GF reader** and is never
decayed by PES, so do not model it. (The earlier "always written equal" reading was wrong: only
the payload and zero writers write both; the per-tick decay writes `0x6a4` alone.)

Constants: read them from the install at runtime (`PeImage::ReadVa` / VFS), **never** hard-code
the numbers above — the table above is for the spec/verification, and the stock values are the
pins (brief-rules: internalized values must be stock `dt18_all.cpk`).

---

## OPEN questions (blocker / needs a deeper trace)

1. **27A0 base-term inputs** — `local_100`/`uStack_108` (`d_1405027A0.c:159-160`) have no
   supported provenance: 27A0's prologue never reads XMM0, neither step-9 call site preloads it,
   and the raw shows `[RSP+0x100]`/`[RSP+0x110]` are XMM9/XMM8 spills (Ghidra aliases XMM9's low
   12 bytes onto them). The only proven value is the decompile's init `=0`. **Needed:** a
   register-level trace of XMM9/XMM0 across `0x1405027a0..0x1405033e5` to show whether a ball or
   player position actually reaches the base term, or whether it collapses to
   `(int)(rng()*0.6f)-(int)(rng()*0.2f)`. Until then, port the base as the two RNG trims and flag
   the two extra factors `OPEN-1` — do not ship a "ball.yz" that we did not observe.
2. **FDD20 main-branch multiplier constants** — the structure and the table constants above are
   CONFIRMED (image-read), but the exact per-branch multiply/divide sequence (`fVar34`, distance/
   role/GK-attribute terms) needs a single pass of the decompile re-checked against the
   disassembly to pin which constant multiplies which term. Port the early-branch (param_5=0,
   kinds 1-5,7,9,16) first; it is the one with a simpler, fully-traced form.
3. **`0xb860` reader in PES** — confirmed no GF reader; unverified whether a PES-internal reader
   (outside step 9) consumes `0xb860` and would notice it lags `0x6a4`. Out of scope for the
   step-9 port; only matters if a future slice ports that reader.


---

## Self-checking checker

`<workdrive>/gf-TReactRE-scratch/treact_check.py` — re-derives every CONFIRMED byte from the
image + the live install and fails on mutation. Run: `python3 treact_check.py`.
