# CutsceneAnchor17 (CutWorld): the flag lookup is proven; the selector's world build no longer hangs and runs the cut groups and the fill; the wide test is still RED (entry 0x71's node is null: the loader is not run)

Branch `CutsceneAnchor`, pushed head `4a5c369ee650e86b3b78f563cd5bc9917f07bbd4` (`git ls-remote origin CutsceneAnchor` confirms). Commits this round: `c2ddb478` (CutsceneAnchor16 proofs, pushed earlier) and `4a5c369e` (this round). Labels: CONFIRMED = read, decompiled, run or walked this round; INF = inference; OPEN = named blocker.

## 0. Status against Main's list

1. Read PES's registry at [0x1437F0438] from seq321: DONE (CONFIRMED). See section 1.
2. Translate and prove 0x141B36030 (lookup), the found object's method, and the once-init 0x141B34E50 closure: PARTLY. 0x141B34850 (the flag lookup, which calls 0x141B36030) is proven bit-exact (2/2 synthetic states). The found object's method is NOT needed (no object is found in PES's registry for the flag key). The once-init 0x141B34E50 is NOT translated and NOT run (section 2).
3. Build the registry map the PES way at world build: DONE as an empty map (sentinel header, like InitHistory); PES's ten environment entries are not built (ponytail, section 2).
4. Re-run the wiring to the fill: DONE. The world build runs 0x141E5BAF0, 0x141E5C8F0, the nine constant groups through 0x141E5BC40, then the fill 0x141E5D560. The test TheCutLibraryHoldsOneSlotPerCutName is GREEN (it hung in CutsceneAnchor16).
5. Node-0x71 test (entry 0x71's node = -80.0 at +8, 16.0 at +0xC): NOT green. Entry 0x71's node is null in GF: the loader 0x141E5D980 (which stores the node at the element's +8) is not run. The test is kept out of the commit (red), in scratch `cutworld/node_test_pending.txt`.
6. Key feed: NOT done (the world build has no home-team input; unchanged).
7. Wide test: RED, no longer hangs. 9 EXPECT failures on T and pos, the same as before (T[0] -41.5502167 vs -41.6211472, pos[0] 0 vs -41.5502167, pos[2] 0 vs 34). bc, radius and rotY pass for DC 5, 100, 161.
8. Neighbours: pes_fixdemo_select_test 7/7 green (full suite); pes_goal_anchor_test non-wide 23/23 green. Not run: registry, camera_list, proc_camera binaries (not rebuilt this round; the change touches only the fixdemo selector and gf_match's call).
9. Checks: gen_translated --check byte-identical (5803 functions, 3772 recorded bit-exact, 2031 closure). Ledger --check current.

## 1. The registry (CONFIRMED on seq321)

- [0x1437F0438] = container 0x1437F0538; [container] = the std::map header 0x13E68B0 (heap): head 0x13E6D00, root 0x13E6990, last 0x13E6920, size word 0x101 (not a count of nodes; see the walk).
- Walk (CONFIRMED): 10 nodes, keys: application_dir, argc, argv, company_name, cpu_core, fullscreen, processor_core_count, product_name, screen_size. These are the application's environment and machine info. The key "Def_Game_ai_use_text_file" (the flag's name, string at 0x142B48368, readable in the state) is NOT among them.
- So PES's answer for the flag key is "not found" (INF on its use: the lookup returns null and the caller takes the plain path; consistent with the proof of 0x141E5BC40 in CutsceneAnchor16, which entered 0x141E5BEE0 and never 0x141E5C060).
- The earlier claim that the flag was a config value or a fatal once-guard is corrected: 0x141B34850 is the name lookup; its fatal branch only fires while the counter [0x1437F0428] is 0.

## 2. The once-init 0x141B34E50 (NOT run)

- Its post-state, from seq321: counter [0x1437F0428] = 1 (CONFIRMED). Its effect on the registry: the ten environment entries (section 1).
- Its closure reaches the .impdata thunks 0x141B39230 and 0x141B39240, which jump into 0x15B2... (the protector's region). Per Main's ruling these are environment/protection code and are never run or translated. GF writes the counter 1 and an empty registry as the post-state (ponytail).

## 3. The clock cache (found this round)

- The flag path's neighbour 0x1403E8800 reads [0x1436EDE58]: nonzero takes its fast path, zero calls an import (timeGetTime, winmm). PES's value is 0x989680 = 10,000,000 (CONFIRMED, seq321). That is the OS's QueryPerformanceFrequency (10 MHz on Windows).
- The install's PES2021.exe does NOT carry the value on disk: the .data raw size ends before 0x1436EDE58, and PeImage::ReadVa returns false there (CONFIRMED). So GF names the constant (kQpcFrequencyHz) with its provenance. ponytail.
- Without this write the wiring hits the untranslated import dispatch (PES_DISPATCH_UNTRANSLATED on a host address, from k_c14252FE88_q = the baked IAT value; the IAT slot is winmm!timeGetTime per import_slots.json, and import_stubs.json has no model for it).

## 4. The wiring hang (CutsceneAnchor16 section 3) is resolved

- Cause: the flag lookup walked an absent registry (the map header was zero in GF's arena, and unmapped reads return 0, so the walk never found a sentinel). Fix: InitRegistry builds the empty map at [0x1437F0438]; a page segment 0x1437F0000 maps the global, the container and the counter.

## 5. Proofs and records (CONFIRMED)

| Root | States | Result | Own-block | Record |
|---|---|---|---|---|
| 0x141B34850 (flag lookup) | pes_map (the capture's registry), empty_map (sentinel) | 2/2 bit-exact, 0 mismatches | 5/7 | results_trapaction.json, closure_cutflag.json (15 members; heap natives 0x1403FBC10/0x1403FBCB0 excluded) |

## 6. Why the node is null (INF, next step)

- Node 0x71 (entry at the cut slot, +8 = node, +0x10 = key 0x100000071) is written by the loader 0x141E5D980 (CONFIRMED by decompile: `*(param_1 + 8) = node`, `strncpy(param_1+0x10, name)`, `*(param_1+0x90) = 1`). The loader parses the object's bytes (0x141E6C280, proven emulated by CutsceneAnchor11 on demoarea_st001.o) into a 0x158-byte node built by the constructor 0x141E6C100.
- The fill 0x141E5D560 does not call the loader (CutsceneAnchor14 section 2: the dispatch through the loader's vtable slot +8 is OPEN).

## 7. Next steps (named blockers, in order)

1. Find the caller of the loader 0x141E5D980 (xrefs: its vtable slot or the fill's dispatch) and decide whether GF calls it per element (the path name is the element's file; the stream 0x1414A8790 reads the VFS by name, which needs a native file-read service as a boundary).
2. Translate and prove the loader's closure (0x141E6C100 constructor, 0x141E6C280 parser, the stream and file-read services) over synthetic states with the object bytes of demoarea_st001.o (the install's, unwrapped).
3. Wire it per element in the world build; then run TheStadiumCutNodeHoldsPesLiveValues (scratch `cutworld/node_test_pending.txt`) and the wide test.
4. Key feed (the home-team stadium id): the writer of T+0x1C is still OPEN (CutsceneAnchor15 section 5).

## 8. Disclosures

- The commit draft's `take` of docs/re/LEDGER.md failed (no change to take; the ledger check passes), so LEDGER.md is not in this commit.
- The node test is not in the commit (red). It is in scratch.
- The build directory is removed; the flag states (3.8 GB each) are deleted; cstates for create_st001 remain in scratch (used by future proofs).

## 9. Tests run (this round)

- pes_fixdemo_select_test full: 7 passed (rc 0).
- pes_goal_anchor_test non-wide: 23 passed.
- pes_goal_anchor_test WideChannelScorerTakesTheElseArmAndEqualsPes: runs to the end; 9 EXPECT failures (red).
- gen_translated --check: rc 0. ledger.py --check: rc 0.
- Mutations: none run (not requested for these steps; the InitRegistry, the clock write and the segment are each needed by the cut test: removing the registry write re-creates the hang).
