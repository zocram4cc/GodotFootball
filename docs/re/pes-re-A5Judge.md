# A5 PES referee decode: judge → whistle writer → sequencer → restart programmes, with the GF map

Scope: `tasks/15-10-26.md` §A5. Base: install `PES2021.exe` via the Ghidra bridge (`pes_full.bin`, base 0x140000000). Read-only. No engine code edited.
Scratch: `<workdrive>/gf-A5Judge-scratch/d_<VA>.c` (decompiles this session). GF side read from `origin/main` tree as checked out at `<workdrive>/GodotFootball` (`src/sim/onthepitch/referee.cpp`, 1062 lines).

Verdicts: **CONFIRMED** = decompile read this session (scratch file named) or constant read from the install; **DOC** = taken from an existing `docs/re/` report with its own OBS evidence (not re-read here unless said); **INF** = inference, confidence given; **OPEN** = blocked, blocker named.

---

## 0. Summary

1. **Judge (J-JDG).** One object, `Flow+0x908` (the Judge), with its result record `JR` (bound by `FUN_14050b2b0`; `Flow+0x49c0` in the per-frame calls, `MatchControl+0x2910` in the reports). It detects: ball out (3 kinds), contact foul (kind 2, levels 1–4), dive, offside (pending, kind 1), late-card owing, and the period/shootout stops. It has no referee strictness and no RNG. It is driven from the in-play sequencer step `0x3a`.
2. **Whistle writer `FUN_14050e770`.** Every stop goes through it. It applies a mode override from `MI+0x1775`, clamps the spot (TouchReach's writer, re-confirmed), writes the `JR` fields, and clears the judge's pending state. It does **not** start a programme. The programme starts when the flow reads a non-zero `JR` restart.
3. **Sequencer.** A 30-entry step array at `Flow+0x878`, index byte `Flow+0x8f0`. Runner `FUN_14044c880`, driver `FUN_140445e90`, frame sync `FUN_140422030`. Start hook `FUN_1404537b0` writes step 1 at index 0. Step 1 (`FUN_14044d110`) dispatches the builder `FUN_1404526a0` by stop reason `MI+0x1574` and restart `MI+0x1554`. The programme parks at `0x1a` (armed). In play it parks at `0x3a` (judge tick).
4. **Restart programmes.** Each restart kind has a builder with a fixed step list; the programme is `wipe → teleport → fade-in → armed`. Taker release gates are frame-rate multiples for corner, free kick and penalty; throw-in and goal kick are counter-gated (≈83 with a receiver test); kick-off is the receiver's ball-ownership test. Details §4.
5. **GF map.** Most of `referee.cpp` `Process()`, `CheckFoul()` and `PrepareSetPiece()` has a PES counterpart. Four GF-only mechanisms have no PES counterpart and should be deleted at cutover: the 4 s dead-ball auto-restart, the 30 s deferral cap, the 400 ms relax, and the throw-in ball "teleport back". The spot numbers differ in several places (§5.3). Map in §5.

**Top discrepancies to act on (all CONFIRMED in code unless marked):**
- **OPEN-1 settled (§9.1):** the restart type and the side both come from the last-player team `T = *(ring+0x180700)`: goal kick iff `dir_T·ball.x ≥ 0`, side = (T==0). A defender's own-end-line touch gives a corner to the attacker (live, 264.51 s). The earlier §3.4 claim (team 0's direction, so a goal kick to the attacker) is withdrawn.
- GF's penalty / throw-in / goal-kick / corner spots differ from PES's writer numbers (§5.3).
- GF's late card is "owed until shown"; PES's window is 16 game-minutes (DOC leftovers §3; GF `referee.cpp:64-69`).

---

## 1. Judge `match::ai::Judge` (J-JDG)

### 1.1 Object, record, binding (CONFIRMED + DOC)

| item | value | evidence |
|---|---|---|
| judge object | `Flow+0x908` | DOC duels §2.1 (ctor `FUN_14050ab20`); `FUN_1404a56c0` passes `param_1+0x908` |
| bind | `FUN_14050b2b0(judge, JR, ctx)`: `judge+0x3b8 = ctx`, `judge+0x3c0 = JR` | DOC duels §2.1; call in `FUN_1404a56c0` |
| result record `JR` | `Flow+0x49c0` in the per-frame calls; MI block = copy at `MI+0x1554` (`MI+X = JR+(X−0x1554)`) | CONFIRMED call site; DOC duels §2.1 |
| judge state | `+0x08` pending flag; `+0x0c` advantage timer (gs·4 = 216 frames); `+0x10` whistle hold; `+0x14..0x54` pending record; `+0x58` advantage copy; `+0x9c` judge enabled; `+0xa0` previous reason; `+0xcc` ball-out code (1 goal, 2 end line, 3 touchline) | CONFIRMED `FUN_14050ee40`, `FUN_14050b490`, `FUN_14050e4a0` |

**JR fields** (CONFIRMED `FUN_14050e770`; MI names DOC):

| JR off | MI | field | writer |
|---|---|---|---|
| +0x00 | 0x1554 | restart kind (1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 FK, 6 PK, 7 period end) | `FUN_14102cad0` |
| +0x04 | | awarded side (team index) | `FUN_140a3cd90` |
| +0x08 | 0x155c/0x1564 | spot x, z | `FUN_140a3cd10` (copies +0x14→+0x08) |
| +0x20 | 0x1574 | stop reason (1 period end, 3 goal/kick-off, 4 ball out, 5 foul/offside, 9 shoot-out, 10 abandon) | `FUN_141b8b250` |
| +0x34 | 0x1588 | foul kind (1 offside, 2 direct foul, 3 indirect/dive) | `FUN_14050e770` param 6 |
| +0x38 | 0x158c | card level (1 foul, 2 foul+warning, 3 yellow, 4 red) | commit / late-card writer |
| +0x3c | 0x1590 | fouler (player index, 0xff none) | param 7 |
| +0x40 | 0x1594 | victim (player index, 0xff none) | param 8 |
| +0x44 / +0x4c / +0x58 | | replay snapshot / frame / snapshot | `FUN_14050de50`, `FUN_140a81ac0` |

### 1.2 Inputs (CONFIRMED)

- **Ball:** position at `ball+0x27b8` (x) and via `FUN_140a57800(ball)` (returns a vector: `+0` = x, `+8` = across-pitch z). Used as `|x|`, `|z|` with abs mask `0x142565d30`.
- **FieldInfo** `F = *(G+0x2aa8)`: `F[0]` = 52.56 (goal-line half-length), `F[1]` = 34.06 (touchline half-width), `F[4]` = 47.06 (goal-area line), `F[5]` = 9.16, `F[6]` = 36.14 (penalty-area line), `F[7]` = pen-area half-width, `F[8]` = 41.56 (penalty spot). DOC pes-re-fieldinfo; values in `gametypes.hpp:79-85`.
- **Team attack sign:** `team+0x254` (`s8`) for team `t` via `FUN_140a48a30(G, t)`. CONFIRMED (TouchReach §2; `FUN_14050e020`, `FUN_14050b490`).
- **Match phase/mode:** `MI+0x116c` (play state: 1 in play, 2 stopped, 3 set-play ready), `MI+0x1168` (period), `MI+0x1775` (mode byte), `MI+0x1198` (frame).
- **Event ring** per team (`FUN_140a3e190(ring, i)`, 8 entries, 800-B stride at `ring+0x180698`); `ring+0x180700` is a field read by the corner/throw-in side (semantics OPEN, §3.4).
- **Offside category flags** `MA+0x253c4 + 0x10·t + 4·c` (DOC duels §5.1; read by `FUN_1405b6490`, only categories 0..2 punishable).
- **Contact records** from event kind 0xf (victim A = event+0x14, fouler B = event+0x20).

### 1.3 Detectors and outputs (CONFIRMED structure; spot/level details DOC)

| # | infringement / stop | detector (VA) | pending or direct | restart / reason / foul kind | hold | DOC |
|---|---|---|---|---|---|---|
| J1 | ball out, end line / goal / touchline | event 10 in `FUN_14050e4a0` (requires `MI+0x116c==1` and flag bit 2 of the event) → code 1/2/3 (§1.4) → Update branch | direct (`FUN_14050e110/e020/e320`) | 1: kick-off (goal), 3: goal kick, 4: corner, 2: throw-in | writer then step 0x0b | duels/throwin §1 |
| J2 | direct foul (contact) | event 0xf → gate `FUN_14050d6a0` → `FUN_1405b7470` (level) → `FUN_1405b8000` → `FUN_14050ee40` (SetPending) | pending, whistle delay `⌊0.8·gs+0.5⌋` frames (43) | restart 5, or 6 when the spot is in A's attacking box; kind 2; reason 5 | step 5 (5.0 s no scene) | duels §2 |
| J3 | dive / simulation | `FUN_1405b83b0` (diver in DIVE 0xf; contact-frame and 1.25 m tests) | pending | real foul → kind 2 level 1; else booking of diver, indirect FK (kind 3, level 3) | as J2 | duels §2.8 |
| J4 | offside | `FUN_1405b6490` finds a flagged player (category 0..2) who is now involved (`FUN_1405b5a00`) → `FUN_1405b6660` (SetPending) | pending, hold `gs/2` = 27 frames (0.5 s), +category 2 waits for a shot | restart 5, kind 1, level 1, never advantage | step 0x0b (2.5 s; 3.0 s immediate overwritten unless downed) | duels §5 |
| J5 | late card owed | `FUN_1405103e0` (advantage let go on a yellow-level foul) stores `judge+0xa8..0xc0`; `FUN_14050de50` writes the card at the next stop: `JR+0x3c=fouler`, `JR+0x38=3`, `JR+0x44`, `JR+0x4c` | none | card only | step 6 (`FOUL_CMN_CARD_Y_B`, max 2.5 s / 340 ticks) | duels §3, foul §4.5 |
| J6 | period end | time-up gate `FUN_14050b9b0` (`FUN_14050c3e0`) → protected thunk `0x143f559f0` (INF, protected code; not decoded) | direct | restart 7, reason 1 (writer protected `0x143f5530b`; DOC) | builder `FUN_1404505d0` | DOC foul §9 |
| J7 | penalty shoot-out kick | `FUN_14050cc40` (caller `FUN_1404a42e0`) | direct | restart 6, reason 9 | builder `FUN_14044eb10` | DOC duels §2.1 |
| — | handball | not an offence (no caller of `FUN_14050ee40` for hands; DOC duels §4) | — | — | — | DOC |

