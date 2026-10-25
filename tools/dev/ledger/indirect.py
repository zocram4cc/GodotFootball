#!/usr/bin/env python3
"""indirect.py: resolve indirect CALL/JMP targets and switch-table cases statically from the image.

Reads the unpacked image (file offset = VA - 0x140000000; never the packed exe) and writes
indirect.json: {site VA: {targets, rule, evidence}}. Consumed by the ledger's universe walk
(universe.py: indirect sites gain resolved targets) and the pipeline's closure check
(driver.py: resolved callees become direct-call edges). Deterministic: sorted VAs, no timestamps,
no bridge, no dumps beyond the image bytes.

Rules (each proven on real sample sites; see docs/re/pes-re-indirect.md):
  import   call/jmp [rip+disp] into the live IAT area (0x14252F000-0x142531000): the slot's thunk
           is named from the KNOWN table (docs/LEDGER evidence) or the dump-time code value.
  switch   jmp reg through a relative table: guard cmp/ja proves the bound; cases enumerated
           from the image (shapes: base+map+rva, base+rva, base+cdqe+map+rva).
  vtable   call [reg+off] (two-step: mov reg,[obj]; call [reg+off]): slot index = off/8; receiver
           provenance (arg/global/stack/ret). The class comes from the constructor store
           (LEA vtable + MOV into the global) when the receiver is a global with exactly one
           storing class (single), else the candidate set is every in-scope RTTI vtable with
           that slot (never one guessed).
  fnptr    call [abs] into .data with a dump-time code value: single target, dump-time evidence
           (uncorroborated: the store is not proven).
  callback call [rsp+off] / call reg: unresolved (input/context dependent).

Output per site: {"targets": [VAs or []], "rule": ..., "evidence": [...], "class": single|candidates|unresolved}.
Callees that stay unresolved keep model null downstream.

Usage: indirect.py --image IMG [--universe tools/dev/ledger/universe.json] --out indirect.json
         [--sites 140451267,1404F4B00,...] (default: every in-scope register-shape indirect site
         in the universe + every impure-sample site)
"""
import argparse
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
import universe as U

BASE = U.IMAGE_BASE
CODE_LO, CODE_HI = U.CODE_LO, U.CODE_HI
RDATA_LO, RDATA_HI = U.RDATA_LO, U.RDATA_HI
IAT_LO, IAT_HI = 0x14252F000, 0x142531000

# Slots the docs name (evidence in docs/re/pes-re-indirect.md S4; LEDGER engine rows for the CRT ones).
# Key: IAT slot VA. Value: (name, evidence).
KNOWN_SLOTS = {
    0x1425301F0: ("sqrt", "docs/re/pes-re-carrier-helpers.md FUN_1415A1780=sqrt; slot = thunk+6+disp"),
    0x142530278: ("logf", "docs name 1415A17C8/17CE as the logf/powf imports (ImpureMap S2)"),
    0x142530290: ("powf", "docs name 1415A17C8/17CE as the logf/powf imports (ImpureMap S2)"),
    0x14252FDE0: ("abort", "tamper-path trap FUN_1415A1714; docs/ports/pes-port-TeamT2.md:43"),
    0x14252FD98: ("engine-ctor", "thunk FUN_1415a1720 is LEDGER engine; callers build records"),
    0x14252F340: ("crt-init", "Ghidra _Init_thread_header calls (*DAT_14252f340)"),
    0x14252F6E8: ("crt-init", "Ghidra _Init_thread_header calls (*DAT_14252f6e8)"),
    0x14252F678: ("crt", "CRT thunk page 14159Fxxx; Ghidra library match"),
    0x14252F680: ("crt", "CRT thunk page 14159Fxxx; Ghidra library match"),
    0x142530868: ("crt-tls-callback", "thunk called from _Init_thread_notify/_Init_thread_wait; dump = ret stub"),
}

