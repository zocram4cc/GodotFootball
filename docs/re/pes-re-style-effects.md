# PES 2021 playing-style effects at the read sites (StyleDecode)

Scope: the 228 census sites (`census.tsv`, bit per site), the 13 callers of `FUN_1405C0F00` (dt18 selector), and the 7-record table at `0x1429530D8`. Read-only. No engine edits, no live writes.

Scratch: `<workdrive>/gf-StyleDecode-scratch/`. `dec/*.c` holds Ghidra decompiles of every census host and selector caller (fetched this session). `site.py` prints each site's gate and branch body with the `DAT_` constants resolved. `ctx.py`, `const.py` (f32 at VA from `pes_full.bin`), `bitctx.sh`.

Tags:
- **CONFIRMED-gate**: the bit test and branch body are read in a Ghidra decompile this session.
- **CONFIRMED-const**: the value is read from `pes_full.bin` at the VA (file offset = VA − 0x140000000).
- **INF**: the meaning of a variable, or the host's system, is inferred from the branch shape and from census §2 and the callers.
- **OPEN**: blocked; the block is named.

Requests: about 62 tool calls, about 115 Ghidra bridge calls.

**Main's independent check (17-10-26).** Decompiled `FUN_140595AB0`, `FUN_1405FC500`, `FUN_1405FC860` and `FUN_1405C0F00` through the Ghidra bridge, and read the constants from the running PES process (`/proc/<pid>/mem`, read-only): `0x142591140` = 30.0, `0x14259BE0C` = 15.0, `0x142598578` = 0.2, `0x1425A8A28` = 0.05, `0x1425C77D8` = 121.0, `0x1425CA314` = 256.0, `0x14259BE20` = 25.0. All match. One effect missing from the tables below was found and added: Goal Poacher also widens a close-to-goal radius in `FUN_140595AB0` from 11 m to 16 m (`d² < 121` → `d² < 256`).

## 0. Findings

1. **All 21 bits have in-match gates.** The earlier claim "5 styles branch in-match (7, 8, 12, 19, 20); 16 do not" (`pes-re-playing-styles.md` §10) is wrong. Bits 1 and 14 have effects only at the selector level (dt18 profile, approach-rate field); the rest have gates plus constants or branch bodies.
2. **Quantitative effects, CONFIRMED-const** (full list §4): Goal Poacher run threshold 30→15; Target Man candidate +30 and offset −6; Hole Player and Box-to-Box score +5; Extra Frontman +0.2; winger, roaming-flank, cross and full-back-finisher terms in `1405E6E80` (+15, ×0.5, +1); full-back blend divisors 1.5 and 2.0; set-piece clamps [0, 0.1] and [0, 0.5]; keeper range 15→17, angle 48→43, distribution factors 0.25/1.1/0.6; Destroyer behind-tolerance 11.25° and 45°.
3. **dt18 selectors (§3):** key 0x3A (bit 10) and key 0x21 (bit 1) gate the profile records 0xBC and 0xBD. Key 0x1D adds +14, key 0x1F multiplies by 1.3, key 0x1E switches the record fields 0xD8→0xDC. The `_lineBreaker` selector (key 0x20) and the keys 0x27, 0x28, 0x30, 0x34 have no caller in the 13-caller census: those switch arms are dead.
4. **`FUN_141465180` is record 6 of a 7-entry table of predicate handlers** (§5). The table is a rule-kind registry. Its dispatcher is still OPEN: no 8-byte pointer or RIP-relative reference to the table exists in the image.
5. **Corrections to earlier docs** (§7): the Destroyer 0° tolerance VA `0x14259BE20` reads 25.0, not 0°. The `1405D8B70` bit-4 term is `−s·T[slot]` (a per-slot table at `team+0x3A9C`), not `−10·s`.

## 1. Per-style table

Sites = census count (A64030 + A63FE0 + E890). "Host" = Ghidra function name (`FUN_` omitted). `s` = `fVar34 = (float)*(char*)(player+0x254)` in `1405D8B70` (side sign, INF). `slot` = the formation slot `uVar33` in `1405D8B70` (doc §4 order, INF).

