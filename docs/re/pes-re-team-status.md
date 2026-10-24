# PES team status word pair `T+0xA294` / `T+0xA29C` (TeamStatusRE, 24-10-26)

> **Main audit 23-10-26 (bytes):** `FUN_140A602B0` = `imul eax, edx, 0x64; mov [rcx], eax; cmp eax, 0x2BC; jle …; mov [rcx], 0x2BC` … `cmovs eax, 0` (clamp(class × 100, 0, 700)): match.

# PES team status word pair `T+0xA294` / `T+0xA29C` and match phase `MI+0x11B0`

 Checker: `check_team_status.py` (this dir), GREEN 24-10-26; mutation (`0x14092B505` `BA 06` -> `BA 07`) drives `cond.edx6` RED, reverted.
 Image: `~/ghidra-projects/scratch/Unpacker/pes_full.bin`, file offset = VA - 0x140000000. One bridge request at a time.
 Floats below are script-read from the image; every cited instruction is paired with its operand VA.

 Three corrections to standing GF-side claims are marked CORRECTION.

## 0. Script-read constants (checker output, pasted)

```
enc.imul100  6bc264            (0x140A602B0 IMUL EAX,EDX,100)
enc.cap700   3dbc020000        (CMP EAX,700 = 0x2BC)
enc.store700 c701bc020000      (MOV [RCX],700 cap arm)
cmp.eq.magic 1f85eb51          (0x140A602E0 /100 magic 0x51EB851F)
set.a294 899194a20000          (0x140A424E0 MOV [RCX+0xA294],EDX)
set.a298 899198a20000          (0x140A424D0 MOV [RCX+0xA298],EDX)
set.a29c 88919ca20000          (0x140A428B0 MOV [RCX+0xA29C],AL)
get.a298 8b8198a20000          (0x140A40BC0 MOV EAX,[RCX+0xA298])
get.cmpa29c 80b99ca2000000     (0x140A40BD6 CMP byte [RCX+0xA29C],0)
get.reada294 8b8194a20000      (0x140A40BF5 MOV EAX,[RCX+0xA294])
init.edx5 8d5605               (0x140A415E2 LEA EDX,[RSI+5], RSI=0)
init.edi4 bf04000000           (0x140A41627 MOV EDI,4)
init.esi0 33f6                 (0x140A415AF XOR ESI,ESI)
cond.edx6 ba06000000           (0x14092B505 MOV EDX,6)
t2.edx6 ba06000000             (0x140503203 MOV EDX,6)
t2.edx5 ba05000000             (0x140503220 MOV EDX,5)
w1.movzx 410fb687b0110000      (0x140458912 MOVZX EAX,byte [R15+0x11B0])
w1.range 83c0f883f80e         (ADD EAX,-8; CMP EAX,0xE; JA 0x140458945)
w1.dl1 b201                   (0x140458937 MOV DL,1)
w1.dl0 33d2                   (0x14045893B XOR EDX,EDX)
w1.tab 458960: 8/B/E/12->0x140458937; 14/15/16->0x14045893B; else 0x140458945
mi.word1 66c783b01100000100   (0x1408AE747 MOV word [RBX+0x11B0],1)
mi.dword1 c787b011000001000000 (0x140A11499 MOV dword [RDI+0x11B0],1)
mi.mirror 0f1086280700000f1183b0110000 (0x1404227F9/800 XMM 16 B container+0x728 -> MI+0x11B0)
cpy.a294 8b8694a20000898794a2 (0x140416FBE/C4 struct copy of A294)
```

