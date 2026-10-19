# S1Port: receiver enumeration and ball-track arm

> **Main audit 19-10-26:** `0x14063AA79` `movzx ecx,byte [rax+0x31]`, then `divss [0x14253EE30]` (100.0) and `comiss` against `movss xmm0,[rdi+0x44]` at `0x14063AA74`: matches (P+0xA1 given RAX = P+0x70, the agent's reading). Correction: OneTwo is not held by G3 (ported since A4LaneInputs); per A4Level's probe it dies at G12.
> **Main audit 19-10-26 (round 2, `90c94a3`):** `FUN_140587E70` calls `0x140A714C0` (Safety gate 4 reads the score row): confirmed. Safety now dies at gate 5 (backward mates; PES candidate set not compared). B+0x4C writer open (handed to RE for a live watch).
> **Main audit 19-10-26 (round 3, `666328b`):** the ball-track arm is AND, confirmed in raw asm `0x14063AAFA` `cmp [r12+0x4c],r13b; je skip` / `0x14063AB01` `test r14b,r14b; jne skip` / `0x14063AB06` `movb [rdi+0x6e],1`. Safety candidate set = PES receiver status 0x0B (0x142117C30). Units still silent.

## Commits (pushed to S1Port, fast-forward)
- `adfd230` Port PES's receiver enumeration as the carrier's candidate set
- `666328b` Read the ball-track arm's B+0x4C from the ball track

ls-remote (verbatim):
```
666328bc2e0f549a30e636c3967a836c6d846909	refs/heads/S1Port
```

## gtest (PES21_ROOT=<workdrive>/PES21)
```
pes_carrier_test (666328b):   [  PASSED  ] 175 tests.
pes_offball_test (666328b):   [  PASSED  ] 62 tests.
pes_scorerows_test (666328b): [  PASSED  ] 7 tests.
pes_lane_test (666328b):      [  PASSED  ] 21 tests.
pes_offside_test (adfd230):   [  PASSED  ] 17 tests.
```

## Determinism (final, unprobed code at 666328b; --minutes 2, seed 7)
```
[determinism] 9fc0b08314812d8f    (--pes-loop)
[determinism] 75a11cdb2b3c8ec5    (switch-off)
```
Both equal the baselines before this round, so neither port moved the smoke.

## Silence probe
Receiver enumeration (adfd230, --pes-loop, temporary probes removed after):
- rethinks 24. Safety, OneTwo, BackSpace and DHB are each silent 24/24 (fired 0). Unchanged from 90c94a3.
- statuses: 203 valid (11), 24 carrier (2), 34 predicted off (4), 3 busy (8). No 5, 6, 7, 9 or 10.
- candidates per rethink: 9 6 6 6 6 10 10 9 9 9 9 9 9 9 9 9 9 9 7 9 9 8 8 10.
- switch-off: 522 valid (11), 53 carrier (2), 5 offside (6), 3 busy (8).

Ball-track arm (666328b, temporary probes during the --pes-loop smoke, removed after):
- bounce sample valid in 0 of 24 rethinks; K+0x6E volatile in 0 of 24; the four units silent 24/24.
- So B+0x4C is never set in the 2-minute smokes; the keeper speed arm is inert there too.

## Red, then green
- Receiver: stub returned kReceiverValid, four value failures (predicted off, offside, busy, not-on-pitch/set-piece), then green.
- Bounce: stub returned no sample, two value failures (the positive bounce tests), then green.

## Mutations (each reverted, tag restored)
1. adfd230: CarrierOffsideLine without the 16-frame prediction. One failure: `ReceiverStatus.ALineManIsReadSixteenFramesAhead`. The other receiver tests passed.
2. 666328b: FirstBounceSample without the latch. Two failures: `CarrierBallTrack.ABounceBeforeTheBallHasBeenAMetreUpIsNotSampled` and `CarrierBallTrack.ARollingOrFallingBallHasNoSample`.

## Doc conflict for Main and S1Watch
`pes-re-ball-track.md` §7 (and `docs/re/pes-re-ball-track.md` on main §7) gives K+0x6E := 1 by arm B iff `K+0x6C == 0 ∧ B+0x4C ≠ 0 ∧ (B+0x80 == 0 ∨ ¬(d2 > d1))`. The decompile of FUN_14063A490 (lines 274–286) and the asm at 0x14063AB01 give AND: `cVar4 = B+0x80`, the distance block runs only when it is 0, and arm B needs `cVar4 == 0`. B+0x80 != 0 blocks arm B. The port follows the decompile, which VolatileBall already encoded. §7 needs correcting.

## Stand-ins left (each named in code)
- K+0x40 (the ctx4 point). FUN_14063A490 §5 has five cases that read PES's ctx4 and per-player records. GF has none, so the ball's own point stands in, and the distance test never blocks.
- B+0x80: no GF state, kept clear.
- The sampler's vec[4] (vertical velocity): the first difference of the height stands in. GF's track carries heights only.
- hu+8 bit 2 (`publishedFlag`): no GF source, false.
- Set-piece taker: GF's single piece taker stands for MI+0x173C/+0x1740.
- Busy classes: GF's `AnimeClassOf` maps only trips (16/17) onto a busy class. The install's class-word bits 9 and 27 are read exactly.
- Keeper test on the offside list: GF's GK role stands in for FUN_140A638C0's team keeper slot.

## Behaviour changes (PES-faithful)
- GF's filter in the mate loop is deleted: the GK skip, the ball-to-target distance cut, and `AI_ReceiverIsOffside`. `OffsideRule::ReceiverIsOffside` sorted every opponent by predicted x and kept keepers. Its tests are replaced by value tests in pes_carrier_test.cpp.
- Keepers and mates beyond 52.56 m from the ball are now candidates when PES's statuses allow. PES's pass gates decide the rest.

## Open
- The four carrier units stay silent (24/24). The enumeration and the arm's flag are ported, and neither un-silences them in the smokes. Their remaining inputs are the blocker.

## Docs touched
- `docs/re/pes-re-offside-port.md`, `docs/ports/offsideawareness.md` (adfd230)
- `docs/re/pes-re-carrier-port.md` §3.4 and §4 (666328b)
