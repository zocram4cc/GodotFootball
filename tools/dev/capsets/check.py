#!/usr/bin/env python3
"""check.py: report broken oracle links and the registry's captures missing on disk. Exit 2 only when the registry is invalid.

A broken oracle link is a symlink named mem.bin (an oracle directory's link to a capture) whose target is gone: the same set as
`find ROOT -maxdepth 4 -xtype l -name mem.bin`. Nothing is removed. A registry capture missing on disk is reported, not fatal.

Usage: check.py [--registry PATH] [--root DIR]... [--maxdepth 4] [--list]
Gate: tools/dev/gate.sh runs it as a fast step and prints the count line.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import registry  # noqa: E402

# The roots default to the scratch area the registry's captures live in (live2's grandparent), so no path is written here.


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
    """{set name: [capture folders of the set that are not on disk]}, for the sets that have any."""
    out = {}
    for s in reg["sets"]:
        gone = [d for d in (os.path.join(reg["live2"], s["launch"], q) for q in s["seqs"]) if not os.path.isdir(d)]
        if gone:
            out[s["name"]] = gone
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--registry", default=None)
    ap.add_argument("--root", action="append", default=None)
    ap.add_argument("--maxdepth", type=int, default=4)
    ap.add_argument("--list", action="store_true", help="print every broken link and missing capture, not only the counts")
    a = ap.parse_args(argv)
    try:
        reg = registry.load(a.registry)
    except registry.RegistryError as e:
        print("CAPSETS: registry invalid: %s" % e)
        return 2
    roots = a.root or [os.path.dirname(os.path.dirname(reg["live2"]))]
    links = broken_links(roots, a.maxdepth)
    gone = missing_captures(reg)
    total = sum(len(v) for v in gone.values())
    slots = sum(len(s["seqs"]) for s in reg["sets"])
    print("CAPSETS: %d broken oracle links (mem.bin under %s, maxdepth %d)" % (len(links), ", ".join(roots), a.maxdepth))
    print("CAPSETS: %d of %d registry captures missing on disk, in %d sets" % (total, slots, len(gone)))
    if a.list:
        for p in links:
            print("broken: " + p)
        for name in sorted(gone):
            for d in gone[name]:
                print("missing: %s (%s)" % (d, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