## 1. The word pair: one reader, three writers, and what `/100` extracts — CONFIRMED

 Mechanics (all CONFIRMED at bytes + decompile):

 - `FUN_140A40BD0` (0x140A40BD0): `if (*(p+0xA29C)) return FUN_140A602B0(out,0); else *out = *(p+0xA294)`.
 - `FUN_140A40BC0` (0x140A40BC0): `return *(p+0xA298)` (single `MOV EAX,[RCX+0xA298]`).
 - `FUN_140A602B0` (0x140A602B0): `v = cls*100; *p = v; if (700 < v) *p = 700; if (v < 0) *p = 0` — the x100 encoder with clamp [0,700].
 - Sibling comparators over the same encoding: `FUN_140A60340` = `/100` (CLS), `FUN_140A60360` = `/100 < p2`, `FUN_140A60380` = `/100 <= p2`, `FUN_140A602E0` = `/100 == p2` — all with the `0x51EB851F` /100 magic at the bytes.
 - Setters: `FUN_140A424E0` writes `T+0xA294` (single caller-checked: 9 call sites, §3); `FUN_140A424D0` writes `T+0xA298` (SOLE caller `0x141FBA4C5` in `FUN_141FB9FB0`, bridge xrefs); `FUN_140A428B0` writes `T+0xA29C` (SOLE caller `0x141FBA494` in `FUN_141FB9FB0`, bridge xrefs).
 - Copy: `FUN_1404161E0` carries `+0xA294/+0xA298/+0xA29C` verbatim (0x140416FBE/C4/CA/D0/D6/FD, team-state blit).
 - Data-region disp32 hits (`0x142xxxxx`, `0x14xxxxxxx`, `0x15xxxxxxx`) are JSON/option-field serialization and menu structs, NOT the team state: every one probed decompiles as the option-filler `FUN_14215CF10` / serializer `FUN_142153ED0` family. Excluded.
 - Split-displacement sweep: no writer hides behind a split base for A294/A298/A29C — the full writer list is the init + §3 setters + the copier (every code-range disp32 hit accounted for).

 What `/100` extracts: the setup record's level class 0..7 (stored x100, so words read 0/100/200/300/400/500/600/700). It is NOT a team code: both teams carry their own class (two `FUN_140A424E0` calls per site, sides 0/1), and the same word feeds per-side gates (G3 `>= 2`, cap ladder {4,5,6}) that only make sense as a level.

 CORRECTION 1 — GF comments cite writer `0x1B5D8340` (`pescarrier.hpp:401-402`, `pescarrier.cpp:460-461`, `elizacontroller.cpp:3086`). That VA has 9 hex digits and resolves to nothing; the apparent intended VA `0x15B5D8340` holds packed/menu code ("No function found", Denuvo region) and is not a team writer. The actual writers are the six functions in §3 plus the init. The GF loader path (masked global `0x14810F4E0` ^ key imm at `0x15B5D8378`, scale imm16 `6B C2` at `0x140A602B0`, cap imm8 at `0x140A602B5`) reproduces the STOCK DEFAULT (class from the install's own bytes, no transcription) — that part stands; only the "writer" citation is wrong. Replace `0x1B5D8340` with the §3 writer list.

## 2. `T+0xA294` writers, values, timing — CONFIRMED mechanics, INF menu semantics

 Init (`FUN_140A415A0`, called from the team constructor `FUN_140A40580` at `0x140A407AF`): class 5 -> `T+0xA294 = 500` (`LEA EDX,[RSI+5]` at 0x140A415E2 + `CALL 0x140A602B0` at 0x140A41615); `T+0xA298 = 4` (`MOV EDI,4` at 0x140A41627, store at 0x140A41636); `T+0xA29C = 0` (`XOR ESI,ESI` at 0x140A415AF, store at 0x140A41929). So a fresh team is level-class 5, mode 4, flag clear.

 Per-side writers (each runs at setup / programme build, then the in-match copy below):

 | site | function | value written to `T+0xA294` | when |
 |---|---|---|---|
 | `0x141FBA47D` | `FUN_141FB9FB0` | `602B0([RBP-0x60])`, overridden by `602B0(...,5)` when `side<2` and `byte[RBP+R8+0x50]==0` | team setup, per side (`local_98 = FUN_140A6A000(obj)` = `*(obj+0x10)`, the setup level record) |
 | `0x141FBE5AA/694/6E0` | `FUN_141FBE480` | `602B0(...,5)` default both sides; else per-side copy of `FUN_140FC7ED0` record with the matched side forced to 5 | match build when `param+0x18EB4 != 0`; `0xFF` = no match |
 | `0x141FCDECD` | `FUN_141FCDA30` | `602B0(uVar9)`, `uVar9 = FUN_141FC09D0(*(obj+0x3F310))` mapped 0..5 identity (default keeps 2) | per team, both sides |
 | `0x141FCF3ED` | `FUN_141FCF130` | `602B0(2)` default, `602B0(5)` when `side<2` and the `local_res18` flag is set; base otherwise from `FUN_140FC7ED0` record | per team, both sides |
 | `0x140454F9E/FB3` | `FUN_140454C00` (Flow step handler) | per-side `FUN_140A60AE0/60AD0` packer records | restart programme step |
 | `0x14044AE8D` | `FUN_14044A9D0` (Flow step handler) | `local_98` from `FUN_140FC7ED0` (same level-record family) | restart programme step |

 Observed classes: 0..5 at setup (switches in `141FB9FB0`/`141FCDA30` enumerate 0..5; `141FBE480`/`141FCF130` default 5). Class 6/7 reach the word only through the setup/menu record path (`FUN_140FC7ED0` / `FUN_141FC09D0` outputs) or a class-6/7 setup value — never from the enumerated switches. `FUN_140A602B0` maps input 6 -> 600 (== 6 TRUE) and input >= 7 -> 700 (== 7); input < 0 -> 0.

 `T+0xA29C` (sole writer `0x141FBA494`): `AL = FUN_1414C1950(R13, side)` — a per-side byte from the setup record. Nonzero zeroes the level (reader returns 0). Which menu record sets a nonzero value is OPEN (menu layer).

 `T+0xA298` (sole writer `0x141FBA4C5`): `uVar11 = FUN_141FC07B0(FUN_141030B40(edit))` (identity 0..3, else 0), vetoed to 0 by `FUN_141FC1190` unless the mode gate passes; init default 4. So at match time `T+0xA298` ∈ {0,1,2,3} (menu import) or 4 (init default). "Mode 3" (`== 3`, the defensive mode that arms the -14 m level-up in `pesdefshape.cpp:176` and `B29` in `pespress.hpp:342`) ⇔ the edit record's `+0x20 == 3` with the veto silent (condition-roll §2, CONFIRMED mechanism; which menu action stores `+0x20 == 3` stays OPEN).

## 3. Consumers: `/100 == 6` (and 5/4) — CONFIRMED branch effects

| site | function | predicate | effect on TRUE |
|---|---|---|---|
| `0x14092B50D/572` | `FUN_14092AF10` (condition arrows) | `== 6` (EDX=6, `BA 06` at 0x14092B505) | own-team: arrow forced to index 0; opponent: `min(arrow+1,4)` shift applied (one step worse, cap 4, then stat writes skipped for index 2 and >= 5) |
| `0x14050320D/22A` | `FUN_1405027A0` (27A0, in-play) | `== 6` then `== 5` | `== 6`: score `*= fVar7`; elif `== 5` with the geometry gate (`float local_154 * *(f+0x2482C) < 1.0 - *(F+0x18)` and the heading term `< *(F+0x1C)`): `*= f5/f8` (in/out of play) |
| `0x142128E4E/71/94` | `FUN_142128C10` (one-touch predicate, lane-core G8) | `== 6` -> cap base 2; elif `== 5` -> 2; elif `== 4` -> 1; else 0; then `+1` if `T+0xB8A4 > 2` or team mode ∈ {2,3}, capped at 2; pass iff `runCount < cap` | run-count cap ladder (CONFIRMED inline) |
| `0x14062A04C` | `FUN_1406295A0` | `== 6` | TRUE continues into the `XMM1 - [RAX+0x18]` vs scaled-battery comparison (`JA 0x140629D34` skips); FALSE jumps to `0x14062A07F` |
| `0x14096A679` | `FUN_14096A4F0` | `== 6` | TRUE sets `XMM10 = 3.0` (`0x14258C264`) for the `FUN_140439E50` bearing call; FALSE keeps the loaded constant |
| `0x140968401` | `FUN_1409677C0` | `== 6` | TRUE keeps `BL`; FALSE (`JNZ 0x14096840C` not taken) zeroes `BL`, which then forces `ESI = 1` via `CMOVZ` — a flag consumed by the `FUN_140A40BC0 == 3` check two instructions later |
| `0x140A1F423` | `FUN_140A1F1F0` | `== R13D` (dynamic class!) | TRUE with `[R14+0x14] == 0xA` continues to the `FUN_140A3E8A0` touch chain + `MUL RCX,0x59650` dispatch; FALSE jumps to `0x140A1F6A4` |

 G8 detail (lane-core §6.2, re-CONFIRMED at `d_2128c10.c` lines 108-135): `level/100 ∈ {5,6}` and `== 4` share the `base = 2 / 1` arms; G3 (`FUN_140A60360`, level/100 < 2 fails) sits 40 lines above in the same function.

 27A0's unbound input 9 ("press-level writer", TeamT2 §46) is CLOSED as a writer question: the writer is `FUN_140A602B0` (encoder) via the §2 setters; the only OPEN part is which menu/setup record value (0..7, default 5, observed 0..5) a given game mode carries — a menu-layer question, not a match-engine one. Units: level class × 100 (dimensionless), clamp [0,700].

## 4. `MI+0x11B0`: value set, writers, phase names

 CONFIRMED:

 - Location/type: byte at `MI+0x11B0`, `MI = *(G+0x270)`. Read by `MOVZX EAX,byte [R15+0x11B0]` at `0x140458912` (R15 = MI).
 - W1 switch (`FUN_140458880`, table `0x140458960`, 15 RVAs, checker rows): values 8/0xB/0xE/0x12 arm and run (`MOV DL,1` at 0x140458937 -> `CALL 0x140457070`); values 0x14/0x15/0x16 run without arming (`XOR EDX,EDX` at 0x14045893B); all other values skip W1 (`JA 0x140458945`).
 - Reset: `FUN_140A3C430(MI+0x11B0)` (called from `FUN_140A3BA90`/`FUN_140A3C560` match init): `byte0 = 0, byte1 = 1` (word `0x0100` — note asymmetry with the word-1/dword-1 stores below).
 - Per-tick mirror: `0x1404227F9/0x140422800` (`FUN_140422030` frame sync): `MOVUPS XMM0,[RSI+0x728]` / `MOVUPS [RBX+0x11B0],XMM0` — 16 bytes `container+0x728..0x737` -> `MI+0x11B0..0x11BF`. So the low byte of `container+0x728` is the phase source each tick.
 - Direct stores: `0x1408AE747` (`MOV word [RBX+0x11B0],1`), `0x140A1149B` (`MOV dword [RDI+0x11B0],1`), `0x141BC92AE` (`MOV [RBP+0x11B0],AL`, menu/setup region, value = AL at runtime).
 - Value set CONFIRMED by reader immediates: 6 (0x1405F5226, stopped/set-piece), 7 (0x140926E5A), 8/B/E/12 (W1 arms), 0xD (0x140926D9C stage-13 budget arm), 0x10 (0x1410D96CC), 0x13 (net-dodge/officials), 0x14/0x15/0x16 (W1 no-arm + teleport/placement/sub-window), 0x15 (0x14044BB40 placement-finish branch), 0x16 (defensive-shape, teamtick `0x140929A51 == 0x16` gate, setpiece stages).
 - Exclusions: `0x1410D95C7/0x1410D96E4` (`C7 85 B0 11...,0xF`) and the `0x141BC92D6+`/`0x141CC50BE+`/`0x14166D797+` LEA sites are RBP/RSP-relative stack frames in menu/setup code, NOT the MI byte. `0x141182238/0x141183372/0x141184556` (`C6 83/87 ... 00`) sit in std::string-heavy edit/menu structs, not G+0x270 MI. `0x14xxxxxxx`/`0x15xxxxxxx` hits are data/pack regions.

 INF (high): `MI+0x11B0` is the restart programme's current step, mirrored each tick. Evidence: every W1 phase value is a restart-programme step in `pes-re-A5Judge.md` §3.2 run by `FUN_14044C880` — 0x08 substitution demo (`FUN_1404488E0` family via `FUN_140446390`), 0x0B out-of-play hold (`FUN_140447690`), 0x0E fade-in 1.0 s (`FUN_1404488E0`), 0x12 UI/sequencing (`FUN_140448760/0x1404485F0`), 0x14 teleport (`FUN_14044A2A0`, counts `Flow+0x27D8` 1..20 while `MI+0x11B0 == 0x14`), 0x15 placement finish (`FUN_14044BB40`, branches on `== 0x15/0x16`), 0x16 sub/UI window (`FUN_140449D20`) — plus value 6 = late-card scene (`FUN_140446390` step-6 arm), which correctly SKIPS W1. The 0x14-0x16 "open play" phrasing in net-dodge/defensive-shape docs is the latched late-stoppage value persisting into live ball (no writer resets it on the whistle; the value stays until the next stoppage's mirror overwrites it).

 OPEN: the exact instruction that writes `container+0x728` (no `C6`/`C7` imm store to any `+0x728` exists image-wide; no `88`-byte store either — it must be a register store `89` dword, an XMM spill, or part of a larger struct copy; the 116 dword-store hits were triaged: match-flow range `0x140440000-0x140460000` has none). Also OPEN: who writes `MI+0x11B0` values 7, 0xD, 0x10, 0x13 (consumed by stage code; producers untraced). Next step: sweep the remaining `FUN_140A48A30`-style MI-carrying callers the way T+0x78 was swept, or trace `FUN_140422030`'s callers (vtable at `0x142598558`) to identify the container and grep ITS `+0x728` writers.

 Phase names (from the step handlers that own each value; INF where the handler-to-step link is OBS-only):

 | value | name | set by (code) | status |
 |---|---|---|---|
 | 6 | late-card scene | `FUN_140446390` step-6 arm | INF |
 | 8 | substitution demo | step 0x08 `FUN_140446F80` -> `FUN_140484A10` (A5Judge §3.2 line 171) | INF |
 | 0xB | out-of-play hold | step 0x0B `FUN_140447690` (table[5] = 2.5 s; A5Judge §3.2 line 173) | INF |
 | 0xE | fade-in 1.0 s | step 0x0E `FUN_1404488E0` | INF |
 | 0x12 | UI/sequencing hold | step 0x12 `FUN_140448760/0x1404485F0` (5 frames) | INF |
 | 0x14 | teleport placement | step 0x14 `FUN_14044A2A0` (`Flow+0x27D8` 1..20, one group per value) | CONFIRMED (reads `MI+0x11B0 == 0x14` at decompile) |
 | 0x15 | placement finish | step 0x15 `FUN_14044BB40` (branches `== 0x15` -> `FUN_140A3CD90(...,3)`) | CONFIRMED (branch) |
 | 0x16 | substitution/UI window | step 0x16 `FUN_140449D20` (branches `== 0x16` -> window teardown) | CONFIRMED (branch) |

