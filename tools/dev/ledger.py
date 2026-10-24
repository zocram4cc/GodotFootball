#!/usr/bin/env python3
"""Ledger generator: joins universe + decode/ports/standins/diff + hand list.

Writes docs/re/LEDGER.md with a sha256 stamp of its own generated content. In-scope rows are listed
per subsystem; out-of-scope rows are counted, not listed.
Usage: python3 tools/dev/ledger.py [--check]
  --check exits 1 if LEDGER.md is stale, if a row regressed against the LEDGER.md committed at HEAD
  (skipped with a note outside git), or if a GF port is outside the universe and the allow list.
"""
import hashlib
import json
import os
import subprocess
import sys
from collections import Counter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
LEDGER = os.path.join(REPO, "tools", "dev", "ledger")
LEDGER_MD = os.path.join(REPO, "docs", "re", "LEDGER.md")

STATUS_ORDER = ["ported", "diffed", "stand-in", "decoded", "leaf accessor",
                "engine", "undecoded", "inert", "out of scope"]
# Regression rank. Moving to engine, leaf or out of scope is a reclassification, not lost knowledge.
RANK = {"ported": 0, "diffed": 1, "stand-in": 2, "decoded": 3,
        "leaf accessor": 3, "engine": 3, "undecoded": 4, "inert": 5,
        "out of scope": 3}
# Subsystem tags (owner's list). The tag is the nearest root's; out-of-scope rows carry their category.
SUBSYSTEMS = ["pre-match", "gameplay", "ball", "AI", "referee", "restart", "animation", "camera",
              "replay", "audio", "crowd", "HUD", "cutscene", "in-match menu", "render"]
OUT_SUBS = ["out: edit mode", "out: career", "out: save/load", "out: online"]
# Boundary rules as the universe script applies them (universe.py module doc, meta.rules).
RULES = [
    ("library", "a Ghidra symbol that names CRT, STL, zlib, libxml or Winsock code; "
                "no symbol in the bridge's list is in a match:: namespace"),
    ("doc", "a report names the VA as the engine side of a call (phrase, then the VA, same line)"),
    ("rtti", "a vtable a walked function references has an engine RTTI owner "
             "(fox, cobra, KS, gr, draw, std, collision, Concurrency, or a template)"),
    ("crt", "a FUN_ function inside the address span of the CRT's named symbols"),
]
NOT_FOUND = [
    ("pause menu", "found: RTTI menu::MatchPause* (see Roots)"),
    ("team select", "INF: RTTI menu::AfterTeamSelect (name only; no doc names it)"),
    ("league select", "not found: menu::MatchRsSelectionLeagueSel is not tied to exhibition select by any doc"),
    ("side select", "not found: the only RTTI hits are online (SelectedSide, CmdSelectSideReceive: out of scope)"),
    ("kit select", "not found as a screen: the kit loaders are rooted (pre-match, kit assembly doc)"),
    ("crowd audio", "found: audio.md §4.1 config loaders (Supporter, Chant); sound::...chant vtables"),
]


def load(name):
    with open(os.path.join(LEDGER, name)) as f:
        return json.load(f)


def vkey(v):
    v = v.upper()
    if v.startswith("0X"):
        v = v[2:]
    return v


def classify(va, f, joins, hand):
    if va in hand:
        return hand[va]["status"]
    if f.get("scope") == "out":
        return "out of scope"
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


def pure_rows(funcs):
    """Pure computation, judged on the call graph: walked, not a boundary, no indirect site, no vtable
    reference or vtable call, and every callee pure (greatest fixed point over the universe). A callee
    that is a boundary, unresolved or outside the universe makes its caller impure. Live state (globals,
    heap objects read at run time) is not judged."""
    ok = set()
    for va, f in funcs.items():
        if not f.get("walked") or f.get("boundary") or f.get("unresolved") or f.get("indirect") or f.get("vtables"):
            continue
        if "vtable" in (f.get("callees") or {}).values():
            continue
        ok.add(va)
    changed = True
    while changed:
        changed = False
        for va in list(ok):
            if any(t not in ok for t in funcs[va].get("callees") or {}):
                ok.discard(va)
                changed = True
    return ok


