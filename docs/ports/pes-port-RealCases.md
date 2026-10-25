# RealCases: does real state lift impure-function coverage? (25-10-26)

Branch `RealCases` (worktree `<workdrive>/gf-RealCases`, base `origin/ArgState` 41659b6). Pushed to `origin/RealCases` at `6a7a1ee` (confirmed by `ls-remote`). Commits: `2f8b2cf` (real-instance cases in run.py and seed.py), `f290021` (map faults, specgen stack artifact, chain fix, 1404F6C30 regenerated), `42de21e` (banks in diffmain.py, diffparse.hpp, driver.py), `6a7a1ee` (diffharness README section). Snapshot: `<workdrive>/gf-LiveState-scratch/live/` (static; PES was down, so no live reads). Scratch and PES-derived data are not committed.

## Answer

No. Real state does not lift the floor. Real instances move the median only when the inputs are perturbed around them, and they lower the floor count. The G seed is still the best variant by floor count, and its translated side is bit-exact on 13 of its floor specs.

| 50 impure specs | ran (any coverage) | floor >= 0.50 | floor, among specs with >= 1 passing case | median | mean |
|---|---|---|---|---|---|
| z: unseeded | 50 | 14 | 12 (of 39) | 0.202 | 0.316 |
| g: G seed (ArgState2) | 50 | **16** | **14** (of 40) | 0.207 | 0.324 |
| r: real instances, no perturbation | 50 | 12 | 10 (of 40) | 0.198 | 0.286 |
| p: real instances + perturbation | 50 | 13 | 11 (of 40) | **0.272** | 0.317 |

Reading the table:

- "ran" is 50 everywhere because run.py now records failing cases and keeps going. A failing case still executes blocks, and those blocks count. The comparable column is "among specs with >= 1 passing case". ArgState2's counts (9 unseeded, 10 with G) were over the 28 specs that ran under all three of its variants, so they are not directly comparable.
- Two facts drive the result. (1) One real state per field collapses coverage: 1404F8C70 has 163 blocks with the spec's draws over the G seed, 3 with real values only, and 178 with perturbation. The branches need input variety over real state. (2) Only 6 of 50 specs bind an argument instance (team 4, man 2). The argmap leaves 132 "small-offsets (ambiguous)", 94 "A0 not in rcx" and 34 "no A0 offsets" roots untyped. For most specs, "real instance" means the G seed plus real region-backed fields.

## Five functions (own-block coverage: unseeded / G / real / real+perturb)

| spec | class (INF) | z | g | r | p | blocks (p) | passing cases (p) |
|---|---|---|---|---|---|---|---|
| 140503430 | none | 0.619 | 0.619 | 0.585 | 0.602 | 142/236 | 128 |
| 1404F47A0 | team | 0.400 | 0.400 | 0.400 | 0.400 | 8/20 | 82 |
| 1404F8C70 | team | 0.106 | 0.574 | 0.011 | **0.627** | 178/284 | 128 |
| 1404FA720 | team | 0.136 | 0.136 | 0.091 | 0.091 | 2/22 | 93 |
| 1404F6C30 | team | 0.034 | 0.034 | 0.034 | 0.034 | 6/179 | **0** (128 failed) |

## Full table: 50 impure specs (own-block coverage)