Judge update `FUN_14050b490` (CONFIRMED, decompile): each tick, if no pending foul: `(ball-out 1/2/3)` branches as in §1.4; otherwise the pending/advantage state machine. Pending `+0x08` → if `adv_timer==0` and `FUN_14050ff50` (ShouldPlayAdvantage) → copy to `+0x58`, `adv_timer = gs·4`, pending cleared. Otherwise, hold counts down, and when hold reaches 0 and `FUN_14050dd10` (DelayWhistle) is false → commit via `thunk_EXT_FUN_143f546f0` (Judge::Commit; → `FUN_14050e770`). Advantage timer runs at double speed while the fouled team has the ball; `FUN_1405105c0` (still on?) false → call back and commit the advantage copy. DOC duels §3 matches this structure.

### 1.4 Ball-out classification in event 10 (CONFIRMED, `FUN_14050e4a0` case 10)

```
gate:   MI.116c == 1  and  event.flags & 2
if |ball.x| <= F0:
    if |ball.z| > F1:           judge.cc = 3        # touchline
    (else nothing)
else:                           # |x| > F0
    inGoal = FUN_140a51cb0(ball pos, ..., F)        # goal-mouth test
    if inGoal:
        scan ring 0, up to 8 entries, skipping kind 0xe:
            first non-0xe entry: if kind==5 and FUN_140a63940(player) → judge.cc = 2 (end line); done
        if newest event is not (kind 0x10 and sub 2):  judge.cc = 1   # goal
        else:                                          judge.cc = 2
    else:                                              judge.cc = 2      # end line
```

Implementation note: `FUN_140a51cb0` (CONFIRMED, 1100 B) tests the ball is within the goal-mouth box (`F[0]`, `F[2]`, `F[3]`, `F[0xf]`, `F[0x10]`, `DAT_142565600` height margin). Its exact margins are the protected `.rdata` floats; not read here (OPEN-5, small).

### 1.5 Update branches for ball-out (CONFIRMED, `FUN_14050b490`)

| code | no pending foul | pending foul |
|---|---|---|
| 1 (goal) | if `MI+0x1168==9`: `FUN_140a6a3b0` → `FUN_14050e110(side)` (period-9 kick-off). Else `FUN_14050e110(judge, param = (team0.dir·ball.x ≤ F0))`; inside it side = (param == 0) = (team0.dir·ball.x > F0): kick-off to the **conceding** team (side 1 when team 0 scored into x > F0). | commit pending (`thunk_EXT_FUN_143f546f0`), `judge+0x50=0`, clear |
| 2 (end line) | `FUN_14050e020`: restart 3 (goal kick) iff `dir_T·ball.x ≥ 0`, else 4 (corner), where T = `*(ring+0x180700)` (the team argument the decompile hides; §9.1); reason 4; side = (T==0) | commit |
| 3 (touchline) | `FUN_14050e320` (throw-in; §3.2) | commit (+ advantage hold) |

### 1.6 Judge tick gating (CONFIRMED, `FUN_14050c3e0`)

`FUN_14050c3e0` runs `FUN_14050b490` only if `judge+0x9c != 0` (set by event 0x14 when both teams have the flag: `FUN_140a3bfc0(team,0/1)`), and only in MI phases not in `{2,6,9,10}` (mask `0x644`). Then `FUN_14050b9b0` (time-up gate) → protected `0x143f559f0`. The judge tick itself runs as the in-play programme step `0x3a` (`FUN_140449130` case `0x3a` → `FUN_1404a56c0`), see §2.3.

---

## 2. Whistle writer `FUN_14050e770` and the hand-on

### 2.1 Signature and effect (CONFIRMED decompile `d_0x14050e770.c`)

`FUN_14050e770(judge, side, restart, reason, spot*, foulKind, fouler, victim)`:

1. **Early out** if `judge+0x3b8 == 0`.
2. **Cutscene/replay gate:** `cVar6 = FUN_141f9c3b0(G+0x400)`, `cVar7 = FUN_1423e7fd0(G+0x3b0)`. If either is set and `reason == 0`: `judge+0xcc = 0`.
3. **Mode override** (`MI+0x1775` = `m`), when neither gate is set:
   - if `m ∈ {0, 8, 9, 10}`: skip the rest of the override.
   - `side = 0` unless `DAT_1437050d0 == 6` (a global flag, INF: a mode flag).
   - `reason == 3` → `reason = 4`.
   - if `m == 0` or `m ∈ {0x12..0x16}`: keep restart; else **`restart = 1` (kick-off)**.
4. If `restart == 0`: skip to clear.
5. **Spot:**
   - `spot == NULL` (ball-out / throw-in / corner / goal kick path): `R = FUN_14050d280(restart, F, ball, MI+0x173c)` (DOC throwin §1.3: throw-in `R = (ball.x, 0, sign(ball.z)·(F1+0.25))`), stored via `FUN_140a3cd10`. Then **restart 2** (throw-in): if `F0−1 < |x|`, `x = sign(x)·(F0−1)`. **Restart 5** (FK): if `F1−2 < |z|`, `z = sign(z)·(F1−2)`. No other clamp on this path.
   - `spot != NULL` (foul / offside / PK): (a) if `|z| < F5` and `dir·x < −F4` (own goal area): `x = sign(x)·(F4+0.7)`, `z = sign(z)·2.6`. (b) store. (c) if `|x| > F0`: `x = sign(x)·F0` (no margin). (d) if `|z| > F1`: `z = sign(z)·F1`. (e) restart 5 and `|z| > F1−2`: `z = sign(z)·(F1−2)`.
   - Constants (CONFIRMED read from the image, TouchReach §2): `0x142565604` = 2.0; `0x14257a194` = 0.7; `0x1425ab138` = 2.6; sign ±1 `0x14256ae1c`, `0x142588218`; abs mask `0x142565d30`.
6. **Writes:** `JR+0x14`/`+0x08` spot; `JR+0x00` restart (`FUN_14102cad0`); `JR+0x04` side (`FUN_140a3cd90`); `JR+0x20` reason (`FUN_141b8b250`); `JR+0x34` foul kind; `JR+0x3c` fouler; `JR+0x40` victim; `JR+0x28 = judge+0xa0` (previous reason), `judge+0xa0 = 0xff`; `FUN_14149a470()+0x82 = 0`.
7. **Clears:** `judge+0x0c = 0`, `judge+0x08 = 0` (pending gone).

### 2.2 What it hands on

- It sets `JR`. Nothing starts until the flow reads `JR`. The flow copies `JR → MI` each frame (`FUN_140422030` line 175, `FUN_140a3bcf0(MI+0x1554, JR)`).
- The programme start is `FUN_1404537b0` (§3.3). Callers (xref, CONFIRMED): `FUN_140449b00` (step 0, idle), `FUN_1404a56c0` (judge tick exit), `FUN_14044d2e0` (armed, `0x1a`), `FUN_1404491d0` (`0x1b`), `FUN_1404494e0` (`0x1c`), `FUN_140455090`, `FUN_140454570`.

### 2.3 Where the judge runs (CONFIRMED)

- Runner `FUN_14044c880` case `0x35..0x3a` → `FUN_140449130`. Case `0x3a` calls `FUN_1404a56c0` and returns 0, so the programme **parks at `0x3a`** while in play.
- `FUN_1404a56c0` (6111 B, `d_0x1404a56c0.c`): calls `FUN_14050b2b0` (bind), `FUN_14050c3e0` (judge update gate, §1.6), `FUN_14050c3c0`, `FUN_140a3bcf0` (JR copy). Then a restart-spot refinement on the JR record: restart 4 → `FUN_14050d250(…,3)`, restart 3 → `FUN_14050d250(…,4)`, restart 6 → kicker lookup `FUN_140a48a10` then `FUN_14050d250(…,6)`, restart 5 → `FUN_14050d250(…,6)` when `FUN_1404a7410` holds (INF: the penalty-area test). The spot-kind numbering of `FUN_14050d250` differs from the restart numbering (INF). Ends in the stop-entry path (`LAB_1404a5be8`), which reaches `FUN_1404537b0`.
- **OPEN-2:** the exact guard on `FUN_1404537b0` at the end of `FUN_1404a56c0` (the early returns at `cVar2`, `local_res18`, `Flow+0x49c4/0x49e0`). The decompile shows it reachable only on the stop-entry path, but the branch structure needs one more read. Blocker: Ghidra's merged gotos. Effect on the state diagram: none beyond the transition shown.

---

## 3. Sequencer (restart state machine)

### 3.1 Object and runner (CONFIRMED)

| item | place |
|---|---|
| step array | `Flow+0x878`, 30 × int32 (`FUN_140451fd0` fills 0x48 = none) |
| index | `Flow+0x8f0` (byte); write index `Flow+0x8f1` |
| programme kind | `Flow+0x874` (set by builders: 1, 2, 3, 4, 5, 6, 8, 10, 12, 14) |
| frame counter | `Flow+0x900` (int); sub-state `+0x8f2`; hold `+0x868` (float seconds × gs) |
| teleport counter | `Flow+0x27d8` (`FUN_14044a2a0`/`FUN_140a3c890`), mirrored `MI+0x11b0` |
| wall block | `Flow+0x2ab4` (`FUN_1404a9ce0`) |
| kickers | `Flow+0x4954` (`FUN_1404a7d60`) |
| set-play counter | `MI+0x11ac` (from step `0x1a`), published as `match+0x724` (DOC throwin §6.2) |
| driver | `FUN_140445e90`: per-player flags, `FUN_14044c880` (runner), then `FUN_140449c60`, `FUN_14044a9d0`, `FUN_14044ce80`, `FUN_14044a5e0`, `FUN_14044b5f0` |
| frame sync | `FUN_140422030` line 108: `FUN_140445e90(Flow, …)`; line 14042225e |

