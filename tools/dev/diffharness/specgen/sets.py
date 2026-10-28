"""VA sets for the spec generator: committed set files, and the ledger's pure rows closed over their callees.

A set file has one function per line: the VA, a tab, then its status (optional; default stand-in). Order is kept for
readability only: a spec's seed is 1000 + its VA's offset from the image base (`spec.seed_of`). `select` adds a subsystem
and a role (seed or callee) column; readers ignore them.
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
    """Rows are (VA, status) or (VA, status, subsystem, role)."""
    with open(path, "w", encoding="utf8") as f:
        for row in rows:
            f.write("\t".join(row) + "\n")


def closure_rows(subsystems, path=UNIVERSE):
    """The union of the subsystems' pure closures, as (VA, status, subsystem, role) rows.

    A seed is a pure row tagged with one of the subsystems; its closure adds the pure rows reached through the
    callee edges. Seeds come first (subsystems in the order given, VA order within each), callees follow in
    discovery order. Each row takes the subsystem of the seed that reaches it first, so every row has one
    subsystem. With one subsystem this is the cutscene set's closure.
    """
    sys.path.insert(0, os.path.join(REPO, "tools", "dev"))
    import ledger  # tools/dev/ledger.py: the pure rule (pure_rows)

    raw = json.load(open(path, encoding="utf8"))["functions"]
    rows = {norm(k): f for k, f in raw.items()}
    pure = {norm(k) for k in ledger.pure_rows(raw)}
    owner, role, order = {}, {}, []
    for s in subsystems:
        for va in sorted(va for va in pure if rows[va].get("subsystem") == s):
            if va not in owner:
                owner[va], role[va] = s, "seed"
                order.append(va)
    i = 0
    while i < len(order):  # FIFO over the discovered rows
        va = order[i]
        i += 1
        for c in sorted(norm(k) for k in (rows[va].get("callees") or {})):
            if c in pure and c not in owner:
                owner[c], role[c] = owner[va], "callee"
                order.append(c)
    return [(va, DEFAULT_STATUS, owner[va], role[va]) for va in order]
