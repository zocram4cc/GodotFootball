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

STATUS_ORDER = ["ported", "diffed", "stand-in", "decoded", "undecoded",
                "inert", "out of scope"]


def load(name):
    with open(os.path.join(LEDGER, name)) as f:
        return json.load(f)


def classify(va, uni, joins, hand):
    if va in hand:
        return hand[va]["status"]
    ports = joins["ports"].get(va, {}).get("files", [])
    if va in joins["standins"]:
        return "stand-in"
    if ports:
        return "ported"
    if va in joins["diffed"]:
        return "diffed"
    if dec in ("CONFIRMED", "OBS", "DOC", "INF"):
        return "decoded"
    return "undecoded"


def vkey(v):
    v = v.upper()
    for p in ("0X", "FUN_", "SUB_"):
        if v.startswith(p):
            v = v[len(p):]
    return v


def main():
    check = "--check" in sys.argv
    uni = load("universe.json")
    joins = load("joins.json")
    hand = load("hand.json")
    hand_map = {vkey(e["va"]): e for e in hand["entries"]}
    for va, f in uni["functions"].items():
        status = classify(va, uni, joins, hand_map)
        dec = joins["decode"].get(va, {})
        rows.append({
            "va": "0x" + va,
            "name": f.get("name", "FUN_" + va),
            "sub": f.get("subsystem", "?"),
            "status": status,
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

    L = []
    L.append("# Match-path ledger (generated — do not edit by hand)")
    L.append("")
    L.append("Regenerate: `python3 tools/dev/ledger.py`. "
             "Enforced by `tools/dev/gate.sh`. Inputs: `tools/dev/ledger/` "
             "(`universe.py` + `universe.json`, `joins.py` + `joins.json`, "
             "`hand.json`).")
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
    gap.sort(key=lambda r: (-r["callers"], -r["size"]))
    L.append("## Gap list: top 10 undecoded / stand-in by callers, then size")
    L.append("")
    L.append("| VA | name | status | callers | size | sub |")
    L.append("|---|---|---|---|---|---|")
    for r in gap[:10]:
        L.append("| %s | %s | %s | %d | %d | %s |"
                 % (r["va"], r["name"], r["status"], r["callers"],
                    r["size"], r["sub"]))
    L.append("")
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
        m = _re.match(r"<!-- LEDGER-STAMP ([0-9a-f]+) -->\n", cur)
        if not m or m.group(1) != stamp:
            print("LEDGER.md stamp missing or stale (hand edit or stale "
                  "copy): regenerate with tools/dev/ledger.py")
            return 1
        if _re.sub(r"<!--[^-]*-->\n", "", cur) != body:
            print("LEDGER.md content differs from generated output "
                  "(hand edit): regenerate with tools/dev/ledger.py")
            return 1
        old_rows = {}
        for line in cur.splitlines():
            if line.startswith("| 0x140"):
                cells = [c.strip() for c in line.split("|")]
                old_rows[cells[1][2:]] = cells[3]
        new_rows = {r["va"][2:]: r["status"] for r in rows}
        rank = {s: i for i, s in enumerate(STATUS_ORDER)}
        regressed = sorted(
            va for va in old_rows if va in new_rows
            and rank.get(new_rows[va], 99) > rank.get(old_rows[va], 99))
        if regressed:
            print("ledger regressed: %s" % ", ".join(regressed[:20]))
            return 1
        offpath = sorted(set(joins["ports"]) - set(uni["functions"]))
        if offpath:
            print("src cites %d VAs outside the universe: %s"
                  % (len(offpath), ", ".join("0x" + v for v in offpath[:20])))
            return 1
        print("LEDGER.md current (%d rows)" % len(rows))
        return 0
    open(LEDGER_MD, "w").write(out)
    print("wrote %s: %d rows, stamp %s" % (LEDGER_MD, len(rows), stamp))
    for s in STATUS_ORDER:
        print("  %-12s %d" % (s, counts.get(s, 0)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