| spec | class | z | g | r | p |
|---|---|---|---|---|---|
| 1404386D0 | none | 0.088 | 0.084 | 0.038 | 0.092 |
| 1404405D0 | none | 0.072 | 0.072 | 0.072 | 0.072 |
| 1404F47A0 | team | 0.400 | 0.400 | 0.400 | 0.400 |
| 1404F6C30 | team | 0.034 | 0.034 | 0.034 | 0.034 |
| 1404F8C70 | team | 0.106 | 0.574 | 0.011 | 0.627 |
| 1404FA720 | team | 0.136 | 0.136 | 0.091 | 0.091 |
| 1404FB2A0 | none | 0.750 | 0.750 | 0.312 | 0.625 |
| 1404FE9C0 | none | 0.214 | 0.214 | 0.186 | 0.214 |
| 1404FEDC0 | none | 0.163 | 0.163 | 0.141 | 0.163 |
| 140503430 | none | 0.619 | 0.619 | 0.585 | 0.602 |
| 140504D20 | none | 0.333 | 0.333 | 0.333 | 0.333 |
| 1405055E0 | none | 0.848 | 0.848 | 0.848 | 0.848 |
| 140509A40 | none | 0.045 | 0.045 | 0.045 | 0.045 |
| 140510D30 | none | 0.818 | 0.818 | 0.291 | 0.291 |
| 14051B8B0 | none | 0.058 | 0.058 | 0.058 | 0.058 |
| 14051CF70 | none | 0.700 | 0.700 | 0.700 | 0.700 |
| 140529490 | none | 0.174 | 0.174 | 0.739 | 0.717 |
| 14052A190 | none | 0.639 | 0.639 | 0.333 | 0.389 |
| 14052A820 | none | 0.548 | 0.548 | 0.526 | 0.532 |
| 14053B110 | none | 0.259 | 0.259 | 0.259 | 0.259 |
| 14053B420 | none | 0.200 | 0.200 | 0.350 | 0.325 |
| 14057A3E0 | none | 0.286 | 0.286 | 0.286 | 0.286 |
| 14057BA50 | none | 0.545 | 0.545 | 0.545 | 0.545 |
| 14057D3A0 | none | 0.172 | 0.172 | 0.052 | 0.155 |
| 1405A6DE0 | none | 0.039 | 0.039 | 0.039 | 0.039 |
| 1405AD720 | none | 0.370 | 0.507 | 0.521 | 0.466 |
| 1405CCD50 | none | 0.400 | 0.400 | 0.400 | 0.400 |
| 1405CE4F0 | none | 0.318 | 0.318 | 0.318 | 0.318 |
| 1405CE930 | none | 0.008 | 0.008 | 0.008 | 0.008 |
| 1405DC350 | none | 0.070 | 0.070 | 0.070 | 0.070 |
| 1405DE950 | none | 0.107 | 0.107 | 0.107 | 0.107 |
| 1405E4460 | none | 0.081 | 0.081 | 0.081 | 0.081 |
| 140601110 | none | 0.312 | 0.312 | 0.312 | 0.312 |
| 14062E240 | none | 0.142 | 0.142 | 0.142 | 0.142 |
| 140639100 | none | 0.871 | 0.886 | 0.814 | 0.829 |
| 1406486D0 | none | 0.007 | 0.007 | 0.007 | 0.007 |
| 14064DC50 | none | 0.175 | 0.175 | 0.175 | 0.175 |
| 14064DE60 | man | 0.052 | 0.052 | 0.052 | 0.052 |
| 140654E60 | none | 0.760 | 0.760 | 0.702 | 0.721 |
| 14065C2A0 | none | 0.058 | 0.058 | 0.058 | 0.058 |
| 14065DAB0 | none | 0.771 | 0.619 | 0.210 | 0.495 |
| 140687310 | none | 0.130 | 0.130 | 0.130 | 0.130 |
| 14080EBA0 | none | 0.500 | 0.500 | 0.500 | 0.500 |
| 140975710 | none | 0.322 | 0.304 | 0.330 | 0.313 |
| 140978E70 | man | 0.090 | 0.090 | 0.078 | 0.078 |
| 14097CD00 | none | 0.050 | 0.050 | 0.050 | 0.050 |
| 14097FCE0 | none | 0.204 | 0.143 | 0.143 | 0.143 |
| 14097FFF0 | none | 0.062 | 0.062 | 0.113 | 0.113 |
| 140A539F0 | none | 0.692 | 0.692 | 0.692 | 0.827 |
| 140A57650 | none | 1.000 | 1.000 | 1.000 | 1.000 |

Per-spec differences between p and g: 19 specs. Largest: 140529490 (+0.543 for p), 140510D30 (-0.527), 14052A190 (-0.250), 140A539F0 (+0.135).

## Translated side (driver: translate, build, and diff bit-exact against the harness corpus)

| variant | attempted | translated | diffed (bit-exact, >= floor) | mismatches (not counted) | unmodellable callee |
|---|---|---|---|---|---|
| g: G seed | 50 | 46 | **13** | 140503430 at floor (0.619): case 123 `ret_rax` 0x22800000 (harness synthetic region base) vs 0x1001FF00 (translated stack top); 1405CE4F0 below floor (0.318): 3 mismatches | 4 |
| p: real + perturbation | 50 | 46 | 11 | 1405CE4F0 below floor, same mismatch | 4 |

