# PES community Q&A audit: external auditor posts and channel export (2024-09-30 to 2026-10-19)

Scope: one community Discord channel export (3,725 messages). It holds technical claims from an external PES 2021 reverse-engineering auditor (84 posts, 2026-10-16 to 2026-10-19), technical claims from the community (hearsay, mostly PES16 to PES21 era), and questions put to the auditor. Read-only audit: no `src/`, `tests/`, `tools/` or existing `docs/re/` edits. Only this file and one index row changed. Proposed doc corrections are in §6, not applied.

Tags: **NEW** (not in `docs/re/`), **CONFIRMS** (doc:line), **CONTRADICTS** (doc:line and both statements), **PARTIAL** (doc:line; what is missing). Source column: **A** = auditor, **C** = community. "My check": **dec** = Ghidra bridge decompile (127.0.0.1:8080); **raw** = bytes read from the unpacked image `pes_full.bin` (base 0x140000000, offset = VA − base) with capstone; **doc** = read in our docs only; **-** = not checked. Auditor claims marked "A, not re-verified" were not checked by me.

## 1. Method

- **Export:** 3,725 messages, 2024-09-30 to 2026-10-19, sorted by timestamp.
- **Auditor:** 84 posts, matched by author name or nickname (case-insensitive). All fall between 2026-10-16 and 2026-10-19; the auditor is absent from the earlier export. I read every auditor post in full.
- **Community:** 3,641 non-auditor records; 3,628 carry text. Five read-only passes covered all 3,628 text records in five contiguous chunks of about 90 KB each (anonymised labels, time order). Each pass returned a claim table, a question list, a correction list and open leads. Those tables were capped, so low-value chatter is summarised rather than tabulated. I also read the 245-hit keyword scan (the technical hits are the ones tabulated) and the 15 technical-looking non-auditor posts directly. A Track-B keyword scan (glove, turf, LUT, exposure, uniform, grading) found 20 hits, none of them technical.
- **Questions:** 60 records that either reply to an auditor post or @-mention the auditor and contain a question or request. Some are meta pings ("are you still working on this"). 49 pair with an auditor answer through the reply chain; the other 11 were paired by reading the text (§5).
- **Exe checks:** see §7 for each claim checked and how. The Ghidra bridge xref endpoint returned 404, so every "zero xrefs / zero callers" claim is unverified by me.

## 2. Who answers

The auditor is the only poster who gives byte-level technical answers, with addresses, asm and constants. Most of those answers were posted in the three days from 2026-10-17 to 2026-10-19. Community technical claims are hearsay with no VAs and mostly concern PES16 to PES21 playstyle and position behaviour. Most community questions are directed at the auditor and concern the auditor's areas.

## 3. Ranked findings (by what they unblock)

Ranked against the open work list in the brief: A5 restarts, A4 carrier units, role-array writers (T+0x4BD4/0x4C00/0x3FB8), directive word T+0x80A0 writer, playing styles and reverter, advanced instructions, stamina and subs, keeper, set pieces, Track B, tactics and team concepts.

### F1. Role-array writer chain (A4; open writer hunt for T+0x4BD4 / 0x4C00 / 0x3FB8)

- **Claim (A, 2026-10-18):** `sub_140A425A0` stores the raw role-class at `+0x3F8C+4·slot` and `sub_140A42590` stores the array at `+0x3FB8+4·slot`. Both are filled per team by `sub_140A59F90` from the formation registry (`T+0x8E08`) via `FUN_1408C51F0` (class) and `FUN_1408C5310` (role), with a card-mode remap. The output enum is 0 GK, 1 CB, 2 FB, 3-6 MF, 7/8 FW, 9 CF. Rebuilt at discrete phases (phase handler `sub_1404A5DE0`). _Corrected 19-10-26 from pes-re-role-writer.md §6: both stores write the same class value (same R8D at all 7 call pairs); A/B differ only after the `FUN_140A422C0` B-swap — the auditor's A/B distinction does not exist._
- **Status:** the writer functions are **NEW** (`grep 140A59F90 / 140A425A0 / 140A42590` in `docs/re/`: no hits). Our docs read the arrays without a writer: `pes-re-stamina-subs.md:26` gives the class map GK0 CB1 LB/RB2 DMF3 CMF4 LMF/RMF5 AMF6 SS7 LWF/RWF8 CF9 (**CONFIRMS** the reader; **PARTIAL** on the writer). ~~The auditor's remapped enum (7/8 = FW, 9 = CF, 3-6 = MF) is a different output and conflicts with that map's 7 = SS and 8 = WF. The two should be treated as the two branches of the `param_4` flag until the caller is read (**PARTIAL**, see §6 item C5).~~ _Corrected 19-10-26 from pes-re-role-writer.md §6 (SUPERSEDES §6 C5): both branches implement the SAME role→class map (switch tables dumped byte-identical); the flag selects source row +0x28 vs +0x8C8, not a different enum; the coarse family names describe the same map._
- **My check (dec, 0x140A59F90 / 0x140A425A0 / 0x140A42590):** the switch maps remap outputs 0→0, 1→1, 2/3→2, 4→3, 5→4, 6/7→5, 8→6, 9/10→8, 0xB→7, 0xC→9. The loop calls `425A0(base, i, v)` and `42590(base, i)` once per slot. The 425A0 body stores `*(base+0x3F8C+4i) = v`. The 42590 body stores `*(base+0x3FB8+4i) = param_3`. ~~The decompile shows only two arguments at the 42590 call site, so the value passed is inferred from the register state and is **INF**.~~ R8D=ESI at both calls (MOV R8D,ESI before each CALL); same value CONFIRMED (role-writer §6: asm `0x140A5A073-8A`, `0x140A5A1C1-DE`; same pattern at all 7 pairs). Both arrays therefore appear to receive the same value per slot from this writer. That makes array A and array B identical right after this writer, which the auditor's A/B distinction does not allow for. ~~The base pointer `param_1+0x44` was not resolved (**INF** that it is `T+0xC48`, which would give the docs' `T+0x4BD4` and `T+0x4C00`).~~ `param_1+0x44/+0x8204` are the two teams' S blocks; S=T+0xC48 CONFIRMED (role-writer §6: setup caller `FUN_1404F5F10(T+0xC48,...)` at `0x141FBB06A`, dec L179). The caller that sets `param_1` and `param_4` was not found.
- **Unblocks:** the role-array writers in A4 and the TeamData offsets used by the positioning selectors.

### F2. The "designated attacker" lookup is the keeper selector (A4 keeper; label conflict)

