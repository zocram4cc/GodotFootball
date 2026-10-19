# PES 2021 loose-ball PRESS lifecycle: writers, ActionPress, DELAY, C3 unit (PressLife)

Read-only RE. Scratch: `<workdrive>/gf-PressLife-scratch/` (`d_*.c` decompiles).
Tags: **CONFIRMED** = read in the decompile and checked against disassembly bytes or install bytes.
**INF** = inference with reason. VAs are PES2021 image VAs (base 0x140000000).
Bridge: `http://127.0.0.1:8080` (`/decompile_function`, `/disassemble_function`).
`E8` scans of `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − base).

Closes `docs/re/pes-re-first-touch.md` §10 items 1 (planner, PRESS release path) and 3
(arrival/C3 units), and §7.1/§7.5/§9 corrections.

## 0. Float table (install bytes, CONFIRMED)

| VA | value (f32 bits) | use |
|---|---|---|
| `0x14259BE44` | `64.0` (`0x42800000`) | press-writer angle window 64°; first-touch predicate C3 test `< 64` |
| `0x14259BE40` | `57.29578` (`0x42652EE0`) | rad→deg factor in `FUN_140439EF0`/`FUN_140439E50`/`FUN_14045D8B0` |
| `0x14259BE5C` | `90.0` (`0x42B40000`) | press-writer angle window 90° |
| `0x14259BE20` | `25.0` (`0x41C80000`) | press-man pick ramp span 25 m |
| `0x1425C6780` | `81.0` (`0x42A20000`) | d² = 81 (9 m) press radius |
| `0x1425C6784` | `155.0` (`0x431B0000`) | press-writer angle window 155° |
| `0x1425C6788` | `169.0` (`0x43290000`) | loose-ball d² = 169 (13 m) gate |
| `0x14259BE0C` | `15.0` (`0x41700000`) | press-man score scale 15 |
| `0x14259BE18` | `18.0` (`0x41900000`) | press-man pick radius base 18 m |
| `0x1425A1FF8` | `12.5` (`0x41480000`) | press-man score scale 12.5 |
| `0x1425A8A28` | `0.05` (`0x3D4CCCCD`) | speed-class cut 1 |
| `0x1425A1FFC` | `13.5` (`0x41580000`) | speed-class cut 3 |
| `0x1425C48C0` | `22.0` (`0x41B00000`) | speed-class cut 4 |
| `0x14259BDF4` | `8.0` (`0x41000000`) | speed-class cut 2; `local_24C` top-man radius 8 m |
| `0x14259BDF0` | `7.0` (`0x40E00000`) | press-man pick ramp rate 7 |
| `0x14259BDEC` | `6.0` (`0x40C00000`) | MATCH_UP 6 m near-ball return |
| `0x14259D608` | `7.84` (`0x40FAE147`) | C3-writer `fVar12` angle floor |
| `0x14259D610` | `12.25` (`0x41440000`) | C3-writer `DAT_14259D610` angle ceiling |
| `0x14259D634` | `10000.0` (`0x461C4000`) | non-MATCH_UP score penalty |
| `0x14259D654` | `-3.0` (`0xC0400000`) | press-man score rate divisor |
| `0x14253EE30` | `100.0` (`0x42C80000`) | score angular knee 100°; advance 100 m |
| `0x1425CEE20` | `462.25` (`0x43E72000`) | candidate-table radius² (21.5 m) |
| `0x1425CEE24` | `250000.0` (`0x48742400`) | MATCH_UP score penalty |
| `0x1425CEE28` | `-145.0` (`0xC3110000`) | angle-code window |
| `0x1425CB488` | `145.0` (`0x43110000`) | angle-code window |
| `0x1425C48D4` | `115.0` (`0x42E60000`) | angle-code window |
| `0x1425C6608` | `-55.0` (`0xC25C0000`) | angle-code window |
| `0x1425AA1BC` | `75.0` (`0x42960000`) | DELAY-gate distance 75 m |
| `0x142572348` | `0.8` (`0x3F4CCCCD`) | C3-writer advance scale |
| `0x14258C264` | `3.0` (`0x40400000`) | press-writer distance ramp 3 |
| `0x1425656C0` | `4.0` (`0x40800000`) | press-writer distance ramp 4; arrival flag |
| `0x142565600` | `0.5` (`0x3F000000`) | arrival writer base 0.5 |
| `0x14259222C` | `5.0` (`0x40A00000`) | ActionPress 5 m touch-frame step; arrival writer 5 m knee |
| `0x142592228` | `0.25` (`0x3E800000`) | carrier-arm window rate 0.25 |
| `0x142590D98` | `0.3` (`0x3E99999A`) | carrier-arm window base 0.3 |
| `0x14259B89C` | `0.4` (`0x3ECCCCCD`) | carrier-arm window 0.4 |
| `0x142588200` | `0.1` (`0x3DCCCCCD`) | no-carrier arm `k = fps·0.1+0.5` rate |
| `0x142588210` | `10.0` (`0x41200000`) | score distance knee 10 m; arrival writer 10 m |
| `0x14259BE30` | `45.0` (`0x42340000`) | angle-code window |
| `0x14259BE34` | `50.0` (`0x42480000`) | second-presser bearing window |
| `0x14259BE38` | `55.0` (`0x425C0000`) | angle-code window |
| `0x14259BE60` | `135.0` (`0x43070000`) | angle-code window |
| `0x14259BE6C` | `160.0` (`0x43200000`) | C3-writer 160° fold |
| `0x14259A764` | `60.0` (`0x42700000`) | score angular span 60° |
| `0x14259858C` | `360.0` (`0x43B40000`) | wrap period |
| `0x142598580` | `180.0` (`0x43340000`) | wrap threshold |
| `0x142588218` | `-1.0` (`0xBF800000`) | wrap negative branch |
| `0x14256AE1C` | `1.0` (`0x3F800000`) | wrap positive branch |
| `0x142565D30` | abs mask qword `0x7FFFFFFFFFFFFFFF` | `\|x\|` |
| `0x1425795E4` | `2π` (`0x40C90FDB`) | atan2 negative wrap |
| `0x14257A194` | `0.7` (`0x3F333333`) | ActionPress formation-shift 0.7 |
| `0x14259BE58` | `85.0` (`0x42AA0000`) | SAND-writer 85° knee |
| `0x14259BE1C` | `20.0` (`0x41A00000`) | SAND/arrival 20° knee |
| `0x1425C48F8` | `-25.0` (`0xC1C80000`) | arrival-writer divisor |
| `0x14259BDD0` | `0.69444` (`0x3F31C71C`) | arrival velocity floor `0.6944/fps` |
| `0x142596E6C` | `80.0` (`0x42A00000`) | arrival-writer 80° window |
| `0x142565604` | `2.0` (`0x40000000`) | arrival-writer 2 m knee |

`VA | bits | value` per §0: each row gives VA, f32 bits, value. `DAT_142565D30` is the
8-byte abs mask (`0x7FFFFFFFFFFFFFFF`), read as two u32 halves in the decompiles.

## 1. Setter `FUN_140A42940` (VA `0x140A42940`, CONFIRMED)

Signature: `void FUN_140a42940(longlong T, int slot, u32 v)`.

```
T.u32[+0xA2A0 + slot] = v                                        # the state array
FUN_140A40200(T + 0xA2CC + 0x34*slot, v, T.u32[+0x21C])          # 4-deep history ring
FUN_140A85AA0(T + 0x28C + 0x40*slot, v)                          # per-slot target block
```

`FUN_140A40200` (CONFIRMED decompile): 4-entry ring at `+0x30` index; a repeat value
is a no-op; otherwise advances the index only when the stored stamp `< T+0x21C`,
then writes `(value, T+0x21C, T+0x21C)`. So the history keeps the last 4 distinct
states with the team-tick stamp.

`FUN_140A85AA0` (CONFIRMED decompile): zeroes the 0x40-byte target block
(`T+0x28C+0x40·slot`, 4 u64 words), then per value: `1` → sets marker words;
`0x1D..0x1F` → `0x10100` patterns; `0x22` → goal-kick-like pattern; `0x27` → clears.
For PRESS/DELAY/MATCH_UP (`0x33/0x34/0x3A`) it only zeroes — no target is pre-set
by the setter; the target comes from the per-pass point writers (`T+0x26C`,
`T+0x27C`, `T+0x28C` blocks) and `FUN_140639100` at action time.

`E8` scan: 63 direct callers (+2 in the `0x143F…` VM region). State immediates at
the defence-relevant sites (capstone, CONFIRMED asm):

| call site | owner | `r8d` (state) |
|---|---|---|
| `0x140536059` | `FUN_140535C60` dispatcher loop | `0x33` DELAY / `0x34` PRESS (`test al,al; jne` selects) |
| `0x1405373F3` | `FUN_140536FC0` core | `0x34` PRESS (score-0 table) |
| `0x1405377D3` | `FUN_140536FC0` core | `0x34` PRESS (`bVar4` true arm) |
| `0x14053780A` | `FUN_140536FC0` core | `0x33` DELAY (`bVar4` false arm) |
| `0x140537966` | `FUN_140536FC0` tail | `0x33` DELAY (`T+0xA548[0]` of pair) |
| `0x140537985` | `FUN_140536FC0` tail | `0x34` PRESS (`local_24C` top man) |
| `0x14053647A`/`0x1405365C3`/`0x14053667E` | `FUN_140536210` | `0x3A` MATCH_UP (all three) |
| `0x1405367B2`/`0x14053681A`/`0x1405369F4` | `FUN_140536690` head | `0` reset (PRESS-state men not in `T+0xA548`) |
| `0x140536F07` | `FUN_140536690` tail | `0x34 − (gate != 0)` (PRESS or DELAY) |
| `0x140534C50`/`0x140534FA9` | `FUN_140534A20` mark pass | `0x36` MARK |
| `0x140535006` | `FUN_140534A20` mark pass | `0x32` COVER promote |
| `0x1406345AD`/`0x140634993`/`0x140634A3A` | `FUN_140634470`/`FUN_140633940` | `0x34` PRESS |
| `0x1406349E0` | `FUN_140634470` | `0x35` SAND |
| `0x1406368A8` | `FUN_140635AB0` | `0x3D` PASS_COURSE_CUT |
| `0x140637711`/`0x1406377C3`/`0x14063788F` | `FUN_1406377F0` | `0x3C` GOAL_COVER |
| `0x140631A44` | `FUN_1406315B0` | `0x32` COVER |

Non-defence owners (bridge owner query, CONFIRMED): set-piece writers
(`FUN_140525DF0/EC0/60C0/61A0/6270` → `0x69/0x6A/0x6B` wall/support runs;
`FUN_14057B190/BA50/BAF0/C190/CBC0/CF40/D240/F720/FDE0/80630` → `0x25/0x3B/0x23/
0x2C/0x6D` offside-trap/corner/kick states; `FUN_140539CF0/53AA90/53AE60` →
`0x10/0x2D`; `FUN_1405415B0/465FB0/5654B0` → `0x22` PASS_SUPPORT;
`FUN_14056CB00` → `0`; `FUN_14061EF90/F110` → `0x2A/0x2E` runs).
`0x1404FD150` → `0x2E`. Three sites (`0x140540F2D/0x14055CC3C/0x140601C2C/
0x14061C087/0x14061CA6A/0x14061CC84/0x14061DDA3`) sit in undefined code (bridge
"no function found") — OPEN, all outside the defence region.

## 2. Priority gate (CONFIRMED, corrects defshape §2)

`FUN_140A40000(cur, new, idx, T) = FUN_140A40110(new,…) < FUN_140A40110(cur,…)`
(CONFIRMED decompile). `FUN_140A40050` switch (CONFIRMED decompile):

- prio 0: `0x22/0x23/0x24/0x25/0x2C/0x2D/0x2E/0x3A/0x3B`
- prio 2: `0x1D/0x1E/0x1F/0x20/0x21/0x27/0x28/0x2A/0x2B/0x33/0x34/0x35/0x3C`
- prio 3: `0x10/0x26/0x2F/0x36/0x37/0x69/0x6A/0x6B`
- prio 4: `0x22…` no — prio 4: `0x22`? re-read: `0x22/0x32/0x3D/0x6D` (CONFIRMED
  decompile lines 31–35; note `0x22` PASS_SUPPORT is prio **4**, and the prio-0
  `case 0x22` line in the raw listing is the switch fallthrough display —
  the decompile's `return 0` arm lists `0x23/0x24/0x25/0x2C/0x2D/0x2E/0x3A/0x3B`
  only. Correction: defshape §2's prio-0 row wrongly includes nothing extra;
  it lists exactly these; `0x22` is prio 4.)
- prio 5: default (0 NOTHING, 1 BASE_POSITION, keeper/fall states, `< 0x10`,
  `> 0x6D`).

`FUN_140A40110` (CONFIRMED): returns the table value directly when `< 3`; for
`≥ 3` re-requesting the slot's current state returns 2 when `cur == 1
BASE_POSITION` with flag `T+0x29C+0x40·slot` set, or `cur == 0x22 PASS_SUPPORT`
with `T+0x290+0x40·slot` set.

Consequences for PRESS (CONFIRMED by the table):

- PRESS/DELAY/SAND/GOAL_COVER (prio 2) beat MARK/DELAY_MARK (3), COVER/PCC (4),
  NOTHING/BASE (5). They never beat MATCH_UP/OFFSIDETRAP (0) or each other
  through the gate (`2 < 2` false) — PRESS↔DELAY switches are direct writes,
  not gated upgrades.
- The gate is only consulted by writers that call `FUN_140A40000` first (mark
  pass, cover pass). The PRESS/DELAY writers (`FUN_140536FC0/36690/34470/
  33940`, dispatcher loop) call `FUN_140A42940` unconditionally — they overwrite
  whatever is there, including MATCH_UP, except where they explicitly test for
  `0x3A` and queue to the secondary slot instead (below).

## 3. Press writer `FUN_140536FC0` (VA `0x140536FC0`, CONFIRMED decompile+asm)

Signature: `void (brain, {ctx, ?, side}, T)`. Runs every frame from
`FUN_140535C60` (`0x140536002`), after `FUN_140536210` (MATCH_UP),
`FUN_140634470` (PRESS/SAND), `FUN_140536690` (NOTHING/PRESS+k).

### 3.1 Head gates (CONFIRMED)

1. Clear `T+0xA548[0..3]` entries `< 0x16` → `0xFF`; `T+0xA558` → `0xFF`.
2. `T+0x78 < 0x16` (we have a designated man): return — no press search.
3. `FUN_141B92460(2)` stub returns 5; `< 5` returns (never).
4. Keeper hold `FUN_140A53F90 > 0`: return.
5. Opponents' `FUN_140A62690` man in anime class `0x41/0x42`: return.
6. `MI+0x116C != 1` (not in play): return.
7. Newest event kind in {1,2,3,0x10} (else `FUN_140A6CE10(newest, carrierValid,
   0)` must be true): then pressure score `FUN_140A51320(ctx) < 3` returns.
   `FUN_140A51320` (CONFIRMED decompile): carrier valid → `FUN_140A3E840()`;
   else `MI.frame − newest.frame` (frames since the newest event).
8. Per-slot body filter (11 slots): on-pitch, fall/action class open
   (`FUN_140A6E150/5E0/340/580` with `human+8` bits 22/9), clip-window
   `human+0x1A ≤ human+0x1C` with `+0x1A ≥ frame − newest.frame`: any eligible
   man → return (the pass only runs when every man is filtered out — INF: this
   is the "dead-ball / restart" shape; the exact semantics are OPEN).
9. Concept flags `T+0xF1 | T+0xB712 | T+0xB715` all zero: return (block ends,
   nothing written).
10. Team level `FUN_140A604D0(level, 6)` true: skip the match-object `0x10`
    branch (else the `0x1423E7FD0` object must be mode `0x10`, else return).
11. `T+0xB70C != 0`: return.
12. Carrier valid (`≤ 0x15`): skip the loose-ball 13 m gate; else ball-sample
    `S0` vs ball-now d² `> 169` (`0x1425C6788` = 13²) returns, and restart kinds
    {1,2,3,0x10} need `FUN_140A6CE10(newest, carrierValid, 0)` true.

### 3.2 Core: score-0 table → PRESS (CONFIRMED)

`FUN_140639830(ctx, side, T, &tab)` fills the candidate table (§5). Every entry
with score-code 0 (slots `local_200[4..14]`, code = `FUN_140639600` result):

```
FUN_140A42940(T, slot, 0x34)                                   # 0x1405373F3
T.i32[+0xA548 + k] = side·11 + slot                            # k = table index
T.f32[+0xA55C + k] = FUN_140535260(angle of (ball→slot + 90°·k)) # 0x140537414
```

`FUN_140535260(out, ctx, slot, k)` (CONFIRMED decompile): ball→slot bearing +
`90·k` degrees, wrapped to `[0,360)`. So `T+0xA55C[k]` is the bearing from the
ball to the k-th score-0 man, stepped 90° per index (INF: arrival-ring slots).

### 3.3 Core: `FUN_140639A20` pick → PRESS vs DELAY (CONFIRMED)

`FUN_1404567E0(zeros)` then `FUN_140639A20(ctx, side, &tab2, zeros)` (§6).
Per shortlisted man (up to 11, `local_250`; stops after 3 picks,
`local_204 > 2` breaks):

- `local_208` byte-1 set (the `+0x38`-flag/`+0x48`-flag/`r15` branch) and score
  `!= 2` → the `LAB_1405375E9` arm: man not yet scored.
- Else the PRESS-vs-DELAY test at `0x1405376xx–0x140537749` (CONFIRMED asm
  shape, decompile lines 265–311):
  - `bVar4` (press-wins flag): true unless `local_258 == 0` and the ball-velocity
    bearing `FUN_1404A53E0(FUN_140A57720(ballVel))` fails `FUN_140A92B90(·, 60)`
    (i.e. ball moving toward the man within 60°) — then `iVar14 == 0`
    (first-list man) needs `(dist + 0)² ≤ d²(ball, man)` else `bVar4` stays true.
    In short: the first-list man presses only if he is at least as close as the
    (ball + 0) point (INF: the ball's own spot); other men always pass this gate.
  - `T+0xB709 != 0` skips the angle branch (goes to `LAB_140537749` directly).
  - Else (CONFIRMED asm `0x140537700–0x140537749`): own-frame line id
    `T+0x87D0[slot] != 0`, or bearing `FUN_140439EF0(man, ball-now)` vs C3 pair
    value `fVar7 = 64.0` with `wrap180(90 − bearing)` in
    `(25, 155]` degrees (`0x14259BE20`/`0x1425C6784`, `||` with the `fVar10 =
    155 ≤ wrap` leg) → `LAB_140537749`. Otherwise falls to `LAB_1405375E9`
    (the not-scored arm).
  - At `LAB_140537749` (CONFIRMED asm):
    - slot in MATCH_UP (`T+0xA2A0[slot] == 0x3A`) and `local_258 == 0`:
      `FUN_140A42B80(T, slot, 0)` when `iVar14 == 0` (first-list man keeps
      MATCH_UP, records `local_248`), else `FUN_140A42B80(T, slot, 0x34)`
      (queue PRESS behind his MATCH_UP) when `bVar4`, or `FUN_140A42B80(T,
      slot, 0x33)` (queue DELAY) with `local_254 = man`.
    - else `bVar4` → `FUN_140A42940(T, slot, 0x34)` (`0x1405377D3`, PRESS);
      `!bVar4` → `FUN_140A42940(T, slot, 0x33)` (`0x14053780A`, DELAY) with
      `local_254 = man`.
  - `T+0xA548[rank] = man`, `T+0xA55C[rank] = FUN_140535260(…)` (arrival record),
    `local_200[rank] = 1`, `local_204++`.

### 3.4 Tail: runner-up + pair resolve (CONFIRMED)

- `T+0xF1` set, `local_204 < 2`, `local_248 < 0x16`: `FUN_140A42B80(T,
  local_248 % 11, 0x34)` — queue PRESS behind the first-list man's MATCH_UP.
- `local_24C < 0x16` (top man: `T+0xA548` entry with `FUN_14044E0D0 < 8.0`,
  `0x14259BDF4`) and `local_254 < 0x16` and different: `FUN_140A42940(T,
  local_24C % 11, 0x33)` + `FUN_140A42940(T, local_254 % 11, 0x34)`
  (`0x140537966/0x140537985`) — the pair is resolved to DELAY + PRESS.

### 3.5 What DELAY means (CONFIRMED structure)

DELAY (`0x33`) is written at exactly two places in this writer: the `!bVar4`
arm (`0x14053780A`) and the pair-resolve tail (`0x140537966`). A candidate gets
DELAY instead of PRESS when (a) he fails the press-wins test (first-list man
farther than the ball spot, or outside the 64°-pair / 25°–155° bearing window
with `T+0xB709 == 0`), or (b) he is the `local_24C` top man displaced by the
`local_254` pick. DELAY is prio 2, same as PRESS: it holds the slot out of
MARK/COVER/PCC and runs the same dispatcher-loop branch (§4), but no
`ActionPress` consumes it — the `0x140962440` action is dispatched on state
`0x34` only (zero `E8` callers; dispatched through the action table, INF: the
action id equals the state id, as with TRAP `0x878 = 7`). A DELAY man stands
off: he keeps his shape slot while the PRESS man goes. What consumes DELAY per
tick (which action function, if any) is OPEN — no DELAY action was found in
this pass.

## 4. Dispatcher loop `FUN_140535C60` +0x26 branch (CONFIRMED asm)

At `0x140536011–0x140536059`, after the four defence passes, per slot:

```
if T.i32[+0xB748 + slot] == 0x26 and T.i32[+0xA2A0 + slot] ∉ {0x33, 0x34}:
    idx = side·11 + slot
    v = FUN_14062AF10(ctx, &idx) ? 0x33 DELAY : 0x34 PRESS     # 0x140536049/53
    FUN_140A42940(T, slot, v)                                  # 0x140536059
