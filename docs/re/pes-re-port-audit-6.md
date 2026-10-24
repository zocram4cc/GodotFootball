# PES re-port audit 6: StrategyFix + CondTactics (PortAudit6RE, 24-10-26)

Read-only audit of two branches against PES's bytes. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at a time.
GF code read from the named branches (`git -C <workdrive>/GodotFootball show <ref>:<path>`);
scratch absolute outputs in `<workdrive>/gf-PortAudit6RE-scratch/`
(`tick_dis*.txt`, `tick_dec.txt`, `drift_dec.txt`, `selector_dec.txt`,
`shift_dec.txt`, `decider_dec.txt`).
No repo edits. Budget: ~40 of ~150 requests used.

Reference docs (all on `origin/main` unless named): `docs/re/pes-re-plan-selector.md`,
`docs/re/pes-re-port-audit-2.md` (S1/S2), `docs/re/pes-re-tactics.md` §3,
`docs/re/pes-re-team-layer.md` row 12, `docs/re/pes-re-team-status.md`,
`docs/re/pes-re-role-rebuild.md` §2, `docs/re/pes-re-role-writer.md`,
`docs/re/pes-re-lane-writers.md` §2, `docs/re/pes-re-condition-import.md`,
`docs/re/pes-re-condition-roll.md`, `docs/re/pes-re-condition-bands.md`,
`docs/re/pes-re-tick-steps.md` §25.

Branch SHAs: `origin/StrategyFix` = `c929824`; `origin/CondTactics` = `586fd03`.
The doc/render deletions in both diffs (`pes-re-port-audit-5.md`, Lacc/SkyAmbient/PortAudit4
render docs) come from the `52020fb`/`586fd03` batch-L2 merges, not from either branch's own
commits — out of scope, not findings.

Byte anchors re-verified this pass (raw image reads): tick `0x1404F6DB1 LEA RCX,[RDI+0xBC0]`
/ `0x1404F6DBD MOV RCX,[RDI+0x6D50]` / `0x1404F6DC4 MOV EAX,[RDI+0xBCC]` /
`0x1404F6DCA MOV [RCX+0x134],EAX`; selector `0x140501076 MOV EAX,[RBX+0xBCC]` /
`0x14050107C TEST;JS` / `0x140500FA9 CMP EAX,2` / `0x140500FB2 MOV EDX,2`;
flags `0x1404F6E1C C6 80 A2 B8 00 00 00` (`b8a2=0`), `CMP [B8A4],4 / CMOVL`,
`0x1404F6E30`-region `b8a3` (`CMP [B8A4],0 / CMOVG`); entry `0x14051E92E DEC EAX;
TEST EAX,0xFFFFFFFD; JNZ` (states 1,3); reset `0x1408ADEA5/0x1408ADFAD MOV [B34/BDC],EDI`
with `EDI=0` (`0x1408AD0CB XOR EDI,EDI`).

## Verdicts per branch

| Branch | Verdict |
|---|---|
| StrategyFix (`c929824`) | **Land with 3 follow-ups** (F1 word-identity note, F2 stoppage-state-3 selector gap, F3 kind-7 routing gaps — all bounded, fixes below). The tick order, decider-before-drift, flags, restart word, S2 keep-rule, plan-2 rule, start-plan-0, kind-7 constant, and RolePlayState all check out. |
| CondTactics (`586fd03`: ConditionPort + C1 + test reframe) | **Land with 1 follow-up** (G1 same-side init feeds both legs — inert today, one-line fix below). The opponent-shift side-select, band ladder, inverse map, write-skips, per-side plumbing, loader, and the reframed test's premise all check out. |

## Findings

### F1 — `modeBcc`/`word134` are a second copy of one word, not a two-word chain (comment-only, no behaviour change)

- GF (`pesteamtick.hpp:41-52` on StrategyFix): `modeBcc` ("setup +0xBCC") → `CopyModeWord`
  → `word134` ("T+0x134"), drift reads `strategy.strategy`, selector reads
  `SelectorMode(deciderRan, strategy.strategy)`. The comments describe a chain in which the
  drift "reads 134 once the writer lands".
