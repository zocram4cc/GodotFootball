# Team/AI state spec: the team-tick path's records (TeamStateMap, 18-11-26)

What Wave-1 needs before touching the team/AI line (team accessor, team tick, PressAI/PressCut).
Image `pes_full.bin`, base `0x140000000`. CONFIRMED = tool output this session or a cited doc's
CONFIRMED; INF = inference; OPEN = blocker named. No GF code changed, no captures unpacked.

## 0. VA self-check (script, with mutation)

Light Python read of the image (flat mapping: file offset = VA - 0x140000000):

- Control: float at VA `0x14256AE1C` = `1.0` (CONFIRMED this session).
- Mutant: float at `0x14256AE20` = `-8.68e-31` (not 1.0: the read is address-sensitive).
- `.pdata` (file offset `0x3AC5000`, 12-byte entries) contains an entry with begin
  `0x4F6C30` end `0x4F7AC9`: team tick `0x1404F6C30` is `.pdata`-anchored (CONFIRMED).
- Prologues look like functions: `0x1404F6C30` = `48 895C24 2055 5657`,
  `0x14041ECF0` = `41 5641 5748 83EC 38`, `0x140579840` and `0x140536FC0` =
  `48 895C24 0855 5657` (CONFIRMED bytes, not decoded further here).

## 1. Roots covered (all AI-tagged in LEDGER.md)

Team tick `0x1404F6C30`, owner H slot 2 `0x14041ECF0`, plan selector `0x140500B90`,
CPU strategy `0x14051E970`, attack level `0x1404F8350`, concept selector `0x140579840`,
concept start `0x1406424A0` (leaf), style gate `0x140A62180`, pass/shot block `0x140A08EA0`,
accessors `0x140A48A30` (T) / `0x140A48A10` (man), dispatcher `0x140535C60`,
MATCH_UP writer `0x140536210`, press writer `0x140536FC0`, coordinator `0x140634470`,
eval `0x140633940`, partner matcher `0x1405346E0`, man think `0x14092ED30`,
arrival writer `0x14045E0D0`, ctor `0x1404F0530`. Docs: `docs/re/pes-re-teamtick.md`,
`docs/ports/pes-port-AIConcepts.md`, `pes-port-PressAI.md`, `pes-port-PressCut.md`.

## 2. Field table (PES fact -> writer -> GF status on origin/main)

Widths: u8=1, u16=2, u32/f32=4, u64=8, s8=signed 1. "Writer" is the
displacement-scan result where the docs record one (team-tick doc §2.2 body scan,
§3.4/§4 exclusion scans: `store_scan.txt`, `binder_stores.txt`), else OPEN.
GF file:line is the holding or the `// ponytail:` that names the PES site.

