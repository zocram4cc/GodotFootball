# Foundation verification (round-1 tables, independent re-check)

Owner: FoundationVerify. Read-only round. Date: 2026-10-15. Base: origin/main `1ff9c3b`
(tables cite "origin/main 2026-10-15"; GF symbols re-grepped at `7a15efe` — all
`teamAIcontroller.cpp`/`pesoffball.hpp`/`pespress.hpp`/`pesdodge.hpp`/`pesdefshape.cpp`/
`match.hpp`/`team.cpp`/`ball.cpp`/`gametypes.hpp` md5-identical to `1ff9c3b`, so no drift).
Canonical paths: `<local>`.
Methods: Ghidra bridge decompile per writer VA; S1 scan re-run (`extscan-full.txt`,
57 lines); new avenues for writer-not-found rows (SIB-mod0 scan over 23 disps: all 0 hits;
rep-movs audit: 83 sites in .text, none inside the MA/T/ring writer regions; setter family
`0x140a42430`-`0x140a42940` enumerated — all members are T+0x8e08 sheet accessors or
scalar setters, none touches G+0x25cf8); GF symbols grepped in `src/sim` at HEAD.
Live reads (V1/V2/V3/P3 with LiveTeamPlayer-2, V6/P1/P2 with LiveMatchBall-2, MA items with
LivePoke) were queued during the live match but NO values had arrived when this report was
written — rows needing them stay Y/open below, nothing is closed on live evidence.

Verdict per row: CONFIRMED (writer store + meaning + GF status all check out),
CORRECTED (row is right in substance, fix noted — mostly the `ponytail:` line pointer),
REFUTED (claim contradicted by decompile). "Writer not found" rows: search result recorded.

## 1. Counts (recomputed from the canonical files)

TeamPlayer: 82 field rows (file claims 81 — the sections sum 11+2+2+10+12+15+16+8+6=82;
the ranked table says 81 because T-CONC is ranked 9 but holds 10 rows: the T+0x6d4 relay row
was added without bumping the rank count).
Status: faithful 5 (T+0x254, S+0x7b10 line bands, PM base; plus 2 over-claims —
T+0x8e08 sheet and T+0xc48 block are marked faithful but their own cells say
"row-level: sliders/toggles/selector faithful as port, roles/coords/takers pending" and
"formulas faithful, S storage GF-side" — under the STRICT rule used for the 22→3 audit
these are partial ports, i.e. stand-ins with faithful sub-terms, not faithful rows) /
stand-in 40 / absent 37. Sizes S 64 / M 15 / L 3 — confirmed. Live Y 17 / N 65 — confirmed.
Corrected: 82 rows, faithful 3 / stand-in 42 / absent 37.

MatchBall: 48 field rows + E1x/E6x resolved notes (not rows) + §2 prediction table (6 fn rows,
5 unique: 57800/57720/6c6d0/6cf70/6c990+458740) + §4 tick order (no rows).
Status over 48: faithful 3 (D2, F1, F2) / stand-in 23 / absent 22 — file claims
"faithful 3, stand-in 26, absent 22" over 54 rows, but 54 double-counts (48 field rows +
E1x/E6x notes + 4 of the §2 fn rows already covered as F-rows). Sizes S 28 / M 15 / L 4 /
— 1 (D2, no size) — file claims S 30 / M 17 / L 7, which folds in §2/H rows; field-row
recompute is S 28 / M 15 / L 4. Live Y 24 / N 22 / done 1 (C1) / open 1 (G5, cell truncated
mid-row: no size/live cells). Corrected: 48 rows, faithful 3 / stand-in 23 / absent 22.

## 2. Faithful rows (all 6 re-checked against raw decompile)

- TP T+0x254 — CONFIRMED. Setter `FUN_140a42570`: `*(p+0x254)=param_2` (raw bridge text).
  GF `w.s[t] = -static_cast<float>(tm->GetSide())` (teamAIcontroller.cpp:2267) is the same
  definitional value by fixture side. Strict-faithful holds.
- TP S+0x7b10 line bands — CONFIRMED. `FUN_1405fbcb0` writes `(bVar2*0x3c+0x7b38+param_2)`
  (rear/front own-frame bands) via the 5f9eb0/5fb6f0/5fa990/5fb2d0 chain (raw decompile).
  GF `TargetLineCushionFactor` (1405f7a00), `DefensiveLineStepUp` (1405ef910),
  `LineHalfWidth_m`, `TeamLength_m` (pesdefshape.cpp:119-220) are literal arithmetic ports.
