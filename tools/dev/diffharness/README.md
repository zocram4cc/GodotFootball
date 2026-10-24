# DiffHarness: the differential test harness

PES's own functions run in an emulator over the user's executable; GF's ports
run on the same inputs; outputs must match bit for bit.

Layout:

- `run.py` — the emulator core (unicorn). Maps the executable's sections from
  the user's image, a stack, synthetic heap regions; sets argument registers
  per the Windows x64 ABI; stubs callees by VA; hooks unmapped reads so an
  unmodelled global stops the run with a clear message instead of a crash;
  stops at the function's return.
- `specs/<fn>.json` — per-function memory-layout spec: which regions at which
  base addresses, which offsets each named input lands at, which callees are
  stubbed and with what behaviour, which registers carry arguments and return
  values. `run.py --check-spec` validates a spec against the image (callee VAs
  hold a `call`-able prologue, constant VAs are mapped) before running.
- No PES bytes live here: specs cite VAs; constants are read from the user's
  image at run time.

Usage (normally via ctest `diff_gen_<fn>` + `tests/diff/`, which call
`generate.sh`; the manual form below is for harness development):

    export GF_DIFF_PYTHON=python-with-unicorn-and-capstone
    export PES_EXE="$PES21_ROOT/PES2021.exe"  # or an unpacked image, same VAs
    $GF_DIFF_PYTHON tools/dev/diffharness/run.py --exe $PES_EXE \
        --spec tools/dev/diffharness/specs/cond_band.json --out build/diff/cond_band.jsonl

`GF_DIFF_PYTHON` defaults to `python3`; without unicorn the generator skips
cleanly (ctest 77). The landing gate passes its `GF_DIFF_PYTHON`/`PES_EXE`
through to ctest. `PES_EXE` defaults to `$PES21_ROOT/PES2021.exe`.