Runner `FUN_14044c880` (CONFIRMED `d_0x14044c880.c`): executes `step = Flow[0x878 + idx·4]`. If the handler returns true, `idx++`, and `+0x900/+0x8f2/+0x8f8/+0x8f4` are zeroed. A handler returning false holds. No advance (the handler returns without moving the index): `0x1a` (armed, `FUN_14044d2e0`), `0x1b`, `0x1c`, `0x3a` (judge tick, in play). Step `0x00` (`FUN_140449b00`) is the idle entry. Ticks are 1/54 s (DOC foul §9.1).

### 3.2 Step handlers (CONFIRMED runner switch unless marked DOC)

| step | handler | role | timing |
|---|---|---|---|
| 0x00 | `FUN_140449b00` | idle / end: judge idle check `FUN_14050c490`, `FUN_14050c3c0`; if `Flow+0x49c0 ≠ 0` → `FUN_1404537b0` | — |
| 0x01 | `FUN_14044d110` | **build**: calls dispatcher `FUN_1404526a0`; then `FUN_1404a7d60` (kicker map) and `FUN_140a3c890(0x27d8)` | 1 frame |
| 0x02 | `FUN_14044cda0` | (not read) | — |
| 0x03 | `FUN_140447230` | stop stamp: `record+0x14 = G+0x2a80+0x60` time, `+0x18 = FUN_140a74d70` | 2 frames |
| 0x05 | `FUN_140446390`→`FUN_14047c2d0` | foul hold: no scene 5.0 s (table[1]); scene gate (referee within 5 m, `0x14259222c`; EXCITE 2 m, `0x142565604`) | DOC foul §9.2 |
| 0x06 | `FUN_140446390` | late card scene (step 6): max(2.5 s, scene) | DOC foul §9.5 |
| 0x08 | `FUN_140446f80`→`FUN_140484a10` | substitution demo | DOC foul §4.1 |
| 0x09 | `FUN_140446390` | fatigue scene 2.5 s | DOC |
| 0x0b | `FUN_140447690` | out-of-play hold: table[5] = 2.5 s; offside `Flow+0x868 = 3.0` immediate (overwritten in sub3 unless downed); downed +2.5 s (`0x14259bde4`) | 135–140 frames live (DOC) |
| 0x0c, 0x0d | `FUN_1404488e0` | fade-in 0.5 s (`0x142565600`) after 5 frames | 32 frames |
| 0x0e | `FUN_1404488e0` | fade-in 1.0 s (`0x14256ae1c`) | 59 frames |
| 0x0f | `FUN_1404488e0`/`FUN_140448c90` | fade-out type 6, 0.5 s | — |
| 0x10 | `FUN_1404488e0` | fade-out type 2, 0.1 s (`0x142588200`) | 5 frames |
| 0x11 | `FUN_1404488e0` | **wipe**, fade-out type 4, 0.1 s | 5 frames |
| 0x12 | `FUN_140448760` / `FUN_1404485f0` | (DOC: UI/sequencing) | 5 frames |
| 0x13 | `FUN_140447b50` | kicker selection sub-states 6/7 (`FUN_1404a7d60`) | DOC setpiece §1.1 |
| 0x14 | `FUN_14044a2a0` | **teleport**: `Flow+0x27d8` counts 1..20; one group per value (`MI+0x11b0`) | 20 frames |
| 0x15 | `FUN_14044bb40` | placement finish | 20 frames |
| 0x16 | `FUN_140449d20` | substitution/UI window (10 sub-states) | UI-dependent |
| 0x17 | `FUN_1404a9ce0` | wall (only when restart = 5) | — |
| 0x19 | `FUN_14044bfb0` | caption hold 0.25 s (+2.0 s with caption) | 16 frames |
| 0x1a | `FUN_14044d2e0` | **armed**: starts set-play counter `MI+0x11ac`; terminal (returns, does not advance); calls `FUN_1404537b0` at `0x14044d755` (OPEN-3) | — |
| 0x1d | `FUN_14044b1d0` | substitution window (DOC) | UI |
| 0x1e, 0x1f | `FUN_14044b380` | substitution / throw-in no-kicker sub-windows | UI |
| 0x20 | `FUN_14044af70` | team-management window | UI |
| 0x23 | `FUN_140448e50` | UI/net fade 0.5 s + 1.0 s (offline skip) | 1 frame offline |
| 0x33 | `FUN_14044b8a0` | **seamless throw-in** (§4.2) | — |
| 0x34 | `FUN_14044a3e0` | 2 frames | 2 frames |
| 0x35 | `FUN_1404a67b0` | in-play sub-step (not decoded here) | — |
| 0x36–0x39 | `FUN_1404a5de0`, `FUN_1404a62b0`, `FUN_1404a6270`, `FUN_1404a5cb0` | in-play sub-steps (not decoded here) | — |
| 0x3a | `FUN_1404a56c0` | **judge tick, in-play park** (§2.3) | every tick in play |
| 0x43 | `FUN_140452e00` | scene-driven kick-off path (goal kick-off branch of `FUN_14044f8a0`) | — |
| 0x44 | `FUN_140451fd0(8)` | reset to idle | — |
| 0x45 | `FUN_140446350` | release MI write lock (`FUN_140a3cb20(MI,0)`) | 1 frame |
| 0x46 | `FUN_14044c6e0` | (not read) | — |

### 3.3 Start, build, dispatch (CONFIRMED)

- **`FUN_1404537b0` (start):** `FUN_140451fd0(Flow, 1)` (programme kind 1), writes `step = 1` at index 0, `FUN_140a3cd90(Flow+0x2790, 2)`, zeroes `Flow+0x4954` (kickers), `Flow+0x2806`. Sets `Flow+0x27fc` if the restart is 2 or 5-without-flag (INF: the throw-in/FK scene gate).
- **`FUN_14044d110` (step 1, build):** if no replay/training gate → `FUN_1404526a0(Flow, param)`. Then `MI+0x1554 == 5` → `FUN_14044ef30`; `== 3` → side flag; always `FUN_1404a7d60(Flow, kickers, MI+0x1554, Flow+0x2ab4, Flow+0x2afc)` and `FUN_140a3c890(Flow+0x27d8)`.
- **`FUN_1404526a0` (dispatcher, CONFIRMED `d_0x1404526a0.c`):**

| condition | builder | programme (summary) |
|---|---|---|
| `MI+0x1775 ∈ {9, 10}` | inline (reset 0xc) | `[0x45, 0x23, 0x12, 0x13, 0x14, 0x15, 0x19, 0x1a]` (no wipe) |
| `MI+0x1775 ∈ {0x0b..0x0f}` | `FUN_14044ffd0` (reset 0xc) | `[0x0f, 0x45, 0x32, 0x0f]` + tail(0xc) (abandon) |
| `MI+0x1775 == 0x10` | `FUN_14044ffd0` + `FUN_140a3cd90(·,2)` | as the row above; the 2790 record set to 2 |
| `reason == 0` | `FUN_14044fef0` | `[0x0f, 0x45]` + tail(0xc) — half start (matches DOC sample `1 f 45 23 16 12 13 14 15 c 34 19 1a`) |
| `reason == 3` (goal) | `FUN_14044f8a0` | kick-off after goal (§4.1) |
| `reason == 5` and `MI+0x15a5 ≠ 0` (card scene) | `FUN_14044f8a0` | as above |
| `reason == 7` | switch on restart 1..6 → `FUN_14044f540 / f780 / f420 / f1c0 / f2e0 / f660` | period-start / half-time variants (not decoded; INF) |
| `reason == 8` | `FUN_140452950` | (not decoded) |
| `reason ∈ {4,5,…}`, restart switch | 1 → `FUN_14044ff60`; 2 → `FUN_1404500c0` (throw-in); 3 → `FUN_14044fb10` (goal kick); 4 → `FUN_14044e720` (corner); 5, 6 → `FUN_14044eb10` (FK / penalty, foul builder); 7 → `FUN_1404505d0` (period end) | §4 |

### 3.4 Ball-out side and kind (CONFIRMED structure, semantics OPEN)

`FUN_14050e020` (corner / goal kick, CONFIRMED `d_0x14050e020.c`):
- restart `= 3` (goal kick) if `dir_T·ball.x ≥ 0`, else `4` (corner), with `T = *(ring+0x180700)`. Raw disassembly (§9.1): `EDX = T` is the second argument of `FUN_140a48a30` at `0x14050e08e`; the decompile shows the call with one argument, so the earlier "team0.dir" reading was wrong.
- side `= (*(ring+0x180700) == 0)`, where `ring = *(*(judge+0x3b8)) + 0xe0` and `FUN_140a3e880(ring) = ring+0x180700`. Passed as `FUN_14050e770(judge, side, restart, 4, NULL, 0, 0xff, 0xff)`, with restart 3 or 4.

`FUN_14050e320` (throw-in, CONFIRMED `d_0x14050e320.c`): walks up to 8 ball events, skips those stamped `MI+0x1198`, takes the first whose kind passes `FUN_140a60530` and whose player resolves to a team; `side = 1` if that toucher is in team 0 (`player < 11`), i.e. the team that did **not** touch last. Default before the loop: `side = (ring+0x180700 == 0)`. If the chosen side has no eligible players (`team+0x87a5 + 0x87b9 + 0x87cd == 0`, not training), the side flips.

**OPEN-1 resolved (§9.1); this paragraph is withdrawn.** The restart type is `3` iff `dir_T·x ≥ 0` with `T = *(ring+0x180700)`, and the side is `(T==0)`. A defender's own-end-line touch therefore gives a corner to the attacker (live 264.51 s: T=0, x=−52.8, side 1).