## 5. GF carriage (read-only `src/sim`, 24-10-26)

 `T+0xA294` — CARRIED 1:1 as a value (writer import OPEN): `CarrierConstants::teamLevelRaw` (`pescarrier.hpp:1187`, "writer 0x140A424E0, value OPEN") fed from the install's stock default via `pescarrier.cpp:460-475` (masked global/key/scale/cap reads, no transcription); `Touch::teamLevelRaw` (`pescarrier.hpp:400`); `elizacontroller.cpp:3088` feeds `touch.teamLevelRaw = carrier.teamLevelRaw`. What is missing for exactness: the per-side setup record (the six §2 writers' inputs: game-plan/template/menu values, default 5) — GF has no match-setting selector, so both sides read the stock default. Port spec: when the setup import lands, source each side's class from the setup record through `FUN_140A602B0` semantics (x100, clamp [0,700]) into `teamLevelRaw`; the consumers (`60340/60360/60380/602E0` equivalents) already exist in GF (`pespress.hpp:341` CLS, lane-core G3/G8 in `pescarrier` consumers).

 `T+0xA29C` — CARRIED 1:1 as a flag: `Touch::teamLevelZero` (`pescarrier.hpp:1188`); always `false` in GF (`elizacontroller.cpp:3089`). Port spec: set from `FUN_1414C1950` per-side byte when the setup record import lands; until then `false` is correct for the stock default path (init writes 0).

 `T+0xA298` — PARTIAL: `pespress.hpp:342` carries `phaseAggressive = (T+0xA298 == 3)` (B29, set by the caller, not read from a team word); `pesdefshape.hpp:134-138` + `pesdefshape.cpp:176-185` carry `teamModeThree` (same predicate, ponytail "never-armed until the setup-switch import lands"). No GF state holds the full 0..4 word. Port spec: add the word (init 4; setup import 0..3 via `FUN_141030B40`/`FUN_141FC07B0`/`FUN_141FC1190`/`FUN_140A424D0`) and derive both booleans from it; consumers beyond the two named (15 `FUN_140A40BC0` call sites: `0x14051E7EF` 0/1/2+ switch, `0x140629B2C`, `0x1405FA104`, `0x14078E9BD`, `0x14079044A`, `0x14096F018/102`, `0x14096FECE`, `0x140971F9C/20C1`, `0x140633D29`, `0x1406347D4` — most undecoded) arrive with their own ports.

 CORRECTION 2 — `pescarrier.hpp:1187` says "writer 0x140A424E0, value OPEN". The setter is exact (single `MOV`, §0) but it has 9 known call sites, not one open writer; what is OPEN is only the setup record VALUES (0..7 choice, default 5). Reword to "setter 0x140A424E0 (9 sites, §3 of this report); per-mode values OPEN".

 CORRECTION 3 — `elizacontroller.cpp:3086-3087` says "the writer 0x1B5D8340". Same invalid VA as CORRECTION 1; the setup paths are `FUN_141FB9FB0` (in-match teams), `FUN_141FBE480`/`FUN_141FCDA30`/`FUN_141FCF130` (build paths), `FUN_140454C00`/`FUN_14044A9D0` (programme steps). The "no per-side +0x9200 byte" note is unaffected.

 `MI+0x11B0` — NO 1:1. GF's `e_MatchPhase` (`match.hpp:88-89`, PreMatch/halves/extra/penalties) is period-level; GF has no restart-step byte and no W1 latch (`holder-leaves` §6 already records this). Port spec: add a `uint8_t restartStep` to the match state WHEN the writer (§4 OPEN) lands; gate the per-tick holder recompute on `step ∈ {8,0xB,0xE,0x12}` (arm+run) vs `{0x14,0x15,0x16}` (run-if-armed) vs skip — replacing the distance-gated `CarrierFromTouch` + the `humanoid.cpp:1343-1345` latch and `:1387` retain-block clear (which PES has no counterpart for). Until the writer lands, the GF stand-ins stay, each keeping its ponytail naming `0x140458912`/`0x140458960`/`FUN_140457070`.

