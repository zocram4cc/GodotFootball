#!/usr/bin/env python3
"""registry.py: the capture-set registry (run/capsets.json): which rendezvous captures are sets, their status, and their owners.

Registry JSON: {"version": 1, "live2": DIR, "sets": [{"name", "hook", "launch", "seqs", "status", "owner", "doc"}, ...]}.
  launch: a folder under live2 (pidN); seqs: the capture folders (seqNNN) of that launch that the set names.
  status: active (an agent is replaying them now), cited (a doc or oracle names them), protected (a brief keeps them).
Guarded statuses are active and protected: auto_slim.py and capz.py refuse to slim or pack them. Cited captures may still be
packed or slimmed by the tools (a cited capture's oracle is regenerated from the registry by gen_oracle.py).

Usage (module): load(), validate(), guarded_dirs(reg), is_guarded(path, reg).
"""
import json
import os

VERSION = 1
STATUSES = ("active", "cited", "protected")
GUARDED = ("active", "protected")
FIELDS = ("name", "hook", "launch", "seqs", "status", "owner", "doc")
ENV = "GF_CAPSETS"  # the registry file (run/capsets.json, outside the repo): set it in the environment of every tool that uses it


class RegistryError(ValueError):
    pass


def path_of(path=None):
    p = path or os.environ.get(ENV)
    if not p:
        raise RegistryError("set %s to the capture registry (run/capsets.json)" % ENV)
    return p


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


def guarded_dirs(reg):
    """The realpaths of the capture folders a tool must not slim or pack (active or protected)."""
    return {os.path.realpath(d) for d in capture_dirs(reg, GUARDED)}


def is_guarded(path, guarded):
    """True when the capture folder `path` is one of the guarded realpaths."""
    return os.path.realpath(path) in guarded
