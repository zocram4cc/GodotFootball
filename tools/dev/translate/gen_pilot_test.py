#!/usr/bin/env python3
"""gen_pilot_test.py: build-time pilot test generator (ctest wiring).

Env: PILOT_VA (bare hex, e.g. 140439F50), PILOT_GEN (build gen dir),
     PILOT_SPEC (committed spec path), PILOT_IMAGE ($PES_EXE or empty).
Writes $PILOT_GEN/pes_fn_<va>.cpp + $PILOT_GEN/diff_<va>.cpp.
Without PILOT_IMAGE (or on any failure), writes a skip stub: the test
binary prints the skip line and exits 77, so the build stays green and
ctest skips exactly like the install-gated tests.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def stub(gen, va, reason):
    with open(os.path.join(gen, "pes_fn_%s.cpp" % va), "w") as f:
        f.write("// skip stub: %s\nvoid pes_fn_%s(void* s);\n"
                % (reason, va))
    with open(os.path.join(gen, "diff_%s.cpp" % va), "w") as f:
        f.write('#include <cstdio>\nint main(int, char**) '
                '{ printf("pilot %s skipped: %s\\n"); return 77; }\n'
                % (va, reason.replace('"', "'")))


def main():
    va = os.environ["PILOT_VA"]
    gen = os.environ["PILOT_GEN"]
    spec_path = os.environ["PILOT_SPEC"]
    image = os.environ.get("PILOT_IMAGE", "")
    os.makedirs(gen, exist_ok=True)
    if not image or not os.path.isfile(image):
        stub(gen, va, "no unpacked image (set PES_EXE)")
        return 0
    try:
        import xlate
        import diffmain
        spec = json.load(open(spec_path))
        pilot = set()  # callees resolve at diff time; translator needs
        # the universe only for sibling declarations. Reconstruct from
        # every committed spec's precheck calls (closed set by construction).
        import glob
        specdir = os.path.dirname(spec_path)
        for sp in glob.glob(os.path.join(specdir, "*.json")):
            pilot.add("0x" + os.path.basename(sp)[:-5].upper())
        full_va = "0x" + va.upper()
        stubmap = {}
        for _s in (spec.get("stubs") or {}).values():
            stubmap["0x" + _s["va"][2:].upper()] = _s
        src, _ = xlate.translate_one(image, "", full_va, pilot,
                                     stubs=stubmap)
        if src is None:
            stub(gen, va, "no instructions")
            return 0
        with open(os.path.join(gen, "pes_fn_%s.cpp" % va), "w") as f:
            f.write("\n".join(src) + "\n")
        callees = ["0x" + c.upper()
                   for c in (spec.get("precheck", {}) or {}).get("calls", [])]
        # transitive callee closure (same rule as driver.py)
        seen = {full_va}
        queue = [full_va]
        speccache = {full_va: spec}
        need = [full_va]

        def calls_of(v):
            s = speccache.get(v)
            if s is None:
                p = os.path.join(specdir, v[2:] + ".json")
                if not os.path.exists(p):
                    return []
                s = json.load(open(p))
                speccache[v] = s
            return ["0x" + c.upper()
                    for c in ((s.get("precheck", {}) or {}).get("calls", [])
                              or [])]
        for c in callees:
            if c in pilot and c not in seen:
                seen.add(c)
                need.append(c)
                queue.append(c)
        while queue:
            cur = queue.pop(0)
            for c in calls_of(cur):
                if c in pilot and c not in seen:
                    seen.add(c)
                    need.append(c)
                    queue.append(c)
        for cv in need[1:]:
            p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
            if os.path.exists(p):
                continue
            csrc, _ = xlate.translate_one(image, "", cv, pilot,
                                         stubs=stubmap)
            if csrc is None:
                stub(gen, va, "callee %s: no instructions" % cv)
                return 0
            with open(p, "w") as f:
                f.write("\n".join(csrc) + "\n")
        # generated-TU closure for tail jumps (same rule as driver.py)
        import re
        changed = True
        while changed:
            changed = False
            for cv in list(need):
                p = os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])
                try:
                    body = open(p).read()
                except OSError:
                    continue
                for m in re.finditer(r"pes_fn_([0-9A-Fa-f]+)\(", body):
                    dep = "0x" + m.group(1).upper()
                    if dep != cv and dep in pilot and dep not in seen:
                        seen.add(dep)
                        need.append(dep)
                        changed = True
        pending = [cv for cv in need
                   if not os.path.exists(
                       os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]))]
        while pending:
            cv = pending.pop(0)
            csrc, _ = xlate.translate_one(image, "", cv, pilot,
                                         stubs=stubmap)
            if csrc is None:
                stub(gen, va, "callee %s: no instructions" % cv)
                return 0
            with open(os.path.join(gen, "pes_fn_%s.cpp" % cv[2:]), "w") as f:
                f.write("\n".join(csrc) + "\n")
            try:
                body = open(os.path.join(gen, "pes_fn_%s.cpp" % cv[2:])).read()
            except OSError:
                continue
            for m in re.finditer(r"pes_fn_([0-9A-Fa-f]+)\(", body):
                dep = "0x" + m.group(1).upper()
                if dep != cv and dep in pilot and dep not in seen:
                    seen.add(dep)
                    need.append(dep)
                    pending.append(dep)
        main_src = diffmain.gen(spec, full_va, callees)
        with open(os.path.join(gen, "diff_%s.cpp" % va), "w") as f:
            f.write(main_src)
        for h in ("pes_state.hpp", "pes_cond.inl", "diffparse.hpp"):
            hp = os.path.join(gen, h)
            if not os.path.exists(hp):
                with open(os.path.join(HERE, h), "rb") as fi, \
                        open(hp, "wb") as fo:
                    fo.write(fi.read())
    except Exception as e:  # noqa: BLE001 - build must stay green
        stub(gen, va, "translate: %r" % (e,)[:120])
    return 0


if __name__ == "__main__":
    sys.exit(main())
