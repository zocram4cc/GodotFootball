# pes-port-StallLive15: the PASS_GET target and the press-speed cause measured; neither landed (ALiveBall still red at 2,401); the census diagnostics landed (StallLive, 20-11-26)

Status: PARTIAL. Goal NOT met: PesStall.ALiveBall red at **2,401** forced ticks (round 14 tree; baseline 223). Branch `StallLive`, HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (ls-remote confirmed). Build dir deleted. Worktree clean (`src/` and `tests/` equal HEAD).

Commits this round (pushed):
- `1b56b726` PES stall census: the press man's issued blend group, rate and the member's natural root speed (diagnostic only: `PesClipNaturalSpeed` in humanoidbase, the census fields). No sim behaviour change.

Not landed, on measured evidence (patches saved in local): the PASS_GET target fix (raises ALiveBall 2,401 -> 2,880) and the red speed and designated-man tests (see section 5).

## 1. Final-tree acceptance (1b56b726; sim behaviour = round 14's 36e7451a)

| Check | Result |
|---|---|
| PesStall.ALiveBall | **RED: 2,401 forced ticks** (census on this tree) |
| PesStall (other 7) | PASS (round 14) |
| PesHang seed 11 | **PASS** (1,042,782 ms) |
| Throw-in 9/9 | **PASS** (8 in one run incl. ankle, + ACeremony PASS 1,913,042 ms) |
| PesPassTarget | **PASS** |
| Press: ALoosePresserCommandNamesTheActionPressTarget | PASS |
| Press: ADesignatedManWithNoHolderIsCommandedToTheRestingBall | **RED: 6 of 563** (round 14; open, section 4) |
| Kick-clear mutation (match.hpp:130 removed) | **RED**: receiverTicks 0 vs 0, foreignNamedTicks 0 vs 0; reverted, match.hpp = HEAD |
| gen_translated --check | GREEN: 10,150 functions byte-identical |
| ledger --check | GREEN: 47,642 rows |
| Seed-7 2-min smoke | **50,435 ticks**, determinism dae72e72a5e831e2 (identical to round 14) |

Smoke note: 50,435 is not "toward ~30k". Round 12's 29,597 came from a tree before the holder and aim changes; the sim now runs a different (still deterministic) trajectory. Not tuned to a number.

## 2. Item 1, the non-designated / PASS_GET pressers with no target: decoded, one bucket fixed in test, fix NOT landed

Mechanism (CONFIRMED by code and census):
- `Think()` (pesmovestep.cpp:275) sets the decision to the **first Movement command** in the controller's queue. The press approach is pushed late, so any earlier Movement command wins.
- The census (`STALLPRESSER`, pg/des flags, 2,401 forced firings): usetgt 0 buckets: **pg=1 des=1: 775** (the PASS_GET receiver who is also the designated man: 274 at 1-2 m cmd 1.39, 501 under 1 m cmd 0.23); pg=0 des=0: 139 (112 at 1-2 m, 26 at 2-5 m); pg=0 des=1: 27.
- The PASS_GET approach (`_PassGetApproach`, elizacontroller.cpp:1179-1206) computes a receiving point and steers by direction and speed, but never sets `useDesiredTarget`. MOVESTEP then projects the direction a second ahead without a target: the same overshoot class as the presser.

Fix tried (PES-faithful: ActionPassGet steers by its receiving point): name the target. Results:
- Designated-man test (now including the PASS_GET receiver): **381 of 938 -> 5 of 2,161** unnamed.
- ALiveBall on the same tree: **2,401 -> 2,880** with the target named; **back to 2,401 with it reverted** (two runs, same code otherwise). So naming the target makes the goal metric worse by 20%. **Not landed.** Patch: `local://pes-port-StallLive15-passget-candidate.patch` (the elizacontroller diff, reverted in the tree).

Why it may raise the stall count (INF, not traced): the receiver now walks to the receiving point and arrives at the ball while no man is commanded to trap it; the stall moves from "stood near the ball" to "arrived and idle". The trap/receive admission for a PASS_GET receiver at rest is the next thing to decode.

