# pes-port-ReplayRuntime3: the replay system decoded (recorder, ring, playback, the FixDemo state-7 record create); the 141EB8E80 "spin" is not a PES spin; plan written; slice 1 NOT implemented (no captures of its writers)

Branch `Presentation`, pushed `61268421..2c124031` (`git ls-remote origin Presentation` = `2c12403125d5`, confirmed). Commit `2c124031` adds `docs/re/pes-re-replay-system.md` only (path-limited). No code change this round. Build dir: none (removed in round 2; no build this round). Captures: `live2/pid950952/seq001` repacked after the probes; the other six boot captures were repacked in round 2. Scratch 2.8 MB.

Labels: CONFIRMED = headless decompile (`dec.sh`), an E8/E9 or absolute-pointer scan of `pes_full.bin`, an RTTI name read from the image, capstone disassembly, a replay run. INF = inferred. OPEN = not settled, with the probe named.

## 1. Decode (done; full detail in `docs/re/pes-re-replay-system.md`)

- **Classes (CONFIRMED, RTTI read from the image):** `match::Replay` 0x142BD3E50, `ReplayHuman` 0x142BF7D58, `DataManagerReplay` 0x142654040, `UnitReplay` 0x142653F58, `match::Human` (vtable 0x142612FB8), `match::Player` (0x1426329A8), `mob::PropMain` (0x142614DA0), `game_mode::MatchListener` (0x142BBA650). The per-tick writers are reached from these vtable slots.
- **Recorder, per tick (CONFIRMED callers; tick root OPEN):** `Player` slot 13 (`140927450`) → `Human` update `1408BED00` (creates the ReplayHuman record while `P+0x938` is zero; stores `this+0x94C`) → `1408BF135` (per-player recorder: anime state via vtable `+0x28` of `P+0xAB8`, motion ids, match clock) → `1408BF724` → human writers `140A893F0`, `140A89030`, `140A898D0`. `PropMain` slot 6 (`1408CCBD0`) → `1408D1880` → `140A89E50/EA0/E00` (prop/ball fields). Header writers `140A88E00` (from `140871E15`, `141F83050`), `140A89F20` (from `140423098`), `140A88CE0` (from `1408D0814`).
- **Ring (OBS, consistent with captures):** 21 units; 660 frames × 0x1FB0 bytes; header 0x264 + 25 × 300-byte human records. **OPEN:** the cursor advance (not in any writer's direct-caller set; probable `140A8EE20`/`140A8ED70`).
- **Playback (CONFIRMED mechanics):** `14217DEF0` (translated on this branch, copy suite green on 8 real captures) reads `frame + i*300 + 0x30C` (absent 0x7FFF) and the palette byte `+0x36B`, copies the 0xA0 entry to the actor `+0x50`. Its only creator is `141EB6840(…,1)` in `1423E90E0`, and `141F7F480` (`+0x58`).
- **FixDemo chain (CONFIRMED decompile):** `141EDE9F0` is the per-tick state machine of one demo object (state at `+0x308`; state 1 → `141EE03E0`; states 2–8 → `141EE0240`, `141EDF690`, `141EDFF10`, `141EDFA80`, `141EDF720`, **`141EDF450` (state 7)**, `141EDF5A0`; state 9 inert). State 7 dispatches on `+0x3C0`: mode 1 → `1423E0170` → `1423E9050` → `1423E90E0`, which creates the replay record only for event ids `0x2C..0xCD`.
- **Goal root (OPEN):** `141EDE9F0` has no direct caller and no absolute pointer in the image (its caller is indirect). The step machine `141F59380` is called from `141F590E4..141F592AF`. GF's `goalsequence.cpp` has timing constants only (confirmed on main). The goal-side request with event id `0x2C..0xCD` is not decoded. Probes named in the decode §6.

## 2. Plan (done): `local://replay-system-plan.md`

Seven slices in dependency order, each with acceptance on real captures: S1 recorder writers (translated, proven); S2 ring cursor and unit ops; S3 ReplayHuman playback from recorded frames; S4 state-7 record create; S5 cameras via the existing FixDemo selection (`fixdemo_select`, `cutscene_timeline`, `pes_cutscene_camera`); S6 goal trigger (event id); S7 simslot smoke and gate. Each slice names its blocking probe.

## 3. Item 4: the 141EB8E80 byte-scan spin (resolved as a harness-mode artifact; cause in the trace layer OPEN)

- The routine is PES code at `0x14252E700..0x14252E7A0` (CONFIRMED disassembly): a name matcher. The outer loop walks a NUL-terminated path in the replayed memory (`common\anime\FoxAnim\Face\Animations\Base\angr_brwn…`, a stack buffer). It is not a spin on bad data: the first name terminates at outer iteration 69 (CONFIRMED read of replayed memory). The boot builder `141EB8E80` runs its name loop over about 300 names (tables `0x143527410..0x143527E50`).
- **Without the trace layer the replay returns:** `frame_trace.run_recorded(seq001, budget=4e8, models=load_import_models())` gives `err None` in 4.5 s (CONFIRMED). **With the trace layer** (`frame_trace.py recorded … --hooks … --out …`, which calls `run.install_hooks`) it does not return within 50M instructions and stops in `ucrtbase.dll` code at `0x6F769F9A19AC`.
- Bisecting stub kinds: the same stop with **zero** stubs installed (`kinds []`, `n_stubs 0`), so the divergence is the trace layer, not a `stubs.json` model. Cause OPEN. Next probe: the trace layer without `trace["impure"]`, and without the `_ret_sentinel` hook, to see which makes the import resolve into DLL code.
- Consequence: `141EB8E80` / `1408BE3C0` are NOT proven and NOT translated. Named gap.
- Also added this round: none (the `strlen` model from round 2 stays; its test is green).

## 4. Item 3: slice 1 (the recorder) NOT implemented

- Reason: no capture of any recorder writer exists. The writers (`140A89F20`, `140A88E00`, `140A89E00`, `140A88CE0`, `140A89030`, `140A893F0`, `140A898D0`, `1408BF135`, `1408BF724`, `1408D1880`, `140871E15`) and their per-tick entries (`140927450`, `1408BED00`, `1408CCBD0`) are unhooked. Proving bit-exactness needs those captures (hooks from Main), then closures and proofs. Two of the entries (`1408BF135`, `1408D0814`) have no direct or absolute caller, so their hook needs the caller `1408BED00` (INF) first.
- I did not translate anything unproven (rule: no stand-ins; no synthetic-only proof).

## 5. Hooks needed (the exact lines, pass them through `untranslated_callees.py --hooks` before arming)

Request these from Main as one batch (per rig rules: EVERY ≥ 20 for per-player-per-frame, predicates where known, caps small):
- `140927450` (Player slot 13): EVERY 40, cap 12, full. Its capture gives the player and the per-tick context.
- `1408BED00` (Human update): EVERY 40, cap 12, full.
- `1408CCBD0` (PropMain slot 6): EVERY 20, cap 12, full.
- `140A88E00`, `140A89F20`, `140A89E00`, `140A88CE0` (header writers): FIRST 1 EVERY 20, cap 6 each, full.
- `1423E14D0` (demo request fetch), with `--when` on category 3 (goal): FIRST 1 EVERY 1, cap 6, full. Names the goal event id (decode §6 probe 1).
- `141EDE9F0` with `--when 'd(rcx+0x308)==7'`, EVERY 40, cap 12 (decode §6 probe 2).
- `141F59380` (FixDemo step machine) with `--when 'd(rcx+0x1d8)==5'` is already staged (`fixdemo_step_machine_141F59380`).
The hook lines themselves must come from `untranslated_callees.hooks_for_image`; I have not generated them this round for these entries (only the three boot hooks in round 1).

## 6. Checks

- No code change this round, so no test binary rebuilt. Last green (round 2): `pes_anime_engine_test` 11/1 skip, `pes_native_service_test` 7/7, `pes_replay_human_test` 5/3 skip, `test_strlen_model.py` 4/4 (mutation red). `gen --check` byte-identical at 10193 (round 2). `ledger --check` current (44468).
- The decode doc contains no machine paths (grep: 0).

## 7. Open items (numbered)

1. Unit cursor advance (decode §2, §9.1).
2. Caller of `141EDE9F0` (decode §5, §9.2).
3. Goal-side demo request id (decode §6, §9.3).
4. `1408BF135`, `1408D0814` callers (decode §9.4).
5. Trace-layer divergence on the boot replay (decode §7, §9.5). Blocks the proof harness for every boot-path root.
6. Captures for S1 (section 5 hooks).
7. `141B01500` teardown capture (Main armed; pending).
