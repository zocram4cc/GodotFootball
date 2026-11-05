# RestingBall: a trap the receive reaction restarts every third tick (the resting-ball stall on PressAI's tree)

Branch `RestingBall` (from main `3a9b9310`, merged with main `c45fbd57`). Stand-alone report; archived by Main into `docs/re/`.

## 1. What was found (all measured, this session)

**The stalled shape reproduces on PressAI's tree `48275157`** (scratch worktree, probe added; not committed to that branch):
seed 7, 2 min, `pes_loop`: keeper H1 goal kick (piece 2); forced man H3 stands 0.1-0.8 m from a ball at rest at (-6.74, 12.26) for 1450+ ticks, then H1 for 400+ ticks at (-35.26, 12.50). Designated man: `HasPossession()==1`, `GetBallRetainer()==null`, ball 0.08 m/s (rest), no presser (as in the brief).

**On main the same shape exists in the small** (probe: designated man + possession + no retainer + ball at rest, seeds 7/3/11): longest runs 58, 103 and 161 ticks, 49/66/80 episodes. Main's seed 7 shows no breaker firing (PesStall green), so main alone does not fail PesStall; seed 11 on main fires the breaker 180 live ticks (a far loose ball, the old PressAI-dependent defect, not this one).

**Mechanism (CONFIRMED by a per-tick trace of the stalled man on PressAI's tree, f 4486-4515):**
- the man's PES action byte `P+0xAC8` alternates `7 (TRAP, phase 0) -> 7 (phase 2) -> 2 (MOVE)` every third tick;
- the receive reaction (`PlayerController::_TrapCommand`, entry 3) runs `RunTrapKind0` and `StartTrapAction` again as soon as the action is back in MOVE;
- each start tick calls `PlayIssuedClip` on a trap clip, so the clip restarts at frame 1 (`dribble_0_3_000_sole fr=1`, `feint_kickout_... fr=1`, `trap_0_0_000_y0_in fr=4`) and never reaches its contact frame (33): the ball is never touched, the man never becomes the retainer, and the breaker (GF) is the only thing that moves the ball;
- on seed 3 the same loop ran 25 ticks without a restart once (f 30520-30545, `trap_0_0_000_y0_in` reached frame 33, touch accepted `fullDist 0.06 < 0.95`), which is why main shows these only as 1-2 s approaches.

**Why the action ends after two ticks (CONFIRMED by experiment, INF for the PES side):** `CurrentMotionOf` (`pesmovestep.cpp:623`) returns -1 for a trap-family clip (it lists dodge, tackle, locomotion only), so `TrapFactsOf` leaves the playing-motion list empty and PES's end handler `0x14079E5C0` falls through to the action switch instead of waiting (docs/ports/pes-port-TrapTick4.md: PES leaves the handler at its first statement while the playing motion is in the table). Listing the trap family (tried on PressAI's tree) removes the early end but enters `PES_DISPATCH_UNTRANSLATED 0x140856370`: the later trap phases are not translated, so this is not a one-line port.

## 2. Change

`PlayerController::_TrapCommand` (`playercontroller.cpp`, the single entry every receive reaction routes through): the existing "already in the Trap action: hold the entry" guard now also holds while the body is on a trap clip whose contact frame is still ahead (`OnPesTrap() && GetFrameNum() < GetTouchFrame()`). Named `ponytail:` stand-in: GF's action state ends at phase 2 where PES stays in TRAP through the clip; removed by translating `0x140856370` and the phases after 2 (the ones that keep `P+0xAC8` on TRAP). No keeper or test special case.

## 3. Tests

- New `PesStall.ATrapClipIsNeverRestartedBeforeItsContactFrame` (`tests/sim/pes_stall_test.cpp`, a probe on the shared seed-7 2-minute match, no extra sim cost). Consumer-visible invariant: a trap clip is never started over a trap clip of the same man that has not reached its contact frame; also asserts trap clips were played (non-vacuous).
  - **Red on main (merge `02a6e89b`): 72 of 121 trap clips restarted before their contact frame.** `PesStall.TheBreakerNeverForcesABallUnderAnArmedRestart` and `ALiveBallNeverLiesStillLongEnoughForTheBreaker` PASS on main seed 7 (the defect is latent there).
- Green run and PressAI-tree PesStall run: see section 5.

## 4. Not done (numbered, honest)

1. `GATE fast` not run (budget: all slots were held >55 min early on, machine at 3-5 GB available; two probes timed out waiting for a slot).
2. Ablation of the smoke moves (switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`) not run; the change is PES-loop only (`MatchPesLoop` branch of `_TrapCommand`), so the switch-off hash cannot move; pes-loop will.
3. No census of other drifted seeds; no re-staging was needed or done (no seed moved).
4. The `P+0xAC8` ending itself is a stand-in (section 2): PES's later trap phases are untranslated (`0x140856370`, phases 2 and 4-9 per TrapTick.md section 5).
5. `PesStall` on PressAI's tree with this change: result in section 5. Landing order: this change first, then PressAI (merge main).
6. Main-tree seed 11's 180 live breaker ticks are a different defect (a loose ball at rest, nobody presses it: PressAI's coordinator) and are not addressed here.

## 5. Results
(filled below by the final message)

(Section 5 placeholder above is superseded by this.)

## 5. Results (measured)

Commits on `origin/RestingBall`: red test `02a6e89b`, fix `b5a416b4` (`git ls-remote` confirmed).

| tree | PesStall.ATrapClip... (new) | PesStall.TheBreaker...ArmedRestart | PesStall.ALiveBall... |
|---|---|---|---|
| main + merge, no fix | **RED** (72 of 121 restarted) | green | green |
| main + merge + fix | green | green | **RED: 167 breaker ticks** (5-s episodes: ball at rest at 33.3,21.4 with A9 1.1 m away; at -28.2,-25.1 with A10 7.9 m away) |
| PressAI `48275157` + fix (scratch) | not run | green | **RED: 184 breaker ticks** (was 8,281); 25 firings, longest idle episodes 26 s and 23 s |

**The acceptance is NOT met.** The 86-s keeper/H3 stall of PressAI's tree is gone (8,281 -> 184 ticks) and the trap loop is fixed, but PesStall's live-ball test now fails on main seed 7 too: the changed trajectory exposes loose balls at rest that nobody plays (one 1.1 m from a man, one 7.9 m away), the far-loose-ball defect PressAI3 also saw. Per the brief PesStall was not re-staged and no seed was moved. The branch is not landable until those episodes are diagnosed.

## 6. Next step (for whoever resumes)
1. Trace the remaining seed-7 episodes on this branch with per-tick prints on the designated man (`P+0xAC8`, clip, frame, the command queue, as in section 1). The 1.1 m one is the same resting-ball shape and the first candidate for a second cause (the hold returns true while the trap clip plays but the ball has moved off the clip's contact point: compare `touchPos` with the ball).
2. If the 7.9 m one is the far loose ball, it belongs to PressAI's coordinator (PRESS 0x34 on a loose ball): land after PressAI, or re-stage PesStall by census with Main's consent.
3. Gate fast, ablation, census: not run (section 4).
