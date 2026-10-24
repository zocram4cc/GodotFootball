# PES re-port audit, round 3 (PortAudit3RE, 24-10-26)

Read-only audit of five branches against PES's bytes, before they land.
Method of `docs/re/pes-re-port-audit.md` / `-audit-2.md`: GF code read from
the named branch, PES claims re-checked in `pes_full.bin`
(file offset = VA − 0x140000000) with capstone disassembly + script-read
floats (no decompiler output trusted for constants; cf. brief-rules §
"Constants in decode reports"). No repo edits.
Scratch: `<workdrive>/gf-PortAudit3RE-scratch/` (branch sources as shown).

Reference docs (all on `origin/main` unless noted):
`docs/re/pes-re-throwin-choreography.md` (§4.2, §3.2),
`docs/re/pes-re-kicker-standby.md`, `docs/re/pes-re-motion-remap.md`,
`docs/re/pes-re-step-rate.md` (§1), `docs/re/pes-re-A5Judge.md` (§9.3/9.3a/9.4),
`docs/re/pes-re-foul-choreography.md` (§9.2/9.4), `docs/re/pes-re-cutscenes.md`,
`kickoff-counter-capture.md`.

## Verdicts per branch

| Branch | Tip | Verdict |
|---|---|---|
| CarryFix | `a6f5a5e` | **Fix before landing: 2 findings** (C1 pin-side sign was inverted and is fixed on the branch, but the comment still misstates which hand holds the ball; C2 second-key selection is unguarded for single-key clips in the test mirror) — details below. The outward-pin formula itself now matches PES's bytes. |
| ClipRoot (`01e3384`) | merge of main + pin work | **Land with a follow-up: 3 findings** (R1 TURN_STOP class word reads the wrong quantity — `CommandSpeedClass` vs PES's `vt+0x68(rate)`; R2 STEP_LOOP_CHANGE's twin site at `:1250` unfixed as named; R3 keeper stand command is an idle stand-in, correctly ponytail'd) |
| InPlayRelease2 | `675ca79` | **Land as is, with the red test resolved first: 1 finding** (I1 path-B facing — `AThrowInIsReleasedFromInsideTheLine` red, cause already diagnosed in the branch's own port doc; fix named below). The `StartPlay`-at-release move matches PES's bytes exactly. |
| FKPenaltyPlace | `03798cc` (work `08977f9…b361177`) | **Fix before landing: 3 findings** (F1 FK clamp pull-in axis predicate is inverted vs `0x14074148B-4BE`; F2 `DeriveFreeKickRunUp` second-kicker arm contradicts PES's `CMOVBE` fallthrough — returns generic walk with stat 0 but never re-runs the player walk; F3 FK/PK mirror now reads `LeftFootedOf` but the remap keys stay zero-stand-ins with no citation of the `_DAT_1436F2600` loader gap) |
| GoalTiming | `74a0eb4` (work `529176d…b0a9387`) | **Land with a follow-up: 3 findings** (G1 goal→ready window still red by model — primary+hug draw only ~10% in-window, own doc admits it; G2 `PickHug` level filter (`>>12|>>13`) fires on bits the doc claims every hug carries — vacuous filter, correctly conservative; G3 `kKickOffReleaseFrames = 125` bakes the polluted-geometry mean over a 118–139 spread — acceptable stand-in, tolerance named) |

## Findings

### C1 — CarryFix's outward pin pushed the taker INTO the pitch; fixed on-branch (`a6f5a5e`), comment now contradicts the code (fix: one comment line)

- GF (`teamAIcontroller.cpp`, `PinBallAtRelease` on `origin/ClipRoot`): `excess = side*hand.y − (F1 − 0.1)`; `OffsetPosition(−side*excess)`, i.e. pushed **inward**, and measured the **hand**. PES (`FUN_140792E10`, phase 1, `0x140793437-55C`): reads `F[1]` (`RBX = [RAX+0x2aa8]`, `+4` = F[1]), adds the install inset `DAT_142588200` = **0.1** (`0x14079351C` addss, target verified `0x142588200` = 0.10000000149011612), compares the **further-outside of bones 3/6** (`FUN_141eb51a0` ×2, `0x140793478`/`0x1407934B0`, cmovbe picks the further-outside z), and pushes the **root** outward by the shortfall via `FUN_141eb5bb0` (`0x14079353C-55C`, `byte [RSP+0x20] = 1` = outward).
- The branch tip (`a6f5a5e`) corrects the direction: `excess = (F1 + 0.1) − side*pos` on the **root**, `OffsetPosition(+side*excess)`. Verified against the bytes above: the target is F[1]+0.1 and the moved body is the root. **The formula now matches.**
- Residual: the commit message says "the ball is in the inner hand" while the code comment says "the outer edge … OUTSIDE the touchline". PES's pushed quantity is the further-outside bone z (either hand, whichever is further out — `cmovbe` selects max |z|), not "the inner hand" nor "the root's outer edge". GF has no finger-bone positions, so a root stand-in is forced — but the ponytail should name the actual quantity (`FUN_141eb51a0` returns bone world-z ×2, max selected), not two different wrong anatomies in message vs comment. Fix: one line — "ponytail: PES pushes on max|z| of bones 3/6; GF's root edge stands in".

### C2 — The release key is hit index 1, but only when the clip has ≥2 keys; the test mirror hard-codes the same unguarded indexing (fix: guard + mirror)

- PES: the release event is `FUN_1406ec260(ap, FUN_1406b08a0(clip))` (`0x1407933D5-3DA`): the release frame comes from the clip's kind-3 event, and the ball request at release reads the ball key AT that frame (throw-in doc §4.2: "release … keys are hit 1 of motions 3324 and 3323"). GF (`PinBallAtRelease` tip): `hits.size() > 1 ? hits[1] : front`. For the two run clips (install table OBS: keys at hit 0/1) this is right.
- Gap: a clip with exactly **one** key falls back to `front` — correct — but the throw-in test (`ACarriedThrowInIsHeldAtItsRowsKeyOffsetUntilTheTouch`) computes its expectation with the same `size>1 ? [1] : front` expression, i.e. **the test mirrors the implementation instead of the install table**. If a placed-taker row ever resolves to a single-key clip, both move together and the pin is untested. The doc's §4.2 table gives the release keys per row; the test should pin at least the two run rows' expected key heights (1.857 m / 1.880 m at scale 1.0) as relations, not re-derive the index. Fix: assert `hits.size() > 1` for the rows the test exercises, or pin the two key heights.

### C3 — `SelectRetainAnim` start-position fix matches PES's hold semantics (checked, correct)

- GF tip (`humanoid.cpp:1906-08`): `startPos = spatialState.position; CalculatePredictedSituation(...)`. PES's hold (`ThrowinLoop` phase 2→6, `0x140793590` family) never re-roots the body except through the explicit warps (`FUN_141eb5b30` snap-back `>0.05 m`, `0x140793A57-C7`; `FUN_141eb37b0` 17-frame warp `0x140793B13-37`). A hold that restarted from the previous clip's start would visibly jump; starting where the body is matches. Correct as ported.

### R1 — TURN_STOP's class word reads the command class; PES reads `vt+0x68(rate)` of the +0xac0 interface (fix before landing ClipRoot's MOVESTEP part: call `ClassForRateFor`)

- GF (`movestep_step.cpp:1491-94` on CarryFix/ClipRoot): `band = clamp(host.CommandSpeedClass(), 0, 3)` → `kClassWord`. PES (`FUN_1406cab20` → `0x1406caf20-2D`: `LEA RBX,[RDI+0xac0]; RCX=RBX; XMM1=req[0x4a]; CALL [RAX+0x68]`, then `0x1406caf30 MOV [R13+0x7C],EAX` = `req[0x1F]` = class word). Input is the request's **rate word** (`req[0x4a]` = `kRate`), output is the class 0..3.
- `host.CommandSpeedClass()` is the AI command's class (`cmd[0x17]` → `CommandClassFor`, `pesmovestep.cpp:46`; turn.cpp:124) — a DIFFERENT quantity from the `vt+0x68` rate class (step-rate doc §1c: "conflating them reintroduces ClipRoot's alternation through the back door"). The commit message (`f393fe7`) claims the port's class stand-in is `CommandSpeedClass` — it is not: `ClassForRateFor(req.F(kRate), host.RunTablesView())` is already ported (`movestep_run.cpp:323`, same ladder/answers VAs as `FUN_1406ba030`) and `RunTablesView` is on the `MoveHost` contract.
- Effect on the branch today: the command class is usually −1 (`CommandClassFor` returns −1 outside 1..5) → clamped to 0 → class word 0 always, where PES returns the rate class (0..3, ladder `0x1425dde20` = 0.29/0.59/0.7/1.0). Observable whenever rate ≥ 0.29 (class ≥ 1): wrong step row (`kindBandRow[kind + band*8]`), wrong clip. Fix (per step-rate §1c): `band = ClassForRateFor(req.F(moveword::kRate), host.RunTablesView())`, drop `kStepBandCount`/`clamp` (the ladder saturates at 3 already). One line + delete the constant.

### R2 — STEP_LOOP_CHANGE's `:1250` site keeps the same misread (named in the commit, still open)

- GF (`movestep_step.cpp`, `STEP_LOOP_CHANGE` run-turn arm): `band = host.BandSpeedKmh(int(req.F(kRate)), host.PhysiqueG())` → `kClassWord`. PES (`FUN_1406c6480` → `0x1406c66E6-799` per step-rate §1b): same `vt+0x68(rate)` call as TURN_STOP, same input/output. The `f393fe7` message explicitly leaves it ("unchanged here"). Same fix as R1, same one-liner. The second `BandSpeedKmh` misuse (the `EnterRunTurnStep` arm just above it) is the `vt+0x48(rate,G)` band-speed shape — that one is a speed, correctly `BandSpeedKmh`-shaped; only the `kClassWord` stores are wrong.

### R3 — Keeper stand command + `!IsInPlay && !IsInSetPiece` gate (checked, correct with a ponytail gap)

- GF (`elizacontroller.cpp:146-162`): the idle branch now excludes set pieces, and the goal-kick branch issues an explicit stand (idle velocity, look at ball) until contact. PES: every standby but the kick-off loops returns 1 from slot 10 (kicker-standby §2 table: `GoalKickStandby 0x140775730 → 1`), i.e. the default locomotion is skipped — and the keeper's body is held by the standby clip (`goalkick_idle` row 0x84/0x83, rate 1.0), NOT by an idle-velocity movement command.
- GF has no standby clips, so a movement-stand is the only available hold; the RED-ALERT it fixes (strike clip ends before contact, no command, 122.97° snap) is real. The stand direction uses `GetDirectionVec` (travel) rather than the root yaw — for a keeper standing on his spot the two coincide; acceptable. Correct to land; the ponytail should cite slot 10's return (already does via `pesgoalkick.hpp`). No change.

### R4 — Clip-start-from-root-yaw (`505a97a`) and `HoldsRoot` removal (checked, correct)

- GF (`humanoidbase.cpp:525-27`): every `PlayIssuedClip` starts from `RootStartAngle()`. PES continues each motion from the root's yaw (MOVESTEP req[2] is the root/body yaw; the throw-in turn warp `FUN_141EB35A0` writes the root, and the next clip continues it). The pre-existing throw-in-only `HoldsRootYaw` special case is deleted with its last caller — clean cutover, no shim. The wire test (`AnIssuedClipStartsFromTheRootYawOfAnyBody`: 45° split, 0.5° tolerance) pins the behaviour, not the implementation. Correct.

### I1 — `StartPlay` at the touch matches PES; the red path-B test is the §4.1 phase-0 yaw, not the flag move (land once green; fix named)

- GF (`referee.cpp:713-719` on InPlayRelease2): `StopSetPiece(); StartPlay()` in the touch block. PES (`FUN_140792E10`, `0x1407933E3-419`): at the release event — `OR [RSI+0x2460],0x80` (`0x1407933EA`), `FUN_1404511E0` + `FUN_140a605E0/607D0` gating, then `CALL 0x140445750` (`0x140793419`) which flips the in-play field. Kicks converge on the same writer (`FUN_14047FEB0 → FUN_140445750`, A5Judge §3.6). Verified bytes this pass. **The flag move is exact.**
- The branch's own port doc already diagnoses the red test (`AThrowInIsReleasedFromInsideTheLine`, 1/4 path-B releases 0.24 m out): the path-B taker turns toward the ball during the take (GF ShortPass approach) while PES's `ThrowinLoop` phase 0 yaws him toward the throw target (choreography §4.1: yaw warp to base ±65° over 13 frames, `0x140793857-85E` `FUN_141EB35A0` + `0x14079385E` apply). Fix: hold a path-B taker's facing on the throw target during the take (the doc's "next action"). Land the flag move together with that fix — they are one behaviour (release geometry), and the red test is its pin.
- `elizacontroller.cpp:150` (`!IsInPlay() && !IsInSetPiece()`) is the necessary companion: with takes out of play, the `!IsInPlay` arm would otherwise swallow every take tick's command. Verified present on the branch. Correct.

### F1 — The free-kick stand pull-in tests the wrong axis (fix before landing: swap the predicate)

- PES (`FUN_1407411F0`, `0x140741456-4DC`): `r5 = |x|−F0` (`0x142565600` = 0.5 verified at `0x14074145E+0x1E241A2`), `dist = sqrt(r5² + dz²)`; `if (dist ≥ 0.5)` skip pull-in. Else `if (|x| > |z|)` pull **x** to `sign·(F0−0.5)` (`0x14074148E-4A4`, xmm3 = −1.0), else pull **z** to `sign·(F1−0.5)` (`0x1407414BE-4D8`, xmm4 = +1.0). I.e. the axis with the LARGER field-coordinate magnitude is pulled in — the axis further past its mark.
- GF (`FreeKickFieldClamp`): `if (|overX| > |overZ|) pull x else pull z` where `overX = |x|−F0`, `overZ = |z|−F1`. **Same predicate — checked, correct.** Withdrawn: the first-read inversion hypothesis fails on re-read (PES compares `|x|>|z|` of the stand, GF compares the overshoots; when the stand is outside the corner these coincide, and inside 0.5 m of the corner with both overshoots negative … `|overX|>|overZ|` picks the axis CLOSER to its mark, where PES's `|x|>|z|` picks the larger coordinate. Counter-case: stand at (F0−0.1, F1−0.4): PES pulls x (F0−0.1 > F1−0.4 in absolute metres — wait, these are different axes with different marks; `comiss xmm2,xmm0` at `0x14074148B` compares |x| vs |z| as raw coordinates, e.g. 52.46 vs 33.66 → always pulls x near the corner. GF compares −0.1 vs −0.4 → pulls z. DIVERGENCE, but only inside the 0.5 m corner disc, and PES's raw-coordinate comparison there is almost surely a "pull the length axis" constant behaviour, not a per-axis overshoot test.
- Verdict: GF's overshoot comparison is arguably the *intended* behaviour but NOT what the bytes do. PES pulls x whenever |x|>|z| (i.e. essentially always at the corner, since F0 ≫ F1). Fix: `if (fabs(fox.x) > fabs(fox.z)) pull x else pull z`, citing `0x14074148B/4BE`. One predicate line.
- The 1.5 m clamp (`0x1407414E9-522`, `0x14259BDDC` = 1.5 verified) and the ±360 fold (`0x14074138D-3A8`, ±`0x14259858C/598` verified) match. Correct.

### F2 — `DeriveFreeKickRunUp` mishandles the second-kicker fallthrough (fix before landing)

- PES (`FUN_1407411F0`, `0x140741286-31A`): `MI+0x1740 → EBX`; `CMOVBE R15D,EAX` zeroes the **stat** when `EBX ≤ 0x15` (a second kicker exists — note polarity: `jbe 0x140741303` SKIPS the player walk when a second kicker exists, and the CMOVBE already zeroed the stat for the generic walk). No-second-kicker (`> 0x15`): player walk with (card, base) → if changed, done; else generic walk with (stat, base).
- GF (`derivation.cpp:258-265`): `if (!secondKicker) { player walk; if changed return; } return generic(secondKicker ? 0 : stat)`. The `secondKicker == true` path returns `generic(0)` — matches PES (player walk skipped, stat 0). The `false` path runs player-then-generic — matches. **Checked, correct.** Withdrawn on re-read; the CMOVBE polarity and the branch agree.
- Residual (advisory, no fix): the keys are `SetPieceKeys{}` zeros (card 0, stat 0) with the MI+0x1740 ponytail. Card 0 = generic path in the walkers (`idField == 0` matches any motion), so a zero card over-substitutes: `DerivePlayerMotion(t, 0, base, kind)` returns the FIRST generic row with `motionIn == base`, not "no walk". PES with card 0 does the same (`FUN_1406EA7B0` → 0 walks the list identically) — so exact, not a stand-in. No change.

### F3 — FK/PK run-up base (0x509/0x8EB, kind 9) and remap kinds (2/4) verified; mirror now foot-sourced (checked, correct)

- PES base: `MOV [RSP+0x40],0x509` / `MOV [RSP+0x140],9` (`0x14074129B-2A6`); kind `MOV R9D,2` ×2 (`0x1407412ED/30E`); PK kind 4 (`0x1407829C1/9E2` per remap doc). GF matches all four immediates. Mirror: PES `FUN_1406E6660(AP,0x34)==1` (`0x140741290-298`); GF `match->LeftFootedOf(playerdata)` (Player.bin `+0x34` preferred foot, `8f11575`). Correct cutover from the `kMirror=false` stand-in.
- `PlaceRunUpStand` negates x AND z of the first hit (`FUN_1406E8CC0` index 0: `r8d=ebx` base, `byte [RSP+0x20]=0` index, `0x14074129B-33E` — index 0 CONFIRMED, not 1: the placer uses the record, and record 0 is the contact offset for the run-up clips). The throw-in's index-1 (C2) is the release-clip convention, not the placer's. No conflict. Correct.

### F4 — The release hold (`ReleasePieceTaker`, `b361177`) ends the hold for the right kinds (checked, correct)

- PES: every standby's slot 10 returns 1 (kicker-standby §2 table) — the hold ends when the kick action takes the body (CanEnter admits the standby's sub-id, §5 table). GF ends `pieceHeld` for FK/corner/PK at the release and keeps it for GK/throw-in (strike / release-clip releases). Matches §5's hand-over table. The stand-in for the run-up clip (approach moves the taker) is §10's documented multi-commit gap, ponytail'd at the call. Correct.

### G1 — Goal→ready window: the pool draw cannot reproduce the live window (follow-up, not a blocker for the hold)

- GF (`goalsequence.cpp:34-55`): primary = uniform over generic-pool scenes of `level` (bit 6 + level bit 11+level), redraw while repeating `previousIndex`, 10 draws then fallback scene 0 (`FUN_140477400` per scene-choice §2a); hug = uniform over bits 2|3 with level bits 12|13 (`FUN_140474b20`); length = Σ(first-cut `maxTime`/60 + step `fade_s`).
- Own-doc admission (A5Judge §9.3a, carried onto the branch): only ~10% of draws land in the 757–843-frame live window; three live goals all inside ⇒ P(draw) ≈ 0.1%. The level source (`goal-ctx[0]`) and the other `FUN_140477bc0` scenes (items 1–4: pre-scene, team-mates, turn, coach) are missing. The branch's `AGoalIsReadyInPesWindowAfterItsScoring` is red (878 vs 757–843 on seed 7) and stays red by design until the level source lands.
- The length formula itself matches foul-choreography §9.2 ("Σ cut maxTime + 0.5 s step fades") modulo the variant question: GF takes `group->front().fdc.front()` (first variant, first fdc) with the condition-key matcher ponytail'd (fixdemo.cpp: ponytail cites cutscenes §4.2). PES picks the variant by the 0x43 roll / condition keys — a per-goal length variance GF collapses to the first row. Bounded (same cut family, ±10% lengths), correctly labelled. Follow-up: the level source + variant matcher, not this branch.

### G2 — `PickHug`'s level-bits filter is vacuous on the install's data (checked, conservative-correct)

- GF (`goalsequence.cpp:48-53`): keep hugs with `((flags>>12)|(flags>>13))&1`. The branch doc (A5Judge §9.3a) states all 60 install hugs carry bits 11, 12 AND 13 — so the filter keeps everything. PES (`FUN_140474b20`) filters by the goal level against level bits 12/13 (hug pool bits 2|3). With GF's level hard-wired 0 (`kGoalLevel = 0`, ponytail: `goal-ctx[0]` OPEN), filtering on 12|13 instead of the level's bit admits level-1/2 hugs into a level-0 draw. Harmless today (lengths, not semantics), and the conservative direction (superset). Follow-up with G1's level source. No change.

### G3 — Kick-off hold 125 = the polluted-geometry mean over a 118–139 spread (checked, acceptable stand-in)

- GF (`pessequencer.cpp`, `kKickOffReleaseFrames = 125`): hold applied to goal kick-offs only (`referee.cpp`, `goalRestart` flag). PES: armed→release 118–139 (capture: 118 ×4 outfield, 139 ×3 keeper-taker) and 125 ×7 (A5Judge §9.3). GF uses 125 for both.
- The capture's geometry is polluted (one outfielder per side; keeper sometimes takes the kick-off) — but the measured quantity is frames from arming (step 0x1a park) to the taker's kick release, which the structure (§9.4: scene-busy path vs taker-kick scan `FUN_14044d2e0`) shows is taker-approach + kick-action timing, geometry-independent to first order. The keeper-taker 139 vs outfield 118 spread (≈2 s at 54 Hz… ≈0.4 s) is real variance GF collapses. The test window (118–139) pins the tolerance, not the mean — the port is inside its own test's window by construction (125 ∈ [118,139]). Acceptable; the variance source (taker distance to the spot) is the natural follow-up, not a finding.
- `goalRestart` scoping (first kick-off, half starts, dead-ball kick-offs unheld) matches §9.3a's "goal programme" framing and the A5Judge §4 programme list (only a goal parks at the celebration; other kick-offs arm through the teleport tail). `RestartLimitTicks` adds `ReleaseFrames(KickOff)` to the goal bound — consistent. Correct.

### G4 — Post-scene programme (`RunGoalProgramme`: wipe → UI/teleport → fade-in → tail) matches the foul tail's shape (checked, correct)

- GF: `RunProgramme({Wipe, UiAndTeleport, FadeIn, Tail}, celebrationEnd, k, false, 0)`. PES goal builder (`FUN_1404505D0` family via A5Judge §4): wipe → teleport → fade-in → armed 0x1a; foul §9.4 tail = 0x11 wipe 5 f + 0x1d/0x16/0x13 11 f + 0x14/0x15 40 f + 0x34/0x19 18 f (+0xd variant). GF's constants come from the install (`GetStoppageConstants`, dt18-correct per brief-rules). The shape (no 0x1d/0x1f substitution window for goals — correct, no subs at a goal) matches. Correct.

## Coverage list (checked and found correct, no finding)

- **CarryFix row/column**: idle rows `0x9b8/9b9/9ba`, run rows `0x9bb/9bc`, 13.0 @`0x1425c5948`, 25.0 @`0x14259be20` (carry `0x140792F13`/`0x140793100` immediates resolved to those VAs this pass; the `jbe`-chain order — `>25 → long` first, then `>13 → mid` — matches `ThrowInIdleReleaseRow`'s `<=` chain; carry `>reach → mid` matches `ThrowInCarryReleaseRow`). Column 10 = 000 plain (animetable 0x12 columns; the `kThrowInPlainZeroColumn` name verified against `Cell(row, 10)` usage in the carry-entry `FUN_1406de990` column call at `0x140792F8F`).
- **CarryFix 0.1**: `DAT_142588200` script-read 0.10000000149011612; loaded via `PeImage::ReadVa` with opcode check (no transcribed table). The `0x140793524`-addend resolution initially mis-attributed a neighbour float (`0x1425881F8` = 0.0) — re-resolved to `0x142588200` by the capstone byte length (comiss/addss are 8 B with the 66 prefix… the disp arithmetic confirmed 0x142588200). Cautionary tale for F1-style neighbour reads; the port's VA comment is the right one.
- **Throw-in rate ladder** (`0x140793128-164`): 5 m floor, 0.75 + 0.3·(d−5)/5, 1.05 cap — not ported by any branch (the take plays at rate 1.0 in GF). OPEN, correctly untouched: porting the rate without the clip-timing consumer (`FUN_1406af620` frame test) would change nothing observable.
- **InPlayRelease2 release-bit chain**: `0x80` at `0x1407933EA`, `FUN_1404511E0` + `FUN_140a605E0/607D0` gates, `FUN_140445750` at `0x140793419` — all bytes verified this pass. `FUN_140445750` head (`0x140445750-787`: `p+0x20 == 0` gate, `xchg [RDI+0x18]`) is the flow in-play flip, consistent with the doc's claim. The ceremony pin (`IsPieceTakerHeld`) covers path B from the take; the placement-to-take window (F0+162→178, ~16 frames unpinned) stays a named ponytail. Acceptable: the ball sits at R (= the stand's design point) during it.
- **FKPenaltyPlace SetKick order**: ABB30 cell → EA7B0 card → (no-second: B3560 player → B3780 generic(stat)) / (second: B3780 generic(0)) → E8CC0 index 0 → B1C70 contact yaw → ±360 fold → 732D40 compose → corner-pull-in → F0/F1+1.5 clamp. GF's `PlaceRunUpStand` follows this order exactly (card/stat/second → motion → offset → yaw → heading → stand → clamp → look). The ±360 fold VAs (`0x14259858C/598`) and the 1.5 (`0x14259BDDC`) verified this pass.
- **FKPenaltyPlace `LeftFootedOf`**: Player.bin `+0x34` preferred foot (`8f11575`, Main-audited per handover). Correct source for `FUN_1406E6660(player, 0x34)`.
- **GoalTiming scene I/O**: strict table reader (every column consumed, lands on section end), first-cut-maxTime + fade per step, terminator at cutRef 0 — matches cutscenes §2.2/§2.3 and foul §9.2's formula shape. `LoadGoalScenes` pool split (generic bit 6 / hug bits 2|3) matches scene-choice §2a and `FUN_140474b20`.
- **Step-rate §1a/1b**: TURN_STOP (`0x1406caf20-2D`) and STEP_LOOP_CHANGE (`0x1406c66E6` per doc) call the same `vt+0x68` implementation with `req[0x4a]` — re-verified TURN_STOP's site this pass (`LEA RBX,[RDI+0xAC0]`, call, `MOV [R13+0x7C],EAX` = req[0x1F]). Supports R1/R2.

## Open questions (no branch impact today)

1. PES's hand-bone world positions (`FUN_141eb51a0` over bones 3/6): GF has no finger bones — the root-edge stand-in (C1) stands until the hand rig lands (AGENTS.md two-skeleton rule).
2. The throw-in release rate ladder (5 m / 0.75–1.05): unported, needs the clip-timing consumer first.
3. `FUN_1406B3780`'s stat keys (`FUN_1406E81F0` FK, `FUN_1406EA760` PK) and the card source (`FUN_1406EA7B0`): GF passes zeros — exact for card 0 (F2), but a future signature-card port must re-audit the zero-stat generic walk.
4. Goal level source (`goal-ctx[0]`) + `FUN_140477BC0` items 1–4 + variant matcher (G1/G2 follow-up).
5. Kick-off release variance 118 vs 139 (taker-distance term): collapses to 125 (G3).

## Recommended next step (per branch)

- **CarryFix**: fix the C1 comment line (name max|z| of bones 3/6), harden the C2 test (assert multi-key rows / pin key heights). Then land.
- **ClipRoot**: apply R1+R2 (two one-liners to `ClassForRateFor(req.F(kRate), host.RunTablesView())`, delete `kStepBandCount`). Then land; R3/R4 as is.
- **InPlayRelease2**: hold the path-B taker's facing on the throw target during the take (I1 fix), confirm `AThrowInIsReleasedFromInsideTheLine` green. Then land.
- **FKPenaltyPlace**: fix the F1 predicate (`|x|>|z|` raw-coordinate comparison per `0x14074148B`). Then land; F2 withdrawn, F3 as is.
- **GoalTiming**: land as is (hold + programme + pool loader all match the bytes; the window miss is owned and tested red). Follow up with the goal-level source.
