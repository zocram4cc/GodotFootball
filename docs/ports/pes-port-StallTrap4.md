# pes-port-StallTrap4: counter-B writer FUN_14092ED30 proven bit-exact on 12 real captures; the layer regeneration is blocked by four gate items; feed and race test not started; nothing landed, nothing pushed (StallTrap, 22-11-26)

Status: PARTIAL. Furthest proven step: the writer's proof record (12/12 real calls bit-exact). Branch `StallTrap` stays at `646cc259`, worktree clean, no push (`git ls-remote origin StallTrap` shows no branch). Reason for stopping: `gen_translated.py` refuses to write the layer (lint, held-outs), and a tip that fails its own `--check` cannot be pushed. The proof artifacts are saved in scratch (section 6) and reproduce the record.

## 1. What was decoded (CONFIRMED, image bytes)

- Writer entry FUN_14092ED30 (flat image, `pes_full.bin`, file offset = VA - 0x140000000). Prologue at 0x14092ED30: `mov %rcx,%rdi` (rcx = the man record, `mov 0x18(%rdi)` reads the player id), `mov %rdx,%r15` (rdx = the slot record, the counter-B store is `mov [r15+0x1438],eax` at 0x14092F204). Reads the match globals through `G = *(0x1436F3DC8)`: `G+0x270` (MI), `G+0x1A8`.
- Calls in the closure: 0x140A48A30, 0x140A51320, 0x140A606C0, 0x140A3E190, 0x140A60300, 0x140928120 (the value; see StallTrap3 section 1), and 0x140A4D350 among others.
- Prior decode (StallTrap3, CONFIRMED): the value's eax comes from 0x678950 -> 0x6788B0 -> 0x678990. Not yet decoded further in this round.

## 2. Closure (CONFIRMED, tool output)

- `closure.py --spec-set trapaction --tag counterb --subsystem AI 0x14092ED30` (dry run first: 154 members from 1 root, 0 universe rows added). 41 of the 154 had no layer file at the start.
- Specs: 154 written to `tools/dev/diffharness/specs/trapaction/` (generator output); 49 already committed and kept, 6 of them differ from the generator's output (kept; `--regenerate` not used). Set file `specgen/sets/closure-counterb.txt`.
- Protector check: the closure's static walk reaches the Steam-dispatched thunk 0x1415A1720 (import slot 0x14252FD98, stubs.json: owner ruling 17-11-26), 0x1415A174A (a thunk at the same region), and 0x141794410 (`ret 0`). None of the three is in the proven set (checked in `closure_counterb.json`). Not run, not modelled.

## 3. Proof (CONFIRMED, snapdiff --bounded, real captures)

- Captures: `live2/pid678718/seq392, 394, 395, 396, 397, 399, 401, 402, 404, 407, 408, 409` (hook `counterb_92ED30`, in play, FULL). Copied out of live2, unpacked in scratch, deleted after the proof.
- `prove_bounded.py 0x14092ED30 ... --tag counterb --real x12 --merge`: calls 12, replayed 12, excluded 0, mismatches 0, own-block 61 of 106 (0.5755). Rax, xmm0 and every written byte compared. Coverage recorded, not gated (owner ruling 26-10-26).
- The first 4 captures alone: 4/4 bit-exact, own-block 0.5189 (prove1 log).
- Record written to `results_trapaction.json` (root 0x14092ED30, closure_file `closure_counterb.json`, 136 entered members, excl. the root). Reverted from the tree (section 6).

## 4. Why the layer did not regenerate (CONFIRMED, gen_translated output)

`gen_translated.py --image` ran through `build` and aborted in `lint` / `silent_unhandled_sites`, so nothing was written. Four items:

