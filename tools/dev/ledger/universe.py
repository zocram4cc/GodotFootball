#!/usr/bin/env python3
"""Match-path universe: BFS over callees from the match tick's roots, via the Ghidra bridge.

One request at a time. Writes tools/dev/ledger/universe.json (committed data).
Re-crawl only when the roots, a rule or the image changes; the gate does NOT run this (no bridge in gate).

Usage: python3 tools/dev/ledger/universe.py --image PES_IMAGE [--out PATH] [--max N]
  --image  the PES image dump (file offset = VA - 0x140000000). Read for the vtable and jump-table
           rules. Without it those two rules are off (meta.image = false).

Per-row rules (meta.rules counts them; docs/re/LEDGER.md states them):
  body     the body is the instructions reachable from the entry by control flow (fall-through,
           direct jumps and branches; calls return). The bridge lists Ghidra's whole body range,
           which runs over neighbouring functions (FUN_1403B9780: 12,705 listed lines, a getter).
  library  a Ghidra symbol that is not FUN_/thunk_/caseD_/... names CRT, STL, zlib, libxml or
           Winsock code; none of the bridge's 2,387 such symbols is in a match:: namespace.  -> engine
  doc      a VA that a report names as the engine side of a call: the phrase, then the VA on the
           same line (ENGINE_PHRASE over docs/re/*.md).                                        -> engine
  rtti     a vtable a walked function references has an RTTI owner (COL -> TypeDescriptor) in an
           engine namespace or a template. Its code slots are engine.                          -> engine
  crt      a FUN_ function inside the address span of the CRT's named symbols.                -> engine
  table    an indirect jump through a relative table (byte-index map and RVA table, read from the
           image at the VAs in the code) takes the table's targets.
  vtable   every code slot of a vtable a walked function references is a callee.
  leaf     <= LEAF_MAX_INSNS instructions, no call, no jump out, no indirect or table jump, no
           vtable reference.                                                                   -> leaf accessor
Boundary rows are recorded, not walked: their callees are not expanded.
"""
import json
import os
import re
import struct
import sys
import time
import urllib.request
from collections import Counter, deque

BRIDGE = "http://127.0.0.1:8080"
IMAGE_BASE = 0x140000000
CODE_LO, CODE_HI = 0x140001000, 0x14252E800      # .trace: code
RDATA_LO, RDATA_HI = 0x14252F000, 0x14339B000    # .rdata: vtables, COLs, TypeDescriptors
IMPDATA_LO = 0x143D14000                         # protected import area: no body on the bridge
LEAF_MAX_INSNS = 10
MAX_INSNS = 20000
ENGINE_ROOTS = {"fox", "cobra", "KS", "gr", "draw", "menu", "flow", "std", "collision", "Concurrency"}
NOT_LIBRARY = ("FUN_", "thunk_", "caseD_", "switchD_", "sub_", "LAB_", "DAT_")
ENGINE_PHRASE = re.compile(r"engine call is|\*\*Engine\*\*|Engine side|engine side|Engine plugin|"
                           r"Engine \(OBS|engine-side|BodyAnimControl::|AnimMovePlugin|AnimSyncPlugin|"
                           r"cobra::|fox::")
VA_TOKEN = re.compile(r"(?:0x|FUN_|sub_)(14[0-9A-Fa-f]{7})(?![0-9A-Fa-f])")
INSN = re.compile(r"^([0-9a-fA-F]+):\s*(.*?)\s*$")
DIRECT = re.compile(r"^0x([0-9a-fA-F]+)$")
DREF = re.compile(r"(?<![0-9A-Fa-f])0x(14[0-9A-Fa-f]{7})(?![0-9A-Fa-f])")
IND_JMP = re.compile(r"^JMP\s+R\w+$")
LEA_BASE = re.compile(r"LEA\s+R\w+,\s*\[0x140000000\]")
TBL_BM = re.compile(r"MOVZX\s+\w+,\s*byte ptr \[R\w+ \+ R\w+\*0x1 \+ 0x([0-9a-fA-F]+)\]")
TBL_RT = re.compile(r"MOV\s+\w+,\s*dword ptr \[R\w+ \+ R\w+\*0x4 \+ 0x([0-9a-fA-F]+)\]")
CMP_IMM = re.compile(r"CMP\s+E\w+,\s*0x([0-9a-fA-F]+)")
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOCS = os.path.join(REPO, "docs", "re")
DEFAULT_OUT = os.path.join(REPO, "tools", "dev", "ledger", "universe.json")

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


