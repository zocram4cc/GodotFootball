# pes-port-StallLive8: the holder field's kick-tick order, and the live-ball census diagnosis (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive` at **ff6dd27d92b1caf5f94268c67559e6b319be5595** (ls-remote confirmed). Acceptance not met: `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` is red at **223** ticks (limit 0; the base b96a4043 measured 223 as well). The holder-field test is green. The build dir is deleted.

Commits (no history rewrite):
- `d57a44c1` PES holder field: W1 writes MA+0x2601C after the tick's kicks, not before. Its end-of-tick move **changed play** (section 1.3). Kept in history, superseded.
- `ff6dd27d` PES holder field: restore the team-tick write; keep the end-of-tick refresh. Final state. Play-identical to the base.

## 1. Item 1: the holder field lags a kick by one tick (30 of 59,194 side-ticks)

### 1.1 The cause (CONFIRMED from the code)
- `Team::RunPesPassTarget` (team.cpp, head of the team tick) wrote MA+0x2601C from `Match::GetPesHolder()` before any player acted.
- `Match::RecordPassEvent` (match.hpp:126-131, called from `Humanoid::Process`, inside `teams->Process()`) clears `pesKeyActor` and `pesHolder` on every kick. That is after the team-head write, so the field keeps the old holder for the kick tick.
- The carrier block (`elizacontroller.cpp:2858`) also writes MA from the holder, per player, before that player's humanoid kicks.
- Per player the order is controller (carrier block, MA write) → Think → `Humanoid::Process` (kick). `Player::Process`, player.cpp:461-510.

### 1.2 PES's order (OPEN; the one-tick question is not settled)
- CONFIRMED: W1 (`0x140457070`, store `0x140457259`) is reached from the listener msg-2 path `0x140426760 → 0x1404582C0 → 0x140458880 → W1` (pes-re-holder-leaves.md §5).
- OPEN: the frame order of the listener msg-2 against the team tick `0x1404F6C30` (called at `0x14041EDD7` inside an unanalysed region; pes-re-holder-leaves.md §5, pes-re-holder.md §8 item 3). pes-re-gaps-match.md (OBS) places the MA build ahead of the team tick.
- Live (StallLive6): the carrier and T+0x78 change on the same sampled frame as the kick. That supports an end-of-tick W1, but a sampler at 54 Hz cannot rule out a one-tick difference.

### 1.3 What was tried (MEASURED)
| Variant | Holder test | Smoke (seed 7, 2 min) | Live-ball census | Ankle census |
|---|---|---|---|---|
| base b96a4043 | 30 of 59,194 red | 29,597 ticks | 223 | 6/211, worst 0.0269 m |
| d57a44c1: team-head write moved to end of tick (`Match::WritePesHolderField` after `officials->Process()`) | **green** | **80,802 ticks** (determinism 6f0decf7c3682532), hold 55,939 ticks | **1,341** | **22/209, worst 0.0816 m** |
| ablation: d57a44c1 with the team-head write restored, end call removed | red (not run) | 29,597 ticks (d5b32ac91262de67) | not run | 6/211, worst 0.0269 m |
| **ff6dd27d (final): team-head write restored + end-of-tick refresh** | **green** | **29,597 ticks** (d5b32ac91262de67) | **223** | **6/211, worst 0.0269 m** |

Killed: "moving the holder write to the end of the tick is play-neutral". The team tick's own readers consume the head write, so moving it gave them a one-tick-stale holder for touch adoptions; the smoke and census moved 2.7x and 6x.

