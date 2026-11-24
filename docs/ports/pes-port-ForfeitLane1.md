# PES port: ForfeitLane1 (branch ForfeitLane, base 37bf1a15 = origin/Integrate)

## 0. Status (top)
- Tip: `ac5d335b` on origin/ForfeitLane (confirmed by `git ls-remote`). Commits: f4999690 (cherry-pick of PressLane 0127b131, driver fix), f2d11632 (static framebuild record + layer), ac5d335b (ARedCard scenario change).
- Reds: ASideAtTheMinimumKeepsPlaying GREEN (787.6 s, no abort). ARedCardRemovesTheOffenderAndTheSideIsNotForfeited GREEN (1,274 s) after a scenario change (section 1). Other PesForfeit* tests GREEN (section 4).
- Ratchet: PesStallQuick forced_live = 53 (bound 53, PASS). PesStall.ALiveBall: PASS against the bound 2229; the exact count is NOT printed by the test, so the measured value is unknown (section 5).
- gen_translated --check: PASS (10,985 functions byte-identical).
- Rig requests: none new. Main's queued capture of 0x1408BBF20 (hook fbbf_1408BBF20) would give the real-call proof that replaces the static record (section 3).

## 1. ARedCard: why the match did not reach full time (CONFIRMED by census)
Probe (scratch, reverted): the test's own scenario (2-minute match, seed 7, offender = first outfield player, GiveRedCard at now+1). Logged every 1000 ticks: clock, referee buffer, stalemate, active counts. 48,427 ticks to the 900 s window.

- The sending off works: home 11 -> 10 at tick 2 (SendOff). The match stays in play, unfrozen, the period clock advances.
- The ball goes loose several times (the stalemate forces a chaser each time); play resumes. No stall.
- The first half ends at simulated ~373 s (tick 20000: period 3, playState 2, stopped at the break).
- The score is level (0-0). GF's MatchProgression::GetNext sends a level second half to extra time (periods 5, 7), then a shoot-out (9), then full time (10). PeriodAfterWhistle (pesclock.cpp:96-103) records this as a ponytail: PES's knockout flags E+0x66 (extra time) and E+0x67 (shoot-out) have no GF setting, and their writers are OPEN (docs/re/pes-re-missing-systems.md 2.9a).
- At the 900 s window the probe is in period 5 (first extra-time half), playState 2, game over 0.

So the match does reach full time, but after the window. The window (900 s) meets a level score that GF extends into extra time and a shoot-out, and the test does not ask for either. The sent-off handling is not on this path: the removal is at tick 2 in both the probe and the test.

