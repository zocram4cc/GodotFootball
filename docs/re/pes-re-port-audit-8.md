# PES re-port audit 8: the seven unaudited reports (PortAudit8RE, 24-10-26)

Read-only audit of everything `docs/re/pes-re-port-audit-7.md` §"Not yet covered"
lists as unaudited, checked against PES's bytes and against the code on
`origin/main` (`a6b870e`). Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at
a time; image constants below re-read by script this pass. GF code read from
`origin/main` via `git show` into `<workdrive>/gf-PortAudit8RE-scratch/main/`.
No repo edits. Budget: ~40 tool requests used of ~150.

Method of `docs/re/pes-re-port-audit.md` (audit 1): every claim gets GF
file:line on main, a PES VA with bytes, what differs, its effect, the fix.
CONFIRMED = bytes/disassembly read this pass; cited doc = already verified in
the named report; INF = inference; OPEN = blocker named. Verdicts per report,
coverage list, final audit-coverage statement.

Reference docs (all on `origin/main` unless noted): the seven target reports
under `docs/ports/`; `docs/ports/pes-ball-A1Spin.md`, `docs/re/pes-re-ball-contact.md`,
`-contact-2.md`, `-contact-port.md`, `-ball-track.md`, `-ball-track-live.md`,
`-hitdata-index.md`, `-touchline-restart-stall.md`; `src/sim/onthepitch/pesballphysics.cpp`,
`pesball.cpp/hpp`, `pescontact.cpp`; `src/pes/mbinfo.cpp`, `movestep_ramps.cpp`,
`movestep_style.cpp`; audits 1–7.

Branch fates (merge-ancestor checks this pass): `Liftoff` IN main (merge
`f10bbc7`); `A3Salvage` ancestors IN main; `AccuracyBisect` NOT in main
(empty doc-only commit by design); `PropAudit`/`PropAudit3`/`PropAudit4` all IN
main (merges `257f581`, `32c3c91`), plus round-5 stock-dt18 commit `02e2734`.

## Verdicts per report

