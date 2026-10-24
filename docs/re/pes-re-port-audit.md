# PES re-port audit (PortAuditRE, 24-10-26)

Read-only audit of the ports that landed on main in the last batches, checked
against PES's bytes. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request
at a time. GF code read from `origin/main` into
`<workdrive>/gf-PortAuditRE-scratch/` (absolute output paths there). No repo
edits. Budget: ~100 of ~150 requests used.

Reference docs (all on `origin/main`): `docs/re/pes-re-mark-table.md`,
`-mark-cone.md`, `-mark-gate.md`, `-mark-readers.md`, `-mark-slot.md`,
`docs/re/pes-re-mentality-blocks.md`, `docs/re/pes-re-role-rebuild.md`,
`docs/re/pes-re-lane-writers.md`, `docs/re/pes-re-lane-post.md`,
`docs/re/pes-re-lane-leaves.md`, `docs/ports/pes-port-TeamT2.md` (runs 6-8),
`docs/re/pes-re-sheet-triple.md`, `docs/re/pes-re-missing-systems.md` (K11).

## Verdicts per port

| Port | Verdict |
|---|---|
| Mark table (`pesmarktable.*`, T7a build) | **PASS with 2 findings** (F1 candidate-order scope, F2 head-arg wiring — both bounded, fixes below) |
| Pair table (`pesscorerows.*` bearing block) | **PASS with 1 finding** (F3 stale-row scope on unmasked ticks) |
| R1/R2/R3 reader arms (`elizacontroller.cpp`, `pesdefenderdecisions.cpp`) | **PASS with 1 finding** (F4 R2 shape — OR-leg ported as early-out) |
| Mentality chain (`pesmentality.*` + wiring) | **PASS with 1 finding** (F5 `FoldCount(0)` off-by-one vs the live doc, inconsequential while the consumer is unwired) |
| Run table (`pesrunslots.*`) | **PASS, no findings** (modulo the known unwired sweep, Main's decision) |
| Slot-point rows (`src/pes/formation.*` + wiring) | **PASS with 1 advisory** (F6 `WriteSlotPoints(level)` conflates the role level with the phase; identical today, diverges if level-0 lands) |
| Substitutions allowance + forfeit | **PASS, no findings** |

## Findings (numbered, each with evidence and a proposed fix)

### F1 — T7a candidate walk reads the NearestList row, GF walks `NearestOpponents` (opponents only). Bounded miss.

- GF: `src/sim/onthepitch/teamAIcontroller.cpp:232-234` (`TickMarkTable`):
  `for (... : pes::offball::NearestOpponents(w, holder))`, `if (out >= kSlots) break`.
- PES: `FUN_1404F61E0` at `0x1404F62FA IMUL RDX,RCX,0xC8` (holder·200) +
  `0x1404F630A LEA R15,[R8+0x22E4C]` (NL row of the **holder**), then the
  11-walk `0x1404F63E0-0x1404F65EB` (`INC R14B; CMP R14B,0xB; JC`) over all 11
  row entries: `0x1404F6400 CMP EDI,0x15; JA skip` (id ≥ 22 skips),
  `0x1404F6409 CMP EDI,ESI; JZ skip` (== holder skips),
  `0x1404F6411 CMP EDI,R13D; JZ skip` (== tgt skips). Teammates are walked
  and skipped **in row order**; they are not pre-filtered.
- What differs: GF's candidate list contains only the holder's opponents
  (up to 11), in opponent-distance order. PES walks the holder's full 11-entry
  row (mates and opponents interleaved, in row order) and skips mates at the
  `holder/tgt` gates only — meaning **a mate that is neither holder nor tgt
  still occupies a walk slot and is gated by the side test / action word /
  wrap tests**, and (more importantly) the **hard exit** `0x1404F6439 COMISS
  + 0x1404F643D JA 0x1404F65F1` (return, not continue) fires on the row's
  d² order, not on the opponents-only order.
- Effect: two bounded divergences. (a) The hard-exit position differs: a far
  mate (d² > 1369) ahead of a passing opponent in row order kills the whole
  build in PES but is invisible to GF. (b) The max-3 commit order differs
  (row order vs opponent order). Both are rare in practice (37 m is far; 3
  commits cap the tail), and the gates admit mostly opponents anyway — but it
  is a real semantic gap, not a ponytail the code names.
- Fix: walk the holder's full NearestList row in row order (both blocks of
  `rows.nearest[holderLane]`, i.e. mates + opponents as PES lists them),
  applying the three skip gates verbatim, instead of `NearestOpponents`.
  Keep the `kSlots` (11) bound — PES walks exactly 11 entries. Proposed diff
  (in `TickMarkTable`): replace the `NearestOpponents` loop with a loop over
  `w.nearestRows[holderTeam]->nearest[holderLane]` entries 0..10 in stored
  order, mapping each entry's lane back to the global id.

