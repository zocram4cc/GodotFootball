#!/usr/bin/env python3
"""branch_select.py: which ctest tests the GATE_TIER=branch run executes (tools/dev/README.md, "Gate tiers").

A test runs when one of its inputs changed since the base. Its inputs are:
  - its binary's build closure (`ninja -t inputs`: sources, headers and every static library object it links). A change to
    any object of a linked archive selects every binary linking it, so a sim source selects every gfsim binary: the seeded
    match families among them, whose matches run the same sim code. A pesfmt-only binary is not selected by a sim source.
  - the build outputs its command names (gfsim_run for gfsim_run_test): their closures;
  - the repository files its command names. For a script or data file, every file under its directory, recursively,
    unless that directory is a root (tools/, tests/, src/, apps/, godot/, assets/, extern/), which contributes only the file.
    `res://x` names godot/x, and a godot test (tools/godot.sh) adds the extension's closure;
  - the EXTRA inputs below, which the command does not name.
Always run: repo_pii_scan and the five diff_*_recorded checks (their GF_RECORDED_SELECT decides what they re-check).
A FIXTURES_REQUIRED setup test runs with the test that needs it.

Fallbacks, conservative:
  - a change under assets/ selects every test (tests read sim data through GF_SOURCE_DIR, which their commands do not name);
  - a changed CMake line naming a test (its ctest name, a binary or a script it runs) selects that test; a changed CMake line
    naming no test (a flag, an include, a subdirectory, a variable) selects every test, as does a changed CMake file with no
    lines (a new file).
Ceiling: closures are static-library granular, and a file read at run time through a path no command names is covered only
by the fallbacks above.

Usage: branch_select.py --build DIR [--repo DIR] (--base REV | --diff FILE) [--index | --list]
  --base REV: the diff is `git diff -U0 --no-renames REV` over tracked files. Untracked files are not branch changes: test runs
    leave some (Godot's .uid files), and a committed branch carries its new files in the diff.
  --index: print the ctest -I argument that runs the selection (ctest's test numbers); --list: one name per line; default:
    the reasons on stderr.
"""
import argparse
import functools
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_DEFAULT = os.path.dirname(os.path.dirname(HERE))
ROOTS = frozenset({"", "tools", "tests", "src", "apps", "godot", "assets", "extern"})
ALWAYS = re.compile(r"^(repo_pii_scan|diff_(core|cutscene|presentation|pilot|impure)_recorded)$")
EXTRA = {  # inputs a test reads without naming them
    "diff_translated_drift": ("src/pes/translated",),
    "parity_metrics_test": ("tools/dev/parity.py",),
}
GODOT_EXT = "godot/bin/libgodotfootball.so"  # the extension the godot tests load (src/gdext, godotfootball target)


def under(path, root):
    return path == root or path.startswith(root + os.sep)


@functools.lru_cache(maxsize=None)
def files_under(repo, rel):
    out = set()
    for root, dirs, files in os.walk(os.path.join(repo, rel)):
        dirs[:] = [d for d in dirs if d != "__pycache__"]
        for f in files:
            out.add(os.path.relpath(os.path.join(root, f), repo))
    return frozenset(out)


def command_tokens(command):
    for element in command:
        yield from re.findall(r"[^\s'\";&|()]+", element)


@functools.lru_cache(maxsize=None)
def closure(build, repo, target):
    """Repository-relative inputs of a build output (target is relative to the build directory)."""
    out = subprocess.run(["ninja", "-C", build, "-t", "inputs", target], capture_output=True, text=True)
    if out.returncode != 0:
        return frozenset()
    inputs = set()
    for line in out.stdout.splitlines():
        if line.strip():
            path = os.path.normpath(os.path.join(build, line.strip()))
            if under(path, repo):
                inputs.add(os.path.relpath(path, repo))
    return frozenset(inputs)


def as_list(value):
    """A ctest property as a list: CMake lists come back as JSON arrays, a single value as a string."""
    if value is None:
        return []
    if isinstance(value, list):
        return [str(v) for v in value]
    return [v for v in str(value).split(";") if v]


@functools.lru_cache(maxsize=None)
def ctest_tests(build):
    data = json.loads(subprocess.run(["ctest", "--test-dir", build, "--show-only=json-v1"],
                                     capture_output=True, text=True, check=True).stdout)
    tests = []
    for t in data["tests"]:
        props = {p["name"]: p["value"] for p in t.get("properties", [])}
        tests.append({"name": t["name"], "command": t.get("command", []),
                      "setup": as_list(props.get("FIXTURES_SETUP")),
                      "requires": as_list(props.get("FIXTURES_REQUIRED"))})
    return tests


class Test:
    def __init__(self, build, repo, raw):
        self.name, self.setup, self.requires = raw["name"], raw["setup"], raw["requires"]
        self.files, self.targets, self.names = set(), set(), {self.name}
        godot = False
        for tok in command_tokens(raw["command"]):
            if tok.startswith("res://"):
                self.files.add("godot/" + tok[len("res://"):])
                continue
            if not os.path.isabs(tok):
                continue
            tok = os.path.normpath(tok)
            if under(tok, build) and os.path.isfile(tok):
                self.targets.add(os.path.relpath(tok, build))
                self.names.add(os.path.basename(tok))
            elif under(tok, repo):
                rel = os.path.relpath(tok, repo)
                self.names.add(os.path.basename(tok))
                if rel == "":  # the repository root (gfsim_run_test's argument) adds nothing: its inputs are named elsewhere
                    continue
                if os.path.isdir(tok):
                    self.files |= files_under(repo, rel)
                    continue
                self.files.add(rel)
                if rel == "tools/godot.sh":
                    godot = True
                parent = os.path.dirname(rel)
                if parent not in ROOTS:
                    self.files |= files_under(repo, parent)
        if godot:
            self.targets.add(os.path.relpath(os.path.join(repo, GODOT_EXT), build))
        for rel in EXTRA.get(self.name, ()):
            self.files |= files_under(repo, rel) if os.path.isdir(os.path.join(repo, rel)) else {rel}

    def inputs(self, build, repo):
        out = set(self.files)
        for target in self.targets:
            out |= closure(build, repo, target)
        return out


