#!/usr/bin/env python3
"""snapdiff.py: the snapshot-backed diff of traced calls.

For each impure function, every traced call its frame traces recorded (frame_trace.py's
calls/<VA>.bin, one per snapshot) is replayed from the snapshot itself, twice:
  - the harness runs PES's bytes (diffharness/run.py --snapshot-dir): a fresh unicorn per call,
    the snapshot's pages copied on first touch, the call's recorded registers and return address;
  - the translated function runs in a diff main (diffmain.gen_snap) over the same snapshot pages.
Each replayed call's rax, full xmm0 and every written byte must match. Own-block coverage is
measured over the replayed calls (the union over snapshots). Floor: bit-exact on every replayed
call, and own-block coverage >= 0.5. Calls that leave the model are excluded with their reason.

Usage: snapdiff.py --image PES_EXE --out-dir BUILD --snapshot SNAPDIR=TRACEDIR [--snapshot ...]
                   [--only 0xVA ...] [--results FILE]
  SNAPDIR is a full_snapshot.py snapshot; TRACEDIR the frame_trace.py output that traced it.
"""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver as drv  # noqa: E402
import diffmain  # noqa: E402

FLOOR_COVERAGE = 0.50
CODE_LO, CODE_HI = 0x140001000, 0x14252E800  # the translated universe's image range (driver.build_and_diff)


def build_closure(va, spec, args, pilot, reached):
    """Translate va and every function its build links (direct callees in the pilot universe, the
    runnable out-of-set callees, and harness-reached indirect targets). Returns (need, build_error)."""
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    os.makedirs(gen, exist_ok=True)
    stubmap = drv.stubmap_of(spec)
    # Every image address the replay executed as an indirect site or target is a root: an indirect-only thunk
    # (reached through a table, never by a direct CALL) is a function the harness runs, so the build translates it.
    targets = {"0x%X" % int(t, 16) for tgts in reached.values() for t in tgts} | set(reached)
    # The harness runs each replayed indirect target's image bytes (no stub entry), and so do the walks of
    # their direct callees: the build closes over those too, or a plain callee is an out-of-set abort.
    reached_seed = {"harness": {"out_of_set_callees": [
        {"va": t, "how": "call"} for t in sorted(targets) if CODE_LO <= int(t, 16) < CODE_HI]}}
    extra, _ = drv.runnable_extra(spec, args.image)
    walked, _ = drv.runnable_extra(reached_seed, args.image)
    extra = list(extra) + [v for v in walked if v not in extra]
    build_pilot = set(pilot) | set(extra)
    cache = {va: spec}

    def calls_of(v):
        if v not in cache:
            cache[v] = drv.load_spec(args.spec_dir, v)[0]
        s = cache[v]
        return ["0x" + c.upper() for c in ((s or {}).get("precheck", {}) or {}).get("calls", [])]

    need, seen, queue = [va], {va}, [va]
    for c in extra:
        if c not in seen:
            seen.add(c)
            need.append(c)
            queue.append(c)
    targets = {"0x%X" % int(t, 16) for tgts in reached.values() for t in tgts}
    for t in sorted(targets):
        if CODE_LO <= int(t, 16) < CODE_HI and t not in seen:
            seen.add(t)
            need.append(t)
            queue.append(t)
    while queue:
        cur = queue.pop(0)
        for c in calls_of(cur):
            if c in build_pilot and c not in seen and c not in stubmap:
                seen.add(c)
                need.append(c)
                queue.append(c)
    # Translate the closure to a fixpoint. A tail jump to plain code outside the build is an OUT-OF-RANGE JMP the
    # translator would leave as a silent return; it joins the build as a direct callee, and every function is then
    # translated against the final universe. A pass that adds nothing is the fixpoint.
    while True:
        before = len(need)
        missing = set()
        for v in list(need):
            try:
                src, _ = drv.translate_one(v, drv.asm_for(args.asm_dir, v), args.image, build_pilot,
                                           stubs={**stubmap, **drv.ABORTMAP})
            except Exception as e:  # noqa: BLE001 - recorded, not raised
                return need, "translate %s: %r" % (v, e)[:220]
            if src is None:
                return need, "translate %s: no instructions" % v
            text = "\n".join(src)
            with open(os.path.join(gen, "pes_fn_%s.cpp" % v[2:]), "w") as f:
                f.write(text + "\n")
            for m in re.finditer(r"pes_fn_([0-9A-Fa-f]+)\(", text):
                dep = "0x" + m.group(1).upper()
                if dep != v and dep in build_pilot and dep not in seen:
                    seen.add(dep)
                    need.append(dep)
            # A tail jump or a direct call to plain image code (no stub entry) is run by the harness as image bytes:
            # the build translates it too. A call to a stubbed or blocked callee is excluded by the harness, not added.
            for m in re.finditer(r"OUT-OF-(?:RANGE JMP|SET CALL) 0x([0-9A-Fa-f]+)", text):
                t = "0x" + m.group(1).upper()
                if CODE_LO <= int(t, 16) < CODE_HI and t not in build_pilot and int(t, 16) not in drv.STUB_TABLE:
                    missing.add(t)
        if missing:
            build_pilot |= missing
        for t in sorted(missing):
            if t not in seen:
                seen.add(t)
                need.append(t)
        if len(need) == before and not missing:
            return need, None


