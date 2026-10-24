#!/usr/bin/env python3
"""Ledger generator: joins universe + decode/ports/standins/diff + hand list.

Writes docs/re/LEDGER.md with a sha256 stamp of its own generated content.
Usage: python3 tools/dev/ledger.py [--check]  (--check exits 1 if stale)
"""
import hashlib
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
LEDGER = os.path.join(REPO, "tools", "dev", "ledger")
LEDGER_MD = os.path.join(REPO, "docs", "re", "LEDGER.md")

STATUS_ORDER = ["ported", "diffed", "stand-in", "decoded", "leaf accessor",
                "engine", "undecoded", "inert", "out of scope"]
# Regression rank. Moving to engine or leaf is a reclassification, not lost knowledge.
RANK = {"ported": 0, "diffed": 1, "stand-in": 2, "decoded": 3,
        "leaf accessor": 3, "engine": 3, "undecoded": 4, "inert": 5,
        "out of scope": 6}
# Boundary rules as the universe script applies them (universe.py module doc, meta.rules).
RULES = [
    ("library", "a Ghidra symbol that names CRT, STL, zlib, libxml or Winsock code; "
                "no symbol in the bridge's list is in a match:: namespace"),
    ("doc", "a report names the VA as the engine side of a call (phrase, then the VA, same line)"),
    ("rtti", "a vtable a walked function references has an engine RTTI owner "
             "(fox, cobra, KS, gr, draw, menu, flow, std, collision, or a template)"),
    ("crt", "a FUN_ function inside the address span of the CRT's named symbols"),
]


def load(name):
    with open(os.path.join(LEDGER, name)) as f:
        return json.load(f)


def classify(va, f, joins, hand):
    if va in hand:
        return hand[va]["status"]
    if va in joins["standins"]:
        return "stand-in"
    if joins["ports"].get(va, {}).get("files"):
        return "ported"
    if va in joins["diffed"]:
        return "diffed"
    if f.get("boundary"):
        return "engine"
    if f.get("leaf"):
        return "leaf accessor"
    dec = joins["decode"].get(va, {}).get("verdict")
    if dec in ("CONFIRMED", "OBS", "DOC", "INF"):
        return "decoded"
    return "undecoded"


def vkey(v):
    v = v.upper()
    if v.startswith("0X"):
        v = v[2:]
    return v


def table(L, head, rows):
    L.append("| " + " | ".join(head) + " |")
    L.append("|" + "---|" * len(head))
    for r in rows:
        L.append("| " + " | ".join(str(c) for c in r) + " |")
    L.append("")