| Report | Verdict |
|---|---|
| `pes-bisect-accuracy.md` (AccuracyBisect, doc-only) | **PASS, no findings** — claims hold: HitIndex `+0x11` CONFIRMED in bytes; T1+T3 on main CONFIRMED; mode-1 stand-in + touchline fold still correctly ponytail'd. Doc-only by design (empty commit `fc56ab7`); nothing to port. |
| `pes-a1t2-Liftoff.md` (T2 lift + landing inputs) | **PASS with 2 findings** (F1 pre-flight claim CONFIRMED on main; F2 grounder-`vy`-only trim CONFIRMED; both as-ported. New: F1 stale `humanoid.cpp:3156-3157` line cite; F2 the report's "no horizontal trim" overstates — record `+0x1f4` 0.09 trims `vy` via `\|v_xz\|`, correctly ported as `vyTrim_mps`. No behavior change.) |
| `pes-a1t4-Liftoff.md` (T4 rolling tangential) | **PASS with 1 finding** (F3: main ADDS two post-Liftoff fixes the report predates — fallback heading `e=(0,0,1)` below 0.001 and zero-divide guard on the Y-rate slope — both byte-correct per `0x140876c28..c50` / `0x140876e1a..e37`. The report's "NOT ported" spin-cap section is closed by T4b on main. No divergence.) |
| `pes-a1t4b-Liftoff.md` (spin cap + convention) | **PASS, no findings** — cap in Hz CONFIRMED (`0x14087704e` `0F 2F D1` + `76 32` vs `boundRollMax²`); X/Z-only scale CONFIRMED (`0x14087708d` `MULSS xmm13,xmm12` pair); convention `(v_z,0,−v_x)/r` on main matches the report; mode-1 column confirmed live-shaped. |
| `pes-a1t4c-Liftoff.md` (test-premise gate fix) | **PASS, no findings** — test carries rolling state on main; cap-slip test present; mutations listed. No code claim to re-check. |
| `pes-a3-salvage-table.md` (salvage bookkeeping) | **PASS, no findings** — "brought onto A3Salvage: nothing" is stale in the kindest sense: every MOVESTEP commit it names (`c042123`, `614cd69`, `cbe68b0`, `f9d0401`) is an ancestor of main (verified). LateralDamping, LocomotionStyle, start-time switch all present on main. `stash@{0}`/`Wave1MBF-diag` dispositions are process, not code. |
| `pes-doc-propagation.md` + `-r3` + `-r4` (doc-only) | **PASS with 3 findings** (F4/F5/F6, all doc-text, none behavior; detail below). All G/G-r3/G-r4 table edits verified present on main at the cited lines; R4 float table (2.5 @`0x14259bde4`, 100.0 @`0x14253ee30`, 0.01 @`0x14256ae18`) re-read from the image and agrees; stock-dt18 round-5 quotes verified. |

## Findings (numbered, each with evidence and a proposed fix)

### F1 — A1t2's `humanoid.cpp:3156-3157` cite is stale on main (doc-only, no behavior)

- Report (`pes-a1t2-Liftoff.md` §"Attribution" of the touchline fold): "34.17 m
  touch reach ... `humanoid.cpp:3156-3157` ... `referee.cpp:579` waits forever".
- On main the fold and the referee wait live elsewhere: the touchline fold is
  `FoldTouchlineRestart`-family wiring (audits 5/7 cover the spot-clamp at
  `offside-rule.cpp:156-160` per the propagation row-55 correction), and the
  line numbers predate the Liftoff merge. The claim's substance (stand-in fold
  for an undecoded VA) still holds — the VA is still OPEN — but a reader
  following the line cite lands in the wrong file.
- Effect: none on behavior. Doc-hygiene only.
- Fix: refresh the two line cites in `pes-a1t2-Liftoff.md` (or point at the
  report's own §"Item 1" instead of line numbers).

### F2 — A1t2's "grounder: no horizontal trim" wording overstates; the 0.09 row trims `vy` through `|v_xz|` (code on main is correct)

- Report (`pes-a1t2-Liftoff.md` item 2): "grounder: K *= (f + 0.985) ...
  vy -= |v_xz_pre|·0.09·(1-f) ... No horizontal trim instruction. GF had a
  horizontal trim; removed."
- Bytes (re-read this pass): `0x140876a4b F3 0F 59 8F F0 01 00 00`
  (`MULSS xmm1,[rdi+0x1f0]` = 0.09), `0x140876a53..a57` (`|v_xz|·0.09·(1−f)`),
  `0x140876a5c F3 44 0F 5C F1` (`SUBSS xmm14,xmm1`: vy −= that term),
  `0x140876a61 F3 44 0F 11 74 24 68` (store to `[rsp+0x68]` = trimmed vy).
  So the grounder does not touch vx/vz — but its vy trim is *proportional* to
  the horizontal speed (record `+0x1f4`/`+0x1f8` = 0.09, ball.o `+0x08c`).
- GF on main (`pesballphysics.cpp:175-214` `GrounderKick`, `:402-404`,
  `:417`): `vyTrim_mps = horizontalSpeed_mps · grounderBoundYAdd · (1−f)`,
  applied to vy only, guards on old x / old z / old vy / `|v_xz/vy|` verbatim.
  **Matches the bytes exactly.** The report's "no horizontal trim" is true of
  vx/vz and the code honors it; the finding is only that the sentence hides
  the `|v_xz|`-proportional vy trim, which the code gets right.
- Effect: none (code correct). Wording only.
- Fix: none in code. Optional one-line touch to the report ("vy-only trim,
  magnitude ∝ `|v_xz_pre|`").

### F3 — A1t4's "NOT ported" spin-cap section and missing fallback heading are both closed on main (report predated; no action)

- Report (`pes-a1t4-Liftoff.md` §"Next unported term"): horizontal spin cap
  `0x14087701e..081` NOT ported; `SpinRebuildMovesRawSpinTowardTheRollingSpin`
  pins GF's 7.5 rad/s cap; "check against a live spin read before porting".
- On main: `SpinCapScale` (`pesballphysics.cpp:165-173`) implements the Hz cap
  verbatim (`rollHz = |v_xz|/(2πr)`, `rollHz <= cap ? 1 : cap/rollHz`, cap =
  `boundRollMax`); bytes CONFIRMED this pass (`0x14087704e 0F 2F D1 / 76 32`
  = `COMISS f²,cap²; JBE skip`, `0x14087708d F3 44 0F 59 EC / F3 44 0F 59 E4`
  = X/Z-only scale). T4b (`68bfc9e`, in main via `f10bbc7`) replaced the old
  test with the Hz-cap trio. Live convention check (13,518 rolling rows)
  stands as the report's evidence.
- Additionally main carries two fixes the T4 report predates: (a) below the
  0.001 heading epsilon PES takes fallback `e=(0,0,1)` instead of skipping
  (`pesballphysics.cpp:302-309`; bytes `0x140876c28 41 0F 28 E0 / 44 0F 2F 3D
  34 2A D3 01` + `0x140876c50 72 2D` = skip-divide JC, entry copy keeps
  `ez=1.0` — CONFIRMED); (b) the Y-rate slope takes `YMax` at zero heading
  (`:335-336`; bytes `0x140876e1a 44 0F 2E FF / 7A 02 / 74 15` = skip-divide,
  take bound — CONFIRMED).
- Effect: none (main is ahead of the report, correctly). No fix.

### F4 — Propagation row 34's tie-break gloss vs the doc text (doc-only; OPEN carried correctly, wording only)

- Report (`pes-doc-propagation.md` row 34): `pes-re-carrier-units.md` §3
  corridor tie-break "RNG draws via `0x141e532e0()/6`" → "`0x141e532e0` is
  game speed (54), not an RNG; tie-break rule OPEN".
- Bytes (re-read this pass): `0x141e532d0 F3 0F 10 05 7C 7B 6C 01` =
  `MOVSS xmm0,[rip+...]` → `0x14351ae54` = 54.0. So the callee returns the
  54.0 game-speed float (fps multiple per attack-decisions :809 context),
  not an RNG draw. The propagation note is byte-correct.
- Status on main: the 17-10-26 note is present wherever row 34 directed it;
  the tie-break rule remains OPEN as the report says. No code impact (carrier
  units are decode docs, not a port).
- Fix: none. Kept as the report's own OPEN.

### F5 — Propagation R3-1/R3-2 "7 inline" claim: table reads agree, dispatcher site unverified this pass (doc-only; INF carried)

- Report (`pes-doc-propagation-r3.md` §1): table `@0x142bf02c0` holds 8 dwords
  `9,8,2,3,1,4,5,6` and `0x142bf02e0 = 0`; entry 7 written inline at
  `0x14211e2d0` (`local_38 = 7`).
- Bytes (re-read this pass): `0x142bf02c0` = `9,8,2,3,1,4,5,6`, `+0x20` = 0.
  **Table part CONFIRMED.** `0x14211e2d0` head bytes
  (`40 53 55 56 48 83 EC 60 ...`) are a function prologue, consistent with the
  dispatcher — but the `local_38 = 7` inline store was not located by a byte
  scan this pass (would need the bridge decompile of the dispatcher body).
- Status on main: `pes-re-carrier-ai.md:372-373` and `pes-re-carrier-port.md`
  carry the corrected order with the `_Corrected 18-10-26_` note; both notes
  name the static-read evidence. The claim is marked CONFIRMED-by-static-read
  in the report, which matches what the bytes show for the table half.
- Effect: none on code (carrier plans are decode docs). 
- Fix: none now. If a future audit touches the dispatcher, decompile
  `0x14211e2d0` via the bridge and cite the `= 7` store offset; until then the
  note's "static read" phrasing is the honest marking. Kept as INF, not a new
  OPEN (the report already scopes it).

### F6 — Propagation G-open item 1 (TeamOrder `1409f1bb0` text) is still the honest state on main (doc-only; no action)

- Report (`pes-doc-propagation.md` §G-open): `docs/re/pes-re-teamorder.md` on
  `origin/main` has no "1409f1bb0" text; the correction was applied from a
  bridge decompile instead; "Main: check whether the TeamOrder correction is
  on another branch."
- On main today: `pes-re-teamorder.md` carries the per-player `PM+0x10`
  counters and the slot-2 22-player tick (the substance), and
  `pes-re-gaps-anime.md:549-564` + `pes-re-a3helpers.md:95` carry the
  `FUN_1409f1bb0`-as-message-handler correction with the `uVar7 = 0/0x3c`
  decode. I.e. the correction lives in the two consumer docs, not in
  teamorder itself — exactly the state the G-open item describes.
- Effect: none. No fix; the item remains Main's call (branch hunt or accept
  the consumer-doc placement).

## Coverage list (checked and found correct)

**Bisect-accuracy.** HitIndex `(w0 & 0x1fff) + 0x11`: bytes CONFIRMED
(`0x1406ad42a 41 B8 FF 1F 00 00 / 41 0F B7 06 / 66 41 23 C0` = AND-mask 0x1fff,
`0x1406ad438 66 83 C0 11` = ADD 0x11 — re-read this pass); GF
`src/pes/mbinfo.cpp:45` (`HIT_MOTION_BASE = 0x11`) with the provenance
comment; sibling bases `+0x61/+0x10/+0x20b` present as comments. BallContact
T1 (same-tick landing, `r <= y` airborne, report 6.3/2.1) and T3 (mode-1
stand-in `(0,1)`, `spinYPump` behind `pes_loop`) on main as the report says;
`ContactModeForConfig` (`cfg85 → 3`, else `T[3·cfg7c+cfg78]`, OOR → 1) matches
the `0x140872403/411` head bytes (re-read: `48 8B CB / E8 F5 C2 AD 01` call +
`CMP ...; 77 1A` range gates) and the mode table `@0x14349f718` =
`2,1,1,2,1,0,1,0,0` (re-read). A2Stall trajectory + touchline fold + restart
watch: doc's characterization (interaction, not a tuning) is consistent with
main's wiring; no code re-audited (out of this ticket's scope — the fold's VA
is still OPEN per the report itself).

**A1t2.** `vy_out = −F·min(−imp, vy_trim)` with the trim on vy only
(`Bounce()` + `GrounderKick()` as above); imp from pre-flight `|v_xz|`
(`0x140876824 E8 57 AF D2 00` call + keep — re-read); guards on old x
(`0x1408769c7 F3 0F 10 44 24 48`), old z (`0x1408769e6 F3 0F 10 44 24 50`),
old vy (`0x1408769a4 0F 2E CF / 7A 02 / 74 1C`), `|v_xz/vy|` (`0x1408769ab`);
`[rsp+0x68]` = trimmed vy (`0x140876894` + overwrite `0x140876a61` — both
stores re-read); mode-1 divisor row is `kGounnderprob` (gate `== 0` →
no-take on main, matching PES never reaching it); `BallTick` flights first,
contacts same tick (`:461-477`); `IsResting` squares the 3-D speed
(`:216-226`, bytes `0x14087648e 41 0F 2F C5 / 77 41 / 0F 2F F7 / 77 3C` —
re-read, touches-continues). Landing-metric improvement (p90 0.47 → 0.14)
cited from the report, not re-measured (harness in scratch, not committed —
correctly not on main).

**A1t4.** `W = K·v + T` horizontal (`:420-426`); heading `e` from pre-flight
`v_xz` with epsilon `0x1425a9668` = 0.001 (re-read) and the `(0,0,1)` fallback;
`A_dot`/`B_dot` over `2π`; `rollHz − over − A_dot` S3 with
`boundRollMax` (report 3.4's cap, in Hz); blend `0.5+0.5·clamp01((4−|A−S3|)/3)`
(4.0/`−3.0` literals named); `±8.5` cap (`rollToSpeedMaxRoll`,
BallBase +0x2AC); Y-rate over `|vy|/|v_xz|` between `YMin..YMax` with the
zero-divide guard; `vmax` from `rollToSpeedMaxKph`; constants
`r = 0x142598958` = 0.108685933 (re-read — contact-2's 0.142599 withdrawn as
the report says), `−1/0.5/π/3600/1000/1/4/−3/12.0/2.2222` all re-read and
agreeing; `RollSpinRamp` in Hz (`spinHz = spinRad·0.5/π`, `:106-113`,
correcting Liftoff's rad/s form); `VerticalKeep` signed (`X = −3.6·vy`,
`:79-98`, XORPS `0x140876ba2 0F 57 35 27 EB CE 01` — re-read, sign mask
`0x1425656d0` = −0.0); spin-Y pump verbatim in form
(`RawYAfterPump :351-358`: `u = |dY|/0.01`, `a = 2.5·keep`,
`blend = a+(keep−a)·u`, `rawY += blend·dY`; `0x140877147 → 0x14256ae18`
0.01, `0x140877169 → 0x14259bde4` 2.5, `×/÷100 @0x14253ee30` elided —
all CONFIRMED by rip-target resolution this pass); 0x14256ae34 = 1000.0 read
at `0x1408771bf+9` (pump's per-second scale, consumed by the `blend` chain —
noted, not separately cited in code); stop gate zeroes vx/vz only
(`:453-455`; `0x1408772d5..323` stream re-read); `kRawSpinBlendFactor = 1.0`
(`pesball.hpp:64`, the 1.0 at `0x1408770c0` per the report); spin scale
492.181 from the initializer imm at `0x140875fb7+6` (`C7 05 ... 2B 17 F6 43`,
re-read 492.1809998).

**A1t4b.** Spin cap + convention + tests (`SpinIsCappedAt...`,
`ARollingBallKeepsItsRollingSpinConvention`,
`RollingSpinAboveTheCap...`, cap-slip pair) all present on main
(`pes_ball_physics_test.cpp:201-245, 649-...`); above-cap input runaway
(10 → 124.9 m/s synthetic) carried as the report's open item 1 — still OPEN,
correctly not papered over. Y-pump behind `spinYPump`, off keeps the GF
zero-write (A1Spin smoke: off hash unchanged — cited, not re-run).

**A1t4c.** Rolling-start premise (`v/r` below the cap ⇒ `K^54`), `slope`
guard, both conversion tests green on main; mutations listed; open items
1–3 (runaway reachability, zero-slip magnitude, live cap above 5.12 m/s)
carried — all still OPEN, none contradicted by anything on main.

**A3 salvage.** All four MOVESTEP commits are ancestors of main (verified by
hash): motion start-time switch (`c042123`), seventh-pass switch port
(`614cd69`), LocomotionStyle ladder (`cbe68b0`), LateralDamping + ninth-pass
(`f9d0401`). `movestep_ramps.cpp:236+` (drift case incl. 0xbde motion,
rate ≥ 0.3, `req[0x4d/0x4e]` steps, 120/tick threshold, 2.5 m clear),
`movestep_style.cpp` (0x4a/1/0x16/20 %/cmd-5 ladder, `kGatedCommands`
`0x4018000000000024`, ball-sample index `54·20/60`), dispatch seams —
presence-verified on main. The table's per-file dispositions (byte-identical
drops, main-is-superset drops, stash patch kept, Wave1MBF-diag left for MB-F)
are process bookkeeping; nothing in them names a live code divergence, and
no live divergence was found (the named files' main versions are newer by
construction).

**Doc propagation (all three).** Every EDITED/NOTED row spot-checked on main
reads as the report says: transitions :32 (8 m from chaser `u`), :135–139
(`p = u.pos + 8 m`, heading-OPEN note kept), :224–227 (lateral sides + R3-3
decode note + −5 m bonus; side gates CONFIRMED in the bridge disassembly this
pass: #1 `0x14054141f COMISS ballz,man+0x564 / 0x140541426 JBE reject`
i.e. accept iff `man ≤ ball`; #2 `0x1405414e9/4f1 COMISS man,ball / JA reject`
i.e. accept iff `man ≥ ball`); playing-styles Destroyer note (25.0 VA);
style-effects keeper rows; A6Period/A6Writers/clock rows; clock-record 43/s;
gaps-anime `vt+0x18` + `FUN_1409f1bb0`-as-handler; stamina-live note;
match-management §2.3 corrected model text; anim-select breath-%/turn-slow
notes + §3.3 112.5°-is-7 correction; sim-motion freemove note; locomotion
banner; attack-leftovers/decisions/carrier notes incl. 54.0
(`0x141e532d0 → 0x14351ae54` = 54.0, re-read); carrier-ai R3-1/R4-H7 rows;
defender/setpiece/stadium/cloth rows; tactics sentinel (tester `CMP R8D,0x11`
pattern at `0x1408c50f0 41 83 F8 03 / 7D 27` family + setter
`FUN_140A42600` head re-read); matchlogic index-space split; tactics-clock
role-class array-B rows + all five clock floats re-read
(9000/7200/3600/5400/1800 ✓); foundation/OffBallSelectors/lane-core/extra-frontman
role-writer rows; A4Lanes/A4Cont/attack-decisions carrier-helpers rows
(H1–H13 present per the report; H14/H15 as NOTEs); stock-dt18 round-5 first
values bracketed with mod values (ball-constants nonSpinMax 105.0,
setpiece-referee sizes, off-ball selectorVision rows — spot-verified).
G-open remaining four and R3-5/R3-6 + R4-C2/C5/C6 parked states all hold as
written (verified present, none silently changed by later edits).

**Index.** `docs/ports/README.md` rows for all seven reports present
(:11–17, :58, :84–85); the audit-7 "not yet covered" list is exactly this
ticket's scope — nothing else was added since.

## Open questions (no port impact today)

1. A1t4c open 1: above-cap input spin runaway (10 m/s + exact-`v/r` input
   → 124.9 m/s over 54 ticks). Reachability (kick/landing/Magnus spin inputs)
   still undecided; the cap clamps carried state each tick so the runaway
   needs a non-capped input spin. Unchanged.
2. A1t4c opens 2–3: zero-slip magnitude and the live cap above 5.12 m/s —
   both need live spin reads, still unverified.
3. Y spin pump (`0x1408770a5..1cd`) is ported behind `spinYPump`, but the Y
   pump's own live behavior above the cap and the Magnus spin-vector
   convention for flight (T4b "Next") remain as the reports left them.
4. Kick spin (A1Spin §2: `FUN_140875580` returns `v_R`, `kicker+0xf0/fc/108/10c/2d8/2d9`
   unmapped) still blocked on the kicker-record decode + live `simBall+0x20`.
5. The `T+0x78` last-kick writer, `+0x5EC/+0x5EC` head-point writer, `T+0x255`
   latch writer, keeper-chase `FUN_140509890`, and class-4/1 press predicates
   (audits 1/7 opens) are untouched by this pass — none of the seven reports
   claims them.
6. Propagation G-opens (TeamOrder branch hunt; setpiece §12 throw-in ≈81–83;
   foul-choreography §2.1 half-start stop kind; ball-contact-2 `[rsp+0x5c]`
   identity; team-plans §2.1 scope) carry as written.

## Final statement of audit coverage across main

With this pass, **every `docs/ports/` report on `origin/main` has been
audited**: audits 1–7 covered the ports listed in audit-7 §"Covered by audits
1–7" (RoleArrays, TeamSliceA/C, FarChasePress(+Main), Parity, press-layer,
Forfeit, TeamT1/T2, walk speed, holder latch, mentality, ZoneJudge/PlayerDB,
StrategyFix, CondTactics, Sun/Grade/Post/TurfFin/exposure, DodgeGate,
GoalKick, MoveStepWire, SwitchSW, MotionRecords, TSheet, ClipWords r1–r3,
RestartRef, ThrowInTaker, SeamWire, StylePort(+2), StepFamily, LanePort,
NearestList, PenaltyScatter/Integrate, SignatureAnims, A5Repro/Tail2,
ShootVerdict, SpinRunaway(+2), BugFix, ComStyleGaps, NullHazards, A2Wiring,
B6Stadium, DeadBall, IdleEpisodes(+2), A4UnitsFix, offside-port,
offsideawareness, A1Spin, A3States, A4LaneInputs, A4Lanes(2,3), A4Level,
A4Units, A4Walk, A5Port(+blocker), A5Tail, A6Breaks, A6Whistle, AccelRamp,
StockConst, ThrowInTaker(doc), CrossLane, S1Port, DefensiveShape, keeperport2,
PenaltyIntegrate, FpkBounds, Hygiene, A5Merge, balltick-state,
carriergainfix-state, carrierimageport-state) — and **this audit closes the
last seven**: bisect-accuracy, the four A1t Liftoff reports, the A3 salvage
table, and the three doc-propagation reports (plus the README index rows
that list them).

Remaining work is **not** unaudited code — it is the carried OPEN items named
inside the audited reports: the Y-pump/Magnus/kick-spin ball tail, the
`T+0x78`/`+0x5EC`/`T+0x255`/keeper-chase/class-predicate writers, the
above-cap runaway reachability question, the live-cap verification above
5.12 m/s, and the doc-side G-opens. No report on main is left whose code it
describes has never been checked against the bytes.