### F2 — T7a's `headArg` is the 4AB8E0 recompute, GF feeds bearing(pos → aim). Approximation, correctly ponytail'd but the wrong stand-in source.

- PES: `0x1404F6498 MOVSS XMM7,[RBX+0x554]` (facing word, subtrahend only) then
  `0x1404F64A8 CALL 0x1404AB8E0` (verified target `0x1404F64AD + rel`); the
  call's `[RSP+0x40]` output overwrites the local the three wrap tests consume
  (`0x1404F64BF MOVAPS XMM1,XMM0` takes the call result, not XMM7). Head =
  bearing(candPos → stored point at cand+0x5EC), 0.0 when coincident.
  CONFIRMED in this pass (disassembly re-read).
- GF: `teamAIcontroller.cpp:243-247` feeds `headPos = pos`, `headPoint = aim`
  (`cf.aim`), i.e. bearing(pos → run-target aim). The header ponytail
  (`pesmarktable.hpp:79-80`) names the cone/count inputs; the wiring ponytail
  for the head names no PES site at the feed (only the generic T7a-holder
  ponytail above it).
- Effect: the three wrap gates (`≤ 105`, `≤ 80`, `≥ 80` floor) test against a
  run-target bearing instead of PES's stored-point bearing. Direction of the
  error is unknowable without the `+0x5EC` writer (still INF per the gate doc)
  — but `aim` (where the runner is going) is systematically *not* the stored
  point (a forward-distance scalar's endpoint, `FUN_14093CB6C` family). Any
  candidate whose aim disagrees with his stored point by > ~25° flips a wrap
  gate.
- Fix: keep the `headBearing_deg` caller-supplied input, but (a) extend the
  wiring ponytail to cite `0x1404AB8E0` + `0x14093CB6C` (as the gate doc §2
  prescribes), and (b) feed the candidate's **facing** (`cf.facing`, i.e.
  `[cand+0x554]`) as the stand-in, not `aim`: the facing word is at least the
  subtrahend PES actually reads, and coincides with the recompute whenever the
  stored point lies ahead. One-line change + ponytail cite. (True fix waits
  for the `+0x5EC` writer; still OPEN.)

### F3 — Pair-bearing table rebuilds on every `Tick`, PES rebuilds it on every S1 call but GF's tick is per-team (stale-row scope).

- PES: `FUN_14045B420`'s pair/NL owner loop (`0x14045BB80-0x14045BD27`) is
  UNGATED (outside the mask arms) — rebuilt on **every S1 call**, and S1 runs
  from the listener step-5 path, not the team tick (mark-table doc §3).