# Record accessors (base index, count, fallback offset): each returns [G + (k+base)*0x28]
# for k < n else [G + fallback]. Proven by decompile (docs/re/pes-re-indirect.md S5).
ACCESSORS = {
    0x140A48910: (0xA5, 26, 0x19C8),
    0x140A48A10: (0xBF, 26, 0x1DD8),
    0x140A489A0: (0xD9, 26, 0x21E8),
    0x140A48A30: (0xA1, 2, 0x1928),
    0x140A489F0: (0xF3, 0x16, 0x25F8),
}
STRIDE = 0x28


def rd(img, va, n):
    img.seek(va - BASE)
    return img.read(n)


def u64(img, va):
    return struct.unpack("<Q", rd(img, va, 8))[0]


def load_names():
    try:
        return U.load_names()
    except Exception:
        return {}


def rtti_vtables(img):
    try:
        return U.rtti_vtables(img)
    except Exception:
        return {}


def site_shape(text):
    """One of: import, vtable, fnptr, callback, reg, unknown."""
    if "rip" in text:
        return "import"
    if "RSP" in text or "RBP" in text:
        return "callback"
    if re.match(r"^(CALL|JMP) R[A-Z0-9]+$", text):
        return "reg"
    if re.match(r"^(CALL|JMP) qword ptr \[0?x?[0-9a-f]+\]$", text):
        return "fnptr"
    if re.match(r"^(CALL|JMP) qword ptr \[R[A-Z0-9]+.*\]$", text):
        return "vtable"
    return "unknown"


def slot_index(text):
    """Slot index for `call [reg+off]` (off/8), else None."""
    m = re.search(r"\+\s*(0x[0-9a-f]+|\d+)\]$", text)
    if not m:
        return 0
    return int(m.group(1), 16 if m.group(1).startswith("0x") else 10) // 8


def resolve_import(img, site_va, text, evidence):
    """Name the IAT slot; single target when the slot/thunk is known."""
    m = re.search(r"\[(0x[0-9a-f]+)\]", text)
    if m:
        slot = int(m.group(1), 16)
    else:
        # rip-relative: target = site + insn_len + disp. The bridge text has no
        # length; read the bytes (FF 15/25 disp32 = 6 bytes).
        blob = rd(img, site_va, 6)
        if len(blob) >= 6 and blob[0] == 0xFF and blob[1] in (0x15, 0x25):
            disp = struct.unpack("<i", blob[2:6])[0]
            slot = site_va + 6 + disp
        else:
            return {"targets": [], "rule": "import", "evidence": evidence + ["undecodable rip slot"],
                    "class": "unresolved"}
    if IAT_LO <= slot < IAT_HI:
        if slot in KNOWN_SLOTS:
            name, why = KNOWN_SLOTS[slot]
            return {"targets": [], "rule": "import", "evidence": evidence + ["slot %#x = %s (%s)" % (slot, name, why)],
                    "class": "single", "name": name}
        v = u64(img, slot)
        if CODE_LO <= v < CODE_HI:
            return {"targets": ["%X" % v], "rule": "import",
                    "evidence": evidence + ["slot %#x dump-time code value" % slot], "class": "single"}
        return {"targets": [], "rule": "import",
                "evidence": evidence + ["slot %#x unnamed (dump %#x)" % (slot, v)], "class": "unresolved"}
    if CODE_LO <= slot < CODE_HI:
        return {"targets": ["%X" % slot], "rule": "fnptr",
                "evidence": evidence + ["absolute code address"], "class": "single"}
    v = u64(img, slot)
    if CODE_LO <= v < CODE_HI:
        return {"targets": ["%X" % v], "rule": "fnptr",
                "evidence": evidence + ["slot %#x dump-time code value (uncorroborated)" % slot],
                "class": "single"}
    return {"targets": [], "rule": "fnptr",
            "evidence": evidence + ["slot %#x dump %#x not code" % (slot, v)], "class": "unresolved"}


