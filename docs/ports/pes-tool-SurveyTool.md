# PES tool: SurveyTool: survey mode (GF_PES_SURVEY), static lister (untranslated_callees.py), hook generator (--hooks)

Branch `SurveyTool` (worktree `<workdrive>/gf-SurveyTool`, base `origin/main` 784ffd5d). Pushed; `git ls-remote origin SurveyTool` confirmed `51cc4bed`.
Commits: `259eb0b3` (survey runtime, dispatch hook, lister, both tests, CMake registration), `13b099af` (a survey run now goes on past every target), `51cc4bed` (tools/dev/README.md section).
CONFIRMED = a tool run in this session. INF = inference. OPEN = not done, with the blocker.

## 1. What landed

### 1a. Survey mode, `GF_PES_SURVEY=<file>` (off by default)
- `src/pes/translated/pes_survey.hpp` (new, header-only). The generator `tools/dev/translate/gen_dispatch.py` writes the committed `src/pes/translated/pes_dispatch_gen.cpp` (the one TU naming every function): a direct miss (`pes_dispatch_missing`) and the table's default case (`pes_survey_absent`) consult it. `pes_dispatch.hpp`: `pes_dispatch_missing` loses `[[noreturn]]` (it returns in a survey run). `src/sim/onthepitch/match.cpp` stores the logic frame each tick (`pes_survey_set_frame(GetSimFrame())`, a store the sim never reads). `driver.py` and `snapdiff.py` copy the header into the diff builds.
- **Unset:** a direct miss runs the recovery (`pes_dispatch_missing_handler`), if one is installed, then aborts with `PES_DISPATCH_UNTRANSLATED`. An indirect miss gets no entry, so the caller aborts, as before.
- **Set:** every hit is counted. The recovery runs first when one is installed: it is GF's pre-cutover path for that call and may not return (the FixDemo rig longjmps; a GF cutover throws). When it returns, or none is installed, the call has no effect and the run goes on. The list is rewritten at each new target and at exit, one line per target: `target hits first_frame caller_offset`. `first_frame` is the logic frame of the first hit. `caller_offset` is the return address into the calling function minus the executable's load bias: `nm -C -n build/apps/gfsim_run` names the function. The process exits **79** (`kPesSurveyExitCode`) with or without hits, so a survey run never passes as a smoke.