- TP PM base — CONFIRMED (structural faithful, as labelled: GF Player/Humanoid root is the
  single authoritative per-player kinematic object; no rule computation claimed).
- TP T+0x8e08 sheet — CORRECTED → stand-in (partial port). The row's own cell admits
  roles/coords/takers pending; strict rule = stand-in with faithful sub-terms.
- TP T+0xc48 block — CORRECTED → stand-in (partial port). Own cell: "formulas faithful,
  S storage GF-side"; GF never allocates PES's S block.
- MB D2 — CONFIRMED (faithful views: `Position()` pesoffball.cpp:1475 is a GF-role→PES-id
  map, `slotPoint` is GF-adapted formation point teamAIcontroller.cpp:2298 — arithmetic
  views with no PES writer, exactly as the row says "no writer (views)").
- MB F1 — CONFIRMED. `FUN_140a57720` = `return param_1 + 0x2948` (velocity pointer);
  GF `Ball::GetMovement()` returns momentum m/s (ball.cpp:45). Live xyz at BallInfo+0x88
  feeds `positionBuffer`/`Predict(0)` (ball.cpp). Same rule.
- MB F2 — CONFIRMED (same evidence as F1; +0x2948 velocity m/s).
- MB F3 (`57800`→`Predict`) stays stand-in — CONFIRMED as stand-in: `FUN_140a57800` =
  `FUN_140a6cf70(param_1,0)` (slot-0 sample), GF predicts with ported flight but never
  compares the MA mirror (divergence unmeasured). Row is honest.

## 3. Writer VAs (spot re-checks; all CONFIRMED unless noted)

- T+0x255 setter `FUN_140a428d0`: `*(p1+0x255)=arg!=0` — CONFIRMED (bridge).
  Rule `FUN_1404fe9c0` (4-step latch, reads `+0x24840` list, `+0x116c`, ring `3e190`)
  — CONFIRMED present as described.
- T+0xa508/brain+0x6cf0 `FUN_140581980`: sets `+0x6cf0=0x30`, counts down, writes
  `+0xa508=2` while nonzero else 0/1 — CONFIRMED (raw decompile; matches counter-trigger §1).
- T+0x6a4/T+0xb860 three writers: `502190` zero path (`[b860]` + `[b860-0xb1bc]`=[6a4] ×11
  when `+0x291c==3`), `4fdd20` delay stores (`+0x6a4`/`+0xb860` pairs), `5027a0` primary loop
  (`+0x6a4+…=10` floor) — all CONFIRMED in raw text. S1 re-run: 0x6a4 38 hits, 0xb860 5 hits
  (0x1404fc485/0x1404fe30d/0x1404fe8b9/0x1405026fa/0x140503382) — matches the row.
- T+0xa2a0 writer `FUN_140a42940`: `*(p+0xa2a0+id*4)` + ring push `40200(p+0xa2cc+id*0x34)`
  + `28c` write — CONFIRMED. Ring `40200` is a 4-deep `{state,stamp,stamp}` ring
  (CONFIRMED body). T+0xa2cc "writer open" is therefore PARTIALLY CORRECTED: the writer is
  `40200` via `42940`; only the per-slot attribution (statesites.py no-imm tuned, O6) is open.
- T+0xb8a4/b8a2/b8a3/b8c0 `FUN_1404f8350` + `0x1404f6e4f/0x1404f6e58` + `0x1404f870c`
  (stamp `+0xb8c0 = MI+0x1198` on change, two sites CONFIRMED in raw text) — CONFIRMED.
- T+0xa53c/a540 `0x1405361ee/0x1405361e7` (same function `536170`: clear/set + stamp)
  — CONFIRMED. S1 re-run matches (a53c 5, a540 4 hits).
- T+0xa548 press region `0x140537411/0x14053784c`, `536fc0` writes `+0xa548` slots
  (CONFIRMED) but reads NO `0x24710`/`0x247be` table address (grep: 0 hits in `536fc0`
  body) — CONFIRMED as "writer region, table unwired".
- T+0xb718 `0x140579b4d` (`*(u16+0xb718+param_3)=...` inside selector 579840) — CONFIRMED.
- T+0xb748 S1 128 movimm32 hits starting `0x14053e825` — CONFIRMED by re-run (128).
- T+0xb7d8..b824 clearer `57a3e0` (`+0xb7d8=0xff…`, `+0xb824=3`) — CONFIRMED in raw text.
- T+0xb824 S1 5 hits (`0x140553e69/0x14055d99a/0x14056f423/0x14057a363/0x14057a53e`)
  — CONFIRMED by re-run. T+0xb834 blend stores `0x140624235/0x1406259e0`
  (`+0xb834+id*4`, rate passes in register — value never re-loaded) — CONFIRMED.
