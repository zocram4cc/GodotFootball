#!/usr/bin/env python3
"""gen_translated.py: the committed translated layer (src/pes/translated).

Each bit-exact record is translated under the universe and stub map its own
diff run used (driver.universe_of + the function's runnable extras), and so is
every direct callee that diff build compiled, transitively (driver.build_and_diff).
Each committed body is therefore the translation its record's proof used.
A callee that is not itself recorded bit-exact is committed under closure/,
with a banner naming its harness record: the recorded callers link it, and
its behaviour is proven only through their diffs.

Output:
  <root>/<subsystem>/pes_fn_<VA>.cpp   recorded functions, one per file
  <root>/closure/pes_fn_<VA>.cpp       callee closure members
  <root>/translated.hpp                declarations of every committed function
  <root>/pes_dispatch_gen.hpp          indirect-call table over the same set

Usage:
  gen_translated.py --image IMG [--root DIR]     regenerate the layer
  gen_translated.py --image IMG --check          exit 1 on any drift

A committed function whose first three lines contain "hand-refined" is not
compared: its bit-exact proof is the harness check, not this regeneration.
"""
import argparse
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import driver  # noqa: E402  (the universe, stub map and translator the diff runs use)
import gen_dispatch  # noqa: E402
import xlate  # noqa: E402

ROOT = os.path.join(driver.REPO, "src", "pes", "translated")
# (committed results file, spec subdirectory of tools/dev/diffharness/specs).
SETS = (("results.json", "pilot"), ("results_impure.json", "impure"),
        ("results_cutscene.json", "cutscene"), ("results_presentation.json", "presentation"),
        ("results_core.json", "core"), ("results_fixdemo.json", "fixdemo"))
CALLEE = re.compile(r"pes_fn_([0-9A-Fa-f]+)\(")
HAND_REFINED = "hand-refined"
CLOSURE = "closure"


def spec_dir(name):
    return os.path.join(driver.REPO, "tools", "dev", "diffharness", "specs", name)


def records():
    """[(VA, set name, diffed, failure class)] for every record, in set order."""
    out = []
    for fname, name in SETS:
        with open(os.path.join(HERE, fname)) as f:
            for r in json.load(f)["results"]:
                out.append((r["va"], name, bool(r.get("diffed")), r.get("failure_class")))
    return out


def context(name, va, image):
    """Spec, stub map and universe that a diff run of va in set name translates under."""
    sdir = spec_dir(name)
    spec, _ = driver.load_spec(sdir, va)
    if spec is None:
        raise SystemExit("gen_translated: no spec for %s in %s" % (va, name))
    stubs = driver.MODEL_MAP
    universe = driver.universe_of(sdir) | set(driver.runnable_extra(spec, image)[0])
    return spec, stubs, universe


def translate(va, image, stubs, universe):
    src, _ = driver.translate_one(va, "", image, universe, stubs=stubs)
    if src is None:
        raise SystemExit("gen_translated: %s does not translate" % va)
    return "\n".join(src) + "\n"

# Markers the translator emits where a callee is outside the set's universe (not the runtime-dispatch fallback,
# which every indirect call carries).
GAP_MARKERS = ("/*OUT-OF-SET CALL ", "OUT-OF-RANGE JMP")


def has_gap(text):
    return any(m in text for m in GAP_MARKERS)



def subsystem_of(spec, name):
    return (spec.get("subsystem") or name).lower().replace(" ", "_").replace("-", "_")


def build(image):
    """{VA: member} for the whole layer; member = raw text, recorded flag, subsystem, harness status."""
    recs = records()
    bit = [(va, name) for va, name, diffed, _ in recs if diffed]
    bit_vas = {va for va, _ in bit}
    sub_of, status = {}, {}
    for va, name in bit:
        if va not in sub_of:
            spec, _ = driver.load_spec(spec_dir(name), va)
            sub_of[va] = subsystem_of(spec, name)
    for va, _, diffed, fc in recs:
        status.setdefault(va, "bit-exact" if diffed else (fc or "not diffed"))
    # Each recorded function's closure (itself plus the callees it links), translated once.
    texts, closure_of, conflicts = {}, {}, []
    for va, name in bit:
        _, stubs, universe = context(name, va, image)
        queue, seen = [va], {va}
        while queue:
            cur = queue.pop(0)
            text = translate(cur, image, stubs, universe)
            old = texts.get(cur)
            if old is not None and old != text:
                # Sets translate under their own universes: a callee one set lacks becomes a gap there (a missing
                # dispatch, an out-of-range jump that returns). The layer keeps the variant without gaps, which some
                # set proved; two variants that differ otherwise, or both with gaps, are a real conflict.
                old_gap, new_gap = has_gap(old), has_gap(text)
                if old_gap and not new_gap:
                    texts[cur] = text
                elif old_gap == new_gap:
                    conflicts.append("%s translates differently under %s (gap: layer %s, this set %s; first set %s)"
                                     % (cur, name, old_gap, new_gap, sub_of.get(cur, "closure")))
            texts.setdefault(cur, text)
            for c in CALLEE.findall(text):
                cv = "0x" + c.upper()
                if cv != cur and cv in universe and cv not in seen:
                    seen.add(cv)
                    queue.append(cv)
        closure_of[va] = seen
    if conflicts:
        raise SystemExit("gen_translated: %d conflicts:\n  %s" % (len(conflicts), "\n  ".join(conflicts[:30])))
    # Never ship a callee the harness recorded as mistranslated: a recorded function whose closure reaches one is left
    # out of the layer (listed), though its own diff passed, since its cases never took the callee's bad path.
    bad = {v for v in texts if v not in bit_vas and status.get(v) == "translator bug"}
    held = sorted(va for va, _ in bit if closure_of[va] & bad)
    for va in held:
        print("gen_translated: held out %s: its closure reaches a mistranslated callee (%s)"
              % (va, " ".join(sorted(closure_of[va] & bad))))
    members = {}
    for va, _ in bit:
        if va in held:
            continue
        for cur in closure_of[va]:
            members.setdefault(cur, {"raw": texts[cur], "recorded": cur in bit_vas,
                                     "sub": sub_of.get(cur, CLOSURE), "status": status.get(cur, "no record")})
    for cur, m in members.items():
        for c in CALLEE.findall(m["raw"]):
            if "0x" + c.upper() not in members and "0x" + c.upper() != cur:
                raise SystemExit("gen_translated: %s calls %s, which is not committed" % (cur, c))
    return members


