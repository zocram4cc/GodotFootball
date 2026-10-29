#!/usr/bin/env python3
"""scene_map.py: FixDemo scene id -> scene -> steps -> cut groups -> variants -> .fdc, from the install.

Reads the nine selection tables `common/demo/fixdemo/<cat>/table_<cat>.bin` and each category's
`cut_data/*.fdc` listing through the repo's `pesfmt_dump` (the mounted VFS, so a mod that wins the
mount is the one read). Decodes the layout documented in docs/re/pes-re-cutscenes.md §2.1-2.2
(T1 cut library, T2 scenes, goal extras, T3 control table; each stream is consumed exactly, and
the section-end read counter must land on the header's end value, or the run fails).

  scene id   = (cat << 16) | (scene index + 1)      cut group id = (cat << 16) | (group index + 1)
  category   = (scene id >> 16) & 0xff              cat ids (pes-re-cutscenes.md §2.3): ent 1,
                                                    foul 2, goal 3, pk 4, change 5, timeup 6, end 7,
                                                    result 8, mode 9

A T2 step with cut == 0 terminates its scene. A step's cut is a cut group id of the same category;
the group's rows are the variants (condition keys, and the .fdc layers that play together). The
variant is chosen at run time by the matcher (141F4B1D0) and its evaluator (141F4D2F0). This script
resolves the candidate set; it does not choose.

Usage: scene_map.py --pes21-root DIR [--pesfmt PATH] [--json OUT] [--scene 0x20008 ...]
Prints per-category counts (scenes resolved, steps, variants, .fdc refs, .fdc missing). The --json
output is PES-derived: write it to scratch, never to the repo.
"""
import argparse
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_PESFMT = os.path.join(REPO, "build", "apps", "pesfmt_dump")

CATS = {1: "ent", 2: "foul", 3: "goal", 4: "pk", 5: "change", 6: "timeup", 7: "end", 8: "result", 9: "mode"}
DIR = "common/demo/fixdemo"
GOAL = 3
NO_SCENE = {
    0x20022: "foul: yellow-card 'no demo' sentinel (sanction builder: id 0x20022 frees and returns)",
    0x50010: "change: out-of-range 'no demo' sentinel (pes-re-scene-choice.md §2b)",
    0x7002b: "end: out-of-range 'no demo' sentinel (pes-re-scene-choice.md §2b)",
    0x301b6: "goal: out-of-range 'no demo' sentinel (pes-re-scene-choice.md §2b)",
}


def pesfmt(binary, root, *args):
    p = subprocess.run([binary, *args, "--pes21-root", root], capture_output=True)
    if p.returncode != 0:
        raise SystemExit("pesfmt_dump %s failed rc=%d: %s" % (" ".join(args), p.returncode,
                                                             p.stderr.decode(errors="replace")[:300]))
    return p.stdout


