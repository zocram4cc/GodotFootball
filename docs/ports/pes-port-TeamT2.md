# PES team tick T2: the support-timer block (steps 8, 9 and 11), TeamT2

Status: T2a, the restart kind wiring, T2b's out-of-play clear and T2c's head are ported and pushed on `TeamT2` (`00c15f9`, `1565d8a`, `578faa9`). The in-play half of step 9 and the score-on half of step 11 are OPEN, with their VAs below. Branch off origin/main `64606ce`.

## What is ported (CONFIRMED at the bytes)

Bytes are read from `pes_full.bin` (file offset = VA - 0x140000000) and from the bridge's disassembly. Constants were read from the image: `0x14256AE1C` = 1.0 (`00 00 80 3F`), `0x142588218` = -1.0 (`00 00 80 BF`).

### Step 8, `FUN_1405008E0`: stopped select, `brain+0x6C40 = 2` (T2a)
- Gate: `T+0x255 == 0` (0x1405008F8), `MI+0x116C != 1` (0x140500913), `MI+0x1554 == 5` (0x140500920). Store `brain+0x6C40 = 2` at 0x14050095B.
- The roster scan (0x140500970-0x1405009D6) calls `FUN_140A66FF0` (0x140A66FF0-0x140A67005), a pure getter (`idx >= 0x18` gives 0, else `sink + idx*0xB4`). Its result is discarded, so the scan has no effect. Step 8's whole effect is the store.
- GF: `pes::StoppedSelect` (`pesteamtick.cpp`), `TeamTick::b6c40`, called from `TeamAIController::UpdateStrategy` (pes path) with the play state from `pes::PlayStateFor`.

### Restart kind, MI+0x1554, into the ball events
- Defect: `BallEventInput::restartKind` was never assigned, so GF's ball events could not carry MI+0x1554. It is now assigned from `Match::PesRestartKind()` (`matchevents.cpp`), the kind of the referee's desired set piece. `DetectBallEvent` writes the kind to payload +0x1C (GF `0x30`) only when payload +0x2D is set (`pesevents.cpp:656`).
- Source: `pes::RestartKindOf` of the referee's desired set piece. The kinds are the JR codes of `pes-re-A5Judge.md`'s JR table (line 40): 1 kick-off, 2 throw-in, 3 goal kick, 4 corner, 5 free kick, 6 penalty kick.
- Effect in GF: none yet. GF's ball builders never set payload +0x2D, and the foul inputs `foulWord14` and `foulByte21` (`pesevents.cpp:526-531`) are never assigned. So the event store's MI+0x1554 is 0 in every GF event, and the repro hash is unchanged (OPEN 12). The time-up judge (`pesclock.cpp:222`) and the dedup (`pesevents.cpp:~650`) read that payload, so they move once it is sourced.

### Step 9, `FUN_140502190`: out-of-play clear of both banks (T2b)
- At 0x14050247E: `MI+0x116C != 1` zeroes `T+0xB860[0..10]` and `T+0x6A4[0..10]` (0x140502726-0x140502740). This runs on every call, so it is exact on its own.
- GF: `pes::ClearReactionDelays` (returns whether it cleared), `TeamTick::reactionDelay` (T+0x6A4) and `reactionDelayMirror` (T+0xB860).

### Step 9 in-play, FDD20 early branch (T2b slice A, UNWIRED)
- Pure functions `pes::EvaluateEarlyReaction`, `EarlyReactionAdmitted`, `LoadEarlyReactionConstants` (`src/sim/onthepitch/pesreaction.{hpp,cpp}`); tests `tests/sim/pes_reaction_test.cpp`.
- Covers FDD20's per-receiver early path: the kind map (1, 2, 3, 16 store; 4, p5, or an actor on the other side go to the main pass; 11-13 gate first, with the holder test and the 54-frame window), the DL/BPL flags, the ball-track extension, the ramp, and the word `T+0x6A4[k]`, k = 0..10.
- NOT called by the match yet. The table is still written only by the out-of-play clear. Slice D supplies the facts from the match; the block-B loop is not exact yet.