def table(L, head, rows):
    L.append("| " + " | ".join(head) + " |")
    L.append("|" + "---|" * len(head))
    for r in rows:
        L.append("| " + " | ".join(str(c) for c in r) + " |")
    L.append("")


def committed_ledger():
    """LEDGER.md as committed at HEAD: the regression baseline (None outside a git checkout)."""
    try:
        return subprocess.run(["git", "-C", REPO, "show", "HEAD:docs/re/LEDGER.md"],
                              capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None


def main():
    check = "--check" in sys.argv
    uni = load("universe.json")
    joins = load("joins.json")
    hand = load("hand.json")
    hand_map = {vkey(e["va"]): e for e in hand["entries"]}
    funcs = uni["functions"]
    meta = uni.get("meta", {})
    pure = pure_rows(funcs)
    rows = []
    for va, f in funcs.items():
        status = classify(va, f, joins, hand_map)
        dec = joins["decode"].get(va, {})
        rows.append({
            "key": va,
            "va": "0x" + va,
            "name": f.get("name", "FUN_" + va),
            "sub": (next((x for x in f.get("roots", []) if x.startswith("out: ")), "out: ?")
                    if f.get("scope") == "out" else f.get("subsystem", "?")),
            "how": f.get("how", "-"),
            "scope": f.get("scope", "in"),
            "status": status,
            "rule": (f.get("boundary") or {}).get("rule", "-"),
            "verdict": dec.get("verdict") or "-",
            "cite": (dec.get("cites") or ["-"])[0],
            "port": ";".join(joins["ports"].get(va, {}).get("files", [])) or "-",
            "callers": 0,  # filled below
            "size": f.get("size", 0),
            "pure": va in pure,
            "path": ">".join((f.get("path") or [])[-3:]),
        })
    by_key = {r["key"]: r for r in rows}
    for f in funcs.values():
        for t in f.get("callees") or {}:
            if t in by_key:
                by_key[t]["callers"] += 1
    ins = [r for r in rows if r["scope"] == "in"]
    outs = [r for r in rows if r["scope"] == "out"]
    counts_in = Counter(r["status"] for r in ins)
    counts_all = Counter(r["status"] for r in rows)
    subs, out_subs = {}, {}
    for r in ins:
        subs.setdefault(r["sub"], []).append(r)
    for r in outs:
        out_subs.setdefault(r["sub"], []).append(r)
    sub_order = SUBSYSTEMS + sorted(s for s in subs if s not in SUBSYSTEMS)
    stat_cols = [s for s in STATUS_ORDER if s != "out of scope"]
    unresolved = [r for r in rows if funcs[r["key"]].get("unresolved")]
    protected = [r for r in unresolved if funcs[r["key"]].get("protected")]

    def stat_row(name, rs):
        c = Counter(r["status"] for r in rs)
        return [name, len(rs)] + [c.get(s, 0) for s in stat_cols]

    def pp_row(name, rs):
        port = [r for r in rs if r["status"] in ("ported", "diffed", "stand-in")]
        how = Counter(r["how"] for r in rs)
        return [name, len(rs), sum(1 for r in rs if r["pure"]), len(port),
                sum(1 for r in port if r["pure"]), how.get("doc", 0), how.get("rtti", 0),
                how.get("owner", 0)]

    L = []
    L.append("# Match-path ledger (generated — do not edit by hand)")
    L.append("")
    L.append("Regenerate: `python3 tools/dev/ledger.py`. Enforced by `tools/dev/gate.sh`. "
             "Inputs: `tools/dev/ledger/` (`universe.py` + `universe.json` (bridge crawl with the image, "
             "run under the capstone venv), `joins.py` + `joins.json` (`--funclist` from the bridge's "
             "function list), `hand.json`, `offpath_allow.json`).")
    L.append("")
    L.append("Statuses: **ported** / **diffed** / **stand-in** (GF code cites the VA), **decoded** "
             "(a report gives a verdict), **leaf accessor** (<= %s instructions, no call, no jump "
             "out, no vtable reference), **engine** (boundary: not walked, not match logic), "
             "**undecoded** (walked or unresolved, no verdict), **inert** (hand.json), "
             "**out of scope** (a rule below: edit mode, career, save/load, online)."
             % meta.get("leaf_max_insns", "?"))
    L.append("")
    L.append("Scope: **in scope** is everything a player goes through to play a match: the pre-match "
             "select flow, the match, its presentation (replay, camera, audio, crowd, HUD, cutscenes) "
             "and its in-match menus. **Out of scope** is only edit mode, career modes, save/load and "
             "online: the rules are under *Scope rules*. Out-of-scope rows are counted, not listed.")
    L.append("")
    L.append("Subsystem: each row carries the tag of its nearest root (BFS distance). `how` says where "
             "the tag came from: `doc` (a report cites the root VA), `rtti` (the root is a vtable named "
             "by its RTTI class), `owner` (a boundary row whose RTTI owner is draw or gr: render).")
    L.append("")
    L.append("## Summary")
    L.append("")
    table(L, ["status", "in scope", "out of scope", "all"],
          [("total", len(ins), len(outs), len(rows))] +
          [(s, counts_in.get(s, 0), sum(1 for r in outs if r["status"] == s), counts_all.get(s, 0))
           for s in STATUS_ORDER])
    L.append("## Scope rules")
    L.append("")
    L.append("In scope: the match roots. Function roots are doc-cited (*Roots*); vtable roots are "
             "named by RTTI class (*Roots*). Out of scope: the vtables below are roots of the out crawl; "
             "rows reached only from them are out of scope. The in crawl does not expand these vtables, "
             "so an in-scope call into one stays a single row, not a walk.")
    L.append("")
    table(L, ["scope", "RTTI rule (why)", "vtables", "slots"],
          [(x["scope"], x["why"], x["vtables"], x["slots"]) for x in meta.get("scope_rules", [])])
    out_vt = Counter(v["category"] for v in meta.get("out_vtables", []))
    table(L, ["out of scope (category)", "rows", "vtable roots"],
          [(s, len(out_subs.get(s, [])), out_vt.get(s[len("out: "):], 0)) for s in OUT_SUBS] +
          [("total", len(outs), sum(out_vt.values()))])
    L.append("Crawl counters (universe.py meta.rules): " + ", ".join(
        "%s %s" % (k, v) for k, v in sorted((meta.get("rules") or {}).items())) + ".")
    L.append("")
    L.append("## Subsystems (in scope)")
    L.append("")
    L.append("Rows per subsystem and status:")
    L.append("")
    table(L, ["subsystem", "rows"] + stat_cols,
          [stat_row(s, subs[s]) for s in sub_order if s in subs] + [stat_row("total", ins)])
    L.append("Pure computation (call graph, see `pure_rows`) and GF ports per subsystem:")
    L.append("")
    table(L, ["subsystem", "rows", "pure (call graph)", "GF port (ported, diffed, stand-in)",
              "pure and GF port", "how: doc", "how: rtti", "how: owner"],
          [pp_row(s, subs[s]) for s in sub_order if s in subs] + [pp_row("total", ins)])
    L.append("## Roots")
    L.append("")
    L.append("Function roots (`how: doc`): the report cites the VA. Where the bridge lists no function, the "
             "body is decoded with capstone from the image (`source: capstone`).")
    L.append("")
    table(L, ["VA", "subsystem", "cite"],
          [(r["va"], r["subsystem"], r["cite"]) for r in meta.get("roots", [])])
    L.append("Vtable roots (`how: rtti`): every code slot of each vtable. The vtable is named by its RTTI "
             "class (TypeDescriptor), found by scanning the image's .rdata for COL and vtable pointers.")
    L.append("")
    grp, slots = Counter(), Counter()
    for v in meta.get("vtable_roots", []):
        k = (v["subsystem"], v["why"])
        grp[k] += 1
        slots[k] += v["slots"]
    table(L, ["subsystem", "RTTI rule (how found)", "vtables", "code slots"],
          [(s, w, grp[(s, w)], slots[(s, w)]) for s, w in sorted(grp)])
    L.append("Not found (owner's list): ")
    L.append("")
    table(L, ["element", "result"], NOT_FOUND)
    L.append("## Boundary and leaf rules")
    L.append("")
    table(L, ["rule", "rows", "what it cuts"],
          [(name, sum(1 for r in rows if r["rule"] == name), text) for name, text in RULES])
    table(L, ["unwalked", "rows", "note"], [
        ("leaf accessor", counts_in.get("leaf accessor", 0),
         "walked; trivially read by the port, counted apart from the gap"),
        ("unresolved (bridge has no body, no capstone decode)", len(unresolved),
         "%d in the protected import area (.impdata, no code on the bridge)" % len(protected)),
    ])
    gap = [r for r in ins if r["status"] in ("undecoded", "stand-in")]
    by_callers = sorted(gap, key=lambda r: (-r["callers"], -r["size"]))[:10]
    by_size = sorted((r for r in gap if r["size"] > 0), key=lambda r: (-r["size"], -r["callers"]))[:10]
    gap_head = ["VA", "name", "status", "callers", "size", "sub"]
    gap_rows = lambda rs: [(r["va"], r["name"], r["status"], r["callers"], r["size"], r["sub"]) for r in rs]
    L.append("## Gap list: top 10 undecoded / stand-in by callers, then size (in scope)")
    L.append("")
    table(L, gap_head, gap_rows(by_callers))
    L.append("## Gap list: top 10 undecoded / stand-in by size, then callers (in scope)")
    L.append("")
    table(L, gap_head, gap_rows(by_size))
    for sub in sub_order:
        if sub not in subs:
            continue
        rs = sorted(subs[sub], key=lambda r: r["va"])
        L.append("## %s (%d)" % (sub, len(rs)))
        L.append("")
        L.append("| VA | name | status | decode | cite | port | callers | size | pure | how | path |")
        L.append("|---|---|---|---|---|---|---|---|---|---|---|")
        for r in rs:
            L.append("| %s | %s | %s | %s | %s | %s | %d | %d | %s | %s | %s |"
                     % (r["va"], r["name"], r["status"], r["verdict"], r["cite"], r["port"],
                        r["callers"], r["size"], "y" if r["pure"] else "-", r["how"], r["path"]))
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
        m = _re.match(r"<!-- LEDGER-STAMP ([0-9a-f]+) -->\n(.*)$", cur, _re.S)
        if not m or m.group(1) != stamp or m.group(2) != body:
            print("LEDGER.md stamp missing/stale or content differs "
                  "(hand edit or stale copy): regenerate with "
                  "tools/dev/ledger.py")
            return 1
        base = committed_ledger()
        if base is None:
            print("regression baseline: no LEDGER.md at HEAD (not checked)")
        else:
            old_rows = {}
            for line in base.splitlines():
                if line.startswith("| 0x"):
                    cells = [c.strip() for c in line.split("|")]
                    old_rows[cells[1][2:]] = cells[3]
            new_rows = {r["key"]: r["status"] for r in rows}
            moves = {(vkey(e["va"]), e["from"], e["to"]) for e in
                     load("scope_moves.json").get("entries", [])}
            regressed = sorted(
                va for va in old_rows if va in new_rows
                and RANK.get(new_rows[va], 99) > RANK.get(old_rows[va], 99)
                and (va, old_rows[va], new_rows[va]) not in moves)
            if regressed:
                print("ledger regressed vs HEAD (not a recorded scope move): %d rows, e.g. %s"
                      % (len(regressed), ", ".join(regressed[:20])))
                return 1
            print("no unrecorded status regression vs HEAD (%d rows compared, %d scope moves recorded)"
                  % (sum(1 for va in old_rows if va in new_rows), len(moves)))
        funclist = set(joins.get("funclist") or [])
        offpath = sorted(v for v in set(joins["ports"]) - set(funcs) if v in funclist)
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
    open(LEDGER_MD, "w", encoding="utf-8").write(out)
    print("wrote %s: %d rows (in scope %d, out %d), stamp %s" % (LEDGER_MD, len(rows), len(ins), len(outs), stamp))
    for s in STATUS_ORDER:
        print("  %-14s in %-6d out %d" % (s, counts_in.get(s, 0), sum(1 for r in outs if r["status"] == s)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
