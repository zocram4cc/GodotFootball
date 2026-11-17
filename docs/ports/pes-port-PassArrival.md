# PassArrival: the pass-receiver red on the merged tree (MergeBKBJ abe9f81d)

Status: branch `PassArrival` pushed at `87acdf6d` (ls-remote confirmed), on origin/MergeBKBJ `abe9f81d`. It lands one faithful input fix (Acceleration byte 0x2B, section 3). Both pass tests are STILL RED, and the fix did not reduce the red (section 5). Root cause A (controller branch) is still unpaired, and section 6 says exactly what pairing needs.

## 1. Measurements (MEASURED)
`pes_pass_target_test`, seed 7, 2 min, pes_loop, PES21_ROOT + PES_EXE, sim slot:

| state | stale (bound 2.94 s) | episodes over 2.94 s | longest | median |
|---|---|---|---|---|
| abe9f81d (base) | 10.11 s | 13 of 50 | 9.28 s | 1.57 s |
| 87acdf6d (Acceleration fix) | 15.89 s | 15 of 56 | 20.70 s | 1.59 s |

`EveryPassInFlightHasItsOwnSidesReceiver` stays green in both.

## 2. Static: what 0x14124DAD0 reads, and the field (CONFIRMED, Ghidra decompile + GF's own layout)
- `0x14124DAD0(p)` = `p + 0x70` (the ability block base).
- `0x140A489F0(G, k)` = `*(G + (k + 0xF3) * 0x28)` for k < 0x16, else `*(G + 0x25F8)`. `0xF3 * 0x28 = 0x25F8`, so this is the world's **slot table** (G+0x25F8, stride 0x28, entry = the record VA; `pesworldrecord.hpp` kSlotTableOffset), and the fallback is entry 0. The entry's record is `SlotVa(k) = Block(k) + 0x10F0` (kSlotRecordOffset).
- `0x1406E6660(P, id)` = `*(byte*)(id + row + 0x70)` where row = the slot record. So **the byte is slot record +0x70 + id**. Speed 0x27 is +0x97 (the value GF already documents as "slot record +0x97"), consistent.
- Field 0x2B = **Acceleration** (anim-select doc §1.3, OBS; `movestep.hpp kAbilityAcceleration = 0x2b`; GF's own Team Ability reads `physical_acceleration` for the same id, pesmovestep.cpp:93-96).
- Who fills it in PES: the roster row (0x50 bytes) is copied by `0x140A58F00` (called by `0x140922BC0`, `PesWorld::LoadRosterRow`), and `0x14092AF10` rewrites the ids whose cells changed (`PesWorld::StoreLiveAbility`).

## 3. What GF was missing (CONFIRMED by a red test)
GF's production writer `WriteSlotAbilities` (elizacontroller.cpp) wrote Speed 0x27, Jump 0x2C, Height 0x31 and the stronger foot 0x34, and copied the roster row when the player had one. **Nothing wrote 0x2B** for a player without a roster row (the synthetic squads and the fixture teams). Their byte stayed 0 in the window, so `AbilityWeight` returned 0 and `TurnPlayback(g, 0)` gave the base playback 1.0 for every single clip (the probe: 90 of 95 receiver issues at playback 1.0000, ability 0 in every row).
- Red test (landed in `pes_playerdb_wiring_test.cpp`): `PlayerDatabaseWiring.TheAccelerationByteIsTheRosterRowsOrTheStatsForEveryPlayer`. Reads the window's accessor `RunAbilityByte(slot, 0x2B)` for a synthetic man: 0 against GF's stat 99.93 on base.
- Fix (`elizacontroller.cpp` `WriteSlotAbilities`): the byte is the roster row's 0x2B when the player has a row, else `physical_acceleration * 100` (the same stat/100 convention as the other bytes), stored with the cells the Jump byte takes (`cellsOf(..., fatigued = true)`; pes-re-stamina-drain.md 4 says the fatigue factor covers 0x28..0x2C).
- Mutation: the base state is the mutation (the write removed): the test is red there (0 vs 99.93).
- Green with the write: `pes_playerdb_wiring_test` 13/13; `pes_movestep_wire_test` 44/44.
- NOT TESTED: the single-clip playback following the byte as a separate assertion. It follows by construction (EntryPlayback reads this byte through the same accessor), but I did not write a test that pins the playback value.
- Cell coverage for 0x2B is INF (the doc's 0x28..0x2C range); the fatigue factor is 1.0 for a fresh man so the value is the stat.

## 4. How much of the red Lead B explains
Measured: **none.** After the fix the single-clip playback is no longer 1.0 for ability-0 men, but the two pass tests are no better (stale 10.11 s → 15.89 s; 13/50 → 15/56 over). So the 1.0 playback was not what made the receiver's approach slow. The body speed under a class-1 command depends on other inputs too. I did not separate the playback from the rest (no run with only the playback changed), so the attribution of the worsening is unknown; it may be the new playback changing the trajectory, which the 2-minute match then amplifies.

## 5. Where Lead B stands
- The 1.0 playback was a genuine input gap (acceleration missing), so the fix is faithful to PES's inputs and is committed.
- It does not reduce the pass-test red, and the stale number got worse. Main should decide whether `87acdf6d` lands with BK (it is faithful and wire-green) or waits for root cause A. I recommend it lands with A, since A changes the same receiver's trajectory.

## 6. Root cause A: what the pairing needs (NEXT ITEM, not landed)
- Cause: on the receiver's ticks, the designated-man branch in `ElizaController::RequestCommand` (elizacontroller.cpp, the "dribble, pass, etcetera" arm, ~line 622-640) queues a BallControl (28.8 km/h, pw=0) before the PASS_GET command. `MovementDecision` takes a BallControl whenever `HasPossession()` (GF's geometric `AI_HasPossession`), so the receiver dribbles a ball he has not been given.
- Routing alone (send the receiver to `_PassGetApproach`) made both tests worse (15.15 s stale, 12/32 over, longest 20.06 s): nothing takes the ball, so T+0x78 never clears.
- Pairing, as one change with its own red test: (a) route the PASS_GET receiver (`team->PassGetReceiver() == player`, PES loop, in play, not GK, not the retainer) to `_PassGetApproach` ahead of the designated branch (elizacontroller.cpp ~622); and (b) make his touch happen: PES's trap action for the PASS_GET man. The PES path: ActionPassGet (`0x14094E0E0`, action 5) hands over to the Trap action when the ball is within reach (the Trap action's reaction gate 4a, `PlayerController::_ReceiveCandidate`, slot +0x10), and the contact frame is the trap clip's `touchframe` issued through `PlayIssuedClip(kTrap)`. GF hook points: the trap issue for this man in `teamAIcontroller.cpp` ~3603-3638 (where `NotePesDribbleIssue`/`PlayIssuedClip(... kTrap)` already run for other PES actions), gated on the PASS_GET receiver.
- Test pair: the receiver takes the ball within the live bound (`IsSetInFlightAndNeverStaleWhileTheBallRests` green) and `EveryPassGetEpisodeIsWithinPesLiveRange` green; mutation: drop (a) or (b) and the stale test goes red.
- TrapContact owns the touch contact path. Do not land (a) without (b).