### Step 9 block B, `FUN_140503430` (T2b slice B, UNWIRED)
- Pure code: `src/sim/onthepitch/pesreactionblock.{hpp,cpp}`, `ComputeBlock9`, with `LoadTables` (constants, the 513-entry quarter-wave sine table at 0x142655950, the 94-word anime class table at 0x1426484f0, all read from the install). Tests: `tests/sim/pes_reaction_block_test.cpp` (CMake `pes_reaction_block_test`, 22 tests; the install test checks the constants and the table).
- block[0] = distance ramp of the anchor to the closest point of the displaced segment (1 when degenerate, forced or action 0x33; 0.3..1.2 over 0.9).
- block[1], block[2]: variant (i) (p5 and R13 clear) uses Lv = (54*60*60/1000)*|motion| with the ramp 15..20 and the bands 2r+5 (F clear) or r*3.5+1.5 against 7 (F set); variant (ii) (p5 or R13) uses the ramp 10..22 over 12 and the band 2r+5 .. 5r+20.
- block[3]: the velocity-angle gap against the ramp hh = 20 - 5*r1 .. tt2 (50 under p5, 45 otherwise), zero for a class-5 holder card with byte +0x40 == 5; the tail (holder's facing, the table gap, a sign-matched 0.2) multiplies by the 60/140 band factor.
- block[4]: under p5, (int)(54*0.3) times the reach gate g, overridden by (int)(54*0.4) when the event's DL holds; with p6, 5 times the reach ramp h, kept if larger.
- Corrections verified by the asm (the decompile was wrong here): the variant selector is p5 OR R13 (asm 0x14050388f-0x1405038a5), not F; the card's flag80 is class bit 4 (`FUN_140A6E1C0`), R13 is class bit 5 with byte +0x38 in {3,4}; the DL for block[4] reads payload +0x46 even when sub-3 <= 1 (FDD20's BPL does not); the angles are in DEGREES (360 per turn); the atan core is PES's three-term series (about 0.015 deg off in the 1.19..2.44 band, which the tests allow for); the mid band's base is atan(1.5) (0x14265615c), not pi/3.
- Not wired: the match does not call `ComputeBlock9`. The two holder predicates are inputs (ponytail at `FUN_140A55D40` / `FUN_140479470`), and the card and holder records are caller-supplied.
- Mutations: 13 run, one per branch group (motion displacement, variant-one ramp, forced offset, variant-two band, p5 gate divisor, DL override, p6 ramp divisor, block-three offset, tail sign test, tail 140 bound, sine reflection, atan sign, substitute rule); each caught by the test for its branch, then reverted. Not mutated: block[0]'s thresholds, the variant-two ramp 10..22, the card's bit-11 and word tests, the tail's Q0 formula, and the class-5 zero test; these are asserted by their tests but have no mutation run.
- `FUN_1405027A0` (27A0): asm 254-778 still to decode; its per-slot logic and the table write are not ported.

### Step 11, `FUN_1404FEDC0`: head and score-off fill (T2c)
- Inputs at the call site (`FUN_1404F6C30`): `R8B` = the return of `FUN_1404FE9C0` (0x1404F6D60, `MOVZX ESI,AL`, `MOVZX R8D,SIL` at 0x1404F6D9B), which is the latch change this tick. GF passes `latch != lastTickPossession`, the same flip the restart word reads (step 28). `DL` (0x1404F6D75-0x1404F6D97) is 1 only in play with `MI+0x119C > 1`.
- Head (0x1404FEDC0-0x1404FEF9B):
  - sign `XMM14` (0x1404FEE10-0x1404FEE28): 1.0 when `T+0x255` is set, else `[0x142588218]` = -1.0;
  - `R8B == 0`: `T+0x218 += 1` (0x1404FEF8B), and the bank is left alone;
  - `R8B != 0`: `T+0x218 = 0`, `brain+0x6CF4 = -1` (0x1404FEE33-0x1404FEE39); the bank `brain+0x6CC4[0..10]` is `sign * T+0x224[i]` in play (0x1404FEE58-0x1404FEF52; the eleven floats are T+0x224..0x24C), and 1.0 out of play (0x1404FEF56-0x1404FEF89);
  - `MI+0x116C != 1`: `brain+0x6CF4 = -1` (0x1404FEF91-0x1404FEF9B).
- Score-off fill, `DL == 0` (0x1404FF760-0x1404FF776): each `T+0x224[i]` becomes `XMM7` = 1.0 through `FUN_140A42BA0` (0x140A42BA0: `[obj+0x224+4*slot] = value`). It runs after the bank has read the old weights.
- GF: `pes::SupportWeights`, `TeamTick::noBallCounter` (T+0x218), `supportWeight` (T+0x224[11]), `supportBank` (brain+0x6CC4[11]), `supportStamp` (brain+0x6CF4).

## Corrections to the RE docs

