# Doc propagation: newer corrections pushed into older docs (17-10-26)

Branch `PropAudit`, commit `b8cad96` (32 files under `docs/`; `git ls-remote origin PropAudit` confirms). Docs only: no build, no tests, no gate.
PII: `tools/check_pii.sh` exit 0 (run from the main checkout); no machine path in `docs/` after the edits.

**Method.** Each correction was taken from the newer doc's own text (its "Correction", "corrects X", or a cross-reference), then the older doc's claim was fixed where it sat and a one-line note was added directly beneath it. Notes use `Corrected 17-10-26 from <doc>` with the evidence in parentheses. Original (pre-edit) line numbers are given. Where a note is inserted, the claim line was not rewritten unless its text was fully visible. Rows marked OPEN were located or flagged but not edited.

**Legend.** EDITED = claim rewritten and noted. NOTE = note added, claim text kept. ALREADY = correction was already in the doc. OPEN = not edited, reason given.

## A. Explicit corrections propagated

| # | Older doc | Orig. line | Old claim | New claim | Source (newer doc) | Status |
|---|---|---|---|---|---|---|
| 1 | pes-re-transitions.md | 32 (§1 item 3) | second man sent 8 m from the carrier | 8 m from the chaser `u`, not the carrier | pes-re-team-concepts-port.md, TransitionChase row | EDITED |
| 2 | pes-re-transitions.md | 135 (§2.1 execute) | target `p = c.pos + 8 m` | `p = u.pos + 8 m`, `u` = chaser (record +0x1c) | same | EDITED; heading reference (`T'+0x250` vs "own attack heading") left OPEN, noted at l.139 |
| 3 | pes-re-transitions.md | 223–227 (§3.1 supporters) | #1/#2 at ball+12 m along heading ±22°; #1 higher-z side, #2 lower-z side | #1 at 180°→A and #2 at 0°→A (lateral bearings, ≤22° toward A); #1 men with y ≤ ball, #2 y ≥ ball | pes-re-team-concepts-port.md, SideCounter row (`FUN_140541010`) | EDITED |
| 4 | pes-re-playing-styles.md | 135, 201–202 (§7) | Destroyer behind-tolerance 0°, read at VA 0x14259BE20 | 0° is a register-zeroed literal; VA 0x14259BE20 reads 25.0 | pes-re-style-effects.md §5/§7; pes-port-StylePort2.md §4m | EDITED |
| 5 | pes-re-playing-styles.md | §2/§3/§10 | reader census, per-style verdicts, exclusion claim | RETRACTED | banner at l.42 (16-10-26); pes-re-style-census.md §4 | ALREADY (banner present) |
| 6 | pes-re-playing-styles.md | 226–229 (§8 knob list) | six knobs listed as to delete | knobs already deleted | update at l.211 (Hygiene branch) | ALREADY |
| 7 | pes-re-style-census.md | 115 (§3, Destroyer) | 0° tolerance | 0° is register-zeroed; VA reads 25.0 | pes-re-style-effects.md §5/§7 | NOTE |
| 8 | pes-re-style-effects.md | 20 (§2) | keeper distribution factors 0.25/1.1/0.6 | positioning factors, not a distribution choice | pes-goalkick-GoalKickPort.md §7 | EDITED |
| 9 | pes-re-style-effects.md | 81 (keeper row) | "Keeper distribution; branch mapping doc-only" | "Keeper positioning"; routine returns a position | pes-goalkick-GoalKickPort.md §7 | EDITED |
| 10 | pes-re-A4Carrier.md | 221 (§8.1 heading) | five real style reads, cited to playing-styles §2–§3 | retraction note added; reads unverified | playing-styles banner l.42; style-census §4 | NOTE |
| 11 | pes-re-A6Period.md | 34 (§3) | stamp base ~7.2 M does not track HUD clock | it does: `HUD minute = (decode+F)/3600 − 2000` | pes-re-A6Writers.md §0, §7 | EDITED |
| 12 | pes-re-A6Period.md | 39 (§4) | referee whistles when `TimeUpGateOpen()` | gate is a precondition only; whistle needs steps 3–6 | pes-re-A6Writers.md §3 | EDITED + NOTE (l.73) |
| 13 | pes-re-A6Period.md | 69–70 (§7.1, §7.2) | ring policy open; LIMIT/F writers not traced | ring is wall-clock driven (any policy round-trips); LIMIT setter `FUN_140A3CBB0`, F setter `FUN_140A3CD70`, both in `FUN_14044CE80`; F += 0.5/frame | pes-re-A6Writers.md §0, §1, §1.2 | EDITED + NOTE (l.70) |
| 14 | pes-re-A6Writers.md | 67 (§3 item 5) | kind 4 at rate·5/rate·10; kind 0 vs 30·rate | kind 4 at rate·10; kind 2 at rate·5 (flag-4); kind 0 vs literal 30 frames (0x1e) | pes-port-A6Whistle.md (its Corrections to A6Writers §3) | EDITED |
| 15 | pes-re-A6Writers.md | 68 (§3 item 6) | final block: out-of-zone pattern only | also returns `rate·30 < R+0xd4` (30 s count) | pes-port-A6Whistle.md | NOTE |
| 16 | pes-re-clock-live.md | 57–58, 110–114 (§5, §6) | `MI+0x118c/0x1190/0x1194` are wall-µs stamps; `MI+0x1180` layout dead | writers act on the MB-E block `S`, not `MI` | pes-re-A6Writers.md §0, §4 | NOTE ×2; index row fixed (docs/re/README.md l.202) |
| 17 | pes-re-clock-record.md | 178 | `+0x1198` ≈57/s over 67 s (46458→49352) | ≈43/s (2,894 ticks / 67 s = 43.2/s) | pes-re-possession-latch.md §30 (arithmetic) | EDITED |
| 18 | pes-re-gaps-anime.md | 126 (vt 0x18 row) | `FUN_1406eadf0` = `*(this+0x2cbc)` = P+0x297c | P+0x377c (this = P+0xac0) | pes-re-teamctx.md §8.1; pes-re-a3helpers.md §4 | EDITED |
| 19 | pes-re-stamina-live.md | 997–1005 (§10.5) | stat output block = `match::Player+0x70` | `PMS+0x70`, `PMS = *(Player+0x1a4a8)`; the ASCII read hit another object | pes-re-stamina-drain.md (live stat bytes row) | NOTE |
| 20 | pes-re-match-management.md | 208 (§2.3 stamina subs) | per-period cap; ×0.5 "when pad-ids differ"; CB/GK not excluded | per-mode schedule; mode 2 never full cap; ×0.5 on match (+500) and form (+100) paths; GK and CB excluded; goal/assist protection; ascending sort | pes-re-stamina-subs.md (its Corrections section) | NOTE (code kept as pre-correction reading) |
| 21 | pes-re-anim-select.md | 306 (§3.5) | `c = FUN_140a581c0`, "condition/stamina byte" | dash-reserve (breath) percentage | pes-re-anim-select-2.md §4.2 (AL-10) | EDITED |
| 22 | pes-re-anim-select.md | 331–332 (§3.6) | section presented as complete | incomplete: turn slow in every phase; target `min(x, rMax·F)`; proximity cap `x *= 1 − 0.2·w·g` | pes-re-teamctx.md §8.3 | NOTE |
| 23 | pes-re-anim-select.md | 574–575 (§10 item 2) | styles 1–5, 9, 11–14 writers: other callers not traced | no other callers; FSM asks for eight | pes-re-anim-select-2.md (conclusion) | NOTE |
| 24 | pes-re-anim-select.md | 593–594 (§10 item 7) | condition byte meaning open | resolved: breath % | pes-re-anim-select-2.md §4.2 | EDITED |
| 25 | pes-re-sim-motion-mapping.md | 77 (§0 item 6) | "Locomotion = Freemove FSM" | Animetable 24-state FSM; `Freemove.bin` not read on the AI move path | pes-re-anim-select.md l.28; pes-re-anim-select-2.md | EDITED |
| 26 | pes-re-sim-motion-mapping.md | 206–207 (§2 heading) | Freemove.bin locomotion graph | describes table structure, not the live path | same | NOTE |
| 27 | pes-re-locomotion.md | 514 | Freemove key solved as locomotion | "Superseded in part" already present | pes-re-anim-select.md | ALREADY |
| 28 | pes-re-attack-leftovers.md | 202–203 (§1.3 spaceRun keys) | keys alphabetical; chanceScore 102/88 | PES19 order; chanceScore 40/69; checkBallDist 19/25; checkLastLineDist 6/11; checkZ 125/24 | pes-re-run-targets.md §0, §7b | NOTE |
| 29 | pes-re-attack-leftovers.md | 378–380 (§1.5) | `FUN_140a50250` is the team-instruction reader; f0–f2 = instruction slots 0–2 | returns the holder's record dword `rec+0xd4+4k`; f0–f2 reading withdrawn | pes-re-OffBallSelectors.md §1 (raw 0xa50250–0xa502e2) | NOTE |
| 30 | pes-re-attack-decisions.md | 512 (§3.1) | Chance: first-listed candidate, no max score | shared sort, score 100 + s·x of the runner | pes-re-off-ball.md §0 | NOTE |
| 31 | pes-re-attack-decisions.md | 524 (§3.2) | accept iff score+awareness < \|Δx\| | branch inverted; 15/21/24/26/37 m is the max longitudinal distance | pes-re-off-ball.md §0 (runs.c:618–625) | NOTE |
| 32 | pes-re-attack-decisions.md | 558 (skill bit 0xd) | "Goal Poacher"-family label | skill 13 = WEIGHTED_PASS in engine order | pes-re-attack-leftovers.md §2 | NOTE |
| 33 | pes-re-attack-decisions.md | 675 (§3.10) | rng-frame cooldowns | fps multiples (54); `0x141e532d0` returns 54.0 | pes-re-off-ball.md §0 | NOTE |
| 34 | pes-re-carrier-units.md | 170 (§3 corridor tie-break) | RNG draws via `0x141e532e0()/6` | `0x141e532e0` is game speed (54), not an RNG; tie-break rule OPEN | pes-re-Dribble-inventory.md §1 | EDITED |
| 35 | pes-re-carrier-ai.md | 193 (§5 veto) | heading within 60° "after 0.6944/gameSpeed of motion" (look-ahead) | velocity heading floored at 0.6944/gameSpeed (speed floor) | pes-re-attack-leftovers.md §1.0 | EDITED |
| 36 | pes-re-carrier-ai.md | 669 (§6.2) | style bit 7 read in carrier code only once | bit 8 also read (PassForward short tier `0x142125C80`) | pes-re-A4Carrier.md §11 | NOTE |
| 37 | pes-re-player-ai.md | 48–49 (§1) | `0x142101AA7` = BallPlayer update | address inside `FUN_1421019D0`, the tick entry | pes-re-A4Carrier.md §11 | NOTE |
| 38 | pes-re-player-ai.md | 166 (§2.3), 202 (§2.4) | units list; adoption | superseded: no score, a walk | pes-re-carrier-ai.md §0, §3; pes-re-A4Carrier.md §11 | NOTE ×2 |
| 39 | pes-re-teamctx.md | 15 (§0 item 1) | `X = *(P+0x3ac0) = PM+0x7e0` | refuted for `match::Player` (`*(pl+0x3ac0)` is garbage) | pes-re-teamtick.md l.142 | NOTE |
| 40 | pes-re-keeper-port.md | 233–237 (§2.2 item 4) | per-keeper table is install data owned by the team | runtime `MA+0x22e4c` struct built at match time; writer OPEN | pes-re-teamctx.md §2.2–§2.4, l.84 | NOTE |
| 41 | pes-re-movestep-wave1.md | 119, 120, 144, 156 (D1) | `FUN_1406ea110` at slot 0x00, `FUN_1406eab00` at slot 0x20 | slot 0x20 = `FUN_1406ea110`; slot 0x40 = `FUN_1406eab00` (vptr 0x1425e35b0); D1 slot resolved, equality with GF speed still OPEN | pes-re-a3helpers.md §4; pes-re-gaps-anime.md gap 3 | EDITED; index row docs/re/README.md l.127 updated |
| 42 | pes-re-movestep-wave1.md | 203–204 (freemove.o bounds) | `0x20, 0, 0x40` denormals are what PES reads | withdrawn: the live path reads another object; do not port | pes-re-a3helpers.md addendum §2 | NOTE |
| 43 | pes-re-ball-contact.md | 258 (§2.5 code) | `xmm14 = vx − …` (grounder kick on vx) | `xmm14 = vy − …` (grounder-trimmed vy) | pes-re-ball-contact-2.md Correction 1 | EDITED |
| 44 | pes-re-ball-contact.md | 388–390 (§3.3) | `[rsp+0x68]` = grounder-trimmed vx; `xmm14 = vx` | `[rsp+0x68]` = grounder-trimmed vy | pes-re-ball-contact-2.md Correction 1 | NOTE at 390; l.389 text left (partly OPEN) |
| 45 | pes-re-ball-contact.md | 443 (§3.4) | `vy_keep` from `\|vy\|` | signed `X = −3.6·vy_pre` (XORPS, mask 0x1425656d0) | pes-ball-A1Spin.md l.11 | NOTE |
| 46 | pes-re-ball-contact.md | 520 (§3.4 stop rule) | `velocity := 0` below 0.0486 m/s | `vx` and `vz` := 0; `vy` untouched | pes-re-ball-contact-2.md Correction 2 | EDITED |
| 47 | pes-re-officials-ball.md | 301 (§6.2) | ball.o 352 B, unnamed, source OPEN | ball.o is 1,040 B and named; 352 B is cameraInplay.o | pes-re-ball-constants.md l.14–19 | NOTE |
| 48 | pes-re-defender-decisions.md | 119, 383 (§2, human angle) | ball heading "0.6944/gs s ahead" (look-ahead) | velocity heading floored at 0.6944/gs (speed floor) | pes-re-attack-leftovers.md §1.0 | EDITED |
| 49 | pes-re-defender-decisions.md | 482 (entry 0) | entry 0 = receive | entry 0 = body-contact reaction | pes-re-receiving.md | ALREADY |
| 50 | pes-re-defensive-shape.md | 1060 (line-chain candidate test) | "0.6944/fps prediction" | velocity floor, not a prediction | pes-re-attack-leftovers.md §1.0 | EDITED |
| 51 | pes-re-setpiece-choreography.md | 127 (§2 row 5 free kick) | keeper if `d < 28` or own half | keeper if distance to own goal `< 28` or `dir·R.x < −F[6]`; designated table `S+0x143c` (= T+0xa244) | pes-re-taker-map.md §8; pes-re-setpiece-kicker.md | EDITED |
| 52 | pes-re-setpiece-choreography.md | 128 (§2 row 6 penalty) | fallback list `FUN_140a40ca0` | online path only | pes-re-setpiece-kicker.md (penalty row) | EDITED |
| 53 | pes-re-setpiece-referee.md | 846 (§1.3 throwin.o) | distFarLongPara 80; base kph 32/25; possibleDist/dist (shifted table) | 32; 50/35; 3 and 5; table offsets shifted by the vtable | pes-re-throwin-choreography.md l.537 (EV-TI-0019) | NOTE |
| 54 | pes-re-stadium-cloth-vfx.md | 60 (§1.5) | stadium id from `*(DAT_1437050c8+0xf0)` | from `FUN_141f500d0()+0xf0`, a singleton at `_DAT_1437f5d50`; `DAT_1437050c8` is a team slot | pes-re-stadium-cloth-vfx-opens.md §1.5 | EDITED |
| 55 | pes-decode-TouchReach.md | 61 (GF comparison) | `RestartPosition` folds about 52.0 / 34.0; 33.8 for stalled case | clamps to F0/F1 at offsiderule.cpp:156–160; fold claim stale | pes-re-A5Judge.md §7 | EDITED |
| 56 | docs/ports/README.md | 53 (index row A6Breaks) | "the clock never ticks in stoppages" | fixed on the PES path (see body) | pes-port-A6Breaks.md | EDITED |

