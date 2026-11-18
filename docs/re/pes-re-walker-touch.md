# pes-re-walker-touch: which PES path touches a resting ball for a designated MOVE man, and which GF rule withholds it

Status: decode done, no code changed, no commit. Scratch: `<workdrive>/gf-WalkerTouch-scratch/` (`verify_vas.py`, disassembly dumps, decompile notes). Image: `pes_full.bin` (flat: file offset = VA − 0x140000000). Ghidra bridge (port 8080) for decompiles/disassembly; byte reads by script from the image.

## 0. Constants (script output pasted, `verify_vas.py` ALL-PASS)

```
PASS float 0x14253ee30 C1 10m gate 100.0 0000c842
PASS float 0x14256ae1c one 1.0 0000803f
PASS float 0x142565600 half 0.5 0000003f
PASS float 0x14259858c wrap+ 360.0 0000b443
PASS float 0x142598598 wrap- -360.0 0000b4c3
PASS float 0x1425eb848 trap kind threshold (~0.71) 0.7099999785423279 8fc2353f
PASS float 0x14259be20 ebf90 frame factor 25.0 0000c841
PASS float 0x14259be58 ebf90 angle floor 85.0 0000aa42
PASS float 0x1425cca3c ebf90 angle commit 157.5 00801d43
PASS float 0x14256ae34 ebf90 speed scale 1000.0 00007a44
PASS float 0x1425a9bd4 kick-branch ball height (approx) 0.217371866106987 bb965e3e
PASS float 0x14263e398 kick-branch 38km/h numerator 38000.0 00701447
PASS float 0x142598590 kick-branch s/h 3600.0 00006145
```

Operand-VA pairs (disassembly shows the memory operand at the citing instruction): `0x1409FA480 COMISS XMM0,[0x14253EE30]` = 100.0; `0x1407B3525/0x1407B3536 COMISS XMM0,[0x1425EB848]` = 0.71; `0x1407B0D53 LEA R8D,[RDX+0x57]` with EDX=7 gives R8D=0x5E (94).

## 1. PES answer: TRAP 7 admitted on the MOVE tick, not by reaction entry 3

**Action: TRAP (anime action 7).** `FUN_1406B4580(anime, action, ball, in, out)` returns nonzero when the action is admissible and dispatches the row pick through `+0x68` of `FUN_1406D9010(anime+0xC40)` (CONFIRMED decompile: `(**(code **)(*plVar1 + 0x68))(...)`). The commit is `FUN_1406EEBB0(anime, 7, 0x5E)`.

**Admitting callers for a MOVE/idle man (CONFIRMED bytes + `.pdata`-anchored, `verify_vas.py`):**

| Site | Bytes | Effect |
|---|---|---|
| `0x1407B0D42` in `FUN_1407B05F0` | `E8 3938F0FF` → `0x1406B4580`, EDX=7 (`0x1407B0D3C LEA EDX,[RCX+6]`, ECX=1) | TRAP admission |
| `0x1407B0D57` | `E8 54DEF3FF` → `0x1406EEBB0`, EDX=7, R8D=0x5E | commit to TRAP 7 |
| `0x1407B35C9` in `FUN_1407B3470` | `E8 B20FF0FF` → `0x1406B4580`, EDX=7 (`0x1407B35BD MOV EDX,7`) | TRAP admission |
| `0x1407B35E6` | `E8 C5B5F3FF` → `0x1406EEBB0`, EDX=7, R8D=0x5E | commit to TRAP 7 |

Both sites are reached indirectly: `FUN_1407B37D0` dispatches on the anime sub-state (`+0x20` slot) and calls `FUN_1407B05F0` for cases 3,4,5,6,9,0xC,0x14,0x15,0x18 and `FUN_1407B2450` (case 8, which calls `FUN_1407B3470`) — CONFIRMED decompiles. `FUN_1407B37D0` itself is only referenced from DATA `0x1425E2460` (a vtable slot; INF: owning action name not established this pass, but its stay-path calls `FUN_1406EEBB0(param2,2,0x5E)` = back to MOVE 2, and it admits actions 0x52/0x53/5/7 — a MOVE/dribble tick, not the reaction).