def get(path, tries=3, timeout=120):
    for i in range(tries):
        try:
            with urllib.request.urlopen(BRIDGE + path, timeout=timeout) as r:
                return r.read().decode("utf-8", "replace")
        except Exception:
            if i == tries - 1:
                raise
            time.sleep(2)


def disassemble(va):
    return get("/disassemble_function?address=0x%08X" % va, timeout=900)


def load_names():
    """Every function start the bridge knows, named: FUN_ and library symbols alike."""
    names = {}
    for line in get("/list_functions?limit=1000000", timeout=900).splitlines():
        name, sep, addr = line.rpartition(" at ")
        if sep and re.fullmatch(r"[0-9a-fA-F]+", addr):
            names.setdefault(int(addr, 16), name.strip())
    return names


def is_library(name):
    return bool(name) and not name.startswith(NOT_LIBRARY)


def doc_engine_vas():
    """VA -> doc, for VAs that a report names as the engine side of a call (same line, after the phrase)."""
    out = {}
    for fn in sorted(os.listdir(DOCS)):
        if not fn.endswith(".md") or fn == "LEDGER.md":
            continue
        with open(os.path.join(DOCS, fn), encoding="utf-8", errors="replace") as f:
            for line in f:
                for pm in ENGINE_PHRASE.finditer(line):
                    for vm in VA_TOKEN.finditer(line, pm.end()):
                        out.setdefault(int(vm.group(1), 16), fn)
    return out


def crt_span(names):
    lib = sorted(a for a, n in names.items() if is_library(n) and 0x141400000 <= a < 0x141800000)
    return (lib[0], lib[-1]) if lib else None


def parse_listing(txt):
    insns = {}
    for line in txt.splitlines():
        m = INSN.match(line)
        if m:
            insns.setdefault(int(m.group(1), 16), m.group(2))
    return insns


def read(img, va, n):
    img.seek(va - IMAGE_BASE)
    return img.read(n)


def jump_tables(insns, order, img):
    """Indirect jump -> table targets, for the relative table shapes read from the image.

    Both shapes take the image base in a register (`LEA Rb,[0x140000000]`), load a dword RVA from
    `[Rb+Ri*4+RT]`, then `ADD R?,Rb; JMP R?`. The index is bounded by the nearest `CMP E?X,imm`
    before the load, so the table has imm+1 entries. Two index forms:
      byte-indexed: `MOVZX E?,byte ptr [Rb+Ri*1+BM]` maps the value to an index into RT
                    (docs/re/pes-re-dead-code.md §4.2, the action switch, 0x1409F8030);
      direct:       the switch value is the index (docs/re/pes-re-runloop-arms.md §4, the
                    MOVESTEP state dispatch at 0x1406BA900, table 0x1406BB50C).
    """
    out = {}
    if img is None or not any(LEA_BASE.search(t) for t in insns.values()):
        return out
    for i, a in enumerate(order):
        if not IND_JMP.match(insns[a]):
            continue
        win = [(order[j], insns[order[j]]) for j in range(max(0, i - 16), i)]
        rt = bm = rt_j = bm_j = None
        for j, (_, t) in enumerate(win):
            m = TBL_RT.search(t)
            if m:
                rt, rt_j = int(m.group(1), 16), j
            m = TBL_BM.search(t)
            if m:
                bm, bm_j = int(m.group(1), 16), j
        if rt is None:
            continue
        cmp_imm = None
        for _, t in reversed(win[:(bm_j if bm is not None else rt_j)]):
            m = CMP_IMM.search(t)
            if m:
                cmp_imm = int(m.group(1), 16)
                break
        if cmp_imm is None or cmp_imm > 0xFFF:
            continue
        if bm is None:
            span = cmp_imm + 1
        else:
            span = max(read(img, IMAGE_BASE + bm, cmp_imm + 1)) + 1
        table = read(img, IMAGE_BASE + rt, 4 * span)
        targets = {IMAGE_BASE + struct.unpack_from("<I", table, 4 * k)[0] for k in range(span)}
        out[a] = sorted(t for t in targets if CODE_LO <= t < CODE_HI)
    return out