```

`FUN_14062AF10(ctx, idx)` (CONFIRMED decompile `d_14062AF10_full.c`): 0 when the
man is already in `T+0xA548[0..3]`; carrier-branch returns 1 when the carrier's
geometry test passes (65 m `0x14259BE48`, 75 m `0x1425AA1BC`, 45° `0x14259BE30`
windows); else scans the 11 ball-distance-list entries `MA+0x2470C` and returns
1 when a PRESS-state (`pl+0x5D0 == 0x34`) man has the ball on his side of both
bearings and is nearer than the candidate. Returns 0 otherwise. So the loop
sends DELAY when another presser already has the geometry, else PRESS.
Cadence: every team tick (no `q` gate — the loop runs after the pass calls on
both the `T+0x255 != 0` and `== 0` paths; CONFIRMED asm `0x140535F54–0x14053606B`).

## 5. Candidate table `FUN_140639830` (VA `0x140639830`, CONFIRMED decompile)

Per own slot (`side·11+k`, 11 slots; on pitch; not the opponents'
`FUN_140A62690` man; d² to ball-now `≤ 462.25 = 21.5²`, `0x1425CEE20`):

- PRESS-state man (`pl+0x5D0 == 0x34`) with `T+0xB88C[slot] == 0` and (`lVar8 =
  table count ≤ 10` or `T+0xB88C` head zero): enrolled directly.
- Else: `FUN_140A41190(T, slot-idx)` must find him in `T+0xA548[0..3]`…
  (decompile line 44: `-1 < iVar5`, i.e. must be a listed arrival man — INF:
  the table re-enrols listed men) and `T+0xA558[slot] != idx` (not the stored
  man), and `FUN_140479470(G+0x180 list, idx)` nonzero (list gate, CONFIRMED).
- Enrolment: `tab.count++`, `code = FUN_140639600(…)`, arrival
  `FUN_14044E0D0(MA, idx) < 4.0` (`0x1425656C0`) sets `tab.flag`; `code == 0`
  sets `tab.has0`.

Score codes `FUN_140639600(ctx, T, slot, extra°)` (CONFIRMED decompile):
bearing ball→slot vs reference (straight-back `FUN_140A4DB20(dir)` =
`dir·(+90°)` float bits, or ball→goal bearing when `T+0xB712` set), plus
`extra`, wrapped to ±180°; returns 3 when `55° < |Δ| ≤ 145°`
(`0x14259BE38`/`0x1425CB488`); 2 when `|Δ| > 115°` (`0x1425C48D4`); 1 when
`55° < Δ ≤ 145°` positive-side only; else the 45°/135° window code
(`0x14259BE30`/`0x14259BE60`, `−55°`/`−145°` legs): 3 outside `[−145°,−55°]`,
else 1/0 by the 45° test. INF(med): goal-side/wide/behind classification.

## 6. Press-man pick `FUN_140639A20` (VA `0x140639A20`, CONFIRMED decompile)

Two sweeps over the MA ball-distance lists (`MA+0x22E4C + capped idx`,
outlet list `FUN_140A71540(MA, dir)`):

1. Per on-pitch man: score = angular term `clamp01((|Δ| − 100)/60)`
   (`0x14253EE30`/`0x14259A764`, wrap-folded, ±1 side weights) `+ 10·
   clamp01((dist − 10)/10)` (`0x142588210`); `+250000` (`0x1425CEE24`) when not
   MATCH_UP (`+10000`, `0x14259D634`, when MATCH_UP but `FUN_140A41190 < 0`);
   sorted by `FUN_140A79640` into `local_1D8` (INF: ascending — nearest,
   most goal-side first).
2. Per shortlisted man (own side's 11): skip the opponents' `FUN_140A62690`
   man and off-pitch; must be within `(18 + 7·r)²` of the ball
   (`0x14259BE18`/`0x14259BDF0`/`0x14259BE20`, `r` = the §3.3 ramp); the three
   `FUN_140639600` codes combine as `12.5·codeProd + 15`
   (`0x1425A1FF8`/`0x14259BE0C`); press if within that, else DELAY
   (CONFIRMED structure; the exact codeProd legs at lines 257–338 mix
   `iVar9/iVar10/iVar11` with the `T+0xF1 == 0 && code == 0 && d² > 100` and
   `pl+0x5D0 ∈ {0x33,0x34}` + arrival legs — not fully reduced, INF on the
   leg mapping).

## 7. Press target `FUN_140639100` (VA `0x140639100`, CONFIRMED decompile)

Signature: `float * (out3, ctx, slot*, mustPress?, flag?)`. Three arms:

- `flag != 0` (touch-frame arm): if the frame equals the newest event frame and
  its kind ∈ {1,2,3,0x10} and not (own `T+0x78 > 0x15` or `!= MA+0x24838`):
  `out = ball-now` (`MA+0x2482C/30/34`).
- Carrier valid (`MA+0x2601C < 0x16`): carrier arm (lines 57–108). Distance
  `S0-to-man` (S0 = `FUN_140A6CF70(BI, 0)`) sets a window `fVar16 = 0.3 +
  0.3·clamp01((d − 4)/8)` (`0x142590D98`/`0x1425656C0`/`0x14259BDF4`,
  `+0.3` when `FUN_140A75BE0(carrierPl) == 4`); carrier-velocity bearing
  (or `FUN_140638FB0` target when the carrier's trap window holds:
  `FUN_140A6E3F0(class)` and `0.4·human+0x1C < human+0x1A`); `out = S0`
  advanced by `(dist(BI0,BIk)/fps)·(fVar16·fps + 0.5)` ticks along that bearing
  (`FUN_140A8F7C0`). INF: leads the carrier's ball sample.
- Else + `mustPress` (ActionPress calls with `mustPress = carrierInvalid`):
  `k = fps·0.1 + 0.5` (`0x142588200`/`0x142565600`; fps = `FUN_141E532D0()`
  = 54 → `k = 5`, CONFIRMED arithmetic); `Sk = FUN_140A6CF70(BI, k)`;
  `out = ball-now` if `|Sk − B|² ≤ |Sk − A|²` else `out = presser base`
  (`AI(presser)+0x1414/18/1C`), where `B` = ball-now, `A` = presser base.
  CONFIRMED against the first-touch doc §7.1 (same reading).

So on a loose ball the target is ball-now or the presser's own base — never an
intercept point. (Corrects nothing; confirms first-touch §7.1.)

## 8. `ActionPress` `FUN_140962440` (VA `0x140962440`, CONFIRMED decompile+asm)

Signature: `float * (action+?, out3, ctxSlot)`. `param_3+4` = slot id,
`+0x38` = ctx, `+0x40` = planner owner. Runs each frame for a PRESS man
(dispatched on state `0x34`; zero `E8` callers — action-table dispatch, INF).

1. `target = FUN_140639100(…, mustPress = carrierInvalid)` (§7) into
   `local_E8`.
2. Formation-shift arm (`FUN_14234E8E0(matchObj) == 0x19`, mode 4,
   `FUN_140A610B0` target): bearing difference of (man→target) vs (man→anchor)
   `> 10°` (`0x142588210`) shifts the target `0.7·(anchor − man)`
   (`0x14257A194`) (CONFIRMED asm+decompile).
3. `action+0x74 = 0`; if `action+0x58 == 0` the stored point `action+0x68`
   takes `target`.
4. If `action+0x5C != 0` (INF: has-base flag): `target = AI(man)+0x1420`
   (the presser's base; `local_88 = AI rec`, `+0x1420/24/28`).
5. Touch-frame arm (`MI.frame − newest.frame < 1`, CONFIRMED asm
   `0x140962A56–0x140962AB4`): if `action+0x58 == 0`: stored point = man pos,
   advanced 5 m (`0x14259222C`) along man→target (`FUN_140439E50` +
   `FUN_140A8F7C0`); return. Else copy stored→out; return. (A 5 m step per
   touch frame — confirms first-touch §7.1.)
6. Else: `out = target`; bearing man→target (`FUN_140439E50`);
   `local_18 = (FUN_140A75B20(pl) == 2)`; `eta = FUN_140A4D350(BallInfo,
   target)`; `cls = FUN_140A75BE0(pl)`; `n = FUN_140678950(plannerOwner, pl,
   target, cls, local_18, 2, bearing, 0, 1, 0)`; `action+0x30 = (n + 10 <
   eta)`. (CONFIRMED decompile lines 141–150 + asm.)
7. Stop/arrival arm (`action+0x48 < 0x16`, i.e. a marked peer exists,
   CONFIRMED decompile lines 161–202 + asm `0x1409629xx`):
   `dMan = |man − ball-now|`, `dPeer = |peer − ball-now| + 0.5`
   (`0x142565600`); if own `T+0xB8D0 == peer` and `|S0 − base| < |S0 −
   ball-now|` (S0 = `FUN_140A57800(BI)` sample): `out = base`, clear
   `action+0x78`, return. If `dPeer ≤ dMan`: keep `action+0x78` if nonzero,
   `out = FUN_1406378F0(ctx, slot, target, action+0x78, …)` (the arrival
   writer rewrites the target in place); `action+0x78 = result`; zero →
   clear. Else clear `action+0x78`.
8. Write-back stored→`action+0x68` when they differ; return `out`.

Per-tick cadence: the whole body runs each frame the man is in PRESS (no
internal tick gate; the target is recomputed every call). Arrival/stop: no
distance arrival test inside ActionPress itself — stopping is (a) the state
leaving `0x34` (§9), or (b) the `FUN_1406378F0` rewrite converging the target
onto the man (returns 0 → `action+0x78 = 0`, target stays). The planner flag
`action+0x30` (`n + 10 < eta`) is an INF: re-plan signal (not named).

## 9. Planner `FUN_140678950` + `FUN_1406788B0` (CONFIRMED, body open → closed)

`FUN_140678950(owner, pl, target, cls4B, flag4B, mode4B, bearing, u8, n8, n8)`
is a pure passthrough (CONFIRMED asm, 13 insns): shuffles the 10 args and tail-
calls `FUN_1406788B0(owner, target, cls, flag, mode, u8, n8)`.

`FUN_1406788B0(owner, target3, cls{u32}, flag{u32}, mode{u32}, u8, n)`:

```
if owner == 0: return 0
speed = cls.word? (decompile: ECX=[RAX] = *cls, EAX=[R9] = *flag → FUN_1406B9270)
ok = FUN_1406B9270(owner, &dist, &flagOut, mode /*1..4, 0→1*/, target,
                   u8, *cls, *flag, n)
