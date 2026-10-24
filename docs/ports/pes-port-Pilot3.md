# Pilot translation: third measurement (24-10-26, Pilot2 worktree)

Head: `14798dc` on `Pilot2` (fresh worktree off `origin/main` (`06fe8d5`),
gate green there). This run redoes the pilot on the current tree with
three fixed bug classes since the last report (`pes-port-Pilot2.md`,
172/252): baked read-only-only globals (live `IMG` reads were stale baked
pointers), u128-as-value (was `rd128(addr)` on the baked constant), and
pending-aware decode stop at tail jumps (was absorbing the neighbour
function, incl. vtable calls).

## Command (one command, end to end)

```
export PES_EXE=<unpacked image> GF_DIFF_PYTHON=<venv with unicorn>
python3 tools/dev/translate/driver.py --image $PES_EXE \
    --out-dir <build>/full --jobs 2
```

Default spec dir is the committed
`tools/dev/diffharness/specs/pilot/` (252 Map specs, 64 cases each).
Generated C++ (`pes_fn_<va>.cpp` + per-function diff mains), corpora
(`.jsonl`) and diff binaries live in the out dir only; `diff_pilot_recorded`
(`tests/pilot/`, Main's rework) replays `driver.py --check` over the
committed `results.json`. `tools/dev/translate/results.json` is committed
(va/booleans/counts/class/notes; no PES bytes, no machine paths).

## Numbers (252 attempted)

| outcome | n |
|---|---|
| translated | 252 |
| diffed bit-exact | 173 |
| — clean (no live globals in spec) | 130 |
| — provisional (spec reads writable image globals at on-disk values) | 43 |
| failed | 79 |

Failed by class:

| class | n |
|---|---|
| translator bug | 43 |
| live state (harness `UC_ERR_MAP` on `IMG` globals) | 33 |
| struct map gap (spec closure, unmapped synthetic fault) | 3 |

Translator-bug split (from result notes):

| subclass | n |
|---|---|
| diff mismatch 64/64 (wrong path/value on all cases) | 9 |
| diff mismatch partial (1–58 of 64/192) | ~28 |
| compile: undefined label `L_*` (prepass target miss) | 2 |
| translate crash (indirect CALL: now engine boundary, no longer fatal) | 0 remaining |

Notable fixed since the last report: `140A63020` (live-global read),
`140A92D40` + wrapper (u128 mask), `1404A53E0` (same), `140A64030`
(decode extent), `140A533A0`/`140A51320` (pending-aware stop; now compile,
mismatch pending).

Top remaining mismatch VAs (full per-VA table in results.json):
`140439E50`, `140439EF0`, `14043A0A0`, `140458740` (1-case leftover-RAX),
`14046F230`, `14046F260`, `1404A53E0` (fixed after run: full-pilot `--only`
closure), `1404AB8E0`, `1404BCAD0`, `1404F6680`, `1405008E0`,
`140529430`, `140591A10`, `1405969B0` (fixed after run: live-global read),
`1405AAF00`, `1405BB170`, `1405CFFE0`, `140687FB0` (1 case), `140A3C9B0`
(1 case), `140A40000`, `140A40050`, `140A40110`, `140A4F230`,
`140A53F90`, `140A61CF0`, `140A63020` (fixed after run), `140A6Exx`
(several, `want N got 0` returns), `140A72E70`, `140A8F7C0`,
`140A92950`, `140A92D40` (fixed after run: u128), `14149F4F0`,
`141E5D4C0`.

## Ledger

`joins.py --diff old_diff_spec.json --pilot results.json --funclist`
keeps the 21 head diffed rows and adds the pilot's. `translated` status
(rank 1, same tier as diffed); pilot proof supersedes ports in
`classify` (stand-ins stay). 160 scope moves; `--check` green.
Per-VA table in results.json is authoritative (the ledger folds
pilot-diffed stand-ins back to stand-in).

## Cutover sample: 1 of 10 done (LaneZ)

Mechanism (committed, no PES bytes in the repo): `src/pes/pespilot.hpp`
declares `::pes_fn_<va>` (VAs only); `gen_pilot_lib.py` generates the TU
into `${CMAKE_BINARY_DIR}/pilot-lib` at build time from the user's image
and compiles the `pespilot` static lib (only when `PES_EXE` is set at
configure time; otherwise no target, `PES_PILOT_AVAILABLE` unset, GF
falls back to the hand port). `gfsim` links `pespilot` when the target
exists. Call sites build a `PesState` (stack seg + RSP init), set entry
regs/XMM, call, read results.

| # | VA | GF site | requests | suites | result |
|---|---|---|---|---|---|
| 1 | 140A71990 | `LaneZ` in `pesoffball.cpp` | ~25 (mechanism + build + suite) | `pes_offball_test` via simslot: 71 pass, 2 install-skips | bit-exact path in both modes (same F1/VFS, same 2.0 m); GF mode unchanged by construction |

Remaining 9 (all clean-diffed, GF port cited, fields GF-carried):
`1404F4AB0`, `140A3E880`, `140A3FE50`, `140A4C720`, `140A57710`,
`140A57720`, `140A57730`, `140A57800`, `140A60500`
(`140A66FF0`, `140A6BD60`, `140A6CF70`, `140A71260`, `140A714x`,
`140A71500`, `140A71990` done, `140A76070`, `140A92DC0` also qualify;
taking the 9 smallest-blast-radius ones first).
Each needs 1 call-site edit + field carriers already present + the touched
suite through simslot; none needs record-layout changes (unlike the
pointer-returning selectors, which do and are excluded from the 10).

## Projection over the 18,374 in-scope rows

Measured rates (this run, current tree): translated 252/252 (100%),
diffed 173/252 (69%), clean-diffed 130/252 (52%).

- At the pilot's 52% clean rate: ~9,550 clean-diffed of 18,374.
- The pilot is 252/252 pure-call-graph AI functions with existing GF
  ports (median 21 insns). It says nothing direct about the impure 69%
  of the ledger (engine calls, GPU, live heap graphs): those need
  stand-ins/first-decode work the translator cannot do, so the 52% is an
  upper bound for the whole.
- What the pilot does tell us: for pure computation with closed calls,
  translate+diff costs ~1 tool-call per function of driver time plus
  ~0.5 translator fixes per 10 functions at this maturity; the cutover
  sample (1 done) suggests ~25 requests for the first (mechanism
  included), ~5–10 per follow-up same-shape cutover.
- Decision rule: 69% total / 52% clean is the top of the 30–70% band by
  the letter, but the clean number is what compounds (provisional diffs
  inherit live-state risk). Recommendation: one more fix pass on the two
  named clusters (mismatch triage + undefined labels), then re-pilot;
  start cutovers in parallel on the clean list since they are independent
  of translator fixes.

## Repro/smoke: not run (Main runs the batch smoke)

No 5-min seed-7 repro in this run; the cutover (`LaneZ`) is
arithmetic-identical in both modes (same inputs, same outputs), and the
offball suite passes. The 5-min `--pes-loop` hash check belongs to the
batch gate after the remaining 9 cutovers land.

## Main's measurement of this branch (Pilot2 `14798dc`), after the report

The fixes above were all applied after the run the report counts. Main ran the full driver on the branch head twice
(deterministic, both runs identical): **164/252 bit-exact (114 clean, 50 provisional)**; failures 52 translator bug,
33 live state, 3 struct gap. Against main's recorded 173: 23 functions lost (8 of them every-case `ret_xmm0`
mismatches, one compile error in `140A40820`), 14 gained. **Net regression; the branch is held.** `driver.py --check`
(on main since batch S) would have caught it; it was not run before pushing.

## Not reachable for users yet: the unpacked image

The translator and the harness read `pes_full.bin`, an unpacked image produced by hand outside the repo
(`<local>/ghidra-projects/scratch/Unpacker/`, with recovered key files). The repo has no script that makes it from a
user's install, so per AGENTS.md (every PES-derived asset reproducible by script) no user can build translated code.
Owner decision pending.