def lint(members):
    """Fail loudly on a committed function that reads a read-only image address live (the GF runtime maps no image
    there, so the read returns 0 while the harness, which maps the image flat, stays bit-exact). Each must be baked by
    the translator or served by a binding listed in runtime_reads.json."""
    with open(xlate.RUNTIME_READS) as f:
        bindings = json.load(f)
    bad = xlate.unserved_reads({va: r for va, r in xlate.LIVE_RO.items() if va in members}, bindings)
    if bad:
        raise SystemExit("gen_translated: %d live read(s) of a read-only image address no bake or runtime binding serves "
                         "(bake in xlate, or list the binding in runtime_reads.json):\n  %s"
                         % (len(bad), "\n  ".join("function %s site 0x%X reads image 0x%X" % b for b in bad)))


def member_text(va, m):
    if m["recorded"]:
        return m["raw"]
    return ("// Closure member: a direct callee of recorded functions, committed so they link. "
            "Harness record: %s. Not recorded bit-exact on its own.\n" % m["status"]) + m["raw"]


def layer_files(members):
    """{relative path: text} for the whole committed layer."""
    files = {}
    vas = sorted(members)
    for va in vas:
        m = members[va]
        files[os.path.join(m["sub"], "pes_fn_%s.cpp" % va[2:])] = member_text(va, m)
    head = ["// Generated by tools/dev/translate/gen_translated.py from the bit-exact records. Do not edit.",
            "// Declares every committed translated function (PES VA -> pes_fn_<VA>).",
            "#pragma once",
            "struct PesState;"]
    files["translated.hpp"] = "\n".join(head + ["void pes_fn_%s(PesState& s);" % va[2:] for va in vas]) + "\n"
    files["pes_dispatch_gen.hpp"] = gen_dispatch.text(vas)
    return files


def write_layer(root, files):
    for old in glob.glob(os.path.join(root, "*", "pes_fn_*.cpp")):
        os.remove(old)
    for rel, text in files.items():
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)
    for d in glob.glob(os.path.join(root, "*")):
        if os.path.isdir(d) and not os.listdir(d):
            os.rmdir(d)


def check_layer(root, files):
    """Drift lines: a committed file that differs, is missing, or is unexpected."""
    drift = []
    have = {os.path.relpath(p, root) for p in glob.glob(os.path.join(root, "*", "pes_fn_*.cpp"))}
    for rel, text in sorted(files.items()):
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            drift.append("missing %s" % rel)
            continue
        with open(path) as f:
            cur = f.read()
        if cur == text or HAND_REFINED in "".join(cur.splitlines(True)[:3]):
            continue
        drift.append("differs %s" % rel)
    drift += ["unexpected %s" % rel for rel in sorted(have - set(files))]
    return drift


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--check", action="store_true",
                    help="regenerate in memory and fail on any drift from the committed layer")
    args = ap.parse_args()
    members = build(args.image)
    lint(members)
    files = layer_files(members)
    recorded = sum(1 for m in members.values() if m["recorded"])
    summary = "%d functions (%d recorded bit-exact, %d closure)" % (
        len(members), recorded, len(members) - recorded)
    if args.check:
        drift = check_layer(args.root, files)
        for line in drift[:40]:
            print(line)
        if drift:
            print("gen_translated --check: %d drift(s) from the image; %s" % (len(drift), summary))
            return 1
        print("gen_translated --check: %s regenerate byte-identical" % summary)
        return 0
    write_layer(args.root, files)
    print("gen_translated: wrote %s to %s" % (summary, args.root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