if !ok: return 0
return max(1, int(fps · dist))                                    # fps=54
```

(CONFIRMED decompile + asm; `FUN_1406B9270` itself not reduced — it fills
`dist`/`flag` through `FUN_140707EE0`, the movement solver. So the planner
returns an ETA in ticks (`fps·dist`), clamped ≥ 1. The `n + 10 < eta`
comparison in ActionPress is therefore ticks + 10 frames vs ball ETA ticks
(INF: re-plan when the man's ETA exceeds the ball's by 10).)

Speed class `FUN_140A75BE0(pl)` (CONFIRMED decompile; cuts CONFIRMED against
`pes-re-stamina-drain.md:94`):

```
if pl.u8[+0x3F4] == 0: return 0
v = pl.f32[+0x3FC + 16·pl.u8[+0x108]]Own (km/h)
v ≤ 0.05 → 0; ≤ 8 → 1; ≤ 13.5 → 2; ≤ 22 → 3; else 4
(0.05 @0x1425A8A28, 8 @0x14259BDF4, 13.5 @0x1425A1FFC, 22 @0x1425C48C0)
```

`FUN_140A75B20(pl)` (CONFIRMED): `pl+0x3F4 == 0 → 1`, else
`pl.u32[+0x40·(pl+0x108)+…]` (stamina-adjacent word; semantics OPEN).

## 10. Who clears/overwrites PRESS `0x34` (CONFIRMED)

In-tick order inside `FUN_140535C60` (CONFIRMED asm): `FUN_140534A20` (MARK) →
`FUN_140628400` (mark spots) → `FUN_140635AB0` (PCC) → [`FUN_140536210`
(MATCH_UP) → `FUN_140634470` (PRESS/SAND) → `FUN_140536690` (NOTHING/PRESS+k)
→ `FUN_140536FC0` (PRESS/DELAY)] → dispatcher loop (§4) → `FUN_140628400`
(spots) → `FUN_140525B50` (FK wall).

| writer | what it does to a PRESS slot | when |
|---|---|---|
| `FUN_140536FC0` core/tail | writes PRESS again, or DELAY (`§3.3–3.4`) | every frame, gated §3.1 |
| dispatcher loop `0x140535C60` | keeps (skips state ∈ {0x33,0x34}) | every frame |
| `FUN_140536690` head (`0x1405367B2/81A/9F4`) | `FUN_140A42940(0)` reset when the PRESS/DELAY man is not in `T+0xA548[0..3]` (`FUN_140A41190 < 0`), incl. the `param_1+0x5C` mismatch leg | every frame, before 6FC0 |
| `FUN_140536690` mid (`0x1405369E6` leg) | `FUN_140A42940(other, 0)` clears a stale PRESS on the matched man | same pass |
| `FUN_140536690` tail (`0x140536F07`) | `FUN_140A42940(slot, 0x34 − (gate != 0))` — rewrites PRESS or flips to DELAY | same pass |
| `FUN_140536210` MATCH_UP | resets every `0x3A` when no mark target matches (`FUN_140A40820` loop); writes `0x3A` on the carrier man (prio 0, beats PRESS next tick only if the PRESS writer stops renewing it) | every frame |
| `FUN_140634470`/`FUN_140633940` | `0x34` PRESS / `0x35` SAND overwrites (direct `FUN_140A42940`, no gate) | every frame |
| `FUN_140534A20` mark pass | MARK (prio 3) never overwrites PRESS (prio 2) through the gate; no-target MARK men promote queued COVER or reset — never touch PRESS | every frame |
| `FUN_1406315B0` cover pass | clears COVER only; skips PRESS men (accepts COVER-or-MARK candidates, queues behind MARK) | q = 1 |
| `FUN_140635AB0` PCC pass | `0x3D` (prio 4) never overwrites PRESS through the gate | every frame |
| `FUN_1406377F0` GOAL_COVER | `0x3C` (prio 2) direct write, same level as PRESS | q = 1 |
| queued secondary `FUN_140A42B80` | PRESS/DELAY queued at `T+0x278+0x40·slot` behind a MATCH_UP man; promoted when the mark pass drops his MATCH_UP (defshape §2) | — |
| ball owned / he touched it | no direct clear: when `T+0x78 < 0x16` the 6FC0 head returns (stops renewing) and the 6690 head resets unlisted PRESS men to 0; contact itself is the TRAP reaction taking the anime action (§11) | — |
| distance | 13 m loose-ball gate (`0x1425C6788`) stops renewal; 8 m `local_24C` top-man rule (§3.4); `(18+7r)` pick radius (§6); 21.5 m table radius (§5) | — |

So a presser stops pressing when (a) the writer stops renewing and `FUN_140536690`
resets him, (b) MATCH_UP/SAND/GOAL_COVER/second-presser overwrites him, (c) the
pair tail flips him to DELAY, or (d) TRAP takes his body. There is no single
"clear PRESS" path — renewal + reset is the lifecycle.

## 11. C3 pair unit: DEGREES (CONFIRMED)

Writer `FUN_14045D8B0` (VA `0x14045D8B0`, CONFIRMED decompile + asm tail):

- Per reference man `a` (newest ring-1 key actor, else holder `MA+0x2483C`):
  base `MA+0x22E4C + 200·a`.
- Sweeps 180 bearings (`local_res8` 0..179, step 1°): probe point = ball-now
  `S0` advanced 100 m (`0x14253EE30`) along the bearing (`FUN_140A8F7C0`).
- Inner loop over both team buckets (11+11): reads the stored C3 value
  `+4+(rank+11·team)·8` (same addressing as `FUN_1404C8EA0`), skips when
  `> 12.25` (`0x14259D610`); computes the man's bearing from `S0`
  (`FUN_140A92D40(dx,dz)`, +2π fold, `× 57.29578`) and compares
  `|probeBearing − manBearing|` against `15°` (`0x14259BE0C`) and `90°`
  (`0x14259BE5C`), with the `7.84°` floor (`0x14259D608`) and `160°`
  (`0x14259BE6C`) fold; `FUN_140A907C0(…, 0.8)` advance test
  (`0x142572348`).
- Stores one byte per bearing at `MA+0x24C6A + k` (`0xB4` = 180 entries):
  **1 when the bearing is blocked, 0 when open** (CONFIRMED asm
  `0x14045DD11/0x14045DD22`: `XOR SIL,SIL` on the advance-test pass = open,
  else `SIL` stays 1).

Wait — the `+4+(rank)·8` float the reader returns is read but only compared
with `12.25`; the writer loop above writes only the `0x24C6A` byte map. The
float records themselves (`MA+0x22E4C + 200·a + 4 + slot·8`) are filled
elsewhere in the same function family (the decompile's `puVar22` window shows
the reader addressing, not a float store — no `MOVSS [r14…]` store appears in
the 298-insn disassembly). Hmm. Re-check: the disassembly has no store to
`[r14]` at all, so `FUN_14045D8B0` maintains the `0x24C6A` open/closed bearing
map, addressed with the same rank scheme — and the C3 float the predicate
reads is written by a sibling not yet identified.

What settles the unit anyway (CONFIRMED): every consumer compares the C3 value
against degree constants with bearing arithmetic:

- first-touch predicate: `FUN_1404C8EA0(MA, oth, s) < 64` with `64.0` =
  `0x14259BE44`, the same VA the press writer uses as its 64° window
  (`wrap180(90 − bearing)` vs 64/90/25/155 — §3.3, CONFIRMED asm).
- `FUN_14045D8B0` itself thresholds the stored value against `12.25°` and
  folds bearings against `15°/90°/160°`.
- `FUN_140439EF0` (used on the same fields) returns `atan2·57.29578`
  (CONFIRMED decompile: `FUN_140A92D40` + `× DAT_14259BE40`).

**C3 pair value unit: degrees (CONFIRMED).** The predicate tests
"pair bearing < 64°". First-touch §3.5/§7.3/§10-item-3 "unit OPEN/INF" is
closed; §7.3's "same constant is an angle window in the press writer" is
confirmed as the same VA `0x14259BE44`.

## 12. State-transition table (PRESS/DELAY/MATCH_UP + overwriters)

Prio: MATCH_UP/OFFSIDETRAP 0 > PRESS/DELAY/SAND/GOAL_COVER 2 > MARK/DELAY_MARK 3
> COVER/PCC 4 > NOTHING/BASE 5 (§2). Direct writers ignore the gate; gated
writers (`FUN_140A40000`) only upgrade.

| from \ to | PRESS 0x34 | DELAY 0x33 | MATCH_UP 0x3A | SAND 0x35 | MARK 0x36 | COVER 0x32 | PCC 0x3D | GC 0x3C | 0 |
|---|---|---|---|---|---|---|---|---|---|
| PRESS | renew (6FC0/6690-tail/34470) | pair tail; 6690-tail gate | never (0 beats 2; 6FC0 queues 0x34 behind) | 34470 overwrite | never (3 < 2 false) | never | never | 377F0 overwrite | 6690-head reset |
| DELAY | 6FC0 re-pick; dispatcher keeps | renew | never | 34470 overwrite | never | never | never | 377F0 overwrite | 6690-head reset |
| MATCH_UP | queue 0x34 behind (42B80); direct 42940 when `local_258 != 0` | queue 0x33 behind | renew (6210) | — | mark pass keeps | queued path | — | — | 6210 reset loop (40820) |
| MARK | 6FC0/34470 overwrite direct | 6FC0 overwrite direct | 6210 overwrite | 34470 overwrite | renew | queued only | never (4 < 3 false) | overwrite | mark-pass reset |
| COVER | overwrite direct | overwrite direct | overwrite | overwrite | never (mark pass promotes queued COVER instead) | renew (q=1) | — | — | 6315B0 clear |
| NOTHING/BASE | all writers | all writers | 6210 | 34470 | mark pass | cover pass | PCC pass | 377F0 | — |

## 13. Loose-ball port spec (chase → contact → latch), mapped to GF

Chase (replaces `elizacontroller.cpp:494-546` designated-man sprint):

1. **Team tick, every frame, order** `6210 → 34470 → 6690 → 6FC0 → dispatcher`
   (CONFIRMED asm §10): port as `PesPressTick(side)` in `pes::` namespace,
   called from the team tick before the latch.
2. **Candidate table** (§5): on-pitch, not opp `FUN_140A62690` man, d² ≤
   462.25; enrol with `FUN_140639600` code; arrival `< 4.0` flag. GF inputs:
   `GetPosition`, ball pos, `GetTimeNeededToGetToBall_ms` (arrival stand-in,
   `// ponytail:` naming `FUN_14044E0D0`).
