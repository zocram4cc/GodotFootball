# PES re-crawl audit: CrawlFix branch (CrawlAudit, 18-11-26)

**Assignment.** Audit branch CrawlFix (2029c733; report `docs/ports/pes-port-CrawlFix.md` on main) before Main lands it. Scope owner ruling: in = everything a player goes through to play a match; out = edit mode, career modes, save/load, online.

**Method.** Static only. Compared branch `tools/dev/ledger/universe.json` against base `21b7f824` (CrawlFix parent = CrawlGap doc commit; identical to origin/main's universe). No recrawl: `universe.py` has no fix toggles (only `--image/--out/--max`), and each toggle would cost a full bridge crawl plus fresh bridge variance — so attribution uses the generated rows' own discovery chains (each row's `path` = BFS discovery chain; parent edge kind from the parent's `callees`). This is exact for discovery cause. Spot-verified machine code from `pes_full.bin` (flat, offset = VA − 0x140000000) with capstone 5.0.7. No bridge calls needed. Ghidra MCP untouched.

**Headline reconciliation.** Branch report numbers (5,894 new / 1,013 moves) are one regen step stale: history shows a final `25739 → 26120 (+381)` regen after they were computed (25 of the moves are `out: online`, matching the +25 gap). Exact committed-state numbers (mine):

| | base | branch | Δ |
|---|---|---|---|
| universe rows | 38,338 | 44,985 | **+6,647** (6,689 new keys − 42 gone; base meta `n_in` 18,374 is stale, the functions dict already held 18,857) |
| in scope | 18,857 | 26,120 | **+7,263 = 6,265 new-in + 1,038 out→in moves − 40 gone-in-scope** |
| out scope | 19,481 | 18,865 | −616 (424 new-out + 40 gone-in + 1,038 moves − 2 gone-out... net) |
| table jumps resolved | 836 | 1,240 (report says 1,231 — also stale by the last regen) | +404 |
| dtable rows | 0 | 54 | +54 (report ✓) |
| sweep requeues | — | 6 (3 in + 3 out = saturated) | (report ✓) |
| capstone rows | 5,719 | 6,660 | +941 |

Moves by base out-category: online 687 / career 291 / edit 55 / save-load 5 (report's 663/290/55/5 = pre-final-regen values).

## 1. Per-fix attribution (unique, discovery-chain; sums verified)

New-in 6,265: SEED 4,450 · DTABLE 1,149 · WALK 297 · JUMPTBL 69 · SWEEP 290 · OLDROOT 10.
Moves 1,038: SEED 229 · DTABLE 354 · SWEEP 445 · WALK 10 (all inherited deep in fix-created subtrees; direct-from-old-row moves ≈ 0 in every class — the report's "0 dtable-driven moves" is true for direct edges, false for subtree footprint; §3).

- **Jump-table bound (bare-hex CMP).** All 69 JUMPTBL rows hang off `table` edges whose base parent lacked the edge (36 parents had 0 tables in base, 33 had 1 — the fix adds a second bare-hex table, e.g. `1405B0620` now `tables=2`). W2 flagship CONFIRMED: `141A89120` `tables=1`, callees contain `141A874A0` (caller list: `141A89120` → `141A874A0`). Wart (§4, item 3).
- **Dtable (LEA-run, MAX 64).** 54 holders (16 animation, 7 in-match menu, rest incl. 19 out-crawl holders), 1,495 dtable edges, 391 distinct targets (340 in-universe, all resolved; 173 bridge entries + 167 capstone interiors; 51 targets rowless — sweep covers only direct/tail, §4 item 4). Precision finding: the flagship `0x142BEEDB0` carrier block does **not** fire — image bytes show data pointers (`0x142E0D498…`) interleaved, breaking the ≥3 code-pointer run (`runs_of`), and holder `14210D320` has `dtables: []`. The 5 W3 slots (`142121920` et al) are in via **`vtable`** edges from relisted `14210D320` (base source `pdata` → branch `bridge`), not via dtable. The rule's real yield comes from cleaner runs (anim `1421xxxx`, menu `1401xxxx`, crowd).
- **Sweep (missing_callees + ≤3 refills).** 290 new-in + 445-move footprint are alias/order restorations: sample chains show `?` edges (parent lists queued VA `V`, row keyed by entry `E` ≠ `V` after `alias`), i.e. rows the base crawl swallowed into visited containers. Not bridge-hiccup miracles, not new edges — the same old match edges, newly rowed. Saturated 6/6.
- **Seed decode.** 12 of the 35 new seeds are capstone-sourced (`140402B90 1404187FF 1406F8518 140796E7F 1408398D5 1414F6C83 1416C7E12 141FCEB1B 1421339C8 142135E84 142135EAF 142136F4E`); all 35 seeds walked.
- **Boundary walks.** 297 new-in + 10 moves, **all cutscene**. Fan-out audited: `141EC4560` 24 direct + 5 table (18 cutscene + gameplay/AI/replay/restart/ball), `141ED4310` 3 direct + 1 tail, `141AFB090` 64 direct + 12 vtable (cutscene/animation/pre-match/gameplay/menu/AI). No draw/gr internals walked.
- **35 seeds.** Footprint 4,450 + 229. Largest: `1404187FF` 3,324 (3,317 AI + 7 boundary-render, 0 library-named walked), `1414F6C83` 347, `141FD5200` 209, `1409224D0` 155, `141FCEB1B` 135, `1419112B0` 102, `140402B90` 91. All 10 allow-listed OFFPATH VAs (`14140EA40`, `141FE3830/9B00/9F40/A000/A250`, `141FC0AD0/C40`, `140A57B00`, `14213A180`) verified **ABSENT** from the universe — the edit/save cluster the seeds approach but never enter.
- **OLDROOT 10.** Pre-existing vtable-edge reaches, order-luck rowed. Residual, match.

## 2. Samples (70 new-in + 30 moves; chains + ports/decode in scratch `samples_{new,moves}.txt`)

Stratified by fix × subsystem (SEED 42 incl. every seed group; DTABLE 10 anim/crowd/menu; WALK 6; JUMPTBL 4; SWEEP 8 ball/gameplay/AI/menu/referee; moves SEED 7/DTABLE 10/SWEEP 12/WALK 1). Representative paths:

- SEED: `1404187FF→140A35A00→…→1407F7030-[vtable]→1407FD680→1406EEB90`; `1414F6C83→1414F3780→1414FCE30`; `140796E7F→14079E5C0(trap-end handler)→14079F440→14079F560`; `142135EAF→142137460→142135430→[tail]→thunk`.
- DTABLE: `1406E5FC0→14080D210→140809070→[dtable]→1422ECD28…`; four independent dtable holders converge on `1401158D0/140115870` (real `sub rsp,28` prologues, bridge-missing entries — legit).
- WALK: `…→141AFB090-[direct]→141B15F00→141B158F0→141B27D70→141B28C70`; `…→141EC4560-[vtable]→141A02E60→141A03AF0`.
- JUMPTBL: `1405B0620→[table]→1405B1073` (real `mov r14d,4…` chunk); `141511070→[table]→14146D217` (verified mid-chunk: `adc/lea/cmp/setbe…jmp 14146CF8F`, W2-pattern fallthrough).
- SWEEP/moves: `14041ECF0→1405069A0→…→140B02120-[?]→140B00ED0` (alias-entry, old edge).

**Bleed per fix: 0 everywhere (0/100 samples).** No edit/career/save/online rows; zero walked rows with library names; the 7 `render` rows are boundary `rtti:draw`, unwalked (correct engine). Suspects checked and cleared: `1414C1980` (ball leaf in the `1414C` range) is a real `movzx/ret` getter among 184 in-scope (non-edit) `1414C` rows; `141FEAFD0` (pre-match, near the edit cluster) is a real `shr/mov/ret` leaf among in-scope pre-match `141FEA/B` rows; `1404031D0` is a genuine import-thunk (`jmp 0x143EC2780`, impdata range).

## 3. Non-bleed defects (count-affecting, not path-affecting)

1. **94 out-of-code unresolved rows** (new-in 79: SEED 63 / DTABLE 10 / WALK 3 / SWEEP 3; moves 15, mostly SWEEP): `0x15Bxxxxxx` wild targets of 1-insn thunks (e.g. `142135430→15B6EFA70`) and `0x143x–0x144x` import-range targets. Real caller edges, junk targets. **Drop the rows** (keep edges).
2. **2 INT3-padding rows** (`140C8C969` JUMPTBL — table target lands 7 bytes early in padding before `140C8C970`, which stays rowless; `141050505` SEED). Filter INT3-leading capstone rows.
3. **40 gone-in-scope rows** (all base-walked; 39 callerless in branch): bridge-listing variance, the class sweep cannot recover (caller itself gone). Re-crawl recovers most; within the report's own ±250 inter-run variance.
4. **51 dtable targets rowless** (queued, alias-swallowed; sweep covers direct/tail only). Coverage gap — extend `missing_callees` to dtable/table kinds or accept.
5. Doc precision: W3 flagship `142BEE` block doesn't fire (mixed run); slots arrived via vtable + relisting. Report's "0 dtable-driven" needs the direct-vs-subtree qualifier (direct 0, subtree 354 moves + 1,149 new-in).

## 4. Verdicts and corrected count

- Jump-table bound: **land as is** (1 INT3 wart, no correction needed).
- Dtable: **land with named correction C1+C2** (C1: don't row out-of-code targets; C2: fix the W3 flagship claim; optional C3: sweep covers dtable/table kinds).
- Sweep: **land as is** (saturated 6/6 is by design; note the caller-gone limitation).
- Seed decode: **land as is**. Boundary walks: **land as is**. 35 seeds: **land as is**.

**Corrected expected in-scope: 26,120 − 94 − 2 = 26,024.** (Re-crawl expectation ≈ 26,024 + ~40 variance returns ± 250.)

Scratch: `<workdrive>/gf-CrawlAudit-scratch/` (`attribute.py`, `sample.py`, `attribution.json`, `samples_{new,moves}.{json,txt}`, `base/main-universe.json`). Worktree `<workdrive>/gf-CrawlAudit-work` (detached 2029c733, read-only use). No builds, no gate (tooling-only branch; Main gates).
