"""Pilot-mode spec builder: one diff-harness spec per function, from its analysis row (analyze.py).

Conventions (see run.py):
- one synthetic region per record object (A0..A3 roots and pointer chains);
- pointer fields are linked with regions_init, IMG pointer slots with patch_addrs;
- scalar fields become per-case inputs; written fields become outputs;
- domains are generic (no doc range) and labelled as such in the schema.
Harness limits honoured: encode() has u8/u32/u64/i32/f32 only and decode_blob()
has u8/u32/i32/f32 only, so 2-byte fields become byte pairs and 8-byte outputs
become u32 pairs; 16-byte fields become two u64 inputs (outputs: four u32).
"""
import collections

from . import analyze as AN

REGION_BASE = 0x20000000
REGION_STRIDE = 0x400000
DOMAIN_INT = "generic u8 range 0..255 (no doc range; width-typed)"
DOMAIN_F32 = "generic f32 -50..50 (no doc range)"
COUNT = 64
REGISTRY_SLOTS = 234  # constant registry size: FUN_141E5DE80() = 234 (pes-re-movestep-wave1.md; pes-re-contact.md)


def domain_for(nm):
    """Schema for an 8/16/32-bit scalar input. Doc-cited ranges where a field's meaning is documented:
    ring count +0x1900 saturates at 8 and head +0x1904 wraps mod 8 (pes-re-record-layouts.md §1).
    Everything else keeps the generic label."""
    if nm.endswith("E0_1900"):
        return {"kind": "choice", "values": list(range(0, 9)), "domain": "ring count 0..8 (pes-re-record-layouts.md §1)"}
    if nm.endswith("E0_1904"):
        return {"kind": "choice", "values": list(range(0, 8)), "domain": "ring head 0..7 mod 8 (pes-re-record-layouts.md §1)"}
    return {"kind": "u8", "lo": 0, "hi": 255, "domain": DOMAIN_INT}


def parse_off(s):
    """'0x10' -> (False, 0x10); 'IDX+0x10' -> (True, 0x10); a negative offset is written '0x-3FA8' -> -0x3FA8."""
    s = s.replace("0x-", "-0x")
    if s.startswith("IDX+"):
        return True, int(s[4:], 16)
    return False, int(s, 16)


def off_hex(v):
    """Signed offset as text run.py parses: '0x10', or '-0x10' (a regions_init key is 'REGION+off')."""
    return "0x%X" % v if v >= 0 else "-0x%X" % -v


def objname(root, path):
    return root + "".join(".%X" % parse_off(p)[1] for p in path)


def intervals(fields, want_written):
    """Non-overlapping byte intervals (off, width, kind). Wider field wins at its start."""
    items = []
    for f in fields:
        if f["ptr"]:
            continue
        if want_written and not f["wr"]:
            continue
        _, off = parse_off(f["off"])
        widths = f["w"] or [8]
        w = max(widths)
        kind = "f32" if ("f32" in f["kind"] and w == 4) else "int"
        items.append((off, w, kind, f["off"]))
    items.sort(key=lambda t: (t[0], -t[1]))
    kept, end = [], -1
    for off, w, kind, raw in items:
        if off < end:
            continue  # inside an already-kept wider field
        kept.append((off, w, kind))
        end = off + w
    return kept


def pieces_in(off, w, kind, mode):
    """Split an interval into (offset, enc, suffix) pieces the harness can encode."""
    if mode == "input":
        if w == 1:
            return [(off, "u8", "")]
        if w == 2:
            return [(off, "u8", "_b0"), (off + 1, "u8", "_b1")]
        if w == 4:
            return [(off, "f32" if kind == "f32" else "u32", "")]
        if w == 8:
            return [(off, "u64", "")]
        if w == 16:
            return [(off, "u64", "_q0"), (off + 8, "u64", "_q1")]
        return [(off + i, "u8", "_x%d" % i) for i in range(w)]
    # outputs: decode_blob knows u8, u32, i32, f32 only
    if w == 1:
        return [(off, "u8", "")]
    if w == 2:
        return [(off, "u8", "_b0"), (off + 1, "u8", "_b1")]
    if w == 4:
        return [(off, "f32" if kind == "f32" else "u32", "")]
    if w == 8:
        return [(off, "u32", "_d0"), (off + 4, "u32", "_d1")]
    if w == 16:
        return [(off + 4 * i, "u32", "_d%d" % i) for i in range(4)]
    return [(off + i, "u8", "_x%d" % i) for i in range(w)]


