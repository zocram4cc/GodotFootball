# Scoping: unifying GF's two PES address spaces

Status: investigation only, no production code (AnimeLayer11, 14-12). Branch `AnimeLayer` at `861f1a7e`.
Every claim below is either a cited file:line from the tree, a measured number (labelled), or [INFERENCE].

## 0. Summary

- Both worlds already map the same PES image (462,929,920 bytes, about 441.5 MiB) privately, copy-on-write (`pesfoxworld.cpp:60`, `pestrapdata.cpp:272`). The VA ranges of the two worlds are disjoint except for the heap base `0x2400000000`, which both use for their own heap. So a single address space is feasible at the VA level.
- The hard part is not the addresses. It is the global state. The fox world runs PES's boot (reset of the static data, static initialisers, the file manager, the engine init) and the trap world runs none of it: it uses the dump's globals as they are, plus its own loader writes. The two states differ, and the trap loader's globals all lie inside the boot's zeroed range. So one image forces a choice: which global state the trap chain runs on, and in what order the writes happen.
- The fox heap is committed at 768 MiB per match (`matchevents.cpp:71`, `pes_native.cpp:33`: a zero-filled `std::vector`). That is the largest single RSS item per match, which the documented 2.3 GiB per match already includes (`simslot.sh`). A copy-on-write template of the booted state, shared across matches within a process, removes most of it.
- Recommended: cut 0 (measure the boot's write set and the touched pages), then cut 1 (extract one memory class and one boot function, behaviour identical), cut 2 (boot once per process into a memfd template, clone per match, behaviour identical, hashes must not move), cut 3 (one memory per match: the trap window and loader join the clone; the trap chain now sees booted globals; the pes-loop hash moves and is re-baselined), cut 4 (the three seams close in that one memory), cut 5 (cleanup). Every cut keeps the 31 direct test files, the 192-test kick/throw/roster ctest subset, and the switch-off smoke green or with a recorded reason.

## 1. What each world holds and how it is built

### 1.1 The fox world (`PesFoxWorld`, `src/sim/onthepitch/pesfoxworld.{hpp,cpp}`, 118 + 441 lines)

| Piece | Where | Size / VA | Lifetime |
|---|---|---|---|
| PES image, private COW mapping | `pesfoxworld.cpp:60` (MAP_PRIVATE of the dump) | 462,929,920 B at `0x140000000` | per match (one world per match) |
| PES heap (`PesHeap`, bump + free list) | `Impl::heap`, `pes_native.hpp:19`, arena `std::vector` at `kArenaVa 0x2400000000` (`pes_native.hpp:40`) | 768 MiB (`matchevents.cpp:71`, `kPesAnimeHeapBytes`), committed (zero fill, `pes_native.cpp:33`) | per match |
| Stack | `kStackVa 0x2600000000`, 1 MiB (`pesfoxworld.cpp:24`) | | per match |
| TEB | `kTebVa 0x2610000000`, 8 KiB (`pesfoxworld.cpp:25`) | | per match |
| OS arena (VirtualAlloc model) | `kOsArenaVa 0x3000000000`, 4 GiB, MAP_NORESERVE (`pesfoxworld.cpp:35,64`) | lazily touched | per match |
| Zeroed .bss-like data | `ZeroInitializedData`: memset `0x1436CF800..0x143AC5000` (about 4.0 MiB) (`pesfoxworld.cpp:251,291`) | in the image | boot |
| Restored .data | `RestoreInitialData`: copies the install exe's empty-hash pattern (`pesfoxworld.cpp:303`) | in the image | boot |
| Boot | `Boot()` (`pesfoxworld.cpp:255`): zero data, restore, reset static-init state, scratch arena and TLS, `malloc_init_hard`, `RunStaticInitializers` (`kSelectedInitializers`, :293), file manager, Lua pool, engine init `FUN_141AE77B0` | | per match, lazy |
| Body archives | `LoadBody` (`pesfoxworld.cpp:350`), through the VFS | | per match |
| Anime control per man | `ConstructAnime(p, 0)` runs `FUN_141EB5250` on a 0x3C00 block; it stores `BodyAnimControl` at `p+0xA08` (`pes_fn_141EB5250.cpp:137`); `Match::PesAnimePlayerOf` (`matchevents.cpp:88-99`) | 0x3C00 B per man | per match, per man |
| Object vtables | the constructors write `0x142ABA1A8` (control) and others | | boot |
| Name registry | `DAT_1437F0AC8` (read by `FUN_141B55F40`; written only by PES's static initialisers, which only the fox boot runs) | | boot |

Lazy creation: `Match::PesAnimeWorldOf()` creates the world on the first anime use (`matchevents.cpp:76-86`). In a PES-loop match that is the first PES clip (`humanoidbase.cpp:489` routes every issued clip to `PesAnimeIssue`). So the boot happens at the start of a PES-loop match.

Readers and writers: written by `PesAnimeWorldOf`, `ConstructAnime`, `PesAnimeIssue` (`matchevents.cpp:101-103`, humanoid `PlayIssuedClip`), and the engine frame step (`teamAIcontroller.cpp:3654`). The render path reads the pose (`src/gdext/pes_animation.cpp`; [INFERENCE] not re-verified here).

### 1.2 The trap world (`PesWorld`, `pesworldrecord.{hpp,cpp}` 1327 + 1050 lines; `pestrapdata.{hpp,cpp}` 137 + 283 lines)

| Piece | Where | Size / VA | Lifetime |
|---|---|---|---|
| Window (`PesWorldImage`: header, 26 player blocks, team, ball, pitch, AI, MA, brain, ctx, roster, events, carriers, trap records, stack, scratch) | bytes inside the `PesWorld` object; `kWorldWindowVa 0x2000100000` (`pesworldrecord.hpp:33`), `kWorldBytes = sizeof(PesWorldImage)` (`:798`) | about 0.5 MiB [INFERENCE from 26 x 0x3F40 + 0x2E58 + tables; to be measured] | per match (`match.cpp:111`) |
| Trap records (P per slot) | `PVa(slot) = kWorldWindowVa + kTrapOffset + slot*sizeof(PesTrapImage)` (`pesworldrecord.hpp:974`) | inside the window | per match |
| Per-call memory | `Bind` (`pesworldrecord.cpp:19-28`): window at `kWorldWindowVa`, an 8-byte segment `kWorldGlobalVa 0x1436F3DC8` holding the window VA, stack in the window (`kStackVa`) | | per call |
| Image for trap calls | `s.mem.flat = ext.image` (`pesworldrecord.cpp:318`); `ext.imageBase = 0x140000000` (`pestrapworld.hpp:250-257`) | | |
| Trap loader segment | `kTrapLoaderVa 0x2100000000` (`pestrapdata.hpp`): Mbinfo motion records 1125 x 0x390 B = 1.6 MiB, eyes table 0x4494, Animetable 0xBE5 x 0x12, trap.o, ball.o; [INFERENCE] about 2 MiB total | once per match (copied from the process-wide source) | |
| Image globals the loader writes | `kMotionTableGlobalVa 0x1436F23C0`, `kAnimeManagerGlobalVa 0x1436F2378`, `kRegistryGlobalVa 0x1437F4808`, `kEyesTargetGlobalVa 0x1436F2648`, `kPadManagerGlobalVa 0x1437F0A58`, the window global `kWorldGlobalVa 0x1436F3DC8`, and the anime chain's `kClipDatabaseGlobalVa` (`pesanimechain.cpp:247`) | in the image | written on every call (`pesworldrecord.cpp:323`: `s.mem.wr(va, value, 8)`) |
| Trap heap | `PesWorld::heap_`, a `PesHeap` with the default 8 MiB arena (`pesworldrecord.cpp:95,169,207,223,239`; `kDefaultArenaBytes`, `pes_native.hpp`), at `kArenaVa 0x2400000000` | 8 MiB, committed | per match, lazy |
| Image mapping | `PesTrapMemory`: MAP_PRIVATE COW of the same dump (`pestrapdata.cpp:270-281`) | the same 441.5 MiB file, a second private mapping | per match |
| Trap source | `LoadTrapSource(...)` once per library instance (`src/gdext/pes_library.cpp:209`); the image is the user's memory dump (`pestrapdata.hpp:112-122`) | | per process |
| Physique stand-in at `P+0xA08` | one 0x200-byte object, G only (`pestrapworld.hpp:141-146`) | | per match |

Readers and writers: the trap world is written by the translated trap, joint and push functions (`PesWorld::RunTrapKind0`, `JointPosition`, `PushGateOpen`, `RunAnkleShove`, `CallPesFn`), `RunPesTrapTicks` (`teamAIcontroller.cpp:3667`), the actor construction (`pestrapactor.cpp`, `ConstructActorP`, `ConstructAnimPlugin`, `match.cpp:296-297`), and per tick (`match.cpp`). It is read by the GF sim (`PesWorldOf().P(slot)`, positions `pestrapactor.cpp:109,156`, `TeamAIController::PushPieceTakerBehindLine`, `teamAIcontroller.cpp:1457-1486`).

### 1.3 The boot policy: the one fact that decides the design

The trap world never calls `Boot`, `ZeroInitializedData`, `RestoreInitialData`, `ResetStaticInitState` or `RunStaticInitializers`. A grep of `pestrapdata.cpp`, `pesworldrecord.cpp` and `pestrapactor.cpp` finds none of them. The trap chain therefore runs on the dump's globals (the dead game's state, with its dead heap pointers) plus the loader's writes. The fox world runs the boot, which resets that state and builds the live objects. The two global states are different objects.

## 2. Measured facts that constrain the options

1. VA map, current. Image `0x140000000 + 441.5 MiB`; trap window `0x2000100000 + kWorldBytes`; trap loader `0x2100000000`; heap `0x2400000000` (fox 768 MiB, trap 8 MiB, one each); fox stack `0x2600000000`; fox TEB `0x2610000000`; fox OS arena `0x3000000000` (4 GiB, NORESERVE). Only the heap base is shared, and each world uses its own copy. A merged layout needs one heap of about 776 MiB at `0x2400000000`; the rest is disjoint.
2. Global overlap (the boot's zero range versus the trap's globals). All six trap loader globals lie inside `0x1436CF800..0x143AC5000` (the boot's zeroed range, `pesfoxworld.cpp:251`). So in one image the boot's zeroing must come before the loader's writes, and each trap call's global writes (`pesworldrecord.cpp:323`) must not clobber fox state at the same VAs. Which globals the translated boot writes is NOT visible in GF source: it needs measuring (cut 0).
3. Boot content. The boot's static initialisers are what create the PES objects the seams need: the anime control (`ConstructAnime`), the name registry `DAT_1437F0AC8` (for the DemoInfoRef writer), and the press planner's virtual objects (PressCut: `0x1406B9270 -> 0x140707EE0 -> 0x140708290`, counter B "held", `docs/ports/pes-port-PressCut.md`). None of them exists in the trap world today (a physique stand-in replaces `P+0xA08`, `pestrapworld.hpp:141-146`).
4. Cost of the boot. The 12-test AnimeEngine suite reports 1928 ms in total (`merge_anime.log`, merged build). That includes the fixture's boot [INFERENCE: the boot is most of it; measure in cut 0]. Per match the fox boot is paid once, at the first PES clip.
5. Memory. Machine: 78 GiB total, 59 GiB available (`free -g`). Simslot: 6 slots, minimum available 5,800 MiB (`simslot.sh`). Documented VmRSS per match: 2.3 GiB after the FlatKeys cut (`simslot.sh` comment). The fox heap (768 MiB) is committed per match, so a second boot with its own heap adds about one third per match.
6. Captured oracles replay dumps. The captured-state oracles (`pes_trap_oracle_test`, the kick, danger, dribble, teampass, walk and angle oracles) replay captured memory, which assumes the dump's globals. They cannot run on a booted image without being re-recorded. So any unification keeps a dump-mode memory for the oracles, or re-records them.
7. Per-call global writes. `ext.globals` are written into the image on every trap call (`pesworldrecord.cpp:323`). In a shared image those writes persist and overwrite whatever else sits at those VAs.

## 3. Who uses what, per match and per process

| Object | Lifetime | Built by | Used by |
|---|---|---|---|
| Image mapping (fox) | per match | `PesFoxWorld` ctor | fox calls, boot, `PesAnimeIssue` |
| Image mapping (trap) | per match | `PesTrapMemory` ctor | trap calls (`ext.image`) |
| Heap (fox, 768 MiB) | per match | `PesFoxWorld::Impl` | anime engine, boot, body load |
| Heap (trap, 8 MiB) | per match, lazy | `PesWorld::heap_` | trap danger builder, BP lists |
| Trap window (about 0.5 MiB) | per match | `PesWorld` (`match.cpp:111`) | GF sim and trap calls |
| Trap loader (about 2 MiB) | per match (copied from the process source) | `PesTrapMemory` | trap calls |
| Trap source (dump + loader data) | per process | `LoadTrapSource` (`pes_library.cpp:209`) | every match |
| Anime control per man | per match | `ConstructAnime` (`matchevents.cpp:88`) | `PesAnimeIssue`, the engine, the pose |
| Name registry | boot | PES static initialisers (fox boot only) | the DemoInfoRef writer (not in the trap world yet) |
| Press planner objects | boot | PES static initialisers (fox boot only) [INFERENCE from PressCut] | the press chain (not in the trap world) |

## 4. The options, with their costs

Notation: "now" is the current state. Memory is per match and in RSS terms; the RSS numbers are estimates until cut 0 measures them.

### Option A. The trap world on the fox mapping (one image, one heap, one boot)

- What: `PesTrapMemory` disappears. The trap window and loader become segments of the fox world's memory. The trap heap merges into the fox heap. The fox boot runs first, eagerly at match start, then the trap world is bound to the same image.
- Files: `pesfoxworld.{hpp,cpp}` (expose the image, heap and services; a `NewCall` for the trap window), `pestrapdata.{hpp,cpp}` (`PesTrapMemory` becomes a view, `LoadTrapSource` unchanged), `pesworldrecord.{hpp,cpp}` (`heap_` becomes the shared heap, `Bind` and `ext.image` point at the shared memory), `pestrapworld.hpp` (`PesTrapExternals` fields), `match.cpp` (construction order at 111, 296-299, 340-342), `matchevents.cpp` (`PesAnimeWorldOf` eager in PES loop), `teamAIcontroller.cpp:3654`, the 31 test fixtures.
- Memory: now minus the trap heap (8 MiB) and minus one image's dirty pages (a few MiB, to be measured). About the same as now.
- Load time: one boot per match, as now for PES matches. Trap-only fixtures that never boot today would now boot (about 1.9 s each) unless a per-process template exists (option D).
- Determinism: (1) the trap chain now runs on booted globals, not the dump: behaviour changes, so the PES-loop hash moves; (2) the per-call global writes land in the shared image (clobber check, section 2.2); (3) trap objects move within a shared heap (layout-only; expected invisible, to be proven by the hashes). Oracles keep a dump-mode memory.
- Tests moved: all 31 direct files (fixtures). The captured oracles stay on a dump-mode memory. The live ones move: `pes_throwin_release_test`, `pes_movestep_wire_test`, `pes_events_test`, `pes_world_test`, `pes_team_tick_test`, `pes_offball_test`, `pes_sanction_test`, `pes_scorerows_test`, `pes_match_info_test`, `pes_playerdb_wiring_test`, `pes_mark_table_test`, `pes_judge_decide_test`, `pes_involvement_test`, `pes_foul_whistle_test`, `pes_tick_a1_test`, `pes_player_object_test`, `diff_cone_test`, and the anime tests `pes_anime_engine_test`, `pes_fox_world_test`. The whole ctest kick/throw/roster subset (192) and both smokes.

### Option B. The fox world absorbs the trap window and loader (ownership inverted)

- What: the same memory as A, owned by the fox world. `PesWorld` keeps the records and holds a reference to the shared memory instead of owning its window bytes.
- Files: as A, with the ownership moved: `pesfoxworld.*` (owner), `pesworldrecord.*` (window pointer from the owner), `pestrapdata.*` (the loader segment into the owner). The fox names become misleading and need renaming (`PesFoxWorld` -> a process-level PES memory).
- Memory, time, determinism, tests: as A. Slightly cleaner ownership; the window's host bytes leave the `PesWorld` object.

### Option C. Two images, two boots (the trap world boots its own memory)

- What: `PesBoot` is extracted into a function of a memory (cut 1). The trap world runs it on its own COW mapping and heap, so the seams close inside the trap world. The fox world stays as it is for the anime engine, or the anime engine moves into the trap world later.
- Files: `pesfoxworld.*` (boot extracted), `pestrapdata.*` (`PesTrapMemory` boots under a flag), `pesworldrecord.*` (heap sized as the fox heap), `match.cpp` (boot the trap memory when the flag is on), fixtures.
- Memory: about plus 776 MiB per match (a second boot's heap plus its dirty image pages): RSS per match about 3.1 GiB, from 2.3 GiB. That is a third more per match, times 6 slots.
- Load time: plus about 1.9 s per match (a second boot).
- Determinism: the trap chain runs on booted globals, so the PES-loop hash moves. No clobbering (separate images), no layout change (separate heaps).
- Tests moved: as A's live set; the oracles stay on dump mode (their own mapping, no boot).
- Use: a stepping stone that closes the seams at once and measures the behaviour change, at a memory cost that the template (option D) removes.

### Option D. Boot once per process into a template, clone per match (copy-on-write)

- What: the booted state (image dirty pages, the fox heap, the OS arena) is produced once per process and stored in a memfd. Each match maps it privately (MAP_PRIVATE of the memfd). Only touched pages cost memory. `LoadTrapSource` is already per process, so the template follows the same lifetime.
- Files: `pesfoxworld.*` (template build and clone), `pesworldrecord.*` and `pestrapdata.*` (a per-match clone of the image and heap), `match.cpp` (the template handle), `gdext/pes_library.cpp` (template lifetime, like `trapSource_`), fixtures (they reuse the template and stop paying the boot).
- Memory: the 776 MiB heap is shared across matches; per match only the touched pages plus the window (about 0.5 MiB) and the loader (about 2 MiB). Expected large drop from the current 2.3 GiB per match [INFERENCE; measure in cut 0 and cut 2]. Plus one template per process (776 MiB, shared by all matches of that process).
- Load time: boot once per process (1.9 s) and O(1) clones. Test time falls sharply.
- Determinism: each match starts from the same bytes as a fresh boot would give, so cut 2 must reproduce the cut-1 hashes exactly. That is a strong gate for the template itself. Requires the boot to be deterministic (two boots produce identical images: to be checked in cut 0).
- Platform: `memfd_create` and `MAP_PRIVATE` (Linux). `PesFoxWorld` already uses `mmap` (POSIX only), so no new restriction [INFERENCE: confirm the target platforms].
- Tests moved: fixtures get faster; no assertion changes until cut 3.

### Option E. One image, two heaps, one boot (the minimal unification)

- What: the shared image is used by both worlds, one boot per match; each world keeps its own heap, at different VAs (the trap heap moves, for example to `0x2500000000`).
- Files: `match.cpp` (order), `pesfoxworld.*` (expose the image), `pestrapdata.*` (`PesTrapMemory` takes the shared image pointer, no second mapping), `pesworldrecord.*` (heap VA as a parameter), fixtures.
- Memory: minus one image's dirty pages; heaps unchanged (768 + 8 MiB).
- Load time: one boot per match (as now for PES matches).
- Determinism: the trap chain runs on booted globals (hash moves once); trap heap addresses change (layout-only); the clobber check applies.
- Tests moved: as A, without the heap merge.

### Recommendation among the options

- A and B end at the same architecture (one memory, one heap, one boot per match). B is the cleaner ownership. Both need the clobber audit and the booted-globals decision.
- C is the fastest route to the seams but costs about 0.8 GiB and 1.9 s per match, and that cost stays until the template exists.
- D removes the cost that C and A carry (the committed 768 MiB per match, and the boot per match), and it makes the one-memory end state cheap. It is the recommended foundation.
- E is only a memory-saving intermediate and is not needed if D is done.

## 5. Recommended order of cuts (suite green between each)

Gates common to every cut: build; the 31 direct test files; the 192-test kick/throw/roster ctest subset; the switch-off smoke (`c2016bf08fd0bc98`, GF mode, which runs no PES memory, so it must not move in any cut); the pes-loop smoke (`da3b1eff7174bc8b`, the current baseline); the ankle test. A hash move is allowed only with a recorded reason and a new baseline (Main's rule).

- Cut 0. Measure (no behaviour change, no production code). (a) Boot the fox world once in a scratch harness and diff the image against the file: the set of written ranges and globals (the boot's write set). (b) Intersect it with the trap loader's globals (section 2.2) and with the trap chain's read set (from a recorded call). (c) Boot twice and compare the images (determinism of the boot). (d) Measure: boot time; heap high-water after boot (`top_`); touched image pages after boot and after a PES match; RSS of a fox world and of a trap memory (`/usr/bin/time -v`). Output: a table that picks between A/B and D. Unblocks: the decision.
- Cut 1. Refactor, behaviour identical. Extract one memory class (image mapping, heap, services, stack and TEB, OS arena) used by `PesFoxWorld` and `PesTrapMemory`. Extract the boot into a function of that memory (`PesBoot`). Nothing else changes. Gate: all hashes equal; 31 tests and the ctest subset unchanged.
- Cut 2. Template (option D), behaviour identical. Build the booted state once per process into a memfd; each fox world maps it privately. Gate: the pes-loop hash equals `da3b1eff7174bc8b` exactly (a fresh boot and a clone must give identical runs); RSS per match measured and recorded; fixture times recorded.
- Cut 3. One memory per match. The trap window and loader join the anime clone's services; the trap world views the clone; the trap chain runs on booted globals. The PES-loop hash moves: record it, with the reason (the trap chain's globals are the booted state). The oracles stay on a dump-mode mapping (no template boot). Gate: the clobber audit from cut 0 shows no trap write lands on fox state; the live tests green; the oracles unchanged.
- Cut 4. Close the seams in the one memory: (i) `ConstructAnime` for each trap slot, so `P+0xA08` is the real `BodyAnimControl` (remove the physique stand-in, `pestrapworld.hpp:141-146`); (ii) the table writer `FUN_141EB4650` runs in that memory and its output is the table test B reads (e should then be 20 or 34 as in PES, so test B closes); (iii) the DemoInfoRef writer `FUN_1406EB250` runs with the registry from the template, so `BookkeepingSkips` reads 0; (iv) the press planner objects come from the template. Gate: `pes_throwin_release_test` (test B closes, the ankle test stays green), `pes_anime_engine_test`, the DemoInfoRef replay (`scratch/dem_replay.py`), `pes_press_test` and `pes_press_approach_test` (the PressCut press chain, in this tree; the kick-off press test lives in the PressCut worktree).
- Cut 5. Cleanup. Remove `PesTrapMemory`'s own mapping and the second heap, retire the live-match dump mode (keep it for the oracles only), delete any remaining stand-ins, update the docs and the day file.

What each cut unblocks: cut 2 removes the committed 768 MiB per match and the per-match boot cost (memory and test time). Cut 3 puts both worlds in one memory. Cut 4 unblocks the BodyAnimControl and the table writer (test B), the DemoInfoRef writer (`BookkeepingSkips` to 0), and PressAI3's planner objects, and removes the bridge from the plan (it is never built).

## 6. Risks and what would stop the plan

1. Clobbering. A trap call writes its loader globals into the shared image on every call (`pesworldrecord.cpp:323`). If the boot writes any of the same VAs, the fox world sees trap values. Stop condition: a non-empty intersection of the trap globals with the boot's write set (cut 0, item b) that cannot be resolved by ordering.
2. Booted globals change the trap chain. The trap oracles are dump-based, so the live trap behaviour changes at cut 3 by design. Stop condition: a trap test that depends on a dump-only global value and cannot be re-recorded.
3. Boot determinism. If two boots differ, the template (cut 2) breaks determinism. Stop condition: a non-identical second boot (cut 0, item c).
4. Layout. Trap objects move in a shared heap (cut 3). Any PES decision that depends on address order would move the hashes. Expected to be absent; the hashes will tell.
5. Memory on this machine. Option C costs about 0.8 GiB per match; the template (option D) is the answer, not C.
6. Oracles. The captured oracles must stay on a dump-mode memory. If they are re-recorded instead, that is a large separate job.

## 7. Decisions for Main

- D1. Accept the booted global state for live matches (cut 3), with the oracles on the dump. Yes or no.
- D2. Confirm the platform: the template needs `memfd_create` and `MAP_PRIVATE` (Linux). `PesFoxWorld` already needs POSIX `mmap`.
- D3. The per-call trap global writes (`ext.globals`): keep them per call (current), or write them once into the template. The second changes the call semantics.
- D4. Order: cut 0 first (it is measurement only and decides between A/B and D). Then cut 1 and cut 2 (both behaviour-neutral, both testable by the hashes). Then cut 3 (the one behaviour change, re-baselined). Then cut 4 (the seams).

## 8. Files touched, summary

`src/sim/onthepitch/pesfoxworld.{hpp,cpp}`, `src/sim/onthepitch/pestrapdata.{hpp,cpp}`, `src/sim/onthepitch/pesworldrecord.{hpp,cpp}`, `src/sim/onthepitch/pestrapworld.hpp`, `src/sim/onthepitch/pesanimechain.{hpp,cpp}`, `src/sim/onthepitch/pestrapactor.{hpp,cpp}`, `src/sim/onthepitch/match.cpp`, `src/sim/onthepitch/matchevents.{hpp,cpp}`, `src/sim/onthepitch/teamAIcontroller.cpp`, `src/sim/onthepitch/player/humanoid/humanoidbase.cpp`, `src/gdext/pes_library.cpp`, `src/pes/translated/pes_native.{hpp,cpp}`, `src/pes/translated/pes_stubs.hpp`, and the 31 test files listed in section 4 (A) plus the fixtures under `tests/`.

## 9. Tests that build the worlds (31 files)

Anime and fox: `pes_anime_engine_test`, `pes_fox_world_test`.
Trap and window (live): `pes_joint_block_test`, `pes_ball_list_test`, `pes_events_test`, `pes_player_object_test`, `pes_movestep_wire_test`, `pes_team_tick_test`, `pes_tick_a1_test`, `pes_offball_test`, `pes_sanction_test`, `pes_scorerows_test`, `pes_match_info_test`, `pes_playerdb_wiring_test`, `pes_mark_table_test`, `pes_judge_decide_test`, `pes_involvement_test`, `pes_foul_whistle_test`, `pes_world_test` (pilot), `diff_cone_test`.
Captured oracles (dump mode): `pes_trap_oracle_test`, `pes_danger_oracle_test`, `pes_dribble_oracle_test`, `pes_teampass_oracle_test`, `pes_walk_oracle_test`, `pes_angle_oracle_test`, `pes_kick_chooser_oracle_test`, `pes_kick_header_oracle_test`, and the helpers `pes_oracle_world.hpp`, `pes_kick_oracle_common.hpp`, `pes_match_constants.hpp`.
Match-level, through `Match` (they move with the match): `pes_throwin_release_test`, `pes_press_test`, `pes_press_approach_test`, the ctest kick/throw/roster subset (192), and both smokes.