### 3.5 Timers (CONFIRMED read from the image; `StoppageConstants` already ported in `pesstoppage.hpp`)

| name | VA | value | use |
|---|---|---|---|
| hold table `FUN_14044e460` | `0x14259ce60` (idx 1), `0x14259ce54` (idx 5) | 5.0 s, 2.5 s (table also 8.0, 3.0, 0, 7.0, 0 by index) | foul hold / out-of-play hold |
| player-down extra | `0x14259bde4` | +2.5 s | sub3 of 0x0b |
| offside immediate | `0x140447a25` | 3.0 s (overwritten unless downed) | 0x0b base |
| wipe | `0x142588200` | 0.1 s → 5 frames | steps 0x10, 0x11 |
| fade-in long | `0x14256ae1c` | 1.0 s → 54 frames (+5) | step 0x0e |
| fade-in half | `0x142565600` | 0.5 s → 27 frames | steps 0x0c, 0x0d |
| tail | `0x142592228` | 0.25 s → 14 frames | step 0x19 |
| referee scene range | `0x14259222c` | 5.0 m | scene start gate |
| EXCITE scene range | `0x142565604` | 2.0 m | scene start gate |
| PK approach | `0x14259be10` | 4.0 m (d² 16) | referee JUDGE approach |
| offside whistle hold | (gs/2) | 27 frames | Judge pending |

### 3.6 State diagram (VAs)

```mermaid
stateDiagram-v2
  [*] --> PLAY : sequencer step 0x3a (judge tick, FUN_1404a56c0)
  PLAY --> PENDING : event 0xf / offside flag (FUN_14050ee40, FUN_1405b6660)
  PENDING --> PLAY : DelayWhistle or advantage (FUN_14050b490)
  PENDING --> WHISTLE : hold done, no delay (FUN_143f546f0 commit)
  PLAY --> WHISTLE : ball out code 1/2/3 (FUN_14050e4a0 -> FUN_14050b490)
  WHISTLE --> START : FUN_14050e770 writes JR (restart != 0)
  START --> BUILD : FUN_1404537b0 writes step 1 at idx 0
  BUILD --> STOP : step 1 FUN_14044d110 -> FUN_1404526a0 (builder by reason/restart)
  STOP --> WIPE : steps 0x03, 0x0b|0x05|0x06, 0x45, 0x11 (FUN_1404488e0)
  WIPE --> TELEPORT : steps 0x1d/0x1f (opt), 0x23, 0x16, 0x12, 0x13, 0x14 (FUN_14044a2a0), 0x15
  TELEPORT --> FADEIN : 0x0e (1.0 s) or 0x0d (0.5 s)
  FADEIN --> ARMED : 0x34, 0x19, 0x1a (FUN_14044d2e0; set-play counter MI+0x11ac from 0)
  ARMED --> TAKE : taker gate (§4.7), taker kick action
  TAKE --> PLAY : kick release event -> FUN_140445750 (in play, MI+0x116c 3 -> 1)
  STOP --> PLAY : no restart (reason 0 builder ends in 0x1a too; OPEN-4)
  PLAY --> [*] : period end (restart 7, FUN_1404505d0)
```

Stop-entry and armed→play transitions are CONFIRMED for the programme shape; the exact guard for `FUN_1404537b0` (OPEN-2), the armed exit into the in-play park (OPEN-3), and what the reason-0 builder means in play (OPEN-4) are the open items.

---

## 4. Restart programmes per type

Verdicts: programme = CONFIRMED (builder code) and sample = DOC (live OBS). Timings from DOC setpiece §8.1 and foul §9.6 unless marked.

### 4.1 Kick-off (restart 1)

- **Half start:** reason 0 → `FUN_14044fef0`: `[0x0f, 0x45] + tail(0xc)` → `1 f 45 23 16 12 13 14 15 c 34 19 1a` (DOC sample). CONFIRMED builder; sample DOC.
- **After goal:** reason 3 → `FUN_14044f8a0`: reset(10); `FUN_140452260(·,0,0)` (emits step 3, then 4 for reason 3); `0x1d` if no sub; if card-scene or `FUN_140a3f580` false: `[3, 0x11, 8]` then tail; else the scene branch `0x43, 7, 0x44`. DOC sample `3 4 45 1d 3 11 8 23 16 12 13 14 15 e 34 19 1a`.
- Placement: `FUN_14050c490` case 0/3/9 sets the kick-off record (`local_d0 = DAT_14259be84`); live: referee at `(−dir·5, 0, −dir·10)` (DOC setpiece §3.2).
- **Taker release:** refuted by the live capture (§9.3a, Main 23-10-26): the kick-off's 3 → 1 comes at the taker's kick, 118–139 frames after arming. The `KickoffReceiver` (`0x140781fc0`) pass gate is not the release (§9.3a). The 28-frame figure of setpiece §10 does not match the capture and is not used.
- **Side:** the conceding team (CONFIRMED, §1.5 code 1).

### 4.2 Throw-in (restart 2)

- Dispatcher → `FUN_1404500c0` (CONFIRMED `d_0x1404500c0.c`).
- **Seamless** (`FUN_140444f00` true, no designated kicker found (`uV4==0xff`, `uV5==0xff`), no action-state holds): program `[0x33]` → handler `FUN_14044b8a0` (seamless pickup, DOC throwin §3.2). On failure it falls back to `[0x10, 0x45, 0x46, 0x16, 0x12, 0x13]` (DOC throwin §3.1).
- **Ceremony** (not seamless, no late card): `FUN_140452260(·,5,0)` → `[3, 0xb, 0x45]` (reason 4, restart 2 gives `0xb`); `[9]` when a designated kicker exists; then `[0x10, 0x46]` with no sub due, or `[3, 0x10, 8]` with a sub due; then tail(0xd): `0x23, 0x16, 0x12, 0x13, 0x14, 0x15, 0xd, 0x34, 0x19, 0x1a` (armed). The live wipe is `0x10` (fade type 2, 0.1 s; DOC setpiece §9.4). CONFIRMED structure of `FUN_1404500c0`; late-card branch not traced here.
- Spot: `R = (ball.x, 0, sign(ball.z)·(F1+0.25))`, `|R.x| ≤ F0−1` (DOC throwin §1.3, clamp CONFIRMED in writer).
- Taker: fastest of the four ranked team-mates (seamless) or same-flank full-back (ceremony) (DOC throwin §2).
- **Taker release:** live (§9.3): ready to release 118–194 frames; counter 88–110 at release. The DOC ≈81/83 counter is not reproduced. Flow: release clip at its event (18/27/99 frames) → `MI+0x116c 3→1` (in play).

### 4.3 Goal kick (restart 3)

