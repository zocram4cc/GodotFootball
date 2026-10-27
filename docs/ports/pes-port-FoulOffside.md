# PES port FoulOffside: three stoppage functions proven bit-exact; the GF switch-over is not done

Branch `FoulOffside` (origin), commit `c428d636533a243018b611cbe0809ef9482048ac` (`git ls-remote origin FoulOffside` confirmed). Base: `origin/NoFloor` f2efe4c3, merged with `origin/main` (39aa0110). Worktree `<workdrive>/gf-FoulOffside`, scratch `<workdrive>/gf-FoulOffside-scratch`. Labels: CONFIRMED = read from image bytes, a driver run or a recorded result this pass; INF = inference; OPEN = blocker named.

## 1. Result

- **Proven bit-exact (CONFIRMED, driver on synthetic cases; `driver --check` 3/3):**
  - `0x14047F170` sanction decision tree: 128/128. Own-block coverage 0.07.
  - `0x140A3A690` involvement (foul / offside): 128/128. Coverage 0.33.
  - `0x140A3A500`, called by 140A3A690: 101/101. Coverage 0.54.
  - Provisional: live-state globals are at their on-disk values (the driver's wording).
- **Proven but owned elsewhere:** `0x141E53520` (label helper): 64/64, coverage 1.00. Recorded in `results_presentation.json`; this set keeps its spec as callee universe and does not record it.
- **Translated, not proven (OPEN):** `0x140A38F20` (linesman look-at), `0x140A39A80` (restart command), `0x140A3A870` (one-signal latch), `0x140A3A930` (approach / JUDGE). Corpus empty: every case reads a null-based record field (`unmodelled read at 0x70`, `0x20`). Needs real-call seeds from a live stoppage.
- **Not translated (OPEN):** `0x14044C880` runner and `0x140445E90` driver: indirect call `qword ptr [rax+0x20]` at `0x141B4B596` (callee `0x141B4B560`) is unmodellable. `0x140480910` selector: callee `0x141F500D0` stores to a global (`qword ptr [rip+0x18A5C33]` at `0x141F50116`).
- **GF switch-over: not done.** Section 5 sizes what it needs. GF's code is unchanged, so both repros equal the baseline (section 7).
- **Frames: none.** Nothing in GF calls a translated stoppage function, so a frame would show GF's current behaviour, not the target.
- **Committed:** own set `results_stoppage.json`; specs `tools/dev/diffharness/specs/stoppage/` (11); `specgen/sets/stoppage.txt` and `stoppage_rows.json`; the `gen_translated.py` SETS entry; the three recorded files in `src/pes/translated/impure_sample/`; 25 closure members; additive lines in `translated.hpp` and `pes_dispatch_gen.hpp`.

## 2. Method

- Specs: `specgen generate --mode impure --pilot-set specgen/sets/pilot.txt` over the 11 VAs: the 9 assigned, plus `141E53520` and `140A3A500`, which the driver needed as callees.
- Universe gap: the Ghidra bridge (127.0.0.1:8080) returned nothing, so `universe.py` could not recrawl. The five referee VAs, `140A3A500` and `141E53520` have no `universe.json` row. specgen read them through an in-memory patch of `sets.load_universe` (scratch `run_specgen.py`, not committed). The committed `stoppage_rows.json` holds their sizes. Sizes are CONFIRMED from the PE `.pdata` exception table (flat dump, file offset = RVA): 140A3A690 468 B, 140A3A930 338 B, 140A3A870 179 B, 140A39A80 434 B, 140A38F20 346 B, 140A3A500 398 B. `141E53520` has no `.pdata` entry; its 32 B is an upper bound (next exported start). The universe sizes are lower bounds: 14047F170 has 707 there and 2771 in `.pdata`.
- Driver: `driver.py --spec-dir specs/stoppage --set sets/stoppage.txt --record results_stoppage.json --exclude-recorded`. Full run, then `--check`.
- Set registration: `gen_translated.py` `SETS` gains `("results_stoppage.json", "stoppage")`.

## 3. Per-function table

| VA | name / role | size (.pdata) | driver result | cases (valid/req) | coverage | class / blocker |
|---|---|---|---|---|---|---|
| 0x14047F170 | sanction decision tree | 2771 | diffed | 128 | 0.07 | none; provisional |
| 0x140A3A690 | involvement (foul: same half and touchline, <14 m; offside: half of offence) | 468 | diffed | 128 | 0.33 | none; provisional |
| 0x140A3A500 | helper of 140A3A690 | 398 | diffed | 101 | 0.54 | none; provisional |
| 0x141E53520 | label helper (owned by presentation) | <=32 | diffed (presentation) | 64 | 1.00 | none |
| 0x140A38F20 | linesman look-at | 346 | translated | 0 | - | harness: null record field |
| 0x140A39A80 | restart command (RefereeActionBasePosition Update) | 434 | translated | 0 | - | harness: null record field |
| 0x140A3A870 | one-signal latch (slot 18) | 179 | translated | 0 | - | harness: null record field |
| 0x140A3A930 | approach / JUDGE (slot 6) | 338 | translated | 0 | - | harness: null record field |
| 0x14044C880 | situation-sequencer runner | 1304 | not translated | - | - | indirect `[rax+0x20]` at 0x141B4B596 |
| 0x140445E90 | sequencer caller (driver) | 1138 | not translated | - | - | indirect `[rax+0x20]` at 0x141B4B596; import slot |
| 0x140480910 | sanction selector (caller of 14047F170) | 707 | not translated | - | - | callee 0x141F500D0 global store |

The driver counts "valid of 64 requested" for the 128-case runs. Failed cases are skipped and named in the log; the corpus is the passing set.

## 4. Layer drift (pre-existing; not caused here; not regenerated)

- Regenerating `src/pes/translated` changes 737 tracked bodies. The diff is the current translator's output: `s.xmm[i][0] &= 0xFFFFFFFFull; s.xmm[i][1] = 0;` after `movss` loads. `xlate.py` on HEAD and on `origin/main` emits the mask (1 match each); no committed body on either ref has it. So the drift is on the base, not in this change.
- Not committed here. The two headers also regenerate, additively only (84 insertions, 0 removals), and those are committed.
- Result on this branch: `diff_translated_drift` FAILS (`gen_translated --check: 737 drift(s) from the image; 3145 functions (2976 recorded bit-exact, 169 closure)`). Main regenerates the layer (your ruling). The proofs ran against the regenerated closure (current translator). Until Main regenerates, the committed versions of shared callees are the base versions. Nothing in GF calls the new functions yet, so no GF path is affected.

## 5. GF switch-over: what it needs (OPEN)

Main's ruling is to give GF these records in PES layout and then switch. Sized here:

- **Involvement `0x140A3A690`:** 30 inputs. `A0+0x4` (u32). `G = *[0x1436F3DC8]` at `G+0`; then `G+0x40` (f32 at `0x2770`, `0x2778`); `G+0x270` (match block: `0x116C` u32, `0x11B2` u8, `0x1544`, `0x1548`, `0x154C`, `0x1554`, `0x1558` u32, `0x155C`, `0x1564` f32, `0x1574` u32, `0x1588`, `0x158C` u32); `G+0x1928+0x254` (u8); `G+0x19C8+0x1` (u8); `G+0x1DD8` (`0x55C`, `0x564` f32); `G+0xE1EE` (`0x1`, `0x254` u8; `0x55C`, `0x564` f32). It also calls `140A48910/A48A10/A48A30` (player accessors) and `140A6CF70` (ball sample). Each field's PES meaning needs decoding before GF's state can map to it. Nothing here is decoded yet.
- **Sanction tree `0x14047F170`:** 53 inputs, mostly `A1+0x18/0xE0/0x1900/0x1904` (the event record). Its only PES caller, `0x140480910`, is not translated, so there is no call site to wire.
- **Linesman / restart actions:** GF's counterpart is `refereecontroller.cpp` (linesman case, chase unconditionally; `holdForStoppage` at line 180). The translated look-at and command are blocked (section 1).
- **Pattern:** the Cutover branch's `pesworldrecord.{hpp,cpp}` (`origin/Cutover` 744ad7fa) is the record pattern to reuse. Not merged here: its merge needs a separate integration pass.

## 6. Proof runs

- Driver: `--check` green, 3/3 (`drvchk2.log`).
- `diff_translated_drift`: FAILS, for the pre-existing reason in section 4 (737 bodies). The ctest regex `pes_team_sheet|pes_offball|diff_translated_drift|diff_band|…` matched only that test, so no other suite was run. The full gate is Main's (brief rules).
- No engine code changed, so no mutation checks apply.

## 7. Repro (build: Release, `cmake --build build -j2`, exit 0)

The first full build (2899 s, exit 0) predates the new files. An incremental pass after them (exit 0) picked them up. Both runs: `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes21-root <workdrive>/PES21`, under `tools/dev/simslot.sh timeout 900`, one at a time.

| run | determinism hash | ticks | stoppages | in-play / hold ticks | stalls |
|---|---|---|---|---|---|
| switch-off (no `--pes-loop`) | `7ca3d08f68567016` | 31521 | 18 | 21600 / 9921 | 14 |
| `--pes-loop` | `67d115b984bab251` | 25398 | 8 | 19691 / 5707 | 32 |

- Switch-off: `7ca3d08f68567016`, 31521 ticks, unchanged (the brief's value).
- `--pes-loop`: `67d115b984bab251`, 25398 ticks. The same as the documented main baseline (`pes-port-Cutover5.md` section 7). The new hash equals main's.

## 8. Open items (numbered)

1. Real-call seeds for 140A38F20, 140A39A80, 140A3A870, 140A3A930. Main says the rendezvous hooks are coming (restart builders 0x14044C880 and 0x140446390). Their synthetic corpora are empty.
2. `0x141B4B560` indirect `[rax+0x20]`: make its targets translated, or name them as stubs. Blocks 14044C880 and 140445E90.
3. `0x141F500D0` global store: decide whether the harness models image-global writes (a harness rule, Main's call). Blocks 140480910.
4. Universe rows for the referee VAs: regenerate `universe.json` when the bridge is up, then drop `stoppage_rows.json`.
5. Layer drift: Main regenerates `src/pes/translated` (737 bodies). Until then `diff_translated_drift` is red.
6. PES-layout G/MI record for the involvement fields (section 5): decode each field's meaning, put GF's record in the Cutover `pesworldrecord` pattern, then the switch and an agreement run.
7. Frames of a foul and an offside stoppage: after item 6. No frame is taken from this branch.
8. Full gate and the other `pes_*` suites: not run here (Main gates).
