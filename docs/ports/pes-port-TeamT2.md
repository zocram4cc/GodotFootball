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
- Pure code: `src/sim/onthepitch/pesreactionblock.{hpp,cpp}`, `ComputeBlock9`, with `LoadTables` (constants, the 513-entry quarter-wave sine table at 0x142655950, the 94-word anime class table at 0x1426484f0, all read from the install), and the holder predicates over `Roster` (G+0x180). Tests: `tests/sim/pes_reaction_block_test.cpp` (CMake `pes_reaction_block_test`: 28 synthetic tests and the install-gated constants check, 29 in all).
- block[0] = distance ramp of the anchor to the closest point of the displaced segment (1 when degenerate, forced or action 0x33; 0.3..1.2 over 0.9).
- block[1], block[2]: variant (i) (p5 and R13 clear) uses Lv = (54*60*60/1000)*|motion| with the ramp 15..20 and the bands 2r+5 (F clear) or r*3.5+1.5 against 7 (F set); variant (ii) (p5 or R13) uses the ramp 10..22 over 12 and the band 2r+5 .. 5r+20.
- block[3]: the velocity-angle gap against the ramp hh = 20 - 5*r1 .. tt2 (50 under p5, 45 otherwise), zero for a class-5 holder card with byte +0x40 == 5; the tail (holder's facing, the table gap, a sign-matched 0.2) multiplies by the 60/140 band factor.
- block[4]: under p5, (int)(54*0.3) times the reach gate g, overridden by (int)(54*0.4) when the event's DL holds; with p6, 5 times the reach ramp h, kept if larger.
- Corrections verified by the asm (the decompile was wrong here): the variant selector is p5 OR R13 (asm 0x14050388f-0x1405038a5), not F; the card's flag80 is class bit 4 (`FUN_140A6E1C0`), R13 is class bit 5 with byte +0x38 in {3,4}; the DL for block[4] reads payload +0x46 even when sub-3 <= 1 (FDD20's BPL does not); the angles are in DEGREES (360 per turn); the atan core is PES's three-term series (about 0.015 deg off in the 1.19..2.44 band, which the tests allow for); the mid band's base is atan(1.5) (0x14265615c), not pi/3.
- Not wired: the match does not call `ComputeBlock9`. The card and holder records and the roster table (`Roster`, G+0x180) are caller-supplied; the roster is the input of the two holder predicates, which are now ported (`SideOfSlot`, `SideHasEntryAtMost` = FUN_140a55d40, `FirstEntryFlagClear` = FUN_140479470; FUN_141f9c3b0 is the byte at entry +0x68).
- Mutations: 13 run for block B, one per branch group (motion displacement, variant-one ramp, forced offset, variant-two band, p5 gate divisor, DL override, p6 ramp divisor, block-three offset, tail sign test, tail 140 bound, sine reflection, atan sign, substitute rule); 6 for the holder predicates (second-side boundary, the indirect byte, the flag sense, the strict limit, the forced-flag AND, the +0x06 skip); 6 for the rules that were test-only before (block[0]'s low threshold, the variant-two ramp divisor, the card's bit-11 test, the card's word test, the tail's Q0 divisor, the class-5 zero test). Each was caught by its branch's test, then reverted. Not mutated: the first-match-only rule of FirstEntryFlagClear (its test covers both orders).
- `FUN_1405027A0` (27A0): NOT PORTED. Run 4 decoded its helpers (below) and mapped its inputs; the port stops at the unbound inputs listed under "27A0 inputs (run 4)".

