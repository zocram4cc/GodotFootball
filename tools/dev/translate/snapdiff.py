#!/usr/bin/env python3
"""snapdiff.py: the snapshot-backed diff of traced calls.

For each impure function, every traced call its frame traces recorded (frame_trace.py's
calls/<VA>.bin, one per snapshot) is replayed from the snapshot itself, twice:
  - the harness runs PES's bytes (diffharness/run.py --snapshot-dir): a fresh unicorn per call,
    the snapshot's pages copied on first touch, the call's recorded registers and return address;
  - the translated function runs in a diff main (diffmain.gen_snap) over the same snapshot pages.
Each replayed call's rax, full xmm0 and every written byte must match. Own-block coverage is
measured over the replayed calls (the union over snapshots) and recorded, not gated. Proven: the closure
built, every replayed call bit-exact, and no call excluded. Calls that leave the model are excluded with their reason.

Usage: snapdiff.py --image PES_EXE --out-dir BUILD --snapshot SNAPDIR=TRACEDIR [--snapshot ...]
                   [--only 0xVA ...] [--results FILE]
  SNAPDIR is a full_snapshot.py snapshot; TRACEDIR the frame_trace.py output that traced it.
"""
import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver as drv  # noqa: E402
import diffmain  # noqa: E402
import frame_trace as ft  # noqa: E402
import pdata  # noqa: E402
import full_snapshot as fs  # noqa: E402
import stubs as stub_table  # noqa: E402

JOBS = max(1, min(4, os.cpu_count() or 1))  # translation units compiled at once per closure
CODE_LO, CODE_HI = 0x140001000, 0x14252E800  # the translated universe's image range (driver.build_and_diff)


_PDATA = {}


def boundary_map(image):
    """The image's own Pdata (function extents, boundaries, indirect-branch kinds), opened once."""
    if image not in _PDATA:
        _PDATA[image] = pdata.Pdata(open(image, "rb"))  # kept open: indirect_kind and decode read the image
    return _PDATA[image]


def build_closure(va, spec, args, pilot, reached):
    """Translate va and every function its build links (direct callees in the pilot universe, the
    runnable out-of-set callees, and harness-reached indirect targets). Returns (need, build_error)."""
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    os.makedirs(gen, exist_ok=True)
    # Every image address the replay executed as an indirect target is a root, and so is an indirect site that starts a
    # function (an indirect-only thunk, reached through a table and never by a direct CALL, is a function the harness
    # runs). A site inside a function is no root: the function holding it is already in the closure, and translating
    # from mid-function decodes garbage (0x140864175 is a `call [rdx]` inside 0x140864084). Nor is a target that lies in
    # the same function as its site: a `jmp reg` switch case (0x1406EA953, inside 0x1406EA840). A `jmp reg` site is a
    # switch dispatch, never a function start, and its targets in the same body are cases even where no .pdata entry
    # says where the leaf ends (0x140848B40 and its case 0x140848B6E, inside the leaf 0x140848AF0). Any other target
    # that is not a function boundary fails the closure.
    pd = boundary_map(args.image)
    roots = set()
    for site, tgts in reached.items():
        sv = int(site, 16)
        reg_jmp = pd.indirect_kind(sv) == "jmp reg"
        for t in tgts:
            tv = int(t, 16)
            if reg_jmp and pd.same_body(sv, tv):
                continue
            if not CODE_LO <= tv < CODE_HI or pd.is_boundary(tv):
                roots.add("0x%X" % tv)
            elif pd.containing(tv) != pd.containing(sv):
                return [va], "closure walk: indirect target %s (from site %s) is not a function boundary" % (t, site)
    targets = roots | {s for s in reached
                       if pd.is_boundary(int(s, 16)) and pd.indirect_kind(int(s, 16)) != "jmp reg"}
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
    targets = roots  # the filtered indirect targets and function-start sites: a switch case is no root
    for t in sorted(targets):
        if CODE_LO <= int(t, 16) < CODE_HI and t not in seen:
            seen.add(t)
            need.append(t)
            queue.append(t)
    while queue:
        cur = queue.pop(0)
        for c in calls_of(cur):
            if c in build_pilot and c not in seen and c not in drv.MODEL_MAP:
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
                                           stubs=drv.MODEL_MAP)
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
                    if not pd.is_boundary(int(t, 16)):
                        # An address inside a function or an instruction is no callee: the build never translates it,
                        # and the closure is reported as not built rather than compared without it.
                        return need, "closure walk: %s (from %s) is not a function boundary" % (t, v)
                    missing.add(t)
        if missing:
            build_pilot |= missing
        for t in sorted(missing):
            if t not in seen:
                seen.add(t)
                need.append(t)
        if len(need) == before and not missing:
            return need, None


