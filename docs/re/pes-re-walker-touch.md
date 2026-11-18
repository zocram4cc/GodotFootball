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
