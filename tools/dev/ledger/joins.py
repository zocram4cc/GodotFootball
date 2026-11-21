#!/usr/bin/env python3
"""Row status joins for one checkout: decode verdicts, port sites, stand-ins, recorded proofs, committed layer.

Reads docs/re/*.md, src/, every tools/dev/translate/results*.json (a record is recorded bit-exact when it is
diffed or proven) and the committed translated layer src/pes/translated (closure/ included). Writes
tools/dev/ledger/joins.json (committed evidence mirror). No hand edits.

A bare run needs no flags: it keeps the committed bridge function list and the committed diff-spec map (the
entries whose source is not a results file). --funclist replaces the list; --diff adds spec VAs.
Usage: python3 tools/dev/ledger/joins.py [--diff specs.json] [--funclist list_functions.txt]
"""
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "joins.json")
RESULTS_PREFIX = "tools/dev/translate/"

# Image VAs 0x140000000..0x144FFFFFF (the universe's code prefixes); every VA a doc or source cites.
VA = r"(?:(?<=0x)|(?<=FUN_)|(?<=sub_))(14[0-4][0-9A-Fa-f]{6})(?![0-9A-Fa-f])"
VERDICT = re.compile(r"\*\*(CONFIRMED|INF|DOC|OBS|OPEN)\b(\s*\([hml]\))?")
RESULTS_FILE = re.compile(r"results(_\w+)?\.json")
LAYER_FILE = re.compile(r"pes_fn_([0-9A-Fa-f]+)\.cpp")


def norm(m):
    return m.group(1).upper()


def doc_verdicts(repo):
    """va -> {verdict, cites: [doc]}."""
    docs = os.path.join(repo, "docs", "re")
    rank = {"CONFIRMED": 3, "OBS": 3, "DOC": 3, "INF": 2, "OPEN": 1}
    out = {}
    for fn in sorted(os.listdir(docs)):
        if not fn.endswith(".md") or fn == "LEDGER.md":
            continue
        with open(os.path.join(docs, fn), encoding="utf-8", errors="replace") as f:
            text = f.read()
        tags = [t[0] for t in VERDICT.findall(text[:3000])]
        for m in re.finditer(VA, text):
            va = norm(m)
            lo = max(0, m.start() - 200)
            vm = VERDICT.search(text[lo:m.start() + 300])
            if not vm:
                vm = VERDICT.search(text[m.start():m.start() + 300])
            label = vm.group(1) if vm else None
            if not label and tags:
                label = tags[0]  # doc's own default evidence grade
            e = out.setdefault(va, {"verdict": None, "rank": 0, "cites": []})
            if label and rank[label] > e["rank"]:
                e["verdict"], e["rank"] = label, rank[label]
            if fn not in e["cites"]:
                e["cites"].append(fn)
                e["cites"].sort()
            if len(e["cites"]) > 8:
                e["cites"] = e["cites"][:8]
    return {va: {"verdict": e["verdict"], "cites": e["cites"]}
            for va, e in out.items()}


LAYER_DIR = os.path.join("src", "pes", "translated")


def source_files(repo):
    """(repo-relative path, lines) for every C++ source under src/ except the committed translated layer: its banners
    name the PES function they translate (provenance), which is not a GF port."""
    for root, _, files in os.walk(os.path.join(repo, "src")):
        if os.path.relpath(root, repo).startswith(LAYER_DIR):
            continue
        for fn in files:
            if not fn.endswith((".cpp", ".hpp", ".h")):
                continue
            p = os.path.join(root, fn)
            try:
                with open(p, encoding="utf-8", errors="replace") as f:
                    lines = f.readlines()
            except OSError:
                continue
            yield os.path.relpath(p, repo), lines


def src_sites(repo):
    """va -> {files: [...], name: best-effort GF symbol nearby}."""
    out = {}
    for rel, lines in source_files(repo):
        for i, line in enumerate(lines):
            for m in re.finditer(VA, line):
                va = norm(m)
                e = out.setdefault(va, {"files": [], "name": None})
                if rel not in e["files"]:
                    e["files"].append(rel)
                if not e["name"]:
                    for j in range(i, max(-1, i - 6), -1):
                        nm = re.search(
                            r"(?:^|[:\[\s])([A-Za-z_][\w:]*)\s*\(", lines[j])
                        if nm and "if" not in nm.group(1):
                            e["name"] = nm.group(1)[-64:]
                            break
    return out


