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
