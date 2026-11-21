# PassReceiver2: ActionPassGet (BallFollow and the route manager): sizing, root, hook request; episode-length red test (DRAFT, 12-11-26)

Branch `PassReceiver` (worktree `<workdrive>/gf-PassReceiver`, `RootMotion` `89d0b5f0`). Image `pes_full.bin`. CONFIRMED = decompile/tool run this session; INF = inference; OPEN = blocker named.

## 1. The root is the action's virtual tick `0x14094E0E0`, not `0x140945EC0` (CONFIRMED)
- ActionPassGet vtable `0x1426301E0`: slot +0x10 = `0x14094E0E0(this, pc, R, p4)`. It binds the route manager `this+0x70`, runs `FUN_140945EC0(this+0x70, &ctx, &outs)` (the route walk) and then writes the action's result code (`this+0x6C`: 0 normal, 1, 3, 4, 5, 7, 8), `R+0xE5C`, `R+0xE48`, and calls the base finalize `FUN_1405141F0(this, pc, R, p4)` (the AI action base: it writes the command record R: `R+0x874` action id, `+0x878` movement code, `+0x894` target, `+0x8B0`, `+0x8C4`, `+0xE30`...). R is the record MOVESTEP's FillCommandWords reads, so PES's receiver request = R after this call.
- Hook windows (rv_hooks.displaced_window): `0x14094E0E0` 15 bytes, `0x140945EC0` 20, `0x1405141F0` 15: all clean. Request sent to Main: arm `hook passget_tick_14094E0E0 0x14094e0e0 15 1 40 48895c241048897424185741564157` (cap 24, FULL), EVERY 40 because ActionPassGet runs only for a man in AI action 5 (~54 calls/s for 0.02-2.94 s a pass).
- Real memory: 22 ActionPassGet objects (vtable `0x1426301E0`) are present in the real pass capture `pid3332804/seq051` (stride 0x1A5F0 per man): the objects exist in live memory, so a REAL call is a clean proof input; synthetic states would need the pc/R frame rebuilt.

## 2. Closure (static, `closure.py --spec-set trapaction --tag passget`)
- `0x140945EC0` alone: 229 members, 129 already in the layer, **100 new** (passget 80, ball 9, animation 9, other 2).
- `0x14094E0E0 + 0x140945EC0 + 0x1405141F0`: see section 4 for the final count after the closure was written.
- The route objects (vtable `0x1426339D8` BallFollow, slots +0x40..+0x70 = `0x140939380`, `0x1409393A0`, `0x1409393D0`, `0x140921270`...) are reached through virtual calls inside the closure.

## 3. Item 3, the PASS_GET episode length on seed 7 (test written, red/green recorded in section 4)
`PesPassTarget.EveryPassGetEpisodeIsWithinPesLiveRange` (`pes_pass_target_test.cpp`, same match as the stale-target test): every episode (consecutive ticks one man is his side's `PassGetReceiver()`) must be <= 2.94 s, PES's longest of 40 live episodes (0.02-2.94 s, `pes-re-receiving.md` 5.2); records the episode count and median.

## 3b. Item 3 result (CONFIRMED run, pushed `c59bd236` on `PassReceiver`)
The test above is **RED: 44 episodes, median 1.24 s, 12 of 44 over 2.94 s, longest 17.24 s** (seed 7, 2-min pes-loop). GF's receiver is inside PES's range in the median, but a quarter of the episodes outlast PES's longest of 40: the receiver arrives late or never plays the ball (the same men as TeamTick7 section 2's stale spans). Not weakened; it turns green when the arrival (MOVESTEP / BallFollow speed) and the touch (TrapContact) match PES.