3. **Pick** (§6): score = angular + 10·distance terms, MATCH_UP −250000
   (i.e. prefer the MATCH_UP man); radius `(18+7r)²`; `12.5·codeProd+15`
   press/DELAY cut. GF: nearest + goal-sideShortlist, same radii.
4. **Write** (§3.3–3.4, §4): PRESS renews each tick; DELAY when the press-wins
   test fails; pair tail resolves the top two to DELAY + PRESS. GF: a
   `pressState[11]` array + `pressTarget[11]` points, same values
   (`0x33/0x34`), same writers.
5. **ActionPress per PRESS man per frame** (§8): target = ball-now or own base
   (§7, k = 5 ticks); 5 m touch-frame step on the event frame; else planner
   ETA `max(1, fps·dist)` (§9) vs ball ETA + 10; `FUN_1406378F0` target rewrite
   when a peer is nearer (+0.5 m). GF: `GetOnTheBallCommands` gains the
   ball-or-base target, the 5 m step, and the peer comparison; speed from
   `FUN_140A75BE0` class → GF run/sprint for that class (`// ponytail:`).
6. **Stop** (§10): renewal + reset — no latch, no timer. GF: drop the
   `timeNeeded < 1000`/`> 0.5 m` stand-in branches when the PRESS tick lands
   (clean cutover, same change).