### 1.4 What the final state is, and what it is not
- ff6dd27d keeps the head write (the team tick's readers unchanged) and adds one refresh after `officials->Process()` (`Match::WritePesHolderField`, match.cpp:850-857, match.cpp:947). In play it changes nothing I could measure (identical smoke hash, census, ankle).
- It is an extra GF write of the same field, not a PES-verified step. The brief said not to add one unless PES does exactly that. The refresh is a GF stand-in until the W1-vs-team-tick order (1.2) is decoded. Main should decide: keep the refresh, or assert the head-of-tick W1 relation in the test if PES runs W1 before the team tick.
- Mutation on the kick clear (run on the d57a44c1 tree; the line is unchanged in ff6dd27d): `pesHolder = nullptr;` commented out in `RecordPassEvent` (match.hpp:130). **`PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` RED**: `receiverTicks` 0 vs 0 (pes_pass_target_test.cpp:147), `foreignNamedTicks` 0 vs 0 (:149). Reverted; the tag returned to the committed content. Not rerun on ff6dd27d.

## 2. Item 2: the live-ball census (223, NOT FIXED)

### 2.1 Census (CONFIRMED, GF_STALL_DUMP=1 on base b96a4043, seed 7, 2 min)
223 firings. All have no PES carrier (`pesHolder=-1`). The dump's clip function is the `e_FunctionType` of `gamedefines.hpp:52-67`:
`0 None, 1 Movement, 2 BallControl, 3 Trap, 4 ShortPass, 5 LongPass, 6 HighPass, 7 Header, 8 Shot, 9 Deflect, 10 Catch, 11 Interfere, 12 Trip, 13 Sliding, 14 Special`.
**Correction of StallLive7 §4:** fn 14 is Special, not Movement.

| Class (P+0xAC8 action, clip) | Firings | Gate state | Reading |
|---|---|---|---|
| MOVE (2), Special locomotion clip (fn 14, `OnPesLocomotion`) | 145 | MOVESTEP runs | FSM runs; the body does not arrive. **Not diagnosed.** |
| MOVE (2), non-locomotion GF clip: LongPass 35 (incl. `gf_anims/pass/idle/000.anim`, 73 frames, one frame per tick), BallControl 14, Sliding 11, Trip 3, Shot 1, Movement 1 | 65 | **refused** | See 2.2. |
| TRAP (7), Special | 13 | trap | Not analysed. |

By presser: `forcedIsPresser=1` 51; `forcedIsPresser=0` 172 (a presser exists but a non-presser is the nearest man). Distance of the forced man: under 1 m 94, 1–2 m 115, 2 m or more 14.

### 2.2 The 65: a deadlock on a GF clip (CONFIRMED from code and dump)
- `ProcessMoveStep` (pesmovestep.cpp, the `if (!player.CastHumanoid()->IsMovementClip())` gate, about line 620) returns before the FSM ticks when the clip is not Movement and not PES locomotion.
- `HumanoidBase::PlayIssuedClip` (humanoidbase.cpp, first statement after the family test) returns false for the same reason: `if (currentAnim->functionType != e_FunctionType_Movement && !onLocomotion && !OnPesTrap()) return false;`. It also returns false when the Animetable has no cell (`dataSet.empty()`).
- Result: a MOVE man left on a GF non-locomotion clip cannot be steered until that clip ends. The census shows the frame advancing one per tick on `pass/idle/000.anim`.
- PES: MOVESTEP runs in MOVE (anime 2) and its issue replaces the clip; the end of a kick returns to MOVE (`FUN_1406EEBB0(anime,2,0x5E)`, pes-re-holder-clear.md §3).
- Why it is not a one-line fix: GF has no action id for the body. `P+0xAC8` is written MOVE for every non-trap man (`teamAIcontroller.cpp:3575` TrapFactsOf; `pestrapactor.cpp:166`). Gating the FSM on it would run MOVESTEP over kick and pass clips. The faithful fix is the body's PES action switch (anime action per clip), which is not ported. Larger than this round; stopped here.
- The humanoid.cpp:604-613 lead in pes-re-move-gate.md §4 is wrong: those lines set `smoothFactor` for the blend; they do not keep a clip.

### 2.3 The 145 and the 172 (OPEN)
- The 145 run MOVESTEP on locomotion clips (`idle_0_3_run_*`, `walk_1_1_relax_mirror`, `run_2_2_back_mirror`, turn clips). The steering is not decoded here. Next step: log the issued motion id and whether `PlayIssuedClip` fails (`dataSet.empty()`) per forced-man tick, not the clip alone.
- The 172: the forced man is a non-presser; the presser is elsewhere. Whether PES's reaction admits a TRAP to that man at that distance is the reaction slice (StallContact/TrapAdmit), not this one.

## 3. Results on ff6dd27d (final tree; binaries built from it)

| Check | Result |
|---|---|
| `PesStall.*` (restart_delay binary, one process) | 7 PASS (incl. TheWorldHolderFieldIsThePesHolderEveryTick, ATrapClipIsNever…, AHolderIsNever…, TheBreakerNever…, ANoManIsEver…, ATrapTouch…, ATrapReaches…); **FAIL ALiveBall: 223 ticks** |
| `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` | **PASS** |
| `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` | SKIPPED (the test's own `GTEST_SKIP`, "named gap" at pes_pass_target_test.cpp:136; unchanged) |
| `PesHang` (seed 11, 2 min) | **PASS** (648.8 s) |
| `PesThrowInRelease` (9 tests) | 8 PASS; **FAIL `TheThrowInTakersAnklesStayBehindTheLine`: 6 of 211 taking ticks short, worst 0.0269 m** (base and round 7: same) |
| Split and cap | 4 processes: ThePlayState+TheMatchIs+ACarriedThrowUses (1108 s + 0 + 0); ACarriedThrowInIsReleased+Ankles+EveryManHasHisDemo (1100 s + 0 + 0); ACeremony (897 s); ACarriedHeld+TheLift (1022 s + 0). Single-process sum ≈ 4,130 s, above the 3,000 s cap, so it is split. One earlier run of ACeremony alone hit my 3,600 s deadline under load. |
| Ankle census | 6/211, worst 0.026874542 m (the same numbers as round 7's 6/211, 0.0269) |
| Mutation, kick clear | RED (see 1.4). Reverted. |
| `gen_translated.py --check` (`--image` = `pes_full.bin`) | green: 10,150 functions byte-identical (rerun on ff6dd27d) |
| `ledger.py --check` | green: LEDGER.md current, 47,642 rows (rerun on ff6dd27d) |
| seed-7 2-min pes-loop smoke (`gfsim_run --pes-loop`) | 29,597 sim ticks to full time, determinism d5b32ac91262de67 (rerun on the same source as ff6dd27d, as the ablation/variant-B build) |

## 4. Killed theories
- K1. "The kick clear needs a GF write in `RecordPassEvent`." Not tried (the brief forbids it without PES evidence).
- K2. "Moving the holder write to the end of the tick is play-neutral." Killed (d57a44c1: smoke 2.7x, census 6x, ankle worse).
- K3. "The 223 is the presser's gate alone." Killed: 145 are on locomotion clips, 172 have a non-presser forced.
- K4. "fn 14 is Movement." Killed: Special (enum at gamedefines.hpp:52-67).
- K5. "humanoid.cpp:604-613 keeps the HighPass clip." Killed: those lines set the blend's `smoothFactor`.
- K6. "The holder test's end-of-tick expectation is wrong (PES's W1 runs before the team tick)." Open, not killed: the order is OPEN (1.2).

## 5. Open items (numbered)
1. PES's listener msg-2 against the team tick (1.2). Settles whether ff6dd27d's refresh is PES-faithful or the test should assert the head-of-tick relation. Needs a rig hook (Main owns it): entry of `0x140457070` (W1) and of `0x1404F6C30` (team tick) with a shared frame counter, cap 12 each.
2. The body's PES action id (P+0xAC8 per clip, the anime action switch) so the FSM gate and `PlayIssuedClip` follow PES's MOVE action (2.2). Blocks the 65.
3. The 145 locomotion firings: a per-forced-man log of the issued motion id and `PlayIssuedClip` failures (`dataSet.empty()`).
4. The 172 non-presser firings: PES's TRAP admission for a non-presser at 0.7–1.2 m (reaction slice).
5. The throw-in ankle census (6/211, 0.0269 m) is unchanged from round 7 and remains red.

Build dir deleted. Worktree clean at ff6dd27d.
