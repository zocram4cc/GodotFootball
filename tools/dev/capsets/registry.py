#!/usr/bin/env python3
"""registry.py: the capture-set registry (run/capsets.json): which rendezvous captures are sets, their status, and their owners.

Registry JSON: {"version": 1, "live2": DIR, "sets": [{"name", "hook", "launch", "seqs", "status", "owner", "doc"}, ...]}.
  launch: a folder under live2 (pidN); seqs: the capture folders (seqNNN) of that launch that the set names.
  status:
    active    an agent is replaying these captures now (a live pin): never slimmed, deleted or packed.
    protected a brief keeps them: never slimmed or deleted. Packing is lossless and keeps the disk above its floor, so it is allowed.
    cited     a doc or an oracle names them: any tool may slim or pack them.
    gone      the captures are no longer on disk; the doc citation is kept. check.py reports them apart from missing captures.
Refusals (guarded_dirs): slim and delete refuse active and protected; pack refuses active only.

Usage (module): load(), validate(), guarded_dirs(reg, action), default_path(), mark_gone(reg).
CLI: registry.py mark-gone [--registry PATH] [--write]: sets status gone on the entries whose captures are not on disk.
"""
import argparse
import json
import os
import sys

VERSION = 1
STATUSES = ("active", "cited", "protected", "gone")
FIELDS = ("name", "hook", "launch", "seqs", "status", "owner", "doc")
REFUSED = {"slim": ("active", "protected"), "delete": ("active", "protected"), "pack": ("active",)}
ENV = "GF_CAPSETS"  # the registry file; unset, the standard run file (default_path) is used
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


class RegistryError(ValueError):
    pass


def default_path():
    """The standard run file: the rendezvous scratch area beside the repository's checkout (the parent of the repo or worktree)."""
    return os.path.join(os.path.dirname(REPO), "gf-Rendezvous-scratch", "run", "capsets.json")


def path_of(path=None):
    return path or os.environ.get(ENV) or default_path()


def validate(reg):
    """Raises RegistryError when the registry is not one this module can read. Returns reg."""
    if not isinstance(reg, dict) or reg.get("version") != VERSION:
        raise RegistryError("registry version must be %d" % VERSION)
    if not isinstance(reg.get("live2"), str) or not os.path.isabs(reg["live2"]):
        raise RegistryError("live2 must be an absolute directory")
    names = set()
    for s in reg.get("sets", []):
        missing = [k for k in FIELDS if k not in s]
        if missing:
            raise RegistryError("set %r lacks %s" % (s.get("name"), ", ".join(missing)))
        if s["status"] not in STATUSES:
            raise RegistryError("set %r: status must be one of %s" % (s["name"], ", ".join(STATUSES)))
        if s["name"] in names:
            raise RegistryError("duplicate set name %r" % s["name"])
        names.add(s["name"])
        if not isinstance(s["seqs"], list) or not s["seqs"]:
            raise RegistryError("set %r: seqs must be a non-empty list" % s["name"])
        if s["launch"] in ("", ".", "..") or "/" in s["launch"]:
            raise RegistryError("set %r: launch must be one folder name" % s["name"])
        for q in s["seqs"]:
            if not isinstance(q, str) or "/" in q or q in ("", ".", ".."):
                raise RegistryError("set %r: a seq must be one folder name, not %r" % (s["name"], q))
    return reg


def load(path=None):
    """The validated registry. Raises RegistryError when the file is missing or invalid (tools refuse to run without it)."""
    p = path_of(path)
    try:
        with open(p) as f:
            reg = json.load(f)
    except (OSError, ValueError) as e:
        raise RegistryError("cannot read the capture registry %s: %s" % (p, e))
    return validate(reg)


def capture_dirs(reg, statuses=None):
    """{absolute capture folder: set} for every seq of every set (of the given statuses, all when None)."""
    out = {}
    for s in reg["sets"]:
        if statuses is not None and s["status"] not in statuses:
            continue
        for q in s["seqs"]:
            out[os.path.join(reg["live2"], s["launch"], q)] = s
    return out


def guarded_dirs(reg, action):
    """The realpaths of the capture folders `action` (slim, delete or pack) must not touch."""
    if action not in REFUSED:
        raise ValueError("action must be one of %s, not %r" % (", ".join(REFUSED), action))
    return {os.path.realpath(d) for d in capture_dirs(reg, REFUSED[action])}


def mark_gone(reg, exists=os.path.isdir):
    """(registry, changes): an entry none of whose captures is on disk becomes gone; a partly present entry is split, its missing
    seqs moving to a set named NAME-gone with status gone (same owner and doc). Present captures are untouched.
    changes: [(set name, "gone" or "split", number of missing captures)]."""
    out = dict(reg, sets=[])
    changes = []
    for s in reg["sets"]:
        if s["status"] == "gone":
            out["sets"].append(s)
            continue
        here = [q for q in s["seqs"] if exists(os.path.join(reg["live2"], s["launch"], q))]
        gone = [q for q in s["seqs"] if q not in here]
        if not gone:
            out["sets"].append(s)
        elif not here:
            out["sets"].append(dict(s, status="gone"))
            changes.append((s["name"], "gone", len(gone)))
        else:
            out["sets"].append(dict(s, seqs=here))
            out["sets"].append(dict(s, name=s["name"] + "-gone", seqs=gone, status="gone"))
            changes.append((s["name"], "split", len(gone)))
    return validate(out), changes


def write_atomic(path, reg):
    tmp = path + ".part"
    with open(tmp, "w") as f:
        json.dump(reg, f, indent=1)
        f.write("\n")
    os.replace(tmp, path)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    mg = sub.add_parser("mark-gone", help="mark the entries whose captures are not on disk as gone")
    mg.add_argument("--registry", default=None)
    mg.add_argument("--write", action="store_true", help="write the registry (default: print what would change)")
    a = ap.parse_args(argv)
    try:
        reg = load(a.registry)
    except RegistryError as e:
        print("registry: %s" % e)
        return 2
    new, changes = mark_gone(reg)
    for name, how, n in changes:
        print("%s: %s (%d captures not on disk)" % (name, how, n))
    print("registry: %d entries change" % len(changes))
    if a.write and changes:
        write_atomic(path_of(a.registry), new)
        print("registry: written %s" % path_of(a.registry))
    return 0


if __name__ == "__main__":
    sys.exit(main())
