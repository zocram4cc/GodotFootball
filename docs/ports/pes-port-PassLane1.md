# PassLane1: the PASS_GET episode census (seed 7); the counter-B hypothesis is not the cause; red aim test pushed, no port landed

Branch `PassLane` from origin/Integrate `a316278d1`, worktree `<workdrive>/gf-PassLane`. Image `pes_full.bin`. Scratch (not committed): `<workdrive>/gf-PassLane-scratch` (`census.py`, `census2.py`, `census_probe.patch`, census logs). CONFIRMED = a run or decode this session; INF = inference; OPEN = blocker named.

## 0. Verdict
1. **Red reproduced (CONFIRMED):** `PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange`, seed 7, 2 min, pes_loop, the brief's numbers: 30 episodes, **10 over 2.94 s, longest 11.72 s, median 1.63 s**. Census run 4,408 receiver-ticks, unchanged by the probe.
2. **The hypothesis "the receiver target is a stand-in for the arrival writer" is NOT what makes the episodes long (CONFIRMED by census).** The aim R+0x894 is PES's own ActionPassGet output (`0x14094E0E0`, 24/24 real calls, PassReceiver2 §4). The stand-in is elsewhere: GF's counter B (SR+0x1438) and the ball prediction samples. Neither is the head-of-list cause in the bucket that holds 84% of the over-bound time (section 2, class A).
3. **Episode end is not the defect (CONFIRMED, PES assembly):** T+0x78 clears only on a touch (owner `MA+0x2601C <= 0x15`, `0x1404FB3E7`), a dead ball (`0x1404FB3F3`), or a newest event 0 frames old (`0x1404FB44E`). A long episode is a long time with no touch (`docs/ports/pes-port-TeamTick.md` section 1).
4. **No port landed.** The counter-B writer chain is blocked (section 4). Landed: the red aim test `PesPassTarget.ThePassGetAimIsTheBallWhileTheBallRests` (section 5).

## 1. Census (CONFIRMED)
Per tick, for each side's PASS_GET receiver: man and ball positions, both speeds, aim R+0x894 (PES x, height, z), speed class/km/h (R+0x8D0, R+0xE3C), route result (`this+0x6C`), counter A/B (SR+0x1430/+0x1438), the arrival list head (MA+0x24908 + side*0x58, entry 0), T+0x78, the owner (MA+0x2601C), the trap action byte (P+0xAC8: 2 = walk, 7 = trap), man action (+0x5D0), function type. Parsed by `census.py` / `census2.py`.

| # | side/slot | length | d0 to ball at start | ball at start | arrives <1 m | trap ticks | closest approach | aim to ball (median) | class |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 1/20 | 11.72 s | 21.5 m | 20.7 m/s | 1.26 s | 95 | 0.10 m at 1.33 s | 0.02 m | A |
| 2 | 1/15 | 8.57 s | 30.1 m | 25.5 m/s | 8.37 s | 17 | 0.07 m at 8.56 s | 0.52 m | A |
| 3 | 1/20 | 7.93 s | 4.3 m | 11.9 m/s | 0.33 s | 99 | 0.06 m at 7.26 s | 0.06 m | A |
| 4 | 1/20 | 7.65 s | 8.4 m | 12.9 m/s | 0.78 s | 67 | 0.28 m at 7.63 s | 0.08 m | A |
| 5 | 1/20 | 4.31 s | 2.7 m | 10.7 m/s | 0.20 s | 18 | 0.27 m at 0.30 s, then 3.3 m | 0.54 m | A |
| 6 | 0/9 | 5.91 s | 0.8 m | 8.8 m/s | 0.00 s | 0 | 0.60 m at 5.89 s | 0.09 m | D |
| 7 | 0/5 | 3.80 s | 4.6 m | 8.8 m/s | never | 0 | 1.75 m at 2.52 s | **7.93 m** | B |
| 8 | 0/9 | 3.72 s | 51.0 m | 41.5 m/s | never | 0 | 3.94 m at 1.91 s | 13.7 m | C |
| 9 | 1/19 | 3.04 s | 43.5 m | 34.4 m/s | never | 0 | 4.19 m at 1.78 s | 19.6 m | C |
| 10 | 1/18 | 3.00 s | 6.6 m | 0.2 m/s (rest) | never | 0 | 2.78 m at 2.70 s | **32.2 m** | B |