1. **Unhandled instructions in 0x140706350 (entered by the replays).** Sites at generated lines 2441-2521 (`pes_fn_140706350.cpp`): OUTSB, LOOP, FIMUL, FILD, OUT. INF -> CONFIRMED by control flow: the block follows the unconditional `goto L_140706DEA` (line 2422) with no label in between; every goto into its labels (L_140707EA8..L_140707F4F, lines 2442-2483) sits inside the same block, so no reachable path enters it. Added to `UNHANDLED_DEAD_NOISE` with that evidence (reverted with the rest; the entry is in the saved patch).
2. **Held out (not in layer; closure reaches a mistranslated callee):** 0x1403BB670 (via 0x14037CB50), 0x1405549C0 (via 0x140555110), 0x14202BA30 (via 0x14202C250). Only blocks entry if a replay enters them; none is needed by the 12 proven calls (the record is bit-exact).
3. **Live reads of read-only image tables (lint, 10 sites in 6 members):**
   - 0x1406B8DE0: site 0x1406B8DF2 reads 0x1425DDCD8
   - 0x1406B9AA0: sites 0x1406B9AC3 (0x1425DDE38), 0x1406B9AEC (0x1425DDE50), 0x1406B9AF5 (0x1425DDE40), 0x1406B9AFE (0x1425DDE60)
   - 0x1406B9B20: site 0x1406B9B70 reads 0x1425DDF10
   - 0x1406B9BD0: site 0x1406B9C02 reads 0x1425DDA50; site 0x1406B9C0D reads 0x1425DD988
   - 0x1406B9E50: site 0x1406B9E6F reads 0x1425DDE10
   - 0x14078E470: site 0x14078E50D reads 0x1425E9F10
   Each needs a declared table in `ro_tables.json` (extent from the index bound at the site, with CONFIRMED evidence). Not done this round (INF on extents; needs the guard of each index).
4. Layer write therefore did not run; `--check` on the tip is not run (the tip would fail it).

## 5. Not done (remaining plan, exact order)

1. Declare the ten live-read tables (section 4.3) in `ro_tables.json`, with a line per site (instruction VA, index bound, extent).
2. Regenerate (`gen_translated.py --image $PES_EXE`), then `--check`. Expect further lint/held-out items; each cycle is ~10 min.
3. Feed (step 3 of the brief): `Team::RunPesPassTarget` (`src/sim/onthepitch/team.cpp`, the store at lines 428-430, `kSlotCounterBOffset` 0x1438) runs the translated writer per on-pitch slot instead of `tt_ms * kPesTickHz / 1000`. Call shape: `PesWorld::RunCounterB(slot)` in the `RunArrivalList` pattern (`NewCall`, `AttachImageTables`, gpr[1] = man record, gpr[2] = slot record, `::pes_fn_14092ED30`). Must run after `SetBallSample`/`SetMaBall` and before `RunArrivalList` (0x14045E0D0 reads SR+0x1438). The `pes_events_test.cpp:1215` counter-B store is a test fixture, not the feed; it stays. The GF `AI_GetTimeNeededForDistance_ms` path is unchanged.
4. Red test first: resting ball <= 1.5 m from the nearest man; he wins the race (0x14079FBC0 / 0x140799440 return 0 for him). Consumer route: `PesWorld::RunTrapKind0` (`pes_trap_oracle_test.cpp` shows the setup) through 0x14079A470 -> 0x140799440. Not written.
5. PesStallQuick, full ALiveBall, PesHang, PesPassTarget, `gen --check`, push. Not run.

## 6. Artifacts (scratch, `<workdrive>/gf-StallTrap-scratch/`)

- `counterb-tracked.patch` (77 lines): the reverted tracked changes (universe rows, `results_trapaction.json` record, the `UNHANDLED_DEAD_NOISE` entry).
- `counterb-untracked.tar`: 105 files (154-member specs, `closure_counterb.json`, `closure-counterb.txt`) minus the already-committed ones.
- `closure_dry.txt`, `closure_counterb.log`, `prove1.log`, `gen_regen.log` (+ `.check`), `callers_scan.py`, `counterb_list.txt`.
- Captures copied from live2 were unpacked (42 GB) and deleted. The originals in `live2/pid678718` are untouched.

## 7. Process notes

- The worktree's full `git status` fails with "expected submodule path 'extern/godot-cpp' not to be a symbolic link". Path-limited status works. Commits need the symlink swap.
- The Ghidra bridge (8080) did not answer this session; the decodes used `objdump` on the flat image.
- Background jobs: `sleep`-based polling turned into a background job; `_wait` used instead.

## 8. Confidence

- Writer's bytes and prologue: CONFIRMED (image bytes, section 1).
- Writer bit-exact on 12 real calls: CONFIRMED (snapdiff, section 3). Coverage 0.5755: INF for the rest of the writer's paths.
- 0x140706350 dead block: CONFIRMED by control flow; INF that no path reaches it in PES (the static proof, not a run).
- Feed effect on the race: unknown (not built, not run).
