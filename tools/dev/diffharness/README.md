# DiffHarness: the differential test harness

PES's own functions run in an emulator over the user's unpacked executable
image; GF's ports run on the same inputs; outputs must match bit for bit.

Layout:

- `run.py` — the emulator core (unicorn). Maps the image flat
  (file offset = VA − 0x140000000), a stack, synthetic heap regions; sets
  argument registers per the Windows x64 ABI; stubs callees by VA; hooks
  unmapped reads so an unmodelled global stops the run with a clear message
  instead of a crash; stops at the function's return.
- `specs/<fn>.json` — per-function memory-layout spec: which regions at which
  base addresses, which offsets each named input lands at, which callees are
  stubbed and with what behaviour, which registers carry arguments and return
  values. `run.py --check-spec` validates a spec against the image (entry and
  stub VAs mapped, regions disjoint) before running.
- No PES bytes live here: specs cite VAs; constants and edge vectors are read
  from the user's image at generation time (boundary builders, `from_image`
  post-fills, full-range enumerations — never committed tables).

Images: `PES_EXE` must be an UNPACKED image. A packed `PES2021.exe` is
refused structurally (its filesize equals the section table's max raw end;
the dump appends live pages past it): packed bytes differ from the image's
at every code VA, so emulating them computes garbage. The C++ compare
binaries load their constants from the packed exe (`$PES21_ROOT/PES2021.exe`,
whose section table addresses .rdata/.text correctly); only the corpus
generator needs the unpacked image.

Usage (normally via ctest `diff_gen_<fn>` + `tests/diff/`, which call
`generate.sh`; the manual form below is for harness development):

    export GF_DIFF_PYTHON=python-with-unicorn-and-capstone
    export PES_EXE=/path/to/unpacked/image
    $GF_DIFF_PYTHON tools/dev/diffharness/run.py --exe $PES_EXE \
        --spec tools/dev/diffharness/specs/band.json --out build/diff/band.jsonl

`GF_DIFF_PYTHON` defaults to `python3`; without unicorn the generator skips
cleanly (ctest 77), as it does without `PES_EXE`. The landing gate passes
its `GF_DIFF_PYTHON`/`PES_EXE` through to ctest.

## Real-instance cases (seed banks)

A bank holds one seed per live instance of an argument class. Case n then runs on instance n % N, with that instance's
real bytes in its regions.

    python3 tools/dev/translate/seed.py --live LIVE --spec-dir specs/impure --out BANKS --bank-classes CLASSES.json
    python3 tools/dev/diffharness/run.py --exe "$PES_EXE" --spec specs/impure/1404F8C70.json --out corp.jsonl \
        --seed-bank BANKS [--perturb]

- `CLASSES.json` maps a lower-case spec name to the class of its A0 root: `team` (T[0], T[1]), `man`, `anime`, `slot`
  (the 22 on-pitch records), or `none` (G only). The classes are INF: the argmap where it names one, otherwise A0
  chains through the team child slots +0x6D48/+0x6D50.
- An input that names a field an instance holds takes that instance's real value. With `--perturb`, each (case, field)
  is deterministically one of: real, nearby (±1 or ±2 for integers, ±5% for f32), the same field of instance n+1, or
  the spec's own draw. A field no instance holds keeps its draw.
- Regions are zeroed before each banked case, so one instance's bytes never leak into the next.
- A failing case (an access through a value no region covers) is logged on stderr as `case N failed: unmodelled read at
  0x...` and skipped. The corpus holds the passing cases, and each record carries `inst`.
- The translated side reads `inst` per record: `translate/driver.py --seed-bank BANKS [--perturb]`.

## specgen: the spec generator

`specgen/` writes one spec per VA from the image (`PES_EXE`, the unpacked executable). Its output is scratch until it
is copied into `specs/`.

    cd tools/dev/diffharness
    python3 -m specgen select --subsystem cutscene --write-set specgen/sets/cutscene.txt
    python3 -m specgen select --subsystem replay camera crowd HUD audio --write-set specgen/sets/presentation.txt
    python3 -m specgen generate --set specgen/sets/pilot.txt --mode pilot --image "$PES_EXE" --out-dir DIR
    python3 -m specgen generate --set specgen/sets/impure.txt --mode impure \
        --pilot-set specgen/sets/pilot.txt --image "$PES_EXE" --out-dir DIR

- `sets/`: VA lists, one per line (`VA<TAB>status`). Order is part of the spec: the seed is 1000 + position.
- `sets/presentation.txt`: the replay, camera, crowd, HUD and audio pure rows as one union (594 seeds and 45 callees).
  The fourth column is the role (seed or callee), the third the subsystem that reaches the row first; the driver's
  `--set` records both on each result. Match commentary is out (owner ruling 25-10-26): no RTTI or string evidence
  places commentary code in these rows, and its class and speech-bank loaders lie outside the walked universe.
- `analyze.py`: the record-field walker over the image's bytes (capstone). `spec.py`: the pilot-mode builder.
- `classify.py`, `impure.py`: impure mode's out-of-set callees, stub references and harness class.
- `rules.py`: each hand fix the committed specs carry, with its provenance: case-varied `.data` inputs, the
  return-register audit, G record accessors laid out from the accessor code, blanket E1EE slots removed, and dangling
  `regions_init` entries dropped.
- `sets.py`: VA sets, and the ledger's pure closure (`tools/dev/ledger.py`, `pure_rows`).
- Not generated yet: the G+0x180 table relink (`14065C2A0`, `140978E70`). Those two specs stay hand-kept.
