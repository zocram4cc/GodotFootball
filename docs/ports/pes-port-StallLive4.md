# pes-port-StallLive4: the pass receiver is found after a kick; the kick release that does it breaks the stall (StallLive, 20-11-26)

Status: NOT landable. Branch `StallLive` at **303dca0b1be94ce86fb64efd4173d9fce56bd5d0** (ls-remote confirmed; parent 7534f5b4). Commit 303dca0b holds TWO things (see 1): the layer additions and a kick release (match.hpp). Acceptance is not met: `PesStall.ALiveBall` is red at 662 (was 36), `PesStall.TheWorldHolderFieldIsThePesHolderEveryTick` is red (26 side-ticks). Main decides.

## 1. What is on the branch
- Layer (tools/dev/translate, src/pes/translated/closure): `closure_trapaction.json` += 0x14084F080 and 0x14079D310; `closure_trapfinish_f560.json` += 0x14079D310; closure.py tags f080 and d310 (--no-specs); gen_translated regenerated (10,150 functions; 0 OUT-OF-SET markers left in 140858D40, 1407A0410, 14079A310). `gen_translated --check` NOT run after this regeneration (budget).
- **match.hpp `Match::RecordPassEvent`: clears `pesKeyActor` and `pesHolder`** (the kick release). This is in StallContact's file and was not split out; it is in the same commit by my mistake (preselected in the draft). Revert it to isolate the layer.

## 2. Pass receiver (the acceptance item 1): CONFIRMED
- Passes DO happen with the holder fix: 33 pass events in a seed-7 match (kinds 1 and 3, PASSDUMP, since removed). `PassGetReceiver()` was null for all of them.
- Cause: the translated pass-target selector (`RunPesPassTarget`, T+0x78) clears the target when MA+0x2601C names an owner (PassReceiver3 section 6). With the holder sticky, the passer still owned the field during the flight, so the target was cleared on the kick.
- Kick release (`RecordPassEvent` clears the holder): `EveryPassInFlightHasItsOwnSidesReceiver` **GREEN** (run 524 s, `r4_pass.log` `RC=0`).

## 3. What the kick release costs (CONFIRMED, census `r4c_stall.log`)
- `PesStall.ALiveBall`: **662** firings (was 36 on 7534f5b4). **All 662 have pesHolder = none**: after a kick, no man holds the ball until a touch key arrives, and the loose-ball machinery (GF's 1 m band and the designated-man route) does not take the ball. Men 0.5-1.5 m away stand on walk and slide clips.
- `TheWorldHolderFieldIsThePesHolderEveryTick`: 26 of 272,930 side-ticks mismatch. A one-tick lag: the kick runs after the team tick has written MA+0x2601C.
- Assessment (INF): PES does release on a kick, but PES also has a loose-ball approach (PRESS man, pes-re-press-life) that GF does not run in this state, so releasing the holder exposes the gap. The probable PES-faithful path is the press/first-touch approach for a loose ball with no holder; that is not ported.

## 4. Stall, step 2 (remaining 36 firings on 7534f5b4): NOT done
Not reached in budget. The 36 were 2 HighPass carrier-brain episodes (non-holder 0.8-1.2 m, PES holder far). Still open.

## 5. Suites and smoke (on 303dca0b, measured)
- `pes_restart_delay_test --gtest_filter=PesStall.*` (`r4c_stall.log`): 6 PASSED; FAILED `ALiveBall` (662) and `TheWorldHolderField...` (26). `ATrapTouch...` passes.
- `pes_pass_target_test`: **PASSED** (the stale test is still the skipped named gap).
- **Smoke: seed 7, 2 min, `gfsim_run --pes-loop`: RC=0, "136465 sim ticks to full time (cap 194400)"** (`r4c_smoke.log`). Earlier in the round it aborted on untranslated 0x14084F080 and 0x14079D310; both fixed by the layer.
- `pes_hang_test`, `pes_throwin_release_test`: queued after the smoke in the same stream (`r4c_hang.log`, `r4c_throwin.log`). Check these logs; not read before this report was written (the throw-in run takes ~45 min). Last measured throw-in ankle (7534f5b4): 2 of 109 taking ticks short, worst 0.0155 m.
- Build lock: another agent's ninja (PresReplay2) held /tmp/gf-build.lock for ~25 min; my build waited, then ran.

## 6. Mutation
Not done for the final state (no final fix). The kick release is itself a single-line mutation of the pass test: remove it and the pass test is red again (as on 7534f5b4).

## 7. Decision for Main
- (a) Keep the kick release and land the loose-ball approach for a holder-less ball (PRESS / first touch, a PES port not done): the stall is then the real target; 662 is the measure.
- (b) Revert match.hpp (drop the kick release): pass test red, stall 36. Layer commit stays (needed only if the kick release stays; smoke was fine before it? NOT measured on 7534f5b4 — check).
- The pass test is a PES fact (the receiver exists after a kick), so (b) leaves a red on a real PES behaviour.

## 8. Open items
1. Loose-ball approach with no holder (PES PRESS man / first touch) — the 662.
2. One-tick lag of MA+0x2601C after a kick (write it at the kick, or reorder).
3. The 36 HighPass firings (hasIt band), step 2 of round 4.
4. gen_translated --check after the regeneration; ledger check; movestep tests; pes_hang and throw-in logs.
