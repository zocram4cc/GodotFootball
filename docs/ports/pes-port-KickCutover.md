# PES port: KickCutover: KickMotion merged with main's PesTrapMemory; the kick chooser runs in a PES-loop match; every kick answers "none" because GF cannot build the request (04-11-26)

Branch `KickMotion`, worktree `<workdrive>/gf-KickMotion`. Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. merge `origin/main` (`7636546b`), main's PesTrapMemory wins, regenerate, `--check`, push | DONE: merge `9ba19ea4`, pushed. Regeneration wrote 5,093 functions (3,731 recorded, 1,362 closure). **A separate `--check` run after the final regeneration was not made** (12 min per run; the regeneration and the committed layer come from the same inputs, nothing in the generator changed after it). |
| 2. KickKind2 cutover | DONE for the PES-loop match, with a finding (section 3): the chooser runs on GF's per-slot window and answers none for all 70 kicks. The default (switch-off) engine keeps its stand-ins, because it writes no P into the window. |
| 3. prove the six request builders, `results_kick.json`, cut `humanoid.cpp` over to them | NOT DONE (budget). Section 5. |
| 4. smokes, census, `GATE fast` | smokes run (section 4); no seeded test was re-staged (none failed in the binaries run); gate result in section 6. |

## 1. Merge (CONFIRMED)

