# PES port: KickQ (branch KickSoftMin): the situation factor Q = Base x G replaces `kSituationNotPorted`

## Verdict

Q is computed as PES computes it (`FUN_14083CC30`, `0x14083DF54..0x14083E69C`, Base and G) from PES-layout inputs, and
`kSituationNotPorted = 1.0` is gone. Against PES's own Q (xmm13 at `0x14083E618`, where every path of the Q block rejoins),
**GF's Q is bit-exact on 1539 of 1539 compared replay rows, 0 differing** (7 real kick_scatter captures + input sweeps, below).
Not closed: (1) PES's *in-flight* branch (motion-descriptor byte 1, subclass 2..12) is ported only for its identity case
(ball record height <= 0.326; all 7 captures are in that case); (2) the GF side has no PES producer for most Q inputs, so the
call site supplies named `ponytail:` stand-ins (list below); (3) F1/F2 keep a separate stand-in (`kMotionFactorNotPorted`).

Branch `KickSoftMin`, head `7fe04160` (confirmed by `git ls-remote`), merge of `origin/main` (`cf297e94`) first.

## What was decoded (all CONFIRMED by replay unless marked)

Q = `xmm13` of the scatter. Registers: `r12d` class, `[rsp+0x5c]`/`ebx` subclass, `edi` kind, `r13` = `ap`, `r15` = motion object
(`ap+0xAB8`), `r14d` = bucket byte `uVar14` (`ap+0x2755` after overrides), `[rsp+0x4c]` = `ap+0x2754`, `[rsp+0x50]` = first-time byte.

**Base** (`0x14083DF76..0x14083E05E`): classBase[class] (0->0.9 `0x14258C260`, 1->0.88 `0x1425DDF70`, 2/3->0.86 `0x1425EB850`,
else 1.0 `0x14256AE1C`) x subMult[sub] (jump table `0x1408400A0`: 1 x0.95 `0x1425AB124`, 5 x0.8 `0x142572348`, 6 x0.85 `0x1425AB120`,
7/0xB x0.9, 8 x0.7 `0x14257A194`) x gate (`!P240F0(kind) && !AI && (P24590||P26610||P26640)(kind) && motion != 0x9F1`:
class<2 x0.95, class 3 x0.925 `0x1425EF9DC`, else x0.9). Class = row byte 13, subclass = row byte 12 of `0x14344E380 + 16*kind`
(out-of-range row `0x14344E620`). The four predicates are evaluated at load from their own code and tables, opcode bytes checked:
`0x1407240F0` (jump table, bytes `0x140724124`, dwords `0x14072411C`, true target `0x140724116`), `0x140724590` (two `lea/cmp`
ranges), `0x140726610` (three), `0x140726640` (bit mask `0x41A0000` over kinds <= 0x1A and a range).

**G** (`0x14083E063..0x14083E69C`, with the window block `0x14083CDCD..0x14083CF07` that feeds it):
1. Speed (km/h) = `((tickRate*60)*60)/1000 * sqrt(ap[0x2488]^2 + ap[0x2480]^2)`; tickRate = 54.0 `0x14259D624` (slot `0x14351AE54`,
   getter `0x141E532D0`). `ap+0x2480/+0x2488` are metres per tick (INF: values ~0.008 m/tick in a standing kicker).
2. Kick angle u = |fold(bearing(launch) - wrap360(ap+0x247C))| (`0x14083D195..D231`, `[rsp+0x78]`); threshold
   T = 11.0 (`0x14259BDFC`) + ramp(u; 50..100) x {class 0: 2.5 `0x14259BDE4`, class 1/3: 5.0 `0x14259222C`, class 2: 11.0}. If speed > T:
   speed = (28 - T) x `ap+0x281C` x 0.01 + T (`0x14083E214..E23E`).