def build_binary(va, spec, args, pilot, reached, native=False):
    """The diff main and the translated closure, compiled. Returns (binary path, error). native: the production
    dispatch (gen_dispatch --native), GF's PesHeap attached per call, and PES's allocator targets left out of the closure."""
    tag = va[2:]
    gen = os.path.join(args.out_dir, "gen", tag)
    need, err = build_closure(va, spec, args, pilot, reached)
    if err:
        return None, err
    # The committed runtime headers, always refreshed (a stale copy in the build tree would link old code).
    heads = ("pes_state.hpp", "pes_cond.inl", "pes_stubs.hpp", "pes_dispatch.hpp") + (("pes_native.hpp",) if native else ())
    for h in heads:
        with open(os.path.join(drv.TRANSLATED_DIR, h), "rb") as fi, open(os.path.join(gen, h), "wb") as fo:
            fo.write(fi.read())
    if native:
        need = [c for c in need if c not in {"0x%X" % v for v in stub_table.natives()}]
        with open(os.path.join(drv.TRANSLATED_DIR, "pes_native.cpp"), "rb") as fi, \
                open(os.path.join(gen, "pes_native.cpp"), "wb") as fo:
            fo.write(fi.read())
    sfx = "_native" if native else ""
    main_p = os.path.join(gen, "snapdiff_%s%s.cpp" % (tag, sfx))
    with open(main_p, "w") as f:
        f.write(diffmain.gen_snap(va, [c for c in need if c != va], native=native, vec_arg=args.vec_arg))
    subprocess.run([sys.executable, os.path.join(HERE, "gen_dispatch.py"), "--out", gen]
                   + (["--native"] if native else []) + ["--vas"] + sorted(need), check=True)
    bin_p = os.path.join(gen, "snapdiff_%s%s" % (tag, sfx))
    srcs = [main_p] + [os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]) for cv in need]
    if native:
        srcs.append(os.path.join(gen, "pes_native.cpp"))
    trace = ["-include", os.path.join(HERE, "lockstep_trace.h")] if os.environ.get("GF_XLATE_TRACE") else []

    def compile_one(src):
        obj = src[:-4] + ".o"
        rc, _, err = drv.sh([args.cxx, "-std=c++20", "-O1", "-I", gen] + trace + ["-c", "-o", obj, src],
                            timeout=600)
        return obj, rc, err

    # One translation unit per process, JOBS at a time. A closure of ~1500 functions in one g++ run outlasts
    # its timeout and the note came back blank (140446390): the errors are per unit, and a timeout is named.
    with concurrent.futures.ThreadPoolExecutor(max_workers=JOBS) as pool:
        built = list(pool.map(compile_one, srcs))
    bad = [b for b in built if b[1] != 0]
    if bad:
        _, rc_, err_ = bad[0]
        lines = [ln.replace(args.out_dir, "<build>") for ln in err_.splitlines()
                 if "error" in ln or "undefined reference" in ln or "timeout" in ln][:6]
        return None, "compile (%d of %d units failed, rc %d): %s" % (
            len(bad), len(built), rc_, " | ".join(lines)[:400] or err_[-200:])
    rc, _, err = drv.sh([args.cxx, "-o", bin_p] + [obj for obj, _, _ in built], timeout=600)
    if rc != 0:
        lines = [ln.replace(args.out_dir, "<build>") for ln in err.splitlines()
                 if "error" in ln or "undefined reference" in ln or "timeout" in ln][:6]
        return None, "link (rc %d): %s" % (rc, " | ".join(lines)[:400] or err[-200:])
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