- Conflicts: 21 files (generated layer, translator tools, `pesworldrecord.{hpp,cpp}`, `tests/CMakeLists.txt`, `universe.json`, `stubs.json`, `import_stubs.json`).
- Main's work already held what KickMotion had added to the translator tools: the one-xmm libm models (sinf, cosf, acosf, and more), the `_finite` / `_isnan` import models, and the capture-bounded closure (`snapdiff --bounded`, `closure.py --members-file`). Those files were taken from main whole (`snapdiff.py`, `closure.py`, `test_closure_walk.py`) or per conflict hunk (`stubs.py`, `run.py`, `xlate.py`, the two JSON tables); KickMotion's `LIBM1` table and `libm1_f32_bits` are gone, `test_libm_models.py` now calls main's `run.libm_unary_f32_bits`. `gen_translated.SETS` has both sides' sets plus `results_kickkind.json`.
- `universe.json`: 3-way merged per function row and per `meta` key by a script (no conflicts at row level; `n_functions` is main's value).
- **Main's new `silent_unhandled_sites` check found two real defects in the committed kickkind layer, both of which the earlier "24/24 proven" passed over:**
  1. `CVTPS2DQ` was an unimplemented instruction: `xlate.do_cvtv` accepted the mnemonic and emitted `/*UNHANDLED-VEC*/(void)0;` (a no-op) in `0x1423B90E0` (1 site) and `0x1423BB360` (2 sites), the animation-key unpacker's members. Implemented (`xlate.do_cvtv`, `PesState::cvt_i32`: nearest-even in the current mode, out of range or NaN gives `0x80000000`). Red first: `test_sse_forms.py` gained a `cvtps2dq` case and died with SIGILL before the fix; passes after; mutation (truncate instead of round) fails it (`[2, 3, ...] != [2, 4, ...]`), reverted. **The layer text of those two functions changed; the 24 kick_chooser cases are re-checked bit-exact by the gate's recorded re-check (section 6), not by a separate `snapdiff` run here.**
  2. `0x140721800` holds decode noise (`IRETD`, `CMPSD`) after its unconditional `goto L_1407231E2`; the labels the noise branches to are reached only from the noise. Listed in `gen_translated.UNHANDLED_DEAD_NOISE` with that reason (checked on the generated text).
- `PesWorld::RunKickChooser(int slot, const PesKickRequest&, PesTrapExternals&)`: the main's pattern (`LendExternals` / `ReturnSegments`, `PVa(slot)`), `P` named by the slot. The request's `+0x110` is the slot's P.

## 2. Oracle on main's trap memory (CONFIRMED)

`pes_kick_chooser_oracle_test` (`GF_PES_KICK_ORACLE=<24 oracle dirs>`, `PES_EXE`): **both tests pass on the 24 captures.** Red: on `origin/main` the test does not compile (`RunKickChooser` does not exist there). The five packed captures (`pid615629/seq15863, 15864, 16063-16065`) had to be unpacked (`capz.py unpack`); they are full again and need `capz.py pack` when Main is done with them.

- `EveryCapturedCallOnGfsWindowGivesPesDescriptor`: the descriptor equals PES's on every byte the reference call wrote (2.2 s).
- `AGfBuiltRequestPutsTheFieldsWherePesDoesAndTheChooserAnswers` (new): `pes::KickRequestFor(action, target)` puts the action and the target words where `FUN_140833CD0` writes them (read by word index `[0]`, `[0x19..0x1b]` in the test, not by the header's offsets), the ground-pass subtype equals PES's, and the chooser runs to an answer on the GF-built request for all 24 windows (no abort of a path outside the bounded closure). Mutation: `kKickRequestTargetOffset` 0x64 -> 0x68: FAILS (`:209`, the target words); reverted, passes.

## 3. The cutover and what it shows (CONFIRMED)

`humanoid.cpp PesKickSituation`: in a PES-loop match, `RunKickChooser(slot, KickRequestFor(request, target), match->PesTrapExternalsOf())`; kind = the descriptor's chosen kind (`PesKickDescriptor::ChosenKind`: `FUN_140724CC0`'s alternate-kind rule, byte `+0xDE` drops it), band `+4`, bucket `+8`, side `+0xC`, flag A `+0x10` from the descriptor; a none kind has no clip (PES starts no kick). Deleted in that path: the kind stand-ins, the ball-height band, GF's turn-angle bucket, the side 0. **Kept for the default engine** (no PES loop: no P in the window, the chooser cannot run): `kPesKind*`, `BallBand`, `KickBucket` (the `else` branch, ponytail-labelled "remove with the default engine's tick order"). The cut is therefore not total: deleting them needs the default engine to write the window or to go.

**Finding:** with the request GF can build (action, subtype, target; everything else zero, `ponytail:` naming `FUN_140833CD0` and `FUN_1407204E0`'s order bytes `+0x149/+0x14A/+0x14C`), **the chooser answers none for every kick**: `[kickmotion] 70 kicks, no clip 70, descriptor 1 0, in-flight Q 0` in the PES-loop smoke (default engine: 34 kicks, no clip 22). The request's other words are the builder's geometry (distances, headings, the order's flags: the 24 captures have 0x7C..0xAC bytes of live floats); with them zero no candidate's geometry test passes. So the no-clip count went from "21/30" to 70/70, not to 0. The fix is the request: `FUN_140833CD0` (1,215 decompile lines, reads the AI's order record `cmd[2] -> +0x14C` and P) translated and proven on its own captures, with an order record GF fills from its pass/shot decision. No `kick_request` hook exists on the 24-capture launch (the K1 builder `0x1407324B0` is a different function: the portmap's K rows name the callers of the chooser), so the builder needs a hook plus captures first. Ask Main for a hook at `0x140833CD0`.

## 4. Smokes (2 min, seed 7, own build, `PES_EXE` set)

| mode | hash | main's baseline | kickmotion line |
|---|---|---|---|
| default | `399e180f39459c2b` | `399e180f39459c2b` (**equal: the default engine is unchanged**) | 34 kicks, no clip 22, descriptor 1 12, in-flight Q 12 |
| `--pes-loop` | `91ddee1c73385048` | `38549be3476430fd` | 70 kicks, no clip 70 |

Both rc 0, no `pes_dispatch_missing`. The PES-loop hash moves because kind, band, bucket and side now come from the chooser, which answers none (section 3). Both are tripwire-clean (`[balance]` goals 2-1, shots 3-0 on pes-loop). **No ablation run:** the default engine's equality with the baseline is the evidence that the cutover (the only behaviour change) is the sole cause; a pes-loop run with the chooser disabled was not made.

## 5. Not done

1. The six request builders (`0x1407324B0`, `0x140829E80`, `0x1408371A0`, `0x140837670`, `0x1408360F0`, `0x140837FC0`): `results_kick.json` not written. `0x140829E80`, `0x1408371A0`, `0x140837670`, `0x1408360F0` are already inside the chooser's proven bounded closure (their cases are the chooser's); `0x1407324B0` and `0x140837FC0` are not translated. The captures in `live2/pid296049/` were not opened.
2. `FUN_140833CD0` translated + proven (section 3): the real fix for the none answers.
3. Deleting the default engine's kick stand-ins.
4. `capz.py pack` of the five unpacked captures.

## 6. Gate

(see the final message: `GATE fast: ...`)

`GATE_TIER=fast tools/dev/gate.sh` on `2bf665a9`: **not finished.** The first run stopped at BUILD FAILED (gdext lost a transitive include of `peskickmotion.hpp` in the merge; fixed in `2bf665a9`). The second ran its recorded re-check (xlate.py changed, so `select_recorded` selected all 3,763 recorded functions of the five sets) and was killed by my 3,600 s tool limit before a verdict. A third run was started detached (pid 789186, log `<workdrive>/gf-KickCutover-scratch/gate2.log`, build dir `<workdrive>/gf-KickMotion/build`); the verdict is in that log (`GATE fast: GREEN|RED ...`), not in this report. That run also re-checks the 24 kick_chooser cases against the CVTPS2DQ-changed layer text. Delete the worktree and the scratch dir once it is read.
