"""Spec rules: the generator's repairs, each one a fix the committed specs got by hand (provenance per rule).

Pilot mode (the pilot and cutscene sets):
- case_varied_globals: the .data globals that five functions read become case-varied inputs (95f986f: 141E532D0,
  141E532E0, 14149A470 and their callers 14042D470, 1404C8DC0). The writer scan classes them CONST.
- audit_outputs: a spec checks every return register its body writes (5484011; translate/spec_outputs.py).

Impure mode (the impure sample):
- closure_fix: the G record accessors lay out their slots from the image bytes (base, count and fallback are read
  from each accessor's code), the blanket E1EE slots the generator put on G are removed, and G's child links are
  restored (docs/re/pes-re-indirect.md §Spec fixes; bb2c491; fixclosure.py).
- drop_dangling: a regions_init entry that names a region the spec does not declare is dropped (bb2c491).
"""
import os
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

from . import analyze as AN
from . import classify as C
from .sets import REPO

sys.path.insert(0, os.path.join(REPO, "tools", "dev", "translate"))
import spec_outputs  # noqa: E402
import xlate  # noqa: E402

G = "IMG.1436F3DC8"  # G, the global record table (VA 0x1436F3DC8)
REC = G + ".E1EE"  # the element object every blanket slot pointed at
STRIDE = 0x28  # bytes per G element: an accessor's lea rax,[rax+rax*4] then *8
ACCESSORS = (0x140A489A0, 0x140A48A10, 0x140A48910, 0x140A489F0, 0x140A48A30)  # record accessors of G
# function VA -> the .data globals it reads case-varied (95f986f); each global's writer scan class is CONST
CASE_VARIED = {0x14042D470: (0x14351AE54,), 0x1404C8DC0: (0x14351AE54,), 0x141E532D0: (0x14351AE54,),
               0x14149A470: (0x143704E38,), 0x141E532E0: (0x14351AE50,)}
ACCESSOR_SHAPE = ["cmp", "jae", "movsxd", "add", "lea", "mov", "ret", "mov", "ret"]
_md = Cs(CS_ARCH_X86, CS_MODE_64)


def read_accessor(va):
    """(base, count, fallback) of a G record accessor, read from its code:
    cmp edx,count / jae miss / movsxd rax,edx / add rax,base / lea rax,[rax+rax*4] / mov rax,[rcx+rax*8] / ret;
    miss: mov rax,[rcx+fallback] / ret. Any other shape is an error, never a guess."""
    ins = list(_md.disasm(AN.rd(va, 64), va))[:len(ACCESSOR_SHAPE)]
    if [i.mnemonic for i in ins] != ACCESSOR_SHAPE:
        raise ValueError("accessor %X: unexpected code shape %s" % (va, [i.mnemonic for i in ins]))
    count = int(ins[0].op_str.split(",")[1], 0)
    base = int(ins[3].op_str.split(",")[1], 0)
    fallback = int(ins[7].op_str.split("[rcx + ")[1].rstrip("]"), 0)
    return base, count, fallback


def closure_calls(va):
    """Functions reached from va through direct calls: the harness runs their bytes too."""
    seen, work = set(), [va]
    while work and len(seen) < 4000:
        a = work.pop()
        if a not in seen:
            seen.add(a)
            work.extend(C.direct_calls(a))
    return seen


def closure_fix(spec, va):
    """Impure mode: G's child links and the accessor layout replace the blanket E1EE slots."""
    called = [a for a in ACCESSORS if a in closure_calls(va)]
    ri = spec["regions_init"]
    removed = [k for k, v in ri.items() if k.startswith(G + "+0x") and v == REC]
    for k in removed:
        del ri[k]
    children = {}
    for name in spec["regions"]:
        if name.startswith(G + ".") and name != REC and "." not in name[len(G) + 1:]:
            children[int(name[len(G) + 1:], 16)] = name
    for off, name in children.items():
        ri["%s+0x%X" % (G, off)] = name
    slots = 0
    for a in called:
        base, count, _fallback = read_accessor(a)
        for k in range(count):
            off = (k + base) * STRIDE
            ri["%s+0x%X" % (G, off)] = children.get(off, REC)
            slots += 1
    spec["closure_fix"] = {"accessors": ["%X" % a for a in sorted(called)],
                           "removed_blanket_E1EE_slots": len(removed), "child_links": len(children),
                           "accessor_slots": slots,
                           "rule": "rules.closure_fix: child links + accessor layout from image bytes"}
    spec["note"] += (" Closure fix (rules.closure_fix): G child links restored; accessor slots from the image bytes; "
                     "blanket E1EE slots removed.")


def drop_dangling(spec):
    """A regions_init entry that names a region the spec does not declare is dropped."""
    ri, regions = spec["regions_init"], spec["regions"]
    for k in [k for k, v in ri.items() if v not in regions]:
        del ri[k]


def _abs_va(f):
    """VA of an absolute IMG scalar field (its off is 0x<VA>), else None (indexed fields)."""
    if f["root"] == "IMG" and not f["path"] and f["off"].startswith("0x"):
        return int(f["off"], 16)
    return None


def _enc(f):
    """Case encoding of a scalar load: f32 for a float load, else the load width (u32 dword, u64 qword)."""
    if "f32" in f["kind"]:
        return "f32"
    width = max(f["w"]) if f["w"] else 8
    return {1: "u8", 2: "u8", 4: "u32"}.get(width, "u64")


def _schema(enc):
    if enc == "f32":
        return {"kind": "f32", "lo": -50.0, "hi": 50.0,
                "domain": "generic f32 -50..50 (.data global the function reads; case-varied)"}
    return {"kind": "u8", "lo": 0, "hi": 255,
            "domain": "generic u8 0..255 (case value of the .data global this function loads, written as %s)" % enc}


def case_varied_globals(spec, fn):
    """Pilot mode, for the functions named in CASE_VARIED: their CONST .data reads become case-varied inputs,
    encoded as the load is. The remaining absolute reads stay live (precheck)."""
    va = int(spec["va"], 16)
    if va not in CASE_VARIED:
        return
    img_reads = [f for f in fn["fields"] if f["root"] == "IMG" and not f["path"] and f["r"] and not f["wr"]
                 and f["sect"] != ".rdata"]
    for f in img_reads:
        g = _abs_va(f)
        if g in CASE_VARIED[va]:
            name = "G_%X" % g
            enc = _enc(f)
            spec["inputs"][name] = {"absolute": "0x%X" % g, "enc": enc}
            spec["input_schema"][name] = _schema(enc)
    left = [f for f in img_reads if _abs_va(f) not in CASE_VARIED[va]]
    spec["precheck"]["live_data_reads"] = ["IMG"] if left else []


def audit_outputs(spec, image):
    """Pilot mode: a spec checks every return register its body writes (5484011)."""
    lines = xlate.capstone_decode(image, int(spec["va"], 16))
    spec["outputs"].update(spec_outputs.missing_outputs(spec, lines))