def ponytails(repo):
    """va -> [file:line] for named // ponytail: PES-site stand-ins."""
    out = {}
    for rel, lines in source_files(repo):
        for i, line in enumerate(lines, 1):
            if "ponytail:" not in line:
                continue
            for m in re.finditer(VA, line):
                out.setdefault(norm(m), []).append("%s:%d" % (rel, i))
    return out


def recorded(repo):
    """va -> {source, synthetic} for every record a results set proves bit-exact (diffed or proven). A VA with
    any real record is real; it is synthetic only when every record of it is synthetic."""
    tdir = os.path.join(repo, "tools", "dev", "translate")
    out = {}
    for fn in sorted(os.listdir(tdir)):
        if not RESULTS_FILE.fullmatch(fn):
            continue
        with open(os.path.join(tdir, fn)) as f:
            rows = json.load(f).get("results", [])
        for r in rows:
            if not (r.get("diffed") or r.get("proven")):
                continue
            va = r["va"].upper().replace("0X", "")
            syn = bool(r.get("synthetic"))
            e = out.get(va)
            if e is None or (e["synthetic"] and not syn):
                out[va] = {"source": RESULTS_PREFIX + fn, "synthetic": syn}
    return out


def committed(repo):
    """va -> repo-relative path of each pes_fn_<VA>.cpp in the committed translated layer. A record file wins over a
    closure copy of the same VA."""
    out = {}
    for root, _, files in os.walk(os.path.join(repo, "src", "pes", "translated")):
        for fn in files:
            m = LAYER_FILE.fullmatch(fn)
            if not m:
                continue
            va, rel = m.group(1).upper(), os.path.relpath(os.path.join(root, fn), repo)
            if va not in out or "/closure/" in out[va]:
                out[va] = rel
    return out


def build(repo, carried=None, diff_map=None, funclist=None):
    """All joins for one checkout. carried: the committed joins.json (its bridge list and spec entries are kept
    unless the caller passes new ones)."""
    carried = carried or {}
    spec = {va: src for va, src in carried.get("diffed", {}).items() if not src.startswith(RESULTS_PREFIX)}
    spec.update(diff_map or {})
    rec = recorded(repo)
    diffed = {va: e["source"] for va, e in rec.items()}
    diffed.update(spec)  # a spec-map entry names its own proof
    return {
        "decode": doc_verdicts(repo),
        "ports": src_sites(repo),
        "standins": ponytails(repo),
        "diffed": diffed,
        "synthetic": sorted(va for va, e in rec.items() if e["synthetic"] and va not in spec),
        "translated": committed(repo),
        "funclist": sorted(funclist if funclist is not None else carried.get("funclist", [])),
    }


def main():
    carried = None
    if os.path.exists(OUT):
        with open(OUT) as f:
            carried = json.load(f)
    diff_map = {}
    if "--diff" in sys.argv:
        with open(sys.argv[sys.argv.index("--diff") + 1]) as f:
            diff_map = json.load(f)
    funclist = None
    if "--funclist" in sys.argv:
        txt = open(sys.argv[sys.argv.index("--funclist") + 1], encoding="utf-8", errors="replace").read()
        funclist = sorted({m.group(2).upper() for m in
                           re.finditer(r"^(?!\s)(.+?) at ([0-9a-fA-F]+)$", txt, re.M)})
    joins = build(REPO, carried, diff_map, funclist)
    with open(OUT, "w") as f:
        json.dump(joins, f, indent=1, sort_keys=True)
    print("wrote %s: decode=%d ports=%d standins=%d diffed=%d (synthetic %d) translated=%d funclist=%d"
          % (OUT, len(joins["decode"]), len(joins["ports"]), len(joins["standins"]), len(joins["diffed"]),
             len(joins["synthetic"]), len(joins["translated"]), len(joins["funclist"])))


if __name__ == "__main__":
    main()
