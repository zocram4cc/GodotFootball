#!/usr/bin/env python3
"""Row status joins: decode verdicts per VA from docs/re, port sites from src.

Reads docs/re/*.md + src/ (+ diff spec VAs passed in as JSON). Writes
tools/dev/ledger/joins.json (committed evidence mirror). No hand edits:
classpath functions (render-only, no match tick) and other out-of-scope
items belong in hand.json, not here.
Usage: python3 tools/dev/ledger/joins.py [--diff specs.json]
"""
import json
import os
import re
import sys
REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
DOCS = os.path.join(REPO, "docs", "re")
SRC = os.path.join(REPO, "src")

VA = r"(?:(?<=0x)|(?<=FUN_)|(?<=sub_))140([0-9A-Fa-f]{6})(?![0-9A-Fa-f])"
VERDICT = re.compile(r"\*\*(CONFIRMED|INF|DOC|OBS|OPEN)\b(\s*\([hml]\))?")


def norm(m):
    return "140%s" % m.group(1).upper()


def doc_verdicts():
    """va -> {verdict: best label seen adjacent, cites: [doc]}."""
    rank = {"CONFIRMED": 3, "OBS": 3, "DOC": 3, "INF": 2, "OPEN": 1}
    out = {}
    for fn in sorted(os.listdir(DOCS)):
        if not fn.endswith(".md") or fn == "LEDGER.md":
            continue
        text = open(os.path.join(DOCS, fn), encoding="utf-8",
                     errors="replace").read()
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


def src_sites():
    """va -> {files: [...], name: best-effort GF symbol nearby}."""
    out = {}
    for root, _, files in os.walk(SRC):
        for fn in files:
            if not fn.endswith((".cpp", ".hpp", ".h")):
                continue
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, REPO)
            try:
                lines = open(p, encoding="utf-8", errors="replace").readlines()
            except OSError:
                continue
            for i, line in enumerate(lines):
                for m in re.finditer(VA, line):
                    va = norm(m)
                    e = out.setdefault(va, {"files": [], "name": None})
                    if rel not in e["files"]:
                        e["files"].append(rel)
                    if not e["name"]:
                        for j in range(i, max(-1, i - 6), -1):
                            nm = re.search(
                                r"(?:^|[\s:\[])([A-Za-z_][\w:]*)\s*\(", lines[j])
                            if nm and "if" not in nm.group(1):
                                e["name"] = nm.group(1)[-64:]
                                break
    return out


def ponytails():
    """va -> {file, line} for named // ponytail: PES-site stand-ins."""
    out = {}
    for root, _, files in os.walk(SRC):
        for fn in files:
            if not fn.endswith((".cpp", ".hpp", ".h")):
                continue
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, REPO)
            try:
                lines = open(p, encoding="utf-8", errors="replace").readlines()
            except OSError:
                continue
            for i, line in enumerate(lines, 1):
                if "ponytail:" not in line:
                    continue
                for m in re.finditer(VA, line):
                    out.setdefault(norm(m), []).append("%s:%d" % (rel, i))
    return out


def main():
    diff_vas = {}
    if "--diff" in sys.argv:
        spec = json.load(open(sys.argv[sys.argv.index("--diff") + 1]))
        for va, src in spec.items():
            diff_vas[va] = src
    funclist = set()
    if "--funclist" in sys.argv:
        txt = open(sys.argv[sys.argv.index("--funclist") + 1],
                   encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"FUN_[0-9a-fA-F]+ at ([0-9a-fA-F]+)", txt):
            funclist.add(m.group(1).upper())
    joins = {"decode": doc_verdicts(), "ports": src_sites(),
             "standins": ponytails(), "diffed": diff_vas,
             "funclist": sorted(funclist)}
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "joins.json")
    json.dump(joins, open(out, "w"), indent=1, sort_keys=True)
    print("wrote %s: decode=%d ports=%d standins=%d diffed=%d funclist=%d"
          % (out, len(joins["decode"]), len(joins["ports"]),
             len(joins["standins"]), len(joins["diffed"]), len(funclist)))


if __name__ == "__main__":
    main()