- Dispatcher → `FUN_14044fb10` (CONFIRMED `d_0x14044fb10.c`): reset(5); `FUN_140452260(·,5,0)` → `[3, 0xb, 0x45]`. Then `0x1e` when no designated kicker (else `0x09`); `0x1d` before the wipe when a sub is due (late-card window `0x11` variant also emits `0x1d`); `0x1f` when a second kicker exists; `0x10` wipe and `3, 0x10, 8` when a sub is due, else `0x10` alone; tail(0xd) (tail(0xe) when the late-card window is open).
- Live sample: `3 b 45 1e 10 23 16 12 13 14 15 d 34 19 1a` (DOC setpiece §6.1), wipe `0x10`, fade-in `0xd`.
- Spot (DOC setpiece §6.1, OBS): `R = (±(F0 − 4.8), ±2.6)`, `±` on the side the ball went out (**not** GF's `(±0.92·pitchHalfW, 0)`).
- **Taker release:** live (§9.3): ready to release 238–253 frames (run 1: 238; run 3: 241, 253). The counter advances (147 at release in run 3), but 83 is refuted as a threshold (min 238 frames). 246 (§6.1) is inside the measured range. OPEN-6.

### 4.4 Corner (restart 4)

- Dispatcher → `FUN_14044e720` (CONFIRMED `d_0x14044e720.c`): reset(4), `FUN_140452260(·,5,0)` (`[3, 0xb, 0x45]`), `0x11` wipe, `0x1d`, `0x1f` (if the 0x1f branch), tail `0xe`.
- Live sample: `3 b 45 11 1d 1f 23 16 12 13 14 15 e 34 19 1a` (DOC setpiece §1.1).
- Spot: ball 0.90 m inside the arc from the flag (DOC setpiece §4.1). Live (§9.5): placed (−52.25, 33.21) and (52.25, −33.21), at 70°, from the fit `(sign(x)(|x|−0.3), sign(z)(|z|−0.8))`. The DOC 30° sample is not reproduced. OPEN-7.
- **Taker release:** live (§9.3): 365–379 frames from ready (379 matches the DOC sample; the DOC gate of 189 is not reproduced). No opponent within 9.15 m at release (§9.2). The taker walks from 4.1 m to 1.2 m of the spot.

### 4.5 Free kick (restart 5)

- Dispatcher → `FUN_14044eb10` (foul builder; DOC foul §4.1 CONFIRMED structure).
- Programme: `[3, 5, 0x45]`, `0x11`, `[0x1d, 0x1f]` (unless `MI+0x177f`), `[3, 0x11, 8]` if a substitution scene is due, then `FUN_1404524e0`: `[0x23, 0x16, 0x12, 0x13, 0x17 (wall, only restart 5), 0x14, 0x15, 0xe, 0x34, 0x19, 0x1a]`. Offside uses `0xb` instead of `5` (`MI+0x1588 == 1`).
- Live sample: `3 5 45 11 1d 1f 23 16 12 13 17 14 15 e 34 19 1a` (DOC setpiece §1.1).
- Spot: §2.5 clamp (`F4+0.7`, `F1−2.0` for FK, `F0`, `F1`); foul spot from `FUN_1405b8000` (DOC; not decoded, OPEN-8).
- Penalty-area FK converts to restart 6 (`FUN_14050d280(6, …)` penalty mark).
- **Taker release:** live (§9.3): 355–356 frames from ready. The DOC gate `fps·3.0` = 162 frames is not reproduced. No opponent within 9.15 m at arm or release (§9.2).

### 4.6 Penalty (restart 6)

- Dispatcher → `FUN_14044eb10`, same foul builder with restart 6, so **no** `0x17` wall (`FUN_1404524e0` adds 0x17 only for restart 5, CONFIRMED).
- Spot: `(±F8, 0)` = ±41.56 (DOC setpiece §7; `F[8]`). Taker: designated kind 5 (DOC setpiece §2).
- **Taker release:** `fps·1` = 54 frames (DOC setpiece §12; not observed live in a match).
- Shoot-out: `FUN_14050cc40` (restart 6, reason 9); `PenaltyShootoutController` (GF) is the owner in GF.

### 4.7 Taker release gates (summary, DOC setpiece §12 + throw-in §6)

| restart | gate | frames at 54 Hz | live |
|---|---|---|---|
| kick-off | the taker's kick, 3 → 1 (capture, 23-10-26); the KickoffReceiver gate is not it (§9.3a) | 125 (ported) | 118–139 frames (capture); 125 (§9.3) |
| throw-in | taker reach + kick (live); DOC counter ≥ 81 (+2) | ≈83 | 118–194 frames; counter 88–110 at release (§9.3) |
| corner (long) | `fps·3.5` (DOC) | 189 | 365–379 frames (§9.3) |
| corner (short) | inside `fps·2`, partner test | 108 | not sampled |
| goal kick | taker reach + kick (live); DOC counter 83 refuted | 83 (refuted) / 246 | 238–253 frames (OPEN-6) |
| free kick | `fps·3.0` (DOC) | 162 | 355–356 frames (§9.3); no opponent within 9.15 m (§9.2) |
| penalty | `fps·1` | 54 | not observed |

### 4.8 Period end (restart 7)

- Dispatcher → `FUN_1404505d0` (CONFIRMED `d_0x1404505d0.c`): reset(8); `FUN_140452260(·,4,0)` (emits `[3, 7, 0x45]`: restart 7 hits the `0x7` case). Then `MI+0x1168 == 10`: `[0x0f if reason 2] + [0x26]`, vtable call, `FUN_140452260` as above, `[0x24]`. Else: `FUN_140452260` as above, then `[0x45, 0x25, 0x24, 0x0f]`, then `[0x13, 0x29]` when `MI+0x1168 == 8`, else `[0x1b]`. Writer protected `0x143f5530b` (DOC foul §2.1); gate `FUN_14050b9b0` (A6 doc: thresholds `.rdata 0x142647c64/68`).
- Half-time GF equivalent: `SetPieceLaws::kHalfTimeCutscene_ms = 10 s` (GF `setpiecelaws.hpp:135-145`). PES value OPEN (cutscene length from the scene table).

---

## 5. GF map: `referee.cpp` and friends → PES replacement

Notation: **replace** = delete the GF path when the PES path is the default; **keep** = PES-numbered already; **delete-GF-only** = no PES counterpart found, GF-only mechanism.

### 5.1 `Referee` members

| GF symbol (file:line) | PES replacement (VA) | action | notes |
|---|---|---|---|
| `Referee::Process` (`referee.cpp:154-599`) | judge tick `FUN_1404a56c0` → `FUN_14050c3e0` → `FUN_14050b490` (Update) for stops; sequencer `FUN_14044c880` for the restart stages | **replace** | Split into (a) judge stop detection (§1), (b) programme (§3). |
| …period end block (`:157-221`) | time-up `FUN_14050b9b0` + builder `FUN_1404505d0` | replace | GF's added-time formula (`PesClock`) is GF; keep the clock, replace the whistle. |
| …goal / corner / goal kick (`:224-287`) | Update codes 1 (`FUN_14050e110`) and 2 (`FUN_14050e020`) | replace | Goal-line test: GF `pitchHalfW + lineHalfW + 0.11` (52.67); PES `|x| > F0` (52.56, no margin). Corner/goal-kick side: see OPEN-1. |
| …touchline (`:289-320`) | code 3 → `FUN_14050e320` | replace | GF `pitchHalfH + lineHalfW + 0.11` (34.17); PES `|z| > F1` (34.06). PES's throw-in side: "did not touch last", flip if no eligible players (not in GF). |
| …`CheckFoul(true)` and `CommitPendingOffside()` (`:322-326`) | judge pending (`FUN_14050ee40`), Update hold/advantage (§1.3) | replace | Keep `pesjudge.hpp` advantage/level code (ported; matches `FUN_14050ff50`, `FUN_14050b490`). |
| …dead-ball auto-restart (`:347-411`, `kDeadBallRestart_ms` 4000, `:29`) | none | **delete-GF-only** | GF safety net for ball-out events not reaching the whistle. PES's ball-out is event 10 (§1.4). Delete when the PES judge covers every ball-out; keep as a tripwire until then. |
| …card book + `kPesCardOnScreen_ms` (`:413-419`) | late card: step 6 (`FOUL_CMN_CARD_Y_B`), `FUN_14050de50` | replace | GF "owed until shown" → PES window 16 game-minutes (DOC leftovers §3; `referee.cpp:64-69` says whole match). |
| …prepare at `prepareTime` (`:421-436`) | teleport/placement steps 0x14/0x15 and the armed programme | replace | The "ready" point is the wipe end (5 frames after 0x11). |
| …throw-in ball hold `ResetSituation` (`:438-448`, `kRestartHoldTolerance_m` 0.05) | none (taker holds the ball, DOC throwin §3.2) | **delete-GF-only** | Ponytail in GF: "a teleport back to the spot once it strays". Delete when the taker carries the ball (seamless) or stays in THROWIN_LOOP. |
| …Law 16 penalty-area hold (`:450-503`, `MayRestart`, `kMaxWait_ms`) | none found (DOC setpiece §9.8: "no PES step waits on the referee") | **keep, ponytail** | OPEN-9: does PES hold the taker until opponents leave the area? AI positioning may do it. Mark `// ponytail:` naming OPEN-9. |
| …Law 13 nine-metre hold (`:506-541`) | same as above | **keep, ponytail** | OPEN-9. |
| …`MayTakeRestart` (`:543-573`) | §4.7 gates (corner/FK/PK frame multiples CONFIRMED in `setpiecelaws.cpp:112-136`) | **replace for throw-in, goal kick** | GF ungates throw-in and goal kick. PES: throw-in ≥81 with receiver; goal kick ≈83 (OPEN-6). Kick-off: receiver ownership. |
| …taker touch → `StopSetPiece` (`:577-595`, `afterSetPieceRelaxTime_ms = 400` at `:587`) | `MI+0x116c 3→1` at the taker's release event (`FUN_140445750`, DOC throwin §4.2) | replace; delete the 400 ms relax | The 400 ms relax has no PES counterpart. |
| `IssueDeferredCards` (`:601-639`) | step 6 late card (DOC foul §9.5) | replace | `kPesCardOnScreen_ms` (3000) is a GF rounding; PES card prop at 40 ticks into the scene. `buffer.holdTime`/`prepareTime += 3000` goes. |
| `PrepareSetPiece` (`:641-681`) | spot writer clamp (§2.1) + teleport placement (DOC setpiece §4, §5) + `pes::ChiefRefereeSpot` | keep spot logic, replace placement | `OffsideRule::RestartPosition` (`offsiderule.cpp:141-162`) matches the writer (CONFIRMED). Note: PES clamps at the whistle, GF clamps at prepare. Move to the writer. |
| `AlterSetPiecePrepareTime` (`:683-719`) and `kMaxRestartDeferral_ms` (`:40`) | none (DOC foul §9.8: no cap) | **delete-GF-only** | Also used by `match.cpp` for cards/celebrations: those become the step holds. |
| `GetOffsideCategory` (`:721-725`) | MA category bitsets `MA+0x253c4` (DOC duels §5.1) | keep until MA flags exist | Only categories 0..2 punishable (`FUN_1405b6490`). |
| `BallTouched` (`:727-807`) | per-touch offside writer `FUN_140a7b7a0` (DOC offside §4; detail OPEN) | keep until writer ported | `OffsideRule::CanCreateOffside` (`offsiderule.cpp:115-121`) matches DOC offside §2.2 (CONFIRMED in GF). |
| `CommitPendingOffside` (`:809-845`, `kPesOffsideWhistleHold_ms` 500 at `:84`) | `FUN_1405b6660` → SetPending kind 1, restart 5, level 1, hold 27 frames | replace | 500 ms = 27 frames at 54 Hz (gs/2), so the value is equivalent. GF DelayWhistle tests `e_FunctionType_Shot` only; PES also newest event type 5 and `FUN_14050d7f0` (DOC duels §3). |
| `JudgeContact` (`:847-892`) | `pes::JudgeLevel` (`pesjudge.cpp`, PES points and thresholds, DOC duels §2.3–2.5) | keep (already port) | Spot shift `spotBias_m` matches `FUN_1405b6870`. Penalty-area line: GF `pesPenaltyAreaLineX` = F6 (CONFIRMED `gametypes.hpp:85`). |
| `NearestToBall_m` (`:894-900`) | PES candidate `FUN_14050f580` (key player `team+0x78`, not offside-flagged, ball-seeking) | replace | GF uses nearest active player. Used only by `ShouldPlayAdvantage` inputs (`:971-973`). |
| `RecordFoul` (`:902-913`) | `FUN_14050ee40` SetPending (fields §1.3, CONFIRMED) | replace | GF `foulPosition = victim position`. PES spot from `FUN_1405b8000` (OPEN-8). |
| `ContactEvent` (`:915-937`) | event 0xf → `FUN_14050d6a0` gate → `FUN_1405b7470` (DOC duels §2.2) | replace | GF gate `(playedBall || targeted || staggered || fell)` (`:928`) approximates `FUN_14050d6a0` (DOC: plus the team-differs and the advantage-timer clauses). |
| `CancelSetPiece` (`:939-950`) | abandon builder `FUN_14044ffd0` (`MI+0x1775 ∈ {9,10}`) and shoot-out restart 6 reason 9 | replace | Used by `PenaltyShootoutController::Start`. |
| `CheckFoul` (`:952-1059`) | advantage (`pesjudge` `InAdvantageDecisionWindow`, `ShouldPlayAdvantage`, `AdvanceAdvantage`, keep); commit `FUN_143f546f0` → `FUN_14050e770` | replace commit, keep advantage | Penalty test GF `|z| < 20.15−0.06`, `x·side > 52.5−16.5+0.06` (`:955-957`) vs PES F6 = 36.14 and F7; GF box line 36.06, 0.08 m short. Stats `AddFoul`/`AddCard` (`:1042-1043`) keep. |

### 5.2 Other GF files

| GF | PES | action |
|---|---|---|
| `setpiecelaws.cpp:112-136` `MayTakeRestart` | §4.7 | keep corner/FK/PK; add throw-in ≈81+receiver and goal kick ≈83 |
| `setpiecelaws.cpp:105-110` `MayRestart` | OPEN-9 | keep as ponytail |
| `setpiecelaws.hpp:135-145` half-time cutscene 10 s | OPEN (scene length) | keep as ponytail until scene table is read |
| `foulsequence.cpp:19-42` `HoldEndsAt_ms`, `RestartPrepareAt_ms`, `RestartTakeAt_ms` | §3.5 timers | keep (already from `pesstoppage.hpp`); `RestartTakeAt` = prepare + tail + fade + 0.25 s |
| `goalsequence.hpp:145-148` `RestartPrepareAt_ms` (22.5 s / 19 s default) | goal stop: `FUN_14044f8a0` | keep; check the `kCelebration_ms` default against the replay (DOC restart-watch) |
| `offsiderule.cpp:141-162` `RestartPosition` | §2.1 writer clamp | keep; apply at the whistle |
| `pesstoppage.hpp` `StoppageConstants`, `RestartSpotConstants` | §3.5, §2.1 | keep |
| `pessetpiecetaker.hpp` | §4.7 | update the comment: throw-in and goal kick are counter-gated (≈83), not "arrival" |
| `restartwatch.cpp:21-37` | bound from `RestartTakeAt` + GF caps | keep as tripwire; PES values from §3.5 |

### 5.3 Constant and placement differences (GF vs PES, CONFIRMED)

| restart | GF (`referee.cpp`) | PES (writer / builder) | Δ |
|---|---|---|---|
| goal-line test | `pitchHalfW + lineHalfW + 0.11` = 52.67 (`:224`) | `|x| > F0` = 52.56 | 0.11 m |
| touchline test | `pitchHalfH + lineHalfW + 0.11` = 34.17 (`:292`) | `|z| > F1` = 34.06 | 0.11 m |
| throw-in spot x | `clamp(x, ±(pitchHalfW − 0.6))` = ±51.9 (`:309`) | `|x| ≤ F0 − 1.0` = 51.56 | 0.34 m |
| throw-in spot z | `±pitchHalfH` = ±34.0 (`:311-313`) | `sign(z)·(F1 + 0.25)` = 34.31 (live ±34.31) | 0.31 m |
| corner spot | `(±pitchHalfW, ±pitchHalfH)` = (±52.5, ±34) (`:269`) | 0.90 m inside the arc (DOC setpiece §4.1) | ~0.9 m |
| goal kick spot | `(±0.92·pitchHalfW, 0)` = ±48.3, z 0 (`:279`) | `(±(F0−4.8), ±2.6)` = (±47.76, ±2.6) | 0.5 m in x; z 2.6 m |
| penalty spot | `(pitchHalfW − 11)·side` = 41.5 (`:1023`) | `±F8` = 41.56 (DOC setpiece §7) | 0.06 m |
| FK inset (touchline) | none in GF's referee (only `RestartPosition`'s `pesFieldHalfWidth − k.freeKickInset_m`) | `F1 − 2.0` = 32.06 (`FUN_14050e770` restart 5) | 2 m; GF has it via `RestartPosition` (`offsiderule.cpp:159-160`) — match. |
| offside hold | 500 ms (`:84`) | 27 frames gs/2 | equivalent |
| foul/out-of-play hold | `HoldEndsAt_ms` (table) | table[1] 5.0 s / table[5] 2.5 s | matches |
| late-card window | whole match (`:64-69`) | 16 game-minutes (DOC leftovers §3) | GF deviates |
| penalty box line | 36.06, half-width 20.09 (`:955-957`) | F6 = 36.14, F7 = 20.12 | 0.08 m, 0.03 m |

