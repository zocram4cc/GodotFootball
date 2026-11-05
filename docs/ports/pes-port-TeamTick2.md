# TeamTick2: survey of the team-tick cutover (one untranslated target), the keeper GKDribble unit translated and proven, the teampass proofs re-run on 38 real captures, and the pass-target selector still naming no target (05-11-26)

Branch `TeamTick` (pushed `17ebc0c0`; `0e0ad2b8` is the translation checkpoint), merged with origin/main, origin/SurveyTool (`GF_PES_SURVEY`) and origin/SynthTool (`synth.py`). Image `pes_full.bin`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. **Acceptance is NOT met**: `pes_pass_target_test` is red (no pass target is ever named), `GATE fast` / branch gate not run, drift census not run (section 7).

## 1. Survey (rule 1): seed 7, 2 min, pes-loop, `GF_PES_SURVEY` (CONFIRMED)
`survey7.txt` (before the translation): exactly ONE untranslated dispatch target:

| target | hits | first frame | caller | verdict |
|---|---|---|---|---|
| `0x142132E20` (keeper GKDribble, kind 0x13) | 1187 | 4816 | `pes_fn_14211EEC0` (the unit evaluate wrapper), reached from `RunDribbleProposal` (`pesworldrecord.cpp`) -> `0x1421102D0(kind)` where `kind = 0x13` when `0x140A638C0(ctx, slot)` holds (the carrier is his side's keeper) | pre-existing GF code path (the dribble proposal's keeper arm), newly REACHED: main's smoke never hits it (it finishes); here a keeper owns the ball after the new pass targets / staging. Not a consequence of one specific call: see 4 |

Its only other untranslated callee is `0x1421334D0` (32-direction table builder, called from the unit; hook_plan refuses it: rip-relative `movsd` at +4), proven inside the unit's closure. The static lister (`untranslated_callees.py`) sees direct `pes_fn_` calls only (0 from `0x14211EEC0`: the unit is a vtable target); the other keeper units (kinds 18, 20, 21, 22, 23 at `0x142BEEFE0..`) are NOT reached because GF's walk list holds outfield kinds only (A4Carrier §3). After the translation a second survey run lists **0 targets** (`survey7b.txt`, exit 79 by design, determinism `ecbe8407d1ee4ef6`).

Hook need (sent to Main in one message, live as `tt_142132E20`, EVERY 5, cap 24, launch `pid615595`): `0x142132E20`. Nothing else.

## 2. Translation and proof of the keeper unit (CONFIRMED; SYNTHETIC cases labelled)
- `0x142132E20` + `0x1421334D0` translated by the closure tool (set `gkdribble`, specs `tools/dev/diffharness/specs/gkdribble`, 27 closure members, 25 already in the layer; the four heap natives are excluded from the committed closure: `gen_dispatch` refuses a VA that is both translated and native).
- Proof `tools/dev/translate/proof_gkdribble.py` (snapdiff `--bounded`; return value and every written byte, PES bytes in the harness vs the translated closure over the same pages): **9 of 9 bit-exact, ALL SYNTHETIC**, own-block coverage 34/39 (0.8718, best seed). No real capture of the unit exists yet. States: `synth.py` runs PES's bytes forward from the danger_builder capture `live2/pid1792161/seq16184` to the proposal writer `0x1421102D0`, sets the kind to 0x13 and arms the kind object (BP+0x3B30+0x38), draws 8 scalar inputs + 44 table rows (ranges in `FIELDS`), keeps cases that open the unit's time gate (`reach 0x1421334D0`: 9 of 32 draws). A state is 2 GiB, so each seed is replayed and deleted. Replace with the real calls when `tt_142132E20` captures land (`--real CAPTURE,...`, same script).
- Layer regenerated (`gen_translated.py`): +`gkdribble/pes_fn_142132E20.cpp`, `closure/pes_fn_1421334D0.cpp`; **`gen_translated.py --check`: 5269 functions regenerate byte-identical** (rc 0); `ledger.py --check` GREEN after `ledger.py` regenerated `LEDGER.md` (38118 rows).
- The translation changes nothing on seed 7: survey-with-skip and translated both hash `ecbe8407d1ee4ef6` (the 1187 evaluations never write an answer there: INF, all early returns).

