# pes-port-ReplayRuntime1: AnimeLayer merged into Presentation; the replay pre-state writers decoded; boot hooks captured; the boot-path translation is NOT done

Branch `Presentation`, pushed `b9bf414e..9cebd346` (`git ls-remote origin Presentation` = `9cebd346c25b...`). Merge commit, no history rewrite. Worktree `<workdrive>/gf-PresReplay2` (detached HEAD at the same SHA). Build dir removed at the end. Scratch `<workdrive>/gf-ReplayRuntime-scratch` (small logs and scripts).

Labels: CONFIRMED = decompile (dec.sh headless), a capstone/E8 scan of the flat image, a generator or test run, or a live capture. INF = inferred. OPEN = not settled.

## 0. Scope reached (read this first)

- Step 1 (decode the pre-frame state and its callers): done to the boot level, plus the per-player level. Not done to a per-goal root.
- Step 2 (translate the setup path, prove it on captures): NOT done. The closure step (`closure.py --spec-set animelayer --tag bootpath 0x141EB8E80 0x1408BE3C0`) hit its 1800 s timeout before it wrote any spec. Nothing was written, nothing was committed for it.
- Step 3 (wire GF's replay trigger to the frame): NOT done. No GF runtime test, no GF match run, no `PES_DISPATCH_UNTRANSLATED` check.
- Main's ruling (merge AnimeLayer, then translate the boot path on top) is followed as far as the merge. Main armed the three boot hooks; the captures are in hand (section 4).

## 1. Merge of origin/AnimeLayer (861f1a7e) into Presentation (CONFIRMED)

- Commit `9cebd346` (merge, `--no-edit`, not rebased). Identity zocram4cc. The `git -c status.submodule=none` check still errors on the `extern/godot-cpp` symlink, but `add`, `commit` and `push` work with explicit paths.
- 38 conflicted paths. Resolved as follows:
  - `CMakeLists.txt`: both source lists kept (`hud_clock*`, `replay_human`, `install_sections` from Presentation; `movestep_turn_entry.cpp` from AnimeLayer).
  - `tools/dev/translate/xlate.py`: both kept (`ImportKeys` from Presentation, `_os_arena` from AnimeLayer).
  - `tools/dev/translate/ro_tables.json`: both sets of rows kept (JSON validated). Every AnimeLayer row is kept; the two Presentation-only rows, `0x142B635D0` (0x100, an INF size) and `0x142B67EF0` (0x2), are added.
  - `src/pes/translated/**`, `docs/re/LEDGER.md`, `tools/dev/ledger/universe.json`: regenerated, not hand-merged.
- Two semantic clashes, not textual, found by the generator:
  - `0x141957C50` (graphics device allocator thunk) is a GF service on AnimeLayer (stubs.json `gf-service`). On Presentation it was a pilot closure member and a stand-in spec `tools/dev/diffharness/specs/presentation/141957C50.json` (status `stand-in`). Removed both (a callee is a model or a spec, never both). Dropped it from `closure_destr14217.json` and `closure_proccam.json`.
- Regeneration: `gen_translated.py --image pes_full.bin` wrote 10192 functions (3824 recorded bit-exact, 6368 closure). `gen_translated.py --check`: byte-identical, rc 0. `ledger.py`: LEDGER.md current (44459 rows); `--check` reports no unrecorded status regression vs HEAD (25826 rows compared, 163 scope moves recorded).
- Pre-push grep of the merge diff for machine paths: 0 added lines.

## 2. Tests on the merged tree (CONFIRMED, four targets built, nothing else)

Built: `pes_replay_human_test`, `pes_native_service_test`, `pes_hud_clock_test`, `pes_anime_engine_test` (ccache, 1553 s; build rc 0).

| test | result |
|---|---|
| pes_replay_human_test | 5 PASS, 3 SKIP (no `GF_PES_COPY_CAPTURES`; the copy suite needs unpacked captures I did not unpack) |
| pes_native_service_test | 7/7 PASS |
| pes_hud_clock_test | 4/4 PASS |
| pes_anime_engine_test | ABORTS: `PES_RO_OUT_OF_WINDOW: read of 8 bytes at offset 416` in `pes_fn_140395FC0` (reads `k_t14257CF50_100`, a 256-byte luaO_log2 window, with displacement `- 0x138`) |

- The anime-engine abort is NOT introduced by the merge (CONFIRMED): `src/pes/translated/closure/pes_fn_140395FC0.cpp` is byte-identical on `origin/AnimeLayer` (`git diff` empty), and its generated read is the same line there. I did not build AnimeLayer's tip to see whether its test is red there too (INF: probably the same state). It is an AnimeLayer-side window-sizing defect (the access is a qword read from a byte table; the offset is a displacement that lands past the window). Owner: AnimeLayer. Not fixed here.
- Mutations: none run (no change of mine to a tested behaviour).
- Not run: `gen --check` after any later change (none), the full gate, GF match smoke (not asked for this step, and Main gates per batch).

## 3. Pre-frame state, decoded (CONFIRMED unless marked)

Flat image `pes_full.bin` (file offset = VA − 0x140000000). Callers found by a flat E8/E9 scan with each site checked against `.pdata` containment; Ghidra bridge (8080) was DOWN; bodies from `dec.sh` (headless).

**0x1437F4B78 (the "key record" the frame's `141B03540`-side lookup reads at `[rdx+0x28]`):** a heap object, not a stack record (this corrects PresReplay16's "stack record" reading, INF until the boot capture shows it; the boot capture seq001 holds the value at the call).
- Built by `141EB8770` (AnimeLayer-translated) and `141EB8E80` (not translated). Each walks two name tables in the image (0x143527410..0x143527940, 0x143527390..0x143527E50 for 8770; 0x143527410..0x143527940 and 0x143527940..0x143527E50 for 8E80) and adds each name that passes `1414A8D60` and `141B021A0` to a new object from `141AFEC30` via `141B00220`. Stores the object into the global only when the global is zero (`_DAT_1437f4b78 == 0`).
- Cleared by `141EB8290` (not translated): calls `1423D4A50`, `1423D4CE0`, zeroes `DAT_1437F4B71/B72`, deletes the object through its vtable +8, and writes 0 to the global. The tool refuses it for a hook (a call inside the displaced window, `0x141EB8294`).
- Callers of the builders: `141EB8770` ← `141ECA375` in `141ECA210` (one-time FixDemo registration, guard `DAT_1437F4F81`; `141ECA210` is reached from the list callback `141EC9AB0`, whose caller is a registration pointer, so no direct E8 caller, INF). `141EB8E80` ← `141FD7946` (in the range of `141FD6F10`) and `141FE4A30` (← `141FE21F3` in `141FE20A0`). `141FD6F10` ← `141FD57D2` (in `141FD5200`); `141FD5200` ← `141FBE30A` (in `141FBE2A0`).

**0x1437CEAA0 (the ResManager; `141B034B0` returns it):** created by `141B02800` (AnimeLayer-translated, ← `141AE7828` in `141AE77B0`, anime engine init). Written by `141B01230` (AnimeLayer-translated, ← `141B0282E` inside `141B02800`) and cleared by `141B01500` (not translated, the destructor; ← `141B0162F` in `141B01620`, not translated).

**ReplayHuman record R (0x418 bytes, the frame's object):** created by `141EB6840` (translated on Presentation only).
- `1408BE3C0` (not translated) creates it at `P+0x938` and a 0x940 companion (per player, `P` = the match player object, vtable 0x142612F98 at P+0x7D0 / P+0x7E0 per `docs/re/pes-re-trap-inputs.md`). It is called from `1408BE740` (not translated; the per-player anime setup, which also allocates a 0x3BC0 anime object through `1406E03D0`) at `1408BE760`, and from `1408BED00` at `1408BED21`.
- `1408BED00` is a vtable slot (the .rdata slot at `0x142613020`, and two more at `0x142641EE0`, `0x142642158`): the per-player update, which calls `1408BE3C0` when `P+0x938` is zero. Its direct callers `140927459` (in `140927450`, which calls `1408BED00` and `140922FF0` on `P+0x1A488`) and `140A38F09` (in `140A38F00`) are the per-frame drivers (INF: match tick).
- `1423E90E0` (not translated; a scene start, `param_2` in 0x2C..0xCD) also creates a record at `+0x28` through `141EB6840(...,1)`. Called from `1423E9050` (← `1423E0298` in `1423E0170`, not translated) and `1423E9384`.
- `141F7F480` (not translated) creates one at `+0x58` (`141EB6840(...,2)`), called from `141F7F4D5`.

**Per-goal root:** not found. The chain reaches the anime world tick (`141FBE2A0`, `141FD1690`, `141FD1B90`, `141FE20A0`, `141FD5200`) and the per-frame player drivers; the goal-specific trigger is not reached (OPEN).

## 4. Boot captures (CONFIRMED: live, Main's rig)

Armed by Main on PES pid 950952 (folder `<workdrive>/gf-Rendezvous-scratch/live2/pid950952/`, full, not packed):
- `seq001` = `boot_141EB8E80` call 1 (project object builder). Mapping list and `call.bin` present.
- `seq002, 004, 006, 008, 010, 012` = `boot_1408BE3C0` calls 1..6 (ReplayHuman record create).
- `boot_141B01500` (ResManager destructor): no capture yet (runs at teardown).
- `141EB8290`: refused by the hook tool (the displaced window holds a call at `0x141EB8294`); not hooked.
- Main's note: the rendezvous DLL installs after PES's earliest boot, so pre-DLL calls are not catchable.

Hook lines (the tool's output, unmodified):
```
hook boot_1408BE3C0 0x1408be3c0 18 1 1 488bc455574156488d68a14881ecf0000000
hook boot_141B01500 0x141b01500 15 1 1 41564883ec3048c7442420feffffff
hook boot_141EB8E80 0x141eb8e80 19 1 1 488bc45541564157488d68a14881ecd0000000
```

## 5. Closure size (CONFIRMED)

`closure.py --dry-run 0x141EB8E80 0x1408BE3C0` (spec set `animelayer`, tag `bootpath`): 1341 members from 2 roots; 5 universe rows would be added; 1336 are already in the universe (AnimeLayer's). The full closure run exceeded 1800 s and wrote nothing.

## 6. What remains (numbered; the plan)

1. Speed up or split `closure.py` for `bootpath` (it runs specgen per member; run it per root, or with `--roots-file`, and detach it with a longer timeout). Its output must be `tools/dev/diffharness/specs/animelayer/*.json` plus `closure-bootpath.txt`.
2. Translate `141EB8E80` (and `1408BE3C0` only if its proof needs it: its closure is 1341 members, mostly already translated) through `gen_translated.py`.
3. Prove `141EB8E80` over `live2/pid950952/seq001` (`prove_bounded.py 0x141EB8E80 WORK --tag bootpath --real <unpacked seq001> --merge`, with `RV_HOOKS=<workdrive>/PES21/rendezvous/hooks.cfg`), and `1408BE3C0` over seq002..012 (six real calls). Unpack first with `capz.py unpack` (they are full now, so `snapdiff` can read them directly; check the replay reads).
4. Translate `141EB8290` and `141B01500` only after a capture (or a synthetic state, `synth.py`, labelled) proves them; `141EB8290` needs a hook the tool accepts (its window has a call), which is a hook-tool question for Main.
5. GF side: a GF runtime test that builds the state from a GF match (not from a capture) and runs `ReplayHumanFrame::Read` without an abort: needs items 2-4 and the GF `pesworldrecord` feed. Not started.
6. Wiring: GF's replay trigger is `goalsequence.cpp` / `Match` timing only (ReplayFiresAt_ms, ReplayStartOffset_ms); it has no PES replay consumer on this branch. The consumer of a replay-human palette entry does not exist in GF yet: the FixDemo presentation (`fixdemo_select`, `cutscene_timeline`, `pes_cutscene_camera`) is owned by CutsceneTiming/CutsceneSelect and is NOT on this branch. OPEN: which consumer Main wants the frame's output to drive (do not invent one).
7. Acceptance (a GF smoke that reaches a goal and runs the frame with no `PES_DISPATCH_UNTRANSLATED` abort): not attempted.

## 7. Open items and named gaps

- GAP: the pre-frame writers `141EB8E80`, `141EB8290`, `141B01500` are untranslated; the frame's pre-state is therefore not produced by translated code yet (OPEN).
- GAP: the per-goal root of the replay record is not decoded (OPEN; the chain above stops at the per-frame match tick).
- GAP: `pes_anime_engine_test` aborts in `pes_fn_140395FC0` (identical on AnimeLayer; AnimeLayer owns it).
- GAP: copy tests skip without `GF_PES_COPY_CAPTURES`; not run with captures in this step.
- GAP: `141EB8290` not hookable by the tool (displaced window has a call).
- GAP: the closure job for `bootpath` timed out (section 5, item 1).

## 8. Budget

About 140 requests used of ~200. Stopped here because step 2 (proof + gen) and step 3 (GF wiring + test + consumer) were not reachable in the remaining budget without the closure fix in item 1 and a consumer decision from Main.