def build_binary(va, spec, args, pilot, reached):
    """The diff main and the translated closure, compiled. Returns (binary path, error)."""
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    need, err = build_closure(va, spec, args, pilot, reached)
    if err:
        return None, err
    # The committed runtime headers, always refreshed (a stale copy in the build tree would link old code).
    for h in ("pes_state.hpp", "pes_cond.inl", "pes_stubs.hpp", "pes_dispatch.hpp"):
        with open(os.path.join(drv.TRANSLATED_DIR, h), "rb") as fi, open(os.path.join(gen, h), "wb") as fo:
            fo.write(fi.read())
    main_p = os.path.join(gen, "snapdiff_%s.cpp" % tag)
    with open(main_p, "w") as f:
        f.write(diffmain.gen_snap(va, [c for c in need if c != va]))
    subprocess.run([sys.executable, os.path.join(HERE, "gen_dispatch.py"), "--out", gen, "--vas"]
                   + sorted(need), check=True)
    bin_p = os.path.join(gen, "snapdiff_%s" % tag)
    objs = [os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]) for cv in need]
    rc, _, err = drv.sh([args.cxx, "-std=c++20", "-O1", "-I", gen, "-o", bin_p, main_p] + objs, timeout=300)
    if rc != 0:
        lines = [ln.replace(args.out_dir, "<build>") for ln in err.splitlines()
                 if "error" in ln or "undefined reference" in ln][:6]
        return None, "compile: %s" % " | ".join(lines)[:400]
    return bin_p, None


def load_rows(path):
    with open(path) as f:
        return [json.loads(ln) for ln in f if ln.strip()]


def first_write_diff(a, b):
    """Index of the first differing written byte (18 hex digits per byte), or the shorter length."""
    for i in range(0, min(len(a), len(b)), 18):
        if a[i:i + 18] != b[i:i + 18]:
            return i // 18
    return min(len(a), len(b)) // 18


def compare(label, corpus_rows, trans_path, mismatches):
    """Compare the translated outputs with the harness corpus, over the replayable (ok) rows in order."""
    want = [r for r in corpus_rows if r["ok"]]
    got = load_rows(trans_path)
    if len(got) != len(want):
        mismatches.append({"snapshot": label, "case": None,
                           "fields": ["count"], "note": "translated %d of %d replays" % (len(got), len(want))})
    for w, g in zip(want, got):
        fields = [f for f in ("rax", "xmm0", "w") if w[f] != g[f]]
        if fields:
            rec = {"snapshot": label, "case": w["case"], "fields": fields,
                   "harness": {f: w[f][:120] for f in fields}, "translated": {f: g[f][:120] for f in fields}}
            if "w" in fields:
                rec["first_write_diff"] = first_write_diff(w["w"], g["w"])
            mismatches.append(rec)
    return len(want)