### 1b. Static lister `tools/dev/translate/untranslated_callees.py [--layer DIR] VA...`
- Walks the calls from each root through the layer (`pes_fn_<VA>(s)` calls) and lists every `/*OUT-OF-SET CALL 0x…*/` marker reached, with call sites `callerVA:line`. Default layer `src/pes/translated`. Exit 2 if a root is not translated in that layer.
- A marker whose callee is also in the layer still aborts at its call site (the caller's build did not link it). The output says so.

### 1c. Hook generator `--hooks` (same tool)
- `PES_EXE=<flat image> untranslated_callees.py --hooks [--first N] [--every N] [--prefix NAME] VA...` prints `hook_plan.py` TARGETS rows (`("callee_<VA>", 0x<VA>, "note"),`) and `hooks.cfg` lines `hook NAME VA LEN FIRST EVERY BYTES`. Defaults FIRST 1, EVERY 20.
- Prologue check = hook_plan's: the target must be a `.pdata` function entry and `rv_hooks.displaced_window` must cover 14 bytes with whole, branch-free, rip-free instructions. Refusals print `# REFUSE 0x<VA>: reason`; exit 1 if any is refused. Nothing was written to `hooks.cfg`; Main arms.

### 1d. Docs
`tools/dev/README.md`, section "Untranslated callees: survey, lister, hooks", with the rule: **survey first, then translate the whole list in one batch.**

## 2. Verified

- **Tests (CONFIRMED):** `translate_pes_survey_test` 5/5 (probe built from the generated dispatch TU and `pes_survey.hpp`: unset aborts; unset+recovery; set lists every hit and runs on; set+recovery counts every hit; a survey run with no hit still exits 79). `translate_untranslated_callees_test` 5/5 (reach, call sites, cycle, unknown root, hook refusals). `translate_sse_forms` (`test_sse_forms.py`, the dispatch stub's signature changed) 4/4 with the harness Python.
- **Mutations (each run, rebuilt or regenerated, reverted; `git diff HEAD` on the touched dirs empty afterwards):**
  - M1: `kPesSurveyExitCode` 79 → 0: 3 survey tests fail (`set_plain`, `set_recover`, `no_hit`).
  - M2: lister walk removed (`todo.extend(callees)`): `reach` test fails.
  - M3: survey miss aborts instead of continuing (`gen_dispatch.py`): `set_plain` fails.
- **Gate (CONFIRMED):** `GATE_TIER=fast tools/dev/gate.sh <workdrive>/gf-SurveyTool/build` → **`GATE fast: GREEN`**. `off-path ports: 125 (all allow-listed)`, `LEDGER.md current (38116 rows)`. ctest **2323/2323 passed**, including `translate_pes_survey_test` (1.40 s) and `translate_untranslated_callees_test` (0.14 s). Gate wall 32,951 s (machine load).
- **Smokes (CONFIRMED, seed 7, 2 min, from the gate):** switch-off `[determinism] 399e180f39459c2b`; `--pes-loop` `[determinism] 85c7d7c515cf92ae`. **Both identical to main's baselines.**
- **Survey in a real match (CONFIRMED):** `GF_PES_SURVEY=… tools/dev/simslot.sh timeout 1500 gfsim_run --pes-loop --seed 7 --minutes 2` → **exit 79**, `[determinism] 85c7d7c515cf92ae` (armed survey, no hit: the sim is unchanged), list header only: **zero untranslated targets reached** in that match.

## 3. Chains

**Trap chain** (main layer; roots `0x14079A470`, `0x14079E5C0`; 294 translated functions walked). Three untranslated callees (CONFIRMED):

| target | call sites (function:line) | hook row | note |
|---|---|---|---|
| `0x14079D4B0` | `0x14079E5C0:150`, `:208` | PASS | translated in the layer, yet these markers still abort: the caller's build did not link it |
| `0x14079F560` | `0x14079F440:120` | PASS | |
| `0x14081D930` | `0x1406EB440:111`, `:118` | PASS | |

All three PASS the prologue check; hooks.cfg lines are in `<workdrive>/gf-SurveyTool-scratch/trap_hooks.txt`.

**Kick chain** (branch `KickMotion`'s layer, extracted with `git archive origin/KickMotion src/pes/translated` into `<workdrive>/gf-SurveyTool-scratch/kick`; roots `0x140833CD0`, `0x1408364D0`; 287 functions walked). **16 untranslated callees** (CONFIRMED). Eleven hook lines; five REFUSE:

| target | call sites (function:line) | hook |
|---|---|---|
| `0x1405B02F0` | `0x140721800:1017` | PASS |
| `0x1406DC770` | `0x1406DA470:94` | PASS |
| `0x1406DCD20` | `0x1406DA950:102` | PASS |
| `0x1406DD0B0` | `0x1406DA580:76` | PASS |
| `0x1406E4230` | `0x140833CD0:418` | REFUSE: not in .pdata |
| `0x1406E99B0` | `0x140829E80:2958` | PASS |
| `0x1407247D0` | `0x140829E80:1224` | REFUSE: branch at 0x1407247dc (ja) |
| `0x1408212D0` | `0x1408364D0:1136` | REFUSE: not in .pdata |
| `0x140821660` | `0x140837670:302` | PASS |
| `0x14082DD20` | `0x1408364D0:817` | PASS |
| `0x140A6CFD0` | `0x140721800:1188` | PASS |
| `0x1415D1980` | `0x1403DB2C0:64` | REFUSE: not in .pdata |
| `0x1423B9470` | `0x1423A5BC0:118` | PASS |
| `0x1423B94C0` | `0x1423A5BC0:424` | PASS |
| `0x1423BB890` | `0x1423A5750:166`, `0x1423A5BC0:342,351,644,653` | REFUSE: rip-relative at 0x1423bb89d |
| `0x1423BC840` | `0x1423A5750:212`, `0x1423A5BC0:256…589` (9 sites) | PASS |

INF: KickBranch2's report named four gaps (`0x1406E4230`, `0x14083486C`, `0x14082DD20`, `0x1408212D0`). The lister finds `0x1406E4230`, `0x14082DD20`, `0x1408212D0` as direct callees. `0x14083486C` has no marker in the KickMotion layer (it is a call site inside `0x140833CD0`, of a translated callee), so it is not a callee. The other 13 rows are reached through translated callees of the two roots.

## 4. Open items (numbered)

1. **Survey run on the kick chain is NOT done.** The acceptance asks the survey to work there. The kick cutover exists only on `KickMotion`, not main. A survey run needs a KickMotion build with this branch's survey files applied and `pes_dispatch_gen.cpp` regenerated from that layer's VA list (`gen_dispatch.py --native --vas …`), then `GF_PES_SURVEY=… gfsim_run --pes-loop --seed 7 --minutes 2`. Not run: it is a second full build, past this run's budget. The lister and hook generator are run on the kick chain (above); the survey's runtime path is tested by the probe and by a main pes-loop match.
2. **The lister ignores `OUT-OF-RANGE JMP` / `OUT-OF-RANGE JCC` markers** (tail jumps into untranslated code). They abort through the same `pes_dispatch_missing`, so a survey catches them at run time. Checked: 7 such markers per layer; **none is reachable from the kick or the trap chain** (walk intersection = 0). Add them to the lister before a batch on other chains.
3. **A survey run is a lower bound per run.** A call with no recovery has no effect, so later hits depend on that. One seed-7 match with zero hits is all that was surveyed. Survey several seeds and the families before the batch.
4. **`0x14079D4B0` in `pes_fn_14079E5C0`** (lines 150 and 208) is translated yet its marker still aborts. The lister flags it, does not fix it. Main decides: regenerate the layer so the call links, or leave it (it aborts if reached; no smoke reaches it).
5. **`hooks.cfg` is not edited.** Main pastes the TARGETS rows and hooks.cfg lines it wants armed (CONFIRMED: none was written).
6. `pes_dispatch_missing` lost `[[noreturn]]`; the smokes are identical, so no behaviour change was observed (CONFIRMED). The generated dispatch TU is the only TU that recompiles with it.
7. Worktree note for Main: `git status` in a worktree fails with `expected submodule path 'extern/godot-cpp' not to be a symbolic link` (the godot-cpp symlink). Path-limited `git diff`, `add`, `commit`, `push` work.

Ops: release build `<workdrive>/gf-SurveyTool/build`; scratch `<workdrive>/gf-SurveyTool-scratch` (kick layer extraction, probe, logs, `trap_hooks.txt`, `survey_pesloop.log`). Main removes the worktree and scratch once this lands.