**Gates on the MOVE-tick TRAP admission (CONFIRMED disassembly + decompile):**
- `*(ball+0x5C3) != 0` (`0x1407B0C76 CMP byte ptr [R13+0x5C3],0 / JZ refuse`; `0x1407B3599` same test in `FUN_1407B3470`) — ball-present flag in the trap externals (INF on the writer).
- `plan+0x35 == 0` (`0x1407B0C8D CMP byte ptr [R12+0x35],0 / JNZ refuse`) — zero plan byte.
- `(**(act+0x70))(act, anime, 7) != 0` — the current action object's own TRAP-admit handler (decompile `FUN_1407B05F0` tail, `FUN_1407B3470` is unconditional on this slot but takes the same externals).
- Request inputs: angle `*(plan+4)` wrapped to [0,360) with ±360.0; kind `(0.71 <= *(ball+0x5E4)) + 3` (kind 3 or 4, float at `0x1425EB848` = 0.71); sub `*(ball+0x5CC)`; flag 1. `FUN_1407B3470` additionally runs `FUN_1406EBF90` (approach test with 25.0/60.0/0.5/85.0/157.5/1000.0); on true it zeroes the sub word. INF on the facing semantics — not used for the red fact.

**Why NOT reaction entry 3:** `FUN_1409F8220` → `FUN_1409F8640` → `FUN_1406B4580(...,7,...)` at `0x1409F878D` / retry `0x1409F880D` (both `MOV EDX,7`, CONFIRMED) admits TRAP for men inside the C1 table (`0x1409FA480 COMISS vs [0x14253EE30]` = 100.0 m², i.e. 10 m; TRAP-admissible test `0x1409FA550 CALL 0x1406B4C10(obj,7)`), but `FUN_1409FA520` refuses the designated owner first: `0x1409FA57B CMP dword ptr [RBX+0x2600C],ESI / JZ refuse` (MA+0x2600C == me → return false, CONFIRMED decompile+bytes). The kick branch (`0x1409F84F5`, EDX=0x5D) needs ball-sample height ≤ ~0.2174 (`0x1425A9BD4`) and |v_player − v_ball| < 38000/(gs·3600) m/tick (`0x14263E398`/`0x142598590`, gs=54 → 0.1955 m/tick) — a moving-ball branch, not the resting case. The TRAP reach test `0x140799060` lives inside the TRAP action tick, so it never runs for a man who is never admitted (brief-given, consistent with the census: walker clip is MOVE/idle fn14/ac8-2, trap firings only 93).

**Caller count:** bridge `function_xrefs` reports **74** direct call sites of `FUN_1406B4580`; the brief's "76" is not reproduced (delta 2, OPEN — possibly stale or two DATA/vtable refs; no indirect-call scan was run).

## 2. GF diff on origin/MergeBKBJ (`abe9f81d`, read-only)

GF ported the reaction's designated-owner refusal but not the MOVE-tick admission, and its only body-contact path requires walking away:

1. `src/sim/onthepitch/player/humanoid/humanoid.cpp:584-597` — `TriggerControlledBallCollision` fires for the designated man only if `functionType == Movement && ballDistanceNow < 0.6 && ballDistanceFuture > 0.65 && ballDistanceFuture > ballDistanceNow` (plus `coords[2] < 1.6`). An arriving man has `future < now`, so the first alternative never fires on arrival; the second needs Trip + HasPossession. **This is the rule that withholds the touch.**
2. `src/sim/onthepitch/player/controller/playercontroller.cpp:400` — PesLoop ports the refusal (`RefusedAsDesignatedOwner`), and `:405-407` the carrier clause, but there is no caller of a `FUN_1407B05F0`/`FUN_1407B3470` equivalent: the MOVE-tick admission has no GF counterpart (grep over `pesmovestep.cpp` shows no trap/receive decision; MOVESTEP steers the Movement command target only).
3. `src/sim/onthepitch/player/controller/elizacontroller.cpp:634` — `hasIt = retainer==player || HasPossession()`; `:639-659` the step-onto-ball branch needs `!hasIt` (and is `!PesLoop()`-gated at `:643`), so on the PesLoop path and whenever `AI_HasPossession` is true the man gets `GetOnTheBallCommands`/press-approach instead of a touch; `:701` the far branch needs `> 0.5 m`.
4. `src/sim/onthepitch/AIsupport/AIfunctions.cpp:1089-1116` — `AI_HasPossession`: false beyond 5.0 m; false if `|z| > 0.5`; true only inside radius **1.0 m** (`.GetLength() > radius` with `radius = 1.0f`, `:1100`) and relative speed ≤ 6. No PES VA (stand-in): it makes `hasIt` true exactly in the 0.3–1.0 m stall band, suppressing even GF's own step.
5. `src/sim/onthepitch/player/controller/playercontroller.cpp:387,410-418` — `_ReceiveCandidate`: C1-table port (`WithinBallDistanceTable`), GF-mode `hasPossession → false` (`:410`), GF-mode designation gate (`:416-418`); on PesLoop the trap goes through `RunTrapAdmit`/`RunTrapKind0` + retry, which never fires for these men (measured stall).