- **Claim (A, 2026-10-17, 2026-10-18):** `GetDesignatedAttacker` (0x140A62690) returns the first on-pitch slot whose array-B value (`T+0x4C00`) is 0, else 0xFF. "Designated attacker" = array-B 0. The chaser and the -1 protection both use this label.
- **Status:** **CONFIRMS** the mechanism and **CONTRADICTS** the label. `pes-re-lane-core.md:158` and `:214-222` (§5.1) describe the same function as the side keeper slot (category 0 = keeper, marked OPEN O1). `pes-re-stamina-subs.md:26` maps class 0 to GK0. The auditor's own remap (F1) also gives 0 = GK. So array-B 0 is the keeper family, and the "designated attacker" name is wrong. The EF designated aerial attacker is a different variable, `P+0x3EF8` (`pes-re-extra-frontman.md:116`).
- **My check (dec):** `0x140A62690` calls `0x140A62610(side, MI, T)`, which returns the first active `j` with `T+0x4C00[j] == 0` (slots ≥ 11 read `T+0x4C00` directly). `0x140A62690` returns `side·11 + j`.
- **Unblocks:** O1 (category 0 = keeper) closes to OBS once the class map is confirmed on the live writer. It also tells the keeper work which function to read.

### F3. Extra Frontman +0.2 exists, but is narrower than our doc says (designated aerial attacker)

- **Claim (A, 2026-10-17):** "The 0.2 bonus does not exist in 1.01.00." The auditor swept `GetDesignatedAttacker` and the lineup picker and found no 0.2. The picker score is geometry only: `+15.0` if array-A role is 5 or 8, `−12.0` if `|z| < 18`, stats considered: zero.
- **Status:** **CONTRADICTS** the existence claim. `pes-re-extra-frontman.md:107-109` gives `+0.2` for bit 15 (the constant at `0x142598578`) in the aerial taker loop `FUN_1405FC860`. The auditor's sweep did not include that function. The picker constants are real and used (**CONFIRMS** `pes-re-positioning-phases.md:22`), but the auditor's "no stats" claim for the picker is not verified.
- **My check (dec):** `FUN_1405FC860` adds `DAT_142598578` (0.2) when `FUN_140A64030(slot, 0xF, …)` is true. `FUN_140A64030` reads `T+0x4C00[slot]` and calls `FUN_1408CB0B0(record, 0xF, B[slot])`. That returns true only if `record+0x48` bit 15 (EF playstyle) **and** `DAT_142613D20[B[slot]]` bit 15 are both set. Decoding the table: classes 0 to 11 have bit 15 set only for class 1 (CB). **Narrowing:** the +0.2 applies to Extra Frontman holders in CB class slots. Our doc (`extra-frontman:107`) says only "bit 15", so it is **PARTIAL** (§6 item C1). Constants checked: `0x14259BE0C = 15.0`, `0x14259D664 = −12.0`, `0x14259BE18 = 18.0`, loaded in `FUN_1404A7D60` (dec, lines 163–165).
- **Unblocks:** the EF/aerial designation (set-pieces, header attacker) and the picker that feeds the designated slot.

### F4. Chip Shot Control and PK Specialist: aim-scatter term in the kick calculator (kick 54 unresolved)

- **Claim (A, 2026-10-17 to 2026-10-18, final):** `FUN_1406B4000` runs for kick kind 54. The base offset is 0.5, or 0.4 with PK Specialist (skill bit 29). The chip branch adds 0.2 with Chip Shot Control (bit 31), else 0.4. Blended from Place Kicking (0.6) and Finishing (0.4), ramp 40 to 99. The card halves the aim-scatter bump. Kind 54's identity is **unresolved**: an earlier "penalty kick" identification (`ANIME_PENALTYKICK`) was retracted.
- **Status:** **NEW** (no `docs/re` hit for `1406B4000`). Skill names CONFIRMED by `pes-re-carrier-ai.md:622-625` (29 PK_KICKER, 31 CONTROL_LOOP_SHOT). Stat-index conflict in our docs: `pes-re-matchlogic.md:93-95` lists 0x18 Finishing and 0x1A Place Kicking (stat-id space), while `pes-re-matchlogic.md:655` says in-match 0x1A = Finishing (**PARTIAL**, §6 item C4).
- **My check (dec + raw):** on `param_5 == 0x36`, base `DAT_142565600` (0.5) becomes `DAT_14259B89C` (0.4) when bit 29 of the dword at `+0x40` is set. With the card-condition argument set, add `DAT_142598578` (0.2) if that dword's sign bit (bit 31) is set, else `DAT_14259B89C` (0.4). Weights are `DAT_14257A190` (0.6) × byte `+0x20` and `DAT_14259B89C` (0.4) × byte `+0x1A`, ramp `0x14259BE2C` (40) to `0x1425C48CC` (99). Which stat each in-match byte holds is **OPEN**: the auditor names `+0x20` = Place Kicking, `+0x1A` = Finishing, and our docs disagree on the index space.
- **Unblocks:** PK and chip accuracy for the league. The kick-54 identity (kick-kind table, not the ANIME action table) is still open.

### F5. Man Marking mark-spot cadence (k-table) and the pressuring gate

- **Claim (A, 2026-10-17):** `FUN_140628400` picks a per-slot refresh cadence k from live Defensive Awareness: ≥96 → 1, 91-95 → 2, 81-90 → 4, 71-80 → 6, ≤70 → 8. Man Marking forces k = 1. The pressuring dispatcher `sub_1409677C0` requires DA ≥ 75, and MM or TB exempts a player.
- **Status:** cadence **CONFIRMS** `pes-re-defensive-shape.md:104-110` and `:356` (EV-DEF-0005). The 75 gate is **PARTIAL** (not in our docs as a gate; not checked by me).
- **My check (dec, 0x140628400):** immediates `0x60` (96), `0x5B` (91), `0x51` (81), `0x46` (70) and the card bit test `>>0x16 & 1` at lines 90-94 match the table.

### F6. Track Back widening judge and the chaser writer (defensive commit radii; chaser slot)

- **Claim (A, 2026-10-17):** the widening in `sub_1405C0630` applies to any player with DA < 65 when the ball is within 36 m of goal. q is 1 at ≤ 55, 0 at ≥ 65, linear between. Track Back (bit 23) skips it with no chaser gate. The chaser write `sub_140536210` (state `0x3A`, MATCH_UP) gives the slot `table[candidate] % 11`, gated on team byte `+0x1775` being 3 or 8 to 10.
- **Status:** widening constants **NEW** in detail. `pes-re-anim-select-2.md:117` has the 55/65 normalisation and `:348` has the +5 m interception radius (**PARTIAL**: no 36 m gate, no Track Back skip). The MATCH_UP writer **CONFIRMS** `pes-re-defensive-shape.md:168` and `pes-re-defender-decisions.md:276`. The `+0x1775` gate and the `% 11` slot rule are **NEW**.
- **My check (dec, raw):** `0x1405C0630` checks `(record+0x40) >> 0x17 & 1` and zeros the widening term when set. Constants `0x14259BE38 = 55`, `0x14259BE48 = 65`, `0x14259BE84 = −10`, `0x1425A9464 = 36`, `0x14259222C = 5`, all read as f32. `0x140536210` gates on `byte [ctx+0x1775] == 3` or `− 8 < 3` and stores state `0x3A` through `FUN_140A42940`, indexed `% 0xB`.