3. Body turn = |fold(ap+0xBE4 - wrap360(ap+0x247C))|; t = ramp(turn; 45..135, **divisor 90.0**) (`0x14083DF47 MOVSS XMM15,[0x14259BE5C]`;
   the kicks doc's decompile reading was right, the first asm reading (57.2958 from `0x14083D622`) was a stale xmm15); low = 22 - 8.5 t100/100,
   high = 28 - 6 t100/100; Q *= 1 - ((1-a)*0.6 + 0.1) * ramp(speed; low..high) (`0x14083E2C1..E314`).
4. Subclass switch (table `0x1408400D4`): 0/1/9/10: Q *= 1 - t^2 * 0.3; 2..7,0xC: bucket 0/2/0xF/0x11: Q *= (double)a*0.3 + (first==0 ? 0.3 : 0.2)
   (f64 `0x14259D5E0`, `0x1425EF868`); bucket 4/0xD: Q *= 1 - {class 2: (1-a)*(first==0 ? 0.1 : 0.15) + 0.05; else (1-a)*(first==0 ? 0.05 : 0.1)}.
5. Tail (`0x14083E691`): subclass 0/1 and `ap+0x2754 == 9`: Q *= first==0 ? 0.9 : 0.85. This is the whole 0.9 of the class-3 header capture
   (seq036: Base 0.7955 x 0.9 = 0.71595); the kinds doc called it "class-3 G".
6. Bucket: `ap+0x2755`, overridden by motion id (0x9BE -> 0xF; 0xA20/0xA7D -> 0xD; 0x9B0/0x9B4/0xA41/0xA42/0xA45 with kind 0x2B/0x3A -> 9), then by the
   first-time window when `ap+0x2764 < 360 && (first == 1 || class == 2)`: d = `FUN_1404B6390(window, bearing(launch))`, negated when the motion
   object's `vtable+0x30` (touch state) is set; class 0/1/2: d > 157.5 -> 0x11, else d > {0: 130/125, 1: 120/115, 2: 115/110} (first byte 0 / != 0) -> 0xF.
   So `ap+0x2764` is an **angle in degrees** (720 = none), not a timer. The kinds doc's "G = 0.99625 (first-time window)" is bucket 0xD (class 0, first != 0): 1 - (1-a)*0.1.
7. **In-flight branch** (`0x14083E42E`: `FUN_1406B0250(motion table, motion id, 0) == 1`, subclass 2..12): `0x14083E447` loads the ball record's
   float +4 (height, 0.1085 m in the captures); at or under 0.3260578 (`0x1425CB2A8`) the ramp is 1 and Q *= 1 exactly. Above it the branch
   multiplies Q by `1 - (ramp(speed)*0.9+0.1)*(1-g^2)*{0.8|0.4|0.6}` with ball speed (`FUN_14042D470`), P1407245D0 and `FUN_1406EAD40`: OPEN, not ported.
   **6 of the 7 on-disk captures take this branch** (descriptor byte 1) with the ball on the ground; 1 (seq036, subclass 0) does not read the descriptor.

## Code

- `src/sim/onthepitch/peskickerror.{hpp,cpp}`: `SituationConstants` (every float read from the install, VA in the comment), kind table and
  predicate evaluation at load (`LoadSituationConstants`, `LoadGatePredicates`; opcode bytes checked before each immediate), `KickSituation`
  (PES layout), `KickBase`, `SituationFactor`, `PesHeadingDeg` (PES's own translated `FUN_14046F230`). `BuildScatter(..., situation)` takes Q;
  `kSituationNotPorted` is deleted; F1/F2 use `kMotionFactorNotPorted` (separate stand-in, `FUN_1406B0250` motion table).
- `src/sim/onthepitch/player/humanoid/humanoid.cpp`: `PesKickSituation` builds the record from GF's kick; `ApplyPesKickError` passes `SituationFactor` to `BuildScatter`.
- `tools/dev/rendezvous/kick_replay.py --q OUT.tsv [--q-count N --q-seed S --q-focus] SNAP...`: replays each capture with input patches written at the
  instruction that first reads each field, records inputs and PES's Q as f32 bits (+ descriptor byte, ball height). PES-derived output stays in scratch.
- `tests/sim/pes_kick_error_test.cpp`: `PesKickErrorOracle.SituationFactorEqualsPesBitForBit` (env `PES_KICK_Q_TSV`; skips when unset; fails on an empty or differing TSV),
  `PesKickErrorInstall.SituationBaseGateAndAUnhurriedKickKeepsTheBase` (relations on the install's table: gate multiplier is one of the install's three, AI bit / motion 0x9F1 switch it off,
  a still aligned perfect kick keeps Base, all 150 kinds), `...FallsWithTheKickersSpeed`, `...HeadingIsPesBearing`.

## GF-side stand-ins (each a named `ponytail:` in humanoid.cpp, naming the PES site that removes it)

kind (first candidate of the request's list: shot 0x69, ground pass 0x21, lofted pass 0x49, header 1; PES chooser `FUN_1408364D0`/`FUN_140829E80`),
`ap+0x2754` = 0xC and `ap+0x2755` = 0x12 (defaults; `FUN_140723DE0` bucket not ported), first-time byte 0 (`FUN_140837FC0`), window = 720.0 (W1 immediate `0x1406EB6FC`,
read from the install), AI bit 0, motion id 0, touch 0, `ap+0x281C` = 0. Heading and body turn are GF's direction vectors (PES z = -GF y); speed is GF's movement / tick rate.
These stand-ins mean GF's Q is Base x (speed, turn terms) for the chosen kind; it matches PES only where PES's inputs agree.

## Oracle counts

Replay rows (`q_all.tsv`, scratch): 1814 = 7 on-disk kick_scatter captures (pid407792: seq017, 022, 031, 036, 048, 067; pid1467004: seq017) + 3 x 400 sweeps
(150-kind walk + 250 random inputs, bases seq017/pid1467004, seq036, seq031) + 4 x 150 focus sweeps (subclasses 2..7/0xC, motions with descriptor != 1, bases seq017, seq036, seq031, seq022).
**1539 compared, 0 differ; 275 skipped** (in-flight branch with the seq036 base: ball 2.54 m, not ported). All 7 captures compared, 6 as identity-branch rows (ball 0.1085 m).
`seq4471` and `seq4774` (only in `kick_oracle.json`): the JSON holds kind, bytes and soft-min args but not the speed, launch vector, quality `a` or touch state, so Q cannot be
recomputed: not compared. pid1467004 holds one kick_scatter (seq017); no newer one had arrived.

## Mutations (each compiled as a scratch mutant of peskickerror.cpp, linked ahead of the library, run on the TSV; repo source untouched)

35 of 40 killed. Killed (oracle red): M0 Q=1.0 stand-in (3 tests red), class 0 base, sub 1/5/6/0xB/8 multipliers, gate class<2, gate class 3, AI bit, motion 0x9F1, P240F0, P26640, speed unit
(tick rate), class-0 gain, stamina term, low knee, effK, subclass 9 turn^2, turn squared, a-branch first/later, bucket 0x11, bucket 0xD, b-branch class 2 and other, tail byte,
tail later, touch negation, motion 0x9BE, motion 0xA20, angle fold, heading wrap, kick-angle reference, window class-2 gate, class-2 window limits.
**Survived (5):** M27 window 157.5 -> bucket 0xF instead of 0x11: *equivalent* (both buckets take the same branch in Q). M29 / M38 class-1 / class-0 window limits swapped: needs
d inside a 5-degree band (115..120, 125..130) with the right bucket; not drawn. M36 motion-A set cut to one id: needs kind 0x2B/0x3A with those motions and an in-flight-identity base; not drawn.
M39 `window <= 360`: needs the window to equal 360.0 exactly; not drawn. Next sweep: aim d at the thresholds (choose window = bearing - threshold +- eps), kinds 43/58 with motions 0x9B0..0xA45.

## Tests run
- pes_* sweep (every binary that links peskickerror, 23 of the 60 in tests/sim; each in a sim slot, 2 lanes, 900 s cap, build at `7fe04160`, `PES21_ROOT` + `PES_KICK_Q_TSV`):
  21 binaries rc=0, 0 failures: pes_clock_match 102 s, pes_defensive_shape 27, pes_event_wiring 4, pes_challenge_counters 3, pes_holder_latch, pes_image 19, pes_kick_error 18 (+1 skip), pes_mark_table,
  pes_forfeit, pes_nearest_opponent, pes_movestep_wire, pes_period_end, pes_playerdb_wiring, pes_press_approach, pes_restart_taking, pes_setpiece_stand, pes_stall, pes_stoppage_timing, pes_tick_a1, pes_tick_order
  (each printed PASSED; per-binary lines in the sweep log, scratch).
  **3 not green, all match-level probes whose seeded trajectory changed because the kick scatter is now narrower/wider by a real Q:**
  1. `pes_defender_decisions_test` 28 passed, 1 FAILED: `PesDefenderJostleGf.SwitchOffMatchJostlesForAPass` (seed-7 two-minute match must reach a pass jostle; test line 531). With `Q = 1.0` (mutant M0 linked into the same test) it passes 29/29.
  2. `pes_throwin_release_test` 3 passed, 1 FAILED: `PesThrowInRelease.AThrowInIsReleasedFromInsideTheLine` (a released throw-in lay outside the touchline at the touch, `outside` = 1; test line 121). With `Q = 1.0` it passes (all 3 tests, 383 s per match test). Needs a look: the new trajectories reach a throw-in the old ones did not, so this may be a latent release-position bug, not a Q bug; not diagnosed.
  3. `pes_restart_delay_test` timed out at 900 s (rc 124, no summary): the known stall binary class (brief: a timeout is a finding); not run with `Q = 1.0`, so not attributed.
  Main decides on 1 and 2 (they are probes of a seed, not of Q): re-seed, or fix the release position.
- `pes_kick_error_test` with `PES21_ROOT` and `PES_KICK_Q_TSV`: 18 passed, 1 skipped (the soft-min oracle needs its own TSV). Also green earlier on this branch without the TSV.
- Gate not run (Main gates). `ninja -t deps`: no `#deps 0` object after the header change.

## Open items

1. In-flight branch above the knee (ball > 0.326 m: headers, volleys): needs the ball record (`FUN_140A6CF70(ball, 1)+4`), ball speed (`FUN_140A57650` -> `FUN_14042D470`), `FUN_1406EAD40(ap, 0xB/0xC)` and the descriptor byte (motion table singleton `0x1436F23C0`).
2. The five surviving mutants (aimed sweep above).
3. GF producers for the stand-ins: kind chooser `FUN_1408364D0`, bucket `FUN_140723DE0`, first-time byte `FUN_140837FC0`, AI bit writer, motion id, `ap+0x281C` (writer `FUN_1406F6790`).
4. seq4471 / seq4774 need a replay (inputs not in `kick_oracle.json`); more kick_scatter captures would extend real-capture coverage (all 7 are ground balls).