| bit | style (UI) | sites | system (INF from census §2) | effect (what changes when the bit is set) | constants (VA = value) | status |
|---|---|---|---|---|---|---|
| 0 | Goal Poacher | 31 | off-ball run threshold; forward-run offset; target clamp | `140595AB0` run-acceptance threshold 30 → 15 (the test `fVar13 < param_4`). `1405D8B70` L650–653: forward-run offset `+5·s` for slots 5–9 (and 5–6 when `local_1d6 < 2`). `140662A10` L102–104: target floor raised to 25 when `fVar14 ≤ 25`. `1406087A0` L75–76: `fVar13 − 12`. | `142591140`=30 → `14259BE0C`=15; `14259222C`=5; `14259BE20`=25; `14259BE04`=12 | CONFIRMED-gate, CONFIRMED-const; meaning INF |
| 1 | Dummy Runner | 8 | Diagonal (`_dummyRunner`) profile selector; run candidates | Key 0x21 (`1405C0F00`) gates the Diagonal branch of `140612FC0` (undefined twin `140613CAE`). When set, the run test uses record 0xBD: `d² < (f[+0x44] + f[+0x4C])²`. Candidate predicates in `14053BB90` (needs `*piVar10==2` and distance ≤ `fVar2`) and `1406768E0`. | record 0xBD fields: values OPEN (dt18 bytes, not in the exe) | CONFIRMED-gate; values OPEN |
| 2 | Fox in the Box | 22 | off-ball forward offset; receiver predicates; copy+0xB8 readers | `1405D8B70` L655–657: `+5·s` (same slot rule as bit 0). `1405DEF90` L21–22: when `!FUN_140A63680` and bit 2, `uVar7 = 2.5`. `140E295D0` (direct, census §5): sets `+0x58=1`, `+0x34=0`, early return. `1402C0B30`: calls `FUN_1402C1570(copy,1)`. | `14259222C`=5; `14259BDE4`=2.5 | CONFIRMED-gate, CONFIRMED-const; `140E295D0` decompile in `dec/` |
| 3 | Target Man | 20 | candidate scores; receiver predicates | `1405FC500` L111–113: candidate rating `+30` (for slot class 6 or 9 in that branch). `1405E6E80` L432–434: `fVar40 −= 6·fVar43` when bit 3 and `idx < 11`. `1405DC350` L574–576: `+fVar33` to the score (`fVar33` value OPEN). `1405571C0` L83–86: bit 3 clear → height test (`byte[+1]` vs 0x57). `1406046B0` L29–31: bit 3 clear → `return 1` (reject). `14061FA40` L172–176: bit 3 clear and distance < 15 → branch. | `142591140`=30; `14259BDEC`=6; `14259BE0C`=15 | CONFIRMED-gate, CONFIRMED-const; `+fVar33` value OPEN |
| 4 | Creative Playmaker | 13 | off-ball forward offset; bit-10 interaction; reject gates | `1405D8B70` L659–661: `fVar36 −= s·T[slot]` (per-slot table `param_3+0x3A9C`, OPEN). Also L704: when bit 4 clear, flag `+0x6C5` set. `1405DEF90` L88–93: bit 4 clear → bit 10 adds `+5`; bit 4 set → no `+5`. `140608AC0` L80–82: bit 4 set, `iVar7 < 7` → `return 0`. `1405E2C90` L96–99: bit 4 or 11 → skip. | `14259222C`=5 | CONFIRMED-gate, CONFIRMED-const; per-slot value OPEN |
| 5 | Prolific Winger | 12 | set-piece clamp; lateral damping; candidate exclusion | `140680980` L301–303: with bits 5 and 6 both clear → clamp `fVar28` to [0, 0.5]. `1405E6E80` L590–594: bit 5 or 6, `fVar48 < 0` → `fVar42 += fVar32; fVar47 ×= 0.5`. `1405A0A70` L29–33: bit 5 required (`return false` otherwise; `fVar13` default −1). `1405DC350` L640–642: bit 5 or 7 → break. | `142565600`=0.5; `142588218`=−1 | CONFIRMED-gate, CONFIRMED-const |
| 6 | Roaming Flank | 10 | lateral term; band; set-piece clamp | `1405E6E80` L552–556: bit 6 and not bit 5, `fVar48 < 0` → `fVar47 += 15`. L1233–1236: `fVar46 = local_420[6] − 25`, `fVar45 = local_420[6] − 18`. `1405D8B70` L468 → `bVar24 = true`. `140663440` L45 → `return true`. `14066B980` L42 → `return 1`. `1405DF5A0` L105: first test of the full-back blend chain (bit-6-set branch partly decoded). | `14259BE0C`=15; `14259BE20`=25; `14259BE18`=18 | CONFIRMED-gate, CONFIRMED-const |
| 7 | Cross Specialist | 7 | set-piece clamp; lateral term; cross fallback | `140680980` L293–296: bit 7 → clamp `fVar29` to [0, 0.1]. `1405E6E80` L527–530: bit 7 and `fVar48 < 0` → `fVar47 = fVar42 + 1`. `14067BC10` L144–147: bit 7 → `fVar14 = param_1[0x20]`. `14053BA10` L22: bit 7 and `FUN_14053B6E0` → candidate mask. Census R3 (`14211AC8D`, `shr 7`): base-50 cross fallback (doc constants, not re-read). | `142588200`=0.1; `14256AE1C`=1 | CONFIRMED-gate, CONFIRMED-const; R3 behaviour doc-only |
| 8 | Classic No. 10 | 13 | receiver scorers (carrier/receiver); lateral offsets | `142125C80` (L1421264D5) and `142132660` (L142132A2B): `R8D=8` gate; doc R2a/R2b: immediate return on bit 8 (the pass-target preference). `1405E6E80` L462–466: `fVar40 = fVar47 − fVar43`, `fVar46 = fVar43·fVar32 + fVar47`. `14067D4A0` L22–24: bit 8 selects branch with `DAT 5`. `1405C1620` L16–19: bit 8 or 11 → `return true`; neither → `return FUN_140A63680 != 0`. `140681A20` L156–158: bit 8 → `fVar34 += fVar8`. | `14259222C`=5 | CONFIRMED-gate; receiver return is doc-level (census §3 CONFIRMED call site, consumer behaviour from doc) |
| 9 | Hole Player | 20 | off-ball run keys 1D/1E/1F; candidate predicates; positional score | Keys 0x1D (`14060D210` L95–97: `+14`), 0x1F (`14060A8D0` L49–51: `×1.3`; `14053E9A0` L348–352: `×1.3`). `1405A6910` L65: bit 9 required (`return 0` otherwise). `14066E130` L37–40: bit 9 clear and `x ≥ 196` with `y ≥ 64` → branch. `14060CB50` L39: thresholds 36 and 42.25. `1405E6E80` L469–472: `fVar46 += fVar43·fVar32`. `140681A20` L160–162: `fVar34 += fVar8`. | `14259BE44`=64; `1425C4C18`=196; `1425A9464`=36; `1425C721C`=42.25; `1425C48B8`=14; `14259BDD8`=1.3 | CONFIRMED-gate, CONFIRMED-const |
| 10 | Box-to-Box | 9 (+ key 0x3A) | `_BoxToBox` marking profile; forward score | Key 0x3A → record 0xBC: `14052F720` L250–255 (role AND gate → `+0x3C`, else `+0x38`); `140533BC0` L10–17 (role AND gate → `+0xF8,+0x100,+0x44,+0x1C`, else `+0xF4,…`). `1405DEF90` L90–93 and `1405DF1F0` L72–74: bit 10 → `fVar8 += 5`. `1405E6E80` L436, L634, L650: bit 10 gates. | `14259222C`=5; profile values OPEN (dt18) | CONFIRMED-gate, CONFIRMED-const; profile values OPEN |
| 11 | Anchor Man | 15 | exclusion gate; score; positional branch | `1405D3550` L172–174: bit 11 clear required (skip when set). `1405DA3A0` L120–122: bit 11 or 17 → skip. `1405E2C90` L96–98: bit 11 → skip. `1405E6E80` L457–461: bit 11 → `fVar46 = fVar43·fVar32 + fVar47`. `140681A20` L152–154: bit 11 → `fVar34 += fVar12` (`fVar12` OPEN). `FUN_141AAB320` (direct copy+0xB8, census §5): alternate path. | none decoded (`fVar12` OPEN) | CONFIRMED-gate; values OPEN |
| 12 | The Destroyer | 1 wrapper (dead) + direct | tackle (R5a, R5d); keeper-free defence | Wrapper key 0x30/0x34 (`1405C0F00` L55–56): no caller → dead. Direct: `14078E5A0` L125 (`>>0xC & 1`), `140971BA0` L234 (`>>0xC & 1`) — both confirmed. Tackle constants (doc §7): behind-tolerance 11.25°/45°/0°; touch-timing 0.075/0.1/0.125·gs; slide `d ≥ 4.0+0.8L`; team mode `+1`. | `1425CC680`=11.25 ✓; `14259BE30`=45 ✓; `14259BE20`=25 ✗ (doc says 0°) | CONFIRMED-gate; 11.25/45 CONFIRMED-const; 0° VA wrong; touch/slide doc-only |
| 13 | Orchestrator | 11 | off-ball forward offset; reject gates; receiver | `1405E6E80` L442–446: bit 13 → `fVar40 −= 6·fVar43`. L226 and L653: chain tests (L653 result discarded, no effect). `140603D00` L58–60: bit 13 → `return false`. `140608AC0` L85–87: bit 13 → `return 0`. `14060FE20` L111, `140611140` L125: receiver lookups. | `14259BDEC`=6 | CONFIRMED-gate, CONFIRMED-const |
| 14 | Build Up | 3 | candidate gate; distance | `14053B900` L16: bit 14 required for the candidate loop. `1405E6E80` L695–698: bit 14 → `fVar48 = f[+0x38]`, `dVar36 = fVar48 − fVar32`. `140610A30` L136–138: bit 14 branch. | none decoded | CONFIRMED-gate; effect OPEN (`approachRateMax_buildUp` reader untraced) |
| 15 | Extra Frontman (OVERLAP) | 6 | candidate score; contact gate | `1405FC860` L62–65 and L132–135: bit 15 → `fVar17 += 0.2`. `14051B8B0` L106–108: bit 15 → store candidate. `1405F63C0` L157–158, `1405F7D00` L56: gates. `FUN_141B620D0` (census §3): reads and clears bit 15 under `[block+0x4C] & 0x01000000`. | `142598578`=0.2 | CONFIRMED-gate, CONFIRMED-const |
| 16 | Offensive Full-back | 7 | full-back blend; forward-run scale; finisher gate | `1405DF5A0` L109–121: bit 16 (not 6/4/13) → `fVar16 = (fVar22 + fVar14)·0.5`, `fVar18 = 2.0`. `14067ECC0` L695–699: bit 16 or 18 and `bVar26` → if teammates > 1: `fVar28 = 2·fVar33`, `fVar32 = 1.0`. `1405E6E80` L474–477: bit 16 → `fVar46 += fVar43·fVar38`. `140640CC0` L262–263: bit 16 keeps `fVar1`; bit 16 clear and bit 18 set → `fVar23 = 0`. `140641690` L161 (call result unused in host). | `142565600`=0.5; `142565604`=2.0; `14256AE1C`=1.0 | CONFIRMED-gate, CONFIRMED-const |
| 17 | Defensive Full-back | 9 | full-back blend; forward offset; SB candidate gate | `1405DF5A0` L111–116: bit 17 → `fVar16 = fVar14` (no blend). Base (none of 6/4/13/16/17), `iVar9 == 0` → `fVar16 = (0.5·fVar22 + fVar14)/1.5`, `fVar18 = 1.5`. `1405E6E80` L447–450: bit 17 → `fVar40 −= 3·fVar43`. `14056DF90` L105: SB slot 2 requires bit 17. `1405A0D20` L26–30: bit 17 test. `14067ECC0` L487–491: bit 17 and `uVar25 < 0x16` → `uVar25 = 0xff` (conditional). | `14258C264`=3; `14259BDDC`=1.5; `142565600`=0.5 | CONFIRMED-gate, CONFIRMED-const |
| 18 | Full-back Finisher | 10 | finisher score; receiver gate; forward term | `1405E6E80` L578–582: bit 18 (behind an earlier test on `cVar7`, not decoded) and `fVar48 < 0` → `fVar47 += 15`. `14067ECC0` L697–701: as bit 16. `140640CC0` L265–266: bit 18 → `fVar23 = 0`. `140641690` L230–232: bit 18 → `fVar26 = 0`. `140608AC0` L73–76: bit 18 clear and `iVar7 == 2` → `return 0`. `1405DC350` L789–791: bit 18 → skip (`fVar32 = 3`). | `14259BE0C`=15; `14258C264`=3 | CONFIRMED-gate, CONFIRMED-const |
| 19 | Offensive GK | 0 wrapper; direct | keeper range, angle, rush mode | Direct R6 sites, gates confirmed: `1409DD470` L264–265 (`>>0x14`, `>>0x13`), `1409DE8C0` L134 (`>>0x13`), `1421163F4` L256–257 (bVar27 = bit 19 clear). Range `15→17` (`1409E0032`, doc R6d). Angle `48→43` (`142115BB2`, doc R6f). Rush mode `0x10` (`14092BD60`, doc R6a). | `14259BE0C`=15 ✓; `14259D614`=17 ✓; `142588214`=48 ✓; `1425CFA38`=43 ✓ | CONFIRMED-gate, CONFIRMED-const; mode values doc-only |
| 20 | Defensive GK | 0 wrapper; direct | keeper distribution | `1409DD470` L264 (`>>0x14`), `1421163F4` L257 (bVar28 = bit 20 set), `1409E03BE` (doc R6e), `1409DDB69` (doc R6b). Distribution factors (doc R6b): DefGK `f·0.25`; OffGK `f+1.1`; neither `f·0.6`. Mode `0x20` (doc R6a). | `142592228`=0.25 ✓; `1425A3590`=1.1 ✓; `14257A190`=0.6 ✓ | CONFIRMED-gate, CONFIRMED-const; mapping of factors to branches is doc-only |