### F7. Attack level, mentality dial and the -2 switch

- **Claim (A, 2026-10-18, 2026-10-19):** the prematch mentality is `+0x134` (0 = −2, 1 = −1, 2 = neutral, 3 = +1, 4 = +2; invalid → 2), copied every think tick from `brain+0xBCC`. The attack level `T+0xB8A4` drifts toward it through phase gates. Flags `+0xB8A2` (level ≥ 4) and `+0xB8A3` (level ≤ 0) are set in the think window. At −2 (level 0) the "one striker stays up" floor switches off and the support licences go to zero.
- **Status:** the copy, the mapping and the flags **CONFIRMS** `pes-re-tactics.md:103`, `:12`, `:109`, `:375` (EV-TAC-0005), `pes-re-com-styles.md:222` and `pes-re-extra-frontman.md:183`. The B8A3 reader in `BallPlayer_DefensiveLineDepth` and the −2 floor and licence changes are **NEW** (not verified by me).
- **My check (raw/dec):** `0x1404F6DBE`: `mov eax,[rdi+0xBCC]; mov [rcx+0x134],eax` (copy). `0x1404F6E1C`–`0x1404F6E58`: `B8A2 = (B8A4 ≥ 4)` and `B8A3 = (B8A4 ≤ 0)`. `0x1404F84BB`: `mov [rcx+0xB8A4],eax` (store).

### F8. The -1 pull-back: which forward is held up (defensive block `0x140682770`)

- **Claim (A, 2026-10-18 to 2026-10-19):** exactly one forward is held up per team. The protection pick is the last live forward in roster order by default (ties go to the later slot), or the first when a counter-target is armed on that forward or ≤1 forward is live. Every other forward takes the level-scaled drop (level × 0.25, with ×0.65 and ×0.2 extras in 3+ forward lineups). Normal forwards get an extra +7.0 pull from `0x1405E7AD1`. Fox in the Box and the Hole Player style are exempt. The selection is deterministic: no RNG.
- **Status:** **NEW** (the protection rule). `pes-re-tactics.md` and `pes-re-team-concepts-port.md` reference `0x140682770` as the defensive block. **PARTIAL:** the "last" rule is correct only for ties. **CONTRADICTS** the 7.0 constant: `0x14259BDF4` holds 8.0, not 7.0 (`pes-re-attack-decisions.md:134`, `:268`, `:387` all use 8.0).
- **My check (dec):** the forward loop in `0x140682770` replaces the best when `fVar43 <= fVar41`, where `fVar41` is a per-slot float plus bonuses: `DAT_142591140` for class 9, and `DAT_14259BE84` (−10.0) when `FUN_1406405C0` and group size ≥ 3. So the protected forward is the maximum of per-slot values, with ties to the later slot. The "last" ordering is therefore not a default; it follows from the per-slot values. Not verified by me: the −2 floor, the `+7.0`/`8.0` site at `0x1405E7AD1`, and the `B8A3` reader.

### F9. Designation list, FALSE_9 helper and designation groups (new per-tick tables)

- **Claim (A, 2026-10-18):** a designation list `+0x8924` (count) and `+0x8928` (up to 11 player IDs), with a twin at `+0x8954`, is rebuilt every brain tick by `sub_1404F6C30` through `sub_140A40CA0` (bucket by class, then lateral order from `+0x46EC`) and published by `sub_14051E350`. The 2-CF designator `sub_140501160` runs when the formation flag `+0x255` is set. `+0x8828` is a per-slot assignment table (2 = CF-pair class).
- **Status:** **NEW** (`grep 0x8924`: no hits; `+0x8828` appears in `pes-re-run-targets*.md` without semantics). Not verified by me except as below.
- **My check (dec, `0x140A63680`, FALSE_9 helper):** requires instruction table entry kind 2 with id 0xFF, `+0x8828[slot] == 2`, and `+0x87CC` count byte ≠ 0; the 2-CF branch uses the `+0x882C` pair index. The auditor's FALSE_9 post cites `+0x87EC` for the same count, so the auditor is internally inconsistent here. The decompile reads `+0x87CC` (matches `pes-re-foundation*.md`).

### F10. Advanced-instruction enum, tester and setter (sentinel 17)

- **Claim (A, 2026-10-18):** the 18-name enum is an inline 16-byte-stride table at `0x14261433C`. Entry 17 `DEFENCE_MAX` has zero xrefs and is unreachable: the tester `0x140A63950` rejects id ≥ 0x11, and the sheet setter `0x140A42600` refuses to store 0x11. The per-slot test census is 8, 0xA, 0xE, 0xF always per slot, plus 4, 0xB, 0xC, 0xD per slot as well (3, 3, 4, 6 sites). Testing `FUN_140A63950` costs one compare.
- **Status:** enum and sentinel **CONFIRMS** `pes-re-tactics.md:235-241` and `pes-re-team-plans.md:316-345`; the sentinel upgrades from INF(high) to OBS. The "exactly 8/A/E/F" census is **CONTRADICTS** by the auditor (§6 item C2). The per-slot counts for 4/B/C/D are **NEW**, not verified by me.
- **My check (dec):** `0x140A63950`: `cmp r8d,0x11` bound and four-entry scan over `+0x25CF8` / `+0x25D38`. `0x140A42600`: `cmp param_3, 0x11 → return`. Raw bytes at `0x14261433C` spell `NONE`, `OPEN_WIDE`, `FALSE_9`, …, `DEFENCE_MAX` (confirms the table). The zero-xref claim was **not** checked (bridge xref endpoint 404).

### F11. Early-cross picker `sub_142125BB0` (receiver choice)

- **Claim (A, 2026-10-17 and 2026-10-18):** the picker walks the 11 slots of the caller's team in ascending order (global id `11·team + slot`), filters each candidate through `sub_142123750` (geometry plus a quality screen), and ranks by **height only** (byte +49). The compare is strictly greater, so ties keep the earlier slot. The screen is skipped when the passer has Pinpoint Crossing (bit 14), the early-cross COM style (bit 3) or centring targets armed. Community read (C): "gold gets you nothing" in this path.
- **Status:** the function is **CONFIRMS** (`pes-re-attack-decisions.md:290`, `:358`). The ranking rule and screen are **NEW**. Pinpoint Crossing = bit 14 **CONFIRMS** `pes-re-carrier-ai.md:622`. Not verified by me.

### F12. Skill cards with no verified consumer (negative static sweeps)

- **Claim (A, 2026-10-17 to 2026-10-18):** Step On Skill Control (bit 32), Outside Curler (bit 15), No Look Pass (bit 28) and Rabona (bit 16) have no `CheckSkill_Live` caller and no confirmed direct mask read in the sweep. The auditor calls them motion or clip-selection cards, inert in the match logic. Heading (bit 6) is a shoot-only +5 on header kicks. The auditor recommends a CE watchpoint on the mask to close them.
- **Status:** the four negative results are **NEW** and **PARTIAL**: our skill table lists them with no consumer status (`pes-re-matchlogic.md:149`, `:164`, `:165`, `:166`). Heading +5 for SHOOT **CONFIRMS** `pes-re-contact.md:359` and `pes-re-kick-accuracy-port.md:10`. Not verified by me; the static sweep may miss computed-offset reads.