### 5.4 What GF has that PES lacks (delete or justify)

1. `kDeadBallRestart_ms` 4 s auto-restart (`:29, 347-411`). No PES counterpart. Delete at cutover.
2. `kMaxRestartDeferral_ms` 30 s cap and `AlterSetPiecePrepareTime` clamp (`:40, 683-719`). No PES cap in the programme (DOC foul §9.8). Delete at cutover.
3. `afterSetPieceRelaxTime_ms = 400` (`:587, 597-598`). No PES counterpart. Delete.
4. Throw-in ball `ResetSituation` tolerance 0.05 m (`:444-448`). Ponytail. Delete when the taker holds the ball.
5. Law 16 and Law 13 holds (`:450-541`). No PES step waits on them (DOC setpiece §9.8). Keep only as ponytails naming OPEN-9.
6. `kPesDownedInjuryLevel = 0.01` (`:57`). A stand-in for PES's `player+0x34` down byte (ponytail in code). Keep until the down flag exists.
7. `kPesNoPerfTier = 0` (`:49`). PES reads `TeamSlice+0x4C00` (DOC tactics clock). Keep as an explicit input until the field is ported.
8. `IssueDeferredCards` `kPesCardOnScreen_ms = 3000` (`:62`). Rounding of the card prop timing. Replace with the scene timing.

### 5.5 What PES has that GF lacks (port or name)

1. Event-10 goal test with the ring rule (`FUN_14050e4a0` case 10, §1.4). GF uses `IsGoalScored()`.
2. Ball-out side via the ring field and the throw-in flip for no eligible players (`FUN_14050e320`, `FUN_14050e020`). OPEN-1.
3. The `JR` record and the writer's mode override (`MI+0x1775`: kick-off forced for modes 1..7, 0xb..0x11, 0x17+; reason 3→4). GF has no mode override.
4. The step programme with its 40+ step codes and builders. GF has none (timers only).
5. Throw-in seamless path `[0x33]` vs ceremony (§4.2).
6. Corner spot 0.9 m inside the arc; goal kick at (±(F0−4.8), ±2.6).
7. Offside with category 2 DelayWhistle on newest event type 5 and `FUN_14050d7f0`.
8. Late card as step 6 (340-tick scene, max(2.5 s, scene)), window 16 game-minutes.
9. Period end programmes by `MI+0x1168` (`FUN_1404505d0`).

---

## 6. OPEN items (blocker and the test that settles each)

| id | question | blocker | settle by |
|---|---|---|---|
| OPEN-1 | Corner vs goal kick: type and side source | **settled (§9.1)**: side = (T==0); type = goal kick iff `dir_T·x ≥ 0`, with `T = *(ring+0x180700)` (raw disasm `0x14050e07a`, `0x14050e08e`). A defender's own-end-line touch gives a corner to the attacker (live 264.51 s). |
| OPEN-2 | exact guard on `FUN_1404537b0` at the end of `FUN_1404a56c0` | **settled (§9.4)**: `LAB_1404a5be8` → `FUN_14042d400` → `FUN_1404537b0` → `Flow+0x58d0 = 1`, reached only when JR restart ≠ 0 and the scene is not busy. |
| OPEN-3 | armed (0x1a) exit into the in-play park (0x3a) | **partial (§9.4)**: the scene branch of `FUN_140453530` pushes `[0x39, 0x3a]`; the non-scene branch pushes `[0x1c]`. The route from `0x1c` to `0x3a` is not traced. |
| OPEN-4 | what the reason-0 builder (`FUN_14044fef0`) means in play (it ends in `0x1a`) | **open**: no reason-0 stop sampled (§9.4). Live: a restart with reason 0 vs a play-resume. |
| OPEN-5 | `FUN_140a51cb0` goal-mouth margins | protected `.rdata` floats not read | read `DAT_142598958` and the `F[0xf]`, `F[0x10]` margin floats |
| OPEN-6 | goal kick release gate: counter 83 (setpiece §12) or 246 (§6.1) | **partial (§9.3)**: goal kick 238–253 frames from ready; counter 83 refuted; 246 inside range; exact gate open. |
| OPEN-7 | corner spot angle inside the arc | **partial (§9.5)**: placed spot fits `(sign(x)(|x|−0.3), sign(z)(|z|−0.8))` (0.05 m z residual); 70° in two live corners; the 30° DOC sample is not reproduced (whistle point off the line?). |
| OPEN-8 | contact-foul spot (`FUN_1405b8000`) and the penalty-mark replacement | **partial (§9.5)**: spot = caller point; box flag `FUN_140a72e70` bit `0x800` → restart 6 penalty spot; the box test and the bit meaning are not decoded. |
| OPEN-9 | does PES hold the taker until the opponents clear the area / 9.15 m? | **settled, no (§9.2)**: no opponent within 9.15 m during the corner and free-kick waits; a throw-in released with one at 4.45 m. The wait is the taker reaching the ball. Open: a second, opponent-dependent kick gate (not sampled). |
| OPEN-10 | period end cutscene length (GF 10 s) and the protected writer `0x143f5530b` | protected code | read the scene table length for the half-time cutscene |

---

## 7. Corrections to existing docs