- PES: `ctx+0xBCC` **is** `R[3]` — the decider's own output slot. Tick passes `R = ctx+0xBC0`
  (`0x1404F6DB1 LEA RCX,[RDI+0xBC0]`), the body writes `param_1[3]` (`= *(R+12) = *(ctx+0xBCC)`,
  ~15 sites in `FUN_14051E970`), the copy moves `ctx+0xBCC → T+0x134`
  (`0x1404F6DC4/0x1404F6DCA`), the drift reads `T+0x134` (ladder `0/1/3/4→self else 2`,
  re-read this pass), the selector reads `ctx+0xBCC` (`0x140501076`) — i.e. the decider
  output buffered by one tick through `T+0x134`. One word, four accesses (decider W, copy,
  drift R, selector R), not two words with an open writer between them.
- What differs: nothing observable. Post-first-move, `strategy.strategy` (GF's R[3]) is exactly
  what PES's selector/drift consume; pre-first-move, GF's −1-keep matches PES's `< 0` arm
  (`TEST EAX,EAX; JS` at `0x14050107C`) whenever the boot word reads negative/absent.
  The residual is the header comment's "setup source" framing, which invites a future editor to
  wire a separate `+0xBCC` writer that would *fight* the decider's R[3] store at the same address.
- Effect: zero today (`word134` has no reader; `modeBcc` stays −1). Risk is future-only.
- Fix (comment-only): reword `pesteamtick.hpp:41-45` to say `modeBcc` is GF's stand-in for the
  R[3] slot (`ctx+0xBC0+12 = ctx+0xBCC`) before the decider first moves it, and `CopyModeWord`
  mirrors the `0x1404F6DBD` copy of the decider output; the drift/selector already read the
  output through `strategy.strategy`. Cite `0x1404F6DB1` alongside `0x1404F6DBD`/`0x140501076`.

### F2 — `UpdatePesSheetPlan`'s stoppage arm tests state 2 only; PES tests `!= 2` inside the `!= 1` else (state-3 gap, gated off)

- GF (`teamAIcontroller.cpp:3284-3293` on StrategyFix): `if (playState == kPlayStateStopped
  && kRestartGate8D)` with `constexpr bool kRestartGate8D = false` — the arm is dead code either
  way, and when enabled it would fire only in state 2.
- PES (`FUN_140500B90`): the `== 1` test at `0x140500F12` splits in-play (l.233+) from *everything
  else* (l.291+); inside the else, `!= 2 → return` (`0x140500F36 JNZ return`) — i.e. the plan-2 rule
  runs in state 2 with the gate chain, while state 3 falls through to… the same function's
  stoppage path only (state 3 never reaches the in-play `+0xBCC` table; the mode table at
  `0x140501076` sits inside the `== 1` arm). GF's in-play table therefore runs on every state-3
  tick where PES would run the stoppage gate chain (return-keep when the gate byte reads 0).
- What differs: nothing observable while `kRestartGate8D = false` — both arms reduce to "in-play
  table or nothing", and the in-play table is only reached… on every tick including state-3
  stoppages, where PES's stoppage path would (gate-closed) also keep the plan. The two agree
  *while the gate is closed*, by vacuity on both sides.
- Effect: zero today. When the `+0x8D` byte lands and the gate opens, GF must route state 3
  through the stoppage arm (plan-2 per `StoppagePlan2`, which is already ported and tested) and
  *not* through `InPlayPlan` — otherwise a state-3 tick with mode ≥ 0 would move the plan where
  PES keeps it.
- Fix: when opening the gate, change the arm to `if (playState != kPlayStateInPlay)` for the
  stoppage path (states 2 *and* 3 enter; the `== 2` test stays *inside* as PES has it at
  `0x140500F36`), and skip the in-play table unless `playState == 1`. One `!=` + one early
  return; name `0x140500F12`/`0x140500F36` at the branch.

### F3 — Kind 7 misses three `e_SetPiece_KickOff` switches (bounded; the break window only)

- GF (StrategyFix): kind 7 is routed in `PrepareSetPiece` (fallthrough to kick-off,
  `teamAIcontroller.cpp:1576`), `PlacementBudget_s` (`pessetpieceteam.cpp:190`), and the
  referee's own take/release. Three other `e_SetPiece_KickOff` switches have no `PeriodEnd`
  arm: `ChiefRefereeSpot` (`pesjudge.cpp:404` — no `default`, so kind 7 falls off the end of a
  non-void function returning `std::optional<PitchSpot>` → `std::nullopt`), `CanCreateOffside`
  (`offsiderule.cpp:82` — kind 7 reads as *creatable*, unlike a kick-off), and the taker-placement
  `setPiece == ThrowIn || setPiece == KickOff` arm (`teamAIcontroller.cpp:2087`).
- Effect: bounded to the break window (kind 7 lives only between `EndPeriod` and the take that
  resets it to 1). `nullopt` referee spot: the break's referee placement; offside snapshot: taken
  during the break when no snapshot should arm (`ShouldSnapshot` requires `!inPlay` → false
  anyway while stopped, so the second is doubly unreachable); taker reset position: the
  break-formation path is `PrepareSetPiece`, not the taker-reset arm. All three are unreachable
  or benign on today's paths — but the `pesjudge.cpp` fall-off is UB-shaped (missing return on a
  non-void path if a caller ever unwraps it) and should not wait.