### Run 4: helpers of 27A0 (EXACT, unwired: `src/sim/onthepitch/pesattrs.{hpp,cpp}`, tests `pes_attrs_test`)
- `FUN_140A603C0` rating curve: row k (0..33) of the install table at 0x142645A40 (32 bytes: a flag dword, then seven floats T[0..6]); q = value/100 clamped to 0..6, r = value%100 (C division); q < 1 returns T[0]; with the row's flag byte set, r != 0 and q != 6, the result is ((T[q+1]-T[q])*f*100)/100 + T[q] with f = r/100; otherwise T[q]. Scale DAT_14253EE30 = 100 (checked by the loader). 27A0 calls it once, with row 0x20, only when T+0x4C00[k] == 0 (the keeper class: `JNZ` at 0x140502e5b skips the call otherwise; `LEA EDX,[RAX+0x20]` at 0x140502e5d; `CALL` at 0x140502e60). PES's `q > 6` alias (DAT_142645A5C) is unreachable after the clamp.
- `FUN_140A62610(side, team)`: the first starter k in 0..10 with MI+0x1748[side*11+k] != 0 and team+0x4C00[k] == 0 (the keeper class); returns k or 0xff. `FUN_140A62690(MA, side)` = side*11 + k. GF carriers: the on-pitch flag (MI+0x1748, CPU path) and `pes::roles::SideRoles::b` (T+0x4C00, `pesroles.hpp`), both 1:1.
- `FUN_140A60500()` = 0xff00000002 = the no-actor key {team 2, slot 0xff}: GF `kNoActorTeam`/`kNoActorSlot` (`pesevents.hpp`), 1:1.
- `FUN_140A58450` (slot record +0xE8 hash, +0xF0 f, +0xF4 index, +0xF8 ring[4]) and `FUN_140A58350` (+0xC8, +0xD0, +0xD4, +0xD8): the normal path returns the encoded word W. Ring word w = ring[index & 3]; keystream K_i = byte i of (3f + 4i); e_i = (w_i + K_i) ^ (K_i * 63), all mod 256; W = e0 | e1<<8 | e2<<16 | e3<<24; the check is FNV-1a 64 over e0, e1, e2, e3 (basis 0xcbf29ce484222325, prime 0x100000001b3) against the stored hash. TAMPER PATH (named, not ported): on a mismatch the code calls `FUN_1415A1714(0, 0, e2, e1, (3f+12) byte)`, an indirect call through the PES pointer at 0x14252FDE0. The port returns `kTampered` and does nothing else; the caller must treat it as unreachable.
- `FUN_140A4D660` (the kind-5 check): NOT an input of 27A0. 27A0's only call of `FUN_140A6CE10` (0x140502d02) passes p3 = 0 (`XOR R8D,R8D`), and `FUN_140A6CE10` calls `FUN_140A4D660` only when p3 != 0. (`FUN_1404F0E50` calls it directly; that path is outside 27A0.)

### 27A0 parked (run 4): bound, named, unbound
Parked by Main's ruling. `FUN_1405027A0` (27A0) is NOT ported; the helpers above are exact and unwired, and nothing reaches the sim until slice D.

Bound (an existing GF state carries the quantity):
- Event store (kind, frame, actor key) and second key E+0x180700: `EventStore`, `EventAt`, `NewestSecondKey`. Caveat: payload +0x45/+0x46 (DL of kinds 2/3) are not written in GF (ponytail: 0).
- MI+0x116C (== 1), MI+0x1748 (on-pitch), MI+0x1198 (match frame): `pessetpieceteam.hpp`.
- Ball: sample 0 (B+0x88), height (B+4), velocity (B+0x2948, m/s).
- Ball owner TM+0x24838 (clamped `< 0x16`): `GetBallRetainer()`.
- Latch T+0x255: `lastTickPossession` (`teamAIcontroller.cpp:2811-2821`).
- Role class T+0x4C00[k] (0 = keeper): `pes::roles::SideRoles::b`.
- Reaction delay T+0x6A4[k]: `reactionDelay`.
- Stamina `cur` (PMS+0xC8, getter 0x140A58350) and `max` (PMS+0xE8, getter 0x140A58450): `pes::StaminaState::{cur, max}`. Breath (PMS+0x108) is read outside 27A0 (`FUN_14092B950`).
- Live player: action +0x5D0, position +0x55C, facing +0x554, velocity +0x574.
- Rating table (install) and keeper index: `pesattrs`.

