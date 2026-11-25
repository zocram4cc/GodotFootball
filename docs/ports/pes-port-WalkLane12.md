# pes-port-WalkLane12: tackle (0xD) action tick: proven, wired, one loud gap in play (NOTHING landed; no commit)

## 0. Verdict
- Proof: the TACKLE tick 0x14078D2C0 is PROVEN on the 4 real captures of launch 9 (pid1257475: seq5199, seq5200, seq5275, seq7854), bounded closure, merged into `results_trapaction.json` (M5, record `closure_tackle.json`, 31 members, all with specs). 4 calls, 4 replayed, 0 mismatches, own-block 7/11 (recorded, not gated).
- Wiring: GF now runs the action object's tick through PES's per-man dispatch for ac8 Trap (7) and Tackle (0xD) (`RunActionTick`, `RunPesActionTicks`), and the tackle start clip is PES's issued motion (`PlayIssuedClip(.., kTackle)`). Trip and Interfere map to 0xD (943f20fd8's rows, the tackle id).
- In play the run ABORTS (loud, by design): a tackle reaches the tackle object's vtable +0x28 = 0x14078C710, which no capture entered (`PES_DISPATCH_UNTRANSLATED 0x14078c710`, quick run). Relaunch 10 is capturing it (hook `tkl28_14078C710`, `--when q(rcx)==0x1425e1a48 or q(rdx)==0x1425e1a48`, EVERY 1, cap 12, FULL). Until it is proven, the wiring cannot land: no PesStallQuick or ALiveBall number for it exists yet.
- The GF tackle stand-in is NOT yet deleted (section 5).
- Gate: not run (Main gates). Tests run: `pes_receive_test` 59/61 (the 2 reds are the ball-id test's Sliding and pass rows, unchanged from the tip: red at the tip, and out of this change), `ATripAndAnInterfereAreTheTackleAction` GREEN (red on the tip: Trip and Interfere returned Move). `pes_trap_oracle_test` rc 0 (its TrapTickOracle tests skip: no live trap-tick oracle dir; `TrapS5` oracle links are broken).

## 1. Rig and captures
- Launch 9 `live2/pid1257475`: tackle captures seq5199, seq5200, seq5275 (packed at capture), seq7854 (full). Copies were slim (`slim.py`, verified replay): 4.08 GB -> 0.03 GB each. Originals untouched.
- Per-capture replay (`frame_trace recorded`): rc 0, `err: null`, return to the recorded rip (0x1406e60ca).
- Launch 8 (pid861979): one slide-hook capture, none for slide; slide still unproven.
- Relaunch 10 (Main, running): tackle slot +0x28 (`tkl28_14078C710`) and slide kept.

## 2. Facts (VA-cited, image `pes_full.bin`)
- Tackle object: slot[0xD] = 0xE0 -> P+0xD20; vtable 0x1425E1A48: +0x20 0x14078D2C0 (tick), +0x28 0x14078C710 (untranslated, see section 4), +0x50 0x141863350, +0x68 0x1407902C0, +0x70 0x140790160, +0x90 0x140791DD0.
- Table: `P+0xC40+0x1528+8*id` holds the object VA (`ConstructActionTable`, `pestrapactor.cpp`), so rcx is read from the window (no constant). Trap object (slot 7 = 0x38, P+0xC78) has vtable 0x1425E1ED0, +0x20 0x140797CD0 (the same tick as before).
- PES's per-man caller 0x1406E5FC0 (translated) reads ac8, gets the object from `0x1406D9010`, calls `[vt+0x20](obj, P, &F)`; this is exactly what `RunActionTick` now does.
- Action names: 0xD `ANIME_TACKLE` (table 0x1434A5470).

## 3. Code (worktree `<workdrive>/gf-WalkLane`, uncommitted)
- `src/sim/onthepitch/pesworldrecord.{hpp,cpp}`: `RunTrapTick` -> `RunActionTick(slot, action, ext)`: object VA from P's table; tick from the object's vtable +0x20 via `pes_dispatch_lookup` (AttachImageTables for the vtable read).
- `src/sim/onthepitch/pestrapworld.hpp`: `kActionTickSlotOffset = 0x20`.
- `src/sim/onthepitch/teamAIcontroller.{cpp,hpp}`, `match.cpp`: `RunPesTrapTicks` -> `RunPesActionTicks`; gate `ac8 in {Trap, Tackle}`; start clip under kTrap or kTackle.
- `src/sim/onthepitch/player/humanoid/humanoidbase.{hpp,cpp}`: `OnPesTackle()`; `PlayIssuedClip` admits kTackle like kTrap (a tackle clip is the tackle action's own tick).
- `src/sim/onthepitch/pestrapactor.cpp`: Trip and Interfere -> `kAnimeActionTackle` (INF rows from 943f20fd8); Sliding and the other ball rows stay Move (sliding tick untranslated).
- `tests/sim/pes_receive_test.cpp`: `PesAnimeAction.ATripAndAnInterfereAreTheTackleAction` (red on the tip by reading the source: Trip/Interfere -> Move; green now).
- `tests/pes_trap_oracle_test.cpp`: `RunTrapTick` -> `RunActionTick(.., kAnimeActionTrap, ..)`.
- Translated layer (regen, `gen_translated.py --image`, 11,225 functions, 4,162 recorded): new `src/pes/translated/ai/pes_fn_14078D2C0.cpp`, closure members, `pes_dispatch_gen.cpp` (+68), `translated.hpp` (+34); two closure files changed where an OUT-OF-SET call now links (`closure/pes_fn_1407823D0.cpp`, `closure/pes_fn_14096F870.cpp`). Drift check: running at the time of writing (`gen_translated.py --check`, see section 7).
- Records: `tools/dev/translate/results_trapaction.json` (+37 lines, additive), `tools/dev/translate/closure_tackle.json` (new), `tools/dev/ledger/universe.json` (+17 rows), and the closure specs in `tools/dev/diffharness/specs/trapaction` (570 + 41 from the slide and exit runs; only the tackle members are needed for the record).

## 4. The gap that stops the wiring (M5: a path no proof reached aborts loudly)
- In play: `PES_DISPATCH_UNTRANSLATED 0x14078c710` (quick run, seed 7, `PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow`, `GF_STALL_DUMP=1`, sim slot). 0x14078C710 is the tackle object's vtable +0x28, a facing/angle virtual (it clamps a yaw against `[this+0x20]`, calls `0x141EB50B0`, `0x14046F230`, `0x140A92C80`). The 4 captured calls are the tick only; the slot-28 call is not in them.
- Next: prove 0x14078C710 (+ its callees, RootMotion-owned `0x141EB50B0` as needed) on relaunch 10's captures, the same bounded way (frame_trace recorded -> snapdiff --bounded -> prove_bounded --merge). Whichever caller reaches it (`this` in rdx by its prologue), the capture's return address names it.

## 5. The GF tackle stand-in (not yet deleted)
- Today a tackle is GF's Special command (`elizacontroller.cpp:1876-1890`): `ChooseTackle` picks `choice.motionNo`, the command sets `specialVar1 = kTackle` and `specialVar2 = motionNo`, and the humanoid's anime query (`humanoid.cpp`, the Special clip path) plays GF's clip for that motion. That GF clip is the stand-in: PES's tick (phase 0) then replaces it with its own issued motion. The deletion is `command.specialVar2 = choice.motionNo` plus the GF clip pick for it, once the tick's phase-0 motion equals `choice.motionNo` (a census check; if it does not, PES's pick wins and ChooseTackle's kind stays as the sub kind). Not done: it changes the command path and needs its own measurement.

## 6. Measurements
- Quick run (PesStallQuick), tackle wiring + Trip/Interfere ids, seed 7: ABORT at 0x14078C710 (no number). Bounds: PesStallQuick 49 (cap 98), ALiveBall 479 (cap 599). WalkLane11's numbers are the reference: base 49 / 479; ids-only 82 / 985.
- ABallAction census, ALiveBall, PesHang: not measured (blocked by the abort).

## 7. Checks
- `gen_translated.py --image PES --check`: background job (`bg_503`), result to follow in the final message.
- `pes_receive_test`: 59 pass, 2 fail (`ABallActionIsNeverTakenForMovementOrForTheTrapItself`, Sliding/pass rows; red at the tip, unchanged by this change).
- `pes_trap_oracle_test`: rc 0; `TrapTickOracle.*` skipped (no `GF_PES_TRAP_TICK_ORACLE`: the TrapS5 oracle links are broken).
- Tests built: pes_receive_test, pes_trap_oracle_test, pes_restart_delay_test, pes_setpiece_stand_test (all rc 0).
- Mutations: none run yet.
- Worktree: `extern/godot-cpp` restored to the symlink after a git check (a missed step caused a glob mismatch; fixed).
- Disk: floor 300 GiB; at the peak the 4 unpacked copies were gone before the merged proof; all copies deleted, slim copies kept only in scratch.

## 8. Item 5 (clip-start blend, walker): decoded to the entry only
- `FUN_141AF61C0` (clip start: reads the previous motion record, resets the blend record `FUN_141AF3FD0` at `+0xf8`, `+0x110/+0x111`, then `FUN_141B03660/FUN_141B03730` for the clip record) and `FUN_141AF52B0` (series blend). The full arithmetic is not decoded; not ported.

## 9. Next steps (numbered)
1. Relaunch 10's `tkl28_14078C710` captures: prove 0x14078C710 bounded, merge, regen the layer, `--check`.
2. Build and run pes_restart_delay (PesStallQuick, GF_STALL_DUMP census) with the tackle wiring; then the ALiveBall run (~20 min, simslot) and PesHang.
3. Decide the GF stand-in (section 5) on the census; delete it in the same change if the tick's motion matches ChooseTackle.
4. Sliding (0xB): the tick 0x140786D60 has its 2 slide captures pending (launch 8 had none; relaunch 8/9 none); same pipeline when they land.
5. Item 5 (blend).