## 3. teampass re-proof on every landed capture (CONFIRMED, REAL captures, 0 synthetic)
`proof_teampass.py --batch 4` (new: unpacks, replays and packs 4 captures at a time; coverage of the sum is the best batch's). Hooks `team_pass_*`, `live2/pid281026`:

| root | calls | bit-exact | own-block coverage (was) |
|---|---|---|---|
| `0x1404FB380` | 24 | 24/24 | 45/67 = 0.6716 (0.7015 on 6) |
| `0x1404F0E50` | 3 | 3/3 | 346/1000 = 0.346 (0.285) |
| `0x1404FB780` | 11 | 11/11 | 5/68 = 0.0735 (unchanged: every capture is the pre-kick-off phase 2; the other arms need captures of phase 2 states 2/3 in play) |
| `0x1404FB2A0` (impure set, not re-recorded) | 12 landed | 4/4 in the oracle test | |

All captures used are listed by the hook name in `index.json` (every `team_pass_*` seq in `pid281026`); Main protected them.

## 4. The smoke and its attribution (CONFIRMED runs; the attribution is a finding, not a closure)
`gfsim_run --seed 7 --minutes 2 --pes-loop`, no abort, `[idle] 0`, 31742 ticks:

| tree | determinism | note |
|---|---|---|
| origin/main | `85c7d7c515cf92ae` | baseline |
| TeamTick (selector call + `kickMan` read) | `ecbe8407d1ee4ef6` | finishes, no `PES_DISPATCH_UNTRANSLATED` |
| ablation A: `RunPesPassTarget()` call removed | `9be506cdd5aa43aa` | 32801 ticks |
| ablation B: `kickMan` read back to the newest-touch stand-in | `d0755e79c5a21372` | 43017 ticks, `[idle] 2 episodes, stalls 2` |

Every move is chaotic (any change in the press / staging chain reshuffles the trajectory): neither ablation returns to main's hash. **Reading (CONFIRMED by the probe in 5): the selector chain never named a pass target in the whole match**, so the only live effects of the cutover are its side effects: the staged records (MI+0x1748 on-pitch bytes, man+0x5D0 words, B sample 0, MA owner, T+0x255) and the `kickMan` read now seeing 0xFF instead of the newest toucher. The GF kick no longer writes a receiver on the PES path, so `Team::PassGetReceiver()` returns null for every pass: **a regression of the receiver's run** that the smoke does not measure (INF).

## 5. Why the selector names no target (CONFIRMED to the last input; OPEN beyond)
Probe (scratch, reverted): TTDBG per team tick and TTDBG2 inside `RunPassTarget` calling the translated helpers on the window.
1. FIXED (committed `17ebc0c0`, unmeasured on the smoke): `0x1404F0E50`'s first arm (a target right after a kick) reads the **second key of the newest kick event**: `FUN_140A3E900` returns payload+0x38 for kinds 1,2,3,0x10 (`0x140A3E900`), written by `FUN_140A18940` cases 1-3 as `FUN_140A62FE0(roster, ball+0x27E4)` with the roster index `FUN_14043B870` at payload+0x4C (`0x140A17E10` is only the tail-struct reset: invalid key, +0x14 = 0x18). GF wrote no such key (`pesevents.cpp` ponytail "FUN_140a17e10/FUN_14043b870 not decoded"). Real capture seq732: payload words `{1,0,6,0x18,..., 0x16, 9, 0, 9, ...}`: ring-0 key at +0x38 = {team 0, slot 9}, and T+0x78 = 9. Now the kick writes the ball record's target word on the PES path (`Match::SetPesKickTarget`, humanoid.cpp; the touch event carries it). Test: `pes_events_test` kind-1 case (valid key, invalid key when none, a shot writes none) 39/39; mutation (never write the key) killed.
2. OPEN, the next blocker: `FUN_140A51320(X)` (the frames-since value that `0x1404F50F0` requires to be exactly 1 and `0x1404FB380` clears on when <= 0) returns **0 in every GF call**, with the 54 Hz cells present. Its decompile: `uVar3 = FUN_140A62EA0(X, key of FUN_140A3E7D0(E)); if (uVar3 < 0x16) return FUN_140A3E840(ring, n) (= the n-th ring record's payload+0 unless its kind is 0x37) else MI+0x1198 - newest.frame`. The pass-1 reading in `pes-port-TeamTick.md` section 1 ("frames since the newest ball event") is the second arm only; with a valid newest actor the first arm runs and GF's ring answers 0. Next step: read the asm at `0x140A51320..` for the exact arguments of the `FUN_140A3E840` call (the decompile drops them) and compare against the captures' ring (seq732: ESI 27), then fix whichever of {ring index, record kind 0x37 fill, actor key} GF's store gets wrong. Probe lines: `owner=255 busy=0 evTeam=<kick team> key=<target> id=<target> esi=0 chk=0 onpitch=1`.
3. Then the unmeasured rest of the staging (section 4 of `pes-port-TeamTick.md`): the owner, `man+0x5D0`, the slot records' +0x1430 counters, class bytes, roster.

## 6. Tests (CONFIRMED runs)
| test | result |
|---|---|
| `pes_teampass_oracle_test` (4 unpacked captures) | PASSED |
| `pes_holder_latch_test` | PASSED (2 tests) |
| `pes_mentality_test` | PASSED (47 tests) |
| `pes_events_test` | PASSED 39/39 (+ the kind-1 second-key case; mutation run and reverted: killed) |
| `pes_pass_target_test` (seed 7, 6 min) | **FAILED**: `targetTicks == 0`, "the selector chain never named a pass target" (the stale-target bound is vacuous until a target is ever set) |
| `gen_translated.py --check`, `ledger.py --check` | GREEN |
Not run: the branch/fast gate (a tree whose own acceptance test is red does not need the 5 h run), the census.

## 7. Report items / resume
1. **Next step**: section 5.2 (`FUN_140A51320` ESI = 0), then re-run `pes_pass_target_test`, the smoke, the two ablations, and ONLY THEN the branch gate. Do not land `TeamTick` before: on the PES path `PassGetReceiver()` is null for every pass.
2. `tt_142132E20` real captures: when they land, `proof_gkdribble.py WORK --real <seq dirs> --merge` (add to the 9 synthetic cases, keep the label for those).
3. Drifted seeds: not censused (merge lane). The survey/gkdribble layer add and the event key change touch seeded matches: the throw-in seed families (PesRestartDelay / ThrowInRelease / Forfeit / Stall) will need the merge-lane census.
4. `GATE fast`: not run. `GATE branch`: not run.
5. Docs: `pes-re-lane-leaves.md` section 4 corrected (phase map: phase 1 runs `0x1404FB380` unless both skip-object gates hold; clear predicate `owner <= 0x15`, assembly `0x1404FB3E7/0x1404FB3EA`) and `pes-re-teamtick.md` 4.2 (writer CONFIRMED, chain and clear predicate).
6. Captures used (protected by Main): all `team_pass_*` in `live2/pid281026`, `live2/pid1792161/seq16184` (synth source), future `tt_142132E20` in `live2/pid615595`.