Net: PES refuses the designated man in the reaction (same as GF) but admits him on the MOVE tick (which GF lacks); GF additionally demands he walk away from the ball to touch it.

## 3. Red-test PES fact

> A designated MOVE man (action 2, speed ~0) 0.5 m from a ball at rest, facing it, with trap externals ball-present (`+0x5C3 != 0`) and a zero plan byte (`+0x35 == 0`), whose action object admits TRAP via its `+0x70` handler, is admitted to **TRAP 7** through `FUN_1406B4580` with EDX=7 (sites `0x1407B0D42` / `0x1407B35C9`) and committed by `FUN_1406EEBB0(7,0x5E)` (`0x1407B0D57` / `0x1407B35E6`) **within 54 ticks** (1 s at gs=54). The reaction path's designated-owner refusal (`0x1409FA57B`) does not apply on this tick. Harness choices (not PES): d=0.5 m, facing ±10°, ball speed ~0, N=54. OPEN (not asserted): a decoded PES distance/facing/speed bound on this path; the `+0x5C3/+0x5CC/+0x5E4` writers.

GF expectation on `abe9f81d`: no `e_FunctionType_Trap` command and no TRAP action within the window (withholder `humanoid.cpp:586-597`).

## 4. Verification

- `verify_vas.py`: **ALL-PASS, 0 failures** (flat control, 8 CALL targets, 6 gate opcodes incl. EDX=7 setups and `LEA R8D,[RDX+0x57]`=0x5E, 13 float bits, 10 `.pdata` anchors with section at file `0x3AC5000`).
- Mutation: `--mutate` (one planted off-by-one VA `0x1407B0D43`) → **FAIL as required** (`0x1407B0D43 not E8`, RESULT FAIL 1 failure).
- Method notes: flat mapping controlled at `0x14256AE1C`=1.0f before every read; every cited code VA `.pdata`-anchored (leaf-function fallback not needed — all anchored); Ghidra xrefs used only for caller enumeration, bodies read from decompile+disassembly; no live reads, no builds.

## 5. Open

1. Name the vtable owning `FUN_1407B37D0` (slot `0x1425E2460`) and the writers of trap externals `+0x5C3/+0x5CC/+0x5E4` (field-scan per `pes-image-field-scan` §3).
2. Reconcile 74 vs 76 callers of `FUN_1406B4580` (indirect/vtable scan).
3. Facing/speed/distance bounds on the MOVE-tick trap path (the 85.0/157.5/25.0 approach test `FUN_1406EBF90` semantics are INF).
4. Confirm `MA+0x2600C` equals the stall's "designated man" on a live/capture pair (needs a hook or capture read; not requested here).

## Round 2 (18-11): correction
# pes-re-walker-touch R2: correction — the EDX=7 admissions are DRIBBLE-tick, not MOVE-tick

Status: round-1 central claim RETRACTED and corrected. StallBreaker's check is CONFIRMED on every point I re-verified. No code changed. Scratch: `<workdrive>/gf-WalkerTouch-scratch/` (`verify_vas_r2.py` 58 checks ALL-PASS, mutation FAILs; `dis_*.txt`, `xrefs_*.txt`, sweep logs). Image `pes_full.bin`, flat mapping (control 0x14256AE1C=1.0f). Ghidra bridge for decompile/disassembly; whole-section E8/E9 byte scan + capstone boundary check + `.pdata` decode for call topology (the bridge xref index misses real call sites).

## 0. Constants (unchanged from R1, re-verified in R2 script)

100.0 (`0x14253EE30`), 1.0, 0.5, 360.0, −360.0, 0.71 (`0x1425EB848`), 25.0/85.0/157.5/1000.0 (ebf90 set), ~0.2174 (`0x1425A9BD4`), 38000.0, 3600.0, gs=54 (`0x14351AE50`=`0x36`). Operand-VA pairs as in R1.

## 1. Attribution: StallBreaker right, R1 wrong (CONFIRMED from the image)