def closure(entry, insns, order, pos, entries, tbl):
    """Body = instructions reachable from the entry by control flow inside the listing.

    Returns (body, direct calls, tail jumps out, table targets out, indirect sites, data VAs,
    resolved table jumps)."""
    body, work = set(), [entry]
    calls, tails, tbl_ext, indirect, drefs, tbl_hits = [], [], [], [], set(), 0
    while work and len(body) < MAX_INSNS:
        a = work.pop()
        if a in body or a not in insns:
            continue
        body.add(a)
        text = insns[a]
        mn, _, ops = text.partition(" ")
        ops = ops.strip()
        drefs.update(int(m.group(1), 16) for m in DREF.finditer(text))
        i = pos[a]
        nxt = order[i + 1] if i + 1 < len(order) else None
        if nxt is not None and nxt != entry and nxt in entries:
            nxt = None  # falls into another function's entry
        if mn.startswith("RET") or mn in ("HLT", "INT3", "UD2"):
            continue
        d = DIRECT.match(ops)
        if mn == "CALL":
            if d:
                calls.append(int(d.group(1), 16))
            else:
                indirect.append((a, text))
            if nxt is not None:
                work.append(nxt)
            continue
        if mn.startswith("J"):
            if a in tbl:
                tbl_hits += 1
                for t in tbl[a]:
                    if t in insns:
                        work.append(t)
                    else:
                        tbl_ext.append(t)
                continue
            if not d:
                indirect.append((a, text))
                continue
            t = int(d.group(1), 16)
            if t not in insns or (t in entries and t != entry):
                tails.append(t)
            else:
                work.append(t)
            if mn == "JMP":
                continue
        if nxt is not None:
            work.append(nxt)
    return body, calls, tails, tbl_ext, indirect, drefs, tbl_hits


def rtti_root(td):
    """Outermost namespace of an MSVC TypeDescriptor name ('?' for a template or anonymous name)."""
    if not td.startswith((".?AV", ".?AU", ".?AT", ".?AW")):
        return None
    body = td[4:]
    if body.startswith("?"):
        return "?"
    parts = [p for p in body.split("@") if p]
    return parts[-1] if parts else None


def vtable_at(img, v):
    """(code slots, RTTI root) if v is a vtable: its COL (at v-8) has the signature and self RVA."""
    col = struct.unpack("<Q", read(img, v - 8, 8))[0]
    if not RDATA_LO <= col < RDATA_HI:
        return None
    sig, _, _, td_rva, _, self_rva = struct.unpack("<6I", read(img, col, 24))
    if sig != 1 or IMAGE_BASE + self_rva != col:
        return None
    td = read(img, IMAGE_BASE + td_rva + 0x10, 160).split(b"\0", 1)[0].decode("ascii", "replace")
    slots = []
    while len(slots) < 512:
        p = struct.unpack("<Q", read(img, v + 8 * len(slots), 8))[0]
        if not CODE_LO <= p < CODE_HI:
            break
        slots.append(p)
    return slots, rtti_root(td)


def rtti_engine(root):
    return root is not None and (root == "?" or root in ENGINE_ROOTS)


def boundary(entry, name, owner, doc_engine, crt):
    """(rule, reason) if the function is the engine boundary, else None (walked)."""
    own = owner.get(entry)
    if own is not None and rtti_engine(own):
        return ("rtti", own)
    if is_library(name):
        return ("library", name)
    if entry in doc_engine:
        return ("doc", doc_engine[entry])
    if crt and name.startswith("FUN_") and crt[0] <= entry <= crt[1]:
        return ("crt", "%X-%X" % crt)
    return None


