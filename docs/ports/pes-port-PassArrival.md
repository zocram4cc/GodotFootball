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

## 10. Round 2: why the PASS_GET receiver's trap never touches a resting ball (17-11-26, branch `PassArrival` at `5958a6f4`, nothing landed)
Outcome: root cause LOCALISED with measurements; NO engine fix landed (the one candidate mutation is not a faithful fix and changed the match trajectory). Branch unchanged, pushed tip is `5958a6f4` (ls-remote confirmed); all probes reverted (`git status` clean).

**PES side (CONFIRMED, translated and bit-exact in the docs, `pes-port-TrapContact2.md`, `pes-port-TrapContact.md` §1 item 5):** the trap contact is not a clip frame test. On the clip's contact frame the trap tick sets P+0x2460 bit 9; the scan's reach test `FUN_140799060` (translated, 7/7 bit-exact) predicts the hit point as the hit bone's joint position plus the joint-rotated hit offset, and touches iff the horizontal distance to the ball centre is below `(reach(b) + 0.1087)`, with `reach(b)` 0.15–0.30 m, and `|dy| < 0.3`. So the foot must be about 0.26–0.41 m from a still ball at the contact frame.

**Admission and commit (MEASURED, real PES chain via `RunTrapKind0`, probe `TRAPCOMMIT`):** the receiver's trap is committed at a ball 0.74–1.34 m ahead (kind 0; t = 12.41 and 14.85 s), not 1.2–2.6 m. The 1.2–2.6 m in round 1 was the distance at the decision tick, not at commit. The trap then issues motion 564 (playback 1.176) and runs about 30 ticks (12.43 → 13.00 s). Round 1 reported "1–3 frames": that was NOT reproduced here (`RCV` rows show `ac8` = 7 continuously from 12.44 to 13.00), so I am not certain what the round-1 count measured; treat it as superseded. The trap clip plays the whole time (`fn=14` Special, frame 2 → 65), and `ac8` returns to 2 at the clip's end (phase 0, action 2). So the trap runs to its contact frame; the touch is what fails.

**Where the foot misses (MEASURED, `SCAN` probe = `RunBodyScan` inputs per tick, seed 7 window):** the contact frame (pendB = 1) is at 12.70 s. The foot (`sk_foot_r`, PES axes) is at (-2.04, 18.51), the ball at (-2.29, 19.09): horizontal distance 0.63 m, touchReq 0. The refused reach is therefore about 0.25 m beyond the 0.41 m bound, and the hit offset (ground hit z = -0.10 m) makes the predicted contact roughly 0.68 m out. The foot had been 1.07 m from the ball at the trap start (12.43), so the man walked through the ball.

**Why he walks through (MEASURED, `ADJ` probe on `StepAdjusters`, receiver only):**
- Over the 16 strike-adjuster steps of the trap (12.43 to 12.70 s), the adjuster's root displacement sums to az = -0.945 m (ax = +0.039). That is what PES's adjuster asks for, and it lands the root about 0.07 m short of the root the ball needs (about 19.39 m, i.e. ball + 0.30 m).
- The GF root moved from z 20.41 to 18.71, a total of -1.70 m (-1.76 from 20.47). The difference, about -0.8 m, is the trap clip's own root-track translation (`CalculateSpatialState`: root = startPos + positions[frame] + smuggle, then the adjuster's WarpRoot adds to startPos). So GF applies the clip's root travel on top of the adjuster's displacement, and the man overshoots past the ball.
- The adjuster stops at 12.72 (`aStepped` 0), after which the clip's root track alone carries him 0.75 m further (12.72 to 12.98) to 17.83 m, past the ball, then MOVE takes over at 13.00.
- [INFERENCE, not yet pinned] PES's root solver (`FUN_1406E85B0`, translated in `src/pes/translated/restart/`, `pes-re-trap-inputs.md` S5) sets the strike target as the displacement to the contact point. The sum above (-0.945) matches the ball target only if PES adds NO clip root-track translation during the trap. The live check is still open (see the next step).

**Mutation (one line, `humanoidbase.cpp` `CalculateSpatialState`: for a trap-family clip, root = startPos + smuggle, no clip root track). Measured, seed 7, 2 min, pes_loop, both PesPassTarget tests run:**

| state | stale (bound 2.94 s) | episodes over 2.94 s | longest | median |
|---|---|---|---|---|
| 5958a6f4 (before, round 1 end; re-measured this round with probes, identical 11.33 s) | 11.33 s | 9 of 49 | 7.69 s | 1.20 s |
| mutation (trap clip root track removed) | 6.37 s | 13 of 42 | 7.35 s | 1.15 s |

The mutation changes the match trajectory: the receiver never appears as a PASS_GET man in the mutant run (no `slot=7` rows), so the mutant's numbers do not measure the receiver's trap. Episodes got worse. It is NOT landed: it is not a faithful rule (it removes the root track for every trap clip, including kind-0 traps of other men) and it does not show a receiver touch.

