# PES re-crawl gap: the 107 off-path VAs (CrawlGap, 18-11-26)

**Objective.** Classify all 107 `UNCLASSIFIED` entries in `tools/dev/ledger/offpath_allow.json`
(real PES functions GF code cites that the doc-rooted in-scope crawl never reaches):
how PES reaches each (E8/E9 scan, vtable-slot refs, dispatch tables) up to an
in-scope or known out-of-scope root; propose exact crawl fixes + expected delta.
No ledger tooling edited (Main lands it).

**Method (static only, image `pes_full.bin`, flat map VA−0x140000000).**
Mapping control CONFIRMED (`f32 @0x14256AE1C = 1.0`). Byte scan of the whole
`.trace` code range: 806,990 E8 + 192,687 E9 sites; 843 E8 + 2 E9 hits against
the 107. **All 845 verified as instruction starts** by `.pdata`-anchored
capstone decode (function-begin → site, mnemonic+target match; 0 false
positives). `.rdata` 8-byte slot scan + u32-RVA table clustering + opcode-directed
RIP-relative ref scan (LEA/MOV/CALL/JMP `[RIP+disp]`, ±0x100 window). Iterative
caller ascent to fixpoint (5 levels, 470 verified up-edges). Ghidra bridge: 5 calls
(3× function lookup, 2× xrefs_to). Closure/delta computed over the verified
direct-edge graph + table-membership edges against committed `universe.json`.
Scripts: `<workdrive>/gf-CrawlGap-scratch/scan*.py`, outputs `scan*.json`,
`final_table.json`, `final_assign.json`.

## Counts (107)

| class | n | what |
|---|---|---|
| missing edge (crawl defect, walked in-scope contact) | 19 | W1 re-crawl drop 1, W2 reg-jump-table chunk 1, W3 LEA-table/vptr block 5 + downstream 12 |
| missing edge, boundary-policy-gated | 6 | direct/direct-transitive targets of 3 boundary rows the in-crawl refuses to expand |
| GF citing wrong VA | 1 | 0x14071D760 is a 5-byte JMP thunk → right VA 0x14071D770 |
| genuinely off-path | 2 | 0x140A57B00 (callback DATA-only), 0x14213A180 (zero refs anywhere) |
| missing root | 79 | 23 seeds below, each with why-match-path |

9 of the 107 are/were live rendezvous hooks (fired during real matches):
`0x140797CD0 0x14079E5C0 0x140829E80 0x1408360F0 0x1408364D0 0x1408371A0 0x140837670 0x140837FC0 0x142129590`
— match-path CONFIRMED by live fire, independent of the static graph.

No static path of any kind (direct, tail, slot, RVA-table, LEA-table) reaches any
of the 107 from an **out-of-scope** (edit/career/save/online) root either. The
absent caller subgraph (~330 fns: 107 + 196 verified direct callers + up-chain)
is closed except the contacts in §1–2.

## 1. Missing edge: three crawl defects (19 VAs, +267 rows, no new roots)

