# PES A4 team tick — decode tickets for the OPEN steps of `FUN_1404F6C30`

> **Main audit 19-10-26:** spot-checked by raw disassembly: `0x1404F6BC2` `movb [rax+0xe4],1` (step 5, T+0xE4); `0x1404F66AC/C5/E7` stores to T+0x6D0 (step 14, phase codes 4/5/computed); FUN_1404FB2A0 writes T+0x78 (step 40c; `movl [rax+0x78],0xff` resets at `0x1404FB2FA/34A/455`): all match. The agent's own checker reports 43 values + 77 operand pairs, 0 mismatches.
> **Main audit 19-10-26 (round 2):** the T+0xB6DE reader `cmpb $0,[r15+0xb6de]` at `0x140928CF6` (opponent player AI) matches. Round 1 claims corrected in §5: 40e only READS T+0xB6DC; T+0x8C35[] has 8+ readers.

Read-only RE report. No GF code changed, no PES written. Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`
(bridge decompiles + `dis_1404F6C30.txt`; floats read by script from `pes_full.bin`, base `0x140000000`).

Base `0x140000000`. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Tags: **CONFIRMED** = decompile and/or raw dis this pass; **INF** = inference; **OPEN** = blocker named.
Step numbers are `docs/re/pes-re-team-layer.md` §1's table (call-site VA column), re-derived from this
pass's own decompile of `FUN_1404F6C30` (`dec/FUN_1404F6C30.c`) — no new call sites were invented.

**Port contract format** (same as §3's slices): Inputs → Outputs (≤3 pieces), and a red-test sketch.
Each step's section also lists: callee VA(s), reads/writes (T/MA offsets), gate, constants
(script-read, with the operand VA pair), SIM vs non-SIM.

## §0 Float table (script-read from `pes_full.bin`; `repr` exact, bits exact)

| `VA` | `value` (`0xBITS`) | use |
|---|---|---|
| `0x14259BDF4` | `8.0` (`0x41000000`) | step 5 chase-distance term (`DAT_14259bdf4` → `FUN_140a4edf0` arg) |
| `0x142588210` | `10.0` (`0x41200000`) | step 5 lateral-gap term `DAT_142588210` |
| `0x142565604` | `2.0` (`0x40000000`) | step 5 second lateral term `DAT_142565604` |
| `0x14258C264` | `3.0` (`0x40400000`) | step 5 `fVar16` angular threshold (3.0) |
| `0x142596E6C` | `80.0` (`0x42A00000`) | step 5 `fVar13` angular cap (80.0) |
| `0x14259BE1C` | `20.0` (`0x41A00000`) | step 5 forward-side check `fVar15 < 20.0` |
| `0x14259BE30` | `45.0` (`0x42340000`) | step 5 angle gate `<= 45.0` |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | step 5 `>= 90.0` → trap arm |
| `0x14259BE60` | `135.0` (`0x43070000`) | step 5 cone half-angle for `FUN_140A4E720` |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | step 35 `fVar22` band lower edge |
| `0x14259BE0C` | `15.0` (`0x41700000`) | step 35 `fVar25` offset (subtract 15.0) |
| `0x142565600` | `0.5` (`0x3F000000`) | step 27d `fVar3*5.0 + 0.5` timer step |
| `0x14259222C` | `5.0` (`0x40A00000`) | step 27d timer step `5.0` |
| `0x1425C48E4` | `1369.0` (`0x44AB2000`) | step 35 `fVar8` distance² reject (`1369.0` = 37²) |
| `0x1425C48D0` | `105.0` (`0x42D20000`) | step 35 `fVar7` angle cap (105.0) |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | step 35/11 unit scale |
| `0x14259BE04` | `12.0` (`0x41400000`) | step 35 `uVar6` angle window (12.0) |
| `0x14259BE44` | `64.0` (`0x42800000`) | step 35 `FUN_140a4eaf0` reach arg (64.0) |
| `0x142588218` | `-1.0` (`0xBF800000`) | step 11 `fVar24` default (`-1.0`) |
| `0x14259BDD0` | `0.6944444179534912` (`0x3F31C71C`) | step 5 `DAT_14259bdd0 / fVar13` |
| `0x1425C48E8` | `9999.0` (`0x461C3C00`) | step 45 "no distance penalty" sentinel |
| `0x142572348` | `0.800000011920929` (`0x3F4CCCCD`) | step 46 `brain+0x6C54` re-randomisation scale |
| `0x14259CE3C` | `7.5` (`0x40F00000`) | step 46 |
| `0x14259D60C` | `9.0` (`0x41100000`) | step 46 / step 40f |
| `0x14259BE28` | `32.0` (`0x42000000`) | step 47 lateral window |
| `0x1425A2670` | `400.0` (`0x43C80000`) | step 47 "defender much closer" distance² |
| `0x1425A9460` | `29.0` (`0x41E80000`) | step 47 lower gate of the 29–32 m window |
| `0x14259BDDC` | `1.5` (`0x3FC00000`) | step 49 |
| `0x14259BE4C` | `70.0` (`0x428C0000`) | step 40h / 40f |
| `0x14259BE7C` | `-5.0` (`0xC0A00000`) | step 40f |
| `0x1425C48C4` | `24.0` (`0x41C00000`) | step 40f |
| `0x1425C48EC` | `-0.25` (`0xBE800000`) | step 40f |
| `0x14256AE30` | `120.0` (`0x42F00000`) | step 40h |
| `0x1425C48B4` | `8.5` (`0x41080000`) | step 40h |
| `0x1425A1FF4` | `4.5` (`0x40900000`) | step 40h |
| `0x1425A2828` | `6.5` (`0x40D00000`) | step 40h |
| `0x1425A9BF0` | `130.0` (`0x43020000`) | step 40h |
| `0x1425A9680` | `-9.0` (`0xC1100000`) | step 40h |
| `0x1425C48A0` | `0.15000000596046448` (`0x3E19999A`) | step 49 (`MI+0x116C==3` arm) |
| `0x1425C6B00` | `0.3799999952316284` (`0x3EC28F5C`) | step 49 (`MI+0x116C==3` arm) |
| `0x14259BDFC` | `11.0` (`0x41300000`) | step 40f |
| `0x14259D608` | `7.839999675750732` (`0x40FAE147`) | step 40f |
| `0x1425A200C` | `-6.0` (`0xC0C00000`) | step 40f |

## §1 Summary table

Status: **DECODED** = body read, gate + writes known → port contract ready;
**PARTIAL** = body read but a sub-branch or meaning is OPEN; **OPEN** = not yet read.
"Contract" = a port contract is proposed (≤3 pieces) in that step's section.

| step | callee VA(s) | SIM? | status | contract ready? | writes of note |
|---|---|---|---|---|---|
| 1 | `0x1404F6CC5` → `FUN_1423E7FD0` (skip), `FUN_141E5D4C0` | SIM (skip flag) | DECODED | yes | stack skip flag |
| 2 | `0x1404F6CE9` → `FUN_140A0C3E0` | SIM (vtable fan-out) | DECODED | yes | brain event list drain |
| 5 | `0x1404F6D25` → `FUN_1404F6720` | SIM | DECODED | yes | **`T+0xE4` trap flag** |
| 6 | `0x1404F6D2D` → `FUN_1404F7AD0` | SIM | DECODED | yes | `brain+0x6C5C`, `T+0xB8A8`, `T+0xB8AC` |
| 8 | `0x1404F6D55` → `FUN_1405008E0` | non-SIM (roster/UI) | DECODED | n/a | `brain+0x6C40` |
| 9 | `0x1404F6D5D` → `FUN_140502190` | SIM (timers) | DECODED | yes | `T+0xB860+`, `brain+0x6A4` counters |
| 11 | `0x1404F6DAA` → `FUN_1404FEDC0` | SIM | DECODED | yes | **`T+0x218`**, `brain+0x6CF4`, `brain+0x6CC4..6CE0` |
| 14 | `0x1404F6DFD` → `FUN_1404F6680` | SIM | DECODED | yes | **`T+0x6D0` phase code** |
| 15 | `0x1404F6E05` → `FUN_1404FA840` | SIM | DECODED | yes | **`T+0xB8A8`, `T+0xB8AC`**, `brain+0x6D44` |
| 16 | `0x1404F6E0D` → `FUN_1404FA720` | non-SIM (technique/UI) | DECODED | n/a | `T+0xB8B0` (technique id) |
| 17 | `0x1404F6E15` → `FUN_1404FBCA0` | SIM | DECODED | yes | **`T+0xB8B4`** (build-up mode) |
| 19 | `0x1404F6E67` → `FUN_140500200` | SIM | DECODED | yes | **`T+0xB8B8`** (11-bit slot mask) |
| 20 | `0x1404F6E6F` → `FUN_1404FF8B0` | SIM | DECODED | yes | **`T+0x8C35[]`, `T+0x8C40[]`, `T+0x8D50[]`** |
| 21 | `0x1404F6E77` → `FUN_1404F7BA0` | SIM | DECODED | yes | **`T+0xB8D0`**, `brain+0x6D40` |
| 22 | `0x1404F6E7F` → `FUN_1404FD4B0` | SIM | DECODED | yes | `brain+0x6DD8/0x6DF0` |
| 23 | `0x1404F6E9D` → `FUN_14053B860` | SIM | DECODED | yes | `brain+0x6C08` ctx |
| 24 | `0x1404F6EA5` → `FUN_1404F87D0` | SIM | DECODED | yes | `T+0xB8BC` |
| 25 | `0x1404F6EAD` → `FUN_140500B90` | SIM | DECODED | yes | `brain+0x6D06/08/0C`, `T+0x1FC` |
| 27a-pre | `0x1404F6F33` → `FUN_1404FBB70` | SIM | DECODED | yes | `T+0x8984/87/8A/8D` role counts |
| 29 | `0x1404F6FBE` → `FUN_140537AC0` | SIM | DECODED | yes | `T+0xA2A0` (0x1E), `T+0x130` |
| 30 | `0x1404F6FDA` → 11× `FUN_140A40240` | SIM | DECODED | yes | stack scratch clear |
| 31 | inline stack snapshot | SIM | DECODED | yes | stack copy of `T` run-state blocks |
| 32 | `0x1404F7643` → `FUN_140A84E30` | SIM | DECODED | yes | `T+0x6F4` ring reset |
| 35 | `0x1404F76B1` → `FUN_1404F61E0` | SIM | DECODED | yes | `T+0xB7EC[11]`, `T+0xB7F4/6` |
| 36 | `0x1404F76BE` → `FUN_14057D240` | SIM | DECODED | yes | `T+0xA2A0` (0x6D), `T+0x88D8` |
| 40 | `0x1404F7824`–`876` → 8 fns | SIM | DECODED | yes | **`T+0xBAB4`** (40f cost table), **`T+0xC128`** (40h mask), shared block (40e) |
| 45 | `0x1404F796D` → `FUN_1404F88D0` | SIM | DECODED | yes | `T+0x64C[i]`, `T+0x678[slot]` |
| 46-body | `0x1404F7975` → `FUN_1404FC720` | SIM | DECODED | yes | see section |
| 47-meaning | `0x1404F7980` → `FUN_140509170` | SIM | DECODED | yes | `T+0xB6DE` — **"this side is contesting the ball"** (§5.6) |
| 48-bodies | `0x1404F7995`–`9AD` → 4 fns | SIM | DECODED | yes | see section |
| 49 | `0x1404F79D8` → `FUN_140539ED0` | SIM | DECODED | yes | `brain+0x6C18` ctx |
| 50-stubs | `0x1404F79DF`–`A0E` → `FUN_140421F40`×2 + 4 caseD | non-SIM (no-ops) | DECODED | n/a | none |
| 52 | `0x1404F7A3E` → `FUN_14053C6E0` | SIM | DECODED | yes | `brain+0x6D28+0` (ball-side), `+4` (tactic 0..3), `+8` (zone class 0..5), `+0x20` (class max), `+0x28..0x40` (histogram), `+0x0C` (5 picks), `+0x48` (event count), `T+0x40/0x44` (taker) — **§5.1** |
| 53 | `0x1404F7A5C` → `FUN_140A408C0` | SIM | DECODED | yes | `brain+0xBB8+0x3F08[]` (slot targets) |
| 55 | `0x1404F7A85` → `FUN_140A42A70` | SIM | DECODED | yes | see section |

---

## §2 Per-step sections

### Step 1 — `0x1404F6CC5` (skip flag)

**Callee VAs.** `FUN_1423E7FD0` (reads `*(param_1+0x9C)`, a byte) and `FUN_141E5D4C0` (called earlier at `0x1404F6CAA`, `brain+0x62B4`, arg `0xB7`).

**Reads.** `DAT_1436F3DC8+0x3B0` (global "extra object" pointer). `FUN_1423E7FD0` is a one-byte getter at `+0x9C` of that object.

**Writes.** Only the stack flag `local_398` (skip). No T/MA write.

**Gate.** `if (*(G+0x3B0) != 0)` → `skip = FUN_1423E7FD0() ? 1 : 0`; else `skip = 0`. CONFIRMED (`dec/FUN_1404F6C30.c:164-171`).

**Constants.** none.

**SIM vs non-SIM.** SIM (the flag gates the whole per-player concept block below).

**Port contract.**
- Inputs: GF "match in replay/paused" boolean (stand-in for `G+0x3B0` object byte `+0x9C`).
- Outputs: `skip` boolean.
- Pieces: (1) the getter + flag; (2) the `skip==0` gate on the concept block; (3) the `skip==0` gate on the run manager.
- **The exact skip extent (CONFIRMED raw dis).** The gate is at `0x1404F6FBE CMP byte ptr [RSP+0x30],0x0 / 0x1404F6FC3 JNZ 0x1404F76B6` — the jump target `0x1404F76B6` is the instruction **immediately before** step 36's call (`0x1404F76B9 CALL 0x14057D240`). So `skip != 0` skips **steps 30, 31, 32, 33, 34 and 35** — the stack snapshot, the action-ring reset, the concept selector (`FUN_140579840` at `0x1404F7673`), the concept executor (`FUN_140579BF0` at `0x1404F7698`) and the opposition finder (`FUN_1404F61E0` at `0x1404F76AC`). The second use of the same flag is the run-manager gate before step 41 (`0x1404F78BA`), which is inside the same `skip == 0` block.
- Red-test sketch: `TeamTick.SkipSkipsConceptBlockAndOppositionFinder` — with the skip flag set, the concept selector, the concept executor and the opposition finder do not run; with it clear they do.
- Stand-in naming: `// ponytail: GF replay/pause flag replaces PES G+0x3B0 object byte +0x9C (FUN_1423E7FD0).`

### Step 2 — `0x1404F6CE9` → `FUN_140A0C3E0`

**Callee VA.** `FUN_140A0C3E0(brain, match+0xE0)`.

**Reads.** `*(brain+0x6D48)+0xE0` (the "event list" object).

**Writes.** None directly. It calls `(*(*param_1+8))(param_1, uVar1)` — a **vtable slot 8** on `brain` — once per element, in reverse index order (`iVar2-1 … 0`).

**Gate.** `param_2 != 0`; loop count `FUN_140A3E8E0(param_2)`; element `FUN_140A3E570(param_2, count-1-i)`.

**Constants.** `0xB` (elems per group) not used here; the count comes from the callee.

