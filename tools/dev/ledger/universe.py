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
    (0x14041ECF0, "TeamTick", "pes-re-teamtick.md §2.1 (team-tick owner H slot 2, DATA 0x142597590)"),
    (0x140925750, "Player", "UNRESOLVED in bridge (slot-2 thumb only, DATA 0x1426329b8); seed kept, crawler records the gap"),
    (0x140925470, "Player", "pes-re-teamtick.md §5 correction (player driver, CONFIRMED raw)"),
    (0x1408BFCF0, "Player", "pes-re-a3helpers.md (per-player owner tick, called at 0x140925793)"),
    (0x1408C48C0, "Player", "pes-re-contact.md (PvP contacts, per player per tick)"),
    (0x1409F8030, "Player", "pes-re-dead-code.md §4.2 (action switch dispatcher)"),
    (0x1406E5FC0, "Player", "pes-re-anim-select.md (AnimePlayer Exec, per-tick action virtual)"),
    (0x1421019D0, "Player", "pes-re-A4Carrier.md §11 (BallPlayer tick entry)"),
    (0x14092C9F0, "Player", "pes-re-ball-track.md §8 (carrier-sense, tick call L199)"),
    (0x140876360, "Ball", "pes-re-officials-ball.md §0.7 (ball integrator)"),
    (0x140874470, "Ball", "pes-re-officials-ball.md §0.7 (acceleration fn)"),
    (0x140874860, "Ball", "pes-re-ball-track.md §8 (ball init/reset)"),
    (0x140873EE0, "Ball", "pes-re-ball-contact.md (ground/air integrator driver)"),
    (0x140425E20, "Ball", "pes-re-ball-collision.md (net in/out state)"),
    (0x140424F70, "Ball", "pes-re-ball-collision.md (BallControl slot 4, DATA 0x1425988d0)"),
    (0x1404A56C0, "Referee", "pes-re-A5Judge.md (judge tick, step 0x3a)"),
    (0x140A38A00, "Referee", "pes-re-officials-ball.md EV-SPC-0008 (referee chief, MODE 1)"),
    (0x140A38A70, "Referee", "pes-re-officials-ball.md EV-SPC-0008 (referee PK placement)"),
    (0x140A38C40, "Referee", "pes-re-dead-code.md §4.3 (referee dispatcher, DATA 0x142642190)"),
    (0x14044C880, "Restart", "pes-re-foul-choreography.md (situation-sequencer runner)"),
    (0x140445E90, "Restart", "pes-re-foul-choreography.md (sequencer caller)"),
    (0x140422030, "Restart", "pes-re-restart-step.md (restart-step container, caller-traced)"),
    (0x140423000, "Restart", "pes-re-A5Judge.md (event-driven record slot 3, DATA 0x142598570)"),
    (0x140418940, "MatchTick", "pes-re-teamorder.md (listener registrar: MatchMain+MatchSub via FUN_14042fe20)"),
    (0x14042FE20, "MatchTick", "pes-re-teamorder.md (listener builder, MatchMain then MatchSub)"),
    (0x140433760, "MatchTick", "pes-re-teamorder.md (MatchSub listener, vtable 0x14259b1a8 slot 2)"),
    (0x1404334E0, "MatchTick", "pes-re-teamorder.md (MatchSub path to team tick via FUN_140a8eff0)"),
    (0x140A8EFF0, "MatchTick", "pes-re-teamorder.md (MatchSub to team-tick owner H)"),
    (0x1407824A0, "Player", "pes-re-anim-select.md (action tick, MOVESTEP caller)"),
    (0x140789DD0, "Player", "pes-re-anim-select.md (class-0x53 gate, MOVESTEP caller)"),
    (0x1407D1E60, "Player", "pes-re-anim-select.md (velocity integrator, MOVESTEP caller)"),
    (0x140791EC0, "Player", "pes-re-throwin-choreography.md (SeamlessThrowin phases)"),
    (0x140792E10, "Player", "pes-re-restart-step.md (restart release phase)"),
    (0x1409F8220, "Player", "pes-re-receiving.md (receive reaction entry 3, live-data-root)"),
    (0x140A07E40, "Player", "pes-re-dodge-gates.md (dodge eligibility gate, ported)"),
    (0x140A08340, "Player", "pes-re-dodge-port.md (POST_DODGE predicate)"),
    (0x140A08EA0, "Player", "pes-re-run-targets.md (pass/shot block entry 8)"),
    (0x14042B9F0, "MatchTick", "pes-re-foundation-matchball.md §H2 (match tick, 10 steps, OBS)"),
]

CALL = re.compile(r"CALL\s+0x([0-9a-fA-F]+)")
ADDR = re.compile(r"^([0-9a-fA-F]+):")
# Bridge artifact guard: the boundary walk can run past a jump table into
# unrelated code (match-tick step 8 FUN_140426A80 reads 1.4M lines). Past
# this many lines the tail is untrusted: stop and record it as overspill.
MAX_LINES = 20000


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
        overspill = len(lines) > MAX_LINES
        if overspill:
            lines = lines[:MAX_LINES]
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
                      "callees": callees, "indirect": indirect,
                      "overspill": overspill or None}
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