## 2. Quantitative before/after

Only the terms with a literal constant. "Before" is the unmodified value at that statement.

| effect | site | before | after (bit set) | status |
|---|---|---|---|---|
| Poacher run threshold | `140595AB0` L29–33 | 30 | 15 | CONFIRMED-const |
| Poacher close-to-goal radius | `140595AB0` (the `FUN_140a4c720` branch) | `d² < 121` (11 m) | `d² < 256` (16 m) | CONFIRMED-const (Main, live read) |
| Poacher target floor | `140662A10` L102–104 | `fVar14` | `max(fVar14, 25)` when `fVar14 ≤ 25` | CONFIRMED-const |
| Forward-run offset, bit 0 / 2 | `1405D8B70` L650–657 | `fVar36` | `+5·s` each | CONFIRMED-const, `s` INF |
| Forward-run offset, bit 4 | `1405D8B70` L659–661 | `fVar36` | `−s·T[slot]` (T OPEN) | CONFIRMED-gate |
| Target Man candidate | `1405FC500` L111–113 | `fVar17` | `+30` | CONFIRMED-const |
| Target Man offset | `1405E6E80` L432–434 | `fVar40` | `−6·fVar43` | CONFIRMED-const |
| Hole / Box-to-Box score | `1405DEF90` L88–92; `1405DF1F0` L72–74 | `fVar8` | `+5` | CONFIRMED-const |
| Extra Frontman candidate | `1405FC860` L62–65 | `fVar17` | `+0.2` | CONFIRMED-const |
| Lateral damping, bits 5/6 | `1405E6E80` L590–594 | `fVar47` | `×0.5` when `fVar48 < 0` | CONFIRMED-const |
| Roaming Flank lateral | `1405E6E80` L552–556 | `fVar47` | `+15` (bit 6 and not 5, `fVar48 < 0`) | CONFIRMED-const |
| Cross lateral | `1405E6E80` L527–530 | `fVar47` | `fVar42 + 1` (`fVar48 < 0`) | CONFIRMED-const |
| Full-back finisher lateral | `1405E6E80` L578–582 | `fVar47` | `+15` (bit 18, `fVar48 < 0`) | CONFIRMED-const |
| Orchestrator offset | `1405E6E80` L442–446 | `fVar40` | `−6·fVar43` | CONFIRMED-const |
| Defensive FB offset | `1405E6E80` L447–450 | `fVar40` | `−3·fVar43` | CONFIRMED-const |
| Full-back blend, base | `1405DF5A0` L111–121 | `fVar16` | `(0.5·fVar22 + fVar14)/1.5`, `fVar18 = 1.5` | CONFIRMED-const |
| Full-back blend, bit 16 | `1405DF5A0` L115–121 | `fVar16` | `(fVar22 + fVar14)·0.5`, `fVar18 = 2.0` | CONFIRMED-const |
| Full-back blend, bit 17 | `1405DF5A0` L111–113 | `fVar16` | `fVar14` (no blend) | CONFIRMED-gate |
| Set-piece clamp, bit 7 | `140680980` L293–296 | `fVar29` | clamp to [0, 0.1] | CONFIRMED-const |
| Set-piece clamp, bits 5/6 absent | `140680980` L301–303 | `fVar28` | clamp to [0, 0.5] | CONFIRMED-const |
| Keeper range | `1409E0032` (R6d) | 15 | 17 | CONFIRMED-const (both values) |
| Keeper angle | `142115CEC` (R6f) | 48° | 43° | CONFIRMED-const (both values) |
| Keeper distribution | `1409DD470` (R6b) | `f` | `f·0.25` / `f+1.1` / `f·0.6` | CONFIRMED-const (factors); branch mapping doc-only |
| Destroyer behind-tolerance | `14078E9B4` (R5a) | 0° | 11.25° / 45° | CONFIRMED-const for 11.25 and 45; 0° VA wrong |