- The G variant is the best by floor count, so it is the one the bit-exact check is about: 13 of its 14 floor specs are bit-exact. The missing one (140503430) is the pre-existing harness-vs-translated return-value difference that ArgState2 recorded. It is not caused by real state.
- The p variant has the best median, but only 11 of its floor specs diff. Its 140503430 is bit-exact, because perturbation avoids case 123.
- The unmodellable callees (1404F6C30, 1404FEDC0, 14080EBA0, 140975710) are the same in both.
- The driver's floor is applied on its own trace of the same corpus, so the translated counts match the harness counts (13 vs 14 for G, 11 vs 11 for p).

## Findings

**F1. CONFIRMED: `UC_ERR_MAP` was run.py masking its own unmapped-read stop.** `hook_bad` recorded `uc._bad` ("unmodelled read at ...") and called `emu_stop()`. Unicorn 2.1.0 reports the stop as UC_ERR_MAP. `run_case` checked the exception before `_bad`, so the recorded reason was never shown. Probe (venv unicorn 2.1.0, capstone decode of the faulting RIP, EA from registers at the failure): 1404386D0 at RIP 0x14043A57B, `movzx eax, byte ptr [rax+0x60]`, EA 0x212, not mapped; 1404FEDC0 at RIP 0x1404FF0E3, `movsd xmm0, qword ptr [rax]`, EA 0x91, not mapped. The faulting pointers are small integers that spec inputs wrote into pointer-valued fields. Fixed in `f290021`: the reason is reported, regions are restored on every path, and a failing case is logged to stderr and skipped instead of aborting the spec. Scope: CONFIRMED for the probed specs; INF for "all 20" (not every one was probed). Recorded failures per variant: about 1460-1490 across the 50 specs.

**F2. CONFIRMED: `1404F6C30`'s stack slot is an analyzer artifact.** Slot k=5095 gave `stack_offset` 0x9F58, above the mapped stack (run.py maps 0x100 bytes above `stack_top`), so the write raised UC_ERR_WRITE_UNMAPPED before emulation. The prologue (`0x1404F6C40 lea rbp,[rsp-0x290]`, `0x1404F6C48 sub rsp,0x390`) gives a frame of about 1 KB, so +0x9F58 is neither a frame nor an argument slot. Fixed in `specgen/spec.py`: stack slots past the mapped area are dropped. The spec was regenerated from the image (`python3 -m specgen generate --set specgen/sets/impure.txt --mode impure --image $PES_EXE --pilot-set specgen/sets/pilot.txt`); only this spec was copied. 14065C2A0, 140978E70, and 14051B8B0 also regenerate differently, for reasons outside this fix (closure_fix, regions_init, and a `.pad` region); they were not copied.

**F3. CONFIRMED: `1404F6C30` has 0 usable cases after F2.** Every case fails with "unmodelled read at 0xf3 / 0x150 / 0x1a1", i.e. small integers used as pointers. INF: null child slots in this snapshot (F5). Bank instance 0 leaves A0.6BB8, A0.6BF8, A0.6C08, A0.6D50 and several A0.6D48.* chains unseeded.

**F4. CONFIRMED: argument classes from spec evidence, not the argmap.** The argmap rows for 1404F47A0, 1404F8C70 and 1404FA720 are ambiguous. Their specs chain A0 through the team child slots +0x6D48 and +0x6D50 (`regions_init`: `A0+0x6D48 -> A0.6D48`, `A0+0x6D50 -> A0.6D50`; `pes-re-player-record.md` §0.2 names those slots team child slots). They are classed `team` (INF). 1404F6C30 is argmap `team-field`. The bank instances are T[0] and T[1] (`T[s] = *(G + (s+0xA1)*0x28)`).

**F5. CONFIRMED: null or uncaptured chains (fresh-capture list).** 30 of 50 specs have a chain region with no seed in bank instance 0, and 29 of those read it through an input. A chain is "null or uncaptured" when the walk hits a zero pointer or an address outside the captured mappings; the snapshot does not separate the two. The list, by cause:

- **A. Bound team record, child slots null.** 1404F47A0 (A0.6D48, A0.6D50), 1404F8C70 (A0.6D48.18, A0.6D48.E0, A0.6D50), 1404FA720 (A0.6D48.68, A0.6D50), 1404F6C30 (19 chains). A mid-play capture with team child slots set would test these.
- **B. G-table children null** (independent of the argument class): 1404405D0 (IMG.1437F4808*), 14062E240, 14080EBA0 (IMG.1437F4808.0, read), 140975710 (IMG.1437F4808.0, read), 14051B8B0 (IMG.143704E38, read), 1404FA720 (IMG.143704E38), 1404F6C30 (IMG.143704E38, 2AA8.30.E0).
- **C. Bound man record, children null:** 14064DE60 (A0.0.18, A0.8; 22 instances).
- **D. Unbound root** (class `none`, so A0 is never bound and its chains are never captured): 1404FE9C0, 1404FEDC0, 14057BA50, 14057D3A0, 140503430, 1405A6DE0, 140529490, 14052A190, 14053B110, 14053B420, 14064DC50, 14065DAB0, and the rest of the 29. A capture cannot help these until the argument type is known.

Recommendation: a mid-play capture is worth it for A, B and C. D needs argument typing first.

**F6. INF: 1404F47A0 does not move in any variant (8/20 blocks).** Its A0 children are null in this snapshot, so every variant reads zero children. Consistent with F5.A; not measured directly.

**F7. CONFIRMED: the real-instance path works end to end on one spec.** 1404F8C70 with banks and perturbation: translated, diffed, 128 cases bit-exact, coverage 0.627 (>= floor).

## Interfaces defined

- `seed.py --live LIVE --spec-dir D --out DIR --bank-classes CLASSES.json` writes one bank per spec (`<name>.bank.json`): instance k binds A0 to the k-th live record of the class. Case n runs instance n % N.
- `run.py --seed-bank DIR [--perturb]`: real-instance cases. Regions are zeroed before each banked case. A field an instance holds takes its real value. With `--perturb`, each (case, field) is deterministically real / nearby (±1 or ±2 for integers, ±5% for f32) / the same field of instance n+1 / the spec's own draw. Corpus records carry `inst`. A failing case is logged and skipped.
- `driver.py --seed-bank DIR [--perturb]` passes the banks to run.py and to the diff main.
- `diffmain.gen(..., bank=...)` embeds one seed per instance; each case copies the instance named by its record's `inst` (0 when unbanked, so the single-seed path is unchanged).

## Code changed

- `tools/dev/diffharness/run.py`, `tools/dev/translate/seed.py` (2f8b2cf, f290021).
- `tools/dev/diffharness/specgen/spec.py` (f290021).
- `tools/dev/diffharness/specs/impure/1404F6C30.json` (regenerated, f290021).
- `tools/dev/translate/diffmain.py`, `diffparse.hpp`, `driver.py` (42de21e).
- `tools/dev/diffharness/README.md` (6a7a1ee).

## Verification

- `py_compile` on run.py and seed.py: ok.
- Harness: all 50 specs under four variants (tables above). Each variant's coverage is the same `driver.block_coverage` on the `--trace-addrs` trace used by ArgState2.
- Translated side: `driver.py` over all 50 specs for the p and g variants (table above); the driver's bit-exact check is against the harness corpus.
- Not run: the repo gate and the engine test suite (the harness is a tool, not engine code). No mutation checks were run.

## Open items

1. Type the A0 root for the 132 "small-offsets (ambiguous)" and 94 "A0 not in rcx" specs from their field offsets. Until then the real-instance mode binds only 6 specs.
2. Capture mid-play with team child slots and G-table children set (list F5 A and B), and man children (C).
3. Input generation writes pointer-valued fields as scalars, which causes most `unmodelled read` failures. The fix belongs in specgen (type the fields). Not done here.
4. 140503430's `ret_rax` mismatch (harness synthetic base vs translated stack top) remains; it blocks this spec from the G variant's floor.
5. 1405CE4F0 has a translated mismatch on 3 cases (`w_A0_B8_d0`, 3 vs 236/254). Below the floor, so not counted; not investigated.
6. 14065C2A0, 140978E70 and 14051B8B0 regenerate differently. Not copied.

## Recommended next step

Type the A0 roots for the untyped specs from their offsets (the argmap's A0 offsets are the evidence), then rerun the bank mode. Real instances can lift coverage only where the argument is bound, and only with perturbation.