## B. Index updates (summary lines that were stale)

- docs/re/README.md l.127 (`pes-re-movestep-wave1.md`): D1 slot now resolved.
- docs/re/README.md l.202 (`pes-re-clock-live.md`): wall-clock reading of `MI+0x118c/0x1190/0x1194` withdrawn.
- docs/ports/README.md l.53 (`pes-port-A6Breaks.md`): stoppage-clock finding is fixed on the PES path.
- All other summary lines were checked and are still accurate (e.g. the playing-styles retraction and the transitions/team-concepts-port correction rows).

## C. Not propagated (OPEN, with reason)

| Older doc | Orig. line | Claim | Why not edited |
|---|---|---|---|
| pes-re-anim-select.md | §3.3 | "112.5° split" (teamctx §8.2) and anim-select-2's "§3.3 parenthetical" | section text not located in the reads |
| pes-re-ball-contact-2.md | 94–95 (inside code block) | `vy_keep` uses `\|vy\|` | claim is inside a code fence; the signed correction is noted in ball-contact.md (row 45) |
| pes-re-carrier-units.md | §2.2 (sub 2 and 6 "stored"; "only 3/4/5") | conflicts with pes-port-A4Walk.md l.19 ("no PES path stores sub 2"); pes-re-A4Carrier.md §11 says sub 2 is stored (C-now) | source conflict; owner decision needed |
| pes-re-A4Carrier.md | §11 row "answer id = 0x19 none" | MainEval claim | source of the older claim not located |
| pes-re-cloth-nets.md | §3 | ClothShirtParts paragraph | replacement table is stadium-cloth-vfx §2.5; cloth-nets §3 not located |
| pes-re-physique.md | §4/§5 playbackSpeed; §0.5 collar sign | physique-helpers corrections | claim lines not located in the reads |
| pes-re-leftovers-2.md | §1.2 (0x24 stride) | officials-pick l.57–169 | not located |
| pes-re-pitch-grass.md | (whole) | pitch-b4-corrections "corrections to pitch-grass" | not read |
| pes-re-duels-fouls.md | §5.1 "offside position but not punishable" | pes-re-offside.md row 3 | claim not located |
| pes-re-foul-choreography.md | §1.2 "7 = game end" | setpiece-choreography §0 | claim not located |
| pes-re-setpiece-choreography.md | §1.1 half-start stop kind 3; §12 counter 83 and throw-in 81–83 | A5Judge §4.3, §7, §9.3 (goal-kick counter 83 refuted, release 238–253 frames) | rows not read |
| pes-re-team-plans.md | §2.1 | defensive-shape (README row) | claim not located |
| pes-re-defensive-shape.md | timer trigger | transitions (README row) | claim not located |
| pes-re-Dribble.md | FUN_14212e110 keeper-branch naming | Dribble-inventory §1 | naming only, low value |
| pes-port-StylePort.md | l.56 ("live callers") | its own l.68 correction | internal, already corrected in the same doc |
| pes-re-ball-collision.md | §5 "50.27 rad/s" | fix-SpinRunaway-2 note | clarification, not a correction |