### F13. Mechanic corrections that stop docs absorbing wrong claims

- Speeding Bullet and Double Touch: the −50 "penalty" is retracted. The real mechanism is a priority override: COM bits 1/4/2 replace the tentative DT base of 70 with 20. Compare `pes-re-com-styles.md:135` (SB section) and `pes-re-carrier-ai.md:639`. **PARTIAL**, not verified by me.
- Long Ball Expert: +50 to the long-pass score in `ImageUnitLong` (`0x14211B2D1`–`DC`) **CONFIRMS** `pes-re-carrier-ai.md:389` ("+50 COM 6 LONG"). Not re-decompiled by me.
- Classic No. 10, Anchor Man and Orchestrator are **vetoed** from the enhanced diagonal run in `sub_140613910` at `0x140613B48` (auditor correction; the wiki said "parameters"). **PARTIAL:** `pes-re-style-census.md` cites `140613b48`; wording not checked.
- Dummy Runner's diagonal run is the enhanced parameter set; the veto excludes CN10, Anchor Man and Orchestrator from the enhanced run only.
- Destroyer tackle readers: `0x140790441` (+ `444` shift) **CONFIRMS** `pes-re-playing-styles.md` and `pes-re-style-census.md`. Auditor adds `0x14078E9B4`, `0x14096FEC5/C8` and `0x140971F85/F88` (**NEW**, not verified by me).

### F14. Playstyle slot names (bit 9 and the "Hole Player" confusion)

- **Claim (A, 2026-10-19):** the playstyle name table at `0x1434A10D0` (slot N = entry 42+N) gives slot 2 = FOX_IN_THE_BOX and slot 9 = FREE_ROAMING. The wiki "Hole Player" is slot 9. The auditor's earlier "deep-lying forward" was not a playstyle and is retracted.
- **Status:** **CONFIRMS** `pes-re-team-plans.md:316-345` (style list, CLASSIC_NO_10 = 8, FREE_ROAMING = 9). The wiki name map is a community note (C), not a byte fact.

### F15. Tactics/team-concepts gaps the auditor filled

- The sheet setter `FUN_140A42600` writes the team sheet records (`T+0x8E08`). `pes-re-team-concepts-port.md:58` says the in-match writer is unfound. **PARTIAL:** the sheet writer is found; the in-match writer is still open.
- The fullback and wide instructions are inert when no slot of the matching class exists (candidate lists are class-keyed). **NEW** (A, not verified by me).
- The `GEGEN_PRESS` veto on TransitionChase/ForeCheckPress **CONFIRMS** `pes-re-tactics.md` §9 (the ForeCheckPress inversion note) and `pes-re-team-concepts-port.md:58`.

### F16. Track B: no evidence in the corpus

- The Track-B keyword scan (glove, turf, LUT, exposure, uniform, grading, kit) found 20 hits, none technical. No claims for turf colour, grading LUT, exposure, kits, gloves or uniform parts. **No new evidence for Track B from this channel.**

## 4. Claim table (by topic)

Columns: ID | date | src | claim | evidence (poster) | status (doc:line) | my check. Community rows are summaries; no community claim carries a VA.

### 4.1 Playing styles, COM styles and skill cards

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| P1 | 2026-10-18 | A | Playstyles bit table: slot 2 FOX_IN_THE_BOX, 9 FREE_ROAMING | IDB name table | CONFIRMS team-plans:316 | - |
| P2 | 2026-10-18 | A | Long Ball Expert: +50 in ImageUnitLong | decompile | CONFIRMS carrier-ai:389 | - |
| P3 | 2026-10-18 | A | Speeding Bullet and DT: COM bits 1/4/2 override base 70 with 20; not a −50 penalty | decompile trace | PARTIAL com-styles:135 | - |
| P4 | 2026-10-18 | A | Classic No. 10 / Anchor / Orchestrator vetoed from enhanced diagonal run | decompile 0x140613B48 | PARTIAL style-census | - |
| P5 | 2026-10-18 | A | Classic No. 10 positioning: back limit +1 m, forward limit +8 m | decompile sub_1405E6E80 | NEW | - |
| P6 | 2026-10-18 | A | Creative Playmaker near Goal Poacher: positioning adjustment | wiki + 13 call sites | NEW (unspecified values) | - |
| P7 | 2026-10-17 | A | Extra Frontman: +0.2 in aerial taker loop; gate is CB-class-only | decompile FC860 | PARTIAL extra-frontman:107 | dec |
| P8 | 2026-10-17 | A | Man Marking card: mark-spot cadence k = 1 and pressuring exemption | decompile 140628400 | CONFIRMS defensive-shape:104-110 | dec (cadence) |
| P9 | 2026-10-17 | A | Track Back: skips widening (bit 23), chaser DA→99 local only | decompile 1405C0630 | PARTIAL anim-select-2:348 | dec (bit 23) |
| P10 | 2026-10-17 | A | Chip Shot Control halves aim-scatter bump on kind-54 kicks | decompile 1406B4000 | NEW | dec |
| P11 | 2026-10-17 | A | PK Specialist: base offset 0.5 → 0.4 in same calc | decompile 1406B4000 | NEW | dec |
| P12 | 2026-10-18 | A | Step On Skill Control, Outside Curler, No Look Pass, Rabona: no consumer found | static sweep | NEW (negative, PARTIAL) | - |
| P13 | 2026-10-18 | A | Heading card: +5 on header SHOOT only | contact research | CONFIRMS contact:359 | - |
| P14 | 2026-10-18 | A | Destroyer readers: four sites (tackle, slide) | decompile (A) | PARTIAL: `0x140790441` in playing-styles and style-census; other three NEW | - |
| P15 | 2026-10-17 | A | Low Lofted Pass: loft factor 3.5 vs 4.2; pass-type weight +1; range 18–50 m | decompile 0x140866F9A / 0x14053C475 / 0x1409805ED | NEW | - |
| P16 | 2026-10-17 | A | Weighted Pass / Low Lofted Pass weight used only when passer stat ≥ 80 | decompile (A) | NEW | - |
| P17 | 2026-10-18 | A | Outside Curler: no CheckSkill_Live(15) caller; candidate 0x14081D120 eliminated | sweep | NEW (negative) | - |
| P18 | 2026-10-18 | A | Heading and LBE: consumer checks by style bit in ImageUnitLong | decompile | CONFIRMS carrier-ai:389 | - |
| P19 | 2026-10-18 | C | Speeding Bullet makes players run at goal; Mazing Runner increases dribbling | hearsay | not checked | - |
| P20 | 2026-10-18 | C | Only one COM style used per situation | hearsay | not checked | - |
| P21 | 2026-10-17 | C | Track Back works like gegen press on DMF/CMF/AMF (PES16) | hearsay | not checked | - |
| P22 | 2026-10-17 | C | Track Back does not work on CBs (PES16) | hearsay | consistent with P9 | - |
| P23 | 2026-10-18 | C | Fighting Spirit broken; "90% functional" | hearsay, community wiki | not checked | - |
| P24 | 2026-10-18 | C | Hole Player does nothing on SS | live observation (community) | not checked | - |