## 3. dt18 selector table (callers of `FUN_1405C0F00`)

`FUN_1405C0F00(playerIdx, key)` switches on the key and returns the style bit's gate for the player. The 13-caller census is in `pes-re-style-census.md` §1.2. Record accessors: `FUN_141E5D4C0(team, index)` returns `*(*team + index·8) + 8` via `FUN_141E5CA20`; index → name is OPEN (the dt18 key table is not decoded).

| key | bit | callers (VA, status) | gate use | record / fields | effect when the gate is true | constants | status |
|---|---|---|---|---|---|---|---|
| 0x3A | 10 Box-to-Box | `14052F720` L250; `140533BC0` L10 | role `FUN_14052C440` AND gate | record **0xBC** (name INF: `_BoxToBox`) | `14052F720`: `+0x3C` instead of `+0x38`. `140533BC0`: `+0xF8,+0x100,+0x44,+0x1C` instead of `+0xF4,…` | record values OPEN | CONFIRMED-gate |
| 0x21 | 1 Dummy Runner | `140612FC0` L31; undef `140613CAE` | gate enters Diagonal branch | record **0xBD** (name INF: `_dummyRunner`) | radius = `f[+0x44] + f[+0x4C]`; test `d² < radius²` | record values OPEN | CONFIRMED-gate |
| 0x1E | 0 / 9 | `1405ACA50` L18; undef `140602651`, `14060725F` | gate true → alternate fields | record **0xC3** (name OPEN) | gate false and `FUN_1405A0A70` false → `+0xD8,+0x64,+0x7C,+0x6C,+0xBC,+0x84,+0xB4,+0x9C`; otherwise `+0xDC,+0x68,…` | record values OPEN | CONFIRMED-gate |
| 0x1D | 9 Hole Player | `14060D210` L95; `140612660` L12 | `fVar11 += 14` | record 0xC3 (`140612660`) | `+14` (`1425C48B8`) | 14 | CONFIRMED-const |
| 0x1F | 9 Hole Player | `14060A8D0` L49; `14053E9A0` L348; undef `14060B05C` | `×1.3`; `14053E9A0` else branch `1405C1580` | — | `×1.3` (`14259BDD8`); `14060A8D0` `fVar10 − (−8)` when `iVar4 < 5` (`1425C7BA4`) | 1.3; −8 | CONFIRMED-const |
| 0x2F | 0 Goal Poacher | undef `14061E180` (54-byte shim) | gate | — | OPEN (shim body not read) | — | OPEN |
| 0x20 | 0 | **no caller** | — | — | `_lineBreaker` selection is not reached through this key | — | dead arm |
| 0x27 / 0x28 | 16 / 15 | **no caller** | — | — | dead arms | — | dead arms |
| 0x30 / 0x34 | 12 Destroyer | **no caller** | — | — | dead arm | — | dead arm |
| — | `_lineBreaker` | LineBreak scorer (unscouted) | no key | — | OPEN | — | OPEN |
| — | `approachRateMax_buildUp` | no reader found | — | — | OPEN | — | OPEN |

