# pes-port-TrapTick7: SetMotion's anime slot +0x80 (0x1406E72A0) proven 12/12; the pes-loop smoke now finishes through PES's trap chain; a trap clip carries its contact frame; pes_stall_test still RED on 2 live-ball stalls (31-10-26)

Branch `StoppageFlow`. Image `pes_full.bin` (base `0x140000000`). CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Verdict

| Item | State |
|---|---|
| 1. `git merge origin/main`, push | done (`80451235`) |
| 2. `0x1406E72A0` translated, proven | **DONE**: `snapdiff.py --bounded --only 0x1406E72A0` over `live2/pid3061186/seq707..715, 726, 727, 733` (12 FULL captures of hook `anime_iface_80_6E72A0`): `calls=12 replayed=12 mismatches=0 own_block=0.9 proven=True` (CONFIRMED). Closure `closure_trapanime.json` (3 members: `0x1406AF6D0`, `0x1406B0ED0`, `0x1406E72A0`; the first two were already in the layer, so one new unit `restart/pes_fn_1406E72A0.cpp`); layer regenerated 4444 functions; `gen_translated.py --check` rc=0 (CONFIRMED). Spec `specs/trapaction/1406E72A0.json` (closure.py, pilot mode, tag `trapanime`; like the switch, only the root has a spec) |
| 3. pes-loop smoke | **FINISHES**: no abort, rc 0, `[determinism] adae5a731cee276c` twice (before the contact-frame fix), `[determinism] 1c4900af85dd6a35` twice (after it); 2-min seed 7 |
| 3. `pes_stall_test` | **NOT GREEN**: no abort any more; `TheBreakerNeverForcesABallUnderAnArmedRestart` PASSES, `ALiveBallNeverLiesStillLongEnoughForTheBreaker` FAILS (see section 2) |
| 4. tick oracle 14/14, kind-0 oracle, action-table test | `pes_trap_oracle_test`: 14 of 15 pass (all `TrapOracle` 11 incl. `TheActionTableResolvesEveryIdToThisMansOwnObject`, `TrapTickOracle` 3 incl. `EveryCapturedTickOnGfsWindowEqualsPes`); the 15th `LoaderDataTheTickReadsEqualsTheCapture` is the known stock-vs-mod `constant::Trap +0x8` (capture 0a is a mod's, GF reads stock) |
| `GATE fast` | see section 4 |

## 1. 0x1406E72A0 (CONFIRMED: decompile `FUN_1406e72a0`, replay)

`FUN_1406E72A0(iface, out, motion, mirror)`, the anime interface vtable `0x1425E3508` slot `+0x80`: for `motion < 0x1125` it reads the motion record (`FUN_1406B0ED0` -> `FUN_1406AF6D0`), scales its three floats by the image scale `DAT_14256ae18`, multiplies x by `DAT_142588218` when mirrored, then all three by `P+0x3060` clamped between `DAT_1425ddf78`/`DAT_1425ddf80`; stores the vector in `out` (`out[0..8]`, `out+8`); for `motion >= 0x1125` it stores zeros. SetMotion `0x1406F0F70` rotates it by `P+0x20` (yaw, `FUN_140a92c80`) and adds `P+8/0xC/0x10` into `P+0x34E4..0x34EC` (the clip's end position). The call is the `+0x80` branch when `FUN_1406AFBC0(motion db, motion, 5)` returns 0; the 24 chain captures all took `+0x88`.
- The 12 captures replay bit-exact: the written `out` bytes (28 to 42 per call) and the return value, from the same memory. Coverage 9 of 10 blocks (the zero branch for `motion >= 0x1125` is unreached: real motions are below it; every other path aborts loudly, as the bounded closure does).
- Commands (all under `GF_DIFF_PYTHON`, from the worktree): `closure.py --image $PES_EXE --spec-set trapaction --tag trapanime --subsystem restart --members-file root.json 0x1406E72A0` (spec only; set file `closure-trapanime.txt` removed again, as the switch has none), `frame_trace.py recorded <snap> --hooks hooks.cfg --out <trace> --spec-dir specs/trapaction` per capture, `snapdiff.py --bounded --only 0x1406E72A0 --snapshot S=T ... --results res.json` (writes `gen/<VA>/members.json`), then `closure_trapanime.json`, the `results_trapaction.json` record and `gen_translated.py`.

## 2. The next failure was not a translation abort: the issued trap clip had no contact frame (CONFIRMED by test; INF that it is the whole cause)

After the translation both runs finish. The stall test's `[stalemate]` prints (smoke log) showed the breaker forcing the nearest man at 0.1 to 1.1 m from a ball at rest 0.08 m/s, 6 episodes in 2 minutes; four of the men were in a PES trap clip (`trap_0_0_000_y0_in frame 45/121`, ...). Cause (CONFIRMED by reading the code and the red test): `HumanoidBase::PlayIssuedClip` (the Trap action tick's clip issue, `RunPesTrapTicks`) set `touchFrame = -1` for every clip. The touch block (`humanoid.cpp:641`, `touchFrame == frameNum`) therefore never ran for an issued trap clip: the man played the whole clip and never touched the ball. The command path already took `touchframe` (AddPesClips: the install's first HitData record) and the ball's prediction at that frame (`humanoid.cpp:2761`); the issued path did not.
- Fix (`humanoidbase.cpp`): for the trap family `touchFrame` = the clip's `touchframe` variable and `touchPos` = the ball prediction at that frame (guarded: frame > 0, a mental image present); every other family keeps -1.
- Red-first test `MoveStepWire.AnIssuedTrapClipTouchesAtItsContactFrame` (`pes_movestep_wire_test.cpp`): a trap clip with a hit record, played through `PlayIssuedClip(.., kTrap)`; expects `touchFrame == PesTouchAnimFrame(hit[0].frame)` (a relation from the same record, no pinned number). Red on the parent (`-1` vs `20`), green with the fix (CONFIRMED runs). Mutation: the pre-fix behaviour is the mutation of the family branch (the test failed with it, passes with it). No further mutation: no other branch is added (the guard `frame > 0 && image` only protects a clip without a hit record).
- Effect (2-min seed 7, pes-loop): `[idle] 6 episodes, stalls 6` -> `[idle] 2 episodes, stalls 2`; `[restarts] 9` -> `16 stoppages` (longest 23.7 s, bound 32.5 s both). Tripwire only, not a target.
- **Remaining two stalls** (smoke3, 5.0 s and 7.1 s): a ball at rest 0.3 to 1.5 m from the nearest man, who is in a Movement clip (`fn 1`, MOVESTEP 12<-3 run or 1<-6 idle), no trap issued. INF: PES's reaction entry 3 refuses the designated owner (`RefusedAsDesignatedOwner`, gate 4c) and GF's carrier/pickup (`AI_HasPossession` radius 1.0 m, `hasIt`) is the stand-in that has to move him: the open "Group A" of `pes-stall-IdleEpisodes-2.md` and `pes-port-StoppageFlow5.md` (the `[stalemate-who]` lines name the men). Not a translation gap; not pursued here.

## 3. Open (numbered)

1. `pes_stall_test.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: 2 live-ball stalls remain (section 2). Next step: log per tick, for the forced man, `_ReceiveCandidate` / `RefusedAsDesignatedOwner` and the carrier brain's command, to see whether entry 3 refuses him; then the PES holder/pickup writer (the `hasIt` radius stand-in).
2. `TrapTickOracle.LoaderDataTheTickReadsEqualsTheCapture` (stock vs mod): `live2/pid3061186/seq2016, 2028, 2035` are `trap_action_tick_797CD0` captures from the No-Mod launch (stock constants); rebuilding the oracle's dirs from them would turn it green.
3. The pes-loop 20-minute smoke and the switch-off smoke baseline are Main's (batch).
4. Other action objects' `+0x18/+0x28` through the table and the `0x1406E72A0` zero branch stay loud aborts until a capture runs them.

## 4. Gate and smoke

- **`GATE fast`: interim, RED so far, killed at 1146 of 2186 ctest tests** (`GATE_TIER=fast GATE_SLOTS="3 2"`, 2 h 20 min; each pes test took 700 to 800 s under load 20 to 35; killed to rebuild after merging main). Failures seen: `PesForfeitMatch.ASideAtTheMinimumKeepsPlaying` and `PesForfeitMatch.AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil`, both `Subprocess aborted` = a new untranslated call (section 5). The gate's own 2-min seed-7 smokes reached full time: switch-off `[determinism] 4a350d095284288e` (24839 ticks, 15 stoppages), pes-loop `1c4900af85dd6a35` (31041 ticks, 16 stoppages); the pes-loop one reproduced by a direct rerun and again on the merged tree.
- **Switch-off hash differs from main's baseline**: main's batch AR lines are `e663d1e917d6c4f4` (switch-off) and `980c54e27f93ced2` (pes-loop); this branch gives `4a350d095284288e` / `1c4900af85dd6a35`, also after merging main `d4fff751` (rerun on `6098d071`, identical). The pes-loop difference is expected (the trap chain runs). The switch-off difference is NOT explained: nothing I changed runs there (the trap tick and `PlayIssuedClip`'s trap family are pes-loop only, `match.cpp:900`); candidates are earlier StoppageFlow commits (TrapTick2..6). Bisect needed (item 5).
- Branch: merged `origin/main` (`d4fff751`; conflicts in `xlate.py` (memset and fdtest models both kept), `gen_translated.py` (SETS: trapaction and dribble), `snapdiff.py` (main's inline form), `pesworldrecord.cpp` (main's carrier functions and my `LendExternals` both kept)); layer regenerated 4554 functions, `gen_translated.py --check` rc 0; head `6098d071` (`git ls-remote origin StoppageFlow` confirms).

## 5. The next untranslated call (CONFIRMED by gdb) and what Main did

`PesForfeitMatch.ASideAtTheMinimumKeepsPlaying` (pes_loop match, `[stalemate]` path): `pes_dispatch_missing(0x1406e85b0) <- pes_fn_14079C2D0 (call site 0x14079C5EC, ret 0x14079C5F1) <- pes_fn_140795AB0 <- pes_fn_140795A40 <- Match::Process`. `FUN_1406E85B0(iface, out, motion, pos, yaw, mirror, 0, 0)` calls `FUN_1406E8B70` and rotates its result by yaw like SetMotion's tail; the tick calls it on the `*(char*)(action+0x26) == 0` branch. No capture covers it: the 3 No-Mod `trap_action_tick_797CD0` captures of `pid3061186`, the 12 of `pid3439759` and the 14 oracle ticks never enter it (`frame_trace.py recorded` call sets checked). `hook_plan.py` (scratch copy, target added): PASS, window 15 bytes (`mov,mov,push,sub`), no real branch into the window. Main armed `anime_displace_6E85B0` (predicate `ret==0x14079C5F1`, cap 12, full) at the 60-hook restart; **no capture had landed 25 minutes after the restart** (the branch is rare in live play). When one lands: `frame_trace recorded` + `snapdiff --bounded` of `0x1406E85B0` as of `0x1406E72A0` (section 1), members into `closure_trapanime.json`, regenerate, `--check`, then the two forfeit tests and the rest of the gate.

## 6. Unfinished (numbered; supersedes section 3)

1. `0x1406E85B0` captures and proof; the two `PesForfeitMatch` aborts; then the full fast gate.
2. `pes_stall_test.ALiveBallNeverLiesStillLongEnoughForTheBreaker`: 2 live-ball stalls (section 2).
3. `LoaderDataTheTickReadsEqualsTheCapture` (stock vs mod; No-Mod `trap_action_tick` captures exist in `pid3061186`, `pid3439759`).
4. 20-minute smokes are Main's.
5. Bisect the switch-off hash difference.
6. Other action objects' `+0x18/+0x28` and the `0x1406E72A0` zero branch stay loud aborts.

## 7. Tests and mutations

Red-first: `MoveStepWire.AnIssuedTrapClipTouchesAtItsContactFrame` red on `f084d859` (`-1` vs `20`), green on `d45c588e` (the pre-fix state is the mutation of the family branch). `0x1406E72A0`: 12/12 bit-exact by `snapdiff`, generated unit, no code mutation. `pes_trap_oracle_test` 14/15 (known LoaderData) before the merge. After the merge only the two smokes ran.
