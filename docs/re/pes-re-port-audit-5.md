# PES re-port audit 5: the older landed ports (PortAudit5RE, 24-10-26)

Read-only audit of six older landed ports on `origin/main` (`d6a26b8`), checked against PES's bytes. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at a time. GF code read from `origin/main` into scratch. No repo edits. Reference docs all on `origin/main` unless named. Verdicts: CONFIRMED = bytes/disassembly read this pass; INF = inference; OPEN = blocker. GF paths are `src/sim/onthepitch/…` / `src/pes/…` / `src/sim/…` on main.

## Verdicts per port

| Port | Verdict |
|---|---|
| Forfeit + minimum-players (`pesforfeit.*`, `match.cpp`) | **PASS with 2 findings** (F1 second writer unported — bounded; F2 review-mark staleness) |
| TeamT1 trap/counters + TeamT2 slices A/B (`pesreaction*`, ComputeBlock9) | **PASS with 1 advisory** (A1 the four corrections are already in the code) |
| Walk speed (`elizacontroller.cpp:165-192`) + RootStart (`humanoid.cpp`/`elizacontroller.cpp`) | **PASS with 1 finding** (F3 walker's second branch still `FloorVelocity`) |
| Holder latch (batch D, `humanoid.cpp:1344-1350`) + keeper held standby (batch B wiring) | **PASS with 2 findings** (F4 latch superseded by HolderSplit but still live; F5 keeper held = idle stand, correctly ponytail'd) |
| Mentality first rules (`pesmentality.*`, wiring) | **PASS with 2 findings** (F6 FoldCount off-by-one vs live doc — inconsequential; F7 level/override parity sub-path unported) |
| Zone judge + PlayerDB (batch D) | **PASS, no findings** (both check out; coverage notes below) |

## Findings

### F1 — The second forfeit writer `FUN_140A23380` is not ported (bounded, OPEN by doc)

- GF ports only the period-10 writer: `LoadForfeitConstants` reads three immediates — `0x140A23279 cmp [r12+0x1168],0xa` ✔ (`41 83 bc 24 68 11 00 00 0a`, verified this pass), `0x140A2330E cmp [rax+8],4` ✔ (`83 78 08 04…`), `0x140A23329 mov [rax+0x1b74],3` ✔ (`c7 80 74 1b 00 00 03 00 00 00`), `0x140447EF8 mov edx,0xa` ✔ (`ba 0a 00 00 00`) — and `match.cpp:639-648` (`ApplyPostMatchForfeits`) replays the per-player test `card == sentOff && injury == 0` over on-pitch players in period 10 only.
- PES has a **second writer** `FUN_140A23380` (case 0 of `FUN_140A22130`): disassembled this pass. It runs at **any period** (no `0x1168` gate): per side, per slot `0x00..0x0a`, when on-pitch (`[rdi+rbp+0x1748]`) or at `MI+0x175e`, it resolves the player record, requires `player+0x14 == 1` (`0x140A23611 cmp [rax+0x14],1`), then stores flag `+0x1b70 = 1` (`0x140A2361a c6…01`) and score `+0x1b74 = player+0x1c` (`0x140A23626 mov ecx,[rax+0x1c]` / `0x140A23629 mov [rbx+0x1b74],ecx`).
- What differs: GF's 3-0 fires only when a sent-off-state man is still on the pitch at full time **and** the match reached period 10. PES's second route writes the forfeit at any period with the score taken from the player's own `+0x1c` word — so a `+0x1c == 3` there is 3-0 without period 10 and without the `+0x0c == 4` test.
- Effect: **bounded, currently unreachable.** GF has no writer of `player+0x1c`-equivalent and no producer of the `0x14 == 1` state, so the second route has no GF input to read; and the missing-systems doc (§2.7 note) leaves open which writer fired in the owner's runs. No live GF match can name a case where the second writer changes the result.
- Fix: none now. If a `player+0x1c` producer is decoded, add it as a second `IsForfeitOffence` arm behind its own install-checked immediate. Record as OPEN in the forfeit header's doc pointer.

### F2 — `AYellowThenRed…` and `ARedCardRemoves…` encode a GF-only removal premise (stale, acknowledged)

- `tests/sim/pes_forfeit_test.cpp`: `ARedCardRemovesTheOffenderAndTheSideIsNotForfeited` pins "GF removes the sent-off man, so no forfeit" and names the gap explicitly: "PES keeps a sent-off player on the pitch when no replacement is queued (`FUN_140523110`, `FUN_140523210`); GF does not port that path yet (OPEN)".
- What differs: in PES a sent-off man with no replacement stays on the pitch (still counted by `FUN_140a3bfc0` / still visible to `FUN_140A23260`'s scan), so the minimum-players count and the forfeit scan both see him. In GF `SendOff` removes him, so both see 10, not 11-with-a-sent-off-man.
- Effect: the minimum-players end (`ASideShortOfTheMinimumEndsTheMatch…`, five removals → 6 on pitch) is reachable in GF but rests on GF's removal semantics, not PES's keep-on-pitch semantics; the "red card at full time" case is reachable only via the `kNeverSentOff_ms` trick, never via live cards.
- Fix: port the keep path (`FUN_140523110`/`FUN_140523210`) when its reader is decoded; the test already names the swap. No action now — correctly marked.

### F3 — The walker's second approach branch still uses `FloorVelocity(2×gap)` (real gap, bounded)

- GF `elizacontroller.cpp:165-192` (main): the set-piece walk branch correctly applies `walkDown_ms` (11 km/h from `DAT_14259BDFc`, verified `00 00 30 41` = 11.0 this pass; call site `0x1406b77d3 mov ecx,r15d` / `0x1406b77d6 call 0x140a76070` / `0x1406b77db movss [rsp+0x40],xmm0` all verified) — **except** the `IsCarryingBall()` arm, which keeps `FloorVelocity(pes::kPieceTakerApproachGain * gap)`. Same `FloorVelocity(approach)` at `:520` for the FK/corner/penalty run-to-ball.
- PES: the walk-down sits in the general filler `FUN_1406B7560`, reached by every MOVE handler; the doc (§3 gate) names the gate inputs as undecoded and the port deliberately applies the fixed rate to the whole walk. A carrying walker's class is run/sprint at distance, the exact case the walk-down caps — PES would also hand him 11 km/h.
- Effect: bounded. A carrier far from his spot approaches at sprint (up to 8 m/s) instead of 3.0556 m/s, arriving up to ~2.5× sooner; once within ~2.5 m both agree. The FK/corner/penalty run (`:520`) is behind a ponytail naming the AI emitter's `cmd+0x5c8` write as undecoded, so it is marked, not wrong.
- Fix: apply `walkDown_ms` to the carrier arm too (one-line: drop the ternary, keep the ponytail for the gate). Leave `:520` until the emitter's `cmd+0x5c8` writer is decoded.

### F4 — The batch-D holder latch is live but superseded by HolderSplit (advisory, no change)

- `humanoid.cpp:1344-1350` (main): `if (PesLoop() && !GetBallRetainer() && GetLastTouchPlayer() == me && AI_HasPossession(…)) SetBallRetainer(me)` — the batch-D patch (`e06e481`): last toucher holds while live.
- The L2 `HolderSplit` (`match.hpp:226-256`) now carries PES's actual holder separately: `RecomputePesHolder` behind the `pesRestartStep` switch (`0x08/0x0B/0x0E/0x12` arm, `0x14/0x15/0x16` run-if-latched, default skip — the switch itself exact, the step-value mapping behind a ponytail), adopting `pesKeyActor` (the ring-1 touch key, set at the touch write + `SetPesKeyActor` on keeper catch) and never clearing except W2's object reset. The split tests (`pes_holder_latch_test.cpp`: two-tick-gap arm + never-cleared-in-play) are green.
- What differs: two holders coexist. `GetPesHolder()` is PES's (persists on loose balls, per `pes-re-press-coord.md` §5); the humanoid latch still rewrites GF's `ballRetainer` on top of it every tick the last toucher has possession. Where they disagree (loose ball settled at a non-toucher's feet while the key is elsewhere) the latch can re-retain a man PES does not hold.
- Effect: bounded and currently load-bearing — the far-loose-ball test and the ceremony throw-in both pass through the latch's trajectory; removing it without re-pointing the loose-ball predicate (`ballRetainer == 0` in `match.cpp:891,909,918`, the press reset, keeper catch, far-loose detector) would regress them.
- Fix (advisory only): when a consumer reads `GetPesHolder()` for the latched cases, delete the humanoid latch in the same change (clean cutover, no dual-holder window). Not now — the latch's tests pin its ticks.

### F5 — The keeper's held standby is GF's idle stand, not a standby clip (correctly marked, no fix)

- `pes-re-kicker-standby.md` §§1-2,4 (CONFIRMED): every standby but the two kick-off loops returns 1 from slot 10 ("skip the default locomotion"); the taker is PLACED by SetKick and held in one idle clip; the kick action's clip + root adjusters move him at release. Keeper: `GoalKickStandby` Update `0x1407748F0`, skip `0x140775730 → 1`, SetKick `0x140774AE0` (clip `0x6B1`, yaw 0.0).
- GF (`teamAIcontroller.cpp:2060-2071`, `:1239-1254`): the keeper is placed on `PrepareGoalKickStand` (SetKick port) and held via `pieceHeld` + `ResetPosition`; the non-PES-path walk in `elizacontroller.cpp` is gone on the PES path. The hold body itself is GF's idle stand (`elizacontroller.cpp:210-220`, `IsPieceTakerHeld`), behind the exact ponytail: "PES holds the body in the standby clip with the default locomotion skipped; GF holds it with this idle stand until the standby clip family lands (kicker-standby §3)".
- What differs: a held taker plays GF's idle instead of PES's standby clip (`goalkick_idle` 1709 / `fk_idle` / `throwin_loop` phases). No locomotion, no drift — positionally exact, visually a stand-in.
- Effect: none on sim state; animation-only until the standby clip family lands. Correct as-is.

### F6 — `FoldCount(0)` contradicts the live doc's "one fold at neutral" (inconsequential, fix the comment or the rung)

- `pesmentality.cpp:150`: `FoldCount(dial) = dial < 2 ? 2 - dial : 0` — neutral (b8a4 = 2) gives **0** folds, −1 gives 1, −2 gives 2.
- `pes-re-mentality-reclass-live.md` §2 (cited at the function's own comment) reads "one fold at dial −1, two at −2, none at neutral" — consistent with the code. But the first audit's F5 already noted the port doc's table vs the function, and `pes-re-mentality-blocks.md` §3.1 as corrected states the depth-push gate at `0x14067D285` (`DAT_142565604` = 2.0, verified `00 00 00 40`… actually 2.0f this pass) governs the fold pass, with the `b8a2` full-attack skip at `0x14067CDFB`.
- The live `T+0x110` writer is still undecoded (blocks §7 OPEN), and `foldSlots` still has **no reader** (MentalityPort §"Open": "written and never read").
- Effect: **none today** — the function feeds a dead store. When the writer lands, neutral-dial behaviour (0 vs 1 fold) moves the block-B test.
- Fix: keep the code; change the header comment to cite blocks §3.1/§7 (the writer OPEN), not the live doc's §2 paraphrase, so the next decoder checks `T+0x110`'s true neutral value.

### F7 — The parity sub-path (F inverted on even frames in play) is not ported (known, marked)

- `pes-re-mentality-blocks.md` §1 (byte-verified): once `MI+0x119C > 1` in play, `FUN_1404F9DC0` alternates the block flag per frame — odd `MI+0x1198`: `F = (T+0x255 != 0)`; even: `F = (T+0x255 == 0)`.
- GF: `WriteRoleArrays`/`SetUpRoleArrays` (`teamAIcontroller.cpp:3208-3235`) take `RoleLevel(possession latch, …)` and `BlockFor(IsPesPossession())` from the latch only, every tick. The `T+0x255` latch vs EC600-arg4 `F` distinction the F1 doc's Open-3 names ("match except the parity sub-path") is correctly observed, and the MentalityPort doc lists it OPEN — but `pesteamtick`/header carry no `// ponytail:` naming `9DC0`/`MI+0x119C` at the flag site.
- Effect: in-play, GF holds one block per possession phase; PES alternates per frame once the kick sum exceeds 1. Downstream: block-A forward pull (unported) vs block-B fold pick alternate in PES; GF never alternates. Bounded by both consumers being half-built (fold has no reader; block A not ported).
- Fix: add the ponytail at `BlockFor`'s call sites (`teamAIcontroller.cpp:518, 3208`) citing `4F6C30 l.193-198` / `9DC0` and `MI+0x119C` (no GF counterpart, `team.cpp:500`); port the alternation with the block-A pull (F2–F4).

## Coverage: what checks out

1. **Forfeit immediates + opcodes** (all re-read this pass): `0x140A23279` `41 83 bc 24 68 11 00 00 0a` (period 10) ✔; `0x140A23308` `83 78 14 00` (injury marker `+0x14 == 0`) ✔ — note the header comment says `player+0x18`; the instruction reads `[rax+0x14]`. The doc's `+0x18` is the record-relative name vs the resolved pointer's `+0x14`; flag as a comment nit, not a logic error (the load path is shared with `0x140A2330E`'s `[rax+8]` for `+0x0c`). `0x140A2330E` `83 78 08 04` (sent-off 4) ✔; `0x140A23322` `c6 80 70 1b 00 00 01` (forfeit flag) ✔; `0x140A23329` `c7 80 74 1b 00 00 03…` (score 3) ✔; `0x140447EF8` `ba 0a 00 00 00` (post-match 10) ✔; `0x140447E79` `3a 42 17` (`cmp al,[rdx+0x17]`, threshold byte) ✔; `kPesMinimumPlayers = 7` correctly a named stand-in with its writer OPEN (live-read 7).
2. **Forfeit count semantics**: `BelowMinimumPlayers` is `<` (strict, per `JNC` at `0x140447e7c`) ✔; period-10 agreement check (`postMatch != gatePeriod` fail) ✔; both-sides loop (`match.cpp:640-648` iterates both teams, matches PES's two-side loop `r14d 0..1`) ✔.
3. **TeamT1's four corrections are all in the code**: kinds `{1,2,3,16}` (`kKindStoreFirst=1, Span=2, StoreSixteen=16`; `kKindSixteen` gate at `0x1404fe154`) ✔; ball-record ramp (`f.ballSpeed`, header: "B+0x2940") ✔; jump table re-verified from the image this pass (kinds 1-5,7,9,16→FDD20/p5=0; 10-14→FDD20/p5=1; 6→27A0; 8,15→dead tail; 16 entries + padding) ✔; `SampleHeight` clamp j≥250→249 (`kSampleLastIndex=249`, `0x1404fdd20` prologue path) ✔.
4. **ComputeBlock9 (slice B) spot-checks**: `speedScale 25/36` (`0x14259bdd0`), `speedK1 60`, `speedK2 1000`, `ramp1 15/20/5`, `ramp2 10/22/12`, `forced 3.5/7`, `p5 50 / plain 45`, `block0 0.3/0.9/1.2`, `tail 70/140/-70/0.2`, `dl 0.4`, `p 25/-20` — all named at the header VAs matching treact §constants; variant selector `p5 || R13` (not F), flag80 = class bit 4 / R13 = bit 5 + `+0x38 ∈ {3,4}`, angles in degrees — all per the TeamT2 doc's asm corrections ✔. Unwired status correctly declared (nothing calls it).
5. **Walk-speed port**: `walkDown_ms = 11 km/h` from `DAT_14259BDFc` on the main walk branch; RootStart (lift clip starts from root yaw, taker keeps lift yaw while held — `475e237`, `ddbed91`) intact; carrier-arm exception ponytail'd ✔ (F3).
6. **Mentality first rules**: role-level values (2/1/0, `NEG/SBB/ADD` + CMOVZ override) ✔; reclassify table (jump table `0x1405ED1F4`, arms `0x1405ECB49/53/5D`, default line 0) opcode-checked by the loader ✔; default-arm bytes `4D 8B D6 41 8B FE` ✔; block polarity (F=0→A `681FF0`, F≠0→B `67B640`, `0x1405ECC21/28`) ✔; fold score/penalty/best (`0x14067CE72 cmp r8d,6`, `0x14067CDE3`, `DAT_14259D634`=10000.0 ✔, `DAT_1425C65FC`=100000.0 ✔), protected-slot filter `0x14067CD9D` with last-class-9 stand-in ✔, full-attack skip ✔, "earlier picks excluded" ponytail (same for 2 folds) ✔.
7. **ZoneJudge** (`dbb05ad`): payload word `+0x14` carries the zone code (`SetU32(0x14,…)`), `JudgeActsOnOutOfPlay` tests bit `0x2`, detector gates on it — matches `FUN_140a17810` / `FUN_14050e4a0` case 10 ✔; the `869a43a` citation correction (arm `FUN_1408722c0` writes 3, exits write 1/2, exit frame OPEN) ✔.
8. **PlayerDB**: record `0x138` bytes, id u32 at `+8` ✔ (matchlogic 1.1); injury-resistance bit 458 w2 (id `0x2E`, fn `0x141F0FDD0`) ✔; stronger-foot bit 514 w1 (id `0x0F`, fn `0x141F103A0`) ✔; tier-3 skip (`> kInjuryTierA` continue) matches "code 3 names no tier" ✔; consumed by `InjuryResistanceOf` into `RecordContact` and `LeftFootedOf` ✔ — the two handover items ("injury resistance", "preferred foot") are landed.
9. **Keeper held standby wiring**: goal-kick/corner place-and-hold (`PrepareGoalKickStand`/`PrepareCornerStand`, `pieceHeld`), throw-in path A walk vs path B placed hold — matches kicker-standby §§4/7 ✔.

## Open (carried, not new)

- `player+0x18` vs `[rax+0x14]` naming (F1 nit); `FUN_140A23380`'s `+0x1c` producer; `FUN_140523110/210` keep-on-pitch; `MI+0x119C`/`MI+0x1198`-parity; `T+0x110` writer; fold readers (`+0x7CD4`); 27A0 (9 unbound + 7 named inputs per TeamT2 doc); walk-down gate inputs (`match+0x116c`, `playerRec+0x25930`, `FUN_140479470`); emitter `cmd+0x5c8`; standby clip family; `p+0x298` exit frame; `MA+0x2483C` holder fallback (Q1-2, now HolderSplit's key actor — reconcile).
