# pes-port-WalkLane12: sliding (0xB) and tackle (0xD) action ticks (status: BLOCKED on rig captures; static work done; NOTHING landed)

## 0. Rig request (Main owns; relaunch 8 is live; capture blocked by the disk floor)
- Relaunch 8, PES pid 861979, folder `live2/pid861979/`. Hook lines in `<workdrive>/PES21/rendezvous/hooks.cfg` lines 1-2:
  - `slide_140786D60` (VA 0x140786D60, SLIDING tick). Predicate: `b(rdx+0xac8)==0xb and b(q(q(0x1436F3DC8)+0x270)+0x116C)==1`.
  - `tackle_14078D2C0` (VA 0x14078D2C0, TACKLE tick). Predicate: `b(rdx+0xac8)==0xd and` the same in-play term.
  - Entry registers: rcx = the action object (P+0xD00 slide, P+0xD20 tackle), rdx = P, r8 = frame slot holding F. Capture must show the full entry memory and the return. Cap 12 each, FULL, EVERY 20.
- Blocker: `live2/watch.log` released every call with "disk under 300 GiB" from 06:35 to at least 07:29 (298 GiB free at 07:29). Launch 8 has ONE capture (seq554, `ringw2_140651430`, not ours). Main is moving ~250 GB of archives to Dati III; Main said the watcher resumes by itself above 300 GiB.
- The filtered watcher I ran (notify on `index.json` hook `slide_`/`tackle_`) was stopped at the end of the run. Main should re-arm if needed.
- Messages sent: hook request and the disk blocker (to Main). Main acked both.

## 1. Facts (CONFIRMED by image read, `pes_full.bin`, VA = offset + 0x140000000)
- Names: table 0x1434A5470 (index = id): 0x7 `ANIME_TRAP`, 0xB `ANIME_SLIDING`, 0xC `ANIME_SLIDING_KEEP`, 0xD `ANIME_TACKLE`.
- Action objects are offsets from P+0xC40. The id -> object table is at `P+0xC40+0x1528+8*id` (FUN_1406D9010; `src/sim/onthepitch/pesactiontable_stores.inc`):
  - slot[0x7] = 0x38 -> P+0xC78 (the Trap object; `kPActionOffset` in pestrapworld.hpp, the rcx of 0x140797CD0).
  - slot[0xB] = 0xC0 -> P+0xD00, vtable 0x1425E1878 (slot[0xC] is the same object).
  - slot[0xD] = 0xE0 -> P+0xD20, vtable 0x1425E1A48.
  - slot[0x4] = 0x600 (Dribble, vtable 0x1425E2440).
- SLIDING vtable: +0x20 0x140786D60 (tick), +0x50 0x140788C00 (end), +0x70 0x140787EC0 (leave test, TRANSLATED), +0x90 0x140789D00 (translated closure member).
- TACKLE vtable: +0x20 0x14078D2C0 (tick), +0x50 0x141863350 (stub shared by the three vtables), +0x68 0x1407902C0 (standing tackle, slot 13, per docs/re/pes-re-duels-fouls.md), +0x70 0x140790160 (leave test), +0x90 0x140791DD0.
- PES's per-man generic tick is `0x1406E5FC0` (translated closure). It reads `ac8` (`[P+0xAC8]`), looks up the object with `0x1406D9010(P+0xC40, ac8)`, and calls `[vtable+0x20](obj, P, &F)` through `pes_dispatch_lookup`: rcx = object, rdx = P, r8 = F slot. This matches `RunTrapTick`'s convention (rcx = `PVa(slot)+kPActionOffset` = P+0xC78), so the hook predicates on rdx are right.

## 2. Why the sliding and tackle ticks never run in GF (CONFIRMED by reading)
- `teamAIcontroller.cpp:3571` (`RunPesTrapTicks`) calls `world.RunTrapTick` only when `pb[kPByteAC8Offset] == kAnimeActionTrap` (7); other ids `continue`.
- `RunPesDribbleTicks` covers ac8 4 only. No loop covers ac8 0xB or 0xD.
- So with the ids (943f20fd8), a body under 0xB or 0xD keeps the GF clip (`sliding/idle/000.anim`, census ac8 11) and no PES tick runs. That is the stall.
- `RunTrapTick` calls `pes_fn_140797CD0`, the Trap object's own phase machine (it tail-calls 0x140796CE0). Sliding and tackle need their own object's tick.

