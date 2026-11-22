# pes-port-StallLive9: the holder's kick order (static, landed), the live-ball census by command class, and what remains (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive`, HEAD **f6b07661ffb22eee8b1adf19c0f779bd478e90de** (ls-remote confirmed). Pushed commits, in order:
- `f6a0ad18` PES holder: the kick pushes the none key; W1 moves the holder on the next frame. (Item 1; accepted by Main.) Also carries WIP for item 2 in four files (see below); it is not reverted in history.
- `694e354e` Revert the PES body action-id WIP (the four item-2 files back to their pre-WIP text; cleanup of f6a0ad18).
- `f6b07661` PES stall census: the dump names each firing's clip command (diagnostic only).

Acceptance: NOT met. `PesStall.ALiveBall` red at 223 (unchanged); item 2 (body action ids) not landed; item 3 (the 145) not diagnosed; item 5 (ankle) not fixed. Budget exhausted. Build dir to be deleted at the end (see section 7).

## 1. Item 1 (landed): PES's order, decided statically (CONFIRMED from pes_full.bin)
Flat image, file offset = VA - 0x140000000; xrefs by capstone scan of `.trace` (scripts in gf-StallLive-scratch/r9/pe.py); decompiles by dec.sh.
- Listener FUN_14042B9F0, msg 2 (the per-frame arm): `0x140426760` runs (a) `FUN_140A0D190` = the event dispatcher (`0x140A0E400` = producer table of 223 handlers; each handler that returns true pushes its record into ring E via `FUN_140A3DC40`; then the consumers `FUN_140A22130` per event, and the state machine), then (b) `FUN_1404582C0 -> FUN_140458880 -> W1 (FUN_140457070, store 0x140457259)`. W1 reads the newest ring-1 key: an invalid or none key gives 0xFF.
- The team tick `FUN_1404F6C30` is reached from `FUN_14041ED22` (MatchSub), which the hub runs after the listener (the docs' OBS order). The per-player action ticks, including the kick commit, sit in the team tick's tree (`0x1404F7955`, which calls the master tick `0x140535C60`, is inside `0x1404F6C30`).
- Therefore: frame N listener (event pass, W1) -> frame N team tick (kick committed) -> frame N+1 listener (event pass produces the kick's record, W1 sees the none key). The holder moves one frame after the kick commit. The team-head write of MA+0x2601C (RunPesPassTarget) reads the W1 value, so MA(end of N) = holder(W1 of N), and the holder-field test's expectation (MA == the PES holder at the end of every tick) is correct as written. The wrong part was GF's kick clearing pesHolder inside the team tick.
- Change: `RecordPassEvent` clears only `pesKeyActor`; `RecomputePesHolder` does `pesHolder = pesKeyActor` (none included); the end-of-tick refresh is deleted.
- Measured (f6a0ad18 build): holder-field test GREEN without the refresh; pass-target `EveryPassInFlightHasItsOwnSidesReceiver` GREEN; seed-7 smoke 29,597 ticks, determinism d5b32ac91262de67 (unchanged); census 223 (unchanged).
- Not verified at 54 Hz (per the ruling). The brief's note "the carrier flips on the kick frame" (StallLive6, sampled) is now a claim the static order contradicts by one frame; a one-frame hook on W1 (0x140457070) and on the kick commit would settle it. Open.

Kick-clear mutation (final tree): `pesKeyActor = nullptr;` removed from RecordPassEvent -> **`PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` RED** (receiverTicks 0 vs 0, foreignNamedTicks 0 vs 0). Reverted; src/ equals HEAD (git diff empty).

## 2. Item 2 (NOT landed): the body's PES action id
- Names read from the image's action table at 0x1434A5470 (index = id, CONFIRMED): 2 MOVE, 3 STEP_MOVE, 4 DRIBBLE, 6 DRIBBLE_STEP_MOVE, 7 TRAP, 8 LONG_PASS, 9 SHORT_PASS, 10 SHOOT, 11 SLIDING, 13 TACKLE, 14 BLOCK, 16 STAGGER, 21 DODGE, 56 KP_CATCH, 58 KP_DEFLECT, 68 KP_DROP_BALL, 74 DEMO_MOVE.
- Tried (f6a0ad18, reverted in 694e354e): TrapFactsOf sets P+0xAC8 from the clip's kind (PesActionOfFunction), MOVESTEP gate and PlayIssuedClip guard read P+0xAC8. **Smoke aborts** at the first kick-type clip: `PES_DISPATCH_UNTRANSLATED: 0x140789d00` (pes_dispatch_gen.cpp abort). The action tick for those ids is not translated. `untranslated_callees.py 0x140789D00` says it is not in src/pes/translated.
- Main's ruling (received): translate 0x140789D00 and its closure through the normal pipeline, then carry the ids. Not started: the closure is the work, not a one-line change, and the budget is spent. What it needs: the closure list from `untranslated_callees.py --image <pes_full.bin> 0x140789D00`, a closure json tag, `gen_translated.py --check`, and a proof on kick-clip captures (existing captures under gf-Rendezvous-scratch/live2 if they cover the kick action; otherwise a hook on entry 0x140789D00 with predicate "P+0xAC8 == 8/9/10 at the kick start", cap 12, FULL).

## 3. The 223 live-ball firings, by the clip's command (CONFIRMED, f6b07661 dump; body P+0xAC8 in the last column)
| clip fn (GF enum) | commanded fn | firings | P+0xAC8 | reading |
|---|---|---|---|---|
| 14 Special (PES locomotion) | 1 Movement | 127 (+1 fn 1) | 2 MOVE | 145 class with the next row. MOVESTEP runs (the gate passes on the locomotion clip). Not diagnosed. |
| 14 Special | 14 Special | 18 | 2 | locomotion command on a locomotion clip. Not diagnosed. |
| 5 LongPass (pass/idle, 73 frames) | 5 LongPass | 35 | 2 | the commanded kick is the clip. The man is in the kick wind-up; the kick never commits (the ball never moves). Not the presser class. |
| 2 BallControl | 2 BallControl | 14 | 2 | commanded control; same reading as LongPass. |
| 13 Sliding | 13 Sliding | 11 | 2 | commanded slide; same reading. |
| 12 Trip / 8 Shot / 1 | same | 3 + 1 + 1 | 2 | same reading. |
| 14 Special | 1 Movement | 13 | **7 TRAP** | the 13 trap cases: the PES trap tick owns the body (P+0xAC8 = TRAP) on a Special trap clip while the GF command is Movement. Classified as "trap owns the body, GF command stale". Not analysed further. |

Correction to the brief: the 65 are NOT presser deadlocks on a GF clip. In 60 of 65 the command equals the clip's function, i.e. the AI commanded that ball action and the body is in it. The "pass/idle as an idle for a presser" reading in StallLive8 was wrong. So item 2's premise (the presser's MOVE action vs a kick clip) does not describe these firings; the kick command that never commits is the defect to chase next.

Presser split (StallLive8 census, unchanged): 51 forced men are the side's press man; 172 are non-pressers. Distances: <1 m 94, 1-2 m 115, >=2 m 14.

## 4. Item 3 (145 locomotion non-arrivals): NOT diagnosed
Needs one forced case per class: target, MOVESTEP output speed and heading against PES's planner formula (ETA = max(1, int(fps * dist)), fps 54, pes-re-move-gate section 1.1), and the stop radius (0.1 m precise, 0.2 m not; section 1.5). The dump has no speed/target fields yet. Next step: add the MOVESTEP command speed and the issued motion id to STALLDUMP for the fn-14 / cmd-1 rows.

## 5. Item 4 (13 trap cases): classified (section 3 last row). Open: why a Movement command sits on a PES trap body; probably the trap tick's contact owner (StallContact).

## 6. Item 5 (throw-in ankle): NOT fixed. Current runs: `TheThrowInTakersAnklesStayBehindTheLine` FAILED: 4 of 154 taking ticks short, worst 0.0269 m (round 8 said 6 of 211 with the same worst; the tick set changed between runs; not chased). It fails on base too (red in the suite). Needs its own diagnosis after items 2-3.

## 7. Acceptance table (final tree f6b07661; built from the same sources as HEAD; binaries from f6a0ad18 + the test-dump line)
| Check | Result |
|---|---|
| PesStall.* (8 tests) | 7 PASS; **ALiveBall FAIL: 223** |
| Holder-field test, no refresh | PASS |
| PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver | PASS |
| PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests | SKIPPED (the test's own named gap) |
| PesHang (seed 11, 2 min) | PASS (904.7 s) |
| PesThrowInRelease (9 tests, 4 processes) | 7 PASS; **FAIL TheThrowInTakersAnklesStayBehindTheLine (4 of 154, worst 0.0269 m)**; **ACeremonyThrowInIsReleasedFromTheHandOverTheSpot: no result, hit its 5400 s cap (RC 124; ~90 min, the split was too coarse for it)** |
| Kick-clear mutation | pass-target RED (receiverTicks 0 vs 0); reverted |
| gen_translated.py --check | GREEN, 10,150 functions byte-identical |
| ledger.py --check | GREEN, 47,642 rows |
| Seed-7 2-min pes-loop smoke | 29,597 ticks, determinism d5b32ac91262de67 |

Build dir: deleted (`rm -rf build`, after every test process had exited: the ceremony test timed out, RC 124).

## 8. Killed theories
- K1. "The holder-field test's expectation is wrong for one tick." Killed: MA = W1 value at the end of each tick; the test is right. The bug was the team-tick clear of pesHolder.
- K2. "Carry clip-kind action ids on P+0xAC8 now." Killed: aborts the run on 0x140789D00 (untranslated).
- K3. "The 65 are a presser on a GF kick clip." Killed: command == clip function in 60 of 65.
- K4. "fn 14 is Movement." Killed (Special, gamedefines.hpp:52-67).

## 9. Open items (numbered)
1. Translate 0x140789D00 and its closure (Main's ruling), then carry kick ids on P+0xAC8 (section 2).
2. Why a kick/slide/control command never commits (the 60 class in section 3): the first thing to chase, since it is where the live ball lies.
3. The 145 locomotion non-arrivals: add speed/target to the dump, then one forced case per class (section 4).
4. The 13 trap cases: a Movement command under a PES trap body (section 5).
5. Throw-in ankle, 4 of 154 (section 6).
6. One-frame hook on W1 (0x140457070) and on the kick commit, to confirm item 1's order live (section 1).