- Fix: add `case e_SetPiece_PeriodEnd:` alongside kick-off in all three switches (fallthrough in
  `ChiefRefereeSpot` to the kick-off spot; `|| restart == PeriodEnd` → false in
  `CanCreateOffside`; include in the `2087` taker-reset condition). Three one-liners; cite
  plan-selector §4 at each.

### G1 — `InitPlayers` feeds this team's word to both shift legs (inert today; wire the opponent's word when it lands)

- GF (`team.cpp:97-107` on CondTactics): `ownLevelClassSix = oppLevelClassSix =
  match->GetTeamLevelRaw(id) / 100 == 6` — both legs read this team's word. Audit-2 C1's
  prescription was "source the opponent arm from the OPPONENT's `+0xA294`".
- PES (`FUN_14092AF10`, re-read this pass): own-team leg — `FUN_140A40BC0(lVar16) != 3` guards the
  *level block*, inside which `FUN_140A602E0(...,6) == 0` (own `+0xA294`-via-`+0xA29C` class ≠ 6)
  selects the opponent shift on the flipped side (`uVar32 ^ 1`, `0x14092B516`); the own-team
  state-3 force-to-0 sits *outside/after* that block (`iVar29 = 0` fallthrough vs early shape —
  the GF comment's "no level gate on the state-3 leg" matches the decompile's nesting).
  The opponent leg therefore tests the *shifted* side's word: for team 0's players the shifted
  side is team 1 when `ownTeam == false`… but note the call site: `InitPlayers` runs per-team with
  `ownTeam = (id == 0)`, so the opponent leg (`ownTeam == false`) fires only for team 1's players,
  testing team 1's own word — which is exactly the shifted side. GF's wiring is *correct for the
  two-team exhibition* (each side's opponent-leg shift reads that side's own word = the shifted
  side), and both words are the stock 500 (class 5, shift inert) until the setup import lands.
- Effect: zero today (class 5 both sides; both legs inert). When per-side setup levels land,
  `GetTeamLevelRaw(1 - id)` must feed `oppLevelClassSix` — today the two locals alias, so a
  future per-side import that only sets one side would silently mis-shift the other.
- Fix: `const bool oppLevelClassSix = match->GetTeamLevelRaw(1 - id) / 100 == 6;` — one token
  (`id` → `1 - id`), correct under both today's aliasing and tomorrow's per-side words. Keep the
  `0x14092B516 R12D^1` citation; extend the existing ponytail with "each side's word is its own
  until the setup import; the opponent leg reads the shifted side".

