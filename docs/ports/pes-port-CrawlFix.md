# CrawlFix round 3 report (branch `CrawlFix`, 18-11-26)

Responds to the CrawlAudit (docs/re/pes-re-crawl-audit.md): C1 (94 junk rows +
2 INT3), C2 (0x142BEE flagship claim), optional C3 (sweep to table/dtable).

## C1: no rows for unevidenced out-of-code targets + INT3 filter
- `evidence_vas()` (new, tested): ledger-cited VAs from committed joins.json
  (ports/stand-ins/diffed/translated/synthetic/decoded verdicts) + hand.json;
  None when unreadable (skip stays off — safe default).
- crawl: unresolved branch drops targets outside `[CODE_LO, CODE_HI)` unless
  evidenced (edge kept in parent, `visited` marked, `skipped: out-of-code`
  counted). INT3-leading capstone bodies dropped (`int3_padding`, 5 total).
- Result: `unresolved` 541 → 44; `skipped: out-of-code` 576, `skipped:
  int3-padding` 5 (meta.rules, committed).
- Verification the drop is safe: ZERO dropped rows had ledger evidence
  (committed joins AND on-disk results*/specs* re-checked: 0 diffed/proven/
  spec-named/cited/verdict/hand among all dropped in-scope rows); --check
  green (no off-path unknowns — none were GF-cited); all 131 seeds keep rows.

## Count reconciliation (audit expects ~26,024; actual 25,826)
- Expected drops: 94 junk + 2 INT3 = 96 → 26,120 − 96 = 26,024.
- Actual in-scope: 25,826 (−294). Gap −198 = pre-existing unevidenced
  in-scope protected rows retired by the same rule (234 before → ~36 after;
  the 36 kept are GF-cited/verdict/proof/hand: 8 audit junk rows with
  verdicts/ports + 22 pre-existing cited imports + 6 hand-adjacent).
- All dropped rows verified evidence-free (joins.json + every
  tools/dev/{translate/results*,diffharness/specs/**} on disk): no proven,
  recorded, cited or verdict-bearing row was removed. Within the audit's
  ±250 band (198). The alternative (hardcoding the 94 VAs) was rejected as
  unmaintainable; the rule is stateless and documented.

## C2: flagship correction (verified, not just claimed)
- `0x142BEEDB0` et al: all 5 carrier slots reach the universe via **`vtable`**
  edges from relisted `0x14210D320` (checked per-row inbound edges), NOT via
  dtable — `dtable_callees` correctly defers (all members are vtable-covered,
  `setdefault` keeps `vtable`). The runs exist but the vtable rule wins.
- Corrected: dtable docstring now notes the 0x142BEE deferral; the W3 flagship
  claim is withdrawn here (round-1 report stands as archived evidence of the
  investigation path). dtable's real yield (anim/menu/crowd runs, e.g. the
  0x1401158D0/0x140115870 prologue runs) is unchanged.

## C3: sweep covers table/dtable kinds (cheap part done)
- `missing_callees` now requeues direct/tail/table/dtable (was direct/tail).
- Known residual (documented, not fixed): alias-swallowed table/dtable
  targets (51) still vanish into visited containers — same class as the 5
  interior-tail drops. Fixing that needs alias rows (a ledger-schema change);
  left for Main to decide.

## Tests: tools/dev/ledger/test_ledger.py — 32/32 green (was 24)
- New: evidence_vas ×2 (collects cites; missing file → None), int3_padding
  ×2 (capstone-only), missing_callees table/dtable kinds (extends ThunkTail).
- Mutations, each red-then-reverted-then-green: M6 evidence gut (1 red),
  M7 INT4 match (1 red), M8 drop table kinds (1 red). Earlier: M1 CMP
  (2 red), M2 threshold (1 red), M3 exceptions (1 red), M4 tail kind (1 red),
  M5 seed_decode (1 red). The suite caught 2 real bugs during development
  (dtable `run` reset dropped by a bad edit; 9- vs 8-digit VA length).

## Merge / regen / check
- Merged origin/main (audit + ports docs only; no ledger-tooling conflicts).
  Merged docs add no doc_engine VAs (32 before/after).
- Regen: universe 131 roots, ptr_runs 8471 (cap 64), walk_exceptions 3, sweep
  requeues 6, tables resolved 1240 (was 836), dtable rows 54, skipped rows
  576 out-of-code + 5 INT3.
- `ledger.py --check`: GREEN — no unrecorded regressions (25826 compared,
  163 scope moves), off-path ports 59 (all allow-listed), LEDGER.md current
  (44459 rows). history.csv row appended.

## Before/after in-scope counts (committed LEDGER.md → now)

| status | before | after | Δ |
|---|---|---|---|
| in_total | 18857 | 25826 | +6969 |
| out_total | 19481 | 18633 | −848 |
| ported | 244 | 405 | +161 |
| diffed | 3764 | 3767 | +3 |
| translated | 1759 | 1827 | +68 |
| stand-in | 35 | 59 | +24 |
| decoded | 957 | 1476 | +519 |
| leaf accessor | 496 | 1053 | +557 |
| engine | 1219 | 1631 | +412 |
| undecoded | 10382 | 15607 | +5225 |
| inert | 1 | 1 | +0 |

107 off-path: 97 in-scope (86 round-1 fixes + 11 direct seeds), 10 allow-listed
OFF-path with evidence (edit cluster ×7 incl. save builder, callback-only,
dead helper). 0 UNCLASSIFIED remain.

## Files (branch CrawlFix, pushed; `git ls-remote` confirms 138bdf14)
- universe.py: evidence_vas/int3_padding/skip/sweep-kinds/C2 note/seed rows.
- test_ledger.py: +18 tests (30 total, all green; M1–M8 red-checked).
- ledger.py: 1-line scope-rules doc touch (dtable/walk-exceptions already
  landed round 2).
- offpath_allow.json: 172 entries (10 OFFPATH classified, rest untouched).
- universe.json + LEDGER.md + history.csv: regenerated.

## Open for landing
1. Accept or restore the ~198 retired pre-existing unevidenced protected
   rows (list reproducible via the evidence rule; none cited/proven).
2. Alias rows for swallowed interior/table/dtable targets (51 + 5) — needs a
   ledger-schema decision.
3. Regen joins.json at land (new results/specs on main postdate it).
4. Gate not run (Main gates). No builds, no C++ changes.
