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
