# pes-port-StallLive13: the throw-in loop's aim turn (seed 25 fixed, PesHang seed 11 not), the seed-11 stall is a live ball, the press man's issued motion (StallLive, 20-11-26)

Status: PARTIAL. Acceptance NOT met. Branch `StallLive`, HEAD **8496e8633676a3385d5103785f318f0625e113eb** (ls-remote confirmed `refs/heads/StallLive`). Pushed commits (test-only census diagnostics; `src/` identical to `08296496`):
- `4303487a` PES stall census: STALLPRESSER reads the press man's decision command and issued motion
- `8496e863` PES stall census: the press line reads the MOVESTEP decision command, not the stale clip command (the second hunk sat in another agent's journal in this worktree; committed as the brief asked)

No sim change landed. Two candidate source fixes were measured and are NOT landed, because each trades one acceptance item for another (section 2). Build dir deleted.

## 1. Acceptance on the final tree (8496e863; src = 08296496)

| Check | Result | Notes |
|---|---|---|
| PesStall.* (8 tests) | 7 PASS; **ALiveBall FAIL: 223 ticks** | unchanged from round 12 |
| PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime | **PASS** (883,147 ms) | |
| PesPassTarget (EveryPassInFlightHasItsOwnSidesReceiver) | **PASS** | gtest reports 1 test passed; the file's second test was not counted and not checked |
| Throw-in suite 9/9 | **NOT RUN on HEAD this round** | round 12 on this src: ankle 1 of 156 short (gate on) / 4 of 154 (reverted); ACeremony hits its cap on seed 25 |
| Kick-clear mutation (`pesKeyActor = nullptr;` removed, match.hpp:130) | **RED**: receiverTicks 0 vs 0, foreignNamedTicks 0 vs 0 (pass-target, 446 s) | mutation reverted; src equals HEAD |
| gen_translated --check | GREEN: 10,150 functions byte-identical (3,824 recorded bit-exact, 6,326 closure) | ran on the candidate working tree (edits outside translated code); not re-run on HEAD |
| ledger --check | GREEN: LEDGER.md current, 47,642 rows, no regression vs HEAD (28,773 rows compared) | ran on the candidate working tree; not re-run on HEAD |
| Seed-7 2-min pes-loop smoke | 29,597 ticks, determinism **d5b32ac91262de67** | identical to round 12 |

## 2. Fixes measured this round (NOT landed)

Candidate A, the loop's stand faces the throw aim (elizacontroller.cpp:566-582). The ThrowinLoop stand faced the taker's root direction, and the throw pass the AI queues first (`humanoid.cpp`: the first applicable command wins) never found a clip. PES's loop turns him to the target's bearing (pes-re-throwin-choreography.md 4.1, phase 2). The stand now takes the pass's aim, as the retainer branch does.
- Seed 25: **full time, 35,025 ticks, determinism f2bede24e50292b2**, identical to the round-12 gate-on run. The round-12 stall is gone.
- PesHang seed 11: **FAIL**, stall 1,758 ticks at frame 23,732 (the same stop as round 12's gate).

Candidate B, A plus the carrier brain from PES's holder (elizacontroller.cpp:654, `hasIt`): under the PES loop `hasIt = GetBallRetainer()==player || GetPesHolder()==player` (was `|| HasPossession()`).
- PesHang seed 11 (A+B tree): **PASS** (625,227 ms).
- Seed 25 (A+B tree): full time, 50,737 ticks, determinism 3c20f11cef81e7d1.
- Pass-target (A+B tree): PASS.
- **PesStall.ALiveBall (A+B tree): 3,104 ticks forced (was 223).** The live-ball stall grows about 14x. Not landable: it is the owner's primary metric.
- Ankle (TheThrowInTakersAnklesStayBehindTheLine) on A+B: **FAIL, 2 of 37 taking ticks short, worst 0.0155 m** (a different tick set from round 12's 154).
- ACeremony on A+B: not finished after about 75 minutes (run cancelled; no result).

Patch (both hunks, 41 lines): `local://pes-port-StallLive13-candidate.patch`.

Decision: neither is landed. A breaks PesHang. A+B keeps PesHang green but moves the live-ball stall (section 3), which worsens ALiveBall about 14x. The owner should choose: PesHang green (A+B) against the ALiveBall count (223 versus 3,104), or fix the presser's trap first (section 7 item 2).

## 3. The PesHang seed-11 stall is a live ball, not a free-kick stall (MEASURED, candidate A, gfsim seed 11)
- At the stop the buffer's `piece` is 3 (`e_SetPiece_FreeKick`), but `buf active 0 prepared 1 started 1`, `setPiece 0` and `inPlay 1`: the free kick is over. The ball lies at 0.08 m/s at (-16.6, 22.8). The restart watch counts the stoppage from the restart's start.
- The men the breaker names (H4, H5 at 0.7-1.1 m from the ball) hold **Movement commands with no target** (`[stalemate-cmd]`: fn 1, usetgt 0, v 3.5-6.5 m/s). They are not commanded to the ball, and the breaker forces them every few seconds while the ball stays still.
- Candidate B removes the seed-11 stall. The mechanism by which the `hasIt` change does that was not traced; its cost shows up as the ALiveBall rise (section 2).
- The census's LongPass wind-ups (fn 5, 35 firings with `pesHolder=-1`) are a different state, from the seed-7 baseline, and were not seen in seed 11.

## 4. Items 1-4 of the brief

1. **Presser closing the ball (161/172 in round 12): the stand-in is consistent with the data, not confirmed as the cause.** STALLPRESSER now prints `motion` (committed). Aim-only census (candidate A tree; press-man lines, unaffected by the throw-in loop): usetgt 1 and dist >= 2 m, 148 firings. Issued motions: **2221 in 86** (command mean 4.11 m/s, body 1.22 m/s), 4152 in 32 (6.19 / 3.02), 2220 in 13 (3.75 / 1.38), 3133 in 6 (5.62 / 5.37), none (AI clip) in 11 (6.73 / 2.02). A single member (2221) serves commands from 2 to 7 m/s, which fits `Issue()` playing "the blend group's nearest member" (pesmovestep.cpp, the ponytail stand-in). **Not a port:** SetParametric's record flush exists (`pesfoxworld.hpp:130`, `IssueSeries`), but its caller, the request filler FUN_1406F14E0, is not translated (the ponytail there says so), and the GF body plays its clips through `PlayIssuedClip`, not the fox world. Open: translate FUN_1406F14E0 and its closure, then route the body's parametric issue through the series (needs a proof on captured SetParametric calls).
2. **25 pressers with no ball target (usetgt 0, 2-5 m): not decided.** `pes-re-press-coord.md` section 2 (KEEP branch): `FUN_140633710` clears the man's target to 0xFF, so a no-target press man may be PES-faithful. Open: decode FUN_140633710's clear condition against the 25 firings before porting anything.
3. **Seed 25 throw-in stall: cause found; fix is candidate A (section 2).** The stall is the ThrowinLoop's stand facing the root; PES's loop turns to the aim. Candidate A fixes seed 25 but not PesHang seed 11, whose stall is a live ball (section 3).
4. **Ankle 2 of 37 (candidate) / 4 of 154 (HEAD, round 12): red.** Not chased.

## 5. Killed theories
- K1. "The ThrowinLoop gate (`!started`) is the exit condition." Partly: gating it off reproduces candidate A's seed-25 result exactly (same determinism), but PES's exit is the throw command, and the loop's stand must turn to the aim (A).
- K2. "PesHang seed 11 is a free-kick taker stuck far from the ball." Killed: the set piece ended (`active 0`, `setPiece 0`); the stall is a live ball with Movement commands.
- K3. "GF's `HasPossession()` is what keeps the ALiveBall stall." Killed: replacing it with PES's holder (B) raises the forced live ticks 223 -> 3,104.
- K4. "The stand-in's single member explains every press man's slow body." Not established: member 2221 serves 2-7 m/s commands, but no playback check against the members' rate tables was made.
- K5. "PesHang seed 11 is a throw-in problem." Killed: the throw-in changes only move the trajectory into a live-ball stall that already exists (seed 11 passes at HEAD only on its trajectory).

## 6. Commits and verification
- `4303487a`, `8496e863`: pushed; `git ls-remote origin StallLive` = `8496e8633676a3385d5103785f318f0625e113eb`.
- Worktree: `extern/godot-cpp` symlink swapped for an empty dir around the commits and restored to `<workdrive>/GodotFootball/extern/godot-cpp`.
- `src/` diff against `08296496`: empty. Build dir deleted (`rm -rf build`).
- PII scan: no machine paths in the changed range (checked on the diff; `tools/check_pii.sh` fails inside the worktree because of the symlink).

## 7. Open items (numbered)
1. Decide A+B versus HEAD: PesHang green (A+B) versus ALiveBall 223 -> 3,104 (section 2). Owner call.
2. The presser's trap at 0.7-1.1 m from a resting ball, which both ALiveBall and PesHang seed 11 hit (section 3). Trace why the breaker's forced men hold no ball target (the press writer's assignment and the target clear, `pes-re-press-coord.md` section 2).
3. Translate FUN_1406F14E0 (blend filler) and wire SetParametric into the body (section 4.1).
4. Decode FUN_140633710's target clear against the 25 no-target pressers (section 4.2).
5. Throw-in suite 9/9 on HEAD (ankle, ACeremony) and candidate A's ACeremony (not finished).
6. Ankle 2/37 under candidate A+B (section 2).
