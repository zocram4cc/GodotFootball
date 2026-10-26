#!/usr/bin/env python3
"""bankstore.py: per-spec seed-artifact banks on disk, read one instance at a time.

A bank is `<tag>.bank.jsonl`: line 1 is `{"roots": {...}}`, then one seed artifact per
line (compact JSON, so no raw newline inside a line). `Bank` indexes the line offsets once
and reads instance k with `bank[k]`, so memory stays at one instance however large the bank.
The legacy `<tag>.bank.json` (`{"roots", "instances"}`, seed.py) is only read by `merge`.

Usage:
  bankstore.py merge OUT_DIR SRC_DIR [SRC_DIR ...]   union over the sources, deduplicated by content
  bankstore.py --selftest
"""
import hashlib
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


def _open_source(path):
    """One source bank as an indexable sequence of instances: jsonl (read on demand) or legacy json (loaded whole)."""
    if path.endswith(".bank.jsonl"):
        return Bank(path)
    with open(path) as f:
        return json.load(f)["instances"]


def digest(art):
    """Content identity of one instance: the sha256 of its canonical JSON."""
    return hashlib.sha256(json.dumps(art, sort_keys=True).encode()).hexdigest()


def merge(out_dir, src_dirs):
    """Union of every spec's bank over src_dirs, written to out_dir as <tag>.bank.jsonl.

    Instances are deduplicated by content and written in digest order, so the merged bank depends
    only on the set of instances, never on the order of src_dirs. Roots: the union of every
    source's roots; a root bound to two different values is an error.
    Returns {tag: {"read": {source file: instance count}, "kept": distinct instance count}}."""
    os.makedirs(out_dir, exist_ok=True)
    tags = sorted({f.split(".bank.")[0] for d in src_dirs for f in os.listdir(d)
                   if ".bank.json" in f})
    report = {}
    for tag in tags:
        srcs = [os.path.join(d, f) for d in src_dirs for f in (tag + ".bank.jsonl", tag + ".bank.json")
                if os.path.exists(os.path.join(d, f))]
        roots, read, where = {}, {}, {}
        for p in srcs:
            for k, v in sorted(_roots_of(p).items()):
                if roots.setdefault(k, v) != v:
                    raise SystemExit("merge %s: root %s bound to %r and %r" % (tag, k, roots[k], v))
        for p in srcs:
            src = _open_source(p)
            read[os.path.basename(p)] = len(src)
            for k in range(len(src)):
                where.setdefault(digest(src[k]), (p, k))
        opened = {}

        def chain():
            for dg in sorted(where):
                p, k = where[dg]
                if p not in opened:
                    opened[p] = _open_source(p)
                yield opened[p][k]

        write_bank(os.path.join(out_dir, tag + ".bank.jsonl"), dict(sorted(roots.items())), chain())
        report[tag] = {"read": read, "kept": len(where)}
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
        assert len(m) == 5, len(m)  # the legacy json repeats two instances: deduplicated
        assert m.roots == {"A0": "team", "A1": "man"}, m.roots
        assert report["t"] == {"read": {"t.bank.jsonl": 5, "t.bank.json": 2}, "kept": 5}, report
        # The merged bank does not depend on the order of the source directories.
        d2 = os.path.join(d, "second")
        os.makedirs(d2)
        shutil.copy(os.path.join(d, "t.bank.json"), d2)
        merge(os.path.join(d, "ab"), [d, d2])
        merge(os.path.join(d, "ba"), [d2, d])
        with open(os.path.join(d, "ab", "t.bank.jsonl"), "rb") as fa, \
                open(os.path.join(d, "ba", "t.bank.jsonl"), "rb") as fb:
            assert fa.read() == fb.read()
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
