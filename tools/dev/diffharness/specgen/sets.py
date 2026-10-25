"""VA sets for the spec generator: committed set files, and the ledger's pure rows closed over their callees.

A set file has one function per line: the VA, a tab, then its status (optional; default stand-in). Order is kept,
because a spec's seed is 1000 + its position in the set.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
UNIVERSE = os.path.join(REPO, "tools", "dev", "ledger", "universe.json")
DEFAULT_STATUS = "stand-in"


def norm(key):
    """Ledger keys are bare hex ("140001000"); the generator uses "0x140001000"."""
    return "0x%X" % int(key, 16)


def load_universe(path=UNIVERSE):
    """{VA: row} for the ledger universe; each row's callees are keyed by VA too."""
    raw = json.load(open(path, encoding="utf8"))["functions"]
    out = {}
    for key, row in raw.items():
        row = dict(row)
        row["callees"] = {norm(c): how for c, how in (row.get("callees") or {}).items()}
        out[norm(key)] = row
    return out


def read_set(path):
    """[(VA, status)] in file order."""
    rows = []
    with open(path, encoding="utf8") as f:
        for line in f:
            line = line.split("#")[0].rstrip("\n")
            if not line.strip():
                continue
            parts = line.split("\t")
            status = parts[1].strip() if len(parts) > 1 and parts[1].strip() else DEFAULT_STATUS
            rows.append((norm(parts[0].strip()), status))
    return rows


def write_set(path, rows):
    with open(path, "w", encoding="utf8") as f:
        for va, status in rows:
            f.write("%s\t%s\n" % (va, status))


def pure_closure(subsystem, path=UNIVERSE):
    """The subsystem's rows that the ledger's pure rule accepts, closed over their pure callees.

    Seeds come first in VA order, callees after them in discovery order. The pure rule already makes every callee of
    a pure row pure, so the closure only adds rows the rule has judged.
    """
    sys.path.insert(0, os.path.join(REPO, "tools", "dev"))
    import ledger  # tools/dev/ledger.py: the pure rule (pure_rows)

    raw = json.load(open(path, encoding="utf8"))["functions"]
    rows = {norm(k): f for k, f in raw.items()}
    pure = {norm(k) for k in ledger.pure_rows(raw)}
    seeds = sorted(va for va in pure if rows[va].get("subsystem") == subsystem)
    seen, queue, out = set(seeds), list(seeds), []
    while queue:
        va = queue.pop(0)
        out.append(va)
        for c in sorted(norm(k) for k in (rows[va].get("callees") or {})):
            if c in pure and c not in seen:
                seen.add(c)
                queue.append(c)
    return out