Action vtables are 0xE0 blocks (+0x20 phase tick, +0x50/+0x70 handlers — pattern verified on the action-80 block `0x1425E28F0`: +0x20=`0x1407E7260`, +0x50=`0x1407EBD90`, +0x70=`0x1407E8D20`, matching StallBreaker §7.1):

- DRIBBLE base `0x1425E2440`: +0x20=`0x1407B37D0` (slot `0x1425E2460` ✓), +0x28=`0x1407B3650` ✓, +0x58=`0x1407A38E0` ✓, +0x68=`0x1407BA840` ✓, +0x70=`0x1407B9280` ✓ (matches TrapContact3 §21).
- MOVE base `0x1425E13F0`: +0x20=`0x1407824A0` (slot `0x1425E1410` ✓), +0x70=`0x140782570` (matches StallBreaker §195).
- 0x53 = DEMO_INPLAY_MOVE: its vtable `0x1425E2BB0`+0x68 reads `0x1407EB8A0` from the image (matches TrapContact4 §14). 0x52 is INF (likely sibling demo; not verified).

DATA-ref topology (image-wide qword scan): the only DATA refs to `FUN_1407B37D0`/`FUN_1407824A0` are their +0x20 slots; the whole dribble cluster (`FUN_1407B05F0/3470/2450`, `FUN_1407AA4E0`, `FUN_1407A7E40/8F60/D150`, …) has ZERO DATA refs. Whole-section E8 scan (capstone boundary-checked from `.pdata` starts): `FUN_1407B05F0`←1 site (`0x1407B39E9`), `FUN_1407B3470`←5 (`0x1407A7E9D/9177/B71F/D283`, `0x1407B2558`), `FUN_1407B2450`←`0x1407B3A21` (in `FUN_1407B37D0`), `FUN_1407AA4E0`←`0x1407B0DAA` (in `FUN_1407B05F0`); zero E9 tail-calls to any of them; both dispatchers have zero direct callers. So every path through `FUN_1407B05F0`/`FUN_1407B3470` runs on a DRIBBLE man (action 4). **R1's "MOVE-tick admission" is retracted: `0x1407B0D42`/`0x1407B35C9` (EDX=7) admit TRAP for a dribbler, committed by `FUN_1406EEBB0(7,0x5E)` at `0x1407B0D57`/`0x1407B35E6`.**

## 2. EDX census of `FUN_1406B4580`: 77 direct E8 sites, all boundary-verified (bridge listed 74; it misses `0x14072EEC4`, `0x1407822B6`, `0x140A08B37`; "76" superseded)

- EDX=7 (TRAP), 4 sites: `0x1407B0D42` + `0x1407B35C9` (dribble tick), `0x1409F878D` + `0x1409F880D` (reaction try/retry). The ONLY trap admissions in the image.
- EDX=0x5D, 3 sites: `0x1409F84F5` (reaction kick: height ≤~0.2174, rel-speed <38000/(gs·3600) — moving-ball branch), `0x1407C4839`, `0x1407D2D1C`.
- EDX=0x52, 2 static +1 dynamic: `0x1407824F3` (MOVE tick), `0x1407B38AD` (dribble tick); `0x14058A701` LEA dynamic.
- EDX=0x53, 3 static: `0x140782524` (MOVE tick), `0x1407B38DE` (dribble tick), `0x140789E23`.
- EDX=4 (dribble), 6: `0x140796C1B` (from trap tick `FUN_140797CD0`, falls back to MOVE 2), `0x14079F17D`, `0x1407D1216/3B0E/45E7/5307`.
- EDX=5: `0x1407B3026`. EDX=9: `0x1407822B6` (fn `0x140781FC0`, vtable `0x1425E1230`+0x10 — a third action, not MOVE/DRIBBLE). EDX=0x50: `0x14072EEC4` (fall-rise injury → demo-contact). EDX=0x51: `0x14071B9D2`, `0x140742372`, `0x140782DA2`. Remainder: pass/shot cluster (0x38/0x3A/0x3C/0x3D/0x39/0x3B), keeper/referee regions (0x15/0x48/0xE/0x14/0x28/0x1D/0x45/0xD/0x8/0x12), dynamics (EDI, [RSI+4], R12D×6, EBX×2, [R8+0x15] at `0x140A08B37`).

## 3. Corrected path for a designated MOVE/idle man at a resting ball