def decode(d, cat):
    """Decode one table_<cat>.bin. Returns dict(cutgroups, scenes, goal). Raises if any stream is left over."""
    H = struct.unpack_from("<14I", d, 0)
    nA, nS, nE, nB, nW, n8, sb, oA, oSe, oE, oB, oW, o8, oC = H
    A = struct.unpack_from("<%df" % nA, d, oA)
    B = struct.unpack_from("<%dI" % nB, d, oB)
    W = struct.unpack_from("<%dH" % nW, d, oW)
    U = d[o8:o8 + n8]
    ends = struct.unpack_from("<%dI" % nS, d, oSe)
    end_reads = struct.unpack_from("<%dI" % nE, d, oE)
    S, p = [], 0
    for e in ends:
        S.append(d[oC + p:oC + e].decode("latin1"))
        p = e
    ix = {"A": 0, "B": 0, "W": 0, "U": 0, "S": 0}

    def total():
        return sum(ix.values())

    def f():
        ix["A"] += 1
        return A[ix["A"] - 1]

    def u32():
        ix["B"] += 1
        return B[ix["B"] - 1]

    def u16():
        ix["W"] += 1
        return W[ix["W"] - 1]

    def u8():
        ix["U"] += 1
        return U[ix["U"] - 1]

    def s():
        ix["S"] += 1
        return S[ix["S"] - 1]

    def at_end():
        return total() >= end_reads[0]

    n = u16()
    ver = 0
    if n == 1:
        ver = 1
        n = u16()
    grp = [(u16(), u16()) for _ in range(n)]
    gname = [s() for _ in range(n)]
    m = u16()
    nk = u8()
    ns = u8()
    rows = []
    for _ in range(m):
        keys = [u16() for _ in range(nk)]
        fd = [s() for _ in range(ns)]
        b0 = u32()
        ff = f()
        b1 = u32()
        rows.append(dict(keys=["%04x" % k for k in keys], fdc=[x for x in fd if x], b0=b0, f=ff, b1=b1))
    n2 = u16()
    sgrp = [(u16(), u16()) for _ in range(n2)]
    sname = [s() for _ in range(n2)]
    m2 = u16()
    steps = []
    for _ in range(m2):
        kind = u8()
        fade = f()
        cut = u32()
        fl = u32()
        c0 = u8()
        c1 = u8()
        w0 = u16()
        w1 = u16()
        k = u8()
        tr = [(u8(), u8(), u8()) for _ in range(k)]
        w2 = u16() if ver == 1 else None
        steps.append(dict(kind=kind, fade=round(fade, 3), cut=cut, flags=fl, c0=c0, c1=c1, w0=w0,
                          w1=struct.unpack("<h", struct.pack("<H", w1))[0], trip=tr,
                          w2=None if w2 is None else struct.unpack("<h", struct.pack("<H", w2))[0]))
    goal = None
    if cat == GOAL:
        # Goal extras (pes-re-cutscenes.md §2.2): edit scene list, then (if not at end) playerId->scene
        # pairs; one 0x3c-byte row per T2 scene; then the optional T3 control table.
        ne = u16()
        edit = [u32() for _ in range(ne)]
        pairs = []
        if not at_end():
            npl = u16()
            pairs = [(u32(), u32()) for _ in range(npl)]
        scene_rows = []
        for _ in sgrp:
            b = [u8() for _ in range(5)]
            v0 = [f() for _ in range(4)]
            v1 = [f() for _ in range(4)]
            v2 = [f() for _ in range(4)]
            flag = [u8() for _ in range(4)]
            scene_rows.append(dict(b=b, v0=v0, v1=v1, v2=v2, flag=flag))
        ctrl = None
        if not at_end():
            ng = u16()
            cgrp = [(u16(), u16()) for _ in range(ng)]
            cname = [s() for _ in range(ng)]
            cm = u16()
            cnk = u8()
            crow = []
            for _ in range(cm):
                keys = [u16() for _ in range(cnk)]
                nf = u8()
                ni = u8()
                fs = [f() for _ in range(nf)]
                ids = [u32() for _ in range(ni)]
                crow.append(dict(keys=keys, f=fs, ids=ids))
            ctrl = dict(groups=cgrp, names=cname, rows=crow)
        goal = dict(edit_scenes=edit, player_scenes=pairs, scene_rows=len(scene_rows), ctrl=ctrl)
    used = dict(A=nA, B=nB, W=nW, U=n8, S=nS)
    got = dict(ix)
    if got != used or total() != end_reads[0]:
        raise SystemExit("cat %d: table not fully consumed: %s of %s, reads %d of end %d"
                         % (cat, got, used, total(), end_reads[0]))
    cutgroups = [dict(index=i, id=(cat << 16) | (i + 1), name=gname[i], rows=rows[g[0]:g[0] + g[1]])
                 for i, g in enumerate(grp)]
    scenes = [dict(index=i, id=(cat << 16) | (i + 1), name=sname[i], steps=steps[g[0]:g[0] + g[1]])
              for i, g in enumerate(sgrp)]
    return dict(cutgroups=cutgroups, scenes=scenes, version=ver, goal=goal)


