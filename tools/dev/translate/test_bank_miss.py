#!/usr/bin/env python3
"""A bank-corpus record whose corpus misses re-records from the capture bank and still diffs bit-exact.

results_impure.json records some functions with corpus "bank": their corpus replays GF_IMPURE_BANKS. This checks those
records with driver.py --check against an empty corpus cache, so each one misses, and asserts that:
  1. without --seed-bank the driver refuses a bank record, whatever the cache holds (the guard);
  2. with --seed-bank the first run re-records each bank record from the bank (its recorded inputs include the bank file)
     and every function still diffs bit-exact;
  3. a second run on the same cache hits those records and still diffs bit-exact.
Needs the harness Python (GF_DIFF_PYTHON), the unpacked image (PES_EXE) and the capture bank (GF_IMPURE_BANKS). Without
them, or when the record has no bank VA, it skips (77). Usage: test_bank_miss.py OUT_DIR (OUT_DIR is cleared first).
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DRIVER = os.path.join(HERE, "driver.py")
RECORD = os.path.join(HERE, "results_impure.json")
SPEC_DIR = os.path.join(HERE, "..", "diffharness", "specs", "impure")
SKIP = 77


def bank_vas():
    with open(RECORD) as f:
        results = json.load(f)["results"]
    return sorted(r["va"].upper() for r in results if r["diffed"] and r.get("corpus") == "bank")


def check(out, cache, select, bank=None):
    cmd = [os.environ["GF_DIFF_PYTHON"], DRIVER, "--check", "--image", os.environ["PES_EXE"], "--spec-dir", SPEC_DIR,
           "--jobs", "1", "--out-dir", out, "--record", RECORD, "--select", select]
    if bank is not None:
        cmd += ["--seed-bank", bank]
    run = subprocess.run(cmd, env=dict(os.environ, GF_DIFF_CACHE=cache), capture_output=True, text=True)
    return run.returncode, run.stdout + run.stderr


def tally(text, name):
    found = re.search(r"corpus %s=(\d+)\b" % name, text)
    return int(found.group(1)) if found else 0


def cache_line(text):
    found = re.search(r"^driver: cache .*$", text, re.M)
    return found.group(0) if found else "no tally"


def recorded_inputs(cache):
    """Every file a record under cache read: the keys of each validation document's files."""
    seen = set()
    root = os.path.join(cache, "records")
    for key in os.listdir(root) if os.path.isdir(root) else []:
        for name in os.listdir(os.path.join(root, key)):
            path = os.path.join(root, key, name, "run.json")
            if os.path.isfile(path):
                with open(path) as f:
                    seen |= set(json.load(f)["deps"]["files"])
    return seen


def main(argv):
    if len(argv) != 2:
        sys.exit("usage: test_bank_miss.py OUT_DIR")
    image = os.environ.get("PES_EXE", "")
    bank = os.environ.get("GF_IMPURE_BANKS", "")
    if not (os.environ.get("GF_DIFF_PYTHON") and os.path.isfile(image) and os.path.isdir(bank)):
        print("skip: needs GF_DIFF_PYTHON, PES_EXE (the unpacked image) and GF_IMPURE_BANKS (the capture bank)")
        return SKIP
    vas = bank_vas()
    if not vas:
        print("skip: %s records no bank VA" % os.path.basename(RECORD))
        return SKIP
    out = os.path.abspath(argv[1])
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    select = os.path.join(out, "select.json")
    with open(select, "w") as f:
        json.dump({os.path.basename(RECORD): vas}, f)
    cache = os.path.join(out, "cache")  # empty: every bank record misses on the first run
    n = len(vas)
    failures = []

    rc, text = check(os.path.join(out, "refused"), cache, select)
    if rc != 2 or "give --seed-bank" not in text:
        failures.append("1. no --seed-bank: want rc 2 and the guard, got rc %d:\n%s" % (rc, text))

    rc, first = check(os.path.join(out, "first"), cache, select, bank)
    if rc != 0 or tally(first, "miss") != n or tally(first, "hit"):
        failures.append("2. first run: want rc 0 and %d corpus misses, got rc %d:\n%s" % (n, rc, first))
    elif "all %d recorded-diffed functions still diff bit-exact" % n not in first:
        failures.append("2. first run: not every one of %d functions diffs bit-exact:\n%s" % (n, first))
    inputs = recorded_inputs(cache)
    bases = {f[:-5].upper(): f[:-5] for f in os.listdir(SPEC_DIR) if f.endswith(".json")}
    for va in vas:
        bank_file = os.path.abspath(os.path.join(bank, bases[va[2:]] + ".bank.jsonl"))
        if bank_file not in inputs:
            failures.append("2. the re-recorded record of %s does not read %s" % (va, bank_file))

    rc, second = check(os.path.join(out, "second"), cache, select, bank)
    if rc != 0 or tally(second, "hit") != n or tally(second, "miss"):
        failures.append("3. warm rerun: want rc 0 and %d corpus hits, got rc %d:\n%s" % (n, rc, second))
    elif "all %d recorded-diffed functions still diff bit-exact" % n not in second:
        failures.append("3. warm rerun: not every one of %d functions diffs bit-exact:\n%s" % (n, second))

    if failures:
        print("\n\n".join(failures))
        return 1
    print("ok: %d bank record(s) from %s; first [%s]; warm [%s]"
          % (n, bank, cache_line(first), cache_line(second)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