def build(fn):
    fields = fn["fields"]
    groups = collections.OrderedDict()
    for f in fields:
        key = objname(f["root"], f["path"])
        groups.setdefault(key, {"root": f["root"], "path": f["path"], "fields": []})["fields"].append(f)
    # Constant registry (0x1437F4808): read directly, then the element is fetched by a tail-jumped getter
    # outside the set. Model it as a 234-slot table whose every slot is one shared payload object (the
    # getter returns list[id]+8; each function reads one id, so one payload object is faithful per case).
    if any(f["root"] == "IMG" and not f["path"] and f["off"] == "0x1437F4808" for f in fields):
        # list object (slot target) -> +0 array -> array[8*id] -> payload (getter FUN_141E5CA20: [[list]+8*id]+8)
        groups.setdefault("IMG.1437F4808", {"root": "IMG", "path": ["0x1437F4808"], "fields": []})
        groups.setdefault("IMG.1437F4808.0", {"root": "IMG", "path": ["0x1437F4808", "0x0"], "fields": []})
        groups.setdefault("IMG.1437F4808.0.E1EE", {"root": "IMG", "path": ["0x1437F4808", "0x0", "0xE1EE"], "fields": []})
        groups.setdefault("IMG.1437F4808.0.E1EE.8", {"root": "IMG", "path": ["0x1437F4808", "0x0", "0xE1EE", "0x8"], "fields": []})
    regions, regions_init, patch_addrs = {}, {}, {}
    names = []
    # objects that need a region: non-IMG roots, and IMG objects reached through a pointer slot
    for key, g in groups.items():
        if g["root"] == "IMG" and not g["path"]:
            continue  # absolute global scalar: not a region
        names.append(key)
    # pointer links: child object at parent's field
    for key, g in groups.items():
        if not g["path"]:
            continue
        parent = objname(g["root"], g["path"][:-1])
        last = parse_off(g["path"][-1])[1]
        if g["root"] == "IMG" and len(g["path"]) == 1:
            patch_addrs["0x%X" % last] = key
        else:
            if last == 0xE1EE:  # element of global pointer table: every registry slot -> this object
                for slot in range(REGISTRY_SLOTS):
                    regions_init["%s+0x%X" % (parent, 8 * slot)] = key
            else:
                regions_init["%s+%s" % (parent, off_hex(last))] = key
    base = REGION_BASE
    for key in names:
        g = groups[key]
        top, low = 0, 0
        for f in g["fields"]:
            ind, off = parse_off(f["off"])
            w = max(f["w"]) if f["w"] else 8
            top = max(top, off + w)
            low = min(low, off)
        pad = (-low + 0xFFF) & ~0xFFF  # zero-filled room below the object for negative field offsets
        if pad:
            regions[key + ".pad"] = {"base": "0x%X" % base, "size": "0x%X" % pad}
            base += pad
        size = max(0x200000, ((top + 0x40) + 0xFFF) & ~0xFFF)  # 2 MB: lane blocks reach +0x180698
        regions[key] = {"base": "0x%X" % base, "size": "0x%X" % size}
        base += max(REGION_STRIDE, size)  # a larger region moves the next base past its end (no overlap)

    inputs, schema, outputs = {}, {}, {}
    for key in names:
        g = groups[key]
        for off, w, kind in intervals(g["fields"], False):
            for poff, enc, suffix in pieces_in(off, w, kind, "input"):
                nm = "%s_%X%s" % (key.replace(".", "_"), poff, suffix)
                inputs[nm] = {"region": key, "offset": off_hex(poff), "enc": enc}
                if enc == "f32":
                    schema[nm] = {"kind": "f32", "lo": -50.0, "hi": 50.0, "domain": DOMAIN_F32}
                else:
                    schema[nm] = domain_for(nm)
        for off, w, kind in intervals(g["fields"], True):
            for poff, enc, suffix in pieces_in(off, w, kind, "output"):
                nm = "w_%s_%X%s" % (key.replace(".", "_"), poff, suffix)
                outputs[nm] = {"region": key, "offset": off_hex(poff), "enc": enc}

    args = {}
    roots_used = {g["root"] for k, g in groups.items() if not g["path"] and g["root"] != "IMG"}
    live = set(fn["live_in"])
    for reg, root in (("rcx", "A0"), ("rdx", "A1"), ("r8", "A2"), ("r9", "A3")):
        if root in roots_used:
            args[reg] = root
        elif reg in live:
            nm = "scalar_%s" % reg
            inputs[nm] = {"reg": reg, "enc": "u32"}
            schema[nm] = {"kind": "u8", "lo": 0, "hi": 255, "domain": DOMAIN_INT}
    for x in ("xmm0", "xmm1", "xmm2", "xmm3"):
        if x in live:
            nm = "scalar_%s" % x
            inputs[nm] = {"reg": x, "enc": "f32"}
            schema[nm] = {"kind": "f32", "lo": -50.0, "hi": 50.0, "domain": DOMAIN_F32}
    # Stack slots the harness maps: run.py maps 0x100 bytes above stack_top
    # (stack_top = base + size - 0x100). Slots past them are not arguments.
    arg_area_end = 0x100
    for k, widths in fn["stack_args"].items():
        if 0x20 + 8 * int(k) + 8 > arg_area_end:
            continue  # analyzer rsp artifact, not an argument (1404F6C30: k=5095 -> +0x9F58)
        w = max(int(x) for x in widths)
        enc = {1: "u8", 2: "u8", 4: "u32", 8: "u64", 16: "u64"}.get(w, "u64")
        nm = "stk%s" % k
        inputs[nm] = {"stack_offset": "0x%X" % (0x20 + 8 * int(k)), "enc": enc}
        schema[nm] = {"kind": "u8", "lo": 0, "hi": 255, "domain": DOMAIN_INT}

    rets = fn["rets"]
    if any(r["rax_ptr"] for r in rets):
        outputs["ret_rax"] = {"reg": "rax"}
        ret_note = "rax (pointer-valued return)"
    elif any(r["rax_w"] in (1, 2, 4) for r in rets):
        outputs["ret_eax"] = {"reg": "eax"}
        ret_note = "eax (integer/bool return; upper bits unchecked)"
    else:
        outputs["ret_xmm0"] = {"reg": "xmm0", "enc": "f32"}
        ret_note = "no rax write before ret: xmm0 (float) or void"

    # IMG .data reads and writes: live-state disclosure
    img_scalar = [f for f in fields if f["root"] == "IMG" and not f["path"]]
    live_reads = sorted({f["obj"] for f in img_scalar if f["r"] and not f["wr"] and f["sect"] != ".rdata"})
    live_writes = sorted({f["obj"] for f in img_scalar if f["wr"]})
    rdata_reads = sorted({f["obj"] for f in img_scalar if f["r"] and f["sect"] == ".rdata"})

    spec = {
        "name": "pilot_%s" % fn["va"][2:],
        "va": fn["va"],
        "entry": fn["va"],
        "note": "pilot-set spec (PilotMap). Generated by specgen (pilot mode) from the image; callees all in pilot set (no stubs). Regions: one per record object reached from an arg or IMG pointer slot.",
        "status": fn["status"],
        "subsystem": fn["subsystem"],
        "stack": {"base": "0x10000000", "size": "0x20000"},
        "regions": regions,
        "args": args,
        "regions_init": regions_init,
        "patch_addrs": patch_addrs,
        "inputs": inputs,
        "input_schema": schema,
        "outputs": outputs,
        "ret_note": ret_note,
        "seed": seed_of(fn["va"]),
        "count": COUNT,
        "precheck": {
            "indirect_calls": [x for x in fn["flags"] if x.startswith("indirect-call")],
            "tailjumps": [x for x in fn["flags"] if x.startswith("tailjump")],
            "unresolved_mem": len([x for x in fn["flags"] if x.startswith("unresolved-mem")]),
            "live_data_reads": live_reads,
            "live_data_writes": live_writes,
            "rdata_constant_reads": rdata_reads,
            "stack_args": fn["stack_args"],
            "calls": fn["calls"],
            "flags": fn["flags"],
        },
    }
    return spec


def seed_of(va):
    """Seed of a NEW spec: 1000 + the VA's offset from the image base, independent of set order. A recorded spec keeps
    its committed seed: a changed seed changes its cases (pes-port-SpecGen2 §4)."""
    return 1000 + int(va, 16) - AN.BASE


def build_all(analysis_rows):
    """One spec per analysis row, in set order."""
    return [build(fn) for fn in analysis_rows]