def iter_switch_tables(img, only_sites=None):
    """Yield (site_va, targets) for relative-table switches in .text.

    Shapes (all read from the image; guard cmp/ja proves the bound):
      base+rva:    cmp EAX,imm; ja default; lea R,[rip]; mov E,r32 [R+rax*4+T]; add R,Rx; jmp R
      base+map+rva: ... movzx E,byte [R+rax+M]; mov E,r32 [R+rax*4+T]; add; jmp
      base+cdqe+map+rva (signed index): adds CDQE after the lea.
    With only_sites set, only the windows around those sites are decoded.
    """
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    out = {}
    windows = []
    if only_sites is not None:
        for sv in only_sites:
            lo = max(CODE_LO, sv - 64)
            windows.append((lo, min(CODE_HI, sv + 16) - lo))
    else:
        va = CODE_LO
        CH = 1 << 20
        while va < CODE_HI:
            n = min(CH, CODE_HI - va)
            windows.append((va, n))
            va += n
    for wva, n in windows:
        blob = rd(img, wva, n)
        for ins in md.disasm(blob, wva):
            if ins.mnemonic != "jmp":
                continue
            ops = list(getattr(ins, "operands", []))
            if len(ops) != 1 or ops[0].type != 1:  # X86_OP_REG
                continue
            site = ins.address
            if only_sites is not None and site not in only_sites:
                continue
            # Window: previous 64 bytes hold lea base, table loads, guard cmp.
            w = rd(img, max(CODE_LO, site - 64), min(64, site - CODE_LO))
            win = list(md.disasm(w, max(CODE_LO, site - 64)))
            base = rtab = mmap = None
            bound = None
            for w_ in win:
                t = "%s %s" % (w_.mnemonic, w_.op_str)
                m = re.search(r"lea\s+(r\w+),\s*\[rip\s*([+-])\s*0?x?([0-9a-f]+)\]", t)
                if m:
                    d = int(m.group(3), 16)
                    base = w_.address + w_.size + (d if m.group(2) == "+" else -d)
                m = re.search(r"mov\s+\w+,\s*dword ptr \[\w+ \+ \w+\*(0?x?[0-9a-f]+) \+ 0?x?([0-9a-f]+)\]", t)
                if m and base is not None:
                    scale = int(m.group(1), 16 if "x" in m.group(1) else 10)
                    if scale == 4:
                        rtab = base + int(m.group(2), 16)
                m = re.search(r"movzx\s+\w+,\s*byte ptr \[\w+ \+ \w+ \+ 0?x?([0-9a-f]+)\]", t)
                if m and base is not None:
                    mmap = base + int(m.group(1), 16)
                m = re.search(r"cmp\s+e\w+,\s*0?x?([0-9a-f]+)", t)
                if m:
                    bound = int(m.group(1), 16)
            if base is None or rtab is None or bound is None or bound > 0xFFF:
                continue
            if not (CODE_LO <= base < CODE_HI and RDATA_LO <= rtab < RDATA_HI):
                continue
            if mmap is None:
                span = bound + 1
            else:
                if not (RDATA_LO <= mmap < RDATA_HI):
                    continue
                span = max(bytearray(rd(img, mmap, bound + 1))) + 1
            if span > 0x1000:
                continue
            try:
                table = rd(img, rtab, 4 * span)
            except Exception:
                continue
            if len(table) < 4 * span:
                continue
            targets = set()
            for k in range(span):
                t = BASE + struct.unpack_from("<i", table, 4 * k)[0]
                if CODE_LO <= t < CODE_HI:
                    targets.add("%X" % t)
            if targets:
                out["%X" % site] = sorted(targets)
    return out