Change (test only, tests/sim/pes_forfeit_test.cpp, commit ac5d335b): the score is set as data (AWAY 1, as the sibling AYellowThenRed test does), so the match ends at the second half's whistle. The assertions are unchanged: game over, offender inactive, HOME not forfeited. Not chosen: a longer window (extra time plus a shoot-out run past 900 s; a unit test that long is not acceptable); a change to GF's draw rule (PES's E+0x66/E+0x67 writers are OPEN, so there is nothing to port from).

Red-first note: the red was observed in the brief (fails at :318, 1,590 s) and reproduced by the probe; the green is a scenario change, so no mutation applies to it.

## 2. Sent-off handling (PES decode, CONFIRMED by headless decompile of the unpacked image; NOT ported)
- FUN_140523110 (red-card repair, restart programme case 2, raw 0x140521408): for each side, scans slots 0..10 for an on-pitch player whose record (FUN_140a73e20) has +8 > 1 (a send-off marker). It calls FUN_140521930(side, slot, kind 2, ...) for a bench replacement. If that returns 0xFF, it RETURNS: no substitution is queued and the sent-off man stays on the pitch. Otherwise it queues (in, out, kind 2) with FUN_14051f5f0 and returns. Nothing in this routine removes him; the removal is the queued substitution running at a later restart.
- FUN_140523210 is the keeper/outfield replacement path (kind 5): FUN_140521e40 / FUN_140522300 / FUN_140521bb0 choose a slot, FUN_140521930 kind 5, FUN_1405204d0 when none. It is not the outfield red-card path.
- GF: Player::GiveRedCard adds 3 cards; Player::Process calls SendOff at the effective time (player.cpp:543), which deactivates the man at once and queues nothing.
- Verdict: the divergence is real (decode), it does NOT cause the ARedCard non-finish, and porting it changes behaviour under the ratchet. Not ported in this lane. The OPEN note in the test (lines 306-309) stays true.

## 3. ASideAtTheMinimum: 0x1408BBF20 in the layer (static record; coverage reported)
- Abort at baseline: PES_DISPATCH_UNTRANSLATED 0x1408bbf20, from 0x1409F2990's OUT-OF-SET call, after a stalemate line (base_aside.log, RC 134).
- Static record: `closure.py --spec-set framebuild --tag framebuild 0x1408BBF20` (17 members; specs/framebuild/; set file closure-framebuild.txt); `driver.py --only 0x1408BBF20` (113 synthetic cases, bit-exact on rax, xmm0 and written bytes; labelled provisional by the driver: writable image globals read at on-disk values); a SETS entry for results_framebuild.json in gen_translated.py.
- Coverage (CONFIRMED, driver coverage field): root 0x1408BBF20 7 of 42 own blocks (0.17). Callee coverage: 0x140A48A30 2/3; 0x140439E50, 0x14043B8B0, 0x140456EA0, 0x140A48A10, 0x140A4C4E0, 0x140A4EEF0, 0x140A4F160, 0x1403FB410, 0x1403FB860, 0x1403FB5B0: 0 reached. The callees are compiled into the layer with the root, but their bit-exactness is UNPROVEN.
- Layer diff: 10,985 functions (3,907 recorded, 7,078 closure). Changed: closure/pes_fn_1409F2990.cpp (the OUT-OF-SET call is now a real call), pes_dispatch_gen.cpp, translated.hpp. New: closure/pes_fn_140A4EEF0.cpp, closure/pes_fn_140A4F160.cpp, gameplay/pes_fn_1408BBF20.cpp.
- Result: ASideAtTheMinimumKeepsPlaying GREEN (787.6 s). This is a static-record green, not a real-call proof.
- Driver fix: cherry-picked 0127b131 (driver: a reached indirect target's direct callees join the universe) as f4999690, so the closure builds.
- Real-call proof (preferred): when Main's fbbf_1408BBF20 capture lands, prove_bounded on real calls replaces this record. Until then the coverage is 0.17 and the callees are unproven.

## 4. Tests (this tip, ac5d335b)
- PesForfeitRule.ASideIsShortOnlyBelowTheMinimum: PASS.
- PesForfeitInstall.TheOffenceIsTheExactSentOffStateWithNoInjuryMarker: PASS.
- PesForfeitInstall.ThePostMatchPeriodIsTheOneTheWhistleWrites: PASS.
- PesForfeitMatch.ASideShortOfTheMinimumEndsTheMatchAtItsNextStoppage: PASS (57.5 s).
- PesForfeitMatch.AnInjuredPlayerStaysOnThePitchWhenTheBenchIsUsedUp: PASS (42.2 s).
- PesForfeitMatch.AnInjuredPlayerIsReplacedWhileASubIsLeft: PASS (43.2 s).
- PesForfeitMatch.AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil: PASS (1,438.8 s).
- PesForfeitMatch.ARedCardRemovesTheOffenderAndTheSideIsNotForfeited: PASS (1,274.3 s).
- PesForfeitMatch.ASideAtTheMinimumKeepsPlaying: PASS (787.6 s).
- Totals: 9 of 9 green in pes_forfeit_test (two runs: 7 in one, 2 in another).
- Mutations: none run. The ASide fix is a layer addition whose test is the red; the ARedCard change is a scenario change (section 1).

## 5. Ratchet (on the tip; behaviour change = the layer addition)
- PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow: PASS, forced_live = 53 over 12,960 ticks (bound 53; unchanged from Integrate's 53). 479 s.
- PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker: PASS against kBestMeasuredLiveFirings = 2229 (EXPECT_LE). 2,019 s. The test does not print the count on pass, so the exact value is UNMEASURED. The ratchet rule says a landing that lowers the count lowers the constant; that needs the exact value (re-run with a printf, about 34 min) and is not done here.

## 6. Verification
- gen_translated.py --check: PASS, 10,985 functions (3,907 recorded bit-exact, 7,078 closure) regenerate byte-identical (1,268 s).
- PII: no `<workdrive>` or `<home>` in the added lines of f4999690..ac5d335b. The full tools/check_pii.sh was not run from the main checkout.
- Gate: not run (fast tier is Main's). Diff suites not run.

## 7. Open items (numbered)
1. PES's E+0x66 / E+0x67 (extra time and shoot-out flags) have no GF setting; GF always plays extra time and a shoot-out after a draw. Writers OPEN (pes-re-missing-systems.md 2.9a). Blocker: the flag writers.
2. Sent-off handling (section 2): GF removes the sent-off man at once; PES queues a kind-2 replacement at the next restart and keeps him when no bench player is left. Not ported; OPEN note kept in the test.
3. 0x1408BBF20: static record, 0.17 coverage, callees unproven. Replace with the real-call proof from Main's fbbf_1408BBF20 capture.
4. ALiveBall exact count unmeasured (section 5). Needs a re-run with the count printed.
5. The Ghidra bridge was down for the decode; decompiles came from dec.sh (headless) for 0x140523110, 0x140523210, 0x140521930.
6. Probe and binary copies are in <workdrive>/gf-ForfeitLane-scratch (logs kept); the worktree build dir is removed at the end.