## 3. Item 2, the body speed at ~3.9 m/s command, playback 1.00: cause located, fix NOT landed

Census (the new fields, command >= 2 m/s, usetgt 1, 2,138 + 68 firings):
- **grp -1 (SetMotion, no blend group): 2,138 firings: command 3.25 m/s, body 1.39 m/s, member natural speed 1.22 m/s, playback 1.05.** The body moves at the issued clip's own natural speed, not at the command.
- grp 80 (parametric, the walk-to-run blend): 68 firings: command 3.73, body 1.33, natural 1.32, rate 0.000, playback 1.00.

So the gap is not the blend stand-in (only 68 of 2,206 firings are blends). It is the SetMotion path: MOVESTEP issues a locomotion clip whose natural speed is about 1.2 m/s, at a playback of about 1.0, for a 3.25 m/s command. The playback is set by MOVESTEP's own playback ladder (pes/movestep.hpp: `kmh172`, `rate055`, `foot075`, the `stepBase` / `stepSlope` ramp), and the motion by its speed-class table. That selection is what does not match PES. Decoding FUN_1406BA070 / the EnterAccelerate playback ladder and the motion table is the fix; not done.

Measured speed test (red, local patch `local://pes-port-StallLive15-redtests.patch`): a targeted presser at >= 3 m/s has median body/command **0.387 over 10,329 samples** (threshold 0.7).

Killed in this round:
- K1. "The blend stand-in's playback is the cause" (round 14 K4 leaning): the blend is 68 of 2,206 firings; the bulk is SetMotion.
- K2. "The root-motion physics scale (`pluginScale`) explains the gap": the physique scale is about 1.0 for these players (anim-blend doc 7.1).

## 4. Item 3, the designated centre-forward fall-through (zero speed, no target, ~1 m): not decoded
- 5 to 6 ticks of 2,161 (role 9 = CF, zero-speed Movement, no target, 0.6-1.2 m, after the PASS_GET fix). The decision is the first Movement in the queue; the approach for a designated in-reach man is pushed at elizacontroller.cpp:669 and is named. So either the designated man at Think time differs from the one the test reads (a man changes designation in the tick), or an earlier Movement (ReactionCommands, the fall-through) wins. Not separated.
- The 6 ticks keep `designated-man` red (HEAD test). They are 1.4% of the in-reach ticks.

## 5. Tests and patches
- Committed red test on HEAD (from round 14): ADesignatedManWithNoHolderIsCommandedToTheRestingBall, 6 of 563. Open.
- Local, NOT committed: `pes-port-StallLive15-redtests.patch` (designated test including the PASS_GET receiver: 381 of 938 red; speed test: median 0.387 red). Reason: committing more red tests on a branch that already has one red is not the state to leave the branch in before the PASS_GET trap decode.
- Local, NOT committed: `pes-port-StallLive15-passget-candidate.patch` (the target naming; empty in the tree after revert, see the patch file for the diff text).

## 6. Open items (numbered)
1. PASS_GET receiver at rest: the trap/receive admission (why the arrived receiver does not trap a resting ball). This is the reason naming the target raises ALiveBall. Decode `_TrapCommand` / the receive entry (elizacontroller `_ReactionCommands`, playercontroller `_TrapCommand`) against PES's receive entry FUN_1409F8220.
2. SetMotion locomotion: MOVESTEP's playback ladder (EnterAccelerate, FUN_1406BA070 / FUN_1406E9F10) and the speed-class motion table. The body speed gap (section 3) lives here. Largest single lever for ALiveBall: 2,138 of 2,206 forced firings are SetMotion.
3. The centre-forward fall-through (section 4).
4. Non-designated pressers with no target (139): not traced beyond `_ReactionCommands` (the reaction's Movement pushes are the next check).
5. Commit the designated-test and speed-test reds once item 1 and 2 are decoded, or delete them with a reason.

Verification summary: census and tests run on the stated trees; all numbers above are from the logs in `gf-StallLive-scratch/r15` and `final15`.