Classes:
- **A, at the ball, no touch (5 episodes, 25.5 s of the 30.3 s over-bound time):** the man reaches 0.06-0.28 m of the ball and stays in the trap action (P+0xAC8 = 7) for 17-99 ticks without a touch. Same symptom as PassArrival rounds 4-7 (reach test refuses; the joint-pose verdict there is still open). Not counter B.
- **B, aim far from a resting or slow ball (2 episodes, 0.9 s):** the aim stands 7.9 m (episode 7) and 32 m (episode 10) from the ball. PES's own aim is the ball (median 0.03 m over 1158 live rows with the ball at rest, `docs/ports/pes-port-PassReceiver3.md` section 14). See section 3.
- **C, far receiver of a long ball (2 episodes, 0.9 s):** the nearest man of the team at kick time is 43-51 m away; he never gets within 3.9 m.
- **D, close, never trapped (1 episode, 3.0 s):** arrives to 0.60 m, no trap ticks.

**Arrival list (CONFIRMED):** in all 10 episodes the PASS_GET man is the arrival-list head (MA+0x24908 entry 0) on every tick (`arrHeadIsMe` = all rows). So the stand-in counter B does not change who is head in these episodes.

**Counter B range (CONFIRMED):** GF's counter B (SR+0x1438) reaches 624 frames on the census run (204 of 4,408 receiver-ticks above 500). PES's observed counter-B range is 21..500 frames (`docs/re/pes-re-player-record.md` 2.1, 22 distinct values). GF's stand-in is outside PES's range: `GetTimeNeededToGetToBall_ms * 54 / 1000` (team.cpp `RunPesPassTarget`), stale by design (StallTrap2).

## 2. Attribution of the over-bound time
| cause | episodes | over-bound time | status |
|---|---|---|---|
| A: at the ball, trap, no touch (contact/reach, pose) | 5 | 25.5 s (84%) | not counter B; owner TrapContact / PassArrival rounds 4-7 |
| D: arrives, never trapped | 1 | 3.0 s | the same gate as A, not yet split (OPEN) |
| B: aim off the ball | 2 | 0.9 s | ball-sample stand-in (section 3) |
| C: far receiver of a long ball | 2 | 0.9 s | arrival and aim inputs; cannot be split without PES values (OPEN) |

## 3. The aim stand-in (CONFIRMED stand-in, INF on mechanism)
- `team.cpp` `RunPesPassTarget` (ponytail, `SetBallSample(0, ...)` only): "the prediction samples 1..249 (FUN_140A6CF70) stay zero until the ball port writes them". Writer not decoded (`docs/re/pes-re-first-touch.md:146`: "`FUN_140A6CF70` body NOT decoded").
- The aim of episodes 7 and 10 sits 8-32 m from a resting or slow ball. INF: the BallFollow route (`0x140945EC0`, PassReceiver2 §2) reads those samples; the aim then points at a zero-filled sample. Not verified.
- OPEN: the sample writer (B+0x88 + 0x28*j) and which sample the route reads.