## 3. Port design (not written; waits for the proof)
- Replace the trap-only loop with a table-driven call: `RunActionTick(slot, id)` reads the object offset from the P window (`P+0xC40+slot[id]`), reads its vtable, and dispatches `+0x20` through the translated dispatch (the same call as 0x1406E5FC0's). Ids 0x7, 0xB and 0xD (and any id whose tick is proven). Trap men keep the same object and call.
- GF stand-ins to delete in the same change (the cheat and touch stand-ins for the slide, not PES's contact):
  - `humanoid.cpp` ~2962: the `desiredFunctionType == Sliding` clip choice (`GetBestCheatableAnimID` with the slide's `radiusFactor 0.2`, `radiusCheatOffset 0`, `humanoid.cpp` ~3585).
  - `humanoid.cpp` ~1485: the `currentAnim->functionType == Sliding` ball touch (`touchVec = ... * 6.0f ...`, `Vector3(0,0,6)`).
  - `AnimeActionOfClip` 0xB/0xD arms (`pestrapactor.cpp`), once the tick runs them.

## 4. Translation status (checked against `src/pes/translated`)
- Untranslated direct callees. Sliding: tick 0x140786D60; end 0x140788C00 (vtable +0x50, indirect); leave helper 0x1406E6620 (called by 0x140788C00 and 0x1407C1AD0); phase 0 0x140789A70 (calls 0x140788760 and 0x140788DA0); 0x1407C1AD0 (dribble end, also called by the sliding tick). Tackle: tick 0x14078D2C0; phase 0 0x140791A10; adjuster arm 0x141EB34B0 (P+0x4d0; RootMotion owns the adjuster); leave test 0x140790160 (indirect); 0x140791DD0 (indirect).
- Translated and reused: 0x1406EBD00, 0x1406EEBB0, 0x1406F02F0, 0x1406B0ED0, 0x1406B03D0, 0x1406B0250, 0x1406EC260, 0x1406AF8E0, 0x140A3E190, 0x140A62EA0, 0x140A48A10, 0x1406E6660 (translated; 0x1406E6620 is not).
- Dry translation (`xlate.translate_one`, no callee linked, scratch only): sliding tick 163 lines, tackle tick 135 lines. The only markers are OUT-OF-SET calls to the untranslated callees above. No unsupported instruction.
- Specs, all in my worktree (not committed):
  - `closure.py --spec-set trapaction --tag slide` for both roots: 570 members, 516 committed kept, 44 differ from the generator (`--regenerate` would rewrite them). Set file `specgen/sets/closure-slide.txt`.
  - `closure.py --tag slideexit` for the five indirect exits (0x140788C00, 0x1406E6620, 0x1407C1AD0, 0x140790160, 0x140791DD0): 41 members, 36 committed kept, 5 differ. Set file `specgen/sets/closure-slideexit.txt`.
  - The first closure run did NOT include the indirect exits, because the direct-call closure never reaches vtable targets. Now covered.
- Full closure of 0x140786D60 alone (`--dry-run`): 515 members, all already in the universe, mostly reached through indirect tails. The bounded snapdiff (`--bounded`, as TrapTick12 did for 797CD0) needs only what the replays enter.

## 5. Plan once captures land
1. Unpack the captures (`capz.py unpack`) and keep them FULL until the proof is done.
2. `frame_trace.py recorded SNAP --hooks hooks.cfg --out TRACE --spec-dir specs/trapaction` per capture (the venv Python: `GF_DIFF_PYTHON`).
3. `snapdiff.py --bounded --only 0x140786D60 --hooks hooks.cfg --spec-dir specs/trapaction --snapshot S=T ...` (and 0x14078D2C0), then `trap_oracle.py` per capture. Abort loudly on any untranslated callee the replays enter.
4. `gen_translated.py --image` and `--check`; build only the touched test binaries.
5. GF side: table-driven `RunActionTick`; delete the sliding and tackle stand-ins (section 3); re-apply ids 0xB and 0xD.
6. Measure: `PesAnimeAction`, `SetKickStand.AFarSetPieceWalker*`, `PesStallQuick`, `PesStall.ALiveBall*` (~20 min, simslot), and the census (`GF_STALL_DUMP`), against the bounds (49 quick, 479 ALiveBall) and the M2 caps (98 / 599).

## 6. Status and numbers
- Nothing landed. No commit on WalkLane. `943f20fd8` (ids) stays the local probe `WalkLane11-ids-probe`.
- Tests run this round: none. Gate: not run (Main gates). `gen_translated.py --check`: not run (no translated change).
- Measurements: none new. WalkLane11's numbers stand: base PesStallQuick 49, ALiveBall 479; ids (A) PesStallQuick 82, ALiveBall 985 (over the M2 caps 98 / 599).
- Worktree: `extern/godot-cpp` is a symlink, so `git status` needs the swap (empty dir for git, symlink back for builds). Scratch: `<workdrive>/gf-WalkLane-scratch/slide/` (lea_scan.py, ctor_dump.py, dec_tackle_set.txt, dec_blend.txt, xlate dry runs).

## 7. Item 5: the clip-start blend (walker; WalkLane11 section 2), started, not ported
- The note in `docs/re/pes-re-anim-blend.md` 7.1 is the root-motion apply (`AnimMovePlugin::apply`), not the blend.
- Decompiled (headless, scratch `dec_blend.txt`): `FUN_141AF61C0` (AnimePlayer clip start: reads the previous motion record through `*(param_1+0x60)+0x60`, resets the blend record `FUN_141AF3FD0` at `+0xf8` on the player, sets `+0x110` and `+0x111`, then `FUN_141B03660`/`FUN_141B03730` to find the clip record) and `FUN_141AF52B0` (the series blend, WalkLane11 section 3). The full blend arithmetic is not decoded yet. Needs its own round, plus a clip-start capture of the walker.

## 8. Open items (numbered)
1. Captures of `slide_140786D60` and `tackle_14078D2C0` (launch 8, disk floor). Nothing proven yet. Main owns re-arming.
2. Translate and prove 0x140786D60 and its bounded closure, then 0x14078D2C0 (with 0x141EB34B0 owned by RootMotion).
3. The GF table-driven `RunActionTick`, deletion of the sliding and tackle stand-ins, re-apply the ids, measure (section 5).
4. Item 5 (section 7): decode the clip-start blend of `FUN_141AF61C0` and port it, then re-measure the walker.