- T+0xb6e8 relay pair — CORRECTED: the cited `0x14057acec=0/0x14057ad05=1` VAs sit in
  `FUN_14057ac20`, which writes `+0xb6df/+0xb6e0` (enable row), NOT `+0xb6e8` (concept
  flags). The b6e8 writer VAs are UNVERIFIED (copy `0x1404171bf`, init, serialiser only
  from S1-relay). Downgrade T+0xb6e8 writer cell to "writers open; S1-relayed copy/init/
  serialiser VAs only".
- T+0xa298 setter `FUN_140a424d0` (`*(p+0xa298)=param_2`) — CONFIRMED (bridge). Meaning open
  stays Y.
- T+0x558 setter `0x140a428c4` (`*(p+0x558+id*4)`) — CONFIRMED (bridge text).
- T+0xc12a: `FUN_140533d10` is a READER (`*(lVar13+0xc12a)` once, no store) — CORRECTED:
  row lists it under "writer"; move to readers, writer stays open (Y unchanged).
- image+0x32B0: `0x140458c98/0x140459266` hit 12× for `0x32b0` in the `458b20` body but all
  are READS/lea in a getter (`float * FUN_140458b20`), no store in the fetched body —
  CORRECTED: writer VAs are candidate sites (S1 hits), not verified stores; keep Y (static
  attribution still open).
- ai+0x134 `FUN_140a596f0` (`*(p+0x134)=param_2`) — CONFIRMED as a store; base (ai vs T)
  unverified (caller chain open) — row already says that. Reset `58930`/mirror `431800`
  not re-fetched (bridge budget); cited as relayed, low risk.
- ai+0x1474/148c publisher `92d580` (`+0x1474=0`, `+0x148c=1`, `+0x12a0=±1.0`) — CONFIRMED.
- E5 MI+0x11a0 triple init (`8ae72b=10`, `100ce84=EAX`, `118221c=EDI`) — CONFIRMED via
  mb-writers evidence (re-read from scratch file; base RBX unverified as the row states).
- E5b MA+0x2601c clear `71c30`, E5c setter `72a60` (`if (*(p+0x2483c)!=p2) *(p+0x2483c)=p2`),
  E5d `6f240` (`*(p+0x2482c)=0; *(p+0x24834)=0`) — all CONFIRMED (raw bridge text).
- A1 writer `3d060` (6-word record writer, called at 56 sites) + router `3dc40` (ring map
  `@0x142642e60`, `iVar1==7` arm copies `rec+5→E+0x180698`; ring-1 `3dac0(local*0x1908+p)`)
  — CONFIRMED. A7 busy `6ce10` (kind 5/6-7/9-14 logic) — CONFIRMED raw text; row's
  "no writer (pure)" is accurate.
- E1x/E6x conflict resolutions — CONFIRMED: `115a0` writes `+0x116c/1168/728` on param_2
  (statistics accumulator, same-object `+0x710-0x724` counters adjacent), not on MI; `60320`
  is a 2-word key comparator (CONFIRMED body), consistent with the +0x18/+0x1c key reading.
- H2 tick order: all 10 steps present in `42b9f0` (re-verified call list); `4267f0` =
  detector loop `a0d3f0` (CONFIRMED); `4f6c30` calls `4f8350/4fe9c0/502190/525530/51d9e0/
  51e910` (CONFIRMED) but its own caller is still open (bridge: "No function found at
  0x14041edd7" — the PoseVeto correction stands: 41edd7 is a mid-function tail of
  `FUN_14041ecf0`). H3 row's "via FUN_14041edd7" should read "via 0x14041edd7 (tail of
  FUN_14041ecf0)" — CORRECTED label, substance unchanged.
- F4 floor `6c6d0` (state-5 `< minSpeed` → facing-projected 0 via `8f7c0`) vs F6 mover
  `6c990` (state-5 slow → hard zero) — CONFIRMED distinct functions; rows correctly split
  stand-in (F4, no floor in GF) vs ported-where-used (F6). `458740` = bearing of 6c6d0
  output — CONFIRMED.