### 4.2 Team instructions (advanced instructions) and formation semantics

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| T1 | 2026-10-18 | A | Enum 0 NONE … 16 WING_BACK, 17 DEFENCE_MAX | raw bytes 0x14261433C | CONFIRMS tactics:235-241 | raw |
| T2 | 2026-10-18 | A | 17 unreachable: tester `<0x11`, setter refuses 0x11 | dec | CONFIRMS tactics:239 (upgrade to OBS) | dec |
| T3 | 2026-10-18 | A | Per-slot test sites: 8/A/E/F always; 4/B/C/D also (3,3,4,6) | census | CONTRADICTS tactics:236-240 (C2) | - |
| T4 | 2026-10-18 | A | FALSE_9: −10 positioning score when helper holds; per-slot, no forward count | dec 0x140A63680 | CONFIRMS tactics §6.3 | dec |
| T5 | 2026-10-18 | A | FALSE_9 helper uses `+0x8828==2`, `+0x87CC`, pair index `+0x882C` | dec | NEW | dec |
| T6 | 2026-10-18 | A | FALSE_SB: eligibility via flag `+0x8C2A` and formation word `+0x8CE8` in {11,12} | census | NEW | - |
| T7 | 2026-10-18 | A | BOTH_SB_UP: 8 m support push; push count capped at 3 | decompile 0x1405DF1F0 | NEW | - |
| T8 | 2026-10-18 | A | FLOW_SIDE: long-pass flick suppressed at 0x1405465B0:44 (start-veto) | dec | NEW | - |
| T9 | 2026-10-18 | A | TIKI_TAKA: receiver difficulty gate skipped; short windows 6→3 m | dec | NEW | - |
| T10 | 2026-10-18 | A | TARGET_CROSS: box runners 3 instead of 2 when level > 2 and ball > 16 m | dec | NEW | - |
| T11 | 2026-10-18 | A | DEFENSIVE (per-slot): excluded from attacking runs, pulled 5 m deeper | dec | NEW | - |
| T12 | 2026-10-18 | A | GEGEN_PRESS (13): blocks ForeCheckPress and TransitionChase starts, +5 m line | dec | CONFIRMS team-concepts-port:58 (partial) | - |
| T13 | 2026-10-18 | A | MARK_HARD (14): stores opponent index at +0x2C; +5 m cover | dec | NEW | - |
| T14 | 2026-10-18 | A | READY_COUNTER (15): +10 m line for Counter Target; excluded from WING_BACK drop | dec | NEW | - |
| T15 | 2026-10-18 | A | WING_BACK (16): eligibility via class {5,8}, ≤4 defenders | dec | NEW | - |
| T16 | 2026-10-18 | A | OPEN_WIDE: width cap 5 m vs 20 m | dec | NEW | - |
| T17 | 2026-10-18 | A | Instructions with no matching class (no FB, no wing) are inert; nothing reassigns | class-keyed candidate lists | NEW | - |
| T18 | 2026-10-18 | A | Setter `0x140A42600` writes team sheet; id 0x11 skipped | dec | NEW (sheet writer) | dec |
| T19 | 2026-10-18 | A | Sentinel 17 = "DEFENCE_MAX", intent (sentinel vs unfinished) not provable | - | NEW | - |
| T20 | 2026-10-18 | A | Preset change re-derives contents of slots; slot order fixed | formation-editor caller chain | NEW | - |
| T21 | 2026-10-17 | A | Instruction-driven slot tags: 0 designated attacker, 1/2/5/8 selectors | (see F2) | CONTRADICTS (label) | - |

### 4.3 Mentality, attack level and the dial

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| M1 | 2026-10-18 | A | `+0x134` = prematch mentality: 0 −2, 1 −1, 2 neutral, 3 +1, 4 +2; invalid → 2 | raw/dec (A) | CONFIRMS tactics:12, :109 | dec (copy) |
| M2 | 2026-10-18 | A | Copy each think tick from `brain+0xBCC` to `+0x134` at `0x1404F6DCA` | raw | CONFIRMS tactics:103 | raw |
| M3 | 2026-10-18 | A | Flags `+0xB8A2` (level ≥ 4), `+0xB8A3` (level ≤ 0) set in think window | raw | CONFIRMS com-styles:222; extra-frontman:183 | raw |
| M4 | 2026-10-18 | A | Level store to `+0xB8A4` at `0x1404F84BB` | raw | CONFIRMS tactics:375 (EV-TAC-0005) | raw |
| M5 | 2026-10-18 | A | `B8A3` read in `BallPlayer_DefensiveLineDepth` at `0x1405FA423` | (A) | NEW | - |
| M6 | 2026-10-19 | A | −2: floor switches off; support licences zero (only forwards) | (A) | NEW | - |
| M7 | 2026-10-19 | A | −1: two non-forwards licensed for attacks | (A) | NEW | - |
| M8 | 2026-10-18 | A | Level-scaled line drop factor = attack level × 0.25 | (A) | NEW | - |
| M9 | 2026-10-18 | A | Auditor's OA gradient for pull-back — RETRACTED same day | - | not imported | - |
| M10 | 2026-10-17 | C | "Dial" concept: −1 is modest pull; −2 is full drop | community summary | not checked | - |

### 4.4 Defensive block, forward protection and line depth

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| D1 | 2026-10-18 | A | One protected forward per team: last live by default, first on counter-target or ≤1 forward | dec 0x140682770 (A) | PARTIAL (F8) | dec |
| D2 | 2026-10-18 | A | Other forwards: level-scaled drop; ×0.65 / ×0.2 extras with 3+ forwards | (A) | NEW | - |
| D3 | 2026-10-18 | A | Style extra pull +7.0 at `0x1405E7AD1`; exempt Fox and Hole Player | (A) | CONTRADICTS constant (C6) | - |
| D4 | 2026-10-18 | A | Protected forward gets floor protection while level ≠ 0 | (A) | NEW | - |
| D5 | 2026-10-18 | A | Pair-rank via `FUN_1406405C0` with a per-slot rank byte vs group count | dec (call sites seen) | NEW | offsets not verified |
| D6 | 2026-10-17 | A | Man Marking refresh and pressure gate DA ≥ 75 (`sub_1409677C0`) | (A) | PARTIAL | - |
| D7 | 2026-10-17 | A | Widening: +5.0 for DA < 65 when ball ≤ 36 m from goal line | dec 1405C0630 | PARTIAL anim-select-2:348 | dec |
| D8 | 2026-10-17 | A | Chaser write: `table[cand] % 11` from MI+0x24840 (0x58 stride), gated on `+0x1775` ∈ {3, 8..10} | dec 140536210 | NEW | dec |
| D9 | 2026-10-18 | A | Chaser write state 0x3A (MATCH_UP) | dec | CONFIRMS defensive-shape:168; defender-decisions:276 | dec |
| D10 | 2026-10-18 | A | Early-cross receiver: height-only ranking, roster-order ties | (A) | NEW (F11) | - |
| D11 | 2026-10-17 | A | "Low DA CBs dragged to ball" from farther out | (A) | PARTIAL | - |