def resolve(cat, table, fdc_set):
    """Attach the resolved chain to each scene: step -> cut group -> variants -> .fdc, with problems."""
    groups = {g["id"]: g for g in table["cutgroups"]}
    out = []
    for sc in table["scenes"]:
        chain, bad = [], []
        for st in sc["steps"]:
            cid = st["cut"]
            if cid == 0:
                break  # terminator (pes-re-cutscenes.md §2.3)
            g = groups.get(cid)
            if g is None or (cid >> 16) != cat:
                bad.append("step cut 0x%x not in cat %d" % (cid, cat))
                chain.append({"kind": st["kind"], "cut": cid, "variants": []})
                continue
            vs = []
            for r in g["rows"]:
                miss = [x for x in r["fdc"] if x not in fdc_set]
                if miss:
                    bad.append("missing %s" % miss[0])
                vs.append({"keys": r["keys"], "fdc": r["fdc"], "missing": miss})
            chain.append({"kind": st["kind"], "cut": cid, "group": g["name"], "variants": vs})
        if not chain:
            bad.append("scene has no steps")
        out.append({"id": sc["id"], "index": sc["index"], "name": sc["name"], "steps": chain, "problems": bad})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pes21-root", required=True)
    ap.add_argument("--pesfmt", default=DEFAULT_PESFMT)
    ap.add_argument("--json", default=None, help="write the resolved map here (scratch; PES-derived)")
    ap.add_argument("--scene", action="append", default=[], help="print one scene's chain, e.g. 0x20008")
    a = ap.parse_args()

    result, summary = {}, {}
    for cat, name in CATS.items():
        base = "%s/%s" % (DIR, name)
        d = pesfmt(a.pesfmt, a.pes21_root, "cat", "%s/table_%s.bin" % (base, name))
        table = decode(d, cat)
        listing = json.loads(pesfmt(a.pesfmt, a.pes21_root, "ls", base + "/cut_data/"))
        fdc_set = {os.path.basename(e["path"]) for e in listing}
        scenes = resolve(cat, table, fdc_set)
        result[name] = dict(cat=cat, version=table["version"], scenes=scenes, goal=table["goal"],
                            cutgroups=len(table["cutgroups"]), fdc_listed=len(fdc_set))
        n_ok = sum(1 for s in scenes if not s["problems"])
        steps = sum(len(s["steps"]) for s in scenes)
        variants = sum(len(st["variants"]) for s in scenes for st in s["steps"])
        refs = sum(len(v["fdc"]) for s in scenes for st in s["steps"] for v in st["variants"])
        missing = sum(len(v["missing"]) for s in scenes for st in s["steps"] for v in st["variants"])
        summary[name] = dict(scenes=len(scenes), resolved=n_ok, steps=steps, variants=variants, fdc_refs=refs,
                             fdc_missing=missing, cutgroups=len(table["cutgroups"]), fdc_listed=len(fdc_set))
        print("%-7s cat %d: scenes %3d resolved %3d | cut groups %4d | steps %4d | variants %5d | fdc refs %6d missing %d"
              % (name, cat, len(scenes), n_ok, len(table["cutgroups"]), steps, variants, refs, missing))

    total = {k: sum(v[k] for v in summary.values())
             for k in ("scenes", "resolved", "steps", "variants", "fdc_refs", "fdc_missing")}
    print("TOTAL scenes %(scenes)d resolved %(resolved)d | steps %(steps)d | variants %(variants)d | "
          "fdc refs %(fdc_refs)d missing %(fdc_missing)d" % total)
    g = result["goal"]["goal"]
    print("goal extras: edit scenes %d, playerId->scene pairs %d, T3 rows %s"
          % (len(g["edit_scenes"]), len(g["player_scenes"]),
             len(g["ctrl"]["rows"]) if g["ctrl"] else "none"))

    for sid_s in a.scene:
        sid = int(sid_s, 16)
        cat = (sid >> 16) & 0xFF
        if sid in NO_SCENE:
            print("%s: %s" % (sid_s, NO_SCENE[sid]))
            continue
        name = CATS.get(cat)
        sc = next((s for s in result[name]["scenes"] if s["id"] == sid), None) if name else None
        if sc is None:
            print("%s: no scene in cat %s (index %d)" % (sid_s, cat, (sid & 0xFFFF) - 1))
            continue
        print("%s = %s[%d] %s" % (sid_s, name, sc["index"], sc["name"]))
        for st in sc["steps"]:
            print("  step kind %d cut 0x%x %s" % (st["kind"], st["cut"], st.get("group", "")))
            for v in st["variants"]:
                print("    keys %s -> %s" % (",".join(v["keys"][:3]), ", ".join(v["fdc"]) or "-"))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(dict(summary=summary, total=total, categories=result), f, indent=1)
    return 0 if total["resolved"] == total["scenes"] else 1


if __name__ == "__main__":
    sys.exit(main())