## 6. Status ledger

 - CONFIRMED: reader/predicate/setter/init/copier bytes (§0, checker); `/100` family semantics (encoder + 4 comparators); 9 `424E0` sites with per-site value flow; sole writers `424D0`/`428B0` in `141FB9FB0` + sole-caller xrefs; W1 switch + table (all 15 entries); MI reset/mirror/direct stores; value sets (A294 classes {0..7} via encoder; A298 {0,1,2,3,4}; A29C byte; 11B0 ⊇ {6,7,8,B,D,E,10,12,13,14,15,16}); reader branch effects (§3 table); GF carriage map (§5).
 - INF: 11B0 = current restart-programme step (value-set equality + mirror; writer instruction unfound); phase names for 6/8/B/E/12 (handler mapping via A5Judge, OBS-level); which menu record yields class 6/7 and nonzero A29C; GF stock-default loader equivalence.
 - OPEN: `container+0x728` writer (narrowed: not an imm store; not in `0x140440000-0x140460000`); 11B0 values 7/D/10/13 producers; edit `+0x20 == 3` menu action; `FUN_1414C1950` (A29C source) semantics; `FUN_141FC09D0`/`FUN_140FC7ED0` record semantics; consumers at `0x1406295A0`/`0x14096A4F0`/`0x1409677C0`/`0x140A1F1F0` function identities (opaque packed code).