- GF: `Team::Process` (`team.cpp:571-575`) ticks **each team's** `TickScoreRows`
  then `TickMarkTable` in the same call: team 0 builds its rows from team 0's
  `RunWorld`... but `BuildRunWorld` (teamAIcontroller.cpp:3305+) builds the
  **global** world (both teams' players) identically for either team, and
  `TickScoreRows` fills lanes 0..10 with *its own* actives and 11..21 with the
  opponent's (`fillSide(team,0); fillSide(opponent,11)`). So team 1's writer
  transposes the lanes. `RowBearing(w,id,target)` reads
  `w.nearestRows[t]->bearing[LaneOf][LaneOf]` with `t = TeamOf(id)` — the
  owner's own-team rows — so the transpose is handled **iff** both teams'
  writers ran this tick with the same positions. They do (same tick, same
  positions) — but the mask rotation (`kRotation[frame & 3]`) means a block is
  up to four frames old, and team 0's T7a reads team 1's rows that team 1
  refreshes **later in the same tick** (team 0 processes first). One-tick skew
  on a quarter of the blocks.
- Effect: bounded staleness — pair reads agree to ~1 frame of motion. Not a
  correctness bug in the port's logic, but the audit's scope item "branch
  order" fires here: PES orders pair-rebuild strictly before the mark build
  on shared state; GF interleaves per-team (rows, marks, rows, marks).
- Fix (cheap): in `Team::Process`, tick **both** teams' `TickScoreRows`
  before **either** team's `TickMarkTable`. Two-line reorder; no new state.
  (Full fix — a shared S1 writer outside the team tick — is the S1Watch item;
  not this audit's ask.)

### F4 — R2's mark arm is an OR-leg; GF ports it as an early-out (`return true`). Over-admits the cheap verdict.

- PES: `0x140993258 75 0B / 0x140993265 C6 84 24 F0 00 00 00 01` — hit sets
  frame flag `[RSP+0xF0] = 1` (verified bytes this pass); consumed at
  `0x14099369C CMP [RSP+0xF0],0` inside the stamp gate
  `((!bVar5) || (72E70-flag, bVar6) || (...) || d2 ≤ 25.0)` — marked is one
  true-leg of a four-way OR admitting the cheap `2/4` verdict over `4ECF0`.
  CONFIRMED (readers doc §2).
- GF: `elizacontroller.cpp:1366` (`_BlockCandidate`) and `:1385`
  (`_PassDuelCandidate`):
  `if (match->PesLoop() && ... IsMarkedByTable(...)) return true;`
  **before** the carrier/radius gates — i.e. a marked man is unconditionally a
  block/pass-duel candidate, even with no carrier, no ball in window, or out
  of radius. PES's flag only shortcuts the *verdict choice* once the earlier
  gates (facing band, gate A, cone, `643A30`, holder d2 bands) already passed.
- Contrast R1 (`_TackleCandidate ... return false`) which matches PES exactly
  (R1 mark-SET ⇒ early return, no verdict — exemption CONFIRMED at
  `0x140969979 JNZ 0x140969D81`, epilogue target verified this pass).
- Effect: marked defenders engage blocks/duels they should refuse (no carrier,
  ball out of window). Small (marks ≤ 3 per team) but systematic and on the
  hot path.
- Fix: move the mark read to the verdict choice, not the entry gate. Minimal
  form preserving the port's structure: delete the two early `return true`
  arms; thread `marked` into `ChooseMarkedReaction` (which already takes it
  for R3) so the block/jostle-vs-rest choice prefers block/jostle when marked
  — or, if that threading exceeds the fix budget, gate the early-out on the
  same conditions the function already checks below it (carrier present,
  retainer == carrier / ball in window, in radius). The R3 skip
  (`ChooseMarkedReaction` zeroing tackle/block/jostleForPass — a *skip of the
  holder duel*, readers doc §3) is a different arm and stays.

### F5 — `FoldCount(0)` returns 2; the live doc says one fold at dial −1, two at −2 — and dial 0 is neutral.

- GF: `pesmentality.cpp:142`:
  `int FoldCount(int dial) { return dial < kNeutralDial ? kNeutralDial - dial : 0; }`
  with `kNeutralDial = 2`. So dial 0 → 2, dial 1 → 1, dial 2 → 0.
- Docs: mentality-blocks §3.1 — "the fold-count `T+0x110` bounds the number of
  pick iterations"; the live reclassify doc (per the handover) reads one fold
  at −1, two at −2, none at neutral. GF's dial encoding (`WriteAttackFlags`:
  `b8a4 = 0..4 = −2..+2`, init 2) means dial 1 = −1 → `FoldCount(1)` = 1 ✓,
  dial 0 = −2 → 2 ✓ — **the mapping is correct for −1/−2/neutral(2)**.
  The residual: dial 0 in the doc's §6 table is **neutral** ("neutral (shared
  reclassify values stand)"), but GF's `FoldCount(0)` = 2 folds. If the doc's
  "0" means b8a4 = 2 (neutral, display 0), no bug; if it means b8a4 = 0
  (display −2), also no bug. The function is self-consistent with the header
  comment ("one fold at dial −1, two at −2, none at neutral").