### 4.5 Designation, keeper, lineup and role arrays

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| R1 | 2026-10-17 | A | `GetDesignatedAttacker` = first slot with array-B 0, else 0xFF | dec | CONFIRMS mechanism; CONTRADICTS label (F2) | dec |
| R2 | 2026-10-18 | A | Role-array writer: `sub_140A59F90` + `425A0` / `42590` | dec | NEW (F1) | dec |
| R3 | 2026-10-18 | A | Remap enum 0 GK, 1 CB, 2 FB, 3-6 MF, 7/8 FW, 9 CF (card mode) | dec switch | PARTIAL stamina-subs:26 | dec |
| R4 | 2026-10-18 | A | Designation list `+0x8924/+0x8928` rebuilt each tick by `sub_1404F6C30` | (A) | NEW (F9) | - |
| R5 | 2026-10-18 | A | 2-CF designator `sub_140501160` runs when formation flag `+0x255` set | (A) | NEW | - |
| R6 | 2026-10-18 | A | Goal Poacher pairing group 7 (anchor style 0/1 with style-9 partner) | (A) | NEW | - |
| R7 | 2026-10-17 | A | Lineup pick (`sub_1404A7D60`): +15.0 for role {5,8}, −12.0 if |z| < 18 | dec constants | CONFIRMS positioning-phases:22 | dec |
| R8 | 2026-10-17 | A | Picker stats considered: none | (A) | not verified (PARTIAL) | - |
| R9 | 2026-10-17 | A | Sort: descending 3-way compare, top entry wins; phase-2 excludes winner | (A) | CONFIRMS off-ball:77 (qsort desc) | - |
| R10 | 2026-10-17 | A | Lineup slot array: stable; subs and formation changes remap; red cards mark unusable | (A) | NEW | - |
| R11 | 2026-10-17 | A | Save-load: squad maps into slots at load | (A) | NEW | - |
| R12 | 2026-10-17 | C | "Array-B = job tag; slot 0 = designated attacker" | community | CONTRADICTS (F2) | - |

### 4.6 Kicks, chips, PK and kick kinds

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| K1 | 2026-10-18 | A | Kick-kind 54 identity unresolved (ANIME_PENALTYKICK retracted) | - | OPEN | - |
| K2 | 2026-10-18 | A | Kick calc `FUN_1406B4000`: stat weights 0.6 and 0.4; ramp 40..99 | dec + raw | NEW (F4) | dec |
| K3 | 2026-10-17 | A | Panenka maps to kind 54 (inference) | - | OPEN | - |
| K4 | 2026-10-17 | A | Two other chip sites `0x140932251`, `0x1409325A6` read bit 31 of the action-state dword, not the skill mask | (A) | NEW | - |

### 4.7 Passing, long ball, crosses and set pieces

| ID | date | src | claim | evidence | status | my check |
|---|---|---|---|---|---|---|
| S1 | 2026-10-18 | A | Early cross picker: quality screen (height dominance, heading/finishing > 90, physical+jump > 170, spacing ≥ 64) | (A) | NEW | - |
| S2 | 2026-10-18 | A | Early cross screen skipped for Pinpoint (bit 14), early-cross COM (bit 3), centring | (A) | NEW | - |
| S3 | 2026-10-18 | A | Set-piece designated takers `T+0x8E08` point at keeper in this install | - | CONFIRMS setpiece:516 | - |
| S4 | 2026-10-17 | C | Corners: curl direction depends on taker footedness | hearsay (PES16) | not checked | - |
| S5 | 2026-10-17 | C | Corner: "players to join attack" 1st left post, 2nd centre, 3rd far post | hearsay (PES16) | not checked | - |

### 4.8 Community hearsay (summaries; no VAs)

About 90 technical rows across the five passes. Common themes: COM style stacking (PES16 to PES21), position and playstyle activation (grey = inactive), weather and grass (rain, wet grass), defensive line and compactness, formation and width. None carries an address. None is checked against the exe. They are useful as a list of community beliefs to test with a live capture, not as sources.

## 5. Questions put to the auditor and their answers

60 records (reply to or @-mention of the auditor, with a question or request). Paraphrased. "Answer" = first auditor post that answers the question (date, 2026). Status: ANS = answered; PART = partly answered; RET = answer later retracted; SUP = superseded by a follow-up; META = not technical; NONE = unanswered.