def constructor_stores(img, vts, only_globals=None):
    """{global VA: vtable VA} for globals stored exactly one vtable (LEA+MOV join)."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    from capstone.x86 import X86_OP_MEM, X86_OP_REG
    vtset = set(vts)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    stores = {}
    va = CODE_LO
    CH = 1 << 20
    while va < CODE_HI:
        n = min(CH, CODE_HI - va)
        blob = rd(img, va, n)
        pending = {}  # reg -> (vtable, insn_va)
        for ins in md.disasm(blob, va):
            if ins.mnemonic == "lea" and ins.operands and \
                    ins.operands[0].type == X86_OP_REG:
                op = ins.operands[1]
                if op.type == X86_OP_MEM and op.mem.base != 0 and \
                        ins.reg_name(op.mem.base) == "rip":
                    tgt = ins.address + ins.size + op.mem.disp
                    if tgt in vtset:
                        pending[ins.reg_name(ins.operands[0].reg)] = (tgt, ins.address)
                        continue
            if ins.mnemonic == "mov" and len(ins.operands) == 2 and \
                    ins.operands[0].type == X86_OP_MEM and ins.operands[1].type == X86_OP_REG:
                dst, src = ins.operands
                if dst.mem.base != 0 and ins.reg_name(dst.mem.base) == "rip":
                    g = ins.address + ins.size + dst.mem.disp
                    if only_globals is not None and g not in only_globals:
                        continue
                    r = ins.reg_name(src.reg)
                    if r in pending:
                        stores.setdefault(g, set()).add(pending[r][0])
            if ins.mnemonic in ("call", "jmp", "ret"):
                pending = {}
        va += n
    return {g: next(iter(v)) for g, v in stores.items() if len(v) == 1}


def resolve_site(img, site_va, text, vts, stores, switch_tables, scope_in):
    ev = ["site %#x: %s" % (site_va, text)]
    shape = site_shape(text)
    if shape == "import":
        return resolve_import(img, site_va, text, ev)
    if shape in ("callback", "reg", "unknown"):
        return {"targets": [], "rule": shape,
                "evidence": ev + ["input/context dependent; no static rule"], "class": "unresolved"}
    if shape == "fnptr":
        r = resolve_import(img, site_va, text, ev)
        r["rule"] = "fnptr"
        return r
    # vtable
    idx = slot_index(text)
    hit = {}  # code target -> example vtable
    for v, (td, slots) in vts.items():
        if len(slots) > idx and (not scope_in or U.out_rule(td) is None):
            hit.setdefault(slots[idx], v)
    tgts = sorted("%X" % t for t in hit)
    # Global receiver with a single storing class: single target = its slot value.
    m = re.search(r"\[(0x[0-9a-f]+)\]", text)
    if m:
        g = int(m.group(1), 16)
        if g in stores:
            v = stores[g]
            td, slots = vts[v]
            if len(slots) > idx:
                return {"targets": ["%X" % slots[idx]], "rule": "vtable-global",
                        "evidence": ev + ["global %#x stores vtable %#x (%s); slot %d" % (g, v, td, idx)],
                        "class": "single"}
    key = "%X" % site_va
    if key in switch_tables:
        return {"targets": [], "rule": "vtable", "evidence": ev, "class": "unresolved"}
    return {"targets": tgts[:64], "rule": "vtable",
            "evidence": ev + ["slot %d; %d in-scope vtables, %d distinct code targets (capped at 64)"
                              % (idx, sum(1 for v in hit.values()), len(tgts))],
            "class": "candidates", "count": len(tgts)}


def collect_sites(universe):
    """{site VA: text} for every in-scope register-shape indirect site + switch jumps."""
    out = {}
    for va, r in universe.get("functions", {}).items():
        if r.get("scope") != "in":
            continue
        for s in (r.get("indirect") or []):
            txt = s.get("text", "")
            if "rip" in txt or "RSP" in txt or "RBP" in txt:
                continue
            if txt.startswith("CALL qword ptr [0x") or txt.startswith("JMP qword ptr [0x"):
                continue
            if txt.startswith("CALL ") or txt.startswith("JMP "):
                try:
                    out[int(s["site"], 16)] = txt
                except (KeyError, ValueError):
                    pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--universe", default=os.path.join(REPO, "tools", "dev", "ledger", "universe.json"))
    ap.add_argument("--out", default=os.path.join(HERE, "indirect.json"))
    ap.add_argument("--sites", default=None)
    args = ap.parse_args()
    uni = json.load(open(args.universe))
    if args.sites:
        want = {int(x, 16) for x in args.sites.split(",")}
        with open(args.image, "rb") as img0:
            sw_first = iter_switch_tables(img0, only_sites=want)
        sites = {int(k, 16): "JMP switch-table" for k in sw_first}
        for va, r in uni.get("functions", {}).items():
            for s in (r.get("indirect") or []):
                try:
                    sv = int(s["site"], 16)
                except (KeyError, ValueError):
                    continue
                if sv in want:
                    sites[sv] = s.get("text", "")
        # rip/abs sites are not register-shape rows; add bare entries decoded below
        for sv in want:
            sites.setdefault(sv, "")
    else:
        sites = collect_sites(uni)
    with open(args.image, "rb") as img:
        vts = rtti_vtables(img)
    if args.sites:
        only = set(sites)
        with open(args.image, "rb") as img:
            # globals referenced by absolute-memory sites bound the store sweep
            globs = set()
            for sv, txt in sites.items():
                m = re.search(r"\[(0x[0-9a-f]+)\]", txt or "")
                if m:
                    globs.add(int(m.group(1), 16))
            stores = constructor_stores(img, vts, only_globals=globs or None)
        with open(args.image, "rb") as img:
            switch_tables = iter_switch_tables(img, only_sites=only)
    else:
        with open(args.image, "rb") as img:
            stores = constructor_stores(img, vts)
        with open(args.image, "rb") as img:
            switch_tables = iter_switch_tables(img)
        for k in switch_tables:
            sites.setdefault(int(k, 16), "JMP switch-table")
    out = {}
    for sv in sorted(sites):
        text = sites[sv]
        if text.startswith("JMP switch-table") or (not text and "%X" % sv in switch_tables):
            tgts = switch_tables.get("%X" % sv, [])
            out["%X" % sv] = {"targets": tgts, "rule": "switch",
                               "evidence": ["guard cmp/ja proves bound; %d cases from image" % len(tgts)],
                               "class": "single" if len(tgts) == 1 else "candidates"}
            continue
        if not text:
            # bare site (rip/abs): decode the likely call/jmp [rip] at the VA
            with open(args.image, "rb") as img:
                blob = rd(img, sv, 6)
            if len(blob) == 6 and blob[0] == 0xFF and blob[1] in (0x15, 0x25):
                mn = "CALL" if blob[1] == 0x15 else "JMP"
                disp = struct.unpack("<i", blob[2:6])[0]
                text = "%s qword ptr [rip + 0x%x]" % (mn, (sv + 6 + disp) & 0xFFFFFFFF)
        with open(args.image, "rb") as img:
            out["%X" % sv] = resolve_site(img, sv, text, vts, stores, switch_tables, True)
    # switch tables found by the sweep that the universe never listed
    for k, v in switch_tables.items():
        if k not in out:
            out[k] = {"targets": v, "rule": "switch",
                      "evidence": ["guard cmp/ja proves bound; %d cases from image (universe-unlisted)" % len(v)],
                      "class": "single" if len(v) == 1 else "candidates"}
    json.dump(out, open(args.out, "w"), indent=1, sort_keys=True)
    from collections import Counter
    c = Counter(v["class"] + ":" + v["rule"] for v in out.values())
    print("indirect: %d sites -> %s" % (len(out), dict(sorted(c.items()))))
    print("wrote %s" % args.out)


if __name__ == "__main__":
    main()