## 7. Hooks
None needed. The field's meaning is settled by the decompile, the anim-select doc and GF's own stat use. The optional hook for a live check of the byte (PES's real value per slot) is `hook ab__1406E6660 0x1406e6660 17 1 1 48895c2408574883ec200fbe9958240000` (every call, cap 12, FULL), if Main wants a live cross-check.

## 8. Hygiene and state
- Branch `PassArrival` at `87acdf6d`, pushed. Commit contains: `pesworldrecord.hpp` (the `kAbilityAccelerationId = 0x2B` constant with its meaning), `elizacontroller.cpp` (the write), `pes_playerdb_wiring_test.cpp` (the red test). No probes remain (earlier probes reverted).
- Worktree `<workdrive>/gf-PassArrival`. Scratch: `<workdrive>/gf-PassArrival-scratch/` (logs: `red-accel.log`, `green-wire.log`, `wire-after.log`, `pass-after.log`, `iss.log`; decompiles in `ab/`).

## Final handover
Landed: Acceleration byte written for every slot (faithful input, red/mutation/green tests). Not landed: root cause A, which must pair the controller routing with the trap touch (section 6). Both pass tests remain red on BK; the Acceleration fix alone does not move them (worse in stale time). Owners: the controller and TrapContact for A.

## 9. Root cause A landed, 17-11-26 (branch `PassArrival` `5958a6f4`, pushed; ls-remote confirmed)
Change: `elizacontroller.cpp` `RequestCommand` adds `passGetMan = PesLoop && PassGetReceiver() == player`. For him the on-ball branches stand aside (the designated `GetOnTheBallCommands` arm, and the `_BallControlCommand` gated on geometric `HasPossession`), and the movement route to `_PassGetApproach` no longer waits on `!hasPossession` (GF mode keeps `PassGetReceiver() == player && !hasPossession`). Removed: the `!hasPossession` route gate for the PES man, and the BallControl he used to queue while walking.

1. Step 1, confirmed (MEASURED on the branch, `pesmovestep` MovementDecision + a queue probe, temporary, reverted): on ticks with the ball at rest (< 0.5 m/s) the receiver's queue held the PASS_GET command (`Movement` with pesWords) and, whenever `HasPossession()` (AI_HasPossession: ball within 1 m, height < 0.5 m, relative speed < 6 m/s) was true, a `BallControl` ahead of it. MovementDecision takes a BallControl when holds. Pre-fix: 628 of 630 holding ticks decided BallControl (on-ball override). PES side, static (Ghidra decompile of FUN_1409F8220 this session, project PES21; `docs/re/pes-re-receiving.md` 1.2/3): the receive contains no AI_HasPossession-style geometric test. Its gates are the man's own PASS_GET state (it refuses when `+0x5d0 == 5` and the anime sub-state bytes at +0x1457/+0x1458 are 3), the ball-distance table FUN_1409FA400 (carrier clause), the designated-owner refusal and the trap admission, then FUN_1409F8640 sets cmd+0x874=4 TRAP and cmd+0x878=7.
2. The two PesPassTarget tests already isolate it (red before, same numbers reproduced): no new red test was needed.
3. Measurement (seed 7, 2 min, pes_loop, PES21_ROOT=<workdrive>/PES21):

| state | stale (bound 2.94 s) | episodes over 2.94 s | longest | median |
|---|---|---|---|---|
| 87acdf6d (before) | 15.89 s | 15 of 56 | 20.70 s | 1.59 s |
| 5958a6f4 (after) | 11.33 s | 9 of 49 | 7.69 s | 1.20 s |

Both PesPassTarget tests stay red (stale 11.33 > 2.94; 9 episodes over). `EveryPassInFlightHasItsOwnSidesReceiver` green.

4. Remaining cause, named and evidenced (post-fix queue + state probe, `fix-A-diag.log`, seed 7 stale window t 8.8 to 15.2 s, slot 7): the receiver now walks with the PASS_GET command on every tick (queue `[Movement w]` only, no BallControl), reaches the resting ball (dist 0.3 to 1.4 m), and is admitted to TRAP (ac8 = 7) for 1 to 3 frames at 1.2 to 2.6 m. The ball speed stays 0.08 m/s through every trap, so the trap's touch never lands. The trap ends (ac8 back to 2 or 11) and the approach command walks him back out to 3.7 to 4.5 m. Owner: the trap's contact path (TrapContact / the body scan, `pes-re-trap-contact.md`), and the trap admission distance, not the controller routing. Not decoded here: why the contact misses at 1 to 2.6 m.
5. Mutation: the change reverted is the 87acdf6d row above (red, 15.89 s); the change is the 11.33 s row. The test bites in the direction of the change.
6. Tests (on `5958a6f4` build; the final rebuild after reverting the diagnostics compiled the same sources):
   - `pes_playerdb_wiring_test`: 13/13 green.
   - `pes_movestep_wire_test`: 44/44 green, including `MoveStepWire.TheKickoffTakerClosesToTheBall`.
   - `pes_throwin_release_test`: 6/6 green (the ceremony case alone 3347 s wall on this loaded machine).
   - `PesPassTarget`: 1 green, 2 red as above.
7. Smoke: `gfsim_run --seed 7 --minutes 2 --pes-loop` on `5958a6f4`: `[determinism] d50b22118877a9f7`, 35962 sim ticks, longest stoppage 23.6 s, idle 8 episodes, stalls 8. Not compared with the BK-a baseline `3456abc4998e107e` in this run: the branch base is abe9f81d, so the hash moves with any PES-loop change; I did not run the 87acdf6d smoke. [INFERENCE] the hash difference is this change, not the base, but that is not measured.
8. Hygiene: `git status` in this worktree fails on the `extern/godot-cpp` symlink, so the `commit` tool could not draft here; the commit was made with `git add <one file>` and `git commit` in this dedicated worktree (`--ignore-submodules=all` for status). `tools/check_pii.sh` exit 0. Scratch: `<workdrive>/gf-PassArrival-scratch/` (`diag-base.log`, `diag-queue.log`, `fix-A-pass.log`, `fix-A-smoke.log`, `fix-A-movestep.log`, `fix-A-throwin.log`, `fix-A-diag.log`).
9. Not done: the trap contact (owner TrapContact) and the trap admission distance; stale and episodes remain red.