def run_one(va, args, pilot, pairs):
    tag = va[2:]
    res = {"va": va, "calls": 0, "replayed": 0, "excluded": 0, "reasons": {}, "snapshots": {},
           "mismatches": [], "translated": False, "note": "", "own_block": None, "floor": False}
    spec, spec_path = drv.load_spec(args.spec_dir, va)
    if spec is None:
        res["note"] = "no spec"
        return res
    blocked_note = drv.blocked_reason(spec)
    extra_now, blocked_callees = drv.runnable_extra(spec, args.image)
    stub_vas = set(drv.stubmap_of(spec))
    blocked_callees = {v: r for v, r in blocked_callees.items() if v not in stub_vas}
    res["blocked_callees"] = sorted(blocked_callees)
    if blocked_note is not None:
        res["note"] = "blocked closure (harness excludes cases entering it): " + blocked_note[:160]
    gen = os.path.join(args.out_dir, "gen", tag)
    os.makedirs(gen, exist_ok=True)
    blocked_json = os.path.join(gen, "blocked.json")
    with open(blocked_json, "w") as f:
        json.dump(sorted(set(blocked_callees) | set(args.block_entry)), f)
    gen = os.path.join(args.out_dir, "gen", tag)
    os.makedirs(gen, exist_ok=True)
    runs, addrs, reached = [], set(), {}
    for snap, tdir in pairs:
        label = os.path.basename(snap.rstrip("/"))
        calls = os.path.join(tdir, "calls", tag + ".bin")
        if not os.path.exists(calls) or os.path.getsize(calls) == 0:
            continue
        out_s = os.path.join(gen, "snap_" + label)
        extra = ["--hooks", args.hooks] if args.hooks else []
        rc, out, err = drv.sh([drv.diff_python(), drv.RUN_PY, "--spec", spec_path, "--snapshot-dir", snap,
                               "--calls", calls, "--out-dir", out_s, "--blocked", blocked_json] + extra,
                              timeout=args.timeout)
        if rc != 0:
            res["note"] = "harness failed on %s: %s" % (label, (err or out).strip()[-200:])
            return res
        corpus = load_rows(os.path.join(out_s, "corpus.jsonl"))
        n_ok = sum(1 for r in corpus if r["ok"])
        res["calls"] += len(corpus)
        res["replayed"] += n_ok
        res["snapshots"][label] = {"calls": len(corpus), "replayed": n_ok}
        for r in corpus:
            if not r["ok"]:
                key = r["reason"].split(":")[0].split(" (")[0]
                res["reasons"][key] = res["reasons"].get(key, 0) + 1
        addrs |= set(json.load(open(os.path.join(out_s, "addrs.json"))))
        for site, tgts in json.load(open(os.path.join(out_s, "trace.json"))).items():
            reached.setdefault(site, {})
            for t in tgts:
                reached[site][t] = 1
        runs.append((label, snap, out_s, corpus, tdir))
    res["excluded"] = res["calls"] - res["replayed"]
    if res["replayed"] == 0:
        # Every case excluded: the reasons are the answer, so they go in the note (never silent).
        top = sorted(res["reasons"].items(), key=lambda kv: -kv[1])
        res["note"] = "NO REPLAYABLE CASE: %d of %d excluded; reasons: %s; blocked callees: %s" % (
            res["excluded"], res["calls"], "; ".join("%s x%d" % kv for kv in top) or "none recorded",
            ",".join(res.get("blocked_callees") or []) or "none")
        return res
    bin_p, err = build_binary(va, spec, args, pilot, reached)
    if err:
        res["note"] = err
        return res
    res["translated"] = True
    for label, snap, out_s, corpus, tdir in runs:
        trans = os.path.join(out_s, "translated.jsonl")
        rc, out, err = drv.sh([bin_p, os.path.join(out_s, "ok_calls.bin"), os.path.join(snap, "mem.bin"),
                               os.path.join(out_s, "maps.txt"), trans, os.path.join(out_s, "ok_delta.bin"),
                               os.path.join(tdir, "pool", "pages.bin")], timeout=args.timeout)
        if rc != 0:
            res["mismatches"].append({"snapshot": label, "case": None, "fields": ["run"],
                                      "note": "diff main rc=%d: %s" % (rc, (err or out)[-200:])})
            continue
        compare(label, corpus, trans, res["mismatches"])
    cov = drv.block_coverage(args.image, [va], sorted(addrs)).get(va)
    if cov is not None and cov[1]:
        res["own_block"] = {"reached": cov[0], "total": cov[1], "fraction": round(cov[0] / cov[1], 4)}
    frac = res["own_block"]["fraction"] if res["own_block"] else 0.0
    res["floor"] = not res["mismatches"] and res["replayed"] > 0 and frac >= FLOOR_COVERAGE
    return res


def norm_tag(s):
    """A VA as the bare upper-case hex tag the rest of the tool keys on: '0x1404f6c30' -> '1404F6C30'."""
    s = s.strip()
    if s[:2].lower() == "0x":
        s = s[2:]
    return s.upper()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--spec-dir", default=drv.DEF_SPEC_DIR)
    ap.add_argument("--asm-dir", default=None)
    ap.add_argument("--snapshot", action="append", required=True,
                    help="SNAPDIR=TRACEDIR (repeatable): a snapshot and the frame_trace run that traced it")
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--cxx", default="g++")
    ap.add_argument("--timeout", type=int, default=3600)
    ap.add_argument("--results", default=None)
    ap.add_argument("--hooks", default=None,
                    help="rv.dll config (rendezvous captures): each hook entry replays as the code it displaced")
    ap.add_argument("--block-entry", action="append", default=[],
                    help="an extra VA a replayed case must not enter (proves an init path unreached)")
    args = ap.parse_args()
    pairs = [tuple(s.split("=", 1)) for s in args.snapshot]
    pilot = drv.universe_of(args.spec_dir)
    if args.only:
        tags = [norm_tag(v) for v in args.only]
    else:
        tags = set()
        for _, tdir in pairs:
            cdir = os.path.join(tdir, "calls")
            if os.path.isdir(cdir):
                tags |= {f[:-4] for f in os.listdir(cdir) if f.endswith(".bin")}
        tags = sorted(tags)
    results = []
    for tg in tags:
        va = "0x" + tg
        r = run_one(va, args, pilot, pairs)
        results.append(r)
        print("%s calls=%d replayed=%d mismatches=%d own_block=%s floor=%s %s" % (
            r["va"], r["calls"], r["replayed"], len(r["mismatches"]),
            r["own_block"] and r["own_block"]["fraction"], r["floor"], r["note"][:400]), flush=True)
    floor = sum(1 for r in results if r["floor"])
    doc = {"note": "snapshot-backed diff of traced calls (snapdiff.py): each traced call replayed from its "
                   "snapshot's own memory on the harness and the translated side; floor = bit-exact on every "
                   "replayed call and own-block coverage >= 0.5 over the replayed calls.",
           "floor_count": floor, "results": results}
    if args.results:
        with open(args.results, "w") as f:
            json.dump(doc, f, indent=1)
    print("floor %d of %d reached" % (floor, len(results)), flush=True)


if __name__ == "__main__":
    main()