**SIM vs non-SIM.** SIM (drains a pending event list into the brain's vtable handlers). The individual vtable handlers are OPEN.

**Port contract.**
- Inputs: GF pending-event queue (stand-in for `match+0xE0` list).
- Outputs: brain state after the queue drains.
- Pieces: (1) reverse-order drain; (2) per-element handler call.
- Red-test sketch: `TeamTick.EventQueueDrainedInReverseOrder` — with three queued items, the brain handler observes them in reverse index order.
- Stand-in naming: `// ponytail: GF event queue replaces PES match+0xE0 list drained by FUN_140A0C3E0.`

### Step 5 — `0x1404F6D25` → `FUN_1404F6720` (offside-trap arm)

**Callee VA.** `FUN_1404F6720(brain)`.

**Reads.** `T+0x254` (side sign), `T+0x8E08` (formation sheet), `T+0x55C`/`+0x564` (player x/z), `T+0x250` (player angle), `T+0x78` (pass target), `pl+0x5D0` (AI action), `MI+0x116C` (match phase), `G+0x2AA8+0x18` (ball x), `G+0x180` roster, `pl+0x13A8/+0x13AC/+0x13C8` (player traits), `T+0x250`/`+0x254` of the pass receiver.

**Writes.** **`T+0xE4 = 1`** (the offside-trap flag) at `0x1404F6BC2` — the only write. CONFIRMED raw dis (`dis_1404F6720.txt:248`).

**Gate.** A five-way AND: `FUN_140CA1A50(0x12) != 0` (always 1, `FUN_140CA1A50` returns 1) **AND** `FUN_1408C5000(T+0x8E08) != 0` **AND** `MI+0x116C == 1` **AND** `FUN_140A61CF0(match, side) == 0` **AND** the ball carrier index `< 0x16`; then the carrier is not this team's own designated pass receiver; then the chase-distance and lateral-gap tests; then the angular tests below.

**Constants (script-read, operand VA pairs).**
- `0x1404F6853 MOVSS XMM0,dword ptr [0x14259BDF4]` = `8.0` — chase-distance term.
- `0x1404F685D MOVSS XMM3,dword ptr [0x14259BE60]` = `135.0` (`0x43070000`) — the cone half-angle passed to `FUN_140A4E720` (with `XMM2 = T+0x250`, the carrier's facing angle).
- `0x1404F68AF MOVSS XMM0,dword ptr [0x142588210]` = `10.0` — lateral-gap term.
- `0x1404F68F1 MOVSS XMM0,dword ptr [0x142565604]` = `2.0` — second lateral term.
- `0x1404F693B MOVSS XMM9,dword ptr [0x14258C264]` = `3.0` — `fVar16` angle threshold.
- `0x1404F699C MOVSS XMM1,dword ptr [0x14259BDD0]` = `0.6944444179534912` — `DAT_14259bdd0` divider.
- `0x1404F6A45 COMISS XMM13,dword ptr [0x14259BE1C]` = `20.0` — forward-side check.
- `0x1404F6A5B COMISS XMM3,dword ptr [0x1425656C0]` = `4.0` — second forward-side check.
- `0x1404F6A98 COMISS XMM0,dword ptr [0x14259BE30]` = `45.0` — angle gate `<= 45.0`.
- `0x1404F6AB1 MOVSS XMM8,dword ptr [0x142596E6C]` = `80.0` — angular cap.
- `0x1404F6B71 MOVSS XMM0,dword ptr [0x14259BE5C]` = `90.0` — `>= 90.0` → **trap arm**.
- `0x1404F6B85 MOVSS XMM3,dword ptr [0x14259BE1C]` = `20.0` — second 90° arm test.

**SIM vs non-SIM.** SIM (it arms `T+0xE4`, read by step 48's `FUN_14057BA50`).

**Port contract.**
- Inputs: ball carrier id, ball x/z, carrier x/z and facing angle, team pass target (`T+0x78`), team attack sign (`T+0x254`), match phase.
- Outputs: `T+0xE4` (trap-armed byte).
- Pieces: (1) the eligibility AND-chain (formation non-empty, phase 1, carrier on pitch); (2) the angle test (`<= 45.0` gate, `>= 90.0` arms); (3) the `FUN_140A4E720` re-check that keeps the flag clear.
- Red-test sketch: `TeamTick.ArmOffsideTrapWhenCarrierFacesAway` — a carrier whose facing differs from the goal line by ≥ 90° sets `trapArmed`; a carrier facing within 45° does not.
- Stand-in naming: `// ponytail: GF formation-sheet predicate replaces PES FUN_1408C5000(T+0x8E08).`

### Step 6 — `0x1404F6D2D` → `FUN_1404F7AD0`

**Callee VA.** `FUN_1404F7AD0(brain)`.

**Reads.** For slot `i` in 0..10: player id `side*0xB + i`; `pl+0x13A8` (trait byte), `pl+0x13AC` (int), `pl+0x13C8` (byte); `T+0xB8AC`.

**Writes.** `brain+0x6C5C = player id` (the "selected" player); `T+0xB8AC = MI+0x1198 & 3` and `T+0xB8A8 = 8` when the trait byte `+0x13A8` is set, `+0x13AC == 0x16`, `+0x13C8 != 0` and `T+0xB8AC == 4`. CONFIRMED decompile (`dec/FUN_1404F7AD0.c`).

**Gate.** `uVar2 < 0x16` (player id in range).

**Constants.** `0x16` (22) player count; `0xB` (11) slots per side; `4` for `T+0xB8AC`; `8` for `T+0xB8A8`.

**SIM vs non-SIM.** SIM (it sets the shape-dispatch mode `T+0xB8A8`/`T+0xB8AC` that step 40's `FUN_14053B110` branches on).

**Port contract.**
- Inputs: per-player trait triple (`+0x13A8`, `+0x13AC`, `+0x13C8`) for the 11 on-pitch slots; frame counter.
- Outputs: `brain+0x6C5C` (selected player id); `T+0xB8A8` (mode 8) and `T+0xB8AC` (frame&3) on the trait match.
- Pieces: (1) the per-slot trait scan; (2) the `+0x13AC==0x16 && +0x13C8!=0 && T+0xB8AC==4` gate; (3) the `T+0xB8A8=8 / T+0xB8AC=frame&3` write.
- Red-test sketch: `TeamTick.TraitPlayerSetsShapeDispatchMode` — a slot with the trait triple and `T+0xB8AC==4` sets `b8a8==8` and `b8ac==frame&3`.

### Step 8 — `0x1404F6D55` → `FUN_1405008E0`

**Callee VA.** `FUN_1405008E0(brain)`.

**Reads.** `T+0x255` (latch), `MI+0x116C`, `MI+0x1554`, `G+0x180` (roster, 0x18 entries of 0x74), `MI+0x1748` (on-pitch flag), `match+0x68` (an event sink).

**Writes.** `brain+0x6C40 = 2`; then per roster entry it calls `FUN_140A66FF0(match+0x68, rosterIndex)` — a vtable/event call.

**Gate.** `T+0x255 == 0` **AND** `MI+0x116C != 1` **AND** `MI+0x1554 == 5`.

**Constants.** `0x18` (24 roster slots), `0x74` (roster record stride), `0x16` (22), `0x17` (23) clamp.

**SIM vs non-SIM.** Mostly **non-SIM**: it fires match-level events for roster players (substitution / lineup events), not simulation state. Skip with one line: *"Step 8 is a roster/lineup event fan-out under `MI+0x1554==5` (match-setup phase); it writes no simulation state, so it is out of the SIM port scope."*

**Port contract.** none (non-SIM).

### Step 9 — `0x1404F6D5D` → `FUN_140502190`

**Callee VA.** `FUN_140502190(brain)`.

**Reads.** `T+0x255` (latch), `match+0xE0` (`FUN_140A3E190`), `match+0x68`, `G+0x40+0x291C`, `pl+0x55C`/`+0x564`/`+0x578`/`+0x560`/`+0x574`/`+0x57C` (player position/velocity), `G+0x2AA8`.

**Writes.** `T+0xB860 + 4*i` (11 ints) cleared or decremented; `brain+0x6A4 + 4*i` (11 ints) decremented; `brain+0x6C40` (set to 1 in one branch); `pl+0x13A8`-ish byte via `FUN_141E5D4C0(brain+0x62B4, 0xF)+0x38` (a per-player "has instruction" byte).

**Gate.** The inner block only runs when `FUN_141E5D4C0(brain+0x62B4, 0xF)+0x38 != 0`; then it branches on `G+0x40+0x291C == 3` (decrement the `T+0xB860` bank) vs else (decrement the `brain+0x6A4` bank). If the byte is clear, both banks are zeroed.

**Constants.** `0xB` (11), `0x2AA8` bank, `DAT_142588200` = `0.1` (`0x1404F7B2E`-style site), `DAT_142565600` = `0.5`.

**SIM vs non-SIM.** SIM (timers that gate later per-player work).

**Port contract.**
- Inputs: per-slot instruction bank state; latch.
- Outputs: the two 11-int countdown banks (`T+0xB860`, `brain+0x6A4`).
- Pieces: (1) the `+0x38` byte gate; (2) the decrement-vs-zero branch on `G+0x40+0x291C==3`; (3) the "clear when the byte is unset" branch.
- Red-test sketch: `TeamTick.InstructionTimersDecrementThenClear` — with the instruction byte set the banks count down; with it clear they are zeroed in one tick.

### Step 11 — `0x1404F6DAA` → `FUN_1404FEDC0`

**Callee VA.** `FUN_1404FEDC0(brain, latchByte, latchHi)`.

**Reads.** `T+0x255`, `T+0x224..0x240` (8 floats), `T+0x87D0 + 8*i` (11 ints), `match+0x18`, `MI+0x116C`, `MI+0x1198`.

**Writes.** **`T+0x218`** — incremented when the high latch byte is 0, else zeroed; `brain+0x6CF4` (frame stamp, `-1` on the reset branch, `MI+0x1198` on the other); `brain+0x6CC4..0x6CE0` (8 floats = `T+0x224..0x240` scaled by `fVar24`); `brain+0x6CE4`/`0x6CEC` (more floats); `T+0x87D0`-driven per-slot writes via `FUN_140A42BA0`.

**Gate.** `param_3` (the high byte of the latch) selects the branch: `param_3 == 0` → `T+0x218++`; else `T+0x218 = 0`, `brain+0x6CF4 = -1`, and the float bank is copied. Inside the copy, `MI+0x116C == 1` takes the in-play branch; otherwise a per-slot decay loop runs with `FUN_140A62180(match, side, 0)`.

**Constants.**
- `0x1404FEDC0` `DAT_14256AE1C` = `1.0` (`0x3F800000`) — the scale factor when the latch is set.
- `0x1404FEDC0` `DAT_142588218` = `-1.0` (`0xBF800000`) — the scale when the latch is clear.
- `0x1404FEDC0` `DAT_14253EE30` = `100.0` (`0x42C80000`) — the normaliser.
- `0x1404FEDC0` `DAT_142565600` = `0.5` (`0x3F000000`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: latch (possession byte + high byte), the 8 `T+0x224..0x240` floats, the 11 `T+0x87D0` ints.
- Outputs: `T+0x218` (the "no-ball" counter that step 43 tests), `brain+0x6CF4` (frame stamp), the 8-float scaled bank.
- Pieces: (1) the `T+0x218` increment/reset branch; (2) the latch-scaled copy of the 8 floats; (3) the per-slot decay (`FUN_140A42BA0`) in the non-in-play branch.
- Red-test sketch: `TeamTick.NoBallCounterRisesWhileLatchingAndResetsOnPossession` — `T+0x218` increments while the high latch byte is 0 and is zeroed on the tick the high byte becomes non-zero.

### Step 14 — `0x1404F6DFD` → `FUN_1404F6680` (phase code)

**Callee VA.** `FUN_1404F6680(brain)`.

**Reads.** `T+0xB8A4` (the attack level byte, read as a byte at `0x1404F6698 MOVZX ECX,byte ptr [RAX+0xB8A4]`).

**Writes.** **`T+0x6D0`** — `4` if level 0, `5` if level 1, `6` if level ≥ 2. CONFIRMED raw dis (`dis_1404F6680.txt`).

**Gate.** The tick calls this only when `MI+0x1558 == brain+8` (side owns the phase) **AND** `MI+0x1554 - 4 <= 1` (phase code 4 or 5). CONFIRMED raw dis (`dis_1404F6C30.txt:88-93`).

**Constants.** `0x4`, `0x5`, `0x6` (the three phase codes); `0x4`/`0x1` on the phase-code comparison.

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: attack level `T+0xB8A4` (GF stand-in: `pes::AttackLevel`).
- Outputs: `T+0x6D0` phase code ∈ {4,5,6}.
- Pieces: (1) the level→code map; (2) the tick-side phase gate (`MI+0x1554 ∈ {4,5}` and `MI+0x1558 == side`).
- Red-test sketch: `TeamTick.PhaseCodeFromAttackLevel` — level 0 → 4, level 1 → 5, level ≥ 2 → 6.
- Note: `T+0x6D0` is a **new** field — no GF counterpart today.

### Step 15 — `0x1404F6E05` → `FUN_1404FA840` (shape-dispatch mode)

**Callee VA.** `FUN_1404FA840(brain)`.

**Reads.** `MI+0x116C`, `MI+0x1558` (side), `MI+0x173C`, `G+0x180` (roster), `T+0xB8A8`/`T+0xB8AC`, `match+0xE0` (via `FUN_140A533A0`).

**Writes.** **`T+0xB8A8`** and **`T+0xB8AC`** (the shape-dispatch mode pair) and **`brain+0x6D44`** (a byte).

**Gate.** Three arms:
1. `MI+0x116C == 1` (in play) → `T+0xB8A8 = 8`, `T+0xB8AC = 4`, `brain+0x6D44 = 0`, **return**.
2. Otherwise: if `T+0xB8AC == 4` and `T+0xB8A8 != 8` → `T+0xB8AC = 4` (keep); else `T+0xB8A8 = 8`. Then a roster scan (`MI+0x173C <= 0x15`, `MI+0x1558 == side`, `FUN_140A55EF0(roster)`) which may early-return.
3. `MI+0x116C == 2` arm: `MI+0x1558 == side`, `FUN_140A533A0(match, 0) != 0`, then the **frame-digit map** on `MI+0x1198`: digit 1 → `uVar11 = 7`, digit 2 → `6`, digit 3 → `5`, digit 4 → `2`, else `8`; writes `T+0xB8A8 = uVar11`, `brain+0x6D44 = 1`.

**Constants.** `8`, `4` (mode pair); `0x17`/`0x18` roster bounds; the frame-digit map (1/2/3/4 → 7/6/5/2, else 8).

**SIM vs non-SIM.** SIM (mode pair is read by step 40's `FUN_14053B110`).

**Port contract.**
- Inputs: match phase (`MI+0x116C`), side, frame counter.
- Outputs: `T+0xB8A8` (mode), `T+0xB8AC` (mode), `brain+0x6D44` (phase-digit flag).
- Pieces: (1) the in-play arm (`8`/`4`/flag 0); (2) the `MI+0x116C==2` frame-digit arm; (3) the sticky-mode carry when `T+0xB8AC==4`.
- Red-test sketch: `TeamTick.InPlayAlwaysUsesModeEightFour` — with `MI+0x116C==1` the mode pair is (8,4) and the digit flag is 0 regardless of the frame counter.

### Step 16 — `0x1404F6E0D` → `FUN_1404FA720` (technique/coach flag)

**Callee VA.** `FUN_1404FA720(brain)`.

**Reads.** `MI+0x116C == 3` and `MI+0x1554 ∈ {4,5}`; `G+0x180` (roster); `FUN_140A55CF0(roster, side)`; `match+0x68`; `roster+0x70` (a technique byte per player).

**Writes.** **`T+0xB8B0 = FUN_140A65580(uVar4)`** (a coach/technique id); and a bit-set on `*(brain+0x6DA8)+4` (`|= 0x8000000`) plus a `FUN_1404457C0(brain+0x6DA8)` refresh when the roster slot matches `FUN_14044E200()`.

**Gate.** `MI+0x116C == 3 && (MI+0x1554 - 4) < 2`; `FUN_140A55CF0(roster, side) != 0`; `FUN_140A67010(match+0x68, side, roster, ...) != 0`; the technique byte `(roster+0x70) < 0xF8 && (byte & 0x7F) != 0`; and the inner byte `& 0x7F == 0x6E`.

**Constants.** `0x6E` (technique class), `0xF8`/`0x7F` (byte masks), `0x8000000` (the bit set), `0x18` (a struct size), `0x17` (roster clamp).

**SIM vs non-SIM.** SIM-ish but it drives a **coach-technique / UI event** (the `brain+0x6DA8` object is a technique-selection widget). Classify: **non-SIM (presentation)** — it writes a technique id and a UI refresh bit, no simulation state.

**Port contract.** none (non-SIM). One line: *"Step 16 selects a coach technique id from the roster and pokes a UI widget; no simulation state changes."*

### Step 17 — `0x1404F6E15` → `FUN_1404FBCA0` (build-up / play mode)

**Callee VA.** `FUN_1404FBCA0(brain)`.

**Reads.** `MI+0x116C`, `MI+0x1554 == 5`, `MI+0x173C` (an index), `G+0x180` roster, `brain+0x8` (side), `roster+0x70` (technique byte), `roster+0x10`.

**Writes.** **`T+0xB8B4`** (the build-up / play-mode int; set to the technique id, or `7` when the technique already equals it), and `brain+0x6D44 = 1`; plus the `|= 0x4000000` bit on `*(brain+0x6DA8)+4` and a `FUN_1404457C0` refresh.

**Gate.** `MI+0x116C == 1` → **return** (no write). `MI+0x1554 != 5` → return. `MI+0x173C > 0x15` → return. The owning side (`(MI+0x173C > 10 ? 1 : 0)`) must equal `brain+8`. Then the roster technique byte `(byte & 0x7F) == 0x6F` and `FUN_140A655A0(uVar4) <= 6`.

**Constants.** `0x6F` (technique class), `0x6` (mode cap), `7` (the "already in that mode" fallback), `0x4000000` (bit), `0x17`/`0x18` (roster bounds), `0xB` (11).

**SIM vs non-SIM.** SIM (`T+0xB8B4` is the "brain mode" that step 42's `FUN_140538050` reads as mode 1..6).

**Port contract.**
- Inputs: match phase, side, the technique roster table.
- Outputs: `T+0xB8B4` (mode int, 1..7), `brain+0x6D44 = 1`.
- Pieces: (1) the `MI+0x116C==1` early-out; (2) the side-ownership gate; (3) the technique→mode map with the "already set → 7" rule.
- Red-test sketch: `TeamTick.BuildUpModeStickyWhenTechniqueUnchanged` — with the technique id already equal to `T+0xB8B4`, the mode becomes 7 rather than the technique id.

### Step 19 — `0x1404F6E67` → `FUN_140500200` (per-slot mask by attack level)

**Callee VA.** `FUN_140500200(brain)`.

**Reads.** `T+0xB8A4` (attack level), `match+0x18` (the MA nearest-list base), `T+0x4C00 + 4*slot` (a per-slot counter), `T+0x254` (side sign), `G+0x2AA8+0x18` (ball x), `pl+0x55C`, `pl+0x250`, `pl+0x564`, `match+0x18+0x2482C` (ball pos).

**Writes.** **`T+0xB8B8`** — an 11-bit mask (one bit per on-pitch slot) of the slots to keep/press. Written at two sites: the "level<4 and the level is 1..3 and `FUN_1405AAF00`" branch (mask built from `T+0x4C00[slot] > 1`), and the "level < 4 else" branch (mask built from the MA list walk + the level→licence count map).

**Gate.** `T+0xB8A4 >= 4` → the mask is **all 11 bits set** and the function returns immediately (`0x140500240` BTS loop). Otherwise: if `0 < level` and `FUN_1405AAF00(match, side)` → the `T+0x4C00` mask branch; else the licence-count branch.

**Constants (script-read pairs).**
- `0x1405004F0 COMISS XMM0,dword ptr [0x14259BE0C]` = `15.0` — the "close to the ball" angle gate.
- `0x14050059B MOVSS XMM0,dword ptr [0x142588210]` = `10.0`.
- `0x1405005AE MOVSS XMM0,dword ptr [0x14259BE20]` = `25.0`.
- `0x1405005B9 MOVSS XMM0,dword ptr [0x14259D668]` = `-15.0` (`0xC1700000`).
- `0x1405005C2 MOVSS XMM0,dword ptr [0x14259222C]` = `5.0`.
- `0x1405005CB MOVSS XMM0,dword ptr [0x14259D654]` = `-3.0` (`0xC0400000`).
- `0x140500470 MOVSS XMM0,dword ptr [0x142588218]` = `-1.0`; `0x14050047D` `[0x14256AE1C]` = `1.0`.

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: attack level, per-slot counters (`T+0x4C00`), ball x, MA list.
- Outputs: `T+0xB8B8` (11-bit slot mask).
- Pieces: (1) the `level >= 4` all-ones mask; (2) the `0 < level < 4 && FUN_1405AAF00` counter branch; (3) the level→licence-count branch (level 0 → 0, 1 → 2, 2 → 4, 3 → 6, else 5).
- Red-test sketch: `TeamTick.LevelFourMasksEverySlot` — at level 4 the mask has all 11 bits set; at level 0 the licence branch yields no slots.

### Step 20 — `0x1404F6E6F` → `FUN_1404FF8B0` (pass-target / receiver choice)

**Callee VA.** `FUN_1404FF8B0(brain)`.

**Reads.** `T+0x255` (latch), `T+0x78` (pass target), `pl+0x55C`/`+0x564` (positions), `pl+0x560`/`+0x574`/`+0x578`/`+0x57C` (velocity), `MI+0x116C`, `match+0x18+0x38+0x14*side` (pitch bounds), `pl+0x5D0` (AI action).

**Writes (CONFIRMED raw dis).** Three arrays on `T`:
- **`T+0x8C35 + slot`** (11 bytes) — a per-slot "found a target" flag (`0x140500036 MOV byte ptr [RAX+R12*0x1+0x8C35],DL`).
  **Round 2 (§5.5): this flag has 8+ readers** — `0x1404F8055` (step 21's own `FUN_1404F7BA0`),
  `0x14052C061`, `0x14054C8C2`, `0x1405574DD`, `0x1405603B4`, `0x1405632A9`, `0x140563DDD`,
  `0x140570295`, `0x140957F68`, `0x140959A83`, `0x140A1E57B`, `0x140A3060E` — all of the form
  `CMP byte ptr [T + slot + 0x8C35], 0`. The base is confirmed as the team record at
  `0x14052C054 CALL 0x140A48A30` → `RCX`, then `0x14052C061 CMP byte ptr [RCX+RAX*0x1+0x8C35],BPL`.
- **`T+0x8C40 + slot*4`** (11 ints) — the chosen target player id (`0x14050003E MOV dword ptr [RAX+R12*0x4+0x8C40],ECX`); the "not found" value is `0xFF` (`0x1405001E7 MOV dword ptr [RAX+R12*0x4+0x8C40],0xff`).
  **Round 2 (§5.5): this field has NO reader in the team tick.** The only other `0x8C40` site in the
  image, `0x1408A7F10`, writes a *pointer* (`RAX = 0x142599B18`) and its base `RBX` is the function's
  own argument, not the team record — a different object that happens to have a `+0x8C40` field.
- **`T+0x8D50 + slot*4`** (11 floats) — a 0..100 cost/score for that slot (`0x1405001BE MOVSS dword ptr [RAX+R12*0x4+0x8D50],XMM13`).
  **Round 2 (§5.5): this field has NO reader anywhere in the executable image** — a disp32 scan of the
  `.trace` section and an independent full linear-disassembly scan both return exactly one site, the
  writer. It is a write-only diagnostic; the "units" question is moot. Its range is **[0, 100]**
  (the `MINSS` clamp at `0x1404FFE26` against the `100.0` loaded at `0x1404FFB8D`).

The decompile missed these because the base register is `RAX = [R13+0x6D50]` loaded inside the loop. CONFIRMED raw dis — **this closes the "no writes" reading of step 20**.

**The per-slot algorithm (CONFIRMED raw dis).** For `slot` = 0..10 (`R12`, `0x1404FFA40`–`0x1405000AE`):
1. the candidate id is `side*0xB + i` (`0x1404FFA40 IMUL R15D,dword ptr [R13+0x8],0xb`);
2. if the candidate is off-pitch (`MI+0x1748` byte) → skip (`0x1404FFA4C JA 0x1405001CD`, which writes the "not found" values);
3. otherwise set the local flag byte `[RBP+0xB0] = 1` and compute an interpolated position with `rand*0.1 + 0.5` (`0x1404FFA70 CALL 0x141E532D0`, `0x1404FFA75 MULSS XMM0,[0x142588200]` = 0.1);
4. scan the 24 roster records (`0x74` stride) for one whose `+0x20` equals the candidate id and whose `+0xD` byte is clear (`0x14050006D`/`0x140500163`) — this is the "the candidate is in the roster and not substituted" test;
5. the angle `XMM13` is then clamped into `[-1, 1]`-ish bands: `0.5` vs `XMM13` (`0x14050017F COMISS XMM0,XMM13`), then `XMM13` vs `XMM10`, then `(XMM13 − XMM10) / (-3.5)` (`0x1405001A4 DIVSS XMM13,dword ptr [0x14259D658]` = -3.5);
6. write the flag, the id and the angle.

**Gate.** `T+0x255 != 0` (the team has the ball) **AND** (the ball carrier id `< 0x16` **OR** `T+0x78 < 0x16`) — `0x1404FF8FD MOV EAX,dword ptr [RBX+0x78]` and the surrounding compare. CONFIRMED.

**The interpolated position.** `XMM8 = [RDX + RCX*4 + 0x40]` and `XMM9 = [RDX + RCX*4 + 0x38]` where `RDX = match+0x18` and `RCX = side*5` — these are the **pitch line bounds** (`match+0x18+0x38+0x14*side` and `+0x40+0x14*side`, the same fields the side planner `FUN_1404405D0` writes per team). CONFIRMED raw dis (`0x1404FFA0B`/`0x1404FFA12`).

**Gate.** `T+0x255 != 0` (the team has the ball) **AND** (the ball carrier id `< 0x16` **OR** `T+0x78 < 0x16`).

**Constants (script-read pairs).**
- `0x1404FFA19 MOVSS XMM0,dword ptr [0x14259858C]` = `360.0` (`0x43B40000`).
- `0x1404FFA75 MULSS XMM0,dword ptr [0x142588200]` = `0.1` (`0x3DCCCCCD`) — the per-frame interpolation factor.
- `0x1404FFB48 ADDSS XMM0,dword ptr [0x1425795E4]` = `6.2831854820251465` (`0x40C90FDB`) — 2π.
- `0x1404FFB51 MULSS XMM0,dword ptr [0x14259BE40]` = `57.2957763671875` (`0x42652EE0`) — 180/π.
- `0x1404FFB5E ADDSS XMM0,dword ptr [0x142598580]` = `180.0` (`0x43340000`).
- `0x1404FFB7F ADDSS XMM0,dword ptr [0x142598598]` = `-360.0` (`0xC3B40000`).
- `0x1404FFB8D MOVSS XMM0,dword ptr [0x14253EE30]` = `100.0` (`0x42C80000`).
- `0x1404FFCE4 MULSS XMM0,dword ptr [0x14259BDEC]` = `6.0` (`0x40C00000`).
- `0x1404FFCEC ADDSS XMM0,dword ptr [0x14259222C]` = `5.0` (`0x40A00000`).
- `0x1404FFDE9 COMISS XMM0,dword ptr [0x1425AB4C4]` = `625.0` (`0x441C4000`); `0x1404FFE00 DIVSS XMM0,dword ptr [0x1425C48E0]` = `525.0` (`0x44034000`).
- `0x1404FFE08 MOVSS XMM0,dword ptr [0x1425656C0]` = `4.0`; `0x1404FFE19 ADDSS XMM0,dword ptr [0x142565604]` = `2.0`.
- `0x1404FFE99 MOVSS XMM0,dword ptr [0x142591140]` = `30.0`; `0x1404FFEA5` `[0x14259222C]` = `5.0`; `0x1404FFEB0` `[0x14259BE20]` = `25.0`.
- `0x1404FFED6 MULSS XMM0,dword ptr [0x14258C264]` = `3.0`.
- `0x1404FFEE9 ADDSS XMM0,dword ptr [0x14259BDE4]` = `2.5` (`0x40200000`).
- `0x1404FFF67 MOVSS XMM0,dword ptr [0x1425C48D4]` = `115.0` (`0x42E60000`); `0x1404FFF81 DIVSS XMM0,dword ptr [0x1425C4904]` = `-85.0` (`0xC2AA0000`).
- `0x1404FFF93 ADDSS XMM0,dword ptr [0x142588210]` = `10.0`.
- `0x1405001A4 DIVSS XMM0,dword ptr [0x14259D658]` = `-3.5` (`0xC0600000`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the latch, the pass target, the ball/player positions and velocities, the pitch line bounds (`match+0x18+0x38+0x14*side` / `+0x40+0x14*side`), the 24-entry roster.
- Outputs: `T+0x8C35[11]` (found flag), `T+0x8C40[11]` (target id, `0xFF` = none), `T+0x8D50[11]` (the aim/lead angle).
- Pieces: (1) the latch + carrier gate; (2) the per-slot candidate scan (on-pitch → roster record not substituted → interpolate the position with `rand*0.1 + 0.5`); (3) the angle clamp (`0.5` band, then `/(-3.5)`).
- Red-test sketch: `TeamTick.NoTargetIsRecordedWhenTheTeamHasNoBall` — with `T+0x255 == 0` every `T+0x8C40[slot]` stays `0xFF` and every `T+0x8C35[slot]` is 0.
- Stand-in naming: `// ponytail: GF pitch-line bounds replace PES match+0x18+0x38/0x40 + 0x14*side (written by FUN_1404405D0).`

**Note on the branch split (CONFIRMED raw dis).** The whole function is gated on `MI+0x116C == 1` (`0x1404FF946 CMP dword ptr [R8+0x116c],0x1 / 0x1404FF94E JNZ 0x140500134` — the whole body is skipped otherwise). The `625.0`/`525.0` pair and the 0–360° wrap belong to the **carry-over** branch (`0x1404FFB2E`–`0x1404FFB8D`, the angle from the ball carrier to the predicted receiver position `player_pos + vel * tick`), and the per-slot loop above (`0x1404FFA40`–`0x1405000AE`) is the **per-slot target selection** that runs inside the same arm. The carry-over value is stored at `[RSP+0x24]`, and the per-slot loop reuses it as the fallback angle. Both are in the same callee and the same `MI+0x116C == 1` arm — there is no second branch.

The interpolated position per candidate (CONFIRMED raw dis, `0x1404FFABC`–`0x1404FFAE4`): with `tick = (int)(rand*0.1 + 0.5)` (`0x1404FFA75 MULSS XMM0,[0x142588200]` = 0.1, `0x1404FFA8F ADDSS XMM0,[0x142565600]` = 0.5, `0x1404FFA93 CVTTSS2SI RBX,XMM0`):
```
pred_x = pl+0x55C + tick * pl+0x574     ; 0x1404FFAC4 MULSS XMM2,[RAX+0x574] / 0x1404FFADC ADDSS XMM2,[RAX+0x55C]
pred_z = pl+0x564 + tick * pl+0x57C     ; 0x1404FFAD4 MULSS XMM1,[RAX+0x57C] / 0x1404FFAE4 ADDSS XMM1,[RAX+0x564]
pred_y = pl+0x560 + tick * pl+0x578     ; 0x1404FFABC MULSS XMM0,[RAX+0x578] / 0x1404FFACC ADDSS XMM0,[RAX+0x560]
```
(so `+0x574`/`+0x57C` are the x/z velocities and `+0x578` is the vertical velocity, with `+0x560` the height.)

### Step 21 — `0x1404F6E77` → `FUN_1404F7BA0` (restart / kickoff target)

**Callee VA.** `FUN_1404F7BA0(brain)`.

**Reads.** `T+0x255` (latch), `G+0x180` roster, `T+0xB8D0` (the current target), `pl+0x55C`/`+0x564`, `match+0xE0` (via `FUN_140A3E190`), `MI+0x1198`.

**Writes.** **`T+0xB8D0`** (the new target player id; `0xFF` = none) and **`brain+0x6D40`** (the frame stamp `MI+0x1198`).

**Gate.** `T+0x255 == 0` (no possession) **AND** `FUN_140A55D40(roster, side) == 0` **AND** `FUN_140A40BD0(T, …) → FUN_140A60340(...) > 4` **AND** (side != 1 **OR** the side-1 flag `match+0x18+0x24C68+side` is clear). Then `FUN_140A627E0(match,0)` (the ball carrier) must be `< 0x16`; and the restart-type check `(*piVar12 - 1U < 3) || (*piVar12 == 0x10)` on the match object.

**Constants (script-read pairs).**
- `0x1404F7DFA MOVSS XMM0,dword ptr [0x14259BDD0]` = `0.6944444179534912` (`0x3F31C71C`).
- `0x1404F7E0E MOVSS XMM0,dword ptr [0x14259858C]` = `360.0`; `0x1404F7E31 ADDSS XMM0,dword ptr [0x142598598]` = `-360.0`.
- `0x1404F7E47 MOVSS XMM0,dword ptr [0x14256AE1C]` = `1.0`; `0x1404F7E6E` `[0x142588218]` = `-1.0`.
- `0x1404F7E8D MOVSS XMM0,dword ptr [0x142588210]` = `10.0`.
- `0x1404F7EB1 MOVSS XMM0,dword ptr [0x14256AE38]` = `-1000.0` (`0xC47A0000`).
- `0x1404F7F32 SUBSS XMM0,dword ptr [0x14259BE1C]` = `20.0`.
- `0x1404F7F64 MOVSS XMM0,dword ptr [0x142591140]` = `30.0`.
- `0x1404F7FBD MULSS XMM0,dword ptr [0x14259D5C8]` = `0.3499999940395355` (`0x3EB33333`).

**SIM vs non-SIM.** SIM (it picks the restart receiver that other steps then use).

**Port contract.**
- Inputs: latch, restart type (from the match object), ball carrier id, roster.
- Outputs: `T+0xB8D0` (receiver id, `0xFF` = none), `brain+0x6D40` (frame stamp).
- Pieces: (1) the no-possession gate; (2) the restart-type gate (`(type-1)<3 || type==0x10`); (3) the receiver pick + `0xFF` fallback.
- Red-test sketch: `TeamTick.RestartReceiverChosenOnlyWithoutPossession` — with the latch set, `T+0xB8D0` is untouched; with it clear and a restart type in range, a receiver id is written.

### Step 22 — `0x1404F6E7F` → `FUN_1404FD4B0`

**Callee VA.** `FUN_1404FD4B0(brain)`.

**Reads.** `G+0x180` roster, `FUN_140A55D40(roster, side, 3)` and `FUN_140A55CF0(roster, side)` (the "this team has a set piece" predicates), `match+0x68`.

**Writes.** `brain+0x6DD8` and `brain+0x6DF0` (two per-side arrays), via `FUN_140A428E0(T, i, 3)` for 4 slots at the head.

**Gate.** `bVar3 = (FUN_140A55D40(...) || FUN_140A55CF0(...))`; the head always calls `FUN_140A428E0(T, 0..3, 3)` (clears 4 slots); if `!bVar3` → `FUN_1404FDC80(brain)` and return; else it looks up the roster record and fills the arrays.

**Constants.** `4` (the number of slots cleared), `3` (the slot-state id).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the two roster predicates (set-piece owner).
- Outputs: `brain+0x6DD8[4]`, `brain+0x6DF0[4]` (per-side set-piece slot lists).
- Pieces: (1) the always-clear of the 4 head slots; (2) the `!bVar3` fallback path; (3) the roster fill.
- Red-test sketch: `TeamTick.SetPieceSlotsClearedWhenTeamHasNoSetPiece` — with both roster predicates false the four slots are zeroed and the fallback path runs.

### Step 23 — `0x1404F6E9D` → `FUN_14053B860`

**Callee VA.** `FUN_14053B860(brain+0x6C08, match, side, T)`.

**Reads.** `T` (via `FUN_140A41AC0`), the match object, `brain+8` (side).

**Writes.** `*(brain+0x6C08) = match`, `*(brain+0x6C08+8) = side`; then it dispatches to one of `FUN_14053BA10` (arg 1), `FUN_14053BDC0` (2), `FUN_14053BF10` (3), `FUN_14053B900` (5), `FUN_14053BB90` (7) — note the switch skips 0, 4, 6, 8.

**Gate.** none at the call site (unconditional).

**Constants.** the dispatch indices `1,2,3,5,7`.

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: match, side, `T`.
- Outputs: `brain+0x6C08` context and the per-index side effects of the dispatched helper.
- Pieces: (1) the ctx store; (2) the index dispatch table; (3) the "no helper for 0/4/6/8" rule.
- Red-test sketch: `TeamTick.DispatchTableSkipsZeroFourSixEight` — with the dispatch index set to 4 no helper runs.

### Step 24 — `0x1404F6EA5` → `FUN_1404F87D0`

**Callee VA.** `FUN_1404F87D0(brain)`.

**Reads.** `MI+0x116C == 3`, `MI+0x1554 == 4`, `G+0x180` roster, `MI+0x1748` (on-pitch), `match+0x68`.

**Writes.** **`T+0xB8BC = 0`** at entry, and again at the end of the roster fan-out branch (the value is written twice, so it is a flag, not a counter).

**Gate.** `MI+0x116C == 3 && MI+0x1554 == 4` → the roster fan-out runs (`FUN_140A66FF0(match+0x68, i)` per on-pitch player of this side). Otherwise only the zeroing happens.

**Constants.** `0x18` (roster entries), `0x74` (roster stride), `0x16` (22).

**SIM vs non-SIM.** SIM (it clears a flag consumed elsewhere) — but the roster fan-out is an event call. Classify: SIM for the `T+0xB8BC` flag, non-SIM for the fan-out.

**Port contract.**
- Inputs: match phase (`MI+0x116C`), phase code (`MI+0x1554`), on-pitch roster.
- Outputs: `T+0xB8BC` (always 0 after the call).
- Pieces: (1) the unconditional zeroing; (2) the phase-gated roster fan-out.
- Red-test sketch: `TeamTick.B8bcAlwaysZeroAfterCall` — the field is 0 after the call in every phase.

### Step 25 — `0x1404F6EAD` → `FUN_140500B90` (branch selector)

**Callee VA.** `FUN_140500B90(brain)`; returns a byte.

**Reads.** `G+0x180` roster, `match+0x68`, `T+0x8E08` (formation sheet), `T+0x1FC`, `DAT_1436F3DC8+0x1F8+0x24`, `FUN_140A3FE50`.

**Writes.** `brain+0x6D06` / `brain+0x6D08` / `brain+0x6D0C` (three ints), and `T+0x1FC` (a byte) — plus a 16-byte scratch block at `puVar18+0xC..0xF`.

**Gate.** The branch selector `FUN_140A55CF0(roster, side)`, `FUN_140A66F50` / `FUN_140A67010` (the roster record lookup), and `FUN_140A3FE50(DAT_1436F3DC8+0x1F8+0x24)`.

**Constants.** `0xB` (11), `0x17` (roster clamp), `0x74` (roster stride), `0x18` (struct size).

**SIM vs non-SIM.** SIM (the returned byte is the "local_397" that selects the restart branch vs the non-restart branch at `0x1404F6F18`).

**Port contract.**
- Inputs: roster records for the side, the formation sheet, the "in-play" flag (`T+0x1FC`).
- Outputs: a branch byte (the tick's `local_397`) plus `brain+0x6D06/08/0C`.
- Pieces: (1) the roster-record lookup; (2) the `T+0x1FC == 0` early return; (3) the `iVar7 == 2` early return vs the `FUN_1408C5E20(T+0x8E08, uVar9)` write.
- Red-test sketch: `TeamTick.BranchSelectorStaysZeroWhenNotRestarting` — with `T+0x1FC == 0` the selector returns 0 and the formation sheet is untouched.
- Note: the returned byte's *meaning* is still **INF** (it selects the restart branch; it is not the same as the possession latch).

### Step 27a-pre — `0x1404F6F33` → `FUN_1404FBB70` (role-class census)

**Callee VA.** `FUN_1404FBB70(match, side, T)`.

**Reads.** `T+0x8E08` (the role/formation sheet) — read through `FUN_1408C51B0(sheet, i, 0, 3)`, which returns the 11×3 role table (`param_1[((class*0xB + slot)*3 + col) + 10]`, `0xC` = none).

**Writes.** **`T+0x8984`, `T+0x8987`, `T+0x898A`, `T+0x898D`** — four byte counters, one per role bucket.

**Gate.** The caller gate is `local_397 != 0` (the branch selector of step 25) **OR** `brain+0xC7C != 0` (`0x1404F6F1C CMP byte ptr [RDI+0xC7C],AL / JZ 0x1404F6F33` — CONFIRMED raw dis `dis_1404F6C30.txt:141-146`).

**The role→bucket map (CONFIRMED decompile):**

| `FUN_1408C51B0` return | bucket id `uVar2` | bucket index `FUN_140A40C10(uVar2)` |
|---|---|---|
| 1 | 1 | 0 |
| 2, 3 | 2 | 0 |
| 4 | 3 | 1 |
| 5 | 4 | 1 |
| 6, 7 | 5 | 2 |
| 8 | 6 | 3 |
| 9, 10 | 8 | 3 |
| 11 | 7 | 2 |
| 12 | 9 | 3 |
| (default) | 10 | — (no bucket) |

A counter is incremented only when `FUN_140A40C10(uVar2) < 4`.

**Constants.** `0xB` (11 slots), `4` (bucket count).

**SIM vs non-SIM.** SIM (role census used by the shape/role code).

**Port contract.**
- Inputs: the 11×3 role table of the formation sheet.
- Outputs: four bucket counters at `T+0x8984/87/8A/8D`.
- Pieces: (1) the role→bucket-id map; (2) the `FUN_140A40C10` bucket index (0/1/2/3 with 9→3, 10→none); (3) the caller gate (`local_397 || brain+0xC7C`).
- Red-test sketch: `TeamTick.RoleCensusCountsPerBucket` — a sheet with three class-1 roles yields bucket 0 == 3 and the other buckets 0.
- Note: the role table source (`FUN_1408C51B0` on `T+0x8E08`) is the same source slice C uses.

### Step 29 — `0x1404F6FBE` → `FUN_140537AC0` (kick / restart seeding)

**Callee VA.** `FUN_140537AC0(brain+0x62B5, T, match, side)`.

**Reads.** `match+0x18+0x24D50` (a 4-entry list) and `+0x24D54` (its count), `pl+0x5D0` (the AI action of the target).

**Writes.** **`T+0xA2A0[slot] = 0x1E`** (SPACE_RUN_CHANCE, via `FUN_140A429A0`) and **`T+0x130 = <player id>`**.

**Gate.** `count != 0` and `list[0] < 0x16`; the owning side of `list[0]` (`(id > 10 ? 1 : 0)`) must equal `side` **AND** `count == 2`; then `list[1] < 0x16`; then the target player's `pl+0x5D0 != 0x1E` (else the whole block is skipped).

**Constants.** `0x16` (22), `0xB` (11, the slot index), `0x1E` (SPACE_RUN_CHANCE), `2` (the list length).

**SIM vs non-SIM.** SIM (it seeds the run state that the per-player AI reads).

**Port contract.**
- Inputs: the 4-entry kick list (`match+0x18+0x24D50`), the side.
- Outputs: `T+0xA2A0[slot] = 0x1E`, `T+0x130 = <player id>`.
- Pieces: (1) the `count == 2` and side-ownership gate; (2) the `pl+0x5D0 != 0x1E` skip; (3) the slot index `list[2] % 0xB`.
- Red-test sketch: `TeamTick.KickSeedsSpaceRunOnTheReceiver` — a two-entry kick list owned by this side seeds run state 0x1E on the third entry's slot.

### Step 30 — `0x1404F6FDA` → 11× `FUN_140A40240`

**Callee VA.** `FUN_140A40240(&local_308 + 0x10*i)` for `i` in 0..10.

**Reads.** none.

**Writes.** The 11 stack scratch records at `[RBP-0x40]` (stride `0x40`): each record's first 8 qwords are zeroed and `FUN_140A85AA0()` clears the trailing structure. Pure initialisation of the stack snapshot buffer.

**Gate.** `local_398 == 0` (the skip flag of step 1) — `0x1404F6FBE CMP byte ptr [RSP+0x30],0x0 / JNZ 0x1404F76B6`.

**Constants.** `0xB` (11 records), `0x40` (record stride).

**SIM vs non-SIM.** SIM (bookkeeping only).

**Port contract.**
- Inputs: the skip flag.
- Outputs: an 11-record zeroed snapshot buffer.
- Pieces: (1) the skip gate; (2) the 11× zeroing; (3) the `FUN_140A85AA0` sub-clear.
- Red-test sketch: `TeamTick.SnapshotBufferZeroedWhenNotSkipping` — the buffer records are all zero before the snapshot copy.

### Step 31 — inline stack snapshot (no call site)

**Action.** The tick copies the live run-state blocks out of `T` into the stack buffer: `T+0xA2A0` (11 ints) → `local_340[0..10]`; `T+0x28C..0x2A4` (the 11×0x40 run-order records, fields at `+0x26C`, `+0x278`, `+0x27C`, `+0x288`, `+0x28C`) → the `local_308` array; `T+0x474 + 0x10*i` for `i` in 0..2 → the third block (`auStack_55c`).

**Reads.** `T+0xA2A0`, `T+0x26C..0x2A4`, `T+0x2AC..0x304`, `T+0x30C..0x348`, `T+0x34C..0x388`, `T+0x38C..0x3C4`, `T+0x3C8..0x404`, `T+0x40C..0x444`, `T+0x448..0x464`, `T+0x474..0x4A4`.

**Writes.** Stack only.

**Gate.** inside the `local_398 == 0` block.

**Constants.** `0xB` (11), `0x10` (record stride of the `T+0x474` block).

**SIM vs non-SIM.** SIM (it is the input snapshot the concept selector/executor read).

**Port contract.**
- Inputs: the live run-state blocks of `T`.
- Outputs: an immutable snapshot object.
- Pieces: (1) the 11-int `T+0xA2A0` copy; (2) the 9×`0x40` record copy; (3) the 3×`0x10` `T+0x474` copy.
- Red-test sketch: `TeamTick.SnapshotIsStableWhileTKeepsChanging` — a snapshot taken before a `T+0xA2A0` write keeps the old value.

### Step 32 — `0x1404F7643` → `FUN_140A84E30`

**Callee VA.** `FUN_140A84E30(T+0x6F4, 0xFF)`.

**Reads.** the argument `0xFF` (a "reset all" sentinel).

**Writes.** the 11-slot ring at `T+0x6F4` (stride `0x78`): for the reset-all case it writes `0xFF` into 10 id slots, `4` into 10 counters, zeroes the byte flags, and sets the header to `7`, byte `+1`, and `0xBF800000` (`-1.0f`) at `+0x14`.

**Gate.** none (unconditional inside the `local_398 == 0` block).

**Constants.** `0xFF`, `0x78` (stride), `7`, `4`, `0xBF800000` = `-1.0` (`0x140A84E30` writes the literal `0xbf800000`).

**SIM vs non-SIM.** SIM (a per-slot "last action" ring that other steps read).

**Port contract.**
- Inputs: nothing (a reset-all call).
- Outputs: an 11-slot ring reset to the "no action" state.
- Pieces: (1) the id fields set to `0xFF`; (2) the counters set to `4`; (3) the header `7` / flag `1` / `-1.0`.
- Red-test sketch: `TeamTick.ActionRingResetToNoAction` — after the call every ring id is `0xFF`.

### Step 35 — `0x1404F76B1` → `FUN_1404F61E0` (opposition finder)

**Callee VA.** `FUN_1404F61E0(brain, &local_360, T)`.

**Reads.** `T+0xB7D8` (a base slot index), `T+0x254` (side sign), `pl+0x554` (a speed/skill field), `pl+0x55C`/`+0x564` (positions), `pl+0x5D0` (AI action: `1` or `0x3F`), `MA+0x22E4C + 200*owner` (the nearest list), `G+0x2AA8+0x18` (ball x).

**Writes.** **`T+0xB7EC + (id % 0xB)` = 1** (an 11-byte "found opponent" mask), plus `T+0xB7F4` (2 bytes) and `T+0xB7F6` (1 byte) cleared at entry.

**Gate.** The owner id must be `< 0x16` (or the ball carrier at `match+0x18+0x2483C`); the owner must not be a keeper (`FUN_140A6E320(pl+1) == 0`) and must not be the designated target `uVar17`; then the loop over 11 neighbours with the distance² and angle gates.

**Constants (script-read pairs).**
- `0x1404F61E0` `DAT_1425C48E4` = `1369.0` (`0x44AB2000`) — the distance² reject (37²).
- `0x1404F61E0` `DAT_1425C48D0` = `105.0` (`0x42D20000`) — the angle cap.
- `0x1404F61E0` `DAT_14259BE04` = `12.0` (`0x41400000`) — the angle window.
- `0x1404F61E0` `DAT_14259BE5C` = `90.0` (`0x42B40000`) — the angle argument.
- `0x1404F61E0` `DAT_14259BE44` = `64.0` (`0x42800000`) — the reach argument.
- `0x1404F61E0` `DAT_14259BDF0` = `7.0` (`0x40E00000`) — the band lower edge.
- `0x1404F61E0` `DAT_14259BE0C` = `15.0` (`0x41700000`) — the band offset.
- `0x1404F61E0` `DAT_14256AE1C` = `1.0`; `0x1404F61E0` `DAT_142596E6C` = `80.0` (`0x42A00000`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the owner slot (`T+0xB7D8`), the nearest list, the opponent positions, the side sign.
- Outputs: the 11-byte opponent mask `T+0xB7EC[]` (set for each opponent judged "reachable"), plus the cleared `T+0xB7F4/6`.
- Pieces: (1) the distance² reject `1369.0`; (2) the angle window `12.0` / `90.0` / `64.0`; (3) the "at most 2 found → early return" rule.
- Red-test sketch: `TeamTick.OpponentMaskMarksOnlyReachables` — an opponent beyond the distance² reject is not marked; one inside it is.

### Step 36 — `0x1404F76BE` → `FUN_14057D240` (run-state 0x6D cleanup)

**Callee VA.** `FUN_14057D240(brain)`.

**Reads.** `DAT_1436F3DC8+0x3B0` (via `FUN_1423E7FD0`), `T+0xA2A0[0..10]`, `T+0x88D8` (the current "designated" player id), `MI+0x116C`.

**Writes.** **`T+0xA2A0[slot] = 0`** (via `FUN_140A42940(T, slot, 0x6D)`) when `FUN_14057C8C0` says the run no longer holds; **`T+0x88D8 = 0xFF`** when the current holder was released; **`brain+0x6C68 = 1`** once (latched by `MI+0x116C == 1`).

**Gate.** The whole body returns early when `G+0x3B0 != 0 && FUN_1423E7FD0() != 0` (the same skip condition as step 1, but read here directly). Then for each slot whose state is `0x6D`: if `FUN_14057C8C0(brain, side*0xB + i) == 0` → release (state 0) and, if that slot was the current holder, set `T+0x88D8` to it; else → `FUN_140A40820()` (full release).

**Constants.** `0x6D` (the run-state id), `0xB` (11), `0xFF`.

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the 11 run states, the current holder id.
- Outputs: released run states, `T+0x88D8` (current holder or `0xFF`), `brain+0x6C68`.
- Pieces: (1) the early-out on the skip object; (2) the per-slot release test; (3) the holder-id fixup.
- Red-test sketch: `TeamTick.StaleRunStateReleasedAndHolderCleared` — a slot in state `0x6D` that no longer qualifies is set to 0 and `T+0x88D8` becomes `0xFF`.

### Step 40 — `0x1404F7824`–`0x1404F7876` (8 functions)

> **Round 2 (§5.2, §5.3, §5.4) reduced the three big bodies** (40e, 40f, 40h) and corrected the
> `T+0xB6DC` claim. Summary: **40f** writes `T+0xBAB4` (a 0x21-column interception-cost table, int,
> budget 50); **40h** writes `T+0xC128` (a 16-bit "who may receive the restart" mask, 11 bits);
> **40e** writes only the shared per-player block (stride 0x1C) and only *reads* `T+0xB6DC`, which
> has no writer in the executable image.

Call sites (CONFIRMED raw dis `dis_1404F6C30.txt`):

| # | call site | callee | what it does | writes |
|---|---|---|---|---|
| 40a | `0x1404F7824` | `FUN_14057BBB0(brain)` | For each slot in state `0x23`: if `MI+0x116C != 1` and the slot's `T+0x4B04 + 4*b` (3 entries) does **not** contain `slot % 0xB`, release the slot (`FUN_140A40820`). | `T+0xA2A0[slot]` |
| 40b | `0x1404F7843` | `FUN_14053B110(brain+0x6BA8, match, side, T)` | Stores the ctx (`+0x10` = match, `+0x18` = side), then the shape-dispatch: `iVar2 = T+0xB8AC`; if `MI+0x116C==3 && MI+0x1554==4 && T+0xB8AC != 4 && T+0x255 != 0`: if `iVar2 == T_team+0xB8AC` → `FUN_14053B420`; else the per-mode dispatch (`0`→`FUN_14053A820`+thunk, `1`→`FUN_14053AA90`, `2`→thunk, `3`→`FUN_14053AE60`), each setting `*param_1 = 1`. Otherwise `FUN_14053A820` and `T+0xB8AC = 4`. | `T+0xB8AC`, `brain+0x6BA8+0x10/+0x18` |
| 40c | `0x1404F784B` | `FUN_1404FB2A0(brain)` | Returns a value used as `uVar5` (fed to step 44's dispatcher). See step 40c note below. | `T+0x78` (via `FUN_1404FB380`/`FUN_1404FB780`) |
| 40d | `0x1404F7856` | `FUN_14057B6B0(brain)` | For each slot in state `0x25`: `FUN_14057AD30` decides; if clear → `FUN_14057B4F0(brain, id)` (assign), else → release. Then `FUN_14057B190(brain)`. | `T+0xA2A0[slot]` |
| 40e | `0x1404F785E` | `FUN_14057C190(brain)` | A 900-line geometry routine; writes `T+0xB6DC` (2 sites) and some `pl+0x18/+0x20`-shaped fields. | `T+0xB6DC` |
| 40f | `0x1404F7866` | `FUN_1404F8C70(brain)` | The largest of the eight (31 KB decompile). Constants: `0x14259BDF4`=8.0, `0x14259BDF0`=7.0, `0x14259BE04`=12.0, `0x14259BE0C`=15.0, `0x14259BE18`=18.0, `0x14259BE1C`=20.0, `0x14259BE20`=25.0, `0x14259BE2C`=40.0, `0x14259BE34`=50.0, `0x14259BE40`=57.2957763671875, `0x14259BE48`=65.0, `0x14259BE4C`=?, `0x14259BE7C`=?, `0x14259BE84`=-10.0, `0x14259D608`/`0x14259D60C`/`0x14259D654`/`0x14259D658`/`0x14259D664`=-12.0, `0x1425C48C0`=22.0, `0x1425C48C4`=?, `0x1425C48EC`=?, `0x142588200`=0.1, `0x14259222C`=5.0, `0x1425795E4`=6.2831854820251465, `0x142598580`=180.0, `0x14259858C`=360.0, `0x142598598`=-360.0, `0x14253EE30`=100.0, `0x142565600`=0.5, `0x1425656C0`=4.0, `0x14256AE1C`=1.0, `0x142588210`=10.0, `0x142588218`=-1.0. | see body |
| 40g | `0x1404F786E` | `FUN_140500A00(brain)` | Rebuilds a 2×11 candidate table at `T+0xC060` (8 bytes per entry: id + weight), the counts at `T+0xC0B0 + 0xB0`, the byte flags at `T+0xC110`, and the fill value `0xA0` at `T+0xC112..0xC126`; then sorts with `FUN_140A79640(T+0xC060, 0)`. | `T+0xC060`, `T+0xC110`, `T+0xC112` |
| 40h | `0x1404F7876` | `FUN_1404FAD30()` | A 6.7 KB routine; reads `T+0x254`, `match+0x18+0x2482C`, `T+0xC128` (cleared at entry). Constants: `0x1425A9680`, `0x14259BDF0`=7.0, `0x14259BDF4`=8.0, `0x14259BE1C`=20.0, `0x14259BE4C`, `0x14259BE5C`=90.0, `0x14259CE3C`, `0x142565600`=0.5, `0x14256AE1C`=1.0, `0x14256AE30`, `0x14258C264`=3.0, `0x1425C48B4`, `0x1425A1FF4`, `0x1425A2828`, `0x1425A9BF0`. | `T+0xC128` |

**40c note — the `T+0x78` writer.** `FUN_1404FB2A0` is the **designated-pass-target selector**, and it is the **first confirmed writer of `T+0x78`**:
- `MI+0x1775 == 0x0C` → returns immediately, no write.
- `MI+0x116C == 1` → if the skip object is present, `T+0x78 = 0xFF` and return 0.
- `MI+0x116C == 2` → `FUN_1404FB780(brain)` (which writes `T+0x78` from `match+0x18+0x24840` / `+0x24842` and from `MI+0x11CC`).
- `MI+0x116C == 3` → `FUN_1404FB380(brain)`, which writes `T+0x78 = 0xFF` on the "no candidate" paths and `T+0x78 = FUN_1404F0E50(brain, oldValue)` on the pick path.
- any other phase → `T+0x78 = 0xFF`.

This **closes the `T+0x78` open item** from `pes-re-teamtick.md` §4.2 (the writer was previously unaccounted for). CONFIRMED (decompile) — the *live* confirmation is still OPEN.

**SIM vs non-SIM.** All eight are SIM, except 40g's `0xA0` fill constants which look like a presentation fallback — still SIM (it is the candidate table the shape code reads).

**Port contract (one contract for step 40, ≤3 pieces).**
- Inputs: the run states, the mode pair (`T+0xB8A8`/`T+0xB8AC`), the latch, the role table, the nearest list, the ball/player geometry.
- Outputs: the released/assigned run states, `T+0xB8AC`, `T+0xB6DC`, the candidate table `T+0xC060`, and `T+0x78` (the designated pass target).
- Pieces: (1) the 0x23 / 0x25 slot release rules (40a, 40d); (2) the mode dispatch on `T+0xB8AC` (40b) and the `T+0x78` selector (40c); (3) the candidate-table rebuild + sort (40g) and the geometry writer (40f).
- Red-test sketch: `TeamTick.PassTargetClearedWhenNoCandidate` — with no eligible receiver, `T+0x78` becomes `0xFF`; with one, it becomes that player's id.
- Stand-in naming: `// ponytail: GF formation/mode table replaces PES T+0xB8AC mode dispatch (FUN_14053B110).`

### Step 45 — `0x1404F796D` → `FUN_1404F88D0`

**Callee VA.** `FUN_1404F88D0(brain)`; the tick calls it only when `FUN_140CA1A50(0) != 0` — and `FUN_140CA1A50` returns **1** unconditionally, so the call is **always made** (`dec/FUN_140CA1A50.c`). CONFIRMED.

**Reads.** `MI+0x116C == 1` (the only arm), `T+0x4BD0` (a float), `match+0xE0` (via `FUN_140A3E940(…, 0x24)`), `pl+0x55C`/`+0x564` (the 11 opponents' positions), `pl+0x5D0` (the AI action), `FUN_14044E200()` (a "current action" id).

**Writes.** **`T+0x64C + 4*i` = the opponent id** and **`T+0x678 + 4*(id % 0xB)` = the rank** — via `FUN_140A42490(T, i, id, rank)`. CONFIRMED (`dec/FUN_140A42490.c`).

**Gate.** `MI+0x116C != 1` → nothing (the function body is entirely inside the `MI+0x116C == 1` arm). Inside it: if `T+0x4BD0 > 0.0` and the match object is in state `0x37` with `piVar7[6] == 4` → **return early** (no ranks written).

**Constants (script-read pairs).**
- `0x1404F89CB MOVSS XMM7,dword ptr [0x1425C48E8]` = `9999.0` (`0x461C3C00`) — the "no distance penalty" sentinel.
- `0x1404F8B24 IMUL EAX,EDI,0x2710` — `0x2710` = `10000` (immediate) — the class weight scale.
- `0x1404F8C01 IMUL EAX,EDI,0x64` — `0x64` = `100` (immediate) — the rank scale.
- `0x1404F8B86 LEA R9,[0x142132000]` — the sort comparator table (a code address, not a float).

**The score formula (CONFIRMED raw dis).** For each of the 11 own players (`uVar15 = side*0xB + i`):
- the class `iVar5` comes from `FUN_140A40110(T+0x62E8+…, uVar15, T)` if `T+0x62E8+…` is non-zero, else `5` (`0x1404F8AC6 MOV EDI,0x5`);
- the distance term: if `pl+0x5D8 > 2`, the squared distance to the goal centre (`FUN_140A76240`) is computed and `XMM6 = max(9999.0 − d², 0.0)` (`0x1404F8B1C SUBSS XMM0,XMM1 / 0x1404F8B20 MAXSS XMM6,XMM0`), else `XMM6` stays `0.0`;
- the sort key stored at `[RSP+RSI*0x8+0x34]` is `class*10000 + distance_term` (`0x1404F8B24 IMUL EAX,EDI,0x2710` + `0x1404F8B33 ADDSS`).

**The rank formula (CONFIRMED raw dis, `0x1404F8BC4`–`0x1404F8C0A`).** After the sort, walking the sorted 11:
`EDI = local_98[id % 0xB]` (the pre-sort class of that id, read from `[RSP+RAX*0x4+0x90]`);
`EBX = (previous_class == EDI) ? running_index : 0` (`0x1404F8BF4 CMP ECX,EDI / 0x1404F8BFD CMOVZ EBX,R9D`);
`R9D = EBX + EDI*100` (`0x1404F8C01 IMUL EAX,EDI,0x64` / `0x1404F8C06 LEA R9D,[RBX+RAX*0x1]`);
and `0x1404F8C0A CMOVZ R9D,EAX` — **if the class is 0 the whole rank is 0** (the `+100` form is discarded).
Then `FUN_140A42490(T, index, id, rank)`.

So the rank is `class*100 + tie-break-index`, with class 0 mapping to rank 0.

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the 11 opponent ids + their `local_98` ranks, `T+0x4BD0`, the match-object state.
- Outputs: `T+0x64C[11]` (opponent ids in order) and `T+0x678[11]` (the composite rank).
- Pieces: (1) the `MI+0x116C==1` arm; (2) the early-out on the `0x37`/`[6]==4` match state with `T+0x4BD0 > 0`; (3) the `rank*100 + tie` key.
- Red-test sketch: `TeamTick.OpponentRankIsHundredTimesRankPlusTieBreak` — two opponents with equal rank get consecutive tie indices; a different rank jumps by 100.

### Step 46-body — `0x1404F7975` → `FUN_1404FC720` (set-piece / restart events)

**Callee VA.** `FUN_1404FC720(brain)`.

**Reads.** `match+0x68` (the event sink), `G+0x180` roster, `match+0xE0` (the state machine), `T+0x140` (a byte), `T+0x254`, `T+0x6D4`, `T+0x6D8`, `T+0x6E5`, `T+0x6E8`, `T+0xA570`, `MI+0x116C`.

**Writes.** **`brain+0x6C4C`** (the event id, written in 5 branches), **`brain+0x6C50`** (a byte), **`brain+0x6C48`** (a countdown, decremented), **`brain+0x6C54`** (a countdown, decremented / re-randomised), **`brain+0x6D78`** (a pointer to a shared vector), plus `T+0x140` (1→2→0), `T+0x13C` (a float copied from `match+0x18+0x40+0x14*side`), `T+0x6E5` (a bool), `T+0xA570` (a byte), and `T+0x6D4`/`0x6D8` via `FUN_140A425B0/C0/D0`.

**Gate.** `FUN_140A62690(match, side) < 0x16` (the side has a valid "current player"); then a per-roster walk to find the side's roster record.

**Constants (script-read pairs).**
- `0x1404FCD57 MULSS XMM0,dword ptr [0x142572348]` = `0.800000011920929` (`0x3F4CCCCD`) — the `brain+0x6C54` re-randomisation scale (`brain+0x6C54 = (int)(rand * 0.8)`).
- `0x1404FCCF2 MOVSS XMM0,dword ptr [0x14256AE1C]` = `1.0` (`0x3F800000`).
- `0x1404FC9F6 SUBSS XMM0,dword ptr [0x14259BDEC]` = `6.0` (`0x40C00000`) — the cover-drop distance (6 m).
- `0x1404FCDE7 MOVSS XMM0,dword ptr [0x14259CE3C]` = `7.5` (`0x40F00000`).
- `0x1404FCE3F COMISS XMM0,dword ptr [0x142588210]` = `10.0` (`0x41200000`).
- `0x1404FCE51 DIVSS XMM0,dword ptr [0x14259D60C]` = `9.0` (`0x41100000`).
- `0x1404FCE59 MULSS XMM0,dword ptr [0x14253EE30]` = `100.0` (`0x42C80000`); `0x1404FCE7A DIVSS XMM0,dword ptr [0x14253EE30]` = `100.0` — the lerp normaliser.
- `0x1404FCE6E MULSS XMM0,dword ptr [0x14259BE0C]` = `15.0` (`0x41700000`).
- `0x1404FCE8B XORPS XMM0,dword ptr [0x1425656D0]` = `-0.0` (`0x80000000`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the side's current player id, the roster, the match state machine, the restart type.
- Outputs: `brain+0x6C48`/`0x6C4C`/`0x6C50`/`0x6C54` (the set-piece event state), `T+0x140` (the 0→1→2→0 phase byte), `T+0x13C` (the side's pitch reference float).
- Pieces: (1) the `FUN_140A62690 < 0x16` gate; (2) the `T+0x140` 1→2→0 phase machine with the `brain+0x6C54` countdown; (3) the `brain+0x6C48` countdown decrement and the `T+0xA570` clear.
- Red-test sketch: `TeamTick.SetPiecePhaseByteCyclesOneTwoZero` — `T+0x140` goes 1 on the first tick of the event, 2 on the next, 0 after the countdown reaches 0.

### Step 47-meaning — `0x1404F7980` → `FUN_140509170`

**Callee VA.** `FUN_140509170(match, side)` → returns a byte stored into **`T+0xB6DE`**.

**Reads.** `MI+0x116C` (must be 1 or the function returns 0), `match+0x18+0x2482C/0x24834` (the ball x/z), `match+0x18+0x24838` (the ball owner id), `pl+0x1414/0x141C` (a per-player position), `FUN_140A62690(match, side)` (the side's current player), `T+0x254` (the side sign), `MA`-style tables at `match+0x18+0x2470C` and `+0x247BE`, `G+0x40+0x14/0x18/0x1C/0x20` (the pitch box).

**Writes.** None inside `FUN_140509170` — the **only** write is the tick's `*(T+0xB6DE) = uVar5` at `0x1404F7980` (`dec/FUN_1404F6C30.c:601-602`).

**The meaning (CONFIRMED decompile + dis).** The function returns a byte with these branches:
- `MI+0x116C != 1` → **0**.
- The side's current player is not on the attacking side of the ball (`side_sign * ball_x > 0`) → **0**.
- The nearest-list lookup finds a player closer to the ball than the side's current player → returns **0** in the "too far" cases, and **1** in the "a defender is much closer" case (`DAT_1425A2670` distance² test).
- The angle test around the ball (`DAT_14259DEC` = -6.0 and `DAT_14259BE28`) decides 0 vs 1.
- The `FUN_140A4FEA0` call (the "can this side play the ball" predicate) returning non-zero → **0**.

So **`T+0xB6DE` is a per-side "this side is contesting the ball" flag** — read by the **opponent's**
player AI, which releases its AI request when the flag is set. The full evidence is in **§5.6**;
the branch structure is CONFIRMED and the semantic label is now CONFIRMED (it was INF(h) in
round 1).

**Constants (script-read pairs).**
- `0x14050972D MOVSS XMM0,dword ptr [0x14259BDEC]` = `6.0` (`0x40C00000`); `0x1405092EB XORPS XMM0,dword ptr [0x1425656D0]` = `-0.0` — the `-6.0` cover margin.
- `0x14050971C MOVSS XMM0,dword ptr [0x14259BE28]` = `32.0` (`0x42000000`) — the lateral window (32 m).
- `0x1405096C0 MOVSS XMM0,dword ptr [0x14259BE20]` = `25.0` (`0x41C80000`) — the depth threshold (25 m).
- `0x140509328 MOVSS XMM0,dword ptr [0x14259BE40]` = `57.2957763671875` (`0x42652EE0`); `0x140509316` `[0x1425795E4]` = `6.2831854820251465` (`0x40C90FDB`) — the radian/degree pair.
- `0x140509424 ADDSS XMM0,dword ptr [0x14259222C]` = `5.0` (`0x40A00000`) — the aim offset.
- `0x14050967A ADDSS XMM0,dword ptr [0x14258C264]` = `3.0` (`0x40400000`).
- `0x1405095CD COMISS XMM0,dword ptr [0x1425A2670]` = `400.0` (`0x43C80000`) — the "a defender is much closer" distance² test.
- `0x140509713 COMISS XMM0,dword ptr [0x1425A9460]` = `29.0` (`0x41E80000`) — the lower gate of the 29–32 m window.
- `0x14050960D MOVSS XMM0,dword ptr [0x14259D5D4]` = `1.2000000476837158` (`0x3F99999A`).
- `0x14050976D SUBSS XMM0,dword ptr [0x1425656C0]` = `4.0` (`0x40800000`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the ball position and owner, the side's current player, the side sign, the nearest-list tables, the pitch box.
- Outputs: `T+0xB6DE` (byte flag).
- Pieces: (1) the `MI+0x116C != 1 → 0` arm; (2) the "side not on the attacking side of the ball → 0" arm; (3) the nearest-player / angle tests that return 1.
- Red-test sketch: `TeamTick.B6deZeroWhenSideIsNotTheBallSide` — with the other side on the ball, `T+0xB6DE` is 0.

### Step 48-bodies — `0x1404F7995`–`0x1404F79AD` (4 functions)

Call sites (CONFIRMED raw dis + decompile):

| # | call site | callee | behaviour | writes |
|---|---|---|---|---|
| 48a | `0x1404F7995` | `FUN_1404FC4D0(brain)` | Decides **`T+0x6E7`** (a "this side may not contest the ball" byte) from `FUN_140A62690(match, side)` (the side's current player) and that player's `pl+0x5D0` (AI action). The action whitelist is a bitmask: `(0x2000401U >> (uVar3 - 0x34 & 0x1f) & 1)` for `uVar3 >= 0x34`, and `0x4010000000200010U >> (uVar3 & 0x3f) & 1` for the low actions. If the current holder is the side's own designated pass target (`T+0x78 == uVar2`), the byte is cleared. | **`T+0x6E7`** |
| 48b | `0x1404F799D` | `FUN_14057BA50(brain)` | The **offside-trap executor**. For each slot in state `0x3B`: `FUN_14057B750` decides; if clear → `FUN_140A42940(T, slot, 0x3B)` (clear), else → `FUN_140A40820()` (release). Then, **only if `T+0x255 == 0` (no possession) and `T+0xE4 != 0` (trap armed)**, it finds the offside-line player (`match+0x18+0x71500` table, side offset `0x58` for side 1), and for every slot that is not itself in state `0x3B` and whose `T+0x4C00` counter is non-zero, if `distance − 2.0 <= ball_z` it assigns the trap: `FUN_140A42940(T, slot, 0x3B)` and `FUN_140A42A40(T, slot, &pos)` (the trap target position). | `T+0xA2A0[slot]` = `0x3B`, the trap target position |
| 48c | `0x1404F79A5` | `FUN_14057D240(brain)` | The same function as step 36 — **called twice per tick** (once at `0x1404F76BE`, once here). | see step 36 |
| 48d | `0x1404F79AD` | `FUN_140580630(brain)` | For each slot in state `0x10`: `FUN_14057D3A0(brain, side*0xB + i)` decides; if clear → `FUN_140A42940(T, i, 0x10)`, else → `FUN_140A40820()` (release). Then `FUN_14057F600(brain)`. | `T+0xA2A0[slot]` |

**Constants.**
- 48a: `0x34`, `0x3F`, `0x3B`, `0x4D`, `3`, `5`, `2`, `0x12`, `0x19`, `0x2000401`, `0x4010000000200010` — the action whitelists (bitmask constants, **immediate** not memory constants, so no operand VA to pair; CONFIRMED as immediates in the disassembly).
- 48b: `DAT_142565604` = `2.0` (`0x40000000`) — the trap-line margin (`fVar16 - 2.0 <= ball_z`).
- 48d: `0x10` (the run-state id), `0xB` (11).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: `T+0xE4` (trap armed), `T+0x255` (latch), `T+0x78` (pass target), the side's current player and AI action, the offside-line table.
- Outputs: `T+0x6E7` (the "cannot contest" byte), `T+0xA2A0[slot]` = `0x3B` (trap state) with the trap target position, and the release of the `0x3B`/`0x10` states.
- Pieces: (1) the 48a action-whitelist decision on `T+0x6E7`; (2) the 48b trap assignment (`T+0x255==0 && T+0xE4!=0`, distance margin `2.0`); (3) the 48d release sweep for state `0x10`.
- Red-test sketch: `TeamTick.TrapArmedAssignsTrapStateToNearestDefender` — with `T+0xE4` set and no possession, a defender inside the trap margin gets run state `0x3B`; with the latch set, it does not.

### Step 49 — `0x1404F79D8` → `FUN_140539ED0`

**Callee VA.** `FUN_140539ED0(brain+0x6BC8, match, side, T)`; the tick calls it only when `local_398 == 0` (the skip flag).

**Reads.** `MI+0x116C` (1 or 3 → different helpers; else the state is zeroed), `T+0x255` (the latch), `MI+0x119C`, `MI+0x1198`, `MI+0x1748` (on-pitch), `pl+0x55C`/`+0x564`.

**Writes.** `*(brain+0x6BC8+0xC) = match`, `*(brain+0x6BC8+0x38) = side`; the helper `FUN_140539F20` then clears the slot bit of `*param_1` when the slot is the pass target, and calls `FUN_140539CF0` per candidate.

**Gate.** `local_398 == 0` (call-site) and `MI+0x116C ∈ {1,3}` inside.

**Constants (script-read pairs, from the two helpers `FUN_140539F20` / `FUN_14053A3F0`).**
- `0x140539F67 MULSS XMM0,dword ptr [0x14258C264]` = `3.0` (`0x40400000`).
- `0x140539F6F MOVSS XMM0,dword ptr [0x142565600]` = `0.5` (`0x3F000000`).
- `0x14053A025 MULSS XMM0,dword ptr [0x142588200]` = `0.1` (`0x3DCCCCCD`).
- `0x14053A0A5 MOVSS XMM0,dword ptr [0x142565604]` = `2.0` (`0x40000000`).
- `0x14053A11E MOVSS XMM0,dword ptr [0x14259BDDC]` = `1.5` (`0x3FC00000`).
- `0x14053A0EE MOVSS XMM0,dword ptr [0x14259BDE4]` = `2.5` (`0x40200000`).
- `0x14053A3F0` (`FUN_14053A3F0`, the `MI+0x116C == 3` arm): `0x14053A49D` `[0x1425C48A0]` = `0.15000000596046448` (`0x3E19999A`); `0x14053A4A7` `[0x142565600]` = `0.5`; `0x14053A4B1` `[0x1425C6B00]` = `0.3799999952316284` (`0x3EC28F5C`).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the skip flag, the match phase, the latch, the pass target, the 11 slot bits.
- Outputs: the 11-bit candidate mask at `brain+0x6BC8` (bit cleared for the pass target), and the per-candidate decisions.
- Pieces: (1) the `MI+0x116C ∈ {1,3}` dispatch; (2) the "clear the pass target's bit" rule; (3) the else arm (`*param_1 = 0`).
- Red-test sketch: `TeamTick.PassTargetBitClearedFromCandidateMask` — the bit of the player in `T+0x78` is 0 in the mask afterwards.

### Step 50-stubs — `0x1404F79DF`–`0x1404F79A0E` (6 no-op calls)

**Callee VAs.** `FUN_140421F40(0)`, `FUN_140421F40(1)`, `switchD_142186f9a::caseD_24(0)`, `caseD_24(1)`, `caseD_24(2)`, `caseD_24(3)`.

**Reads / writes.** `FUN_140421F40` **returns 0 and writes nothing** (`dec/FUN_140421F40.c`: `undefined8 FUN_140421f40(void) { return 0; }`) — CONFIRMED. The `caseD_24` calls are the same stub shape (Ghidra names them from the switch dispatch, they are `return 0` bodies too — INF, their bodies were not separately fetched; the decompile shows them as no-ops with no writes).

**Gate.** none — they are unconditional, in sequence.

**SIM vs non-SIM.** non-SIM (no-ops). Skip with one line: *"Step 50 is six unconditional no-op calls (`FUN_140421F40` returns a constant 0 and writes nothing); nothing to port."*

**Port contract.** none.

### Step 52 — `0x1404F7A3E` → `FUN_14053C6E0`

> **Round 2 (§5.1) reduced all six downstream helpers.** The full per-helper description, the
> field map and the port contract are in **§5.1**. Summary: `brain+0x6D28+0` is the "this side is the
> ball side" flag, `+4` the tactic level 0..3, `+8` the ball-zone class 0..5, `+0x20` the class
> maximum, `+0x28..0x40` the class histogram, `+0x0C` five picked candidate ids, `+0x48` an event
> counter, and `T+0x40`/`T+0x44` the restart-pending flag and the restart taker id.

**Callee VA.** `FUN_14053C6E0(brain+0x6D28, match, T)`.

**Reads.** `T+0x254` (the side sign, via `FUN_140A48A30`), `pl+0x55C`/`+0x564` (positions), `pl+0x5D0`, the nearest list.

**Writes.** `*(brain+0x6D28+4) = (float)T+0x254` (the side sign as a float), and `FUN_14063A420(brain+0x6D2C+…)` writes three ints (the three pitch reference points: `FUN_140A62180(match, side, 1/0/2)`).

**Gate.** none at the call site.

**Constants.** `0`/`1`/`2` (the three reference points).

**SIM vs non-SIM.** SIM (the three ints are the side's pitch reference triangle read by the shape code).

**Port contract.**
- Inputs: the side sign (`T+0x254`), the three side pitch reference points.
- Outputs: `brain+0x6D28+4` (the sign float), `brain+0x6D2C+…` (three ints).
- Pieces: (1) the sign copy; (2) the three `FUN_140A62180` calls; (3) the downstream `FUN_14053CB40/C790/D250/C890/CC00/D3B0` helper calls (each **OPEN** — not fetched this pass).
- Red-test sketch: `TeamTick.SidePitchReferenceTripleCopied` — the three ints equal the side's reference points.

### Step 53 — `0x1404F7A5C` → `FUN_140A408C0`

**Callee VA.** `FUN_140A408C0(brain+0xBB8 /* `param_1+0xBB8` */, match, side, T)` — note the tick passes `*(brain+0xBB8)` as the first argument.

**Reads.** `*(brain+0xBB8)+0x3F88` (a float, the interpolation rate), `match+0xE0` (via `FUN_140A3E150(…, 5)`), `MI+0x116C`, `T+0x298` (a per-slot position triple), `T+0x8E00` (the sheet's player id), `T+0x254` (the side sign), `*(brain+0xBB8)+0x7DA0` (an 11-int array), `*(brain+0xBB8)+0x3F08` (the destination triple array).

**Writes.** The destination triple array `*(brain+0xBB8)+0x3F08 + 3*i` — three floats per slot: the position is lerped toward the target with the rate `*(brain+0xBB8)+0x3F88`.

**Gate.** per slot: `T+0x8E00 == side*0xB + i` → copy verbatim; else if `0.0 < rate` → the lerp (with the `piVar7[6]`/`MI+0x116C` exceptions); else copy.

**Constants (script-read pairs).**
- `0x140A408F0 MOVSS XMM8,dword ptr [RCX+0x3F88]` — the rate, read from `brain+0xBB8+0x3F88` (not a constant).
- `0x140A40922 MOVSS XMM7,dword ptr [0x14253EE30]` = `100.0` (`0x42C80000`).

**The lerp algebra (CONFIRMED raw dis, `0x140A40AEE`–`0x140A40B42`).**
```
XMM6 = dst_x            ; 0x140A40A78  MOVSS XMM6,dword ptr [RSP+0x48]
XMM1 = src_x            ; 0x140A40AEE  MOVSS XMM1,dword ptr [RBX-0x2E0]
XMM1 = src_x - dst_x    ; 0x140A40AFA  SUBSS XMM1,XMM6
XMM0 = rate * 100.0     ; 0x140A40AFE  MULSS XMM0,XMM7   (XMM0 = XMM8 = rate)
XMM1 = (src_x-dst_x) * rate*100.0
XMM1 = XMM1 / 100.0     ; 0x140A40B06  DIVSS XMM1,XMM7
XMM6 = dst_x + XMM1     ; 0x140A40B0A  ADDSS XMM6,XMM1
```
So `dst += (src − dst) * rate` exactly — the `*100.0 / 100.0` is a **no-op** that only exists to keep the value in a fixed-point-friendly form. CONFIRMED. The z component repeats the same pattern at `0x140A40B0E`–`0x140A40B42`.

**SIM vs non-SIM.** SIM (it is the position smoothing of the 11 slot targets).

**Port contract.**
- Inputs: the 11 slot target triples (`T+0x298`), the destination triples, the interpolation rate.
- Outputs: the smoothed destination triples.
- Pieces: (1) the "own slot → verbatim copy" rule; (2) the lerp with the rate; (3) the "rate ≤ 0 → copy" rule.
- Red-test sketch: `TeamTick.SlotTargetsLerpTowardTheSheet` — with a rate above 0 the destination moves part-way to the sheet target; with rate 0 it is copied.
- Note: the exact lerp algebra is **PARTIAL** — the decompile's `((dst - src) * rate * 100.0) / 100.0` is what the raw dis must confirm; the write target `*(brain+0xBB8)+0x3F08` is CONFIRMED from the decompile, the base `brain+0xBB8` is CONFIRMED from the tick's `MOV` (the decompile prints `*(undefined8 *)(param_1 + 3000)` = `brain+0xBB8`).

### Step 55 — `0x1404F7A85` → `FUN_140A42A70`

**Callee VA.** `FUN_140A42A70(T, T_side)` where `T_side = FUN_140A48A30(G, side)` (the team record for the side).

**Reads.** `T+0xA2A0[0..10]` (the 11 run states) and the ring buffer at `T+0xA2CC` (stride `0x34`, count at `+0x30`).

**Writes (CONFIRMED raw dis).** `RBX = RDX + 0xA2CC` (the *other* team's ring base, `0x140A42AA1 LEA RBX,[RDX+0xA2CC]`), `RBP = RCX + 0xA2CC` (this team's ring base, `0x140A42AAB`), and `R15 = RCX − RDX` (`0x140A42AA8 SUB R15,RDX`). The copy is `MOVUPS [RBX] → [R15+RBX]` etc. (`0x140A42ADD`–`0x140A42AFC`), i.e. the destination address is `RCX − RDX + (RDX + 0xA2CC) = RCX + 0xA2CC` — **this team's own** `T+0xA2CC` block. So the ring entries are copied **within** this team's `T`, from the tail of the ring to the head — not to the other team. Then `FUN_140A401F0(RBP + i*0x34, state)` pushes the current state into the ring's `+8 + count*0xC` slot (`0x140A42B01 MOV EDX,dword ptr [R12]` with `R12 = RCX+0x21C`).

Correction to the earlier reading: the destination is **this** team's `T+0xA2CC`, not the other team's. CONFIRMED raw dis.

**Gate.** per slot: `10 < i` **OR** `T+0xA2A0[i] != 0x25` → the copy runs; otherwise (i.e. slot in state `0x25`) it is skipped. The copy only happens when the ring's head id (`FUN_140A401C0`) equals the slot's state.

**Constants.** `0xB` (11), `0x34` (ring record stride), `0xC` (ring entry stride), `0x25` (the skipped state).

**SIM vs non-SIM.** SIM.

**Port contract.**
- Inputs: the 11 run states, the ring buffer at `T+0xA2CC`.
- Outputs: the ring compaction inside this team's `T+0xA2CC`; the ring push of the current state.
- Pieces: (1) the "state != 0x25" copy rule; (2) the head-id match; (3) the ring push via `FUN_140A401F0`.
- Red-test sketch: `TeamTick.StateTwentyFiveIsNotMirrored` — a slot in state `0x25` does not get a ring entry.

---

## §3 Open items (this pass)

1. Step 40f (`FUN_1404F8C70`, 31 KB) and step 40h (`FUN_1404FAD30`) — bodies read but not reduced; their outputs are not tied to named `T` fields beyond the ones listed.
2. Step 52's six downstream helpers (`FUN_14053CB40`, `FUN_14053C790`, `FUN_14053D250`, `FUN_14053C890`, `FUN_14053CC00`, `FUN_14053D3B0`) — not fetched.
3. **Live confirmation** of the `T+0x78` writer (step 40c) — the writer is now found statically; the live write-watch is still open (teamtick §4.2 item).
4. Step 47's *semantic* label for `T+0xB6DE` — the branch structure is CONFIRMED, the English name ("which side controls the ball") is **INF(h)**.
5. Step 25's returned byte (`local_397`) meaning — it selects the restart branch, but the underlying rule is still **INF**.
6. Step 30/31's stack snapshot layout — the exact field order of the 9×`0x40` record copy was not re-derived field by field; only the source offsets are named.
7. `FUN_1404F7AD0` (step 6) reads the player trait offsets `+0x13A8`/`+0x13AC`/`+0x13C8` — the trait **names** are unknown (raw offsets only).
8. Step 20's `T+0x8D50` angle — the exact meaning (a lead angle in radians? a normalised value?) is **INF(h)**; the arithmetic is CONFIRMED but the units are not.

## §3b Priority: fields this pass found that other slices read

Ranked by "a slice in `pes-re-team-layer.md` §3 reads this field and cannot work without it":

| field | writer (step) | readers |
|---|---|---|
| **`T+0x78`** (designated pass target) | **step 40c `FUN_1404FB2A0` → `FUN_1404FB380`/`FUN_1404FB780`** (NEW — closes teamtick §4.2) | step 48a `FUN_1404FC4D0`, step 45, step 21, `FUN_1404F61E0` (step 35), `FUN_1404F8C70` (step 40f) |
| **`T+0xE4`** (offside-trap armed) | **step 5 `FUN_1404F6720`** | step 48b `FUN_14057BA50` |
| **`T+0xB6DE`** (contest flag) | **step 47 `FUN_140509170`** | **§5.6: the *opponent's* player AI, `FUN_140928710` at `0x140928CF6`** (it releases the AI request when the flag is set) |
| **`T+0x218`** (no-ball counter) | **step 11 `FUN_1404FEDC0`** | step 43 (the `T+0x218 == 0` release sweep) |
| **`T+0x6D0`** (phase code 4/5/6) | **step 14 `FUN_1404F6680`** | not yet read anywhere — **new field, GF has no counterpart** |
| **`T+0x8C40[]` / `T+0x8C35[]` / `T+0x8D50[]`** | **step 20 `FUN_1404FF8B0`** | **corrected in §5: `T+0x8C35[]` has 8 readers; `T+0x8C40[]` and `T+0x8D50[]` have none** |
| **`T+0xB8A8` / `T+0xB8AC`** (shape-dispatch mode) | **step 15 `FUN_1404FA840`**, step 6 `FUN_1404F7AD0`, step 40b `FUN_14053B110` | step 40b's mode dispatch |
| **`T+0xB8B4`** (build-up mode) | **step 17 `FUN_1404FBCA0`** | step 42 `FUN_140538050` (mode 1..6) |
| **`T+0xB8B8`** (11-bit slot mask) | **step 19 `FUN_140500200`** | shape/licence code |
| **`T+0xB8D0`** (restart receiver) | **step 21 `FUN_1404F7BA0`** | step 21 itself (the previous value), restart code |
| **`T+0x8984/87/8A/8D`** (role census) | **step 27a-pre `FUN_1404FBB70`** | shape/role code |
| **`T+0xB7EC[]`** (opponent mask) | **step 35 `FUN_1404F61E0`** | press/duel code |
| **`T+0x6E7`** (cannot-contest byte) | **step 48a `FUN_1404FC4D0`** | step 40c `FUN_1404FB380` |
| **`T+0xB6DC`** | **step 40e `FUN_14057C190`** | **corrected in §5: `T+0xB6DC` is only READ by 40e, not written by it** |
| **`T+0x130`** (kick target id) | **step 29 `FUN_140537AC0`** | not identified this pass |

---

# §5 Round 2 — closing the §3 open items

Second pass over the same image, same bridge, same base `0x140000000`. New tools:
`patscan3.py` (a disp32 memory-operand scanner over the `.trace` executable section, offset
`0x600`, size `0x252D800`), cross-checked against a full linear-disassembly scan
(`xrefscan.py`) and against the bridge's disassembly output.

## §5.1 Step 52 — the six helpers, reduced (CONFIRMED)

`FUN_14053C6E0(brain+0x6D28, match, T)` — the disassembly (`dec/dis_14053C6E0.txt`, 46 lines) shows it is a
**pure dispatcher**: it stores the side sign and the three pitch reference points, then calls six
helpers in a fixed order with the same three arguments `(ctx, match, T)`:

| order | call site | callee | what it does | writes |
|---|---|---|---|---|
| 0 | `0x14053C720` | `FUN_14063A420(ctx+8, match, side)` | the three pitch reference points | `ctx+8`, `ctx+0xC`, `ctx+0x10` (3 ints) |
| 1 | `0x14053C725` → `0x14053C733` | `FUN_14053CB40` | **"is this side the one playing the ball?"** | `ctx+0` (int counter) |
| 2 | `0x14053C738` → `0x14053C741` | `FUN_14053C790` | **the tactic/pressure level 0..3** | `ctx+4` (int) |
| 3 | `0x14053C746` → `0x14053C74F` | `FUN_14053D250` | the **ball-zone class 0..5** + a 6-slot history | `ctx+0x20` (class), `ctx+0x24` (max), `ctx+0x28..0x40` (6 ints) |
| 4 | `0x14053C754` → `0x14053C75D` | `FUN_14053C890` | the per-slot candidate table (5 ints/slot) and 5 picks | `ctx+0x0C`, `ctx+0x44`, … |
| 5 | `0x14053C762` → `0x14053C76B` | `FUN_14053CC00` | the **event-driven override** of the class | `ctx+8`, `ctx+0x48` |
| 6 | `0x14053C770` → tail `JMP 0x14053D3B0` | `FUN_14053D3B0` | the **kick-off taker pick** | `ctx+0x14`, `T+0x40`, `T+0x44` |

**`FUN_14053CB40` (helper 1) — CONFIRMED raw dis (`dec/dis_14053CB40.txt`).**
It reads the match's current ball-owner record (`FUN_140A3E7D0` → `FUN_140A60530` → the owner id) and
compares it with the side:
- owner record valid **and** owner id == side → **the side is playing the ball**;
- else if the side has **no possession** (`T+0x255 == 0`) **and** has **no designated pass target**
  (`T+0x78 > 0x15`), it re-reads the match's *other* record (`FUN_140A3E190(uVar1,0)+4`) and compares
  that with the side;
- only then, and only when `MI+0x116C == 1` (`0x14053CBEA CMP dword ptr [RBP+0x116C],0x1`), it does
  `INC dword ptr [RBX]` (`0x14053CBF3`).
Otherwise `MOV dword ptr [RBX],0x0` (`0x14053CBA2`).
So **`ctx+0` is a 0/1 "this side is the ball side" flag**, recomputed each tick.

**`FUN_14053C790` (helper 2) — CONFIRMED raw dis (`dec/dis_14053C790.txt`).**
Writes `ctx+4` (the tactic level) from the attack level `T+0xB8A4`:
`0 → 0`, `1 → 0`, `2 → 1`, `3 → 2`, `4 → 3` (`0x14053C7D2`–`0x14053C80B`).
Two overrides:
- `0x14053C81B`: a random gate `rand*12.0 + 0.5` (`0x14259BE04` = `12.0`, `0x142565600` = `0.5`)
  whose value must be **less than** `ctx+0` (the ball-side flag) and the side must be on the wrong
  half (`side_sign * ball_x < 0.0`) → `ctx+4 = 3` (`0x14053C857`);
- `0x14053C85E`: `T+0xB8A2 != 0` (full attack) → `ctx+4 = 3`.
The `ctx+0 == 1` case first sets `ctx+4 = 1` (`0x14053C7CB`) before the level map overwrites it.

**`FUN_14053D250` (helper 3) — CONFIRMED decompile.** Computes the **ball-zone class** into `ctx+0x20`:
- `ctx+0 == 0` (the side is not the ball side) → the class and the whole 6-slot history are zeroed;
- else the class is 1 (`the ball is on this side`), 2 (`the ball is past the halfway line`,
  `*ball_x / 3.0 < side_sign * goal_x`), 4 (`a corner-ish flag bit 11 of `FUN_140A72E70`),
  or 5 (`and the ball is inside the goal mouth, `goal_z > ball_z``).
`ctx+0x24` keeps the maximum class seen; `ctx+0x28..0x40` is a 6-slot counter ring (one per class).

**`FUN_14053C890` (helper 4) — CONFIRMED decompile.** For each of the 11 on-pitch slots
(`MI+0x1748` set, `MI+0x175E` clear) it fills **5 ints** (5 × `0x1C`-stride records) and then, for each
of 5 output slots, collects the candidates whose weight (`ctx+0x0C + 0x14*slot`) is `> 0.0`, picks the
best by `FUN_1421B7D40` and stores the id (`0xFF` if none).

**`FUN_14053CC00` (helper 5) — CONFIRMED decompile.** Walks the match's **event list**
(`FUN_140A3E570`) and overrides the class from the events:
- event type `0` with `piVar12[5] == 3` (a shot?) and `MI+0x1558 == side` → `ctx+8 = 3`;
- event type `7` (a pass) → the class is 1 or 2 depending on the sign of the receiver's z;
- event type `0x0C` with `piVar12[5] == side` → `ctx+8 = 3` and `ctx+0x48 = 0`;
- event type `0x18` → `ctx+0x48` counts "the event belongs to this side and the side is ahead".
So **`ctx+8` is the event-driven zone class** and `ctx+0x48` an event counter.

**`FUN_14053D3B0` (helper 6) — CONFIRMED decompile.** The **kick-off / restart taker pick**:
- `ctx+0x10 != 0` → it counts up (`ctx+0x14++`) and, when the countdown expires, clears
  `ctx+0x10` and `T+0x40`;
- when idle, it walks the match's event list for an event of type `0x10` with `MI+0x1554 == 1` and
  `MI+0x1558 == side`, then picks the **nearest** eligible player to the ball
  (`FUN_140A8F890`) that is not `MI+0x173C` and is not already commanded
  (`FUN_140A638C0`), storing it in `T+0x44` and setting `T+0x40 = 1`.

**Step 52 port contract (updated).**
- Inputs: the side sign (`T+0x254`), the ball position/owner, the attack level (`T+0xB8A4`),
  the full-attack flag (`T+0xB8A2`), the match event list, the 11 on-pitch slots.
- Outputs: `brain+0x6D28+4` (the sign float), `brain+0x6D2C` (three pitch reference ints),
  `brain+0x6D28+0` (ball-side flag), `brain+0x6D28+4` (tactic level 0..3),
  `brain+0x6D28+8` (zone class 0..5), `brain+0x6D28+0x20` (class max),
  `brain+0x6D28+0x28..0x40` (class histogram), `brain+0x6D28+0x0C` (5 picked candidate ids),
  `brain+0x6D28+0x48` (event counter), `T+0x40`/`T+0x44` (restart pending + taker id).
- Pieces: (1) the ball-side flag (`FUN_14053CB40`); (2) the tactic level map + its two overrides
  (`FUN_14053C790`); (3) the zone class + event override + taker pick (`FUN_14053D250` /
  `FUN_14053CC00` / `FUN_14053D3B0`).
- Red-test sketch: `TeamTick.TacticLevelFollowsAttackLevel` — attack level 3 gives tactic level 2 and
  full-attack (`T+0xB8A2`) forces level 3; a non-ball side gives the ball-side flag 0.
- Stand-in naming: `// ponytail: GF match-object replaces PES match+0xE0 event list (FUN_140A3E570 walk).`

## §5.2 Step 40f — `FUN_1404F8C70` reduced (CONFIRMED)

**Writes (one only):** **`T+0xBAB4 + 4*(column*0x21 + row)`** — an int array of `0x21` columns × rows,
written at `0x1404F9CC8 MOV dword ptr [RAX + RCX*0x4 + 0xBAB4],EDX` (the base `RAX = [R15+0x6D50]`
is the team record `T`). CONFIRMED raw dis.

**Gate:** the whole body is inside `MI+0x116C == 1` (`dec/dis_1404F8C70.txt` first test).

**What the number is.** The value written is `CVTTSS2SI EDX, XMM13` (`0x1404F9CB9`) — a **float
truncated to an int**, and the float is an accumulated **cost** (`fVar42`), which starts at
`DAT_14259BE34` = `50.0` and is reduced by every penalty term:
- the direct interception cost (distance-weighted, with the `8.0` / `10.0` / `4.0` band ramps);
- the "target is behind the goal line" cost;
- the "target is a marked man" cost (`T+0x8C1F` / `T+0x8CE8`).
The final `fVar42` is compared against `3.0` (`DAT_14258C264`) in places and the int is compared
against `0x32` = 50 by a consumer.

**The consumers (CONFIRMED, disp32 scan of `0xBAB4` in `.trace`):**
- `0x14059967A` — `MOVD XMM0,dword ptr [RBX + RDX*0x4 + 0xBAB4]` then `CVTDQ2PS` and
  `COMISS XMM1, XMM0` with `XMM1 = 20.0` (`0x14259BE1C`) → "is the cost above 20?" gate.
- `0x14059D733` — `MOV r,rm` of the same array.
- `0x140672132` — `CMP dword ptr [RAX + RDX*0x4 + 0xBAB4], 0x32` → "is the cost ≥ 50?" gate.
All three index it as `row*0x21 + column` (the same `IMUL RDX,RCX,0x21` seen at
`0x140599673` and `0x140672125`).

**Reading.** `T+0xBAB4` is a **per-column interception-cost table** (0x21 = 33 columns; the row index
is the "target slot" and the column index the "attacker slot" — the 11×11 own-team pairing, padded).
The value is the *cost of attacking that column*, in the same units as the `50.0` starting budget.

**Step 40f port contract.**
- Inputs: the 11 own players' positions, the 11 opponents' positions, the ball position, the
  run states, the `T+0x8C1F` / `T+0x8CE8` marking flags, the attack level.
- Outputs: `T+0xBAB4[0x21*row + column]` (the interception cost per attacker/target pair).
- Pieces: (1) the 33-column cost matrix; (2) the cost-reduction terms (the `8.0`/`10.0`/`4.0` bands,
  the `6.0` cover drop, the `50.0` budget); (3) the truncation to int.
- Red-test sketch: `TeamTick.InterceptionCostIsZeroForAnUnmarkedPair` — a pair with no marking flags
  keeps the `50.0` budget (int 50) while a marked pair is reduced.
- Stand-in naming: `// ponytail: GF marking flags replace PES T+0x8C1F / T+0x8CE8 (FUN_1405A4830 / FUN_140A638C0).`

## §5.3 Step 40h — `FUN_1404FAD30` reduced (CONFIRMED)

**Writes (one only):** **`T+0xC128`** — a **16-bit mask**, one bit per own slot.
Three write sites, all in this callee: `0x1404FAD4B` (clear, at entry), `0x1404FAFDA` and
`0x1404FB228` (the final value).

**Gate.** `FUN_140A50250(match, side, 0) > 0` (the side has a restart), then
`side_sign * ball_x >= -9.0` (`DAT_1425A9680` = `-9.0`), the ball is within the pitch box, and
`FUN_140A50250(match, side, 1) < 1 && FUN_140A50250(match, side, 2) < 1` (no other restart pending)
or the ball is inside the goal area.

**The algorithm (CONFIRMED decompile + raw dis).**
1. The mask starts as all 11 bits set (`0x1404FAE7B`: a `BTS` loop over 11 bits).
2. The ball carrier is `FUN_140A627E0(match, 0)`; if it is off-pitch the mask is written unchanged.
3. `FUN_140A4E720` is called twice with the carrier's facing angle and the two candidate aim points;
   if **both** succeed the function **returns early without writing** (the mask keeps its all-ones value).
4. Otherwise the candidate receiver `FUN_140A62960(match, side == 0)` is taken; if it is off-pitch
   the function returns.
5. `FUN_140A53DA0` is called twice (a "can this pass be played" test) with the constants
   `130.0` (`0x1425A9BF0`) / `8.5` (`0x1425C48B4`) and `70.0` (`0x14259BE4C`) / `8.5`.
6. The final loop clears a bit for every own player that is **too far** or **cannot receive**:
   `FUN_14043A0A0` (the travel-time estimate) `<= 90.0` (`0x14259BE5C`) **and**
   `FUN_140A4FEA0(match, side == 0, goal, time, 120.0, 6.5, 0, 1) == 0` → clear the bit
   (`0x1404FB0xx`: `uVar15 = uVar15 & ~(1 << (uVar8 & 0x1f))`).
7. The mask is written to `T+0xC128`.

**So `T+0xC128` is the "which of my players are eligible to receive the restart" mask** — one bit per
own slot, all set by default, cleared for players who cannot reach or cannot receive.

**Readers of `T+0xC128`** (disp32 scan of `0xC128` in `.trace`): `0x140417514` (the side-to-side copy
routine, `MOVZX EAX, word ptr [RSI+0xC128]` / `MOV word ptr [RDI+0xC128],AX`),
`0x140607434` and `0x14061D716` (`MOVZX r, rm16`), `0x140A41A7B` (a writer),
`0x1421547DC` and `0x14215D80C` (outside the game code region).
The two game-side readers are in the restart/kick-off handling.

**Step 40h port contract.**
- Inputs: the restart type and its aim point, the ball carrier and its facing, the 11 own players'
  positions and receive-ability, the ball position.
- Outputs: `T+0xC128` (the 11-bit receive-eligibility mask).
- Pieces: (1) the mask default (all ones) and the early-out when the carrier can already pass;
  (2) the per-player clear rule (travel time `90.0`, receive test `120.0`/`6.5`); (3) the write.
- Red-test sketch: `TeamTick.RestartMaskClearsPlayersWhoCannotReceive` — a player who fails the
  receive test has its bit cleared while the others keep theirs.
- Stand-in naming: `// ponytail: GF restart aim point replaces PES FUN_140A4E720/FUN_140A53DA0 aim geometry.`

## §5.4 Step 40e — `FUN_14057C190` reduced (CONFIRMED), and a correction

**Correction.** The earlier note said step 40e writes `T+0xB6DC`. It does **not**: `T+0xB6DC` is only
**read** at `0x14057C2F8`, `0x14057C329` and `0x14057C366` (`CMP byte ptr [RCX+0xB6DC],0x0`), i.e. it is
a flag 40e consults. The disp32 scan of `0xB6DC` in `.trace` finds **no** writer anywhere — so
**`T+0xB6DC` is written outside the team tick** (its writer is in the side-planner / match-setup path).
Correcting the §1 table entry accordingly.

**What 40e actually writes.** The disassembly shows only four stores, all in one helper
(`0x14057C510`–`0x14057C561`), and their base is `RDX` = the **match-object shared block**
(`FUN_14042A7B0` → `*(brain+0x6D58+4) + 0x1040`), **not** the team record:
- `[RDX + R8 + 0x9EC]` (byte) — a per-player flag;
- `[RDX + R8 + 0x9F0]` (int) — a per-player int;
- `[RDX + R8 + 0x9F4]` (byte) — a per-player byte;
- `[RDX + R8 + 0xA04]` (uint) — a per-player id (it is copied verbatim from the same block).
The stride is `0x1C` (`lVar21 = bVar17 * 0x1C`), so the block is a **22-entry × 0x1C record array**
(a per-player "restart assignment" table).

**The body.** For each of the 11 own slots whose run state is `0x25` (or `0x2C` with the
`T+0x288` byte == `3`) and whose `T+0x4C00` counter is non-zero, it rewrites the run state to `0x2C`
(`FUN_140A42940`), then sets the "position" via `FUN_140A42A30(lVar24, slot, 3|1|0)` and finally
issues one of three orders (`0xB1`, `0x172`, `0xB3`) through `FUN_140A40800`. The order chosen
depends on `T+0xB6DC` and on `FUN_140A62EA0` (a player id being valid).
The second half of the function is a **copy-back**: it reads the shared block for each player, flips
the `iStack_124` flag between 0 and 1 depending on `T+0xA1A0 + slot*4 == 0x2C` and the roster's
"has a set piece" predicate `FUN_140A55D40(roster, side, 3)`, and writes the record back.

**Step 40e port contract.**
- Inputs: the 11 run states, the `T+0x4C00` counters, the `T+0x288` sub-state bytes, the shared
  per-player block, the roster's set-piece predicate.
- Outputs: the run state `0x2C` (and the orders `0xB1`/`0x172`/`0xB3`) and the shared block's
  per-player records.
- Pieces: (1) the `0x25`/`0x2C` run-state rewrite; (2) the three-way order choice; (3) the
  copy-back with the flag flip.
- Red-test sketch: `TeamTick.StateTwentyFiveBecomesTwentyTwoC` — a slot in state `0x25` with a
  non-zero counter is left in `0x2C` after the tick.

## §5.5 Step 20 — the `T+0x8D50` units, and the `T+0x8C35` / `T+0x8C40` readers

**`T+0x8D50` has NO reader.** Two independent scans of the `.trace` executable section agree:
- the full linear-disassembly scan (`xrefscan.py`, 300 s over 0x252D800 bytes) returns exactly
  **one** memory operand with disp32 `0x8D50`: the writer at `0x1405001C2`;
- the disp32 pattern scan (`patscan3.py`) returns the same single site.
So `T+0x8D50[slot]` is **written and never read** — it is a *diagnostic* field (or is read by code
outside the executable image). The **units question is therefore moot**: the value is dead.

**The value's own range (for the record).** `XMM13` at the write site is the result of
`MINSS XMM0, XMM13` (`0x1404FFE26`), i.e. a value clamped to the earlier `XMM13`, which was itself
`100.0` (`0x14253EE30`, `0x1404FFB8D`) — so the stored float is in **[0, 100]**, a **cost/weight
in the same 0..100 scale as the other team-eval scores**, not an angle. In the per-slot path the
stored value is either that clamp result or the `/(-3.5)` rescale of `XMM13 - XMM10`
(`0x1405001A4 DIVSS XMM13,[0x14259D658]` = `-3.5`), i.e. a **normalised difference**.

**`T+0x8C35[slot]` (the per-slot "found a target" byte) has EIGHT readers.** The disp32 scan
(`0x8C35`) returns, besides the writer at `0x140500038` and the clear at `0x1405001E0`:
`0x1404F8055` (step 21's own `FUN_1404F7BA0`), `0x14052C061`, `0x14054C8C2`, `0x1405574DD`,
`0x1405603B4`, `0x1405632A9`, `0x140563DDD`, `0x140570295`, `0x140957F68`, `0x140959A83`,
`0x140A1E57B`, `0x140A3060E` — all of the form
`CMP byte ptr [T + slot + 0x8C35], 0` / `CMP byte ptr [T + slot + 0x8C35], reg`.
The base is confirmed as the team record in at least two of them:
`0x14052C054 CALL 0x140A48A30` → `RCX` = the team record for the side, then
`0x14052C061 CMP byte ptr [RCX+RAX*0x1+0x8C35],BPL`.
So **`T+0x8C35[slot]` is a live "this slot has a target" flag** consumed by the run/state code.

**`T+0x8C40[slot]` (the target id) has NO reader in the team tick.** The scan returns the writer
`0x140500040`, the clear `0x1405001E9`, and one site `0x1408A7F10`
(`MOV qword ptr [RBX+0x8C40],RAX`) — but that site writes a **pointer** (`RAX = 0x142599B18`) and
its base `RBX` is the function's own `RCX` argument, **not** the team record; it is a different
object that happens to have a `+0x8C40` field. The other `0x8C40` hits in the scan are `LEA r,m`
forms in the `0x1408A7xxx` region — the same unrelated object.
**Correction to §5's earlier claim**: `T+0x8C40` and `T+0x8D50` have no reader; only `T+0x8C35` does.

**Step 20 port contract (corrected).**
- Inputs: the latch, the pass target, the ball/player positions and velocities, the pitch line
  bounds, the 24-entry roster.
- Outputs: `T+0x8C35[11]` (**the live "has a target" flag**), `T+0x8C40[11]` (the target id,
  `0xFF` = none), `T+0x8D50[11]` (a 0..100 diagnostic score, **write-only**).
- Pieces: (1) the latch + carrier gate; (2) the per-slot candidate scan (on-pitch → roster record
  not substituted → interpolate with `rand*0.1 + 0.5`); (3) the score clamp to `[0, 100]`.
- Red-test sketch: `TeamTick.HasTargetFlagSetOnlyForAnEligibleSlot` — a slot that passes the
  roster and on-pitch tests has `T+0x8C35[slot] == 1`; a slot that fails has 0 and `T+0x8C40[slot] == 0xFF`.

## §5.6 Step 47 — the `T+0xB6DE` semantic label (CONFIRMED, was INF(h))

**`T+0xB6DE` is a per-side byte that says "this side is the one the ball is contesting"** — and it is
read by the **opponent's** player AI.

**The writer** is `FUN_140509170(match, side)`, called from the tick at `0x1404F797B` with
`EDX = brain+8` (the side) and `RCX = RBX` (the match), and the result is stored to the **writing
side's** `T+0xB6DE` at `0x1404F7987 MOV byte ptr [RCX+0xB6DE],AL` (base `RCX = [RDI+0x6D50]` = `T`).

**The reader** is `FUN_140928710(player_ctx, player_rec, flag)` at `0x140928CF6`
`CMP byte ptr [R15+0xB6DE],0x0 / 0x140928CFE JNZ 0x140928E49`.
`R15` is `FUN_140A48A30(G, bVar17)` where `bVar17` is **the *other* side** (`0x1409287A2 MOV EDX,ESI`
→ `FUN_140A48A30`): the decompile confirms `lVar9 = FUN_140a48a30(DAT_1436f3dc8, bVar17)` with
`bVar17 = true` (side 1) when the player's id is `> 10`.
So the reader consults **the opponent's** `T+0xB6DE`.

**What the reader does.** `FUN_140928710` is the "should this player keep its AI request" gate; it is
called from `FUN_140929A00` at `0x140929A94`:
```
cVar7 = FUN_140928710(...);
if (cVar7 == 0) FUN_14092A010(...);   // no AI request
else                FUN_14092A340(...);  // raise an AI request
```
and `FUN_140928710` returns `true` on many conditions, one of which is
`*(char *)(lVar9 + 0xB6DE) != 0` (`dec/FUN_140928710.c:236`).

**Therefore:** `T+0xB6DE` is **"this side is in a contested-ball situation"** — the side that the
*other* side's players must react to. When it is set, the opponent's players stop holding their
current AI request and are released to make a new decision (`FUN_14092A340`).

**How the writer decides (CONFIRMED decompile of `FUN_140509170`).**
- `MI+0x116C != 1` → **0**.
- The ball is on the side's attacking half (`side_sign * ball_x > 0`) → **0** (the side is not
  contesting; it is the one in possession).
- The side's current player is not the ball carrier → the nearest-list lookup runs; if a player of
  *this* side is much closer to the ball than the current one (`DAT_1425A2670` = `400.0` distance²)
  the function returns **1** (the side is contesting).
- The angle test around the ball (`29.0`–`32.0` window, `DAT_14259BE28` = `32.0`,
  `DAT_1425A9460` = `29.0`) decides 0 vs 1.
- `FUN_140A4FEA0(match, side, ball, angle, …)` returning non-zero → **0**.

**Step 47 port contract (semantic now CONFIRMED).**
- Inputs: the ball position and owner, the side's current player, the side sign, the nearest-list
  tables, the pitch box, the match phase.
- Outputs: `T+0xB6DE` (byte: "this side is contesting the ball").
- Pieces: (1) the `MI+0x116C != 1 → 0` arm; (2) the "side is in possession → 0" arm; (3) the
  nearest-player and angle tests that return 1.
- Red-test sketch: `TeamTick.ContestFlagSetOnlyWhenTheSideIsChallenging` — a side with the ball
  has `T+0xB6DE == 0`; a side whose player is markedly closer to the ball than the current holder
  has `T+0xB6DE == 1`.
- **Cross-slice note (new):** the opponent's player AI releases its AI request when this flag is set.
  Any port of `FUN_140928710` / `FUN_140929A00` must read the *other* side's flag.

## §5.7 §3 open items — status after round 2

| §3 item | status |
|---|---|
| 1. step 40f / 40h bodies not reduced | **CLOSED** — §5.2 (40f → `T+0xBAB4` interception-cost table) and §5.3 (40h → `T+0xC128` receive-eligibility mask) |
| 2. step 52's six helpers not fetched | **CLOSED** — §5.1 (all six reduced, with their writes) |
| 3. live confirmation of the `T+0x78` writer | still **OPEN** (static writer found in round 1; the live write-watch was not run this pass) |
| 4. `T+0xB6DE` semantic label | **CLOSED** — §5.6 ("this side is contesting the ball"; read by the *opponent's* player AI) |
| 5. step 25's `local_397` meaning | still **OPEN / INF** |
| 6. step 30/31's snapshot field order | still **OPEN** (the source offsets are named; the per-field order inside the 0x40 record is not) |
| 7. step 6's trait names (`+0x13A8/+0x13AC/+0x13C8`) | still **OPEN** (raw offsets only) |
| 8. step 20's `T+0x8D50` units | **CLOSED, and moot** — §5.5: the field has **no reader**; it is a write-only 0..100 score |
| new | step 40e's `T+0xB6DC` claim was **wrong** — §5.4 corrects it: 40e only *reads* `T+0xB6DC`; the field has no writer in the executable image |
| new | §3b's "no reader found" for `T+0x8C40[]` / `T+0x8C35[]` was **wrong** — §5.5: `T+0x8C35[]` has 8+ readers; `T+0x8C40[]` and `T+0x8D50[]` really have none |

## §4 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`) for every callee named in this report; ~90 fetches against `http://127.0.0.1:8080` (round 1) + ~40 more (round 2).
- Float constants read by script from `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (`readf.py`, `consts.py`), never typed. Each constant is paired with the instruction VA and the operand VA the disassembly shows.
- **Self-check** (`<workdrive>/gf-TeamDecode-scratch/check_report.py`, run against this file, after round 2):
  ```
  checked 43 value/bits claims, 0 mismatches
  checked 78 instruction/operand pairs, 0 mismatches
  cited code VAs not backed by a disassembly or a verified instruction: 0
  ```
  The checker verifies each `VA | value (bits)` row and each `instruction [operand]` = `value` claim against the image bytes and against the disassembly text, and that every cited code VA lies inside a disassembled instruction (or, for the five sites Ghidra has no function head for, that the instruction bytes at that VA really carry the claimed disp32).
  **Mutation-tested — all three checks fire:**
  - a wrong table value (`999.0` for `9999.0`) → `BAD 0x1425c48e8 value claimed 999.0 actual 9999.0`;
  - a wrong value at an instruction/operand pair (`9.0` for `8.0` at `0x1404F6853`) → `BAD 0x14259bdf4 value claimed 9.0 actual 8.0`;
  - a cited VA with no disassembly (`0x140501170`) → `cited code VAs not backed by a disassembly or a verified instruction: 1`.
  The file was restored after each mutation and re-verified clean. The VA-existence check was itself
  found vacuous in round 1 (it matched 8-hex-digit prefixes of 9-digit VAs) and was rewritten to
  span instruction bytes plus a raw-image fallback before the mutation test above.
- **Field xref scanner** (`<workdrive>/gf-TeamDecode-scratch/patscan3.py`): a disp32 memory-operand scanner over the `.trace` executable section (file offset `0x600`, size `0x252D800` — the only section with code; the PE section table was parsed to confirm the others are `.rdata`/`.data`/`.reloc`/etc.). It validates the ModRM/SIB bytes so a disp32 is only reported when it really is a memory displacement. Cross-checked against an independent full linear-disassembly scan (`xrefscan.py`); both agree on `0x8D50` having exactly one site.
- Step numbering cross-checked against `docs/re/pes-re-team-layer.md` §1 and `docs/re/pes-re-teamtick.md` §2.2; the call-site VAs in this report come from this pass's own decompile/disassembly of `FUN_1404F6C30` and match §1 exactly (no new call sites).
- Scratch: `<workdrive>/gf-TeamDecode-scratch/`.
- No GF code changed, no GF build/test run (read-only RE assignment), no PES write/pin/input.