## 4. Proof on REAL calls (CONFIRMED; 12-11-26)
- Hook `passget_tick_14094E0E0` (armed by Main on my revised request, EVERY 40, cap 24): **24 real captures** `live2/pid480109/seq001..027` (24 different calls: 8 distinct men's objects). Protected.
- `closure.py --spec-set trapaction --tag passget 0x14094E0E0 0x140945EC0 0x1405141F0`: **297 members** (221 were already in the universe; set file `closure-passget.txt`, 297 specs).
- `prove_bounded.py 0x14094E0E0 ... --real <24 captures> --merge` (harness Python, `RV_HOOKS` = the current hooks.cfg): **24/24 replayed bit-exact, 0 mismatches, 0 excluded, own-block 30/47 (0.638)**. Intermediate runs: 3/3 (0.574), 8/8 (0.532, another 8). Record `results_trapaction.json`, `closure_passget_all.json`. Real captures only (no synthetic case). Disk: the captures were unpacked as scratch copies (the live2 lock files are root-owned), at most 24 x 4.2 GB at once, deleted.
- Not reached by any of the 24 calls (so proven only by being in the static closure, translated by the same translator): the 17 blocks of the root not entered, and the route bodies the 24 calls never selected; a call that takes one of them in a match is exactly what `GF_PES_SURVEY` would list as untranslated only if the closure missed it (it is a static closure, so none should abort).

## 5. What the object needs in GF (CONFIRMED decode of the real frame; built, not yet wired)
- `this` (ActionPassGet): vtable `0x1426301E0`, size `0x2B90`, built by `FUN_14091DAB0` (base `FUN_140512CA0`, route manager `FUN_14091E390` at +0x70). **`tools/dev/translate/gen_passgetobject.py` runs PES's own constructor bytes (unicorn over the flat image, memset modelled) and writes `src/sim/onthepitch/pespassgetobject_stores.inc`** (370 u64 rows, no heap cells). Checked against the real object of `seq001` call 1: the 365 differing qwords of 1394 are padding garbage and runtime fields (counters `+0x30`, the route cells' leading bytes), none a vtable or a constant of the ctor.
- constant registry id `0x35` = `constant::PassGet` (vtable `0x142B547A8`, loader `FUN_141E6CDC0`, last store +0x98): `gen_trapconstant.py` run on its decompile gives 45 field rows (`pespassgetconstant_fields.inc`); **`pestrapdata.cpp` now builds it into the loader data (`passGetVa`, `kPassGetRegistryId`) from `passget.o` of constant_player.bin** (the name is a guess: unverified until a test builds the data on the install).
- The call frame (seq001): `rcx` = this, `rdx` = pc (a 0x50-byte record: `+4` the man's slot id, `+0x28` the orders pointer (null here), `+0x38` the ctx pointer -> `+0x18` MA, `+0xE0` the E0 ring, `+0x48` a flag byte), `r8` = R (the AI command record, at pc+0x50 in the live object: `+0x874` action id, `+0x878` movement code, `+0x894` target vec3, `+0x8A0`, `+0x8B0`, `+0x8C4` speed spec, `+0xE48`, `+0xE5C`, `+0x19C8`/`+0x19D0` order pointers), `r9` a stack scratch.
- The tick's result is the command record R plus `this+0x6C` (the route result code) and `this+0x68` (target id): `FUN_1405141F0` fills R from the route outputs (target `R+0x894`, speed `R+0x8C4`, look `R+0x8A0`).

## 6. Landed on `PassReceiver` (pushed `674a4053`; `git ls-remote` confirmed)
| piece | state |
|---|---|
| `0x14094E0E0` + closure (297 members; 5915 functions in the layer, 3779 recorded, 2136 closure) | translated, **24/24 real calls bit-exact**, `gen_translated.py` regenerated (rc 0), `ledger.py --check` current (38,514 rows, no regression) |
| `gen_translated.py` | `UNHANDLED_DEAD_NOISE` += `0x140706350` (x87/OUT noise after its final `JMP 0x140706DEA`; capstone scan: only noise branches into it) |
| `ro_tables.json` | one block `0x1425DD980..0x1425DDF38` (11 live reads in the anime row pickers `0x1406B8DE0/9AA0/9B20/9BD0/9D80/9E50`); evidence marked **INF** (per-table index bounds were not derived from the callers; the block is contiguous initialised .rdata, dumped and checked) |
| `constant::PassGet` | built into the loader data (`passGetVa`); every PES match now builds it: `PesPassTarget.*` ran on the new build (the match constructs, so `passget.o` resolves on this install) |
| ctor image | `gen_passgetobject.py` + `pespassgetobject_stores.inc` (not yet read by any C++) |
| red tests | `EveryPassGetEpisodeIsWithinPesLiveRange` RED (44 episodes, median 1.24 s, 12 over 2.94 s, longest 17.24 s); `IsSetInFlightAndNeverStaleWhileTheBallRests` still RED on this tree (11.37 s) |
| NOT done | the GF wiring (below); the red test of the per-tick approach speed/facing; MoveStraightTrapStop and the other routes as separate items (they are inside the 297-member closure, proven only where the 24 calls entered them) |

## 7. Left (numbered, exact resume)
1. **Window objects.** Add to `PesWorldImage` (pesworldrecord.hpp) per slot: the ActionPassGet object (0x2B90 bytes; initial image = `pespassgetobject_stores.inc`, kind 1 rows relocated to the object's window VA), the pc record (0x50 bytes: `+4` slot id, `+0x28` orders ptr 0, `+0x38` -> the side's ctx, `+0x48` flag) and R (the AI command record, >= 0x19D8 bytes, zero). Register `passGetVa` in the loader globals (`PesTrapLoaderData`) like `trapVa`. ONE read of the real frame (seq001 etc.) fixes the rest of pc/R offsets; every field the 297 members read from MA/MI/G that GF does not stage will show up as a mismatch against a real call.
2. **Oracle proof on the window.** `PesWorld::RunPassGetTick(slot)`: `pes_fn_14094E0E0(this, pc, R, scratch)` on the window, exactly as `RunTrapTick` does; compare R and `this+0x68/+0x6C` against the 24 real calls (a `pes_passget_oracle_test` in the `pes_trap_oracle_test` style; the 24 captures are protected in `live2/pid480109`). Each unstaged input is a `ponytail:` naming its PES writer.
3. **Use the answer.** `ElizaController::_PassGetApproach` (`elizacontroller.cpp:1143`): replace the distance ramp and `ReceivingPoint` by PES's R: target `R+0x894` (vec3), speed `R+0x8C4` (the speed spec: `FUN_140A85A40/A70` write it), look point `R+0x8A0`; build the `PlayerCommand` (`desiredTarget`, `useDesiredTarget`, `desiredVelocityFloat`, `desiredLookAt`). Delete the ramp and `pes::ReceivingPoint` in the same change (clean cutover). The red test: per tick, GF's commanded speed/target for the receiver equals PES's tick output on the same window (red while the ramp is in).
4. Then `EveryPassGetEpisodeIsWithinPesLiveRange` and `IsSetInFlightAndNeverStaleWhileTheBallRests` are the acceptance (still also dependent on MOVESTEP start-up and TrapContact).
5. `GATE branch` was NOT run on this tip (layer regeneration + two tests only); run it after step 3.
6. Scratch `<workdrive>/gf-PassReceiver-scratch` (logs, decompile dumps, `scanT2.py`, `batch.sh`) to delete; no captures kept in scratch (copies deleted).