- **Non-holder** (`MA+0x2600C != me`): reaction entry 3 → `FUN_1409F8640` → EDX=7 (`0x1409F878D`/retry `0x1409F880D`), commit (+0x874=4/+0x878=7). Gates: C1 ≤100 (`0x1409FA480`→`0x14253EE30`, 10 m), TRAP-admissible (`0x1409FA550` CALL `0x1406B4C10`), keeper hold, in-play newest-event gates, retouch lockout. NO facing/speed/height gate on the TRAP branch. Then trap end → DRIBBLE via `0x140796C1B` (EDX=4) or MOVE fallback. So PES path: MOVE →(reaction)→ TRAP →(trap end)→ DRIBBLE.
- **Holder** (`MA+0x2600C == me`): reaction refuses (`0x1409FA57B` CMP/JZ); MOVE tick (`0x1407824A0`, body decompiled: tries 0x52, then 0x53, else `FUN_1406B8340` continuation) offers NO trap/dribble admission — none exists anywhere in the image for a MOVE man. He plays only after the holder changes, which is bookkeeping in `FUN_140457070` (K=8 carrier from ring-1 key, owner from ring-0 scan; someone else's touch changes it), not his own tick. Initial-holder (restart latch from `MI+0x1558`) is first-touch §4.2, not re-verified.
- R1's "facing ±10° / speed<Y on the MOVE-tick trap" conditions are withdrawn (no such path); the 0.71 kind threshold and +0x5C3/+0x35/+0x70 gates belong to the DRIBBLE-tick admission. OPEN as before: picker +0x68 target, `+0x5C3/+0x5CC/+0x5E4` writers, 0x52 identity, the `0x140785B38` E9→SWITCH site (no `.pdata` range, unverified), whether stall walkers are the 2600C-holder (needs capture/census work).

## 4. GF diff (corrected; origin/MergeBKBJ `abe9f81d`)

GF ported the holder refusal (`playercontroller.cpp:400` `RefusedAsDesignatedOwner`) but: (a) its only body-contact path demands walking away (`humanoid.cpp:584-597`: `ballDistanceFuture > ballDistanceNow`, `Movement`, `<0.6`, height `<1.6`) — PES's reaction TRAP branch has no approach-direction gate, so GF refuses arrivals PES admits (non-holder case); (b) `hasIt` (`elizacontroller.cpp:634`, `AI_HasPossession` 1.0 m band `AIfunctions.cpp:1089-1116`, no PES VA) suppresses even GF's step (`:639-659`, `!PesLoop()`-gated at `:643`) and the run needs `>0.5 m` (`:701`); (c) GF-mode designation gate (`playercontroller.cpp:416-418`). The holder-stands part matches PES (no MOVE-tick trap exists); GF's error is over-refusal — a 1.0 m `AI_HasPossession` band ≠ the PES 2600C-holder — plus the walk-away touch gate.

## 5. Red-test facts (corrected)

- **R-RED (fails on MergeBKBJ):** PesLoop, MOVE man who is NOT the designated owner, 0.5 m from a resting ball moving toward it → TRAP admitted via entry 3 (`0x1409F878D`) within 54 ticks (reaction runs every frame; 54 = 1 s @ gs=54, generous vs GF's 9-tick 0.5 m play). PES admits (C1/class/keeper/event/lockout pass; no direction gate); GF yields no Trap (humanoid walk-away conjunct + hasIt suppression).
- **R-NEG guard (passes on both; blocks admit-everyone fixes):** 2600C-holder MOVE man at 0.5 m → no TRAP within 54 ticks (reaction refuses `0x1409FA57B`; MOVE tick: only 0x52/0x53 demo admissions `0x1407824F3/524` + continuation).

## 6. Verification

`verify_vas_r2.py`: 58 checks ALL-PASS (flat control; 77/77 CALL targets; 14 EDX-setup byte asserts incl. `LEA EDX,[RCX+6]`→7 and `LEA R8D,[RDX+0x57]`=0x5E; 8 vtable-slot qword asserts incl. `0x1425E2BB0+0x68`=`0x1407EB8A0`; 13 float + gs asserts; 12 `.pdata` anchors; 7 DATA-topology asserts). `--mutate` (off-by-one CALL + wrong vtable value): FAILs as required (3 failures). Method: whole-section E8/E9 scan, capstone boundary validation from `.pdata` starts, `.pdata`-anchored cites, image qword reads for slots. The background capstone displacement sweep (10.5M insns) finished but yielded zero hits (operand-detail mismatch — unused, noted honestly).