| # | Record.field | Off | W | PES writer (VA or OPEN) | GF status on main |
|---|---|---|---|---|---|
| 1 | G+0x270 MI ptr | 0x270 | u64 | Per-tick set/clear OPEN (setup `0x140A49C3B`, teardown `0x140A48210` are match setup/destructor, not per-tick; team-tick doc §4.1) | HELD bytes (`pesworldrecord.hpp:59,128`), set by GF setup: STAND-IN |
| 2 | G+0x180 roster table | 0x180 | 40B x N | Same binder OPEN as #1 (flips with MI live, §4.1) | HELD bytes, GF-filled: STAND-IN |
| 3 | G+0x1ef8 man-table base | 0x1ef8 | 0x28 stride | Accessor `0x140A48A10` reads (LEDGER diffed real) | HELD (`pesworldrecord.hpp:94,135`); accessor translated |
| 4 | G+0x1928 side-table base | 0x1928 | 0x28 stride | Accessor `0x140A48A30` reads (LEDGER diffed real) | HELD (`pesworldrecord.hpp:91,132`); accessor translated |
| 5 | MI+0x1168 period | 0x1168 | u32 | H gate `MI+0x1168==0xa` skip (`0x14041ED07`, CONFIRMED raw §2.1) | HELD bytes (`pesworldrecord.hpp:64`); writer GF clock: STAND-IN |
| 6 | MI+0x116C in-play | 0x116C | u32 | Press hooks predicate on it; writer not decoded | HELD bytes; STAND-IN (`team.cpp:473` ponytail) |
| 7 | MI+0x1198 frame counter | 0x1198 | u32 | `0x140A128E4` increments (clock doc) | HELD bytes (`pesworldrecord.hpp:78`); STAND-IN (`team.cpp:460` ponytail: no GF counterpart) |
| 8 | MI+0x1748 on-pitch flags | 0x1748 | u8 x 22 | Writer OPEN (read by writer gate, run states) | HELD bytes; populated by GF: STAND-IN |
| 9 | H+0x450+8s brains, H+0x970 T bind, H+0x460 planner gate | - | u64s | H body `0x14041ECF0` writes per frame (§2.1 steps c/d/e) | MISSING: no H/brain object in GF window |
| 10 | brain+0x6d48/+0x6d50 tick ctx | - | u64s | Tick body steps 0/48 (set/clear: "inside the tick" flag, §2.2) | MISSING (no brain object); GF `TeamTick` struct is a STAND-IN (`pesteamtick.hpp`) |
| 11 | T+0x78 pass target | 0x78 | u32 | OPEN (candidate `0x14093A089` refuted, §4.2); readers: side planner `0x1404405D0`, `0x140536FC0` | HELD bytes; writer GF `RunPesPassTarget`: STAND-IN (counter-B ponytail, `team.cpp`) |
| 12 | T+0x7C MATCH_UP man | 0x7C | u32 | `0x140536210` (proven 68/68 on PressAI branch, NOT on main); cleared every tick | HELD bytes (`pesworldrecord.hpp:298`); STAND-IN (`pesworldrecord.cpp:120` ponytail) |
| 13 | T+0x80 restart word | 0x80 | u8 | Tick body step 23 (`=0xff`) | GF `TeamTick`/WriteRestartWord: STAND-IN (`pesteamtick.cpp:66-72`) |
| 14 | T+0x8C tick flag | 0x8C | u8 | Step 5 (`=1`, via `0x140501260`) | HELD bytes; no GF writer: STAND-IN by omission |
| 15 | T+0x134 T-LVL word | 0x134 | u32 | Tick body step 10-11 (`= brain+0xbcc`) | GF `word134` (`pesteamtick.cpp:41`): STAND-IN |
| 16 | T+0x250 attack angle | 0x250 | f32 | Staged from match setting | HELD (`pesworldrecord.hpp:285,991`): STAND-IN staged |
| 17 | T+0x254 attack sign | 0x254 | s8 | Reader `0x140929A00`; writer in undecoded level pull | HELD (`pesworldrecord.hpp:286,1007`): STAND-IN staged |
| 18 | T+0x255 possession latch | 0x255 | u8 | Step 9 via `0x140A428D0`; read by `0x14092C9F0`, `0x140929A00` | HELD bytes; GF latch: STAND-IN |
| 19 | T+0x26C+0x40s targets, +0x278 queued, +0x28C+0x40s sub-kind | 0x26C+ | u32s | `0x140A429A0` / `0x140A42B80` (handoff §5.2) | HELD bytes; GF writers: STAND-IN |
| 20 | T+0x6E6 gate word | 0x6E6 | u16 | Tick body step 23 (cond `=0`) | GF: STAND-IN (`pesteamtick.cpp:70` ponytail: no GF counterpart) |
| 21 | T+0xB8A4 attack level | 0xB8A4 | u32 | `0x1404F8350` drift (translated closure) | GF `TeamTick` level (`pesteamtick.cpp:58`): STAND-IN |
| 22 | T+0xB8A2/B8A3 full-attack/defence flags | 0xB8A2 | u8s | Tick body step 15 | GF WriteAttackFlags: STAND-IN (`pesteamtick.cpp:64`) |
| 23 | T+0xB6DE stoppage byte | 0xB6DE | u8 | Step 40-42 via `0x140509170(side)` | BEYOND 0xA2A0: MISSING (no GF byte) |
| 24 | T+0xA2A0[0..10] run/concept states | 0xA2A0 | u8 x 11 | Step 33 via `0x140A429A0` (=1), step 36 (`0x140525530`, `0x140538050`), `0x140536FC0`, mode-4 via `0x140537C60` | Slot 0 HELD (= last byte `0xA29F` boundary); slots 1..10 BEYOND: MISSING. GF run slots live in `pesrunslots` (plain C++): STAND-IN (AIConcepts §3.1) |
| 25 | T+0xA240 order words | 0xA240 | u32s | Concept stop `0x14057A3E0` via `0x140A40820` | HELD bytes; GF writer: STAND-IN |
| 26 | T+0xA294 team level | 0xA294 | u32 | `0x140A424E0` (one mov; match-setting caller `0x141FBA47D`) | HELD (`pesworldrecord.hpp:301`); staged from import: STAND-IN (`team.cpp:95` ponytail) |
| 27 | T+0xA548/A55C MA-build words | 0xA548 | u32s | Step 39 via `0x140535C60` | BEYOND 0xA2A0: MISSING |
| 28 | T+0xB6E8 running flags (48) | 0xB6E8 | u8 x 48 | Concept executor `0x140579BF0` via | MISSING (AIConcepts §3.1: record too small) |
| 29 | T+0xB718 per-concept byte, T+0xB748 order words | 0xB718+ | u8s | Selector `0x140579840` reads; `0xB748==0x26` starts fresh press (dispatcher loop, NOT ported) | MISSING; `pespress.hpp:322 orderTwentyFour=false`: STAND-IN |
| 30 | MA+0x22E4C+200i nearest list | 0x22E4C | 200B x 22 | OPEN (§3.4: 32/320 base sites eliminated, computed `imul 0xC8` base missed; candidates `0x140534A20`, `0x140536210`, `0x140536690`, `0x140634470`, `0x140635AB0`, `0x140628400`) | HELD bytes + GF `SortedRow::nearest` views (`pesscorerows.hpp:117-119`), populated by GF builder: STAND-IN writer |
| 31 | MA+0x2482C ball copy | 0x2482C | 3x f32 | OPEN, changes every frame (`pes-re-advance.md`) | HELD (`pesworldrecord.hpp:369`); GF stages ball: STAND-IN (`teamAIcontroller.cpp:202` ponytail) |
| 32 | MA+0x2483C carrier id | 0x2483C | u32 | `0x140A627E0(ctx,0)` else-branch | HELD (`pesworldrecord.hpp:370`); GF holder slot: STAND-IN (`teamAIcontroller.cpp:204` ponytail) |
| 33 | MA+0x2601C owner slot | 0x2601C | u32 | Reader carrier walk; writer not decoded | HELD (`pesworldrecord.hpp:372`); STAND-IN staged |
| 34 | MA+0x24908 arrival list | 0x24908 | sorted block | `0x14045E0D0` (LEDGER ported; translated 12/12 on TeamTick4 branch) | HELD bytes; GF `RunArrivalList` on counter-B STAND-IN input (`team.cpp:400` ponytail) |
| 35 | man+0x55C/0x564 pos, +0x5D0 action, +0xC6BC stamp | man+ | f32/s32 | Pos: mover; +0x5D0 readers steps 33/`0x1406CBF70`/`0x140536FC0` (writer in player driver); stamp writer `0x14092C9F0` | HELD (`pesworldrecord.hpp:194-198`); positions staged, action word GF-side: STAND-IN |
| 36 | slot+0x1430 A / +0x1438 B counters | slot+ | s32s | `0x14092ED30` man think (proven 8/8 real on PressCut-wip, NOT on main) | HELD bytes; counter B = GF time-to-ball: STAND-IN (blocks T+0x78 stability) |
| 37 | AI+0x8d4 request code | AI+ | u32 | `0x140929A00` (consumer undecoded) | MISSING (no AI object in GF window) |
| 38 | ctx+0x18 MA ptr, +0xE0 E ptr | ctx+ | u64s | Tick builds ctx on stack before step 33 (AIConcepts §2: at tick entry it is not there) | HELD (`PesCtxRecord`, `pesworldrecord.hpp:351-359`); staged per translated call: HELD on proven paths |
| 39 | E stats block +0x750D0+side*0x59650 (+0x59530 anti-tamper cells) | E+ | mixed | `0x1408C6B80` ctor + `0x1408CA340` reset (proven 4/4+4/4 on PressCut-wip, NOT on main) | MISSING on main (PressCut `ConstructStats`+`ResetEvents` keeps it) |
| 40 | team-info record (table B): [0] side, [1..] squad-id list, +0xA8 blocks | info+ | mixed | `0x140A3B3A0` ctor + `0x1408C7510` squad init (proven synthetic on PressCut-wip) | HELD (`PesTeamInfoRecord`); squad list = GF slot order: STAND-IN (`pesworldrecord.cpp:431` ponytail: PES map is a permutation) |
| 41 | P+0xAB8 anime/planner objects (`0x1406B9270`->`0x140707EE0`->`0x140708290`) | P+ | vtable | Null vtable in GF window (PressCut checkpoint 5: man-think deep arm aborts) | MISSING: the BodyAnimControl seam (OneWorld cut 4). Blocks counter-B wiring |