- G1-G5 classifier `72e70` + ctor `0x140a85f40` not re-fetched (Denuvo-VM, static-only);
  GF constants verified at HEAD (`gametypes.hpp`: pitchHalfW 52.5, goalInnerHalfWidth 3.66,
  goalAreaHalfWidth 9.16, goalDepth 2.55) — stand-in deltas as stated. G5/G6 no-consumer
  claim not re-audited (whole-exe negative; accepted as relayed, low scheduling weight).

## 4. Writer-not-found rows (one new avenue each, result recorded)

- G+0x25cf8/0x25d38: SIB-mod0 scan 0 hits both; setter family enumerated (all sheet/scalar
  setters, no G-table store); raw-imm 9 refs each all attributed readers (re-run matches).
  Result: writer STILL NOT FOUND. Remaining avenues (computed-offset stores in the family,
  rep-movs): 83 rep-movs sites listed, none near MA/T/ring code — bulk-copy writer unlikely
  but not excluded. LivePoke instr-change log is the finder (O7 stands). REFUTED nothing;
  row CONFIRMED as open, Y stands.
- MA+0x24840/B5, MA+0x22e4c, MA+0x247d4, MA+0x24710: SIB-mod0 0 hits all four.
  Result: STILL NOT FOUND statically; live reads queued (LivePoke). Rows CONFIRMED open.
- T+0x4c00/0x4bd4/0x4b4c/0x4b40/0x875c/0x8770/0x89a0/0x89a4/0x8898/0x87d0/0x87cc/0xa2cc:
  SIB-mod0 0 hits all; S1 re-run confirms 0 hits for 4c00/4bd4/4b4c/4b40/875c/8770/89a0/89a4/
  87d0/87cc/a2cc (computed-base/loop-copy writers, as rows state). Result: avenue exhausted,
  Y/N unchanged. (Note: `687820` is a READER of bands, not the 89a0 writer — row already
  lists writer `687820` OBS; the decompile shows it reads `+0x7b88` and calls `48a10` with
  NO store to 89a0 — CORRECTED: 687820 is the band consumer, the 89a0 store site is open.)
- PI+0x13ac/13c4/14a8: SIB-mod0 0 hits; scan open as rows state. LivePoke Chance-end log
  covers 13ac/13c4/14a8 already (watch2.py rec_pa reads them) — Y can ride the existing log.
- T+0x78 (233 hits), T+0x210 (653), T+0x134 (296), T+0x26c (96), T+0x28c (91), T+0xf1 (40),
  T+0x250 (1159), T+0x254 (107), rec+0xdc (434)/d4 (445)/d8 (1947): re-run counts match the
  rows' "unattributable/common" labels — CONFIRMED as stated, no new avenue (base anchoring
  needs live MA/T, queued V1/V2).

## 5. Stand-in rows (GF symbol + ponytail check at HEAD `7a15efe`)