| # | date | question (paraphrase) | answer (date) | status |
|---|---|---|---|---|
| 1 | 10-16 | Can I DM documents? | 10-16 (yes, DM) | META |
| 2 | 10-17 | What does Man Marking do? | 10-17 15:17 | ANS |
| 3 | 10-17 | Low DA near goal vs CB chaser; DA bonus/decrease in defprow | 10-17 15:03/15:04 | ANS |
| 4 | 10-17 | Does MM relate to the skill card? | 10-17 15:17 | ANS |
| 5 | 10-17 | EF: only 0.2? rest of designated-attacker score; stats? | 10-17 15:38 (answer says no 0.2) | RET (F3: 0.2 exists, class-gated) |
| 6 | 10-17 | What does Chip Shot Control do? | 10-17 15:47 | ANS |
| 7 | 10-17 | Can other players be selected for attacker role (EF-only)? | 10-17 15:49 | ANS |
| 8 | 10-17 | PK Specialist bonus to chip accuracy? | 10-17 15:49 | ANS (partly, kind-54 open) |
| 9 | 10-17 | MM value on FWD / MF / DEF | 10-17 15:55 | ANS |
| 10 | 10-17 | How often is CMF/DMF picked as chaser? | 10-17 16:10 | ANS |
| 11 | 10-17 | MM + Track Back on same player: conflict? | 10-17 16:11 | ANS (no conflict) |
| 12 | 10-17 | Expand Tight Marking instruction and MM | 10-17 16:14 | PART (slider field not traced) |
| 13 | 10-17 | What does Step On Skill Control do? | 10-17 16:57 | ANS (no consumer) |
| 14 | 10-17 | Destroyer + MM: fouls, conflicts? | 10-17 16:46 | ANS |
| 15 | 10-17 | Clone repo and surface report on docs/re | 10-17 17:04 | ANS |
| 16 | 10-17 | FB advanced instructions with no FBs; wide instructions with no wide | 10-17 17:37 | ANS |
| 17 | 10-17 | Reply when free (no question) | none | META |
| 18 | 10-17 | What counts as a "narrow formation"? | 10-17 17:47 | ANS |
| 19 | 10-17 | FB advanced instructions are attacking/false FB, not FB playstyles | 10-17 17:50 | ANS (corrected) |
| 20 | 10-17 | What does Low Lofted Pass do? | 10-17 18:06 | ANS |
| 21 | 10-17 | Does False 9 work with <3 forwards (e.g. 4-2-2-2)? | 10-17 18:13 (per-slot); 10-18 06:43 (2 CF works) | ANS |
| 22 | 10-17 | "Low lofted weighting skips slots 4-9": meaning? | 10-17 18:26 | ANS |
| 23 | 10-17 | Roster-slot order in `sub_142125BB0` | 10-17 18:53 | ANS |
| 24 | 10-17 | Orchestrator pastebin (external) | 10-18 05:35 (repo/propagation items) | PART (no technical answer to the pastebin) |
| 25 | 10-17 | What changes the lineup array after load? | 10-18 05:37 | ANS |
| 26 | 10-17 | PK Specialist only helps chip when kind 54? | 10-18 05:37 | ANS (scoped to kind 54) |
| 27 | 10-18 | Find out what kick 54 is | 10-18 05:56 (ANIME_PENALTYKICK) | RET (16:22; kind table unpinned) |
| 28 | 10-18 | List all actions on the action-kind table | 10-18 06:02 | ANS (94 entries) |
| 29 | 10-18 | Every advanced instruction, position calls, EXE code | 10-18 06:43 | ANS (18, byte-verified) |
| 30 | 10-18 | Does changing preset remap slots? | 10-18 06:09 | ANS (structure stable; tags open) |
| 31 | 10-18 | "Are you working on that?" | 10-18 06:30 | META |
| 32 | 10-18 | "Do one at a time" (request) | 10-18 06:30 | META |
| 33 | 10-18 | Elaborate on 17: deadcode? | 10-18 06:51–06:52 | ANS (vestigial; intent open) |
| 34 | 10-18 | What are 11 and 12 called? | superseded by 35 | SUP |
| 35 | 10-18 | Explain "long-pass flick suppressed" (0x1405465B0:44) | 10-18 07:09 | ANS |
| 36 | 10-18 | Are advanced tactic names named constants in the EXE? | 10-18 07:09 | ANS (yes; enum 0x14261433C) |
| 37 | 10-18 | Thanks; answer (pastebin, dispositions) | 10-18 07:26 | META |
| 38 | 10-18 | Expand on Outside Curler mechanism | 10-18 11:36 | ANS (no consumer) |
| 39 | 10-18 | Do No Look Pass and Rabona improve COM passing? | 10-18 16:28, 17:25, 17:37 | ANS (no consumer) |
| 40 | 10-18 | Duplicate of 39 with extra line | 10-18 17:25–17:37 | ANS |
| 41 | 10-18 | Chip Shot Control only for penalty kicks? | 10-18 16:22 | RET (kind unpinned) |
| 42 | 10-18 | Is Heading a defensive card or only scoring? | 10-18 17:47 | ANS (shoot +5; sweep not complete) |
| 43 | 10-18 | Speeding Bullet effect; does it activate CBs? | 10-18 17:50 | ANS |
| 44 | 10-18 | SB and DT together? | 10-18 17:59 | RET (18:07: override, not penalty) |
| 45 | 10-18 | Long Ball Expert on a forward? | 10-18 18:01 | ANS |
| 46 | 10-18 | Which strikers pull back at -1? | 10-18 18:19 | ANS (later corrected 22:28) |
| 47 | 10-18 | Classic No. 10: anything besides positioning limits? | 10-18 18:27 | RET (18:39: veto) |
| 48 | 10-18 | Check point 2 on the dummy-runner section | 10-18 18:39 | ANS (conceded) |
| 49 | 10-18 | Creative Playmaker near Goal Poacher? | 10-18 18:49 | PART (values not given) |
| 50 | 10-18 | Follow-up: which strikers press before dropping at -1? | 10-18 18:50 | ANS |
| 51 | 10-18 | Is one striker still pulled back if higher DA? | none (OA answer retracted 19:05) | NONE |
| 52 | 10-18 | Are condition rolls done by lineup order? | 10-18 19:05 | ANS (per-player rolls) |
| 53 | 10-18 | Trace the forward-drop eligibility test | 10-18 19:44 (then 22:28) | ANS (F8) |
| 54 | 10-18 | "Still working on this?" | 10-18 20:59 | META |
| 55 | 10-18 | "How much longer?" | 10-18 21:46 | META |
| 56 | 10-18 | Does it pick first eligible player by roster order? | 10-18 22:43 (last eligible by default) | ANS (partly; see F8) |
| 57 | 10-18 | Summarise in plain language | 10-19 04:07 | ANS |
| 58 | 10-19 | "Deep-lying forward" is not a playstyle: what do you mean? | 10-19 04:50 | ANS (FOX_IN_THE_BOX) |
| 59 | 10-19 | Elaborate on the -2 code segment | 10-19 04:50 | ANS (partly; B8A3 reader NEW) |

Unanswered (NONE): #51. Superseded: #34. Meta: #1, #17, #31, #32, #37, #54, #55.

## 6. Docs that need correction (do NOT edit here; list only)

Each row: doc, line, old text, proposed text, evidence.

