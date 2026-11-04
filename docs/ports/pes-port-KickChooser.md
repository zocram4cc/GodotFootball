# PES port: KickChooser: the six callees of the kick chooser FUN_1408364D0 translated and proven (64/64 calls bit-exact); the chooser holds on PES's own request, but on GF's window 6 of 25 oracle cases differ; the PES-loop cutover is NOT wired (04-11-26)

Branch `KickMotion`, worktree `<workdrive>/gf-KickMotion`, pushed head `2412967b` (confirmed by `git ls-remote origin KickMotion`). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. translate + prove the six callees | **DONE.** `snapdiff.py --bounded --only 0x1408364D0`: `calls=64 replayed=64 mismatches=0 own_block=0.8317 (173/208) proven=True`; closure 216 -> 255 functions (the six callees and their bounded closures: 39 new members). `results_kickkind.json`, `closure_kickkind.json`, universe rows, set file; layer regenerated (5,127 functions: 3,732 recorded, 1,395 closure; only kickkind call sites changed); `gen_translated.py --check` byte-identical (rc 0); `ledger.py --check` current. |
| 2. chooser re-run with PES's own request | **Closure holds, answers do not all equal PES's** (section 3): 25 oracle cases ran to an answer with no `pes_dispatch_missing`; 19 give PES's descriptor on every byte, 6 differ (2 of them in kind/band/bucket/side). |
| 3. wire `KickOrderFor` + `RunKickRequest`, delete `KickRequestFor` | **NOT DONE** (conditional on step 2 holding; it does not). PES-loop kicks still take the zero-geometry request, so the pes-loop hash is unchanged from `59882c03`: no ablation needed. |
| 4. re-stage `GoalSceneHold ... PesLoop` | **DONE** (`9b653e8419c2`): serial simslot census, `gfsim_run --pes-loop --minutes 5`: seed 3 (goals 1-0) and seed 5 (goals 0-1) score; seed 7 (the old seed) timed out at 900 s without a result. `kSeed` 7 -> 3, assertions unchanged: `pes_goal_scene_hold_test` 3/3 pass (PesLoop 360 s, SwitchOff 30 s, NoHold 94 s). |
| 5. merge `origin/main`, `GATE fast` | merged (`b7e26db0`, clean, godot-cpp symlink intact); main has since moved to `23aed782` (batch BD, new pes-loop baseline `85c7d7c515cf92ae`): merge again. **GATE fast: NO VERDICT.** Two runs: the first died at my 3,600 s tool limit at ctest 1001/2345 (all Passed so far); the second sat >3 h in the recorded re-check (every set selected in full: 2253 core, 685 cutscene, 546 presentation, 34 impure; cold diff cache) and I killed it with no verdict. |

## 1. Proof of the six callees (CONFIRMED)

