# pes-tool-SynthTool: one generator for SYNTHETIC states (`translate/synth.py`)

Branch `SynthTool`, pushed. origin/SynthTool = `bc387359` (confirmed by `git ls-remote`). Commits: `461e87f2` (synth.py and test, WIP), `4a51889f` (caller migration, CMake, README), `bc387359` (emulated specs run in the trace environment). Worktree base: origin/main `18b66a3d` (main had moved past `784ffd5d` when the worktree was cut).

## What changed

- **`tools/dev/translate/synth.py`** (new). One JSON spec and one capture in, N SYNTHETIC states out, in the rendezvous layout (`index.json`, `call.bin`, `mem.bin`) that `frame_trace.py recorded` and `snapdiff.py --bounded` replay. Each `index.json` carries `synthetic.label = "SYNTHETIC"` and every chosen field (spec name, address, type, value, bytes). Seeded draws, no timestamps: the same spec and seed write the same bytes.
  - Static (no run): fields are written into the capture's memory or registers. `as` retargets the state's entry (rv.entry_va) to another function, which is what `synth_call.py` did.
  - Emulated (PES's own bytes via unicorn): `entry` is where the values are written (first arrival), `stop` where the state is captured, `reach` keeps only cases whose run enters that VA afterwards, and `start: return` runs the capture's call to its return first (the forward run of `synth_walk.py`). `--hooks` is required.
  - Addresses: `reg:NAME+off+...` (every offset but the last dereferences), `mem:VA+off+...`, `va:VA`. Registers `rcx`, `edx` (32-bit names zero-extend), `xmm3`.
- **`test_synth.py`** (new), registered as `translate_synth_test` (plain python3, no unicorn).
- **Removed:** `synth_call.py`, `synth_walk.py`. **Migrated:** `synth_members.py` (`build_case` writes a spec and calls `synth.py`; its `State` reads through `synth.read_capture`), `walk_gates.py` (`synth.patch_imports`). `synth_commit.py` and `synth_unit_resets.py` reach the tool through `synth_members.build_case`; only their docstrings were re-pointed. `synth_ctor.py` is untouched (it does not depend on the removed scripts and still works).
- **`tools/dev/README.md`**: section "Synthetic states" with the spec keys and the mapping from the retired flags.

## Folded in, and what is not

- `synth_walk.py` (main): forward run to an entry, `--entry` as `entry` = `stop`, `--hook-name` as `hook`, `--patch-clear` as the field `reg:rcx+0x3EC0` (u32 5). `--in-call` (RestingBall branch) is the default `start: call`. `--bp-ctor` (RestingBall branch) is NOT folded.
- `hdrsyn.py`: emulated spec, `entry` 0x140833CD0, ball and target as chained fields, `reach` 0x140A510F0. Re-run (below).
- `chsyn.py`: expressible (entry 0x1408364D0, `plus` for `edge + pz`, `reach` 0x140A641D0). NOT re-run.
- `rootsyn2.py`: expressible (entry 0x141AFDF10, `reg:rsp+0x28` u32 for the motion, `xmm3` u32 for the seconds bits, `cases` for the rows). NOT re-run.
- `synth_trapkind.py` (RestingBall branch): its plans are explicit cases with hex pokes, so expressible; NOT migrated (branch code).
- `synth_press_order.py` (PressAI branch): NOT folded. It draws values for 24-entry lists with derived stride addresses; needs a repeat/index field.
- `synth_chooser.py` (KickMotion branch): NOT folded. It copies the bytes the builder wrote during its run; needs a copy-written-bytes mode.

## Verification

- **Red first:** `test_synth.py` before the tool existed: `synth.py refused the fixture spec: ... No such file or directory`.
- **Green:** `PASS test_synth`. Checks: two runs of seed 7 give byte-identical `index.json`, `call.bin` and `mem.bin` for all 4 states; seed 8 gives other states; each index says SYNTHETIC; perturbed bytes and registers differ from the fixture; nothing else differs in `mem.bin`, `call.bin` or `index.json`; `as` retargets the entry and an `edx` field zero-extends into `rdx`; an unknown spec key is refused.
- **Mutation (run, rebuilt, reverted):** in `apply_fields`, the write address shifted by +1 byte. `test_synth.py` FAIL: `fixture_0000 mem.bin changed outside the perturbed fields at 0xc`, rc=1. Reverted to the committed content, PASS again.
- **Not run:** a mutation of the seeded RNG (determinism). Not checked by mutation.
- **`synth_members.build_case`** on the fixture (not on PES data): state `case_140002000_t1`, entry 0x140002000, rdx 7 (from `edx`), rcx 0x20000, poke bytes at 0x100, hook `synthetic_140002000`; source `mem.bin` untouched.

## Acceptance: KickBranch wide-pass set re-derived

- Source: `live2/pid802552/seq030` (kick_header capture, builder entry 0x140833CD0, cited in docs/ports/pes-port-KickBranch.md). Unpacked for the run, re-packed after.
- Spec (`accept/widepass.spec.json`, scratch): `seed` 7, `count` 150, `entry` 0x140833CD0, `reach` 0x140A510F0; fields `mem:0x1436F3DC8+0x40+0x88` (ball x), `+0x90` (ball z), `reg:r8+0x8+0x2C` (target x), `reg:r8+0x8+0x34` (target z), each f32 uniform in [-30, 30].
- `synth.py`: 25 kept, 125 dropped as "reach not entered".
- `frame_trace.py recorded` on each of the 25 with `union.cfg` (KickMotion scratch) and `specs/kick` (KickMotion branch, read only), then `snapdiff.py --bounded --only 0x140833CD0` (main's tools).
- **Verdict:** `0x140833CD0 calls=25 replayed=25 mismatches=0 own_block=0.4422 proven=True`, `excluded 0`, `translated True`; own block 264/597. KickBranch's record (docs/ports/pes-port-KickBranch.md): `calls=61 replayed=61 mismatches=0 own_block=0.4422 proven=True`, own block 264/597. Same verdict and the same coverage. The draws differ (hdrsyn did not record its ranges), so the state set is not identical to KickBranch's; the verdict is.
- Found and fixed on the way: the first emulated run dropped all 150 cases "reach not entered". `run_recorded` installs the import and stub models only for a trace run, so emulated specs now run in the trace environment (`bc387359`). A dropped case now prints the run's error to stderr.

## Gate

- Build (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build -j3`): rc 0 after 1892 s (ccache warm).
- `GATE_TIER=fast tools/dev/gate.sh <workdrive>/gf-SynthTool/build` on `bc387359` (sim slots 1 0): **GATE fast: GREEN**. ctest `100% tests passed out of 2322`; recorded re-check 0 of 2253 (core), no unrecorded status regression; ledger current; off-path ports 125 (allow-listed). Gate wall 21,363 s under load 18.
- Smokes (2 min, seed 7): switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`. Both equal the main baselines in the brief.
- Log: `gf-SynthTool-scratch/gate.log`.

## Open items

1. `synth_trapkind.py` and `synth_press_order.py` (RestingBall, PressAI branches) migrate when those branches land; press-order needs the repeat/index field.
2. `synth_chooser.py` (KickMotion) needs a copy-written-bytes mode.
3. `--bp-ctor` (RestingBall branch) is not folded.
4. chsyn and rootsyn2 are expressible as specs but were not re-run.
5. Emulated and static states copy the whole `mem.bin` (about 3.7 GB each for seq030); no overlay format, so the state count is bounded by disk. 25 states were written and removed after the proof.
6. `docs/ports/*` still name `synth_call.py` and `synth_walk.py` in archived reports; not rewritten.
7. The RNG-determinism mutation has not been run.