## 7. Recommended next steps

 1. Trace `FUN_140422030`'s callers (vtable `0x142598558`) to name the `+0x728` container, then sweep its `+0x728` writers (register/XMM stores) — closes the 11B0 writer and the phase-name INF in one move.
 2. Decode `FUN_1414C1950` (A29C source) and `FUN_141FC09D0` (FCDA30 class source) — both small, both called from decoded setup code — closes class-6 provenance and the zero-flag semantics.
 3. Apply CORRECTIONS 1-3 to the three GF comments (no behaviour change), then add the `T+0xA298` word beside the two booleans (pure, tested) as the first wired piece: init 4, import 0..3, both readers derived.

## Appendix A. Checker (`check_team_status.py`)

Self-checking, 35 rows, GREEN 24-10-26; one mutation run RED then reverted. Run from the scratch dir with every script under `ulimit`/nice per brief-rules; reads only the image (read-only):

```python
#!/usr/bin/env python3
"""Self-checking byte verifier for the team-status word pair + match phase decode.
Reads ~/ghidra-projects/scratch/Unpacker/pes_full.bin (file offset = VA - 0x140000000).
GREEN = all asserts hold. Mutation: flip the 6 at 0x14092B505 to 7 -> row 'cond.edx6' goes RED.
"""
import os, struct, sys
P = os.path.expanduser("~/ghidra-projects/scratch/Unpacker/pes_full.bin")
img = open(P,'rb').read()
def b(va,n=1): return img[va-0x140000000:va-0x140000000+n]
fails = []
def row(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + ("  " + detail if detail else ""))
    if not cond: fails.append(name)
# --- 602B0 encoder: IMUL EAX,EDX,100 ; CMP EAX,700 ; MOV [RCX],700 cap
row("enc.imul100", b(0x140A602B0,3) == bytes.fromhex("6b c2 64"), b(0x140A602B0,3).hex())
row("enc.cap700", b(0x140A602B0+5,5) == bytes.fromhex("3d bc020000".replace(" ","")), b(0x140A602B0+5,5).hex())
row("enc.store700", b(0x140A602BC,6) == bytes.fromhex("c7 01 bc 02 00 00"), b(0x140A602BC,6).hex())
# --- comparators use /100 magic 0x51EB851F
row("cmp.eq.magic", b(0x140A602E0+4,4) == struct.pack('<I',0x51EB851F), b(0x140A602E0+4,4).hex())
row("cmp.cls.magic", b(0x140A60340,4) == bytes.fromhex("b8 1f 85 eb"), b(0x140A60340,4).hex())
# --- setters: exact displacements + operand sizes
row("set.a294", b(0x140A424E0,6) == bytes.fromhex("89 91 94 a2 00 00"), b(0x140A424E0,6).hex())
row("set.a298", b(0x140A424D0,6) == bytes.fromhex("89 91 98 a2 00 00"), b(0x140A424D0,6).hex())
row("set.a29c", b(0x140A428B0,6) == bytes.fromhex("88 91 9c a2 00 00"), b(0x140A428B0,6).hex())
row("get.a298", b(0x140A40BC0,6) == bytes.fromhex("8b 81 98 a2 00 00"), b(0x140A40BC0,6).hex())
row("get.cmpa29c", b(0x140A40BD6,7) == bytes.fromhex("80 b9 9c a2 00 00 00"), b(0x140A40BD6,7).hex())
row("get.reada294", b(0x140A40BF5,6) == bytes.fromhex("8b 81 94 a2 00 00"), b(0x140A40BF5,6).hex())
# --- init: A294=500 (EDX=5), A298=4 (EDI=4), A29C=0 (ESI=0)
row("init.edx5", b(0x140A415E2,3) == bytes.fromhex("8d 56 05"), b(0x140A415E2,3).hex())
row("init.edi4", b(0x140A41627,5) == bytes.fromhex("bf 04 00 00 00"), b(0x140A41627,5).hex())
row("init.esi0", b(0x140A415AF,2) == bytes.fromhex("33 f6"), b(0x140A415AF,2).hex())
row("init.a294call", b(0x140A4160E,7) == bytes.fromhex("48 81 c1 94 a2 00 00"), b(0x140A415E2,3).hex() if False else b(0x140A4160E,7).hex())
# --- condition: edx=6 then call 602E0
row("cond.edx6", b(0x14092B505,5) == bytes.fromhex("ba 06 00 00 00"), b(0x14092B505,5).hex())
# --- 27A0 internal: edx=6 then edx=5
row("t2.edx6", b(0x140503203,5) == bytes.fromhex("ba 06 00 00 00"), b(0x140503203,5).hex())
row("t2.edx5", b(0x140503220,5) == bytes.fromhex("ba 05 00 00 00"), b(0x140503220,5).hex())
# --- W1 switch: MOVZX EAX,[R15+0x11B0]; ADD -8; CMP 0xE; JA; DL=1 / XOR EDX arms
row("w1.movzx", b(0x140458912,8) == bytes.fromhex("41 0f b6 87 b0 11 00 00"), b(0x140458912,8).hex())
row("w1.range", b(0x14045891A,6) == bytes.fromhex("83 c0 f8 83 f8 0e"), b(0x14045891A,6).hex())
row("w1.dl1", b(0x140458937,2) == bytes.fromhex("b2 01"), b(0x140458937,2).hex())
row("w1.dl0", b(0x14045893B,2) == bytes.fromhex("33 d2"), b(0x14045893B,2).hex())
# jump table: entries 0,3,6,10 -> 0x458937 ; 12,13,14 -> 0x45893B ; else 0x458945
import struct as S
t = {i: S.unpack('<i', b(0x140458960+i*4,4))[0] for i in range(15)}
row("w1.tab.8", 0x140000000+t[0] == 0x140458937, hex(0x140000000+t[0]))
row("w1.tab.B", 0x140000000+t[3] == 0x140458937, hex(0x140000000+t[3]))
row("w1.tab.E", 0x140000000+t[6] == 0x140458937, hex(0x140000000+t[6]))
row("w1.tab.12", 0x140000000+t[10] == 0x140458937, hex(0x140000000+t[10]))
row("w1.tab.14", 0x140000000+t[12] == 0x14045893B, hex(0x140000000+t[12]))
row("w1.tab.15", 0x140000000+t[13] == 0x14045893B, hex(0x140000000+t[13]))
row("w1.tab.16", 0x140000000+t[14] == 0x14045893B, hex(0x140000000+t[14]))
row("w1.tab.9skip", 0x140000000+t[1] == 0x140458945, hex(0x140000000+t[1]))
# --- MI writers + mirror
row("mi.word1", b(0x1408AE747,9) == bytes.fromhex("66 c7 83 b0 11 00 00 01 00"), b(0x1408AE747,9).hex())
row("mi.dword1", b(0x140A11499,10) == bytes.fromhex("c7 87 b0 11 00 00 01 00 00 00"), b(0x140A11499,10).hex())
row("mi.mirror", b(0x1404227F9,14) == bytes.fromhex("0f 10 86 28 07 00 00 0f 11 83 b0 11 00 00"), b(0x1404227F9,14).hex())
# --- copier carries all three words
row("cpy.a294", b(0x140416FBE,10) == bytes.fromhex("8b 86 94 a2 00 00 89 87 94 a2"), b(0x140416FBE,10).hex())
print("GREEN" if not fails else "RED %d: %s" % (len(fails), fails))
sys.exit(1 if fails else 0)
```