## 4. Counter B (the assignment's hypothesis): decoded, blocked
- Writer: `FUN_14092ED30` (`0x14092ED30`, per-man think; param_1 = the man's AI object: +0x18 slot id, +0x698 action word, +0x7E0 team brain, +0xACC8 counter, +0xAD28, +0xA368, +0x624, +0x61C; param_2 = the slot record). Counter B is stored at `0x14092F204` (`mov [r15+0x1438], eax`). Its value is the result of `FUN_140928120(brain, slot id, ...)` on the common arm, else counter A (dec2 lines 1-266, CONFIRMED by decompile).
- Callers: `0x14092ED30` is called once in `FUN_140928FC0` at `0x1409294EF` (xref scan, CONFIRMED), and `FUN_140928FC0` is called at `0x140925729` from `FUN_140925724`. `FUN_140928FC0` (the press tick) is in the layer on branch `PressLane` (`d4fdb7f5f`), NOT on origin/Integrate. On Integrate `0x14092ED30` is translated (`src/pes/translated/gameplay/pes_fn_14092ED30.cpp`, proven 8/8 real calls per PressCut) but has no caller.
- Blockers (CONFIRMED by PressCut's notes and the code): the man AI object is not in the window (no per-man AI object; `team.cpp` `RunPesPassTarget` comment), and `FUN_140928120`'s deep arm reaches the pass planner through P+0xAB8 (the BodyAnimControl seam), which the window lacks.
- Therefore a faithful counter B needs: the per-man AI object (PES ctor image, the same pattern as `gen_passgetobject.py`), the press tick `0x140928FC0` in the layer (PressLane), and the planner object (OneWorld cut 4). None of these is a wiring step. Not landed.

## 5. Red test landed (this lane)
- `tests/sim/pes_pass_target_test.cpp`: `PesPassTarget.ThePassGetAimIsTheBallWhileTheBallRests`. Per tick with the ball at rest (< `kStalemateBallSpeed_ms`) in play, the largest distance between a PASS_GET aim (R+0x894, PES (x, z) = GF (x, y)) and the ball; bound `kPesRestTargetMaxMiss_m = 1.0` (a scale, not a PES value: PES's median is 0.03 m). Red: a PASS_GET aim stood **43.26 m** from a resting ball (section 6). Test-only commit: no PES or GF behaviour change.
- The census probe is NOT committed (`census_probe.patch` in scratch).

## 6. Measurement (seed 7, 2 min, pes_loop, `PesPassTarget.*`)
| test (seed 7, 2 min, pes_loop, one match for the suite) | result |
|---|---|
| `EveryPassGetEpisodeIsWithinPesLiveRange` | RED: 10 of 30 episodes over 2.94 s; longest 11.72 s; median 1.63 s (matches the brief) |
| `ThePassGetAimIsTheBallWhileTheBallRests` (new) | RED: 43.26 m max aim-to-ball distance with the ball at rest |
| `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` | RED (pre-existing, unchanged by this commit; WalkLane's red) |
| `EveryPassInFlightHasItsOwnSidesReceiver` | GREEN |
| `IsSetInFlightAndNeverStaleWhileTheBallRests` | SKIPPED (named gap in the test) |

Not run: the other pass-target consumers (`pes_receive_test`, `PesStallQuick`, `ALiveBall`): no behaviour change in this commit. Build: `pes_pass_target_test` built clean with ccache (one compile fix in the red test: `std::max<double>`). Commit `7104ed38d` on `origin/PassLane` (ls-remote confirmed). PII: the diff has no `<workdrive>` or `<home>` path (grep).

## 7. Behaviour change and ratchets
No match-behaviour change in this lane (test file only). ALiveBall and PesStallQuick were not run (no behaviour change to measure; they stay at their Integrate values by construction). Gate not run: the fast gate is Main's.

## 8. Rig requests (for Main; none made live by me)
- **R1, counter B against PES:** hook the counter-B store `0x14092F204` (mov [r15+0x1438], eax; EVERY 20, cap 12, FULL), predicate: none, to read PES's counter B for each man during one kick and one pass. Shows whether PES's values are 21..500 frames (GF reaches 624) and whether the arrival head follows them.
- **R2, aim:** hook the BallFollow route entry `0x140945EC0` (PassReceiver2 §2: 15-byte window per Main's `hooks_for_image` run, EVERY 20, cap 12, FULL) to capture B's samples 0..249 and the aim; decides which sample the route reads (section 3).
- Both must go through `tools/dev/translate/untranslated_callees.py hooks_for_image` before arming (brief rule 17-11-26).

## 9. Next steps, in order
1. Decode the ball-sample writer (B+0x88 + 0x28*j): its callers and the ball tick; then wire it and turn the aim test green (acceptance: `ThePassGetAimIsTheBallWhileTheBallRests`).
2. Hand class A (5 episodes, 25.5 s) to the TrapContact owner with this census: the receiver is in the trap at 0.06-0.28 m and does not touch.
3. Counter B: after PressLane's press tick and a per-man AI object exist; use R1 to check the range first.
4. Class C: needs R1 and R2 captures of a long ball before any change.