All cited GF symbols exist except cosmetic line-pointer drift (tables cite `1ff9c3b` lines;
code md5-identical, lines shifted by ≤3 after the dodge-hold commit — no symbol moved):
`w.s[t]` (2267), `GetLastTouchTeamID` (2185), `RunWorld.possession/receiver` (pesoffball.hpp
207/203 — cited as `RunWorld::` but the struct is `ob::RunWorld`; CORRECTED qualifier),
`BallEvent.target` (pesoffball.hpp:196 — `ob::BallEvent`, while the 2-deep history is
`Match::ballEvents` match.hpp:487; row conflates the two — CORRECTED: cite both),
`SideCounterInput::attackAllowed` (pesteamconcepts.cpp:596 — cited as input field, actually
the computed local in `SideCounterStart`; CORRECTED name), `StrategyAttackLevel`,
`AttackLevel(mentalityRung)`, `ConceptOrder` (1663), `PlayerFacts::state/.t` (pesoffball.hpp
170-180; `run->second.state` teamAIcontroller.cpp:2158), `ProcessDesignatedSupporter`,
`ShapeFieldFromPitch` (cited `ShapeField` — CORRECTED name), `TargetLineCushionFactor`/
`DefensiveLineStepUp`/`LineHalfWidth_m`/`TeamLength_m`, `MeasuredLine`,
`GetAdaptedFormationPosition`, `Position(gfRole)` (pesoffball.hpp:214 — GF-role map, as row
states "mapping, not PES value"), `w.p[target].marker` (2319), `pressSlotMan/notInA558/
presserFlag` (pespress.hpp/cpp), `rosterSlotValue` (pesdodge.hpp:281), `f.facing=pesDeg(
GetDirectionVec())` (2291/2258), `Ball::Predict/GetPredictionArray/positionBuffer/
GetMovement`, `Match::ballEvents[2]/PushBallEvent/BallEventKindOf` (team.cpp:366),
`PressCandidateFill`, `GetActivePlayers`, `EnforceMaximumDeviation`
(mentalimage.cpp:51), `AI_GetOffsideLine`, `IsInPlay/IsInSetPiece`, `Match::Process`
(match.cpp:664 — order: collisions→referee→ball→mental→teams→officials→possession/
designated/stalemate→contacts→clock/actualTime→goal-line→goals→…→UpdateBodies CONFIRMED,
matches the row's delta list), `possessionChange_ms`, `matchTime_ms`, pitch/goal constants.
`ponytail:` lines: all cited ones present (teamAIcontroller.cpp:384/436/448/1750/1758/1813/
1919/1995/2088/2183/2188/2195/2203/2294/2307, pespress.hpp:98, pesdodge.hpp:281 context);
three citations need repointing: "ponytail: 2" (no such line — the b8a4 note is
teamAIcontroller.cpp:384, "has no GF counterpart"), "ponytail: slot line ≈ role line"
(lives at :448 as "slot line T+0x7b88 is GF's role line"), "ponytail never-fires" (the
never-fires note is referee.cpp:648, unrelated — the 6a4 row means pesdodge.hpp:281's
plain `= 0`; drop the "never-fires" tag), "ponytail-false terms/assumed" (no such tags —
the G-table row means GF's false-valued terms; reword).
Substance verdict: every stand-in row says what GF actually substitutes — CONFIRMED;
8 citation fixes above, no status changes.

## 6. OBS/INF honesty

OBS claims re-checked are all grounded (bridge text or re-run scan counts). INF labels are
honest where checked (G-table slot layout, 24840 cell semantics, 247d4 heading use, H1 rating,
E6 stamps, 26c/27c concept points). Two upgrades: T+0xa2cc writer (40200 via 42940) and
T+0xb6e8→b6df/b6e0 correction (§3) move one cell each from open→known. No invented finding
encountered; the "0 violations / 7298 snaps / 892 snaps / 267 snaps" live figures are relayed,
not re-verified (live round will confirm or supersede).

## 7. Corrected counts

- TeamPlayer: 82 rows — faithful 3 / stand-in 42 / absent 37; S 64 / M 15 / L 3; Y 17 / N 65.
- MatchBall: 48 rows — faithful 3 / stand-in 23 / absent 22; S 28 / M 15 / L 4 (+1 sizeless
  D2); Y 24 / N 22 / done 1 / open 1. (File's "54 rows / S30 M17 L7" folds in E1x/E6x notes
  and §2 fn rows; field-row recompute above.)
- Widen (not in scope, noticed while avoiding duplication): 36 rows cited, no overlap with
  the corrected rows above (W-A6 cites C2 without duplicating; W-PRESSX extends T-PRESS with
  new field a55c — no double-count).

## 8. Open / waiting on live

V1 (T+0x78), V2 (T+0x210), V3 (PI+0x48), P3 (T+0x255 flips + 6a4) with LiveTeamPlayer-2;
V6 (MI+0x11b4), P1 (MI+0x119c/11a0 + kind-27), P2 (vtable 0x142597580 holder) with
LiveMatchBall-2; MA+0x24840/B3, MA+0x22e4c, instr tables with LivePoke. Second half running;
ClockFreeze may pin clock/phase fields — readers tag held samples.

## 9. Late live updates (arrived while writing; no values yet, methods CONFIRMED)

- LiveTeamPlayer-2 half-time probe: G tables read ZERO during the break (team/player
  pointers null), so T+0x255/6a4/78/210 are unreadable mid-break. Consequence: the
  half-time reset question becomes last-valid-tick vs first-valid-tick (run2/tick.jsonl
  + run2/events.jsonl). V1/V2/V3/P3 armed in run2. No verdict change; Y rows stay open.
- LiveMatchBall-2: ONE full rw-memory scan for the P2 vtable-holder question (capped,
  streamed, ~1 min). No other scans from that reader — fleet scan budget respected.
- ClockFreeze WRITE START: pinning MI+0x1168 = 3 at 20 Hz (MI re-resolved per tick;
  exact field list: MI+0x1168 ONLY, not 1198/119c/11a0). Pre-write snapshot: 1168=3,
  116c=2, 1574=3, 11a8=1080, 1198=46264 flat. Impact on this report: NONE of the verified
  rows read MI+0x1168 except E14 (minute/period consumers, stand-in clock — GF's own tick
  clock, unaffected by pinning PES's copy). Readers tag held samples; E14 needs no re-check.

## 10. Live results (new match, LiveMatchBall-2; MA dead this match, G/MI fine, ~52-59 fps)

- V6 (E7 MI+0x11b4): u32 = 0x100 constant all match + one 22-frame excursion to
  0x01000100 (frames 13655-13677, inside ClockFreeze hold window). Low byte 0x00
  throughout → the row's "==0 detector enable" byte-gate reads ENABLED all match;
  writer still unknown, no status change. E7 CONFIRMED as open, Y stands.
- P1 (PoseVeto item 1): MI+0x119c = 0 all match (never nonzero); MI+0x11a0 advanced
  5730 steps, each equal to the frame delta → E5's open per-tick `+=1` increment writer
  is now CONFIRMED LIVE (frame-locked counter). E5 stays stand-in (GF computes its own
  possession age, not the PES rule); writer cell upgraded to confirmed. Kind-27 ring
  check NOT reported — still open.
- P2 (PoseVeto item 2): holder H=0x1c6403a0 reached from global 0x1436f3f98 (no scan),
  vtable verified; owning object unresolved. H3 caller hunt still open; no row change.

## 11. Live results (new match, LiveTeamPlayer-2 verdicts, pes-re-live-teamplayer.md rev 2)

- V1 (T+0x78): writer EXISTS (measured, not kick-gated — no kick captured, AnimePlayer reads
  0; "kick-gated" retracted): 255 in 5,870 readings, a player id in 1,948, always the same
  team (0 cross-team). Meaning (fallback/last-touch id) CONFIRMED; writer VA still open.
  Row stays stand-in; writer cell upgraded to "writer confirmed live, VA + kick link open".
- V2 (T+0x210): = 2 with no change across ~3,900 consecutive valid pairs (no-in-match-writer
  CONFIRMED); "init 2 at kickoff" NOT observed (retracted). Row stays stand-in; Y STAYS OPEN
  (reason unchanged: the menu feeder is not located).
- V3 (PI+0x48): 806 readable zeros at 47 Chance ends (keeper 0 readable at 46, keeper 11 at
  31); 0x18 NEVER seen. Row stays absent, Y stands.
- P3 (PoseVeto item 3): flips run9/run10/run11/run12 = 5/5/10/2 (valid-sample series, holder
  UNVERIFIED); 6a4 drops equal frame count on 13 clean nonzero windows (≤12 frames),
  0 violations ("exact 1/frame" retracted to window-equality); re-arm-to-event link
  RETRACTED (1 rise pair, no event). NEW: the countdown is nonzero only when MI+0x116c == 1
  (458/2,416 valid ticks) and zero at 116c 2 or 3 (0/1,493, incl. 355 ball-moving ticks) —
  the gate is the 116c == 1 gate, not ball motion, not "in-play" (116c==1 coincides with
  ball motion on 2,415/2,416 ticks, but 260 + 95 moving ticks have 116c 2 and 3). Holder
  correlation still open (MA dead).

## 12. Live results (LiveMatchBall-2 run 5, 912 s in play)

- V6 (E7 MI+0x11b4): low byte alternates 0x00/0x01 (119 changes; values
  0x100/0x101/0x10100/0x1000100/0x1010100; never 0 as u32) — gate UNVERIFIED. Low byte
  0x01 held only 7 s; ring-10 push rate 1.86/s then vs 5.66/s at 0x00 = weak rate-change
  evidence, not a gate. Y stands.
- P1: MI+0x11a0 steps equal frame delta in 11,881/11,980 (99 unequal = jitter);
  MI+0x119c not logged run 5 (0 in match 2). Kind 27 OBSERVED: 11 in-play pushes (ring 10),
  all slot +0x30 = 24 (no player); count/sum E+0x181e00/+0x181dfc stay zero because the T1
  gate (+0x30 < 24) rejects every push. P1 closed.
- P2: M = *(0x1436F3F78) → X = *(M+0x790) → D = *(X+0x18) → E = *(D+0xbd08)
  = 0x7ffffae70018, live, vtable 0x142597580 VERIFIED. H = *(0x1436F3F98) is M+0x788
  (the holder), NOT on the path to E — earlier fold's chain corrected. Recorded for
  PoseVeto; H3 owner semantics still open, no row change.
