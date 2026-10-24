# Pilot translation: second measurement (24-10-26)

Worktree `<workdrive>/gf-Pilot`, branch `Pilot` at `bafb15b`.
Gate input state: `tools/dev/ledger.py --check` green on the committed tree
(LEDGER.md regenerated, 160 scope moves, off-path ports 129 allow-listed).
The full-run numbers below are from scratch `drvfinal2` (build tree only).

## Command (one command, end to end)

```
python3 tools/dev/translate/driver.py --image $PES_IMAGE \
    --out-dir <build>/drvfinal2 --jobs 2
```

Default spec dir is the committed
`tools/dev/diffharness/specs/pilot/` (252 Map specs, 64 cases each).
Generated C++ (`pes_fn_<va>.cpp`), corpora (`.jsonl`) and diff binaries
live in the out dir only. `tools/dev/translate/results.json` is committed
(va/booleans/counts/class/notes; no PES bytes, no machine paths).

Pilot diffs are wired into ctest like `tests/diff/`: `tests/pilot/`
generates one `diff_gen_pilot_<va>` corpus test and one `diff_pilot_<va>`
binary test per committed spec (build-time translate via
`tools/dev/translate/gen_pilot_test.py`, skip-77 without `PES_EXE`).
Not gate-run here (verification is Main's); the wiring compiles the same
generator path the driver uses (smoke-tested on one VA + skip stubs).

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
| translator bug | 43 + 1 untranslated at run time (`KeyError xmm2`, fixed after: MOVD reg source) |
| live state (harness `UC_ERR_MAP` on `IMG` globals) | 33 |
| struct map gap (spec closure, unmapped synthetic fault) | 3 |

Translator-bug split (from result notes):

| subclass | n |
|---|---|
| diff mismatch 64/64 (wrong path/value on all cases) | 9 |
| diff mismatch partial (1–55 of 64/192) | ~30 |
| compile: undefined label `L_*` (prepass target miss) | ~4 |
| compile: `k_c*_h` undeclared (odd-size const scalar view; fixed) | 1 |
| translate crash: indirect CALL/JMP (fixed: engine boundary + tail-stop decode) | 0 remaining |

Top mismatch VAs (full list in results.json):
`140439E50`, `140439EF0`, `14043A0A0`, `140458740` (1-case leftover-RAX),
`14046F230`, `14046F260`, `1404A53E0`, `1404AB8E0`, `1404BCAD0`,
`1404F6680`, `1405008E0`, `140529430`, `140591A10`, `1405969B0`,
`1405AAF00`, `1405BB170`, `1405CFFE0`, `140687FB0` (1 case),
`140A3C9B0` (1 case), `140A40000`, `140A40050`, `140A40110`,
`140A4F230`, `140A53F90`, `140A61CF0`, `140A63020`, `140A6Exx`
(several, `want N got 0` returns), `140A72E70`, `140A8F7C0`,
`140A92950`, `140A92D40` (atan2 wrapper, polarity), `14149F4F0`,
`141E5D4C0`.

Fixed after this run's code snapshot (in tree, not in these numbers):
MOVD reg source (`KeyError xmm2`), odd-size const scalars, full-window rip
sizes, JAE aliases, POP semicolon, sext/SAR order, CVTPS2PD bit
reinterpret, switch on table index reg + structural two-level sizing,
transitive + generated-code link closure, decode stop at tail jumps,
indirect-call engine boundary, note hygiene. The two-level switch and
closure fixes observably convert (spot: `1408C51F0`, `1404F5F10`,
`140A64030`, `140A92DC0`/`140A92C80` now bit-exact standalone).

## Ledger

`joins.py --diff old_diff_spec.json --pilot results.json --funclist`
keeps the 21 head diffed rows and adds the pilot's. `ledger.py` gains the
`translated` status (rank 1, same tier as diffed) and the pilot proof
supersedes ports in `classify` (stand-ins stay). 160 scope moves recorded
(ported→diffed 105, ported→translated 49); `--check` green.

In-scope counts after this run: ported 408, diffed 154, translated 63,
stand-in 100, decoded 1190, leaf 1819, engine 1242, undecoded 13397.

Note: the ledger counts 154 diffed / 63 translated while the driver
reports 173 diffed (130 clean + 43 provisional); the delta is
stand-in-first ordering (pilot VAs already stand-ins stay stand-in)
plus the old/new overlap. The per-function table in results.json is
authoritative.

## Failure classes for the next fix pass (ordered)

1. **diff mismatches (~39)** — needs per-VA triage; biggest win
   candidates: `140A92D40` (atan2 polarity, 10 callers), the `want N
   got 0` returns (`140A6Exx`, `140529430`, `1405969B0`, `1405CFFE0`,
   `1405008E0` — smells like one path-divergence bug), switch functions.
2. **compile: undefined label (~4)** — prepass target misses.
3. **live state (32)** — with PilotMap-2 (lane-base fix landed during
   this run; 30 of these are the loaded-pointer-as-base shape I reported:
   value at A0+off passed as a base; needs spec-side region binding).
4. **struct map gap (3)** — spec closure gaps, Map-side.

## Cutover sample: not done (0 requests spent)

No GF hand port was cut over in this run. Reason: the translator still
moves under us (every fix changes TUs), so cutting over now would churn.
The 130 clean-diffed VAs are the candidate pool; the fieldmap `gf: null`
count per record still needs the audit Main asked for before picking 10
with full GF carriers.

## Requests spent

Approximately 120 this turn (translator fixes, driver, ledger wiring,
ctest wiring, report). Remaining budget reserved for Main's follow-ups.