**W1 — dropped direct row (1 VA).** Committed row `1407A01C0`
(in/trapdribble/**walked**) lists `14079B3D0` in its `callees`, but no
`14079B3D0` row exists; the bridge lists `FUN_14079b3d0` at exactly that entry
(no alias). Re-crawl restores it. Same systematic class: **56** direct/tail edges
from walked in-scope rows to `.pdata` begins have no row (only this one hits the
107; the other 55 e.g. `1403FD360→1403FE0F0/1403FE6B0`, `14061CCC0→8 targets`
are non-107; 5 more are interior-tail aliases, 96 are bogus `table→` targets
outside code — a separate wart). Fix: re-run `universe.py`; add a ledger check
that every walked row's `direct`/`tail` callee has a row (or a recorded alias).

**W2 — register-relative jump table kills a capstone seed body (1 VA + 6).**
Seed `0x141A89120` (in/pre-match/walked, capstone-sourced, "bridge lists no
function") contains at `0x141A89574` a dispatch the crawl's `jump_tables()`
does not model (no `LEA Rb,[0x140000000]`, no CMP bound):
```
0x141A89569 mov edx,[r13+rax*4+0x1a8b5f8] / 0x141A89571 add rdx,r13 / 0x141A89574 jmp rdx
```
`closure()` terminates the path at the indirect JMP, so the fall-through chunk
(`0x141A89576…`, incl. verified `0x141A8966F: call 0x141A874A0`) is never walked.
The chunk holds **92** direct CALL targets (86 already in-scope, 6 absent +
`0x141A874A0`). Fix: model the `r13+Ri*4` absolute-table shape in `jump_tables()`,
or resume/split capstone bodies after indirect jumps. Delta: +7 rows.

**W3 — absolute dispatch tables without RTTI COL are invisible (17 VAs).**
The crawl expands only RTTI-validated vtables, but match code vtable-stores
plain pointer runs. Proof, walked `0x14210D320` (in/dribble/walked):
```
0x14210D455 lea rax,[rip+0xae1954]   ; = 0x142BEEDB0
0x14210D45C mov qword ptr [rbx],rax  ; vptr store into heap object
```
40 of 50 found tables are LEA-referenced from code; the `0x142BEE…` carrier
block is referenced from walked `0x14210D320` (7 tables). Fix: in `closure()`,
treat every RIP-relative ref (LEA/MOV/CALL/JMP) into an `.rdata`
absolute-pointer run like the vtable rule (edge kind e.g. `dtable`: add run
members as callees). Covers: slot members `0x142121920 0x142129590 0x14212A700
0x14212ABA0 0x14212AEB0` + verified downstream `0x1421225E0 0x1421226C0
0x142124950 0x142125140 0x142125C80 0x1421265C0 0x142126A80 0x142126B70
0x142127FF0 0x142128490 0x142128C10 0x142132660`.
Fix-W total (W1+W2+W3, fixpoint over verified edges): **+267 rows, 19 of the 107**.

## 2. Missing edge, boundary-policy-gated (6 VAs, +181 rows)

Three boundary rows the in-crawl will not expand are the only in-scope callers:
- `0x141EC4560` (in/render/**bnd rtti:draw**, 32 direct targets incl. `0x1414E03C0,
  0x141AD98B0, 0x141EC3A20`, transitively `0x141AD9220` via `0x141EC4143`)
- `0x141ED4310` (in/render/**bnd rtti:draw**, 4 targets incl. `0x141ED48F0`)
- `0x141AFB090` (in/cutscene/**bnd doc:pes-re-anim-blend.md**, 75 targets incl.
  `0x141B15F00`, transitively `0x141B158F0` — site `0x141B15F83` sits inside
  `0x141B15F00`'s body)
All verified by `.pdata`-anchored decode of the three boundary bodies. Fix:
owner decision — (a) walk these 3 boundary rows (name them exactly; delta
**+181 rows, 6/107**: `0x1414E03C0 0x141AD98B0 0x141AD9220 0x141ED48F0
0x141B15F00 0x141B158F0`), or (b) accept the 6 as off-path (engine/draw-driven
presentation). Pro reachable-during-match evidence: uniforms/kit/handshake code
runs every match; `0x141B15F00` is one E8 step from a doc-named anim-blend row.

## 3. Wrong VA (1) / genuinely off-path (2)

- `0x14071D760`: first bytes `jmp 0x14071D770` — a thunk. GF should cite
  **0x14071D770** (`.pdata` begin, bridge-listed). Crawl fix: record JMP-tail
  edges out of thunk bodies. The right VA is covered by the K-cluster root
  (§4, `0x1408398D5` reaches the thunk; tail fix extends to the target).
- `0x140A57B00` (off-path): **no CALL/JMP/slot/RVA ref anywhere**; only two DATA
  refs, both `lea r9,[rip→fn]` callback-argument loads in absent template twins
  (`0x14042F374` in `FUN_14042f2d0`, `0x140A44474` in `FUN_140a443d0` — both
  absent, same shape). Invoked only via callback registration: unexpandable by a
  static crawl. GF cite is a `// ponytail:` stand-in comment (stamina cell-key
  helper). Leave off-path.
- `0x14213A180` (off-path): real code (`sub rsp,0x58; movss/comiss…` clamp/lerp
  helper, `.pdata` begin, bridge-listed) with **zero** code/data/vtable/xref
  referrers (Ghidra xrefs_to empty; byte scans empty). Dead/unlinked in this
  build or engine-registered. Leave off-path unless a registrant is found.

## 4. Missing roots (79 VAs, 23 seeds)

Root = top-most traced absent caller (all are `.pdata` begins except hub
`0x141E823E0`, which the bridge lists; all absent from `universe.json`;
coverage = fixpoint over verified direct edges + fix-1 table expansion, so
roots marked † need fix-1 to reach their full list). Why match-path per group.

- `0x140402B90` ×20: `140797CD0(HOOK) 14085E3C0 140A3CCF0 1419112B0 141B51400
  141B51E80 141B53060 141EB4650 141FB1880 141FB9FB0 141FBE480 141FC0AD0 141FC0C40
  141FC1380 141FD5200 141FE3830 141FE9B00 141FE9F40 141FEA000 141FEA250`.
  Upstream of the trapdata/eliza/editsave clusters incl. a live hook; region
  adjacent to seeded match-driver roots. Highest-value seed.
- `0x1408398D5` ×18: `1406E6D10 14071E520 1407205A0 140723DE0 1407240F0 1407241A0
  140724590 1407245D0 140726610 140726640 1408272A0 140829E80(HOOK) 1408360F0(HOOK)
  1408364D0(HOOK) 1408371A0(HOOK) 140837670(HOOK) 140837FC0(HOOK) 140862530`.
  Kick-error/humanoid cluster; 6 live hooks downstream. (Interior cites
  `0x1407240F0/0x1407241A0` are genuine multiple entries of container
  `0x140723DE0` — bridge lists them; not wrong VAs.)
- `0x1404187FF` ×10: `1406D8BF0 1406D9040 1406DF630 1406E03D0 1406E0540 140922BC0
  140A58F00 140A592B0 140A595C0 141EB42B0`. Trap-actor/stamina/worldrecord
  cluster; region adjacent to seeded team-tick owner `0x14041ECF0`.
- `0x1421339C8` ×6: `14211F090 14211FE20 142120E60 1421240B0 142125AB0 142128A20`.
  Carrier sub-cluster (GF `pescarrier` cites); † table-transit.
- `0x1406AEE20` ×4: `1406AC4D0 1406AC660 1406AC990 1406AD810`. Trapdata
  accessors (GF `pestrapdata` cites).
- `0x141E823E0` ×3: `141E67B70 141E73720 141E823E0`. Trap-anim dispatch hub
  (267 direct E8 callers); GF `pesreceive` cites; † table-transit.
- `0x1409224D0` ×2: `1406EB210 141EB5810`. Set-piece/movestep cluster.
- `0x141FCEB1B` ×2: `141FCDA30 141FCF130`. Carrier pair.
- Singles: `0x142136F4E→142135230`, `0x1423CBEC0→142397EA0`,
  `0x1406B4C80→1406B4D40`, `0x140796E7F→14079E5C0(HOOK, trap-end handler)`,
  `0x14040410E→14140EA40`, `0x1414F6C83→1414F5120`, `0x1416C7E12→1416C7930`,
  `0x141805840→141805840` (lighting, self-top via `0x142A383C0` table),
  `0x141EB466D→141AF34B0`, `0x141E681C0→141E681C0` (self-top, shares table
  `0x142B53158` with the TD hub cluster), `0x1406F8518→141EB9A30` (handshake),
  `0x14211A550→14211A550` (carrier init, twin 8-slot tables `0x142BF0030/C0`,
  refs only from absent `0x142119CD2`), `0x142135EAF→142135700`,
  `0x142135E84→142136F00` (E9-tail entry), `0x1406E6DF0→14081D8C0`.
  Each single is the top-most traced static caller; match-path via GF's
  on-the-pitch sim cites (kick/carrier/goalkick/goal-kick/receive/uniform/
  formation/lighting/handshake) and, for the hook-adjacent ones, live fire.

## 5. Exact crawl fixes (for Main; no tooling edited here)

1. `universe.py closure()`: expand absolute-pointer `.rdata` runs referenced by
   RIP-relative LEA/MOV/CALL/JMP (`dtable` edge, same as vtable rule). Evidence §1 W3.
2. `universe.py jump_tables()`: add the `r13+Ri*4` absolute-table shape (or
   resume/split capstone bodies after indirect jumps). Evidence §1 W2
   (`0x141A89569–74`, `.pdata`-anchored decode above).
3. Re-crawl + add a ledger check: every walked row's `direct`/`tail` callee to a
   `.pdata` begin must have a row (56 currently dropped; 96 bogus `table→`
   non-code targets should be filtered — `CODE_LO<=t<CODE_HI` already assumed).
   Also record JMP-thunk-tail edges (§3).
4. `SEEDS` += the 23 roots in §4 (each `.pdata`/bridge-listed; cite this report +
   the GF file in `final_table.json` as the doc reference).
5. Policy: walk `0x141EC4560` + `0x141ED4310` (rtti:draw) + `0x141AFB090`
   (doc:anim-blend) or accept their 6 as off-path (§2).

Expected in-scope delta (verified-edges fixpoint vs committed `universe.json`):
**fixes 1–3 alone: +267 rows (19/107)** plus the 55 sibling dropped rows;
**+ fix 5: +181 rows (6/107)**; **+ fix 4: grand +397 rows covering 105/107**
(only the 2 §3 off-path VAs remain out). In-scope total moves ≈ 18,857 → ≈19,250.

## 6. Per-VA table

`E8` = verified direct callers, `S` = `.rdata` slot refs, `cls` = caller scopes
(A=absent, in/*=universe scope, walked/boundary), `H` = live hook. Full evidence
(rows: `final_table.json`; closures: `final_assign.json`):

EDGE-W (19): `14079B3D0 141A874A0 142121920 1421225E0 1421226C0 142124950
142125140 142125C80 1421265C0 142126A80 142126B70 142127FF0 142128490 142128C10
142129590 14212A700 14212ABA0 14212AEB0 142132660`
EDGE-B (6): `1414E03C0 141AD98B0 141AD9220 141ED48F0 141B15F00 141B158F0`
WRONG-VA (1): `14071D760→14071D770` (12 E8 callers intend the target)
OFF-PATH (2): `140A57B00 14213A180`
ROOT (79) → root map: `140402B90×20 1408398D5×18 1404187FF×10 1421339C8×6
1406AEE20×4 141E823E0×3 1409224D0×2 141FCEB1B×2` + 15 singles
(`142136F4E 1423CBEC0 1406B4C80 140796E7F 14040410E 1414F6C83 1416C7E12 141805840
141EB466D 141E681C0 1406F8518 14211A550 142135EAF 142135E84 1406E6DF0`).
All 79 have E8 call chains verified to their root except slot-only
`141B53060 141E67B70 141E73720 14211A550 142121920 14212A700 14212ABA0 14212AEB0
141805840`(table-entry, §1/§4 tables) and `142136F00`(E9-only, site `0x142135EAA`
in absent `0x142135E84`) — entry mechanism named in each case.

Unresolved: interior entry `0x140724820-group` container `0x140723DE0` is reached
only via K-root (fine); whether `0x140402B90`'s wide coverage routes through
fix-1 tables (likely: trapdata dispatch) — flagged †, re-measure after fix-1
lands. No builds, no gate (static RE task; Main gates).