def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    out = opt("--out", DEFAULT_OUT)
    image = opt("--image")
    limit = int(opt("--max", "0"))
    img = open(image, "rb") if image else None
    names = load_names()
    entries = set(names)
    doc_engine = doc_engine_vas()
    crt = crt_span(names)
    funcs, alias, owner, visited, vt_cache = {}, {}, {}, set(), {}
    q = deque((va, sub, None, "root") for va, sub, _ in SEEDS)
    done = 0
    while q:
        if limit and done >= limit:
            break
        va, sub, parent, _edge = q.popleft()
        va = alias.get(va, va)
        key = "%08X" % va
        if va in visited:
            if key in funcs and sub not in funcs[key]["roots"]:
                funcs[key]["roots"].append(sub)
            continue
        done += 1
        if done % 200 == 0:
            print("%d crawled, %d queued" % (done, len(q)), file=sys.stderr, flush=True)
        base = funcs["%08X" % parent]["path"] if parent is not None and "%08X" % parent in funcs else []
        try:
            txt = disassemble(va)
        except Exception as e:  # bridge error: the row stays, unresolved
            txt = "error: %s" % str(e)[:120]
        insns = {} if txt.startswith(("No function found", "error:")) else parse_listing(txt)
        if not insns:
            visited.add(va)
            funcs[key] = {"name": names.get(va, "FUN_%08x" % va), "size": 0, "subsystem": sub,
                          "roots": [sub], "path": (base + [key])[-4:], "walked": False,
                          "boundary": None, "leaf": False, "unresolved": True,
                          "protected": va >= IMPDATA_LO, "error": txt.strip()[:120],
                          "callees": {}, "indirect": [], "tables": 0, "vtables": []}
            continue
        order = sorted(insns)
        entry = order[0]  # the listing starts at the containing function's entry
        if entry != va:
            alias[va] = entry
        ekey = "%08X" % entry
        if entry in visited:
            if sub not in funcs[ekey]["roots"]:
                funcs[ekey]["roots"].append(sub)
            continue
        visited.add(entry)
        pos = {a: i for i, a in enumerate(order)}
        tbl = jump_tables(insns, order, img)
        body, calls, tails, tbl_ext, indirect, drefs, tbl_hits = closure(
            entry, insns, order, pos, entries, tbl)
        name = names.get(entry, "FUN_%08x" % entry)
        vt_hits = []
        if img is not None:
            for v in sorted(drefs):
                if not RDATA_LO <= v < RDATA_HI:
                    continue
                if v not in vt_cache:
                    vt_cache[v] = vtable_at(img, v)
                if vt_cache[v] is not None:
                    vt_hits.append((v, vt_cache[v]))
        bnd = boundary(entry, name, owner, doc_engine, crt)
        callees = {}
        if bnd is None:
            for t in calls:
                callees.setdefault(t, "direct")
            for t in tails:
                callees.setdefault(t, "tail")
            for t in tbl_ext:
                callees.setdefault(t, "table")
            for _v, (slots, root) in vt_hits:
                for s in slots:
                    callees.setdefault(s, "vtable")
                    owner.setdefault(s, root)
        leaf = (bnd is None and len(body) <= LEAF_MAX_INSNS and not calls and not tails
                and not tbl_ext and not indirect and not tbl_hits and not vt_hits)
        funcs[ekey] = {
            "name": name, "size": len(body), "subsystem": sub, "roots": [sub],
            "path": (base + [ekey])[-4:], "walked": bnd is None,
            "boundary": {"rule": bnd[0], "why": bnd[1]} if bnd else None,
            "leaf": leaf,
            "callees": {"%08X" % t: k for t, k in sorted(callees.items())},
            "indirect": [{"site": "%X" % a, "text": t[:80]} for a, t in indirect],
            "tables": tbl_hits,
            "vtables": [{"va": "%X" % v, "root": info[1]} for v, info in vt_hits],
            "overspill": True if len(body) >= MAX_INSNS else None,
        }
        for t, kind in callees.items():
            q.append((t, sub, entry, kind))

    rows = funcs.values()
    rules = Counter()
    for f in rows:
        if f.get("boundary"):
            rules["boundary:" + f["boundary"]["rule"]] += 1
        if f.get("leaf"):
            rules["leaf"] += 1
        if f.get("unresolved"):
            rules["unresolved"] += 1
            rules["unresolved:protected"] += 1 if f.get("protected") else 0
        rules["table jumps resolved"] += f.get("tables", 0)
        rules["vtable rows"] += 1 if f.get("vtables") else 0
    meta = {"bridge": BRIDGE, "image": bool(image), "named_symbols": len(names),
            "doc_engine_vas": len(doc_engine),
            "crt_span": ["%X" % a for a in crt] if crt else None,
            "leaf_max_insns": LEAF_MAX_INSNS, "max_insns": MAX_INSNS,
            "roots": [{"va": "0x%08X" % va, "subsystem": s, "cite": c} for va, s, c in SEEDS],
            "n_functions": len(funcs), "rules": dict(sorted(rules.items()))}
    with open(out, "w") as f:
        json.dump({"meta": meta, "functions": funcs}, f, indent=1, sort_keys=True)
    print("wrote %s: %d functions, rules %s" % (out, len(funcs), dict(sorted(rules.items()))))


if __name__ == "__main__":
    main()