- `docs/re/pes-re-tick-steps.md` §5: `T+0x6C40 = 3` (0x140502517) is `brain+0x6C40`, the same word as step 8's store.
- `docs/re/pes-re-team-tickets.md` step 8: the roster scan is a pure getter (`FUN_140A66FF0`), so step 8's only effect is the `brain+0x6C40` store. The word is read by step 9, so it is state.
- `docs/re/pes-re-team-tickets.md` step 9: the `B+0x291C == 3` branch zeroes both banks (0x140502274-0x140502292); it does not decrement `T+0xB860`. The decrements are on `T+0x6A4` (0x1405024AC, 0x140502526, 0x140502707).
- `docs/re/pes-re-team-tickets.md` step 9: `FUN_141E5D4C0(brain+0x62B4, 0xF)` is not a per-player "has instruction" byte. It ignores its first argument and returns `menuList[15]+8` from the constant registry (`FUN_141E5CA20`, count `FUN_141E5DE80` = 0xEA). Entry 15 is `match/pes15Test.json` (constant_match.bin; `docs/re/pes-re-tick-steps3.md:157-160`).
- `docs/re/pes-re-team-tickets.md` step 11 and `pes-re-tick-steps.md` §6: the score-off branch (`DL == 0`) fills the base weights with 1.0 (`XMM7` = `[0x14256AE1C]`). It does not zero the scores.
- `docs/re/pes-re-treact.md`: the early branch's "rnd" is the constant 54 (`FUN_141e532d0` reads a cell whose only writer, `FUN_141e532f0`, stores 54.0f); the early kinds are {1,2,3,16}; the ramp reads the ball speed B+0x2940; the table index is the local slot k; the ball-track scan defaults to n, not -1; payload +0x46 never changes the value.

## OPEN, not ported (each with its VA)