- **TouchReach §2** says GF's `RestartPosition` "folds" x about 52.0 and y about 34.0. The current GF code clamps to F0/F1 (`offsiderule.cpp:156-160`). The fold claim is stale.
- **DOC foul §2.1 / setpiece §1.1** puts the half-start on stop kind `3`. The dispatcher (CONFIRMED `d_0x1404526a0.c`) sends reason 3 to `FUN_14044f8a0` (goal kick-off), and the half-start sample matches the reason-0 builder `FUN_14044fef0`. Fix the table's half-start row.
- **`pessetpiecetaker.hpp:19-21`** says throw-in and goal kick are "released by arrival/readiness, not gated here". The docs give counter gates (throw-in ≈81, goal kick ≈83). Update the comment and the gate.
- **§3.4 consequence (OPEN-1) is wrong.** The restart type depends on the last-player team `T = *(ring+0x180700)` through the dropped second argument of `FUN_140a48a30` (raw disassembly at `0x14050e07a` and `0x14050e08e`). Live check: §9.1.

---

## 8. Method and reproduction

- Bridge: `curl http://127.0.0.1:8080/decompile_function?address=0x…` (saved as `d_0x….c`). Xrefs: `xd://mcp__ghidra_get_xrefs_to` (the bridge's own `/get_xrefs_to` returned 404 for every address).
- Decompiled this session (scratch `<workdrive>/gf-A5Judge-scratch/`): `0x14050ee40`, `0x14050b490`, `0x14050de50`, `0x14050e770`, `0x14044c880`, `0x14044eb10`, `0x140445e90`, `0x1404500c0`, `0x14050e110`, `0x14050e020`, `0x14050e320`, `0x14050c490`, `0x140452260`, `0x1404524e0`, `0x14044d2e0`, `0x140447b50`, `0x140447230`, `0x140447690`, `0x1404534f0`, `0x14044e460`, `0x14050e4a0`, `0x14050dd10`, `0x14050ff50`, `0x14050df20`, `0x140446350`, `0x140451fd0`, `0x14044a2a0`, `0x14044bb40`, `0x1404488e0`, `0x140448760`, `0x1404485f0`, `0x14044d110`, `0x1404526a0`, `0x140422030`, `0x140423000`, `0x140452f30`, `0x14044cda0`, `0x14044f8a0`, `0x14044ff60`, `0x14044fb10`, `0x14044e720`, `0x14044fef0`, `0x1404505d0`, `0x14044ffd0`, `0x140452950`, `0x14044ef30`, `0x140449b00`, `0x140447230`, `0x1404537b0`, `0x14050c3e0`, `0x14050c490`, `0x140a3e880`, `0x140a3e190`, `0x140a3e150`, `0x140a51cb0`, `0x140449130`, `0x1404a56c0`.
- Callers read: `FUN_1404526a0`, `FUN_14050c3e0`, `FUN_14050cce0`, `FUN_14050e4a0`, `FUN_14050e110`, `FUN_14044d2e0`, `FUN_14044d110`, `FUN_1404514f0`, `FUN_1404a56c0`, `FUN_140445e90`, `FUN_1404a42e0`, `FUN_14050b490`, `FUN_1404537b0`.
- DOC evidence used (not re-decoded here): `pes-re-duels-fouls.md` §2–§5, `pes-re-foul-choreography.md` §1, §4, §9, `pes-re-setpiece-choreography.md` §1, §4–§8, §12, `pes-re-setpiece-referee.md` §1, §3, `pes-re-throwin-choreography.md` §1–§6, `pes-re-taker-map.md`, `pes-re-touchline-restart-stall.md`, `pes-re-restart-watch.md`, `pes-re-offside.md`, `pes-re-leftovers.md` §3.
- Not run: no Godot, no gfsim, no tests (per brief: agents do not run the gate). No engine code changed.

## 9. Live results (A5Live, 16-10-26): OPEN-1 to OPEN-9 against the running match

Method: read-only (`pread` on `/proc/<pid>/mem`). The endless patch was in place: `0x142647C68` and `0x142647C64` both read 999999995904.0 (1e12, f32). Samplers, in `<workdrive>/gf-A5Live-scratch/`: `a5_sampler.py` (run 1, 900 s), `a5_sampler2.py` (run 2, 900 s, adds scene, kicker and player positions), `a5_sampler3.py` (hold run, 900 s, HOLD every 0.2 s while `MI+0x116c == 3`). Logs: `a5_live_run1.log`, `a5_live_run2.log`, `a5_hold_run3.log`. Parser: `analyze_a5.py`. Runs 1 and 2 are one match (run 2 starts at run 1 t ≈ 142 s; the same ring frames), so their stops are not independent. Run 3 is a later window (frames 1028593–1064658).

Chains used (CONFIRMED live): `G = *(0x1436F3DC8)`; `MI = *(G+0x270)` (null on some ticks; sample again); `M = *(0x1436F3F78)`; `E = *(*(*(M+0x790)+0x18)+0xbd08)` (ring count `E+0x1900` = 8). Team record `*(G+(0xa1+t)·0x28)`, direction `s8(team+0x254)`. Ball: the `pes_state.py` ballstate signature (`+0x00` xyz).

Caveat (ball): on some ticks the fixed-address ball read aliases to another ball copy (run 3 at 450.48 s and 454.60 s: one sample each, between two samples at the same spot (52.25, −33.21)). Readings were used only when consistent with the set-piece geometry. `*(G+0x40)` (the judge's ball pointer, §9.6) was null in every sample.

### 9.1 OPEN-1 (corner vs goal kick; side source): CONFIRMED

- **Toucher field.** `T = E+0x180700` (team), `E+0x180704` (slot). Writer `FUN_140a3dc40`, ball-event message kind 7 (`d_0x140a3dc40_full.c` lines 289–320): copies the record to `E+0x180698` (newest) and, when the event's player ref passes `FUN_140a60530` (team < 2, slot < 40), copies it to `E+0x1806fc`. Any ball event with a valid player ref sets it; the event kind is not filtered.
- **Writer `FUN_14050e020`** (corner / goal kick, judge code 2). Raw disassembly (`dis_0x14050e020.txt`): `0x14050e07a MOV EDX,[RAX]` (RAX = `E+0x180700` from `FUN_140a3e880`), then `0x14050e08e CALL FUN_140a48a30` with `RCX = G` and `EDX` still `T`. The decompile shows that call with one argument; the second argument is `T`. So the attack sign is `dir_T = s8(team_T + 0x254)`, and:
  - restart `3` (goal kick) iff `dir_T · ball.x ≥ 0`, else `4` (corner);
  - side `= (T == 0)` (`SETZ SIL`): the awarded team is the team that did not touch last;
  - for `T ∉ {0,1}`, `FUN_140a48a30` returns `G+0x1928` (team 0's record).
- **Consequence.** The restart type is toucher-aware. The §3.4 concern (a defender's own-end-line touch gives the attacker a goal kick) does not hold: a defender's own-end-line touch gives a corner to the attacker.
- **Live (restart-write rows, `analyze_a5.py`, all `ok`):**
  - 264.51 s (run 1) / 122.74 s (run 2): corner, `T = 0`, ball x = −52.83 (team 0's own end line, `dir_0 = +1`), side 1 (team 1 takes). Law-correct.
  - 530.59 s (run 1): goal kick, `T = 1` (attacker), ball x = −53.25, `dir_1 = −1`, so `+53 ≥ 0` → goal kick, side 0 (team 0, the defender), spot (−47.76, −2.6). A team-0-direction rule gives a corner here, so that reading is rejected.
  - Throw-ins: 150.38 s (`T=0`, side 1), 252.88 s (`T=0`, side 1), 111.12 s in run 2 (`T=0`, side 1).
  - Kick-offs after goals: side is the conceding team in all four (47.89 s `T=1` side 0; 174.85 s `T=0` side 1; 375.58 s `T=1` side 0; 590.58 s `T=0` side 1).
  - Goal-kick spot: `(sign(x)·(F4+0.7), sign(z)·2.6)`; live (−47.76, −2.6) at 530.59 s and 388.83 s (ball z < 0). Consistent with §4.3.
- **Judge-tick re-decision** (`FUN_1404a56c0` swaps 3↔4 when `side == 1` and the scene `+0xc == 1`, §9.4): not observed. Scene `+0xc` read 0 at all 20 run-2 stops, and no restart changed within 2 s of its first write. OPEN for `scene+0xc == 1`, which was never sampled.

### 9.2 OPEN-9 (does PES hold the taker until opponents clear?): SETTLED, NO for every sample taken

- **Corner** (run 3, 448.86–455.27 s; taker = team 0 player 6): the taker walks from 4.1 m to 1.2 m of the spot. The nearest opponent stayed 26.8–28.9 m away for the whole 6.4 s wait. No opponent came within 9.15 m.
- **Free kick** (run 2, 216.21–222.45 s; taker = team 1 player 17): 0 opponents within 9.15 m at arm and at release (nearest 9.6–10.0 m). The taker walks 3.9 m → 1.9 m. The 6.2 s wait happened with no opponent in the zone.
- **Throw-in** (run 2, 117.64 s): released with one opponent 4.45 m from the taker. Throw-ins are not held.
- **Goal kicks** (run 3, 94.32 s and 551.69 s): 0 opponents within 9.15 m at release.
- Verdict: the wait tracks the taker reaching the ball, and the release is the taker's kick action (armed handler, §9.4). No opponent-clearance gate was seen. Remaining OPEN: whether the kick action has a second, opponent-dependent gate. Not sampled.

### 9.3 OPEN-6 and the set-play timings (frames, live): timings CONFIRMED; gates PARTIAL

Frames are `MI+0x1198` (match frame), measured from play 2→3 (set-play ready) to 3→1 (release). Counter is `MI+0x11AC`.

| restart | frames (live) | counter at release | DOC gate (§4.7) | verdict |
|---|---|---|---|---|
| kick-off 1 | 125–137 (×7) | 56 (×3) | receiver ownership | 28-frame DOC sample not reproduced; release is taker-driven |
| throw-in 2 | 118, 168, 171, 178, 194 | 88–110 | counter ≈81–83 | 81–83 **not reproduced** (88–110) |
| goal kick 3 | 238, 241, 253 | 147 (run 3) | 83 / 162 / 246 | 83 **refuted** (min 238); 246 inside range; gate OPEN |
| corner 4 | 365, 373, 379 | 282 | 189 (long) | 189 **not reproduced**; 379 matches one DOC sample |
| free kick 5 | 355, 356 | 0 (no counter) | 162 | 162 **not reproduced** |

- The release follows the taker's reach and kick action (§9.2), not a fixed counter. Kick-off always reads 56 and throw-ins 88–110, so the counter is per kind and is not a time base.

### 9.3a Goal → kick-off, live (Main, 22-10-26; read-only change logger on `MI` polled while non-null, the owner's endless match)

Four goal stoppages by /biz/ in 5 minutes, frames = `MI+0x1198` (54 per second). Raw log: the session scratch `goal_cycle.log`.

| goal (reason 3, play 2) | → ready (play 3) | → in play (play 1) | goal → in play |
|---|---|---|---|
| +1002 | +1759 (757) | +1884 (125) | 882 frames, 16.3 s |
| +3463 | +4224 (761) | +4349 (125) | 886 frames, 16.4 s |
| +7341 | +8184 (843) | +8309 (125) | 968 frames, 17.9 s |

- Ready → in play is 125 frames every time, inside §9.3's 125–137 (×7): the taker-driven kick-off.
- Goal → ready varies with the celebration (757–843 frames, 14.0–15.6 s).
- **Goal sequence, ported on `GoalTiming` (supersedes the 22-10-26 port item).** The fixed 19 s celebration and its 22.5 s goal → kick-off are gone. A goal plays a primary scene from the FixDemo goal pool (`common/demo/fixdemo/goal/table_goal.bin`, generic bit 6, goal level 0 as a stand-in), then a hug drawn from the scenes with bit 2 or 3 and level bits 12 or 13 (FUN_140474b20; all 60 hugs the install carries have bits 11, 12 and 13), both from the goal generator. The sequence lasts their sum (`pes-re-cutscenes.md` §4.3 item 4). Each scene's length is Σ(first cut `maxTime`/60 + fade) over its steps (`pes-re-foul-choreography.md` §9.2), read by `pes::fixdemo` through the VFS. The post-scene programme is `pes::RunGoalProgramme`: wipe 0.1 s, fade-in 1.0 s and tail 0.25 s (install), UI and teleport 0.98 s (GF's `foulsequence.hpp`): 2.33 s. Goal → armed = sequence + 2.33 s. Stand-ins, both `// ponytail:`: the level is 0 (FUN_140477400's goal-ctx[0] is not decoded), and the signature, edit-slot, golazo and mode branches are not ported.
- **The pool does not reproduce the live window (open).** Under that draw only 10% of goals land in 757–843 frames (level 0 13%, level 1 11%, level 2 8%; counted from the install's lists), with a mean near 922 frames. The three live goals in the §9.3a table (its text says four; the table has three rows) were all inside the window, which that draw makes about 0.1% likely, so PES's goal list is not that draw. Missing: the goal-ctx[0] level source and the other scenes of FUN_140477bc0 (items 1–4: pre-scene, team-mates, turn, coach). On the branch, seed 7's first goal misses the window (`AGoalIsReadyInPesWindowAfterItsScoring`, red, not re-pinned).
- **Kick-off release, resolved (Main, 23-10-26, `local://kickoff-counter-capture.md`).** The release is the taker's kick: MI+0x11AC is 47–56 at release (the counter runs only while armed) and arm-to-release is 118–139 frames. The KickoffReceiver's Update (`0x140781fc0`) passes the kicker only when `FUN_1407816b0` is true: MI+0x116C == 3, MI+0x11B3 != 0 and `FUN_140a3bdf0() − MI+0x11AC < −(int)(54·3.0 + 0.5)` (3.0 at `0x14258c264`, 0.5 at `0x142565600`), with `FUN_140a3bdf0` = 54·15 = 810: MI+0x11AC > 972. That gate cannot be the release. The port holds a goal's kick-off `kKickOffReleaseFrames = 125` frames after arming (PES's measured interval, no GF touch term; `pessequencer.cpp`). The capture's geometry is polluted (one outfield player per side), so its 47/56 split is not used.
- Same log: the kick-off kicker is the /xs/ CF (slot 21; a CF may take a kick-off, not a corner, §1.1 row 40); a throw-in (kind 2) went stop → ready 177 frames, ready → in play 332; a free kick (kind 5) 710 and 517.

### 9.4 OPEN-2, OPEN-3, OPEN-4 (armed exit, judge-tick restart guard): CONFIRMED structure

- **Armed handler `FUN_14044d2e0`** (step `0x1a`; `d_0x14044d2e0.c`): first, `if (Flow+0x49c0 != 0) { FUN_1404537b0(Flow); goto LAB_14044dac9; }`. A JR restart written while armed restarts the programme at step 1 (index 0) before the armed body runs. `FUN_1404537b0` (`d_0x1404537b0.c`) resets to step 1, copies kickers, and sets the side and the `Flow+0x27fc` rule.
- **Armed body**: the scene-busy path calls `FUN_140453530` (armed → play). The non-scene path releases on a taker kick action: a scan of 0x18 entries of the table at `G+0x180` (stride 0x74) with action byte `&0x7f` not in `{0, 0x12, 0x13, 0x3a}` sets `bVar24` and calls `FUN_140453530`. A time-out `FUN_140a43750` also releases. Action-byte meaning INF.
- **`FUN_140453530`** (armed → play writer; `d_0x140453530`, via bridge): if the scene is busy (`FUN_141f9c3b0`), it pushes `[0x39, 0x3a]` (`0x3a` is the judge park) and calls `FUN_1404a7130`. Otherwise it resets with `FUN_140451fd0(Flow, 9)` and pushes `[0x1c]`. OPEN-3 is partly answered: the scene branch parks at `0x3a`; the non-scene route from `0x1c` to `0x3a` is not traced.
- **Judge tick `FUN_1404a56c0`** (step `0x3a`; `d_0x1404a56c0.c`), the guard (OPEN-2 settled):
  - `Flow+0x17860 == 0` → return. Throttle `Flow+0x5818` (20-tick hold after a restart; `FUN_1404a6d20` at 0).
  - Bind (`FUN_14050b2b0`), judge update (`FUN_14050c3e0`), copy JR → `Flow+0x58a0` (`FUN_140a3bcf0`).
  - `JR.restart == 0`: no restart. Either the in-play body `FUN_140444a20` runs and it returns, or, with a pending advantage, the 20-tick hold `LAB_1404a5955`.
  - `JR.restart != 0`: return while the scene is busy (`FUN_140a86010(scene)`), or for restart 5 with reason 5 and `local_res18 == 0` (clears `Flow+0x58d0`). Otherwise refine (3↔4 re-decision, PK/FK conversion, §9.1) and reach `LAB_1404a5be8`: `FUN_14042d400`, `FUN_1404537b0(Flow)`, `Flow+0x58d0 = 1`. That is the restart start.
- **Live:** JR restart is 0 at every release (every PLAY 3→1 row) and nonzero in the same sample as the stop (every PLAY 1→2 row). The JR is consumed at play resume. CONFIRMED across all sampled set pieces.
- **OPEN-4** (a reason-0 builder in play): no reason-0 stop in the samples. Still OPEN.

### 9.5 OPEN-7 (corner spot), OPEN-8 (contact-foul spot): PARTIAL

- **Corner placed spot** (release samples: run 2 at 122.74 s and 141.38 s; run 3 at 455.18 s): (−52.25, 33.21) and (52.25, −33.21). Fit: `(sign(x)·(|x| − 0.30), sign(z)·(|z| − 0.80))` from the whistle point at the flag, with constants `DAT_142590d98 = 0.3` and `DAT_142572348 = 0.8` (read from the install with `va.py`). Residual 0.05 m in z (predicted 33.26). Relative to the flag at (±52.56, ±34.06) that is 0.90 m at ≈70°. Two corners, one with no designated kicker (`kick_idx` 255) and one with one (`kick_idx` 18), both placed at (−52.25, 33.21). The DOC 30° sample is not reproduced. The formula is ball-relative, so the angle follows the whistle point; a corner whose whistle point is off the line would show this. OPEN-7.
- **Contact-foul spot** (`FUN_1405b8000`, `d_0x1405b8000.c`): builds the foul record. The spot is the caller's point `param_2`. `FUN_140a72e70(F, x, z, dir_of(param_4))` returns flags; bit `0x800` (penalty box) → restart 6 with the penalty spot (`FUN_14050d250(…, 6, F, …)`), else restart 5 at the caller point. Then `FUN_14050ee40` (SetPending). Clamps are in `FUN_14050e770` (§2.1). The meaning of bit `0x800` and the box test inside `FUN_140a72e70` are not decoded. OPEN-8.

### 9.6 Judge ball pointer (caveat for §9.1)

- `FUN_14050e020` reads the ball x through `FUN_140a57800(*(G+0x40))` = `FUN_140a6cf70(obj, 0)` = `obj+0x88`. `*(G+0x40)` was null in every live sample, so the judge's ball could not be read directly. §9.1 used the sampler's ball and the spot sign, which agreed at both stops checked.

### 9.7 What changed in this document (A5Live)

- §0 bullet 1 (OPEN-1), §1.5 row 2, and §3.4 (first bullet and consequence paragraph): the goal-kick / corner type is `dir_T·x` with `T = *(ring+0x180700)`, not team 0. The §3.4 consequence paragraph is withdrawn.
- §4.2, §4.3, §4.4, §4.5 and the §4.7 table: live frame timings and counters replace the DOC gates where they differ (§9.3).
- §6 OPEN table: statuses set. OPEN-1 and OPEN-2 settled; OPEN-9 settled (negative); OPEN-3, 6, 7, 8 partial; OPEN-4, 5, 10 unchanged.
- §7: correction of the §3.4 consequence.
- Not done: OPEN-5 (goal-mouth margins), OPEN-10 (period-end scene length), the meaning of bit `0x800` in `FUN_140a72e70`, and the 3↔4 swap under `scene+0xc == 1`.