Already noted, no edit: pes-re-team-plans.md l.841 (fore-check inverted); pes-re-tactics.md l.344–346; pes-re-dodge-gates.md l.1 (net-dodge banner); pes-re-tactics-clock.md l.194 and l.569; pes-re-attack-decisions.md l.809; pes-re-defender-decisions.md l.482; pes-re-locomotion.md l.514.

## D. Not located: TeamOrder and FUN_1409f1bb0

No text in pes-re-teamorder.md names `FUN_1409f1bb0`. Its corrections concern the `FUN_1404334e0` listing and teamctx §4.2 (confirmed). pes-re-gaps-anime.md §1 (l.549–564) holds the per-player-loop decode, which pes-re-a3helpers.md l.95 already matches. No edit.

## E. Mapping notes

- The assignment's "clock-record" is applied in two places: the A6Writers corrections go to pes-re-clock-live.md (A6Writers names that doc at its §0), and the rate correction goes to pes-re-clock-record.md (possession-latch §30).
- Notes use the form `Corrected 17-10-26 from <doc>`, with the evidence in parentheses. A few notes in the same doc use `(doc)` without the word "from"; the content is the same.

## F. Checks and open items for Main

- No build, test or gate was run (docs only, per the assignment).
- Owner decision needed: carrier-units §2.2 sub-2 conflict (row C).
- Main to check: the 17-10-26 notes read correctly in place (this pass checked each edit's response, not a rendered view).

## G. Round 2 (Main's leftovers; branch `PropAudit` rebased on origin/main `9ccc0bd`)

| # | Older doc | Orig. line | Old claim | New claim | Source | Status |
|---|---|---|---|---|---|---|
| G1 | pes-re-carrier-units.md | 109, 110, 126 (§2.1) | "else keep" / keep = no direction | keep is stored as sub 4 (blocked), not sub 2 | pes-port-A4Walk.md l.15–19 (Main ruling) | EDITED |
| G2 | pes-re-carrier-units.md | 138–142 (§2.2) | subs 3/4/5 stored, 0/1/2 mapped; hold/stand = keep path | stored subs 3, 4, 5, 6 and 0x11; 0/1/2 never stored; hold = chooser's 4 | pes-port-A4Walk.md l.15–19 | EDITED (closes the §2.2 conflict in C) |
| G3 | pes-re-A4Carrier.md | 21, 185, 190 (§1, §6) | sub 2 (keep) stored; "2 and 6 stored" | sub 2 not stored; 6 stored; notes supersede the sub-2 rows | pes-port-A4Walk.md l.15–19, l.17 | NOTE/EDITED |
| G4 | pes-re-carrier-ai.md | 94–96 (§3 item 3) | default answer 0x19 "none" then Dribble | default writes id 0x10 unconditionally; 0x19 reset overwritten | pes-re-A4Carrier.md §11 (C-now L74) | NOTE |
| G5 | pes-re-gaps-anime.md | 549, 559, 564, 604–605 (§1) | `FUN_1409f1bb0` is the per-player loop; `uVar7` = player count | message handler (returns unless kind 0x14); 60 iterations over one object `P = *(param_1−0x178)`; `uVar7` = 0 or 0x3c; 22-player path = slot 2 | Ghidra bridge decompile of 0x1409f1bb0 (scratch); pes-re-teamorder.md §2 | EDITED |
| G6 | pes-re-clock-record.md | 142–143 | tick counter `matchCtx+0x10` | per-player `PM+0x10` (22 counters), incremented by `FUN_140925750` | pes-re-teamorder.md §0 l.24, §2.1 l.34 | EDITED |
| G7 | pes-re-a3helpers.md | 95 (§4) | `FUN_1409f1bb0` (per-player loop) drives A | owner tick `FUN_1408bfcf0` (slot 2) drives A; `FUN_1409f1bb0` is a message handler | bridge decompile; pes-re-gaps-anime.md §1 | EDITED |
| G8 | pes-re-ball-contact.md | 388, 389 (§3.3) | `xmm13/12/14 = vy/vz/vx`; `[rsp+0x68]` = grounder-trimmed vx | `xmm13/12/14 = vx/vz/vy`; `[rsp+0x68]` = grounder-trimmed vy (the l.390 workaround note is replaced by the corrected lines) | pes-re-ball-contact-2.md l.66–69 | EDITED (both partial lines fixed) |
| G9 | pes-re-ball-contact-2.md | 87, 95 (§1.2 code) | `vy_keep` from `\|vy\|·3.6` | signed `−3.6·vy` (XORPS 0x140876ba2, mask 0x1425656d0); rising ball keeps the base | raw listing of FUN_140876360 via bridge (XMM6 has no writer between 0x140876ba2 and 0x1408770f0); pes-ball-A1Spin.md l.11 | EDITED (raw-checked) |
| G10 | pes-re-anim-select.md | 268–270 (§3.3) | bearing <112.5 → 6, <157.5 → 7; styles 1–5/9/11–14 set by other callers | 112.5 itself → 7; 0x10 for class <2 with a second test; no other callers | pes-re-teamctx.md §5.2; pes-re-anim-select-2.md conclusion | EDITED |
| G11 | pes-re-cloth-nets.md | 56–57, 206–209 (§2, §3) | `ClothShirtParts` holds nocloth at +0x20 and cloth at +0x78 | object A (0x80): nocloth at +0x20 (drawn); object B (0xD8): cloth at +0x58, nocloth at +0x88; +0x78 is not cloth | pes-re-stadium-cloth-vfx.md §2.5 | EDITED |
| G12 | pes-re-leftovers-2.md | 41, 42, 48 (§1.1–1.2) | 0x24-byte sub-blocks at +0x04/+0x28 | 0x90-byte sub-blocks at +0x04/+0x94 | pes-re-officials-pick.md §1.2 | EDITED |
| G13 | pes-re-foul-choreography.md | 85 (§1.2 table) | `MI+0x1554` 7 = game end | 7 = kick-off after a goal | pes-re-setpiece-choreography.md §0, §3 (EV-SPC-0013) | EDITED |
| G14 | pes-re-setpiece-choreography.md | 49 (§1.1 table) | half-start (MODE 1) stop kind 3 | stop kind 0 (reason-0 builder `FUN_14044fef0`); 3 is the goal kick-off | pes-re-A5Judge.md l.212, l.451 | EDITED |
| G15 | pes-re-setpiece-choreography.md | after 831 (§12 killed list) | goal-kick counter 83 as the release threshold | refuted: release 238–253 frames from ready; 246 inside range | pes-re-A5Judge.md §9.3 | NOTE |
| G16 | pes-re-team-plans.md | 263–264 (§2.1 item 8) | `FUN_141b92460(2) >= 2` is a 2-second timer | `FUN_141b92460` is a stub returning 5; test is constant | pes-re-defensive-shape.md l.322 | EDITED |
| G17 | pes-re-defensive-shape.md | 228 (§1 timers) | hold-back after a possession change | after the newest kick-off (`FUN_140a74ec0` is the kick-off timer) | pes-re-transitions.md §1.2 | EDITED |
| G18 | pes-re-Dribble.md | 24–25, 79–80 | `FUN_14212e110` keeper branch and distance-sorted list | address is not an entry; builder is inside `FUN_14212DC80` | pes-re-Dribble-inventory.md §1 | NOTE |
| G19 | docs/ports/pes-port-StylePort.md | 56 | "every knob has live callers" | dead on the PES path (see its l.68) | its own l.68 | NOTE |
| G20 | pes-re-duels-fouls.md | 438–439 (§5.1) | "offside position but not punishable" gloss | category 3 is the near-level onside band; categories 0–2 stand | pes-re-offside.md l.200, l.346–347 | NOTE |
| G21 | pes-re-pitch-grass.md | 785–786 (§7.6 "Skip entirely") | VS fog terms skipped | o7.z/o7.w ported into specular and roughness | docs/render/pes-pitch-PitchV7.md (summary row, l.23) | NOTE (partial: GB3/GB4, t1/t2 "skip" stands) |
| G22 | pes-re-physique.md | 9–18, 56, 162, 216 | collar and playbackSpeed claims | already corrected in place | pes-re-physique-helpers.md | ALREADY (no edit) |
| G23 | pes-re-ball-collision.md | §5 | 50.27 rad/s "cap" | not a correction: 3-D spin cap and horizontal rolling cap (7.5 Hz) are different quantities; both stand | pes-fix-SpinRunaway-2 note | CLOSED (no edit) |

### G-open: could not be closed, with why

- **TeamOrder text (Main's item 2).** `docs/re/pes-re-teamorder.md` on `origin/main` (9ccc0bd) has no "1409f1bb0" and no "message handler" text. The substance (per-player counter `PM+0x10`; slot 2 is the 22-player tick) is in teamorder §0 and §2.1. The `FUN_1409f1bb0` correction was applied from a bridge decompile instead. Main: check whether the TeamOrder correction is on another branch.
- **Setpiece §12 throw-in "counter ≈81–83 not reproduced".** The throw-in counter rows were not located in the reads of §8/§12.
- **Foul-choreography §2.1 half-start "stop kind 3".** A5 names it; the §2.1 text was not located (only the setpiece §1.1 row was fixed).
- **Ball-contact-2 `[rsp+0x5c]` identity.** G9 uses the raw listing for the sign of XMM6 (signed, no writer in between). It does not establish that `[rsp+0x5c]` is `vy_pre`; that rests on A1Spin's reading.
- **Team-plans §2.1 scope.** The defensive-shape correction was applied to the "2-second timer" claim (G16). Other team-plans §2.1 claims were not checked.
- **Keep item (Main's item 1).** Resolved by G1–G3. The conflict in section C row 61 is closed.

### G-count

Section C had 16 OPEN rows. Closed this round: carrier-units §2.2 (G1–G3), anim-select §3.3 (G10), ball-contact-2 code block (G9), cloth-nets §3 (G11), physique (G22), leftovers-2 (G12), pitch-grass (G21), duels-fouls (G20), foul-choreography §1.2 (G13), setpiece §1.1 and §12 (G14, G15), team-plans §2.1 (G16), defensive-shape timer (G17), Dribble.md (G18), StylePort l.56 (G19), ball-collision (G23). Still open: the four items above.