Step 9 (`FUN_140502190`, 359 asm lines):
1. The in-play switch on the newest event kind: jump table at VA `0x140502758` (16 entries, index kind-1). Kinds 1-5, 7, 9, 10-14 and 16 call `FUN_1404FDD20` (0x1405025BD, 0x1405025C0); kind 6 calls `FUN_1405027A0` (0x1405025E5); kinds 8, 15 and the default go to the tail at 0x140502615.
2. The seed-10 tail (0x140502615-0x140502705): runs only when `X+0x38 == 0`, where `X = FUN_141E5D4C0(_,0xF)` (0x140502251, tested at 0x140502260 and 0x14050261A). Its seed writes `T+0x6A4[k] = T+0xB860[k] = 10`.
3. Block B: `FUN_140503430` (block[0..4], the factors `FUN_1405027A0` passes in) is ported as pure code, unwired (see "Step 9 block B" below). OPEN: the per-player loop and the zero path of `FUN_140502190` (`B+0x291C == 3`, 0x140502274-0x140502292) and the per-player `FUN_1405027A0` calls (0x14050244F, 0x140502610); the holder predicates `FUN_140A55D40` and `FUN_140479470` (inputs to the forced flag; ponytail at their VAs, decode next); `FUN_1405027A0` itself (asm 254-778).
4. `FUN_1404FDD20` main pass (non-keepers via 27A0; keepers' GK formula), `FUN_1405027A0` (778 asm lines; its base term is now known: block[2..4] of `FUN_140503430`), and `FUN_1404FC0D0` (242 asm lines, the 6C40 payload at 0x140502512): slices C and the 27A0 port.
5. The in-play decrements (0x1405024CC-0x140502544) and the frame-mismatch decrement (0x140502549-0x140502707): left out, since they hold only on the path items 1-3 decide.
6. `MI+0x1775 == 0x18` (0x1405024A1, 0x140502549): GF's play state is 1, 2 or 3 only, so 24 never occurs.
7. CONFIRMED live (23-10-26, Main's read-only pread of the running PES, mid-match, G = 0x1EB942C0): `DAT_14351AE54 = 54.0f` (raw 00 00 58 42). So the initializer chain (`FUN_141e532f0`, callers `FUN_141fbe2a0` via 141fbf13f and `FUN_140447b50` via `FUN_14044c880`) has run by match time, and the early draw is the constant 54, as decoded in slice A.

Step 11 (`FUN_1404FEDC0`, 524 asm lines):
7. The score-on branch (`DL != 0`, 0x1404FEFDF-0x1404FF75F): the per-slot scores (ball sample `FUN_140A6CF70` at 0x1404FF08D, the player records `FUN_140A48A10`, `FUN_140A489F0`, `FUN_14124DAD0`, stamina `FUN_140A58550`, `FUN_140A62960`, `FUN_1415A1780`, `FUN_141E532D0`), the predicate `FUN_1404F52E0` (decompile line 293), and the recovery toward 1.0 while the stamp is above 0 (decompile lines 297-313).
8. The `DL` input: `MI+0x119C` is the raw kick sum. GF has no counterpart (`src/sim/onthepitch/team.cpp:500`), so the score-on branch is never taken in GF (`scoreFlag = false` at the call).
9. The stamp write `brain+0x6CF4 = MI+0x1198` (decompile line 339), which sits in the score-on branch.
10. The base weights `T+0x224..0x24C`: PES fills them from the team sheet (`docs/re/pes-re-tick-steps.md` §6, "sheet loader"). GF starts them at 1.0 (`UnitWeights`), a stand-in until that loader is ported.
11. Consumers of `T+0x218` (step 43, `T+0x218 == 0`), of the bank and of the weights (steps 19 and 23): not ported.
12. The event payload's restart kind (MI+0x1554 at payload +0x1C, GF `0x30`). Two writers read sources GF never sets: `DetectBallEvent` gates it on payload +0x2D (`pesevents.cpp:656`, no GF builder sets +0x2D), and the foul builder writes `foulWord14` at +0x1C with `foulByte21` at +0x2D (`pesevents.cpp:526-531`, never assigned). The PES source of +0x2D and of the foul word is OPEN.

## Stand-ins (ponytail)

- Step 8's `MI+0x1554` comes from the referee's desired set piece, which is cleared only by `CancelSetPiece`. PES's JR word is reset in `FUN_14050A8C0`/`FUN_14050A990` (callers not traced).
- GF's desired set piece is also set when a foul is decided (`CheckFoul`), before the whistle. PES writes JR at the whistle (`FUN_14050E770`). The ball events can carry the new kind for the advantage window. Not fixed here.
- Step 11's `R8B` is GF's latch flip. The truthiness matches `FUN_1404FE9C0`'s return (old ^ new of `T+0x255`), which is all step 11 reads of it.
- The base weights start at 1.0, and the score-on branch is not run (see OPEN 7-10).

## Tests (`tests/sim/pes_team_tick_test.cpp`, `tests/sim/pes_setpiece_stand_test.cpp`)

| Test | Covers |
|---|---|
| `StoppedSelectSets6C40OnlyWhenLatchClear` | T2a gate, latch and store |
| `StoppedSelectNeedsAStoppedFreeKick` | T2a restart kind and play state |
| `RestartKindIsThePesStoppageCode` | the JR codes of `RestartKindOf` |
| `ReactionDelaysClearWhenPlayIsStopped` | T2b out-of-play clear (both banks, return value) |
| `LatchClearNegatesWeights` | T2c sign and scale of the bank |
| `StoppedPlayFillsOnes` | T2c out-of-play bank and stamp |
| `PesReaction.*` (13) and `PesReactionInstall.LoadsTheInstallConstants` | slice A: gates, flags, ball track, ramp, word, install constants (`tests/sim/pes_reaction_test.cpp`) |
| `SkipFlagIncrements218` | T2c counter |
| `ZeroScoreFlagFillsOnes` | T2c score-off fill |
| `RestartKind.TheMatchReportsTheRefereesDesiredSetPiece` | Match accessor (install-backed) |

Mutations, each applied alone, rebuilt, run and reverted (no `MUTANT` text left):

| Step | Mutation | Killed by |
|---|---|---|
| T2a | latch gate dropped | `StoppedSelectSets6C40OnlyWhenLatchClear` |
| T2a | restart-kind check dropped | `StoppedSelectNeedsAStoppedFreeKick` |
| T2a | in-play check dropped | `StoppedSelectNeedsAStoppedFreeKick` |
| T2a | store 3 instead of 2 | `StoppedSelectSets6C40OnlyWhenLatchClear` |
| T2a | corner restart code 3 | `RestartKindIsThePesStoppageCode` |
| wiring | accessor returns the kick-off kind | `RestartKind.TheMatchReportsTheRefereesDesiredSetPiece` |
| T2b | mirror bank not cleared | `ReactionDelaysClearWhenPlayIsStopped` |
| T2b | in-play check inverted | `ReactionDelaysClearWhenPlayIsStopped` |
| T2b | set-piece state 3 skipped | `ReactionDelaysClearWhenPlayIsStopped` |
| T2c | sign not negated | `LatchClearNegatesWeights` |
| T2c | out-of-play bank takes the sign | `StoppedPlayFillsOnes` |
| T2c | no-flip counter does not count | `SkipFlagIncrements218` |
| T2c | score-off fill writes 0.0 | `ZeroScoreFlagFillsOnes` |
| T2c | out-of-play stamp not set | `StoppedPlayFillsOnes` |

Suites run on the judged heads: `pes_team_tick_test` 16/16 (`578faa9`), `pes_setpiece_stand_test` 11/11, `sim_match_test` 11/11.

## Repro

`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes-loop --pes21-root <install>`, rebuilt in the same command.

Two runs of the judged head `578faa9` (gfsim_run rebuilt in the same command): `[determinism] d0748d444ae7e0e0`, 31803 sim ticks to full time. The `origin/main` build (`b7cd787`, identical sources to `64606ce`, which differs only by two doc lines) gives `[determinism] d0748d444ae7e0e0`, 31803 ticks. T2a, T2b, T2c and the wiring leave this match unchanged: no consumer of the new words is live in GF yet, and the event restart kind is 0 in GF (OPEN 12).