- Verdict: **no fault found** — noted only because the ticket names
  strict-vs-nonstrict: `dial < 2` (strict) is right (neutral 2 folds nothing),
  and `2 − dial` is the live reading. The `T+0x110` writer is still OPEN, so
  the count is a stand-in by design. No change. (If Main wants belt-and-braces:
  `FoldCount` should assert `dial <= kNeutralDial`; a dial of 3/4 today
  returns 0 harmlessly.)

### F6 — (Advisory) `WriteSlotPoints(level)` conflates the role level with the sheet phase. Identical today; diverges if level-0 lands.

- PES: `FUN_1404F5F10`: `f = 2 − (F≠0)`; `f = 0` when `MI+0x116C != 1 &&
  MI+0x1554 == 1` (sheet-triple doc §2). Three phases 0..2; `FUN_1408C50E0`
  bounds `f < 3` (verified: `0x1408C50F7 CMP R8D,0x3; JGE invalid`).
- GF: `teamAIcontroller.cpp:3211-3231` (`WriteRoleArrays`): `level =
  RoleLevel(...)` (0..2, with the out-of-play 0 override) is passed **both**
  to `WriteSideRoles(..., level, ...)` (correct — the matrix row) **and** to
  `WriteSlotPoints(level)` (phase). Today `RoleLevel` returns 1/2 in play
  (phases 1/2 = the two possession phases ✓ — the values coincide), and 0 out
  of play only when restartKind == 1 (phase 0 = the restart rows ✓ — also
  coincides). So the wiring is correct on every reachable path **today**.
