# pes-port-StallLive14: the throw-in aim and PES holder landed; the designated man in reach now approaches; throw-in 9/9 with the ankle; ALiveBall 223 -> 2,401 (red); the item 1 blend and item 2 clear are scoped, not ported (StallLive, 20-11-26)

Status: PARTIAL. Acceptance NOT met: PesStall.ALiveBall is red at 2,401 forced ticks (baseline 223, and 3,104 on the A+B tree this round). One press test is red on purpose: 6 of 563 in-reach movement ticks name no target (section 4). Branch `StallLive`, HEAD **36e7451a0592502fa6e28ce116b0b539ccdfd20f** (ls-remote confirmed). Build dir deleted.

Commits this round (pushed, in order):
- `5c34f4a3` PES throw-in: the ThrowinLoop stand turns the lifted taker to the throw's aim. **Disclosure:** the commit tool did not split the file. This commit carries both the aim turn and the PES-holder carrier brain (`hasIt`, below). The subject covers only the first. The body was written for the first only.
- `05cb00d4` PES press: a designated man in reach of a resting ball takes the ActionPress approach (`_PressApproach`, the second test red on 6 ticks).
- `36e7451a` PES stall census: the press line prints the press man's body playback rate.

Ruling applied: A+B landed (the holder carrier brain and the aim turn), then the faithful fix for what A+B exposed.

## 1. Acceptance on the final tree (36e7451a; src = the fix tree)

| Check | Result | Notes |
|---|---|---|
| PesStall.* (8) | 7 PASS; **ALiveBall FAIL: 2,401 forced ticks** | baseline 223; A+B tree 3,104 |
| PesHang seed 11 | **PASS** (1,124,560 ms) | |
| Throw-in 9/9 | **9 PASS** | 8 in one run (ankle **PASS**, 0 short) + ACeremony PASS (1,877,959 ms) |
| PesPassTarget | **PASS** | |
| Kick-clear mutation (`pesKeyActor = nullptr;` removed, match.hpp:130) | **RED**: receiverTicks 0 vs 0, foreignNamedTicks 0 vs 0 | reverted; match.hpp equals HEAD |
| gen_translated --check | **GREEN**: 10,150 functions byte-identical (3,824 bit-exact, 6,326 closure) | run on the fix tree |
| ledger --check | **GREEN**: 47,642 rows, no regression vs HEAD | run on the fix tree |
| Seed-7 2-min smoke | 50,435 ticks, determinism dae72e72a5e831e2 | round 12's 29,597 was a different trajectory; this one is the fix tree's |
| PesPressApproach.ALoosePresserCommandNamesTheActionPressTarget | **PASS** | |
| PesPressApproach.ADesignatedManWithNoHolderIsCommandedToTheRestingBall | **RED: 6 of 563** | open, section 4 |

## 2. The holder carrier brain and the aim turn (landed as one commit, `5c34f4a3`)
- Aim turn (elizacontroller.cpp ThrowinLoop branch): the stand turns to the throw's aim. Seed 25 reaches full time (35,025 ticks, determinism f2bede24e50292b2 on the aim-only tree).
- Holder (`hasIt` under the PES loop): the carrier is `GetPesHolder()==player`. PesHang seed 11 passes on the A+B tree (625 s), seed 25 full time (50,737 ticks, determinism 3c20f11cef81e7d1).
- Cost: ALiveBall rose 223 -> 3,104. Section 3 is the fix for that.

## 3. The press fix (`05cb00d4`): what it changed, measured
- The cause (MEASURED, `[stalemate-cmd]` and the A+B census): with no holder, a designated man within a second of the ball took the dribble branch. Under the PES loop that branch queued nothing, and the PES approach (`ActionPress` target and speed) ran only in the out-of-reach `else`. So the man beside the ball stood with command speed 0 or a target-less walk.
- The fix: one `_PressApproach` function. The in-reach branch calls it whenever the PES loop has no holder and the man is not the PASS_GET receiver. The out-of-reach block calls it too. PES's target and speed formulas are unchanged.
- Designated movement ticks in reach naming no target: **396 of 872 -> 6 of 563** (the 6 are open, section 4).
- ALiveBall forced ticks: **3,104 -> 2,401**. Still red.