def compare_native(label, trans_path, native_path, mismatches):
    """Compare the native-heap run with the translated run on the same snapshot: rax, xmm0 and the vector output ("vec")
    must match; the heap's own addresses and bytes are not outputs. Returns the number of cases compared."""
    want, got = load_rows(trans_path), load_rows(native_path)
    if len(got) != len(want):
        mismatches.append({"snapshot": label, "case": None, "fields": ["native count"],
                           "note": "native run %d of %d" % (len(got), len(want))})
    for w, g in zip(want, got):
        fields = [f for f in ("rax", "xmm0", "vec") if w.get(f) != g.get(f)]
        if fields:
            mismatches.append({"snapshot": label, "case": w["case"], "fields": ["native " + f for f in fields],
                               "translated": {f: str(w.get(f))[:120] for f in fields},
                               "native": {f: str(g.get(f))[:120] for f in fields}})
    return len(want)


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
           "mismatches": [], "translated": False, "note": "", "own_block": None, "proven": False}
    spec, spec_path = drv.load_spec(args.spec_dir, va)
    if spec is None:
        res["note"] = "no spec"
        return res
    blocked_note = drv.blocked_reason(spec)
    extra_now, blocked_callees = drv.runnable_extra(spec, args.image)
    stub_vas = set(drv.MODEL_MAP)
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
    nbin_p = None
    if not err and args.native:
        nbin_p, err = build_binary(va, spec, args, pilot, reached, native=True)
    if err:
        # A closure that did not build compares nothing: its replays are unverified, never passed.
        res["unverified"] = res["replayed"]
        res["replayed"] = 0
        res["excluded"] = res["calls"]
        res["note"] = err
        return res
    res["translated"] = True
    for label, snap, out_s, corpus, tdir in runs:
        trans = os.path.join(out_s, "translated.jsonl")
        teb = hex(ft.trace_teb(tdir, fs.SnapMem(snap)))  # the GS base of each run: the trace's TEB, or the snapshot's
        rc, out, err = drv.sh([bin_p, os.path.join(out_s, "ok_calls.bin"), os.path.join(snap, "mem.bin"),
                               os.path.join(out_s, "maps.txt"), trans, os.path.join(out_s, "ok_delta.bin"),
                               os.path.join(tdir, "pool", "pages.bin"), teb], timeout=args.timeout)
        if rc != 0:
            res["mismatches"].append({"snapshot": label, "case": None, "fields": ["run"],
                                      "note": "diff main rc=%d: %s" % (rc, (err or out)[-200:])})
            continue
        compare(label, corpus, trans, res["mismatches"])
        if nbin_p:
            ntrans = os.path.join(out_s, "native.jsonl")
            rc, out, err = drv.sh([nbin_p, os.path.join(out_s, "ok_calls.bin"), os.path.join(snap, "mem.bin"),
                                   os.path.join(out_s, "maps.txt"), ntrans, os.path.join(out_s, "ok_delta.bin"),
                                   os.path.join(tdir, "pool", "pages.bin"), teb], timeout=args.timeout)
            if rc != 0:
                res["mismatches"].append({"snapshot": label, "case": None, "fields": ["native run"],
                                          "note": "native main rc=%d: %s" % (rc, (err or out)[-200:])})
            else:
                res["native_compared"] = res.get("native_compared", 0) + compare_native(
                    label, trans, ntrans, res["mismatches"])
    cov = drv.block_coverage(args.image, [va], sorted(addrs)).get(va)
    if cov is not None and cov[1]:
        res["own_block"] = {"reached": cov[0], "total": cov[1], "fraction": round(cov[0] / cov[1], 4)}
    res["proven"] = (bool(res["translated"]) and not res["mismatches"] and res["replayed"] > 0
                     and res["excluded"] == 0)
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
    ap.add_argument("--native", action="store_true",
                    help="also run each replayed call on GF's native heap (production dispatch) and compare it with the "
                         "translated run on rax, xmm0 and --vec-arg's vector (pes_native.hpp)")
    ap.add_argument("--vec-arg", type=int, default=None,
                    help="GPR index of a std::vector<u32>* argument whose elements each run reports (2 = rdx)")
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
        print("%s calls=%d replayed=%d mismatches=%d own_block=%s proven=%s %s" % (
            r["va"], r["calls"], r["replayed"], len(r["mismatches"]),
            r["own_block"] and r["own_block"]["fraction"], r["proven"], r["note"][:400]), flush=True)
    proven = sum(1 for r in results if r["proven"])
    doc = {"note": "snapshot-backed diff of traced calls (snapdiff.py): each traced call replayed from its "
                   "snapshot's own memory on the harness and the translated side; proven = the closure built, "
                   "every replayed call bit-exact, no call excluded. Own-block coverage is recorded, not gated.",
           "proven_count": proven, "results": results}
    if args.results:
        with open(args.results, "w") as f:
            json.dump(doc, f, indent=1)
    print("proven %d of %d" % (proven, len(results)), flush=True)
    # A verdict with nothing compiled compared nothing: that is an error exit, never a pass.
    nothing = [r["va"] for r in results if not r["translated"]]
    if nothing:
        print("ERROR: nothing compiled for %s (no replay was compared with a translated body)" % ", ".join(nothing),
              file=sys.stderr, flush=True)
        sys.exit(1)


if __name__ == "__main__":
    main()