def is_cmake(path):
    return os.path.basename(path) == "CMakeLists.txt" or path.endswith(".cmake")


def parse_diff(text):
    """({changed repository paths}, {CMake path: [changed line text]}) from a unified diff."""
    changed, cmake, current = set(), {}, None
    for line in text.splitlines():
        if line.startswith("diff --git "):
            m = re.match(r"diff --git a/(.*) b/(.*)$", line)
            current = m.group(2) if m else None
            if current:
                changed.add(current)
                if is_cmake(current):
                    cmake.setdefault(current, [])
        elif current and is_cmake(current) and line[:1] in ("+", "-") and not line.startswith(("+++ ", "--- ")):
            cmake[current].append(line[1:])
    return changed, cmake


def significant(line):
    s = line.strip()
    return bool(s) and not s.startswith("#")


def words(line):
    return set(re.findall(r"[A-Za-z0-9_.+\-]+", line))


def select(build, repo, diff_text):
    """{ctest name: [reasons]} for the tests the branch tier runs. build and repo are absolute paths."""
    changed, cmake = parse_diff(diff_text)
    tests = [Test(build, repo, raw) for raw in ctest_tests(build)]
    everything = {t.name: ["fallback: every test"] for t in tests}
    if any(under(p, "assets") for p in changed):
        return everything
    if any(not lines for lines in cmake.values()):
        return everything
    cmake_lines = [l for lines in cmake.values() for l in lines if significant(l)]
    all_names = set().union(*(t.names for t in tests))
    if any(not (words(l) & all_names) for l in cmake_lines):
        return everything
    selected = {}
    for t in tests:
        why = []
        if ALWAYS.match(t.name):
            why.append("always-on")
        if any(words(l) & t.names for l in cmake_lines):
            why.append("CMake line names it")
        hits = sorted(t.inputs(build, repo) & changed)
        if hits:
            why.append("changed: " + ", ".join(hits[:3]) + (" ..." if len(hits) > 3 else ""))
        if why:
            selected[t.name] = why
    setup_of = {s: t.name for t in tests for s in t.setup}
    for t in tests:
        if t.name in selected:
            for fixture in t.requires:
                if fixture in setup_of:
                    selected.setdefault(setup_of[fixture], ["fixture of " + t.name])
    return selected


def git_diff(repo, base):
    return subprocess.run(["git", "-C", repo, "diff", "-U0", "--no-renames", "--ignore-submodules=all", base, "--"],
                          capture_output=True, text=True, check=True).stdout


@functools.lru_cache(maxsize=None)
def ctest_numbers(build):
    """{ctest name: test number} as `ctest -N` numbers the built tests: the numbers -I takes."""
    out = subprocess.run(["ctest", "--test-dir", build, "-N"], capture_output=True, text=True, check=True).stdout
    return {m.group(2): int(m.group(1)) for m in re.finditer(r"Test\s+#(\d+): (\S+)", out)}


def ctest_index(build, names):
    """The -I argument that runs exactly `names` under ctest. A list of test numbers, not a name regex: a -R over 2,311
    names overflows ctest's regex engine ("Expression too big") and runs nothing (05-11-26). Numbers, not names, keep the
    argument far under the kernel's 128 KiB limit on one argument."""
    if not names:
        raise ValueError("branch select: the selection is empty")
    numbers = ctest_numbers(build)
    missing = sorted(n for n in names if n not in numbers)
    if missing:
        raise ValueError(f"branch select: not in ctest's list (built?): {', '.join(missing[:5])}")
    return "0,0,0," + ",".join(str(numbers[n]) for n in sorted(names, key=numbers.get))


def main():
    ap = argparse.ArgumentParser(description="The ctest tests a GATE_TIER=branch run executes.")
    ap.add_argument("--build", required=True)
    ap.add_argument("--repo", default=REPO_DEFAULT)
    source = ap.add_mutually_exclusive_group(required=True)
    source.add_argument("--base")
    source.add_argument("--diff")
    shape = ap.add_mutually_exclusive_group()
    shape.add_argument("--index", action="store_true")
    shape.add_argument("--list", action="store_true")
    args = ap.parse_args()
    build, repo = os.path.abspath(args.build), os.path.abspath(args.repo)
    text = open(args.diff).read() if args.diff else git_diff(repo, args.base)
    selected = select(build, repo, text)
    if args.index:
        print(ctest_index(build, selected))
    elif args.list:
        print("\n".join(sorted(selected)))
    else:
        for name in sorted(selected):
            print(f"{name}: {'; '.join(selected[name])}", file=sys.stderr)
    print(f"branch select: {len(selected)} tests", file=sys.stderr)


if __name__ == "__main__":
    main()
