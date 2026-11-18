# CutsceneLine round 3: slot 0x142530660 is ucrtbase!strcpy_s (not a swapped pointer); the else-arm recompute replays bit-exact with two translator fixes that are NOT yet landable; D/F and the kind-2 anchor case stay open

Branch `CutsceneLine` (worktree `<workdrive>/gf-CutsceneLine`). Pushed head `12c3c64a363c0fc8ef08f1ca3597408574279d53` (`git ls-remote origin CutsceneLine`). Round 3 adds no commit: the translator work is a patch (section 4), because it regenerates 51 recorded functions that need re-proof. Labels: CONFIRMED = decompile, capture read, test or run this round; INF = inference; OPEN = blocker named.

## 0. Answers

1. **The slot and the "halves swap".** My round-2 slot address was wrong. The `call qword ptr [rip+0x1c68fe2]` at 0x1408C7678 is 6 bytes, so the next IP is 0x1408C767E and the slot is **0x142530660**, not 0x142530664 (CONFIRMED by capstone). The call is a plain `call [slot]`, no rol/ror or two-dword load. The slot is not swapped: image 0x6FFFFEC77E00 (a stale loader value), capture 0x6F6C94A67E00 (live). **Resolved live address: `ucrtbase.dll!strcpy_s`** (CONFIRMED: `export_of` on seq203 gives an exact export match). The slot is already in `tools/dev/translate/import_slots.json` as `ucrtbase.dll!strcpy_s`.
2. **Classification: modelled (owner's ruling).** strcpy_s is a CRT string copy with errno returns. It checks no integrity, licence or environment. Its code_hex (import_stubs.json) decodes to: `rcx=dst, rdx=size, r8=src`; dst or size zero → EINVAL, no write; src zero → `*dst=0`, EINVAL; copy through NUL → 0; size exhausted → `*dst=0`, ERANGE (0x22). This matches its `why` text.
3. **Why the translation trapped.** The translator's named-slot path needs a translator mirror (a `"do"` key), and strcpy_s had none, so the call fell through to dispatch on the baked image value → `PES_DISPATCH_UNTRANSLATED: 0x6ffffec77e00`. Patch in §4 adds `"do": "strcpy_s"` and the mirror.
4. **The recompute replay (kind 2, seq203; CONFIRMED).** With the strcpy mirror, snapdiff runs the full call: one mismatch class, **5 bytes** in a bitmap at 0x277a0010..13. PES writes it in `0x1403AE2D0` (`btc eax, edx` on a dword, bit index from `edx` after `imul` by a table value). The translated read `[rax + r8*4 - 0xc]` (table base `0x142581380`, after `cmp r8, 0x1f; ja`) got the wrong bit (bit 0 instead of 15).

## 1. Root cause in the translator (CONFIRMED by the generated code and a register trace)

- The PES register trace for this read: `r8 = 7`, table value `0x4924a`, `edx` 0x69 → bit 15. The translated read is correct in address (`0x142581380 + 7*4 - 0xc = 0x142581390`).
- `bake_ro` (the guard path) does not fire: `bounded_index` takes a guard only on a register a `MOV` copied from. Here the guard is `cmp r8, 0x1f; ja` on the index register itself, after its last write (`shr r8, cl`).
- `bake_window` (the declared-table hull) then bakes the declared table `ro_tables.json` `0x14257F780` (0x1C00 bytes, another function's table). It checks only the start address (0x142581374 is inside), not the index span (start+0x80 is outside). `ro_rd` returns **0** for an out-of-window read, so the value silently becomes 0.

## 2. What the fix does (CONFIRMED by snapdiff, not yet landed)

Fallback `_bounded_by_self` in `tools/dev/translate/xlate.py` (`bounded_index` = copy path, else self-guard path). With it, `snapdiff --only 0x140471A80` on seq203: **calls 1, replayed 1, mismatches 0, proven = True**, own-block 54/138 (not gated). Result file: `gf-CutsceneLine-scratch/round3/results_snapdiff_recompute.json`, also `recompute/snapdiff4/results.json`.

But it changes the generated text of **51 recorded (proven) functions** and holds out 3 more (`gen_translated.py` log: 0x1403BB670, 0x1405549C0, 0x14202BA30, each reaching a "mistranslated callee"). Those re-proofs are not in this round's budget, so nothing of it is committed.

Next fix (not written): in `bake_window`, when the access is guard-bounded, accept the hull only if it covers `start .. start + count*scale + n`; otherwise leave the read to a self-guard bake. This keeps every function that baked correctly unchanged. Then regenerate, list the changed recorded functions, and re-prove each.

## 3. Tests

Nothing committed this round, so no branch test was re-run. The round-2 tests stand (camera list 5/5, goal anchor 20/20, fixdemo select 6/6, camera 17/17, goal scene hold 3/3, timeline 7 + 1 skip; Python SnapMem tests OK).

## 4. Patches (not on the branch)

- `gf-CutsceneLine-scratch/round3/translator-and-record.patch` (150 lines): `xlate.py` (the switch-bound register-compare fix in the jump-table prepass; the `strcpy_s` mirror in `import_model_lines`; `_bounded_by_self` and `bounded_index`), `import_stubs.json` (`"do": "strcpy_s"`), `results_core.json` (the 0x140471A80 snapdiff record, `proof: snapdiff`). Do not land the record without the translator change: the record is only true with it.
- `gf-CutsceneLine-scratch/round3/results_snapdiff_recompute.json` (the proof).
- Known duplicate: `import_model_lines` has a second `memset` branch (about line 579 before this round's edit), unreachable because the first one returns. Remove it in the same change that lands the mirror.

## 5. Open items

1. Land the `bake_window` coverage fix; regenerate; re-prove the changed recorded functions (51, list from the regeneration) and the 3 held-out ones.
2. Land the strcpy mirror and the recompute record together (§4).
3. **Kind-2 anchor case** (`pes_goal_anchor_test`): GF runs only the builder (BC=8). PES's T and radius for BC=2 come from the else arm, which is now replayable. Add the GF else arm (per-tick recompute, the same translated function) before the case can pass; the calc inputs for the case are seq204's entry state (radius 0.0, pos (-41.55, 0, 34.0), rotY 0.0, scene 0x300D9).
4. **D/F (kinds 0/1):** Main has armed `recompute_471A80` on `d(rcx+0xbc)<=1`; no capture yet.
5. The seq203 capture is repacked by auto_slim within minutes. The proof used the scratch copy (`gf-CutsceneLine-scratch/recompute/seq203`, ~4 GB): delete after the record lands.
6. Smokes: not re-run this round (no branch code change).

## 6. Facts for Main

- `strcpy_s` (slot 0x142530660, ucrtbase) is modelled by owner rule; the translator needs its mirror (§4).
- Sider patches in captured code: fixed in `full_snapshot.py` (round 2, `12c3c64a`).