Contact (unchanged from first-touch §7.2): reaction entry 3 TRAP within 10 m;
`touchFrame` from the HitData record bits 13..20; K=7/K=8 records as specced.

Latch (unchanged from first-touch §4.2 + §7.4): add step 4b
`ContestBallPredicate(holder)` only.

GF functions mapped: `elizacontroller.cpp:494-546` (designated chase →
`PesPressTick` + ActionPress target); `playercontroller.cpp:432`
(`_TrapCommand` ← TRAP rows, unchanged); `team.cpp:301/378`
(`HasPossession`/newest-event ← latch step 4b, unchanged);
`AIfunctions.cpp:1119` (`AI_HasPossession` stays out of the touch decision,
unchanged); `humanoid.cpp:524-531/589-600` (touch frame, unchanged).

## 14. Corrections to `pes-re-first-touch.md`

1. §7.1 "planner body NOT decoded (OPEN)": closed — `FUN_140678950` is a
   passthrough to `FUN_1406788B0`, which returns `max(1, fps·dist)` ticks (§9).
2. §7.1 "path that clears 0x34 is OPEN": closed — §10 table (renewal + reset,
   no single clear path).
3. §7.1 "planner flag `(ActionPress+0x30)` not named (OPEN)": INF now — re-plan
   signal `manETA + 10 < ballETA` (ticks).
