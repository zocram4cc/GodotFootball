#!/usr/bin/env python3
"""bankstore.py: per-spec seed-artifact banks on disk, read one instance at a time.

A bank is `<tag>.bank.jsonl`: line 1 is `{"roots": {...}}`, then one seed artifact per
line (compact JSON, so no raw newline inside a line). `Bank` indexes the line offsets once
and reads instance k with `bank[k]`, so memory stays at one instance however large the bank.
The legacy `<tag>.bank.json` (`{"roots", "instances"}`, seed.py) is only read by `merge`.

Usage:
  bankstore.py merge OUT_DIR SRC_DIR [SRC_DIR ...]   union of every spec's bank over the sources
  bankstore.py --selftest
"""
import json
import os
import shutil
import sys


class Bank:
    """Read-only, indexed view of one bank file. len() and bank[k] as for a list."""

    def __init__(self, path):
        self.path = path
        self.offsets = []
        with open(path, "rb") as f:
            self.roots = json.loads(f.readline())["roots"]
            while True:
                pos = f.tell()
                line = f.readline()
                if not line:
                    break
                if line.strip():
                    self.offsets.append(pos)
        self._f = open(path, "rb")

    def __len__(self):
        return len(self.offsets)

    def __getitem__(self, k):
        self._f.seek(self.offsets[k])
        return json.loads(self._f.readline())


def write_bank(path, roots, instances):
    """Write a bank from an iterable of artifacts, one instance in memory at a time."""
    with open(path, "w") as f:
        f.write(json.dumps({"roots": roots}) + "\n")
        for art in instances:
            line = json.dumps(art)
            assert "\n" not in line
            f.write(line + "\n")


def _roots_of(path):
    """The roots of one source bank, jsonl or legacy json."""
    if path.endswith(".bank.jsonl"):
        return Bank(path).roots
    with open(path) as f:
        return json.load(f).get("roots", {})


def _instances(path):
    """The instances of one source bank, jsonl (streamed) or legacy json (loaded whole)."""
    if path.endswith(".bank.jsonl"):
        bank = Bank(path)
        for k in range(len(bank)):
            yield bank[k]
        return
    with open(path) as f:
        doc = json.load(f)
    yield from doc["instances"]


def merge(out_dir, src_dirs):
    """Union of every spec's bank over src_dirs, written to out_dir as <tag>.bank.jsonl.
    Roots: the first source's roots, extended by any root key a later source adds.
    Returns {tag: {source file: instance count}}."""
    os.makedirs(out_dir, exist_ok=True)
    tags = sorted({f.split(".bank.")[0] for d in src_dirs for f in os.listdir(d)
                   if ".bank.json" in f})
    report = {}
    for tag in tags:
        srcs = [os.path.join(d, f) for d in src_dirs for f in (tag + ".bank.jsonl", tag + ".bank.json")
                if os.path.exists(os.path.join(d, f))]
        roots, counts = {}, {}
        for p in srcs:
            for k, v in _roots_of(p).items():
                roots.setdefault(k, v)

        def chain(srcs=srcs, counts=counts):
            for p in srcs:
                n = 0
                for art in _instances(p):
                    n += 1
                    yield art
                counts[os.path.basename(p)] = n

        write_bank(os.path.join(out_dir, tag + ".bank.jsonl"), roots, chain())
        report[tag] = counts
    return report


def _selftest():
    import tempfile
    base = os.environ.get("GF_SCRATCH") or os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "build")
    os.makedirs(base, exist_ok=True)
    d = tempfile.mkdtemp(prefix="bankstore-", dir=base)
    try:
        arts = [{"g_base": 0, "regions": [{"name": "A0", "off": 0, "size": 8, "hex": "%016x" % i}]}
                for i in range(5)]
        p = os.path.join(d, "t.bank.jsonl")
        write_bank(p, {"A0": "team"}, arts)
        b = Bank(p)
        assert len(b) == 5 and b.roots == {"A0": "team"}
        assert [b[k] for k in (4, 0, 2)] == [arts[4], arts[0], arts[2]]
        with open(os.path.join(d, "t.bank.json"), "w") as f:
            json.dump({"roots": {"A1": "man"}, "instances": arts[:2]}, f)
        out = os.path.join(d, "out")
        report = merge(out, [d])
        m = Bank(os.path.join(out, "t.bank.jsonl"))
        assert len(m) == 7, len(m)
        assert m.roots == {"A0": "team", "A1": "man"}, m.roots
        assert report["t"] == {"t.bank.jsonl": 5, "t.bank.json": 2}, report
        print("bankstore selftest ok")
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main(argv):
    if argv[1:2] == ["--selftest"]:
        _selftest()
        return 0
    if len(argv) >= 4 and argv[1] == "merge":
        report = merge(argv[2], argv[3:])
        print("merged %d specs -> %s" % (len(report), argv[2]))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