## 4. Open: the 6 ticks, and the 2,401 live-ball firings
- The 6 red ticks in the new test: one centre forward (role 9) at about 1 m, zero-speed Movement with no target. They come from a fall-through that did not reach the approach. Not decoded. The second test stays red until it is.
- Classification of the 2,401 forced firings (A+B+fix census, STALLPRESSER): presser at 1-2 m with usetgt 0: 406 (command 1.6 m/s, body 0.9); presser at <1 m usetgt 0: 518 (command 0.47, body 0.57); presser at 1-2 m usetgt 1: 452 (command 3.6, body 1.5); presser at 2-5 m usetgt 1: 345 (command 4.4, body 1.5); presser at <1 m usetgt 1: 405 (command 1.6, body 1.6). So roughly half the remaining firings are presser men with no target that are not the designated in-reach case (they are non-designated men whose command the press block does not name). Not traced in this round: the press block runs only in the non-designated branch, and its `pressTarget` reaches `_MovementCommand` only on the `!manualMovement` path (elizacontroller.cpp ~921-954). The gate that leaves these men without a press target is the next decode.

## 5. Item 1 (SetParametric stand-in): measured, not ported
- The blend filler FUN_1406F14E0 is already translated as a closure member (src/pes/translated/closure/pes_fn_1406F14E0.cpp; `pes_dispatch_gen.cpp:15676`), with "not recorded bit-exact on its own". What is missing is the path into GF's body: the PES anime engine receives only SetMotion pushes (`Match::PesAnimeIssue` from `PlayIssuedClip`); a SetParametric is never sent to it. The GF body plays a single clip (`PlayIssuedClip`), not the engine's pose. `ReadPose` has no caller in the sim. So replacing the nearest-member stand-in means routing the locomotion pose through the fox world (ReadPose) — a multi-round change, not this round.
- Measured (A+B+fix census, press man with usetgt 1, playback rate from the body's current anim): motion 2221: command 3.85 m/s, body 1.17 m/s, **playback 1.00**; 2222: 3.57 / 1.22 / 1.00; 3945: 3.90 / 2.27 / 1.00; 4152: 4.08 / 0.30 / 1.00. The stand-in's rate reaches the body (playback ≈ 1 when the member's speed matches), so **the speed gap is not the blend's playback**. It sits downstream: MOVESTEP's speed output or the clip's root-motion scale. Killed: "the single member's playback explains the slow body" (K4 of round 13).
- Not done: the measurement of the root-motion speed per clip (needed to find the downstream gap).

## 6. Item 2 (FUN_140633710, the target clear): blocked by OPEN writers
- Decode (pes-re-press-coord.md §8.5): the clear sets the presser's target to none when his score is under 3.0 or the bearing tests of two tables hold. The tables are T+0x22F7C (A) and T+0x23F7C (M). GF already marks both as writer OPEN (pescarrier.cpp:1983 and :2003: "the bearing stands in for the A_tab table, writer OPEN"). Porting the clear needs those writers decoded first. Not started.
- The "25 pressers 2-5 m with no target" of round 12 is gone on this tree (census: usetgt 0 at 2-5 m: 26, command 7.2, body 4.3 — pursuit-class commands, not the clear's shape).

## 7. Killed theories
- K1. "The blend stand-in's playback is what keeps the presser's body slow." Killed (section 5): playback 1.00 at matching rates.
- K2. "GF's HasPossession is what keeps the ALiveBall stall" (round 13 K3). Still killed: the holder change raised the count, and the approach change lowered it.
- K3. "The designated man in reach is a keeper or a slide case." Partly: 6 of the 8 unnamed in the diagnostic were slides (fn 13), specials (fn 14) and a tackle-class command, which the test now excludes; the keeper was excluded earlier.
- K4. "The 25 no-target pressers of round 12 are the target clear." Killed: that population is gone with the holder change; the remaining no-target men are another gate (section 4).

## 8. Open items (numbered)
1. The 6 zero-speed Movement ticks on a centre forward in reach (section 4): decode the fall-through; the second press test stays red until then.
2. The 2,401 ALiveBall firings: the non-designated presser with no target (section 4, the `!manualMovement` gate in elizacontroller.cpp ~921-954). This is the main remaining red.
3. Root-motion speed per clip and MOVESTEP's speed output: where the body loses ~3/4 of its commanded speed (section 5). This is the item 1 measurement the blend port would not fix.
4. Route the locomotion pose through the fox world (ReadPose) so a SetParametric blend plays in the body (section 5). Multi-round.
5. Decode the A and M table writers (T+0x22F7C, T+0x23F7C) before the target clear (section 6).
6. Decide whether the commit `5c34f4a3` (aim turn + holder, one commit) stands; its subject understates it.
