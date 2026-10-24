#!/usr/bin/env python3
"""Match-path universe: BFS callees from the match tick's roots via the Ghidra bridge.

One request at a time. Writes tools/dev/ledger/universe.json (committed data).
Re-crawl only when roots change; the gate does NOT run this (no bridge in gate).

Usage: python3 tools/dev/ledger/universe.py [--out tools/dev/ledger/universe.json]
"""
import json
import re
import sys
import time
import urllib.request

BRIDGE = "http://127.0.0.1:8080"

# (va, subsystem, doc cite). Cites live in docs/re/; VAs verified against the bridge.
SEEDS = [
    (0x1404F6C30, "TeamTick", "pes-re-teamtick.md §2 (team tick, CONFIRMED)"),
    (0x14041ECF0, "TeamTick", "pes-re-teamtick.md §2.1 (team-tick owner H, CONFIRMED)"),
    (0x140925470, "Player", "pes-re-teamtick.md §5 correction (player driver, CONFIRMED raw)"),
    (0x1408BFCF0, "Player", "pes-re-teamtick.md §5 (wrapper callee, raw)"),
    (0x1421019D0, "Player", "pes-re-A4Carrier.md §11 (BallPlayer tick entry)"),
    (0x14092C9F0, "Player", "pes-re-ball-track.md §8 (carrier-sense, tick call L199)"),
    (0x140876360, "Ball", "pes-re-officials-ball.md §0.7 (ball integrator)"),
    (0x140874470, "Ball", "pes-re-officials-ball.md §0.7 (flight model)"),
    (0x140874860, "Ball", "pes-re-ball-track.md §8 (ball init/reset)"),
    (0x140873EE0, "Ball", "pes-re-ball-contact.md (ground/air integrator driver)"),
    (0x1404A56C0, "Referee", "pes-re-A5Judge.md (judge tick, in-play programme step 0x3a)"),
    (0x140A38A00, "Referee", "pes-re-officials-ball.md EV-SPC-0008 (referee chief, MODE 1)"),
    (0x140A38A70, "Referee", "pes-re-officials-ball.md EV-SPC-0008 (referee PK placement)"),
    (0x14044C880, "Restart", "pes-re-foul-choreography.md (situation-sequencer runner)"),
    (0x140445E90, "Restart", "pes-re-foul-choreography.md (sequencer caller)"),
    (0x140422030, "Restart", "pes-re-restart-step.md (restart-step container, caller-traced)"),
    (0x14042B9F0, "MatchTick", "pes-re-foundation-matchball.md §H2 (match tick, 10 steps, OBS)"),
]

CALL = re.compile(r"CALL\s+0x([0-9a-fA-F]+)")
ADDR = re.compile(r"^([0-9a-fA-F]+):")


def get(path, tries=3):
    for i in range(tries):
        try:
            with urllib.request.urlopen(BRIDGE + path, timeout=120) as r:
                return r.read().decode("utf-8", "replace")
        except Exception:
            if i == tries - 1:
                raise
            time.sleep(2)


def disassemble(va):
    return get("/disassemble_function?address=0x%08X" % va)


def main():
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else \
        "tools/dev/ledger/universe.json"

    names = {}
    try:
        txt = get("/list_functions?limit=1000000")
        for m in re.finditer(r"(FUN_[0-9a-fA-F]+) at ([0-9a-fA-F]+)", txt):
            names[int(m.group(2), 16)] = m.group(1)
    except Exception as e:
        print("name list failed (continuing nameless): %s" % e, file=sys.stderr)

    funcs = {}
    queue = [(va, sub, None) for va, sub, _ in SEEDS]
    seen = set()
    while queue:
        va, sub, parent = queue.pop(0)
        if va in seen:
            f = funcs.get("%08X" % va)
            if f and sub not in f["roots"]:
                f["roots"].append(sub)
            continue
        seen.add(va)
        try:
            txt = disassemble(va)
        except Exception as e:
            funcs["%08X" % va] = {"unresolved_seed": True, "error": str(e)[:120],
                                  "subsystem": sub, "roots": [sub],
                                  "path": [parent] if parent else []}
            continue
        if txt.startswith("No function found"):
            funcs["%08X" % va] = {"unresolved_seed": True, "error": txt.strip()[:120],
                                  "subsystem": sub, "roots": [sub],
                                  "path": ([parent] if parent else [])}
            continue
        lines = txt.splitlines()
        addrs = [int(m.group(1), 16) for m in map(ADDR.match, lines) if m]
        lo, hi = (min(addrs), max(addrs)) if addrs else (va, va)
        callees, indirect = {}, []
        for line in lines:
            m = CALL.search(line)
            if m:
                t = int(m.group(1), 16)
                callees.setdefault("%08X" % t, "direct")
                continue
            if re.search(r"\b(CALL|JMP)\b", line) and not re.search(
                    r"\b(CALL|JMP)\s+0x[0-9a-fA-F]+", line):
                site = line.split(":")[0].strip()
                indirect.append({"site": site, "text": line.strip()[:80]})
                continue
            m2 = re.search(r"\bJMP\s+0x([0-9a-fA-F]+)", line)
            if m2:  # tail jump outside this body = call edge
                t = int(m2.group(1), 16)
                if t < lo or t > hi:
                    callees.setdefault("%08X" % t, "tail")
        key = "%08X" % va
        funcs[key] = {"name": names.get(va, "FUN_%08x" % va), "size": len(lines),
                      "subsystem": sub, "roots": [sub],
                      "path": (funcs[parent]["path"] + [parent]
                               if parent and parent in funcs and "path" in funcs[parent]
                               else ([parent] if parent else [])) + [key],
                      "callees": callees, "indirect": indirect}
        for t in callees:
            queue.append((int(t, 16), sub, key))

    data = {"meta": {"bridge": BRIDGE,
                     "roots": [{"va": "0x%08X" % va, "subsystem": s, "cite": c}
                               for va, s, c in SEEDS],
                     "n_functions": len(funcs)},
            "functions": funcs}
    with open(out, "w") as f:
        json.dump(data, f, indent=1, sort_keys=True)
    print("wrote %s: %d functions" % (out, len(funcs)))


if __name__ == "__main__":
    main()