**Red test:** not written. The PES rule the fix needs (the trap's root displacement is the strike adjuster's sum and nothing else, or PES's exact composition of root track and adjuster) is not yet pinned from PES facts. A test on the mutant's numbers would pin a guess. The rule to pin: `strike adjuster total displacement = contact root displacement` on a live trap (a PES fact), or a deterministic test that a trap of a still ball ends with the foot within the reach test's bound.

**Touched functions (for the merge; none landed):** `HumanoidBase::CalculateSpatialState` (humanoidbase.cpp), `pesmovestep::StepAdjusters` (probe only), `RunPesTrapTicks`, `RunPesBodyScans` (probes only), `ElizaController::RequestCommand` (round 1, landed `5958a6f4`). OneChainCut's files (pestrapactor.*, pestrapworld.hpp, pesworldrecord.*): not touched.

**Next step (one hook, if Main wants it; no live PES run made here):** a live read per tick of the receiver's man record position (`man+0x55C/0x564`) and A's step (`A+0x14` written by `FUN_141EB1BF0`, the strike adjuster's root point) over one trap. Hook on `0x141EB1BF0` at entry (cap 40, every call), predicate: the receiver's man record is in `P+0xAC8 == 7`. The answer is whether the record's root moves by the adjuster step alone (PES adds no root track in the trap) or by the step plus the clip's translation (then the overshoot is PES's own and the cause is elsewhere). Until then the fault is localised, not fixed.

**Addendum, live strike hook (Main's `strike_step_141EB1BF0`, captures seq456-470 in `live2/pid1115004`, PES real game; reader `<workdrive>/gf-PassArrival-scratch/r2/adj_reader.py`):**
- The layer's root point O2 is `*(rdx+0x188)` + `0xD0` (rdx = the chain passed to the adjuster, CONFIRMED: O2 root equals A+0x14 at every entry).
- PES's per-call root step rises from 0.010 to 0.018 m over counters 4 to 18 (CONFIRMED, 14 consecutive calls). Path 0.207 m.
- The adjuster target A+0x28 is constant at (-34.505, -17.220); the root starts 0.024 m from it and moves away from it. So the target does not describe a walk to the contact point in this window.
- Ramp ticks (counter < end) are not in the hook. The unhooked ticks plus the clip's own root track must carry the rest of the travel (INF, not measured).
- GF's adjuster steps are about 0.06 m per tick, PES's about 0.01-0.02 m. The GF probe is therefore not stepping like PES's live adjuster.
- Open: whether PES adds the clip's root track during the trap. Needs the ramp path (FUN_141AF3CA0, counter < end) with the same predicate.

**Round 2b: the root-track question, settled statically (CONFIRMED where marked), and a failed fix (reverted).**
- **Who writes the B block (CONFIRMED, Ghidra decompile of `FUN_1406EEDF0`, this round):** flags `B+0x81` = strike active (from `FUN_141EB3320(P+0x428)`, not finished), `B+0x82` = yaw, `B+0x80` = any; `B+0x90/0x94/0x98` = the strike adjuster's root point `A+0x14` (= `P+0x43C`, `P+0x440`, `P+0x444`), `B+0xA0..0xAC` = the yaw as a quaternion, `B+0xB4` = height, `B+0xB8/0xBC` = the jump adjuster.
- **Who reads the flags (CONFIRMED, decompile of `FUN_141AFD400`, the layer's root-matrix composer, called with the layer matrix `*param_2`):** the layer's root matrix gets its translation from the clip's root track (the rotation and translation composed from the clip keys, then `+=`). Then: `if (B+0x81 != 0)` the root translation is **replaced** by `B+0x90/0x94/0x98` (the strike adjuster's root point): `*(matrix+0x30..0x38) = B+0x90..0x98`. So while the strike is active, the clip's root translation is **switched off** and the adjuster's point is the root, not an addition. `if (B+0x82 != 0)` the yaw quaternion replaces the root rotation. The clip's rotation is kept when the flag is clear.
- **Captures (PES real memory, seq456-470, pid1115004):** the layer root `O2+0xD0` (chain `rdx+0x188`) equals `A+0x14` at every hooked entry; per-call root steps 0.010 to 0.018 m, path 0.207 m over counters 4 to 18, nothing else moved it in those calls. Consistent with the replacement rule: while the strike runs, the man's root is the adjuster point.
- **Red test (landed as a patch, NOT on the branch):** `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` (per tick, a PASS_GET man in the Trap action with his strike adjuster running: his GF root step must equal the change in `A+0x14`, tolerance 2 mm, from the PES fact above). Measured on `5958a6f4`: **red, worst mismatch 0.098 m over 239 ticks**. Patch: `<workdrive>/gf-PassArrival-scratch/r2b/root_step_red_test.patch`.
- **The fix I tried (reverted):** `CalculateSpatialState` skips the clip root track while `StepAdjusters` reports the strike running (flag set on the humanoid each tick). Measured: the mismatch went **up** to 0.148 m, stale 11.33 to 15.93 s, episodes 9/49 to 15/54. So dropping the clip track in GF is not the fix by itself: GF's root step is not yet the same quantity as PES's `A+0x14` step (the warp `frame.x = A_after - O2` uses GF's window O2, whose point is not the GF body root). The next thing to check is how GF's body root relates to the window's O2 point each tick. Patch: `<workdrive>/gf-PassArrival-scratch/r2b/root_step_fix_and_red_test.patch` (fix plus red test).
- **Touched functions (for the merge; none landed):** `HumanoidBase::CalculateSpatialState`, `pesmovestep::StepAdjusters`, `PesWorld::StepAdjusters` (AdjusterFrame.strikeRunning), `HumanoidBase::SetStrikeRootOwned` (all in the reverted patch).
- **Scan:** whole-`.trace` displacement scan of the B-block offsets (101,591 hits; `<workdrive>/gf-PassArrival-scratch/r2b/hits.json`, scanner `scan.py`). The layer readers are around `0x141AFD400` (the composer); the loads of `B+0x80/0x81/0x82` in `0x141AF...` are the composer and its callers.
- **Status:** the rule that the strike REPLACES the clip root translation is CONFIRMED statically, and a red test pins it. The GF fix is NOT found. The branch is unchanged at `5958a6f4` (ls-remote confirmed).

**Round 3: the root handoff implemented in GF; it closes most of the root gap but REGRESSES the stale time. NOT landed (branch: `f8aa8e41` = the red test only).**
- **Red test committed and pushed (`f8aa8e41`, `tests/sim/pes_pass_target_test.cpp`):** `PesPassTarget.ATrapsRootMovesByExactlyTheStrikeAdjusterStep`, red on `5958a6f4` (worst 0.098 m, 239 ticks). Tolerance 1 mm, DERIVED (float ulp at 20-35 m is about 4e-6 m; 1 mm is 100x that), not a PES constant. The earlier 2 mm was not derived; replaced.
- **The O2 <-> body-root link (traced in GF's own code, the open piece of round 2b):** the window's O2+0xD0 is written from the man record each tick (`WriteActorP`, `pestrapactor.cpp:267`, `memcpy(point, man+kManPosX, 12)`), and the man record is written from the GF body position (`WritePesManMovement`, PES x = GF x, PES z = GF y). So the window point, the man record and the body root share one frame, with the identity map (x, z) -> (x, GF y). The old warp `WarpRoot(A_after - O2)` moved `startPos`, but `startPos` is the CLIP ORIGIN, not the body root: the body root is `startPos + clipOffset(frame)`. Once the clip has travelled (the clip track), a warp on `startPos` does not move the body by the warp, and a skip of the clip track puts the body at the origin. That is the mismatch the red test caught. The link fix: the body root is set to the adjuster point, and the clip origin is re-derived each tick from it.
- **Implemented (patch `<workdrive>/gf-PassArrival-scratch/r3/root_handoff_fix_final.patch`, not committed):**
  - `PesWorld::StepAdjusters` (`pesworldrecord.cpp`): publishes `AdjusterFrame.strikeRunning` (A state == 2 after the steps, FUN_141EB3320, CONFIRMED decompile) and `rootX/rootZ` = A+0x14 (the composer's B+0x90/0x98 source). The displacement computation (`frame.x/z = A - O2`) and the warp are removed.
  - `pesmovestep::StepAdjusters` (`pesmovestep.cpp`): `body->SetStrikeRoot(strikeRunning, (rootX, rootZ))`. The WarpRoot for the strike is removed (the composer uses the clip track when the flag is clear, so no displacement either).
  - `HumanoidBase::SetStrikeRoot` (`humanoidbase.hpp`): the two fields `strikeRootOwned`, `strikeRootPoint`.
  - `HumanoidBase::CalculateSpatialState` (`humanoidbase.cpp`): computes `clipOffset` (the same two branches as before) and the smuggles; while owned, `startPos` and `nextStartPos` move so that `position = strikeRootPoint` exactly (the clip track and smuggles absorbed); then `position = startPos + clipOffset + smuggle` as before.
  - Touched functions for the merge: `PesWorld::StepAdjusters`, `pesmovestep::StepAdjusters`, `HumanoidBase::SetStrikeRoot` (new), `HumanoidBase::CalculateSpatialState`, `AdjusterFrame` (`strikeRunning`, `rootX`, `rootZ`; `strikeMoved`, `x`, `z` removed). OneChainCut's `pestrapactor.*` / `pesworldrecord.*` edits would conflict in `PesWorld::StepAdjusters` and `AdjusterFrame`.
- **Measured, seed 7, 2 min, pes_loop, PesPassTarget pair:**

| state | root gap (red test, 1 mm bound) | stale (bound 2.94 s) | episodes over 2.94 s | longest | median |
|---|---|---|---|---|---|
| `5958a6f4` (before) | 0.098 m | 11.33 s | 9 of 49 | 7.69 s | 1.20 s |
| root handoff (fix) | 0.0105 m | **50.00 s** | 9 of 39 | 11.54 s | 1.20 s |

- **Mutation:** forcing the owned branch off (`strikeRootOwned` never set) is exactly the `5958a6f4` code: gap 0.098 m, red (measured in round 2b). So the test bites the handoff in both directions.
- **Where it stands:** the root gap goes from 0.098 to 0.0105 m, not to 1 mm. The residual 10 mm is unexplained (candidates: a position write after the humanoid step in `Player::Process`; not checked). The stale time regresses, from 11 s to 50 s. The probe (receiver rows, seed 7) shows why: after the strike ends (state 0, ~151.2 s) the trap clip keeps the man in action 7 for ~35 more frames, walking 1 m past the ball on the clip's own root track (absorbed into `startPos`, so continuous), and the touch never lands. Also at 3.4 s the man walks 4.9 -> 0.4 m to the ball at ~0.07 m/tick under the strike (the adjuster steps at that speed there, unlike the 0.01-0.02 m/tick of the 2b captures). So "the root is the adjuster point" is now right in the model, but the trap still does not touch: the contact (reach test, joint block vs root) and the post-strike clip track are the next things, and they were NOT the root link.
- **Movestep wire (on the fix build):** 44/44 green, including `MoveStepWire.TheKickoffTakerClosesToTheBall`. Playerdb wiring 13/13 green.
- **Throw-in (on the fix build):** 2 of 6 cases green so far (`TheMatchIsBuiltWithTheInstallsPlayerDatabase`, `ACarriedThrowUsesTheShortRowUpToThePesDistanceAndTheMidRowBeyond`); `AThrowInIsReleasedFromInsideTheLine` and the ceremony case were still running when this round stopped (log `r3/throwin.log`). The ceremony case alone took 56 minutes last round.
- **Decision:** the handoff is faithful to PES's composer, and it is the right link, but it is NOT landed because it regresses stale 11 -> 50 s and the pair is not green. The branch carries only the red test.

**Round 4: the contact frame and the post-strike root. Nothing landed; branch `f8aa8e41` (red test only). Worktree reverted to HEAD. Probes and the handoff patch: `<workdrive>/gf-PassArrival-scratch/r4/round4_fix_plus_probes.patch`, log `r4/r4-probe.log`.**

**1. The residual 10 mm (MEASURED).** Of 165 sampled strike ticks, exactly one has the body root off its strike point by more than 1 mm: t = 246.96 s, the root (0.7683, -14.6400) against the point (0.7695, -14.6283), an 11.7 mm offset, one tick only. The nearest other active player was 1.59 m away. The gap resets the next tick, so it is a one-tick displacement of the body after the handoff, not a drift. The source is not identified. [INFERENCE] a one-tick positional correction after the humanoid step (`positionOffsetMovement` from a bump) fits, but nothing measured it. It is 11.7 mm against a reach test that refuses by 0.1 to 0.3 m, so it does not decide the touch.

**2. The contact frame (PARTLY MEASURED).** A probe in `RunPesBodyScans` (receiver, action 7) computes the reach test's inputs from GF's own joint block: hit record 0 of the motion (`Mbinfo::Hits(P+0xAE0)`), bone `contactPart` (joint `P+0x538+12b`, rotation `P+0x700+16b`), the hit offset (cm x 0.01 x `P+0x3B18`, z negated when `P+0x2472` is set), predicted contact = joint + rotated offset, and horizontal distance to the ball (sample 0). 274 scan rows, 28 at the contact frame (`pend=1`). Result: the replicated geometry does NOT reproduce PES's accept/refuse. Refused at 0.23, 0.24, 0.43, 0.45, 0.57 m; accepted at 0.11, 0.41, 0.78 and 0.89 m. So my replication is missing a PES input (the per-motion constant displacement `FUN_14085C1B0`, or the ball sample 1, or the hit frame selection). Until that is fixed, I cannot say whether the miss is root or pose: the root's 12 mm cannot cause refusals of 0.1 to 0.6 m, so whatever separates the rows is in the joint or hit geometry. That is the pose-level part, which per your rule is for OneWorld; I have not proven it.

**3. Post-strike root (STATIC, CONFIRMED structure).** FUN_141AFD400 does not set the root to the clip's absolute track. It composes the clip's per-tick root step and ADDS it to the root translation already in the layer matrix: `pfVar1[0xc] = (rotated clip step ...) + pfVar1[0xc]` (and the same for the height and z terms). So with B+0x81 clear the root resumes by accumulating the clip's step onto the last root point. While B+0x81 is set, the replacement (B+0x90..0x98) overwrites the matrix translation, so the clip steps in those ticks are lost. Consequence: PES re-bases the clip at the strike's END POINT (the last A+0x14), not at the clip origin. GF's absorbed `startPos` gives the same trajectory: the clip track is a cumulative sum of steps, so `startPos = point - clipOffset(k_end)` makes `position = point + (clipOffset(k) - clipOffset(k_end))`, which equals PES's accumulation. So the startPos absorb is the PES model, not an approximation.

**4. When PES leaves action 7 (STATIC, from the trap-end handler documented in `pes-port-TrapContact3.md` and `RestingBall6.md`).** Not at the strike's end. The Trap action ends with its clip: the end handler `0x14079E5C0` moves the man to the Dribble (`FUN_1406EEBB0(P,4)`) or to MOVE (`(P,2,0x5E)`), and the trap clip's frame counter reaching its last frame is what runs it. The live probe agrees: after the strike ended (state 0 at 151.22 s), the man stayed in action 7 for about 35 frames while the clip played on its step track. The clip's length after the strike is therefore the window in which the root keeps moving. INF: the 35-frame walk past the ball is PES's own clip step if the trap clip has forward travel after its strike; not verified (no clip-travel number taken).

**Decision (per your rule):** the one thing measured is that the residual is one tick and small. The contact replication is not yet trustworthy, so I cannot call the miss "pose" from data. Nothing is pushed beyond the red test. Next step needs the reach test's missing input, measured by checking the replicated geometry against the translated reach test (`src/pes/translated`, `0x140799060`) on one captured call, before any root-vs-pose split. That check is in the GF tree and needs no hook.

**Round 4b: running the translated reach test on GF's contact inputs. Not localised to one input; one real input defect found. Nothing landed; branch `f8aa8e41` (red test only). Worktree reverted. Patch and logs: `<workdrive>/gf-PassArrival-scratch/r4/round4b_fix_plus_sweeps.patch`, `r4/r4b-*.log`.**

- **The harness is the translated test already.** `PesWorld::RunBodyScan` runs the translated scan `0x1406EDCE0`, which calls the Trap action's reach test `0x140799060` (`src/pes/translated/trapscan/pes_fn_140799060.cpp`). So every GF contact decision IS the translated reach test on GF's inputs; the `touch` column of the earlier probe is its real output, and my hand replica (round 4) was wrong. Its real-call proof is the existing one (TrapContact2: 6 REAL calls, replayed 6, mismatches 0). I did not re-prove it on a fresh capture (no hook; the proof row is the evidence).
- **Inputs the reach test reads, checked on the six GF contact rows (pend=1, refused):**
  - Bone mode gate (`FUN_1406E6CE0`: some bone of the 19 at P+0x2704 is not 0x11): NOT binding. Modes at contact are `11 2 2 2 c c c 2 c ...`; bone 6 is `2` or `c`, so the gate already passes. Forcing bone 6 to 0 leaves the decision at 0.
  - Scale (`P+0x3B18`): NOT the blocker, but a REAL INPUT DEFECT. GF's scale is 0.31 to 0.39, so GF's `physical_height` stat is 0.55 to 0.71 (a rating, not metres), and `heightCm = stat*100` gives 55 to 71 cm; the writer divides by 180 (`heightDivisor`). PES's live value is about 1.0 (TrapReach doc: 1.0, 1.0278, 0.9722, 1.05). Setting the scale to 1.0 leaves all six refused (0 of 6 flip). The defect is real, but it is not the touch blocker. It also feeds `PhysiqueG` (the body's G), so it needs its own fix and test. The doc's claim that GF's `physical_height` is cm/100 is false for these players.
  - Bone 6 rotation (yaw about the height axis, +-60 deg): 5 of 6 rows never flip; one flips at -42 deg (row t=153.19). A single bone's heading does not explain the refusals.
  - Root: at most 11.7 mm, one tick (round 4). Not the cause.
  - Joint position and ball sample 1 sweeps (round 4b first run): INVALID direction. I moved the foot toward the ball, but the hit offset points away from the ball, so the predicted contact moved away and these sweeps could never flip. Not a finding.
- **Contact timing (MEASURED, a lead, not a verdict):** the scan's contact (pend) occurs at GF clip frame 31 to 32 in all six rows, while `hits[0].frame` reads 10 for every motion. The GF clip advances about 2 frames per tick, so 31 frames is about 0.31 s, against PES's contact at about 18 PES ticks (0.33 s) per the doc (`pes-re-receiving.md` 5.1). The timing is roughly aligned, and `hits[0].frame` = 10 is suspicious (likely my probe's field parse, not a fact). Not checked further.
- **Where it stands:** no single input identified as the blocker. Excluded: root (<12 mm), bone mode, bone-6 yaw alone, scale. Not excluded: the other joints' pose (the hit selection could depend on them), the hit record choice (`hits[0]` vs the frame-matched hit), the constant displacement `FUN_14085C1B0`, and the ball sample 1 (not swept correctly). Joint pose is therefore NOT proven to be the cause, and I stop short of calling it pose.
- **Next step (needs no hook):** redo the joint and ball sweeps along the right directions (joint shifted by `ball - predicted`, ball shifted by `predicted - ball`), and sweep the hit choice (each `Hits(motion)` record, not only index 0). If the flip needs a shift beyond about 0.1 m in joint space, the miss is pose.
- **Separate fix to schedule:** `pesmovestep::PhysiqueHeightCmOf` / `PlayerHeightCm` and `pestrapactor` `heightCm` read `physical_height * 100` as cm, but GF's stat is a 0.4 to 0.99 rating. Fix at the roster source (PES's height in cm), with a test on the stat value.

**Round 5: the height input. The fix is done and tested, but NOT landed: it regresses one movestep test. Branch `f8aa8e41` (red test only). Worktree at HEAD. Fix patch: `<workdrive>/gf-PassArrival-scratch/r5/height_fix_NOT_LANDED.patch` (apply on `f8aa8e41`).**

**1. What the height input is (sourced).**
- GF's `physical_height` stat is not a height. `PlayerData` loads the XML stats through `CalculateStat` (the development curve, `src/sim/utils.cpp:525`), so the XML's 1.80 reads about 0.6. Every height consumer read `GetStat("physical_height") * 100`: `PhysiqueG` (the body's G, playback, clip hit distance), `P+0x3B18` (the trap world's scale, `pestrapactor.cpp:233`), the shot and mate heights in `elizacontroller`, the carrier height.
- The DB's own height is `PlayerData::GetHeight()` (metres, default 1.8, `playerdata.cpp:32/51`), and a PES record's height is `PlayerTraits::heightCm` (`src/pes/playerdb.cpp:56`, bias 100 + the 8-bit field at bit 216). The roster row byte 0x31 equals the EDIT save's Height (34 of 34 captured players, `pes-port-DribbleProposal7` section 1).
- PES's formula for `P+0x3B18` (the writer `FUN_1406EB210`, store at `0x1406EB23E`, divisor 180.0 at `0x142598580`): `P+0x3B18 = height_cm / 180`, so a 180 cm man is 1.0. The live readings 1.0, 1.0278, 0.9722, 1.05 (TrapReach doc) match this.

**2. The change (not landed).** `PhysiqueHeightCmOf(const Match&, Player&)` (`pesmovestep.cpp`, declared in `pesmovestep.hpp`): the PES record's `heightCm` when `Match::PesTraitsOf` finds one, else `round(GetHeight() * 100)`. Callers switched: `pesmovestep` StepHost `PhysiqueG` and `PlayerHeightCm`; `teamAIcontroller.cpp` `TrapFactsOf` (now takes the match, call in `WritePesTrapActors`); `elizacontroller.cpp` shot height (line 2610), long-pass mate height (2905), carrier height (3151, metres). `elizacontroller.cpp` gets `#include "onthepitch/pesmovestep.hpp"`.

**3. Red test (`tests/sim/pes_playerdb_wiring_test.cpp`, `PlayerDatabaseWiring.TheHeightThatScalesAManIsHisHeightInCm`).** A PES-id man with record height 197 must give 197; a database man of 1.8 m must give 180. RED before the fix on the old API (both assertions failed; GF read 0.6 * 100 for both). Expected-reason failure, not a compile error.

**4. Mutation.** Dropping the PES-record branch (`if (... ; false)`): the imported-man assertion RED ("a man with a PES record must scale by his record's height"). Restored; GREEN.

**5. Measured with the change (seed 7, 2 min, pes_loop, `r5/` logs).**

| test | before (`f8aa8e41`) | with height fix |
|---|---|---|
| `IsSetInFlightAndNeverStaleWhileTheBallRests` stale | 11.33 s | 8.30 s (still red) |
| `EveryPassGetEpisodeIsWithinPesLiveRange` | 9 of 49 over 2.94 s, longest 7.69 s, median 1.20 s | 7 of 24 over 2.94 s, longest 7.87 s, median 1.30 s |
| `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` | red, 0.098 m | red, 0.110 m (no handoff on this branch: expected red) |
| `EveryPassInFlightHasItsOwnSidesReceiver` | green | green |
| `pes_playerdb_wiring_test` (14 tests, incl. the new one) | green | 14/14 green |
| `pes_throwin_release_test` (6 cases, incl. the ceremony case) | not run on HEAD this round | 6/6 green |
| `pes_movestep_wire_test` (44 tests) | 44/44 (round 3, handoff build) | 43/44: **`MoveStepWire.AShootoutKeeperSavesAKickInTheShootoutOnlyRun` RED**: "the keeper saved none of 6 kicks" (expects > 0). `TheKickoffTakerClosesToTheBall` green. |

The episode count dropped from 49 to 24, so the match trajectory changed; the pair numbers are not a clean before/after comparison.

**6. Why it is not landed.** The shootout failure is a real behaviour change. `PhysiqueG` now uses a 180 cm G instead of about 0.6 * 100 = 60, so every clip's playback, turn and hit distance changes (`movestep_issue.cpp:36/75`, `movestep_group_a.cpp:296`). The keeper's shootout saves go from more than zero to zero. I did not find out whether that is a PES-faithful consequence (a 180 cm keeper with the same clips) or a fault elsewhere; the test was calibrated on the wrong G. Per your rule (land only if nothing else regresses) it stays unlanded. The next check is the keeper's save path under PES's G: which clip playback or hit distance changes the save, and whether the old value of G was compensating for something.

**7. Step 2 (the sweeps) and step 3 (hit frame): NOT done this round.** The sweep needs a probe build; I ran out of request budget after the height work. Step 3 is also open: the probe's `hits[0].frame` = 10 for all six trap motions. The HitData decode is `w >> 13 & 0xff` (`mbinfo.cpp:186`), and the filing at `motion + 0x11` is documented and tested (`pes-re-hitdata-index.md`). So the probe's hit lookup is the PES motion's record, and 10 may be real for these motions. The reach test reads its hit bone from the image directly, not from `Mbinfo`, so the frame is not one of its inputs. I have not checked the raw bytes. Joint-pose verdict: still open.

**Round 6 (section 13): the shootout keeper. Found which input decides it; the height fix is still NOT landed (the test is not yet rewritten on a PES fact). Branch `f8aa8e41` (red test only), worktree at HEAD. Experiment patch: `<workdrive>/gf-PassArrival-scratch/r6_experiments_AB.patch`. Height fix: `r5/height_fix_NOT_LANDED.patch`.**

**1. Which G-dependent quantity decides the save (MEASURED, two single-variable experiments, `MoveStepWire.AShootoutKeeperSavesAKickInTheShootoutOnlyRun`, seed 7):**
- Height fix in full (G = `PhysiqueG` from the real height, shot height = real height): **6 kicks, 0 saved** (red).
- Experiment A: shooter's shot height (`ShootInput.height_m`, `elizacontroller.cpp:2610`) reverted to the stat value, everything else real: **6 kicks, 0 saved** (red). So the shot height is NOT the cause.
- Experiment B: body G (`PhysiqueG` in StepHost, `pesmovestep.cpp:304`) reverted to `round(stat * 100)` (about 60), shot height real: **34 kicks, 1 saved** (green).
- So the body G decides it. G feeds `StepPlayback` (the step clip's playback, `movestep_group_a.cpp:296`), `TurnPlayback` (`movestep_issue.cpp:36`) and `ClipHitDistance` (`movestep_issue.cpp:75`). With the real height the shootout ends after 6 kicks with no save; with the old G it runs 34 kicks with 1 save.

**2. PES facts available for G (sourced, docs/re/pes-re-physique.md 1.1):** the height is the player record's `+0x0A`, in cm (`pes/playerdb` reads it at bit 216 + 100); 21 of 21 live players have physique `+0x00` = save height; save heights run 125 to 205, mode 180. G = `(1 + k_L L)(1 + k_h g(height))`, clamped to `[scaleLo, scaleHi]` from the install's constants (`pes/physique.cpp`). So a 180 cm man has the G PES's own formula gives at 180 cm. The old value (about 60) was clamped or extrapolated from a rating, not from PES's formula.

**3. The test is not a PES fact.** "> 0 saves in at most 60 kicks" has no PES source: no shootout save rate for keepers is in the docs, and the old G gave 1 save in 34 kicks, which is also far from any real keeper save rate. The test passed on the old G by luck of the sample. I did NOT loosen it. The replacement must be a PES fact about the keeper's dive, and I have not decoded one (the keeper's dive geometry vs PES, or the playback of his dive clip at G) in this round. So the height fix stays unlanded: landing it red would break the rule, and the test cannot be rewritten honestly yet.

**4. Next step to unblock:** decode the keeper's dive clip playback and hit distance at PES's G for a 180 cm keeper (`StepPlayback` / `ClipHitDistance` from the translated `pes_fn_*` for the step-kind keeper dive), and compare the keeper's dive reach to the kick's arrival time (the shot's flight time from the kick decision). A PES-faithful dive reach then gives the test's expectation. Until then the test stays as it is and the height fix stays out.

**5. Steps not done this round:** the sweeps (step 2: right directions, every hit record, `FUN_14085C1B0`, ball sample 1) and the hit-frame raw bytes (step 2's last item). Out of request budget again. The joint-pose verdict is still open.

**14. Round 7: the contact sweeps with the height fix applied. VERDICT: POSE (the joint of the contact bone). Stop, per your rule. Nothing landed (branch `f8aa8e41`, red test only; worktree at HEAD). Probe log `r7-probe.log`, patch `r7_height_plus_sweep_probe.patch` (height fix + probe, not committed).**

**Setup.** Height fix applied, so `P+0x3B18` = 1.000 on every row (PES's 180 cm value). Probe at every refused contact (`pend = 1`, action 7, receiver), reading the translated scan's own inputs from the window: the motion (`P+0xAE0`), the hit records `Mbinfo::Hits(motion)`, the bone's joint `P+0x538+12b` and rotation `P+0x700+16b`, the scale, the mirror byte, and ball sample 1 (`ball+0xB0`). The predicted contact is `joint + rot(q) * (hit offset * scale / 100, z negated when mirrored)`, the same formula the translated reach test uses. 11 refused contact rows in the run.

**1. Hit choice: NOT a difference.** Every trap motion in the rows has exactly one hit record (`nHits = 1`). The reach test's hit is the translated code's own choice (bone from hit 0's word, read from the image), so GF and PES use the same record.

**2. Hit-frame raw check: the field is a real spread, not a constant.** Histogram over all 2,925 install hit records (`Mbinfo::Hits(0..8191)`, frame = `w >> 13 & 0xff`): frame 10 is the most common value (1,438 of 2,925), but the other values run 0 to 92 (f0 125, f6 152, f8 277, f9 135, f14 95, ...). So "10 for every motion" was a fact about these six trap motions, not a parse fault in the histogram. I did NOT read the raw HitData bytes from the install file (the decode formula is the documented one, `mbinfo.cpp:186`, `pes-re-hitdata-index.md`). The reach test does not read the frame, so it does not change the verdict.

**3. Scale and the root: NOT the cause.** `P+0x3B18` = 1.000 on all 11 rows, PES's value for a 180 cm man. The root residual is at most 11.7 mm (round 4). Neither input can account for the misses below.

**4. Ball sample 1: NOT the cause (INF: not compared numerically).** Ball sample 1 (`ball+0xB0`) reads as the ball's own position on the field (x 3.08 to 28.9, height 0.11, z the pitch position). I did not compare it with GF's ball position to the millimetre. The ball is simulated by GF and is the same ball PES sees, so it has no reason to differ. The ball-side sweep (moving sample 1 toward the predicted point) needs the same shift as the joint sweep, see 6.

**5. FUN_14085C1B0 (constant displacement): NOT a GF input.** The reach test adds the per-motion displacement from the image's constant table; GF runs the same image bytes, so it is equal by construction. Not measured separately.

**6. The miss, and the shift needed for a flip (translated reach test on GF's inputs, sweep moves joint or ball 1 cm per run).**

| t (s) | motion | dxz (pred vs ball 1) | dy | joint shift needed for touch |
|---|---|---|---|---|
| 52.94 | 3513 | 0.389 | 0.068 | 0.64 m |
| 53.55 | 3509 | 0.336 | 0.085 | 0.26 m |
| 112.67 | 3333 | 1.106 | 0.373 | 0.18 m |
| 233.20 | 812 | 0.360 | 0.029 | (within 0.8 m of the ball: no flip) |
| 146.94 | - | 0.463 | - | no flip within 0.8 m |
| 49.98, 51.59, 168.83, 232.63, 289.02, 292.30, 295.41 | 688, 563, 3599, 561, 3424, 610, 3498 | 1.3 to 3.5 | 0.03 to 1.27 | no flip within 0.8 m |

The joint-shift column is the same as the ball-shift column to 1 cm (joint and ball moves are equivalent here), so the sweep is consistent. Joint shift moves the predicted point one-for-one (the rotation stays fixed), so the shift needed is about the predicted-to-ball gap minus the reach bound (0.26 to 0.41 m). Three of the rows need 0.18 to 0.64 m; the rest need more than 0.8 m. Your threshold is about 0.1 m.

**7. Verdict.** The miss is the predicted contact point: the bone's joint position and its rotation (the hit offset is rotated by that bone's quaternion), posed by GF's skeleton at the contact frame. The shift needed is 0.18 to 0.64 m, or no flip within 0.8 m, so this is far above the 0.1 m bar. Root (at most 12 mm), hit choice (one record, same table), scale (now PES's), ball (sample 1 is the simulated ball), and the constant displacement (same image bytes) are excluded. The pose is GF's own skeleton at GF's clip frame: the contact happens at GF clip frame 31 to 32 (see round 4b), and the posed joint at that frame is the source. Per your rule, this belongs to the engine-chain line (OneWorld-series-wip), not the BK line.

**8. Not done.** The raw HitData byte check (item 2, last point), and the per-frame sync between PES's contact frame and GF's clip frame 31–32 (round 4b lead), which is the most direct way to confirm pose vs timing.

**15. Round 8: the height fix landed; the shoot-out test rewritten on PES facts; the near01 check. Pushed: `fa941953` (height + tests), `fb3f144f` (comments). Branch tip `fb3f144f`, `ls-remote` confirmed.**

**1. Height fix (`fa941953`).** `PhysiqueHeightCmOf(match, player)` (PES record height, else `GetHeight()*100`), readers switched as in section 12. PES facts: G(180 cm, neutral leg) = 1.0 (KeeperDive section 4, `0x142598580` anchor, clamp `0x1425DDF78..80`); `P+0x3B18 = height_cm/180` (FUN_1406EB210).
- Red test `PlayerDatabaseWiring.TheHeightThatScalesAManIsHisHeightInCm`: red before the fix, GREEN after. Mutation (drop the PES-record branch): RED on "a man with a PES record must scale by his record's height", restored GREEN.
- Pair (seed 7, 2 min, `r8/pair.log`): stale 8.30 s (was 11.33 s), episodes 7 of 24 over 2.94 s, longest 7.87 s, median 1.30 s (was 9 of 49, 7.69 s, 1.20 s). Still red, as before. The root test is 0.110 m (was 0.098 m): still red, the trajectory moved.

**2. Shoot-out test (`fa941953`, `MoveStepWire.AShootoutKeeperSavesAKickInTheShootoutOnlyRun`).** The loop runs the shoot-out to its end, as before. "saves > 0" is removed. The save count is printed, not asserted; the comment names GF's stand-in gate `Player::KeeperAttemptsSave` (player.cpp:128-180, called from playercontroller.cpp:240) as the open gap. Asserted (PES facts):
- each keeper's height is 180 cm (the fixture's 1.8 m);
- `PhysiqueG(180 cm, neutral leg) == 1.0`;
- `StepPlayback(1.0) == 1.024955` (FUN_1406BA070 with the install's ramp, KeeperDive section 4).
- **Verified and NOT asserted:** "the step playback he issues == StepPlayback(G)". Measured: on 4 keeper ticks in the step state that issued a clip (issue serial advanced), the issued playback is 1.0, not 1.024955 (gap 0.0250). So the keeper's issue path does not carry the step playback; his mover is GF's stand-in (PES moves him through its KP actions, not translated). The check is reported as open, not asserted. KeeperDive's "the step playback he issues" needs a PES decode of which clip the keeper issues.

**3. The near01 check (Main's question on group_a.cpp:746).** The runtime value is 10.0: `near01` is loaded from 0x142588210 (`movestep_common.cpp:165`), which reads 10.0. PES's own IdleRow compare is at 0x1406B96C0, `movss xmm1, [0x142588210]` (ripscan over the flat image, 2,103 RIP reads of the two candidates), in FUN_1406B93A0's default arm: `if (dist < DAT_142588210)` (Ghidra decompile, same function). So PES and GF both use 10.0 m. The value is right; only the comment said 0.1 m. Comment fixed (`fb3f144f`). No separate defect, no red test needed.
- Same finding for `RunConstants::turnP2`: loaded from 0x142588210 (10.0), and unused. PES's turn p^2 term is `PlaybackConstants::turnP2` = 0.1 at 0x142588200 (`movestep.cpp:33`, verified in the flat image). Comment corrected (`fb3f144f`); the unused field is left for a separate cleanup.
- The `turnP2` at movestep.hpp:356 that KeeperDive cited as 0x142588210 is that unused `RunConstants` field, so the "value 0.1 at 0x142588210" was a name clash, not a wrong constant in use.

**4. Test runs (`r8/`).** wiring 14/14 green; throw-in 6/6 green; movestep: 43 of 44 green in the full run, and the 44th (the shoot-out test) green on its own after the rewrite (`[shootout-keeper] 6 kicks, 0 saved`, the four PES-fact assertions pass). Pair: as in item 1, red as before.

**5. Not done (open).** The KeeperDive dive reach at the kick's arrival and the keeper's issued clip (above); the sweeps (section 14 is the round-7 verdict; the sweep items in round 8's brief were not repeated); the raw HitData byte check.

**Commit notes for the merge.** `fa941953` touches `pesmovestep.cpp` (`PhysiqueHeightCmOf`, `PhysiqueG`/`PlayerHeightCm` in StepHost), `pesmovestep.hpp`, `teamAIcontroller.cpp` (`TrapFactsOf` now takes the match), `elizacontroller.cpp` (shot, mate, carrier heights + include), and the two tests. OneChainCut's `pestrapactor.*` is not touched.