- **C1 (PARTIAL)** `docs/re/pes-re-extra-frontman.md:107-109`. Old: "+0.2 if bit 15 (L62–65, `0x142598578` = 0.2, live)". Proposed: "+0.2 if the candidate holds bit 15 of the playstyle dword (`+0x48`) **and** bit 15 of `DAT_142613D20[B[slot]]`, which is set only for class 1 (CB). Gate via `FUN_140A64030(slot,0xF,…)` → `FUN_1408CB0B0`." Evidence: dec `FUN_1405FC860`, `FUN_140A64030`, `FUN_1408CB0B0`, table decode.
- **C2 (CONTRADICTS)** `docs/re/pes-re-tactics.md:236-240`. Old: "exactly ids 8, 0xa, 0xe, 0xf are always tested with a player slot". Proposed: "ids 8, 0xa, 0xe, 0xf are always per-slot; ids 4, 0xb, 0xc, 0xd also have per-slot sites (auditor census 3/3/4/6, not re-verified)." Evidence: auditor census only.
- **C3 (UPGRADE)** `docs/re/pes-re-tactics.md:239`. Old: "17 DEFENCE_MAX sentinel (INF high)". Proposed: "17 DEFENCE_MAX: OBS sentinel. Tester `FUN_140A63950` rejects id ≥ 0x11 (`cmp r8d,0x11`); setter `FUN_140A42600` returns on 0x11." Evidence: dec both.
- **C4 (CONTRADICTS, index spaces)** `docs/re/pes-re-matchlogic.md:93-95` vs `:655`. Old (93, 95): "0x18 Finishing", "0x1a Place Kicking". Old (655): "in-match stat indices 0x1a–0x1d are Finishing, Low Pass, Lofted Pass, Heading". Proposed: state which index space each table uses; for in-match bytes say 0x1A = Finishing; the PK in-match index is OPEN (chip calc reads `+0x20` at weight 0.6). Evidence: contact §3.3 (Heading = 0x1d), attack-leftovers:844 (`pms+0x70+0x1a` = Finishing), chip calc dec (weights at `+0x20`, `+0x1A`).
- **C5 (CLARIFY — SUPERSEDED 19-10-26 by pes-re-role-writer.md §6: do NOT apply; both branches use the same map)** ~~`docs/re/pes-re-stamina-subs.md:26`. Old: "role class 0..9 = FUN_1408c51f0 map ... CF9" (raw map). Proposed: add "the remapped output (`FUN_140A59F90` with the card-mode flag) uses 0 GK, 1 CB, 2 FB, 3-6 MF, 7/8 FW, 9 CF; the raw map applies when the flag is off. Which branch the live writer uses is OPEN." Evidence: dec `FUN_140A59F90` switch; `FUN_1408C51F0` dispatch.~~
- **C6 (CONTRADICTS, constant)** Not one of our docs; the auditor's "+= 7.0 (flt_14259BDF4)" is wrong: the constant is 8.0 (raw bytes `00 00 00 41`). Our docs `pes-re-attack-decisions.md:134`, `:268`, `:387` correctly say 8.0. Keep 8.0.
- **C7 (PARTIAL)** `docs/re/pes-re-team-concepts-port.md:58`. Old: "their in-match writer is unfound". Proposed: "The team-sheet writer is `FUN_140A42600` (refuses id 0x11); the in-match write path is still OPEN." Evidence: dec 0x140A42600.
- **C8 (CONFIRMS — CLOSED 19-10-26 on static by pes-re-role-writer.md §6; prior text kept for the record)** `docs/re/pes-re-lane-core.md:214-222` (§5.1). Old: "category→keeper mapping OPEN (O1)". Proposed: keep OPEN until the live writer is checked; add "auditor and class map agree: array-B 0 = GK family". Evidence: dec `FUN_140A62690` / `FUN_140A62610`; `FUN_140A59F90` switch.
- **C9 (NEW content)** `docs/re/pes-re-defensive-shape.md:168`. Add: "the MATCH_UP chaser write (`FUN_140536210`) is gated on team byte `+0x1775` ∈ {3, 8..10} and picks slot `table[candidate] % 11`". Evidence: dec.
- **C10 (NEW content)** `docs/re/pes-re-anim-select-2.md:348` (AL-04). Add: "the widening judge `FUN_1405C0630` widens by +5.0 for DA < 65 when the ball is ≤ 36 m from goal; Track Back (bit 23 of `+0x40`) zeros the term." Evidence: dec + raw constants.

No doc claim is contradicted by the community. Community claims are hearsay and none carries a VA.

## 7. Verification log

Exe checks run by me. Ghidra bridge (dec) = `127.0.0.1:8080/decompile_function`; raw = `pes_full.bin` via capstone.

- dec `0x140A48A30` (team slot getter), `0x140A62690`, `0x140A62610` (keeper / designated lookup).
- dec `0x140A59F90`, `0x140A425A0`, `0x140A42590` (role-array writer and stores).
- dec `0x1408C51F0` (class dispatch; jump table not decoded further).
- dec `0x140A63950` (tester), `0x140A42600` (setter).
- dec `0x1406B4000` (kick-scatter calc, kind 0x36 branch); raw constants `0x14257A190`, `0x14259B89C`, `0x142565600`, `0x142598578`, `0x14259BE2C`, `0x1425C48CC`, `0x142572348`, `0x142590D98`, `0x142588200`.
- dec `0x140A64030`, `0x1408CB0B0`; raw table `0x142613D20` (classes 0–11: class 1 = `0xD000`, bit 15 set only there).
- dec `0x1405FC860` (aerial taker, +0.2 gate).
- dec `0x1405C0630` (widening judge, TB skip at bit 23); raw `0x14259BE38` = 55, `0x14259BE48` = 65, `0x14259BE84` = −10, `0x1425A9464` = 36, `0x14259222C` = 5.
- dec `0x140628400` (mark-spot cadence immediates 0x60, 0x5B, 0x51, 0x46).
- dec `0x140536210` (chaser write gate and `% 11` slot).
- dec `0x140682770` (defensive block, forward loop comparison; per-slot value and ties).
- dec `0x140A63680` (FALSE_9 helper).
- dec `0x1404A7D60` (picker: constants `0x14259BE0C` = 15.0, `0x14259D664` = −12.0, `0x14259BE18` = 18.0 used).
- raw `0x1404F6DBE` (copy to `+0x134`), `0x1404F6E1C`–`0x1404F6E58` (flags `+0xB8A2`/`+0xB8A3`), `0x1404F84BB` (`+0xB8A4` store), `0x14259BDF4` = 8.0.
- Not verified by me: all "zero xref" claims (bridge xref endpoint 404); `0x142125BB0` (early cross); `0x1405E7AD1`; `0x1405FA423`; `0x14078E9B4`, `0x14096FEC5`, `0x140971F85`; kick-kind table (kind 54); the −2 floor; `sub_1409677C0`; the designation-list writer; the Chip / LBE / SB ImageUnit calls.

## 8. Retractions by the auditor (do not import)

- Kick 54 = ANIME_PENALTYKICK (retracted 2026-10-18: wrong enum; the kick-kind enum is unpinned).
- Chip Shot Control = penalty-only (retracted: the kind is unpinned).
- SB+DT "−50 penalty" (retracted: priority override only).
- Destroyer "zero direct readers" (retracted: four sites found).
- Classic No. 10 "three effects" (corrected: two effects; diagonal veto).
- "OA gradient" pulls the lower-OA striker back (retracted 2026-10-18).
- "First eligible forward" for protection (corrected: last eligible by default, ties).
- "Designated attacker = EF only" (corrected: no; it is array-B 0 = keeper family, see F2).
- Offset slips: `0x8904/0x8908` → `0x8924/0x8928`; `+0xBC8/+0x308` → `+0x134`; `+0x87EC` vs `+0x87CC` (auditor's own inconsistency; decompile reads `+0x87CC`).
- Auditor's "+7.0" constant (wrong; 8.0).

## 9. Next checks (for the main agent)

1. Read the live caller of `FUN_140A59F90` and `param_4` (card-mode flag) to settle C5 and the A vs B array distinction (F1).
2. Verify the 2-CF designator and the `+0x8924` writer (F9) before porting any designation logic.
3. Decode the kick-kind table entry 54 (F4); until then, do not state which kicks the chip card covers.
4. Live watchpoint on the skill mask for the four negative cards (F12), if the league needs them.
5. Verify the `+0x1775` gate on the chaser write (F6) in a live match.