4. §7.3/§3.5/§10-item-3 "C3 unit OPEN/INF": closed — degrees (§11).
5. §7.5 "MATCH_UP not written in open play" (via `MI+0x1775 == 0x17`): the
   `0x1775 ∈ {3, 8, 9, 10}` branch (CONFIRMED decompile lines 113–116) and the
   `T+0x6E7`/6 m near-ball path plus the mark-target-match path (lines
   156–189) are the open-play writers — MATCH_UP marks the carrier-side man
   every tick, not only in the `0x1775` branch. First-touch §7.5's conclusion
   (MATCH_UP ≠ loose-ball chaser) stands; its "otherwise no 0x3A" overclaims.
6. §10-item-1 "whether the clip picker is the vtable target": still OPEN
   (untouched).

## 15. Corrections to `pes-re-defensive-shape.md`

1. §2 prio table: `0x22` PASS_SUPPORT is prio **4**, not prio 0 (CONFIRMED
   `FUN_140A40050` decompile; §2 above). All other rows stand.
2. §15.14.1: the PRESS-vs-DELAY decision is reduced (§3.3: `bVar4` + 64°-pair /
   25°–155° window + `T+0xB709` bypass + MATCH_UP queueing), superseding "not
   reduced".
3. §15.14.3: the `12.5·codeProd + 15` press/DELAY cut is confirmed as the
   per-man outcome (leg mapping still INF).
4. §1 dispatcher loop `→ DELAY if FUN_14062AF10 else PRESS`: confirmed with the
   gate's body (§4); `FUN_14062AF10` returns 1 when another presser holds the
   geometry.
5. §14.4 "DELAY/PRESS `FUN_140536FC0`, structure only": now reduced (§3–§6).

## 16. Open items left

1. DELAY per-tick consumer (which action, if any, runs for a DELAY man).
2. `FUN_1406B9270`/`FUN_140707EE0` movement-solver internals (planner returns
   the ETA; how the ETA becomes footsteps is untouched).
3. `FUN_1406378F0` full reduction (arrival writer; head/decision/tail read but
   not reduced to a port rule).
4. `FUN_140639A20` codeProd leg mapping (INF) and `FUN_140639830`'s
   `FUN_140A41190` re-enrol reading (INF).
5. C3 float store site (the `0x24C6A` bearing map is CONFIRMED; the float
   writer is a sibling — unit settled regardless).
6. Clip picker as `FUN_1406D9010+0x68` target (from first-touch §10-item-1).
7. Undefined-region setter sites (`0x140540F2D` etc., §1) — outside defence.
