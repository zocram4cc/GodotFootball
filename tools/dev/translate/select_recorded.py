#!/usr/bin/env python3
"""select_recorded.py: which recorded functions the fast gate must re-check (tools/dev/README.md, "Gate tiers").

A recorded function's diff builds its call closure, so a change to any function in that closure can break its proof.
The closure is read from the committed layer itself: gen_translated.py commits, for every recorded function, exactly the
callees its diff build compiled (transitively), and each file calls them as pes_fn_<VA>(. A recorded function is
selected when
  - its own generated file, a closure member's generated file, or the spec of either changed since the base, or
  - its record in results*.json is new or changed since the base, or
  - the harness (run.py, the stub table), or the runtime headers the generated code includes, changed (GLOBAL: everything).
A translator edit (xlate.py, flow.py) is not GLOBAL: what it changes shows as a diff of the generated files, and the drift
check (a ctest test of the same gate) fails if the committed layer is not what the translator now produces.
Calls through the indirect-dispatch table are not edges: a change to a dispatch-only target selects nothing here; the
full tier re-checks everything. So does a driver.py edit: it orchestrates the diff but does not shape its output.

Usage: select_recorded.py [--base REV] [--out FILE]   (REV defaults to the merge base of HEAD and origin/main)
Writes {results file: [VA...]} to FILE and prints one count line per set; driver.py --check --select FILE honours it.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
LAYER = "src/pes/translated"
# The committed records a ctest diff_*_recorded test re-checks (tests/pilot/CMakeLists.txt).
SETS = ("results.json", "results_cutscene.json", "results_presentation.json", "results_core.json",
        "results_impure.json")
# Changes that can move any function's proof without changing a generated file: the harness and the runtime headers the
# generated code includes.
GLOBAL = frozenset("tools/dev/translate/" + n for n in (
    "stubs.py", "stubs.json", "diffmain.py", "diffparse.hpp")) | frozenset(
    "tools/dev/diffharness/" + n for n in ("run.py", "bankstore.py", "snapcase.py")) | frozenset(
    LAYER + "/" + n for n in ("pes_state.hpp", "pes_stubs.hpp", "pes_cond.inl", "pes_dispatch.hpp"))
FN_FILE = re.compile(r"^%s/[^/]+/pes_fn_([0-9A-Fa-f]+)\.cpp$" % LAYER)
SPEC_FILE = re.compile(r"^tools/dev/diffharness/specs/[^/]+/([0-9A-Fa-f]+)\.json$")
CALLEE = re.compile(r"pes_fn_([0-9A-Fa-f]+)\(")


def layer_graph(root):
    """{VA (no 0x, upper): {direct callee VAs}} over every committed generated function."""
    graph = {}
    for f in glob.glob(os.path.join(root, LAYER, "*", "pes_fn_*.cpp")):
        va = FN_FILE.match(os.path.relpath(f, root)).group(1).upper()
        with open(f, encoding="utf8") as fh:
            graph[va] = {c.upper() for c in CALLEE.findall(fh.read())} - {va}
    return graph


def layer_path(root, va):
    return os.path.relpath(glob.glob(os.path.join(root, LAYER, "*", "pes_fn_%s.cpp" % va))[0], root)


def closure(graph, va):
    seen, queue = {va}, [va]
    while queue:
        for c in graph.get(queue.pop(), ()):
            if c not in seen:
                seen.add(c)
                queue.append(c)
    return seen


def changed_vas(paths):
    """VAs (no 0x, upper) whose generated file or spec is among paths."""
    out = set()
    for p in paths:
        m = FN_FILE.match(p) or SPEC_FILE.match(p)
        if m:
            out.add(m.group(1).upper())
    return out


def checked(rec):
    """Re-checked by driver --check: diffed, and not proven from real calls (snapdiff.py)."""
    return bool(rec.get("diffed")) and rec.get("proof") != "snapdiff"


def load_results(root, rev=None):
    """{results file: {VA: record}}; rev=None reads the working tree, else `git show rev:path` (a file new since rev is {})."""
    out = {}
    for name in SETS:
        rel = "tools/dev/translate/" + name
        if rev is None:
            with open(os.path.join(root, rel), encoding="utf8") as f:
                text = f.read()
        else:
            r = subprocess.run(["git", "-C", root, "show", "%s:%s" % (rev, rel)], capture_output=True, text=True)
            text = r.stdout if r.returncode == 0 else '{"results": []}'
        out[name] = {"0x%X" % int(r["va"], 16): r for r in json.loads(text)["results"]}
    return out


def select(root, paths, base, now):
    """{results file: sorted [0x VA]} of the recorded functions to re-check, given the changed paths and the records
    at the base and now."""
    graph = layer_graph(root)
    everything = any(p in GLOBAL for p in paths)
    changed = changed_vas(paths)
    out = {}
    for name, recs in now.items():
        old = base.get(name, {})
        out[name] = sorted(va for va, r in recs.items() if checked(r) and (
            everything or r != old.get(va) or closure(graph, va[2:].upper()) & changed))
    return out


def git(*args):
    return subprocess.run(["git", "-C", REPO, *args], capture_output=True, text=True, check=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=None)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    base = args.base or git("merge-base", "HEAD", "origin/main").strip()
    paths = git("diff", "--no-renames", "--name-only", base, "--").split() + \
        git("ls-files", "--others", "--exclude-standard").split()
    now = load_results(REPO)
    out = select(REPO, paths, load_results(REPO, base), now)
    print("select_recorded: base %s, %d changed paths" % (base[:12], len(paths)))
    for name, vas in out.items():
        print("  %-26s %5d of %5d recorded selected" % (name, len(vas), sum(checked(r) for r in now[name].values())))
    if args.out:
        with open(args.out, "w") as f:
            json.dump(out, f, indent=1, sort_keys=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