Hooks: `frame_trace.py recorded` per case (root spec `specs/kickkind/1408364D0.json`, union hook cfg of every `hooks.cfg.*`), then one `snapdiff.py --bounded --only 0x1408364D0` over all 64 (harness runs PES's bytes, the translated closure runs on the same pages; rax, xmm0 and every written byte compared).

Cases:
- 23 REAL `kick_chooser_8364D0` calls of `live2/pid615629` (seq042, 15863-15870, 15997-15999, 16054-16064; **16065 was left out**: only 4 of the 5 packed libm captures were unpacked at a time, per the disk rule).
- 16 REAL calls of `live2/pid1010053` (seq046 171 172 22012 22016 22466 22470 24042 24046 24050 24054 24065 451) and `live2/pid1330921` (seq5035 5038 5040). Mod-era launches, valid for translation proofs (Main's 01-11-26 ruling). ~30 more slim captures I had traced in these two launches were deleted by Main's disk sweep at 13:05 before they were cited; the survivors are now protected.
- 7 REAL calls of `live2/pid1513193` (seq816 881 972 1041 1117 1151 1170; the new `d(r8) in (8,9,10)` predicate: real orders).
- **18 SYNTHETIC calls** (`tools/dev/translate/synth_chooser.py`, new): PES's own builder `FUN_140833CD0` is run (harness) over each of the 15 `kick_header_833CD0` captures of `live2/pid802552`; every byte it wrote is poked into a dense copy of the capture's memory and the recorded call becomes the chooser as the Kick start makes it (`0x140724DEB..0x140724E02`: rcx = request + 0x150, r8 = request, rdx = P; both callers `FUN_140724CC0` and `FUN_1407253D0` lay it out so). 15 states plus 3 with **P+0x2816 poked 0xFF -> 0** (seq4331, 4770, 4772; GF's window holds 0, PES 0xFF in all 15 captures). 13 of the 15 first needed no more than that; seq4770/4772 first refused ("page not captured": the sparse copy turned a zero stack page into a hole, which `SnapMem` reads as a dropped page), so the tool copies dense.

Which cases enter each callee (from `addrs.json` of the replays; CONFIRMED):

| callee | real cases | synthetic cases |
|---|---|---|
| `0x1408371A0` (shot list) | seq22466, 22470 | h4358, h4361 |
| `0x1408387F0` | seq22012, 22016, 22466, 22470, 972 | h4331, 4331p, 4335, 4337, 4340 |
| `0x140830B50` | seq22012, 22016, 5035 | h4363, 4365, 4367, 4368 |
| `0x1406E7A60` | seq1041, 1117, 22466, 22470 | h4331, 4331p, 4335, 4337, 4340, 4342, 4358, 4361 |
| `0x1408223F0` | **none** | h4363, 4365, 4367, 4368 (SYNTHETIC only) |
| `0x1406EA340` | **none** | h4331p, h4770p, h4772p (SYNTHETIC only, P+0x2816 = 0) |

`0x1406EA340` (scans the AI side's `MA+0x22E4C` list for a receiver in an angle window) is called from `FUN_14071ECF0` at `0x14071FFF5` only when `FUN_141E532E0() > byte P+0x2816`; PES holds 0xFF there in every capture, so no PES call reaches it. GF's window leaves 0, which is why KickHeader saw it escape. **OPEN: what writes P+0x2816 in PES** (INF: a saturating counter); the window should hold PES's value, not 0. The two synthetic-only callees need real calls: a capture of `kick_chooser_8364D0` whose request reaches `0x1408223F0` (the h4363..4368 captures are ground passes, action 9) and, for `0x1406EA340`, a call with P+0x2816 < `FUN_141E532E0()` (INF: a kicker who has just kicked or been tackled).

## 2. What the tools do not show

- The synthetic chooser state is the call right after the builder; the re-pick caller `FUN_1407253D0` runs other code between the two calls (return `0x140725679` -> chooser `0x140725E72`); h4335/4337/4340/4342 use its layout without that code.
- `own_block` 0.8317 is the chooser's own blocks reached, not the callees'.

## 3. The chooser on GF's window with PES's request (CONFIRMED)

`pes_kick_chooser_oracle_test` over 25 oracle dirs (`trap_oracle.py` per snapdiff case: 4 real libm cases, 3 pid1513193 ones that could be built, 18 synthetic): **no abort** (the closure holds: all 25 run to an answer), `AGfBuiltRequest...` passes. `EveryCapturedCall...` equals PES's descriptor on 19 of 25; differing: `h4331`, `h4331p`, `h4335` (bytes 0x38-0x3A, 0xEC/0xED, 0xF4/0xF5: low mantissa bytes of floats), `h4766` (0x10..0x2C and floats), `h030` (**kind 0x0, band 0x4, bucket 0x8, side 0xC differ**), real `15864` (byte 0x20 only). Found and fixed on the way: `Build` took the request from rsp+0xD8, but the re-pick caller keeps it at rsp+0xC8 (`Build` now takes the register that holds it: r8; the disabled header test passes rcx); that fixed h4337/4340/4342. Tried and ruled out: the kicker's stamina cells (the "A" lines) written through `WriteSlotStamina`: no change (reverted). **OPEN: which window input differs for h030 / h4335 / h4331 / h4766 / 15864.** Next step: run the translated chooser on the GF window with the write log on and diff its read set against the harness's for h030 (kind differs, so the divergence is early).

## 4. Tools / state of the tree

- `tools/dev/translate/synth_chooser.py` (new, committed), `tests/pes_kick_oracle_common.hpp` (request by register), closure/result/universe/set/layer/LEDGER regenerated.
- Scratch `<workdrive>/gf-KickMotion-scratch` (traces `tr/`, synthetic states `syn/` (~50 GB), `sd4` snapdiff dir, `oracle2/`) is not deleted: Main can remove it once this is read. All captures I unpacked (pid802552 header ones, pid615629 seq15863/15864/16063/16064) are packed again.

## 5. Census and gate

`census.sh 7 3 5 9` (serial `simslot.sh timeout 900 gfsim_run --pes-loop --seed S --minutes 5`) first sat 1 h on the slot queue; rerun directly when two slots were free (see table). The 2-min smokes were not run; nothing in GF's behaviour changed since `59882c03` (layer widening only turns aborts into calls), so the PES-loop hash should be `91ddee1c73385048` (not re-measured).

## 6. Left

1. Find the window input that makes h030 / h4335 / h4331 / h4766 / 15864 differ (section 3); only then wire `KickOrderFor` + `RunKickRequest` and delete `KickRequestFor`.
2. Real calls for `0x1408223F0` and `0x1406EA340` (hook `kick_chooser_8364D0` keeps firing, predicate `d(r8) in (8,9,10)`; for the second add `and b(rdx+0x2816)<... ` : any call with P+0x2816 < 0xFF).
3. Writer of P+0x2816.
4. `GATE fast` on a merge of current main (needs a warm diff cache and ~2 free slots).
5. Delete the scratch dir.