## Coverage list (checked and found correct)

**StrategyFix — tick order (§5 + S1).** `StepStrategyDecider` before `CopyModeWord` before
`DriftAttackLevel` before `WriteAttackFlags`/`WriteRestartWord` before `UpdatePesSheetPlan`
matches `0x1404F6DB8 → 0x1404F6DBD → 0x1404F6DD3 → 0x1404F6E1C-58 → 0x1404F6EA8`
(all five VAs byte-verified this pass). Decider entry `(MI−1 & ~2) == 0` → states 1,3
(`0x14051E92E` bytes); body returns at once in state 1 (`0x14051E9CF CMP/JZ`, decompile);
`DeciderMovesIn`/`DeciderCalledIn` match. Drift ladder `0→0,1→1,3→3,4→4,else 2` matches the
`0x1404F8640+` chain (re-read); manual mesh, freeze (`!= 1 → hold`), clamp `min(4)`, stamp
on change all match `DriftAttackLevel`. Flags `b8a2 = b8a4 > 3`, `b8a3 = b8a4 < 1` match the
`CMP 4/CMOVL`, `CMP 0/CMOVG` bytes. GF-path decider still steps after the in-play early return
(non-PES path untouched) ✓. `deciderRan` set exactly where `StrategyStep` runs ✓.