Counts (41 rows): HELD-with-PES-writer-on-main 2 (#3, #4 accessors only);
HELD-bytes-with-GF-STAND-IN-writer 24; MISSING (no GF byte or no value) 15
(#9, #10, #23, #24-slots-1..10, #27, #28, #29, #37, #39, #41, plus #1/#2 binders'
per-tick writers OPEN). Stand-in rows in LEDGER AI subsystem: 2 (the ledger
counts only GF-cited-with-ponytail VAs; the table above lists every GF-side
stand-in including unstaged ones).

## 3. Memory windows the calls need (fixture from each call's replay read set)

- Image window: `pes_full.bin` bytes + read-only tables (`ro_tables.json`, e.g.
  extents `0x1425DD988`/`0x1425DDA50` TENTATIVE) — read-only, always mapped.
- Engine window (`PesMem::window` at `kWorldWindowVa = 0x2000100000`,
  `pesworldrecord.hpp:30,680-694`): G header/tables, 26 player blocks
  (22 + 4 officials, `kPlayerBlockBytes 0x3F40`), MI (`0x1780`), 2 side records
  (**only `0xA2A0`; fields #23, #24b, #27, #28, #29 need it grown to `~0xB760`**),
  2 team-info records, ball (`0x2960`), pitch, frame-random, 2 ctx (`0x100`),
  2 MA (`0x26040`), event store, 2 carrier blocks, trap data, scratch, stack.
- OS arena: the chain objects (A = P+0xA08, B = A+0xF0, O2/O1), the anime
  interface objects, the planner virtual objects (#41). **Lives in the OS arena,
  NOT in any window on main.** `SharedWindows` (arena mapping) exists only on
  `OneWorld-series-wip` (integration map §2b: 0 hits on main). Every translated
  call touching them aborts on main — the Round 8-11 hunt repeating.
- Globals: G cell `0x1436F3DC8` (mapped to the window), image vtable/DB
  pointers, competition (null), clip DB global. TLS/TEB + stack: translated
  calls run on the window stack; the trap stack once overran slot 21 + missing
  TLS block (Round 2).
- Fixture status: NO engine fixture on main (`tests/` has only
  `movestep_team_synthetic.hpp`; T1 `tests/pes_engine_fixture.*` not built).
  The implementer must build the read-set-derived fixture first (Branch
  EngineFixture): emit per-call window lists from `snapdiff --bounded` read
  sets + standard windows (image, world, OS arena, TEB/TLS, stack), with a
  build-time check naming any unmapped read address.

## 4. Translated/proven coverage (LEDGER.md current) and captures

- LEDGER in-scope: AI 1362 rows (ported 40, diffed 327 real, translated 395
  closure, stand-in 2, decoded 194, undecoded 340); aiconcepts 11 rows,
  translated 11. Path roots: `0x1404F6C30` diffed real (3 calls);
  `0x140579840` diffed real (15: 3 + 12 in-concept); `0x14051E970`,
  `0x140A62180`, `0x140A48A30`, `0x140A48A10` diffed real; `0x1404F8350`,
  `0x140500B90`, `0x140535C60`, `0x140536210`, `0x140536FC0`, `0x140634470`
  translated closure; `0x140633940`, `0x14045E0D0`, `0x140A08EA0` ported;
  `0x1406424A0` leaf; `0x1404F0530`, `0x1405346E0`, `0x14092ED30` decoded
  (the 24/24 dispatcher, 3/3 matcher and 8/8 man-think proofs live on
  unlanded PressCut-wip/PressAI; AIConcepts proofs landed via batch BC).
- Registry (`run/capsets.json`, 35 sets): the four team sets are status **gone**
  (captures deleted, folders absent from `live2/`): `team_tick_pid296049` (3),
  `team_pass_pid1792161` (1), `ai_arrival_pid1887719` (12),
  `carrier_probes_pid2392772` (11). **No press/selector/concept set is
  registered at all** (no `press_*`, `aic_*`, `dispatch`, `partner`, `manthink`
  name matches). The PressAI/AIConcepts proofs cite `live2/pid296049` (writer
  23, eval/coord 3/3) and `live2/pid42407` (12 selector) captures that are
  neither registered nor on disk. Proofs persist only as `results_*.json`
  records (+ closures/specs on their branches). Re-replay from the registry is
  impossible: re-arm `ai_team_tick_4F6C30`, `team_pass_4FB2A0`,
  `ai_arrival_14045E0D0`, press writer/coord/eval, dispatcher `0x140535C60`
  (EVERY 101 + `b(r8+0x255)==0`), matcher `0x1405346E0` hooks before Wave 1.

## 5. First 3 red tests (each from a PES fact above)

- T1 pass-target hold (fact: kicking side T+0x78 = 6 across selector calls 2..12,
  `teamsel_1404FB2A0` oracle, PressCut §8/#11): stage the kick-off scene, run
  GF ticks 1..8, assert T+0x78 names one man on every tick. Red now (flips 7/6:
  counter-B stand-in, #36).
- T2 mark words (fact: defending side slots 4/7/8/10 = `0x36` MARK from the
  2nd dispatcher call, slot 5 = `0x32` COVER from the 6th; writer dispatcher
  `0x140535C60` defence passes, #24-29): run GF's dispatcher on the window,
  assert the five words at those calls. Red now (passes unwired on main).
- T3 nearest-list contract (fact: MA+0x22E4C layout, team-tick doc §3.1: per-team
  ascending d², k-th entry has rank[id]=k, self rank 10, empty slot id `0xFF`
  d² `0.0`, counts at +0xB0/B1): build a 22-man scene, run GF's ScoreRows
  writer, assert all four invariants on the window bytes. Pins the contract the
  still-OPEN PES builder (#30) must satisfy; red if GF's rows violate any arm.

## 6. Open items for Main

1. Grow `kSideRecordBytes` past `0xB760` (fields #23-29) or confirm the smaller
   growth the selector/dispatcher proofs actually read.
2. Re-arm the hooks in §4 (team tick, pass, arrival, press trio, dispatcher,
   matcher) — none are capturable today.
3. Decide EngineFixture ownership (T1) before Wave 1 starts: without the
   arena window + unmapped-read check, the first translated call repeats
   Rounds 8-11.