- Residual: the two are different quantities (matrix row vs phase) that happen
  to share the formula `2 − (F≠0)`; the restart-kind-1 override coincides with
  phase 0 only because GF's `PesRestartKind()` mapping is the same byte.
  If a future port distinguishes them (e.g. a matrix row that isn't a phase),
  this call silently mis-phases. Recommend a one-line rename/ponytail at the
  `WriteSlotPoints(level)` call citing `0x1404F5F38-4A` so the next editor
  knows the two `level`s are different PES words.
- The `GridAxis` op order itself is bit-for-bit the decompile
  (`cell=(h+h)/7; step=cell/15; centre=step*0.5; j=v/15; k=v%15;
  (cell*j−h)+((cell*k)/15+centre)` — verified against `0x14051B468 DIVSS
  [BDF0]` double-divide shape); the `0x31C+12*(3*(11*fam+slot)+f)` accessor
  math re-derives exactly from `0x1408C50E0` (`LEA RAX,[0xC7+RDX*2]; ADD
  RAX,RDX; LEA RAX,[R10+RAX*4]`); keeper `−50.0` opcode-checked
  (`0x141FC1954 C7 45 88`, imm `000048C2` = −50.0 ✓); plan = bit 24 ✓
  (`Tactics::plan()`); keeper cell (3,52) ✓; bounds (49/105/11/3/≤12) ✓.

## Coverage list (checked and found correct)

**Mark table core (T7a `FUN_1404F61E0`, disassembly re-read in full this pass,
243-line `t7a.dis` saved in scratch):**
- Zero-init `T+0xB7EC..B7F6` (`0x1404F61F7/1FE/206`) ✓ (`mark.fill(0)`).
- `tgt = side*11 + B7D8`, exit unless `< 22` (`0x1404F620D-21C CMP R13D,0x15;
  JA`) ✓ (side bound, slot 0..10, `tgt > 0x15` return).
- Holder `< 22` / keeper veto / `!= tgt` (`0x1404F6242-27D`) ✓.
- Ramp base **7.0** at `0x14259BDF0` (`0x1404F6354 SUBSS`, target re-decoded),
  top +3.0 (`0x14258C264`), ladder shape (`COMISS/JC/JMP` — GF's strict
  `lo < sign*x`, `>= top → 1` matches the `JC/JC` fall-throughs), −15.0
  (`0x1404F638F SUBSS [0x14259BE0C]`, target verified) ✓. F5 = 9.16 source is
  GF's `pesGoalAreaHalfWidth` (a named constant, not transcribed per-port —
  dt18/VFS rule is a loader question, out of this audit's scope).
- Hard exit on `d² > 1369` (`0x1404F6439 COMISS; 0x1404F643D JA 0x1404F65F1`
  — leaves the function, **return** ✓, not continue).
- Side test `4C720` strict `<` (`TEST AL,AL; JZ skip`) ✓
  (`>= ... continue`).
- Action word `{1, 0x3F}` (`0x1404F6484 CMP 1 / JZ; 0x1404F648F CMP 0x3F /
  JNZ skip`) ✓.
- Head call `0x1404AB8E0` ✓ (target re-verified); the two `439FE0` pair reads
  (`0x1404F64BA P[cand][tgt]`, `0x1404F64EB P[cand][holder]`) — arg order
  `(MA, A, B) = P[A][B]` consistent with the writer's
  `(owner−other)` store order ✓.
- Wrap tests: `|wrap(P[cand][tgt]−head)| > 105 → skip` (`JA`, strict ✓);
  `|wrap(P[cand][holder]−head)| > 80 → skip` (strict ✓); floor
  `80 > |wrap(P[cand][holder]−cone)| → skip` (`0x1404F6532 COMISS XMM10,XMM2;
  JA skip` — skip when *below* 80, i.e. require ≥ 80 ✓; touching counts ✓).
- Cone call `0x140A53DA0` args (90.0 @`0x14259BE5C`, 12.0 via XMM15 from
  `0x1404F63BF [0x14259BE04]`, inner ~0.0 via `XMM13=0` + low-byte-1 quirk
  `0x1404F654F/580` ✓); gate call `0x140A4EAF0` args (64.0 @`0x14259BE44` ✓,
  12.0 re-stored @`0x1404F658E` ✓); `0x1404F6595 CMOVNZ EBX,EAX` +
  `0x1404F659D CMP AL,BL; 0x1404F659F JA skip` = count ≤ (cone≠0) ✓
  (register-correct: BL = cone bool, AL = count).
- Commit `id % 11` (`MUL 0xBA2E8BA3` ✓), cap `CMP R9B,3; JNC` (stop after 3 ✓ —
  `INC` before the compare means R9B is post-increment; GF's
  `++marks >= kMaxMarks` matches).
- `Wrap180` single-fold vs `FUN_14043A0A0`: equivalent on the reachable
  domain (bearings ∈ [0,360), diffs ∈ (−360,360)) ✓.
- Cone `FUN_140A53DF0` inner: strict annulus both sides
  (`0x140A53E2D JBE exit` outer-touch-fails; `0x140A53E3F JBE exit`
  inner-touch-fails) ✓; `0.5 × 90.0` half-scale ✓; `CMOVA` strict-`<` ✓
  (`< half*coneDeg` ✓); coincident-zero arm in the caller (`439EF0`) ✓.
- Count gate `FUN_140A4E7C0` (decompile re-read): `cand > 0x15 → 0` ✓;
  side from cand ✓; radius **12.0** strict-`<` (`0x140A4E9D5 COMISS;
  0x140A4E9D8 JNC skip` — touch-fails ✓, GF `>= radius2 continue` ✓);
  band `|c| ≤ 64/2 = 32` soft (`0x140A4EA42 COMISS; 0x140A4EA45 JA skip` —
  touch-counts ✓, GF `<= band` ✓); `o == cand` skip + `o > 0x15` skip ✓;
  side-gate OFF (`param_6 = 1` path — GF has no side gate, correct to omit
  since T7a passes 1); `param_7 = 0` ⇒ zero shift ✓ (GF uses plain positions).
  The `6C6D0` velocity shift with scale 0.0 is correctly elided (fVar17 = 0 ⇒
  charPos = position — verified in the decompile: `fVar17=(float)param_6`
  with the b80 low byte 0; GF's omission is exact, not approximate).
- R1 presence `0x140969970 CMP byte [R9+RAX+0xB7EC],0` + `JNZ → epilogue`
  (exemption) ✓ as ported. R3 presence `0x1409ADB40` + `JNZ → bVar14 = true`
  (holder-block skip) ✓ as ported. Forfeit/period/sent-off/score immediates
  (10/4/3) opcode-verified ✓.

**Pair writer (`FUN_14045B420`):** owner-minus-other order, coincident-0 arm,
`2π` fold + `180/π` scale, per-owner zero-first (`REP STOSD` 22 dwords gated
`a < 0x16`), NL `(id, d²)` via the `+0xC8`-stride helper — all match GF's
`BuildScoreRows` pair block; the wrap-back second pass is correctly NOT
ported (would destroy the bearing). GF's `SortedRow` stable ascending +
`0xFF`-fill/NaN analogue ✓; team split at 11 ✓; mask rotation
`{0x01,0x02,0x24,0x10}` ✓; latch `TA+0x11` ✓; NearestList rebuilt every call
✓ (GF rebuilds unconditionally — matches the UNGATED owner loop).

**Mentality:** `RoleLevel` polarity + CMOVZ override ✓; `BlockFor` polarity
(F=0→A, F≠0→B, `0x1405ECC21 CMP / JZ-A-path` re-verified) ✓; reclassify table
RVAs re-read (`5ECB5D,5DCB5D,ECB49...` — default arm bytes
`4D 8B D6 41 8B FE` ✓, `MOV EDI,1/2` arms ✓, `DecodeLineArm` shape ✓);
fold score (`CMP R8D,6` strict->penalty ✓, `COMISS/JBE` min-pick ✓,
`CMP [R13+0xB8A2]` full-attack skip ✓, `CMP [R8+R12+0x1748]` on-pitch ✓,
`CMP [R12+0x173C]` prev-pick ✓, `TEST EAX,EAX / JZ` role-0 ✓,
`CMP [R13+0x4B40]` protected ✓); `initialBest`/`forwardPenalty` VAs ✓
(10000.0/100000.0 script-read); `WriteRoleArrays` head+tail double-write with
the latch level ✓; `DefensiveLineOf` = reclassify-of-B on the PES path ✓
(lane-writers verdict: exact in the common case, cases 1-7 deltas named and
accepted — the lane line itself is correctly NOT ported yet).

**Run table (runs 6-8):** `Push` (`JZ` equal-no-op ✓, `JNC` older-advance ✓ —
strict `<`, equal stamp overwrites in place ✓), `Behind` wrap ✓, `Age` ✓,
`ZeroTargetBlock` variants (1 → `0x10100@+16` + zero tail ✓ — note PES also
writes `[RCX+0x18]=AL(0)/[RCX+0x1C]=EAX(0)`, i.e. pure zeros, GF's fill covers;
`0x1d..0x1f` → dword@+16 + qword@+20 ✓; `0x22` → word@+4 + byte@+6 ✓;
`0x27` → zeros ✓; `v ≤ 0x1c` → zeros ✓ — boundary `0x1c/0x1f/0x22/0x27`
edges match the `JLE/JLE/JZ/JNZ` chain byte-for-byte), `Take` order (variant
then copy — copy last ✓), `SupporterRecord` byte 16 ✓, `Release` (state 0,
push-0, bytes 0..0x1c cleared, target zeroed, 0x1d..0x1f untouched ✓ —
`40820` disassembly re-read: the `0x26C..0x288` stores clear exactly
0..0x1C past the block base... note the stores cover `+0x26C/+0x274`(qwords),
`+0x27C`(qword), `+0x284`(dword), `+0x288`(byte) = block bytes 0..0x1D... see
open question 4), `ReleaseSweep` gate (`noBallCounter != 0` return ✓ — PES
`T+0x218 == 0` runs the sweep; GF's polarity matches), stamp mapping
(frames, `enteredAt`, `f.t = frame − enteredAt`) ✓. The sweep stays UNWIRED
per Main's decision (run-8 note) — not a finding.

**Sheet triple:** accessor/phase/grid/keeper/plan/bit-24/cell/bounds — all ✓
(see F6). `UpdatePesDesignation` lateral key = f2 raw both sides ✓
(`teamAIcontroller.cpp:3262-64`).

**Subs/forfeit:** allowance = install limit (6, `0x141FBA4CE B8 06` ✓);
`Validate` order (stoppage → allowance → out → sent-off → in) matches the
commit's cited `FUN_140521000` queue-count behaviour; the
`GetRemaining − queued <= 0` gate with the `<= 0 → < 0` mutation killed ✓;
`Commit` clamps at limit ✓. Forfeit immediates opcode-checked ✓
(period 10 `41 83 BC 24 68 11 00 00 0A` ✓, sent-off 4 `83 78 08 04` ✓,
score 3 `C7 80 74 1B 00 00 03...` ✓, min-writer 10 `BA 0A000000` ✓ with the
writer≡gate agreement check ✓); `BelowMinimumPlayers` strict-`<`
(`0x140447E79 CMP AL,[RDX+0x17]; 0x140447E7C JNC skip` — touch-continues ✓,
GF `onPitch < minimum` ✓); `3BFC0` counts on-pitch over 11 slots ✓ (GF
`GetActivePlayers().size()` — same count); forfeit loop skips off-pitch
(`SETNZ/TEST/JZ skip` ✓ — GF iterates on-pitch only ✓), checks
`[RAX+0x14] == 0` then `[RAX+0x8] == 4` in that order ✓, writes flag+score ✓.
`FUN_140A23380` (second writer, any-period) is correctly out of scope — GF
runs the writer once at the period change with the documented ponytail.

## Open questions (no port impact)

1. `FUN_140A92D40/2DC0` bit-exactness (inherited lane-core O3) — bearings
   match to ~1e-7, not bits. Affects F2's fix choice not at all.
2. `+0x5EC/+0x5F4` head-point writer (`0x14093CB6C` family) — F2's true fix.
3. `+0xB5AC/+0xB5B0` action-word vocabulary — the `passGet ? 1 : 0x3F`
   stand-in over-admits (correctly ponytail'd; not re-audited here).
4. `Release` byte-boundary `0x1c` vs `0x1d`: `40820`'s last point store is
   `MOV byte [...+0x288]` = block byte 0x1C... wait, `0x288 − 0x26C = 0x1C`,
   so the clear covers bytes 0..0x1C *inclusive* (29 bytes, `kReleaseClearedBytes
   = 0x1d` ✓ — `fill(begin, begin+0x1d)` clears 0..0x1C ✓). No bug. The test's
   `0x1d` sentinel check confirms the exclusive bound. ✓ (Checked because the
   ticket names edge cases; it matches.)
5. R2's `bVar5`/age-gate/`72E70`/`643A30` interior — F4's fix threads around
   them; full R2 payload port is a separate (large) item per the readers doc.
6. `T+0x46EC` second float table (A64410 l.99 reader) — still without a GF
   carrier; blocks only the exact lane line, not this audit's ports.