**StrategyFix — RolePlayState vs PesPlayState.** `RoleLevel` override tests `MI+0x116C != 1 &&
MI+0x1554 == 1 → 0` (role-rebuild §2, role-writer row). Old `PesPlayState(openPlay)` returned 2
for *every* non-open-play tick including kind-1 stoppages, forcing level 0 wherever PES's
`MI+0x116C` reads non-1 — which is the same value, so no divergence there; the report's question
("does it change GF mode?") answers: the 754-tick `IsInPlay && IsInSetPiece` window is the only
input that changes (old: 2, new: 1). PES's `MI+0x116C` reads 1 there (the in-play arm is taken;
`IsInSetPiece` is a GF overlay with no MI word), so new:1 is PES-correct and old:2 was the
stand-in error — the switch-off 5-min hash move (`565856e88da735dd → e3e0ac34920d5d69`) is a
GF-mode behaviour change PES would *not* make (PES has no GF-mode path), but it is the
*intended* consequence of a PES-ward fix applied to shared code, not a regression: GF mode now
reads level 1/2 (possession) in the set-piece window instead of the kind-gated 0/1/2, and kind 7
no longer trips the EC600 kick-off override during the break. The exact line is
`teamAIcontroller.cpp:2930` + `:2933` (`DriftAttackLevel`/`WriteRestartWord` over `pesInPlay`)
together with `pesteamtick.cpp:52-53` (the freeze comment's input) — the `RolePlayState` call
sites (`SetUpRoleArrays`/`WriteRoleArrays`/`UpdateLaneLine`) only change the *role matrix row*,
while the drift/freeze input change is what moves the 5-min hash. All four call sites use the
same helper — no split-brain.

**StrategyFix — selector.** In-play table `<0 keep / <2 → 1 / ==2 → b8a3?1:0 / 3,4 → 0 / else keep`
matches `0x140501076-0x1405010D7` arm-for-arm (`JS`, `JL`, `SE TNZ`, `SUB 3/JA` shapes in the
decompile; `InPlayPlan` rename mode-correct). `SelectorMode` −1-until-decider matches S2's
prescription and the `< 0` arm. `StoppagePlan2` matches `0x140500FA9-DB7` (setup-0 keep, already-2
keep, else 2; `MOV EDX,2` past the keep). Start plan 0 with the reset citation
(`0x1408ADEA5/0x1408ADFAD`, `FUN_1408E16D0`) ✓. Gated-off stoppage arm correctly dead
(`kRestartGate8D = false`) with the `+0x8D`/`+0x1FC` ponytails ✓ (modulo F2's state-3 note).

**StrategyFix — period-end kind 7.** `RestartKindOf(PeriodEnd) = 7` ✓; written at `EndPeriod`
(`referee.cpp:281`), consumed back to 1 at the take (`referee.cpp:697`) — matches "dispatcher
consumes; next kick-off is a fresh 1" (plan-selector §4). Rows rule only special-cases 1, selector
only tests 2 → 7 changes no branch ✓ (modulo F3's three switches). `PlacementBudget_s` −1 and
`PrepareSetPiece` kick-off fallthrough ✓. The `7` store itself stays OPEN behind `0x143F559F0`
(thunk, not re-decoded this pass — chain accepted from plan-selector §4 + judge gate
`0x14050C3E0` re-read: `0x644` mask + protected-thunk call confirmed).

**CondTactics — C1 side-select.** `uVar32 ^ 1` at the opponent path (`shift_dec.txt` gate region;
`FUN_140A40BC0` = `[+0xA298]` getter, `FUN_140A602E0` = `/100 == 6` comparator — both 7-line
decompiles re-read this pass) ✓. `ApplyConditionShift`/`ApplyTeamShift` dual signatures carry
both sides' classes; own leg ignores the level (`(void)ownLevelClassSix`), opponent leg tests
only `oppLevelClassSix` with the cap (`pes < 4`) ✓. `Match::teamLevelRaw[2]` init from the stock
default, `GetTeamLevelRaw` accessor, `InitPlayers` wiring with the `R12D^1` citation ✓ (modulo G1).

**CondTactics — chain.** Band ladder `≤0x40→0, <46/4B/50/55/5A/5F→1..6, else 7` matches
`FUN_140AF2DA0` edges; inverse map `0→4,1→3,2→2,3→1,4→0,else 2` matches `FUN_141FC1380`;
write-skips (`pes != 2 && < 5`) match `0x14092B59B/5A0`; multiplier table base `0x142632D70`
(loader-read, opcode-checked ladder) ✓. `Form = bit 438/3b`, condition byte `0x46` default,
`0x28..0x6D` clamp named ✓. No-RNG (`FUN_141FE3830` RNG-clean per roll doc) ✓. Config removal
(`match_condition_home/away/seed`, `Policy`/`Draw`/`ArrowForPlayer`) is a clean cutover — no
remaining source references (only a stale string in a checked-in `.so` blob, not source).

**CondTactics — test reframe.** Premise verified from the report's trace description and the
decider doc: second-half step machine attacks only at `g < −1` (tactics §3), so 1–2 down at
minute 8 correctly reads Balanced — the old test's "unclosable 0-3" premise was indeed closed
by play, and holding the scoreline tests the switch rather than the comeback. The hold
(`SetGoalCount(0/1)` each tick after `Process`) is the same technique as the PES-loop sibling;
assertions and the 70-minute deadline unchanged. Non-production change; no PES bytes at stake.

## Open questions (no port impact today)

1. `ctx+0xBCC`'s out-of-tick writer stays OPEN (plan-selector §2: no indexed/imm writer
   image-wide). F1 reframes it: the word is R[3], written every state-3 tick by the decider —
   the genuinely open part is only the *boot* value and any menu/setup writer outside the tick.
2. `+0x8D` byte meaning, `G+0x1F8` chain, `setup+0x1FC` writer — still OPEN, still ponytail'd
   (F2's state-3 routing waits on the same decode).
3. `T+0xA29C` nonzero path, per-side setup levels/modes — still OPEN, still ponytail'd
   (G1's one-liner is ready for the import).
4. Drift's `T+0x210` manual arm, `+0x8C` flag, pad branch — unchanged stand-ins, not this audit.

## Recommended next step

Land both branches after the five one-liners: F3's three `PeriodEnd` arms (the only
UB-shaped item), G1's `1 - id`, F1's header reword, F2's `!= 1` routing note. None changes a
test expectation (all gated-off or comment/switch-exhaustiveness). Then, in order:
(1) the `+0x8D` decode (opens the stoppage arm for real, exercising F2);
(2) the per-side setup import (exercising G1 and the kind-7 break-write path end to end).