def main():
    check = "--check" in sys.argv
    uni = load("universe.json")
    joins = load("joins.json")
    hand = load("hand.json")
    hand_map = {vkey(e["va"]): e for e in hand["entries"]}
    rows = []
    for va, f in uni["functions"].items():
        status = classify(va, f, joins, hand_map)
        dec = joins["decode"].get(va, {})
        rows.append({
            "va": "0x" + va,
            "name": f.get("name", "FUN_" + va),
            "sub": f.get("subsystem", "?"),
            "status": status,
            "rule": (f.get("boundary") or {}).get("rule", "-"),
            "verdict": dec.get("verdict") or "-",
            "cite": (dec.get("cites") or ["-"])[0],
            "port": ";".join(joins["ports"].get(va, {}).get("files", [])) or "-",
            "callers": 0,  # filled below
            "size": f.get("size", 0),
        })
    by_va = {r["va"][2:]: r for r in rows}
    for va, f in uni["functions"].items():
        for t in (f.get("callees") or {}):
            if t in by_va:
                by_va[t]["callers"] += 1

    counts = {}
    for r in rows:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    subs = {}
    for r in rows:
        subs.setdefault(r["sub"], []).append(r)
    order = ["TeamTick", "Player", "Ball", "Referee", "Restart",
             "MatchTick", "?"]
    meta = uni.get("meta", {})
    unresolved = [r for r in rows if uni["functions"][r["va"][2:]].get("unresolved")]
    protected = [r for r in unresolved if uni["functions"][r["va"][2:]].get("protected")]

    L = []
    L.append("# Match-path ledger (generated — do not edit by hand)")
    L.append("")
    L.append("Regenerate: `python3 tools/dev/ledger.py`. Enforced by `tools/dev/gate.sh`. "
             "Inputs: `tools/dev/ledger/` (`universe.py` + `universe.json` (bridge crawl, "
             "`--image` for the vtable and jump-table rules), `joins.py` + `joins.json` "
             "(`--funclist` from the bridge's function list), `hand.json`, `offpath_allow.json`).")
    L.append("")
    L.append("Statuses: **ported** / **diffed** / **stand-in** (GF code cites the VA), **decoded** "
             "(a report gives a verdict), **leaf accessor** (<= %s instructions, no call, no jump "
             "out, no vtable reference), **engine** (boundary: not walked, not match logic), "
             "**undecoded** (walked or unresolved, no verdict), **inert** / **out of scope** "
             "(hand.json)." % meta.get("leaf_max_insns", "?"))
    L.append("")
    L.append("## Summary")
    L.append("")
    L.append("| status | rows |")
    L.append("|---|---|")
    L.append("| total | %d |" % len(rows))
    for s in STATUS_ORDER:
        L.append("| %s | %d |" % (s, counts.get(s, 0)))
    for s in sorted(counts):
        if s not in STATUS_ORDER:
            L.append("| %s | %d |" % (s, counts[s]))
    L.append("")
    L.append("## Boundary and leaf rules")
    L.append("")
    rule_rows = []
    for name, text in RULES:
        rule_rows.append((name, sum(1 for r in rows if r["rule"] == name), text))
    table(L, ["rule", "rows", "what it cuts"], rule_rows)
    table(L, ["unwalked", "rows", "note"], [
        ("leaf accessor", counts.get("leaf accessor", 0),
         "walked; trivially read by the port, counted apart from the gap"),
        ("unresolved (bridge has no body)", len(unresolved),
         "%d in the protected import area (.impdata, no code on the bridge); "
         "the rest have no symbol the bridge can disassemble" % len(protected)),
    ])
    L.append("Crawl counters (universe.py meta.rules): " + ", ".join(
        "%s %s" % (k, v) for k, v in sorted((meta.get("rules") or {}).items())) + ".")
    L.append("")
    for sub in order:
        if sub not in subs:
            continue
        rs = sorted(subs[sub], key=lambda r: r["va"])
        L.append("## %s (%d)" % (sub, len(rs)))
        L.append("")
        L.append("| VA | name | status | decode | cite | port | callers | size | path |")
        L.append("|---|---|---|---|---|---|---|---|---|")
        for r in rs:
            f = uni["functions"][r["va"][2:]]
            path = ">".join((f.get("path") or [])[-3:])
            L.append("| %s | %s | %s | %s | %s | %s | %d | %d | %s |"
                     % (r["va"], r["name"], r["status"], r["verdict"],
                        r["cite"], r["port"], r["callers"], r["size"], path))
        L.append("")

    gap = [r for r in rows if r["status"] in ("undecoded", "stand-in")]
    by_callers = sorted(gap, key=lambda r: (-r["callers"], -r["size"]))[:10]
    by_size = sorted((r for r in gap if r["size"] > 0),
                     key=lambda r: (-r["size"], -r["callers"]))[:10]
    gap_head = ["VA", "name", "status", "callers", "size", "sub"]
    gap_rows = lambda rs: [(r["va"], r["name"], r["status"], r["callers"],
                            r["size"], r["sub"]) for r in rs]
    L.append("## Gap list: top 10 undecoded / stand-in by callers, then size")
    L.append("")
    table(L, gap_head, gap_rows(by_callers))
    L.append("## Gap list: top 10 undecoded / stand-in by size, then callers")
    L.append("")
    table(L, gap_head, gap_rows(by_size))
    body = "\n".join(L) + "\n"
    stamp = hashlib.sha256(body.encode()).hexdigest()[:16]
    out = ("<!-- LEDGER-STAMP %s -->\n" % stamp) + body
    if check:
        import re as _re
        try:
            cur = open(LEDGER_MD, encoding="utf-8").read()
        except OSError:
            print("LEDGER.md missing")
            return 1
        m = _re.match(r"<!-- LEDGER-STAMP ([0-9a-f]+) -->\n(.*)$",
                      cur, _re.S)
        if not m or m.group(1) != stamp or m.group(2) != body:
            print("LEDGER.md stamp missing/stale or content differs "
                  "(hand edit or stale copy): regenerate with "
                  "tools/dev/ledger.py")
            return 1
        old_rows = {}
        for line in cur.splitlines():
            if line.startswith("| 0x140"):
                cells = [c.strip() for c in line.split("|")]
                old_rows[cells[1][2:]] = cells[3]
        new_rows = {r["va"][2:]: r["status"] for r in rows}
        regressed = sorted(
            va for va in old_rows if va in new_rows
            and RANK.get(new_rows[va], 99) > RANK.get(old_rows[va], 99))
        if regressed:
            print("ledger regressed: %s" % ", ".join(regressed[:20]))
            return 1
        funclist = set(joins.get("funclist") or [])
        offpath = sorted(v for v in set(joins["ports"]) - set(uni["functions"])
                         if v in funclist)
        allowed = set(vkey(e["va"]) for e in
                      (load("offpath_allow.json").get("entries", [])))
        unknown = [v for v in offpath if v not in allowed]
        if unknown:
            print("src ports %d real function(s) outside the universe AND "
                  "the allow list (off the match path, or typo'd VAs): %s"
                  % (len(unknown), ", ".join("0x" + v for v in unknown[:20])))
            return 1
        print("off-path ports: %d (all allow-listed)" % len(offpath))
        print("LEDGER.md current (%d rows)" % len(rows))
        return 0
    open(LEDGER_MD, "w").write(out)
    print("wrote %s: %d rows, stamp %s" % (LEDGER_MD, len(rows), stamp))
    for s in STATUS_ORDER:
        print("  %-14s %d" % (s, counts.get(s, 0)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