Named (meaning decoded; a new GF state is needed; 7 items):
1. T+0x558[k], starter slot id (the MARK target, `pes-re-press-eval.md:67`): per-starter field.
2. T+0xB8D0, target player id, 0xFF = none (writer `FUN_1404F7BA0`, `pes-re-team-tickets.md` step 21): TeamTick field.
3. T+0x620[k], PCC lane id, 0xFF cleared (`pes-re-defensive-shape.md:925`): per-starter field.
4. Live +0x6B8, frames-held counter (`pes-re-A4Carrier.md:405-407`): per-player counter.
5. TM+0x24710 (d²) and TM+0x247BE (rank), the NearestEntry table (`pes-re-image-rows.md:249`): team table.
6. TM+0x228D4 / +0x228D8, goal geometry (`pes-re-image-rows.md` §3.1): team fields.
7. Roster G+0x180, 24 entries (`Roster` is the predicates' input): GF table.
Partial (meaning known, mapping not verified): T+0x8E08 tactics sheet (GF stand-in); G+0x2AA8 field geometry (F+0x18, F+0x1C).

Unbound (meaning undecoded or OPEN; 9 items):
1. Rating bytes PMS+0x84/+0x85 = stat ids 0x14/0x15 (OPEN; `pes-re-anim-select-2.md:375`).
2. Block-9 keeper bit at P+0x40 (0x24): undecoded.
3. Anchor P+0x1414 (+0x141C): OPEN (`pes-re-carrier-port.md:346`).
4. B+0x80 flag: OPEN (`pes-re-carrier-port.md:327`).
5. B+0x2960 (V1): undecoded.
6. Card words PA+0x1A, +0x1C, +0x38, +0x40, +0x60: undecoded.
7. TM+0x23F7C, the per-holder pair table: undecoded.
8. TM+0x25CF8 / +0x25D38 (instruction flags 13/14/15): undecoded for 27A0's use.
9. Press-level writer T+0xA294 / T+0xA29C (`FUN_140A40BD0`): writer and unit undecoded.

Protected words: the initializer `FUN_140A3B640` (FNV constants at 0x140A3B655) stores hash 0x4D25767F9DCE13F5 (FNV-1a64 of four zero bytes) and a ring word that decodes to 0. The setters (cur 0x140A592C0, max 0x140A595C0, breath 0x140A58F60; `pes-re-stamina-drain.md:33-35`) are not decoded.

Verdict: 27A0 stays unported until the nine unbound items are decoded. The seven named items need GF state, which is slice D territory.

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
3. Block B: `FUN_140503430` (block[0..4], the factors `FUN_1405027A0` passes in) is ported as pure code, unwired (see "Step 9 block B" above), and its holder predicates (`FUN_140A55D40`, `FUN_140479470`) are ported over the roster table. OPEN: the per-player loop and the zero path of `FUN_140502190` (`B+0x291C == 3`, 0x140502274-0x140502292) and the per-player `FUN_1405027A0` calls (0x14050244F, 0x140502610); `FUN_1405027A0` itself (asm 254-778; helpers decoded in the scratch notes, see the status file).
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

## T3-T13 status (run 5): no slice is exact and observable yet
Main's rule: take T3 if a step allows a wired, observable effect; otherwise the first slice that does not depend on 27A0's unbound inputs. Result: every slice is blocked. Slice steps follow `docs/re/pes-re-tick-steps.md` (§13), `pes-re-tick-steps2.md` (§15) and `pes-re-tick-steps3.md` (§12).
- T3 (steps 19-21): BLOCKED. T3a (step 19, `FUN_140500200`, mask `T+0xB8B8`): the list block `FUN_140A714E0` has no writer found (`pes-re-image-rows.md`); the tie rule of the `FUN_140A79640` sort is OPEN; `FUN_1405969B0` is INF. T3b (step 20, tables `T+0x8C35/8C40/8D50`) reads `pl+0x124` (OPEN, `pes-re-tick-steps.md` §15 item 5), P+0x40 (27A0 unbound 2) and the pair table through `FUN_140439FE0` (TM+0x23F7C, writer OPEN: 27A0 unbound 7). T3c (step 21, `T+0xB8D0`) reads `pl+0x124`, the `T+0x78` fallback (writer open) and NearestEntry (27A0 named 5).
- T4 (steps 22, 25): BLOCKED. T4a (`0x38 = 0xFF` clear, `0x1404FDB04`) is exact on its own; T4b/T4c commit payloads are OPEN; the sheet `T+0x8E08` is 27A0's partial item.
- T5b (step 23 callees): BLOCKED. The five bodies' stores (`M+0xB8F8/B8FC` via `FUN_140A424F0` and `FUN_140A42530`, decoded) sit behind `FUN_140A64030` and `FUN_140A63020` (undecoded) and `T+0x46EC` (INF). Readers of the masks are OPEN.
- T6a (step 29, `FUN_140537AC0`): BLOCKED by the run model. Reads the 4-entry kick list `match+0x18+0x24D50` (writer not decoded) and `FUN_140537BF0` (undecoded). Writes `T+0xA2A0[slot] = 0x1E` via `FUN_140A429A0` and `T+0x130` (kick target; readers not identified).
- T6b (steps 30-32, 55): BLOCKED by the run model and by OPEN sources. Zero-init `FUN_140A40240` (decoded), history push `FUN_140A40200` (decoded) and the state-age reader `FUN_140A401C0` (`pes-re-run-targets-s1.md`). The snapshot `FUN_140A42A70(T, X)` is SIM: it copies the alias X's history into `T+0xA2CC+0x34s`, gated on `T+0xA2A0`. X's sources: at step 26c, X = R8 of the tick (`MOV R13,R8` at 0x1404F6C7B, `MOV RDX,R13` at 0x1404F6F0B), the caller's argument; at step 55, X = RAX left by the step-53 callee `FUN_140A408C0` (`MOV RBX,RAX` at 0x1404F7A7D), whose return is the T10c row base (OPEN). `FUN_14051B3E0` (steps 26b/54) only zeroes `brain+0xBB8`; it does not create X. T6b is therefore not free of OPEN inputs.
- T7a (step 35, `FUN_1404F61E0`, mask `T+0xB7EC`): BLOCKED by 27A0 unbound 7. The 146-line body is decoded (the holder, the NearestList row at `MA+0x22E4C + 200i`, the cone `FUN_140A53DA0/53DF0` (OBS), `FUN_140A4C720`, `FUN_1404AB8E0`, `FUN_14043A0A0`, `FUN_140A6E320`, the commits `T+0xB7EC[id%11]` with at most three). It reads the pair bearing table `FUN_140439FE0` (TM+0x23F7C), whose writer is OPEN (`pes-re-lane-core.md` O9). Its readers are press/duel code: a displacement search of the image finds 12 references to `+0xB7EC` (0x14051A996, 0x14054B729, 0x14057A350, 0x14057A52D, 0x140969974, 0x140993253, 0x1409ADB44, 0x140FAA318 among them), none decoded. GF has no reachable-opponent carrier, so a wired T7a would be inert.
- T7b (step 36): depends on T4. T8a/b/c (step 40): depend on C and T5b.
- T9a (step 46 driver): stores known (`T+0x6C4C/6C48/6C50/6C54`, `0x140/0x13C`, `0x6E5`); its 517-line gate chain is not fully decoded; RNG via `FUN_140A66FF0`. T9b: meaning only (NON-SIM). T9c (step 48, `T+0x6E7`): card inputs (27A0 unbound 6). T9d (release and claim loops): the tail `FUN_14057F600` payload is OPEN.
- T10a/b/c (steps 51-53): OPEN bodies (concept fan-out, row base). T11, T13: NON-SIM lock-in, no code.
- T12a (step 27d): predicates `FUN_14057AA70/A580/B30` OPEN; readers of `B6DF/B6E0` OPEN. T12b-d: depend on T12a.
- T12e (step 43, `FUN_1404F78E4`): gate `T+0x218` (GF `noBallCounter`, bound). Callees decoded: `FUN_140A40820` (clears `T+0xA2A0[s]`, pushes the history ring, zeroes the block `T+0x26C/0x28C + 0x40s`), `FUN_140A40200`, `FUN_140A85AA0`. BLOCKED by the run model.

Model gaps (decisions for Main):
1. Run table. PES keeps per slot `T+0xA2A0` (state word: 0x1E SPACE_RUN_CHANCE, `pes-re-team-tickets.md:607`; 0x25 skip gate, `pes-re-tick-steps3.md` step 26c; 0x34 PRESS, `pespress.hpp:276`; 0x36 MARK and 0x3D, `pes-re-press-eval.md:67`; 0x3C refusal, `pespress.cpp:401`), the 4-entry history ring `T+0xA2CC + 0x34s` (entries `(id, stamp, stamp)`, index at +0x30, stamp `T+0x21C`), and the concept-point block `T+0x26C + 0x40s`. GF keeps `runs` (`std::map<const Player*, RunRecord{state, enteredAt, target}>`, `teamAIcontroller.hpp:410`) and sets the press input's `assignment` to 0 as a ponytail (`pespress.hpp:324`). The ring keeps four distinct stamps and RunRecord keeps one, so the mapping is not 1:1. An exact port needs a PES-faithful per-slot table beside `runs`, or a replacement of `runs`.
2. Alias record X (T6b): persistent per team; GF has no counterpart.

Exact pure pieces ready once a model is chosen: `FUN_140A40200` (history push), `FUN_140A40240` (zero-init), `FUN_140A40820` (release), `FUN_140A42A70` (snapshot copy), `FUN_140A401C0` (state age).
Leads, not decoded: the writer of `TM+0x23F7C` (27A0 unbound 7). The image has 39 disp32 hits for 0x23F7C (0x1404BFEE3 ... 0x140A27AEA). The hit at 0x140A26BBA lies in `FUN_140a23c10` (3966 lines), whose decompile shows no reference to the table, so it is a false hit.
Decisions requested: (a) approve a PES-faithful per-slot run table beside `runs` (unblocks T6a, T6b and T12e exactly), or (b) replace `runs`; (c) prioritise the `TM+0x23F7C` writer (unblocks T7a and 27A0 unbound 7).

## Run table cutover (run 6): the PES per-slot run table replaces `runs`
- `src/sim/onthepitch/pesrunslots.{hpp,cpp}` (pure): `Slot` = `state` (T+0xA2A0[k]), `Ring` (T+0xA2CC + 0x34k: four `(id, enteredAt, seenAt)` entries, index at +0x30), and `block` (T+0x26C + 0x40k: the concept point in bytes 0..11, release clears bytes 0..0x1c, the 32-byte target block at +0x20). Rules: `Push` = FUN_140A40200, `Behind` = FUN_140A401C0, `Touch` = FUN_140A401F0, `ZeroTargetBlock` = FUN_140A85AA0 (variants 1, 0x1d..0x1f, 0x22, 0x27), `Assign` = FUN_140A42940, `Release` = FUN_140A40820, `Age` = head seen − head entered (run-targets-s1 `[+8] - [+4]`).
- `runs` (`std::map<const Player*, RunRecord>`) and `RunRecord` are deleted. `TeamAIController::runSlots` (11 own slots) replaces them; `OwnRunSlot` maps a player to his own roster index (`RunSlotOf − team·11`). Every reader and writer moved in the same change: the reset, `GetRunState`, `GetOffBallTarget`, the designated supporter, the RunWorld build, the stop pass, the running count, the two lowered-cap releases, and the chance-run creation.
- Stamp mapping: `RunRecord.start_ms` (ms) → the head's `enteredAt` in PES frames (`GetPesFrame`). RunWorld's `f.t` is now `frame − enteredAt`, the PES state frames. `RunRecord.target` → the point (block bytes 0..11). `cooldown_ms` had no reader and is deleted.
- Ponytails (not PES-exact): (1) the chance target goes to the concept point T+0x26C, while PES writes the selector sub-record (not decoded); (2) the designated supporter keeps GF's erase, i.e. release (state 0), while PES's step 38 writes BASE_POSITION (1) through FUN_140A429A0 (not ported); (3) a slot re-selected while running keeps its entered stamp (PES's push is a no-op for an equal id), where the old code refreshed `start_ms`; (4) the reset releases running slots (the old code cleared the map); (5) the age is in PES frames, not ms.
- Tests: `tests/sim/pes_run_slots_test.cpp`, 9 rule tests. Mutations, each caught by a test and then reverted: push advances on an equal stamp (`TheIndexAdvancesOnlyWhenTheHeadIsOlder`); ring wrap in `Behind` (`TheRingHoldsFourEntriesAndWraps`); release skips the target block and release clears 0x1d..0x1f (`ReleaseClearsTheStateAndThePointAndPushesZero`). A fresh ring head is entered at 0, so the first push advances the index; the expectations follow the decompiled `FUN_140A40200`.
- Repro (5 min, seed 7, `simslot`, `--pes-loop`): pre-cutover HEAD `a5fe3f7b1a6f7faa`, 34380 ticks; cutover `a5fe3f7b1a6f7faa`, 34380 ticks. Identical on this seed: no RunStop or mask decision changed here. This is not a proof for other seeds.
- Suites on the cutover tree (pre-merge `989dd94`): `pes_run_slots_test` 9/9, `pes_team_tick_test` 18/18, `pes_offball_test` 70/70. The merged tree and the full subset are verified separately.
- Not done: T12e (release sweep, step 43). It decodes as Release over the 11 slots when `noBallCounter == 0`, but its placement after `ProcessOffBall` against the PES order of steps 38/40/42/43 is unverified, so it is neither wired nor claimed exact. T6a and T6b remain blocked as before. The `TM+0x23F7C` writer was not started.
