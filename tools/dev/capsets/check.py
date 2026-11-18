#!/usr/bin/env python3
"""check.py: report broken oracle links and the registry's captures missing on disk. Exit 2 only when the registry is invalid.

A broken oracle link is a symlink named mem.bin (an oracle directory's link to a capture) whose target is gone: the same set as
`find ROOT -maxdepth 4 -xtype l -name mem.bin`. Nothing is removed. A live set's capture missing on disk is reported as missing;
captures of sets marked gone (documented, not on disk) are counted apart. The exit status is 0 for any report.

Usage: check.py [--registry PATH] [--root DIR]... [--maxdepth 4] [--list]
Gate: tools/dev/gate.sh runs it as a fast step.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import registry  # noqa: E402


def broken_links(roots, maxdepth=4):
    """Paths of the mem.bin symlinks under roots whose target is missing, at depth <= maxdepth (find's counting: a root's
    children are depth 1; symlinked directories are not followed)."""
    out = []
    for root in roots:
        root = root.rstrip("/")
        for dirpath, dirnames, filenames in os.walk(root):
            d = 0 if dirpath == root else dirpath[len(root):].count("/")
            if d + 1 <= maxdepth and "mem.bin" in filenames:
                p = os.path.join(dirpath, "mem.bin")
                if os.path.islink(p) and not os.path.exists(p):
                    out.append(p)
            if d + 1 >= maxdepth:
                dirnames[:] = []
    return sorted(out)


def missing_captures(reg):
    """{set name: [capture folders not on disk]} for the sets that are not gone (a gone set is documented, not checked)."""
    out = {}
    for s in reg["sets"]:
        if s["status"] == "gone":
            continue
        gone = [d for d in (os.path.join(reg["live2"], s["launch"], q) for q in s["seqs"]) if not os.path.isdir(d)]
        if gone:
            out[s["name"]] = gone
    return out


def gone_sets(reg):
    """{set name: number of captures} of the sets marked gone."""
    return {s["name"]: len(s["seqs"]) for s in reg["sets"] if s["status"] == "gone"}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--registry", default=None)
    ap.add_argument("--root", action="append", default=None)
    ap.add_argument("--maxdepth", type=int, default=4)
    ap.add_argument("--list", action="store_true", help="print every broken link, missing capture and gone set")
    a = ap.parse_args(argv)
    try:
        reg = registry.load(a.registry)
    except registry.RegistryError as e:
        print("CAPSETS: registry invalid: %s" % e)
        return 2
    # The roots default to the directory above the rendezvous scratch area, where the captures and their links live.
    roots = a.root or [os.path.dirname(os.path.dirname(reg["live2"]))]
    links = broken_links(roots, a.maxdepth)
    missing = missing_captures(reg)
    gone = gone_sets(reg)
    live = [s for s in reg["sets"] if s["status"] != "gone"]
    print("CAPSETS: %d broken oracle links (mem.bin under %s, maxdepth %d)" % (len(links), ", ".join(roots), a.maxdepth))
    print("CAPSETS: %d of %d live-set captures missing on disk, in %d sets" % (
        sum(len(v) for v in missing.values()), sum(len(s["seqs"]) for s in live), len(missing)))
    print("CAPSETS: %d captures in %d sets marked gone (documented, not on disk)" % (sum(gone.values()), len(gone)))
    if a.list:
        for p in links:
            print("broken: " + p)
        for name in sorted(missing):
            for d in missing[name]:
                print("missing: %s (%s)" % (d, name))
        for name in sorted(gone):
            print("gone: %s (%d captures)" % (name, gone[name]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