Reading of the table: only bits 10 and 1 select a named dt18 profile through this function. The `_BoxToBox` and `_dummyRunner` profiles are therefore style-gated (the earlier retraction stands and is strengthened). The 0x1D and 0x1F arms are scalar modifiers, not profiles.

## 4. Dispatcher table `0x1429530D8` and `FUN_141465180`

The table is 7 records of 24 bytes: `(fn, 1, ptr)`, read from `pes_full.bin`.

| rec | fn | ptr | handler (decompile) | predicate |
|---|---|---|---|---|
| 0 | `141463030` | `142954CE8` | `FUN_141463030`: `FUN_140A60530(+0x38)` gate, then `FUN_141477920` | not a style bit |
| 1 | `141463080` | `142954D00` | no function at this VA (54-byte stub) | OPEN |
| 2 | `1414652B0` | `142954D20` | `FUN_1414652B0`: `FUN_141477E30(…, 0xB)` vs float `+0x10`, flags `+0x20` (`&8`, `&4`, `&0x20`) | style count compare; not decoded further |
| 3 | `141465340` | `142954D48` | `FUN_141465340`: `FUN_141477F60(…, 0xB)`, same compare shape | as rec 2 |
| 4 | `141463AE0` | `142954D60` | `FUN_141463AE0`: `FUN_140A631F0(idx, bit, roster)`, `+0x18 & 0x20` invert | bit ≤ 6 predicate |
| 5 | `141465220` | `142954D78` | `FUN_141465220`: `(*(u64*)(block+0x48) >> bit) & 1`, bit ≤ 40 | **reads bits 32–40 = COM mask (`+0x4C`)** as well as the style word |
| 6 | `141465180` | `142954D88` | `FUN_141465180` (below) | style-bit gate |

`FUN_141465180(rec)`: if `rec[+0x2C] > 0x15` or `rec[+0x28] > 1` → 0. Else player = `FUN_140A3AF90(roster, rec[+0x2C])`, block = `FUN_141B59E20(player)`, `bit = (int) float[rec+8]`, `slot = FUN_141472400(rec, rec[+0x2C])`. If `bit < 0x15` and `slot < 10` → `FUN_1408CB0B0(block+8, bit, slot)` (the gate). Otherwise 0. The `rec[+0x28] == 0` vs `1` side is INF.

Record layout (INF from the 7 handlers): `+0x08` float (bit or value), `+0x0C` float, `+0x10` float, `+0x14` float, `+0x18` flags (bit 0x20 = invert), `+0x1C` int, `+0x20` flags (bits 4, 8, 0x20), `+0x28` side (0/1/2), `+0x2C` player index.

Dispatcher: **OPEN.** The table base and record-6 `fn` are referenced only from the table itself. The image has no 8-byte pointer to `0x1429530D8`, `0x1429530E0`, `0x142953160`, or `0x142953168`, except that `0x142953168` (rec 6's `fn` field) holds `0x141465180`. The earlier census RIP-relative search also found no code reference. The dispatcher is most likely an indirect call through a rule object filled at load time from this table; not located.

## 5. Before/after corrections to earlier docs

- `pes-re-playing-styles.md` §10 and §9: "5 styles branch in-match (7, 8, 12, 19, 20); 16 do not" → wrong. All 21 bits have in-match gates (§0.1).
- `pes-re-playing-styles.md` §6: `_BoxToBox` selected by "role AND gate 0x3A — NOT the style bit". The gate 0x3A is bit 10 (`1405C0F00` case 0x3A → `FUN_140A64030(…, 0xA)`); the profile selection is therefore style-gated. Matches census §4.
- Destroyer 0° tolerance: VA `0x14259BE20` reads 25.0 in the image, not 0°. The VA is wrong or refers to another value; 0° is not at that address (INF: it may be a literal `0`).
- `1405D8B70` bit-4 term: `fVar30` is reassigned in the slot loop (L196: `fVar30 = fVar34·T[slot]`, `T = f[param_3+0x3A9C+slot·12]`). It is not the constant 10 (`142588210`). Value OPEN.
- `GetAttackingRunAffinity` (doc §8): no definition exists in `src/` or `tests/`; the doc is stale.
- Census §3 "bits 3–6, 11, 13, 16–18: effects OPEN": now decoded at gate and constant level (§1, §2).

## 6. GF gap list

GF knobs (`src/sim/data/playingstyles.cpp`) against the PES sites above. Caller check: grep of `src/` and `tests/`.

| GF knob | GF callers | PES effect | verdict |
|---|---|---|---|
| `GetDepthOffset` (`teamAIcontroller.cpp:729`) | 1 | `1405D8B70` forward-run offset: bit 0 `+5`, bit 2 `+5`, bit 4 `−s·T[slot]` (slots 5–9) | Partial. Only Poacher, Fox and Creative Playmaker have a PES offset in this function. GF's numbers for the other 16 styles (incl. Dummy Runner +3, Hole +5, Box-to-Box ±4, Anchor −5/−2) have no PES counterpart in the code decoded here. Keep as `// ponytail:` naming `1405D8B70` L650–663. |
| `GetWidthOffset` (`teamAIcontroller.cpp:733`) | 1 | `1405E6E80` `fVar47`: bit 6 `+15`, bits 5/6 `×0.5`, bit 7 `fVar42+1`, bit 18 `+15`. Meaning of `fVar47` is INF (lateral?). | Partial. GF's signs and magnitudes (+4 Dummy/Winger, +5 Cross, −4 Fox/FullBackFinisher, −5 Roaming) do not match the PES constants. `// ponytail:` naming `1405E6E80` L527–582. |
| `GetPoacherTargetX` (`teamAIcontroller.cpp:738`) | 1 | `140595AB0` run threshold 30→15; `140662A10` floor 25. No PES site places the Poacher at the offside line. | GF placement (offside line + cushion) has no PES site: `// ponytail:` stand-in. The PES run threshold is the nearest real effect and is not in GF. |
| `GetDribbleDrive`, `GetOpponentRepel`, `GetDribbleCenterPull`, `GetDribbleVelocity` (`AIfunctions.cpp:486–571`) | 4 | No style bit is read in any dribble code in the 228 census sites. | No PES site: `// ponytail:` stand-ins. |
| `GetShotAppetite`, `GetShootingRangeBonus` (`playingstyles.cpp:352, 393`) | 0 (the `playerskills.*` and `gameplaytuning.hpp` overloads are different functions) | No PES shot-appetite site decoded. | Dead code. Delete. |
| `GetFinisherDemand`, `GetCreatorVision`, `GetPassTypeBias`, `GetSpaceRatingWeight` (`playingstyles.cpp:403–469`) | 0 | Nearest PES effect: bit 8 receiver preference (`142125C80`, `142132660`). | Dead code. The receiver preference is a real PES gap in GF (below). |
| `GetAttackingRunAffinity` | 0 | Not defined. | Doc only. Remove the doc entry. |

PES effects GF lacks (gap list, in order of consumer impact):

1. **Run-quality gate and profile selection** (`1405C0F00` keys 0x1D, 0x1E, 0x1F, 0x21, 0x3A, 0x2F). GF has no style-gated run/marking profile selection. Port the selectors (§3), not the constants.
2. **Classic No. 10 receiver preference** (bit 8; `142125C80` and `142132660` return on the gate). GF has no counterpart.
3. **Forward-run offset for slots 5–9** (`1405D8B70`: bit 0 `+5·s`, bit 2 `+5·s`, bit 4 `−s·T[slot]`). `T` is a team table (OPEN); port the slot rule and the `+5` once `T` is traced.
4. **Full-back blend** (`1405DF5a0` L105–121): base `(0.5a+b)/1.5`, bit 16 `(a+b)·0.5` with `2.0`, bit 17 `b`. GF has none of these.
5. **Set-piece clamps** (`140680980`): bit 7 `[0, 0.1]`; bits 5/6 absent `[0, 0.5]`.
6. **Candidate scores**: Target Man `+30` (`1405FC500`); Extra Frontman `+0.2` (`1405FC860`).
7. **Copy+0xB8 readers** (census §5): bit 2 (`1402C0B30`, `140E295D0`), bits 8 and 11 (`FUN_141AAB320`). GF has no FOX read on the copy object.
8. **Extra Frontman normaliser** (`FUN_141B620D0`, census §3): reads and clears bit 15 under `[block+0x4C] & 0x01000000`. GF needs this or a documented omission.
9. **Keepers** (bits 19/20): GF stand-ins in `elizacontroller.cpp:540` and `goalie_default.cpp:159` should be rewired to the R6 VAs; the constants are confirmed in §1.
10. **Destroyer tackle**: GF should read `14078E5A0` L125 and `140971BA0` L234 (bit 12 `>>0xC`); the 0° VA in the doc must be fixed before porting.
11. **Dispatcher-driven rules** (`0x1429530D8`): GF has no equivalent rule registry. Do not port until the dispatcher is found.

## 7. OPEN

- `1405D8B70` `T[slot]` (per-slot table at `team+0x3A9C`, bit 4 term) and the sign `s` (`player+0x254`, side INF).
- `1405DC350` `+fVar33` and `1405FC500`'s slot-class condition (value/meaning).
- `140612FC0` / `140613CC0` dummy-runner record 0xBD fields; record 0xBC and 0xC3 values (dt18 bytes; not in the exe).
- `_lineBreaker` selector (key 0x20 has no caller; LineBreak scorer unscouted).
- `approachRateMax_buildUp` reader (bit 14 is the only gate).
- Dispatcher of `0x1429530D8` (no pointer or RIP-relative reference in the image).
- Destroyer 0° tolerance VA (`0x14259BE20` = 25.0).
- Keeper distribution branch mapping (f·0.25 / f+1.1 / f·0.6 to DefGK / OffGK / neither) and rush modes 0x10 / 0x20: constants confirmed, mapping doc-only.
- Undefined hosts (18 sites) and `1402BD8AE`, `14174B73D`: not decoded.
- `140641690` L161: the bit-16 call result is discarded; confirm the host does not rely on its side effects.
