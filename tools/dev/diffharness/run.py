#!/usr/bin/env python3
"""Differential-test emulator core: run one PES function over the user's image.

Maps the executable's sections, a stack and synthetic heap regions; sets
argument registers per the Windows x64 ABI; stubs callees by VA; hooks
unmapped reads so an unmodelled global stops the run with a clear message;
stops at the function's return (or at the spec's end address for slices).
No PES bytes live here: specs cite VAs and constants are read from the user's
image at run time.

Usage:
  run.py --exe PATH --spec specs/<fn>.json --out OUT.jsonl [--seed N] [--count N]
  run.py --check-spec --exe PATH --spec specs/<fn>.json
"""
import argparse
import json
import math
import os
import struct
import sys

PAGE = 0x1000
IMAGE_CHUNK = 1 << 24  # bytes per read when streaming the image (16 MiB)


def align_up(x, a=PAGE):
    return (x + a - 1) & ~(a - 1)


def image_kind(path):
    """'flat' for the unpacked image (file offset = VA - 0x140000000),
    'pe' for the packed PES2021.exe.

    Discriminator is structural, never byte content (probe bytes would be
    PES bytes, which never enter the repo): the dump keeps the packed
    layout's section table but appends live pages, so its filesize exceeds
    the table's max raw end; the packed exe ends exactly there. A packed
    exe's bytes differ from the image's at every code VA (band/strategy/
    cone/tick entries and .rdata alike, per the DiffHarnessRE byte diff),
    so it must never be emulated.
    """
    import os
    with open(path, "rb") as f:
        head = f.read(0x400)
    if head[:2] != b"MZ":
        raise SystemExit("diffharness: %s has no MZ header" % path)
    pe = struct.unpack("<I", head[0x3C:0x40])[0]
    count = struct.unpack("<H", head[pe + 6:pe + 8])[0]
    opt = struct.unpack("<H", head[pe + 20:pe + 22])[0]
    end = 0
    with open(path, "rb") as f:
        f.seek(pe + 24 + opt)
        for _ in range(count):
            s = f.read(40)
            if len(s) < 40:
                break
            vsize, _, rsize, rptr = struct.unpack("<IIII", s[8:24])
            if vsize == 0 and rsize == 0:
                break
            end = max(end, rptr + rsize)
    if os.path.getsize(path) == end:
        return "pe"
    return "flat"

FLAT_BASE = 0x140000000


def parse_pe_sections(path):
    """Return (va, size, fileoff) for the image's real sections.

    pes_full.bin carries a huge tail of garbage section headers past the real
    table; only entries whose raw range sits inside the file are real.
    """
    import os
    filesize = os.path.getsize(path)
    with open(path, "rb") as f:
        head = f.read(0x400)
    assert head[:2] == b"MZ", "not a PE image (no MZ header)"
    pe = struct.unpack("<I", head[0x3C:0x40])[0]
    with open(path, "rb") as f:
        f.seek(pe)
        coff = f.read(24)
        assert coff[:4] == b"PE\0\0", "not a PE image (no PE signature)"
        count = struct.unpack("<H", coff[4:6])[0]
        opt = struct.unpack("<H", coff[20:22])[0]
        f.seek(pe + 24)
        oh = f.read(32)
        base = struct.unpack("<Q", oh[24:32])[0]
        f.seek(pe + 24 + opt)
        sections = []
        for _ in range(count):
            s = f.read(40)
            if len(s) < 40:
                break
            vsize, rva, rsize, rptr = struct.unpack("<IIII", s[8:24])
            size = max(vsize, rsize)
            if vsize == 0 and rsize == 0:
                break
            if rsize and rptr + rsize > filesize:
                break
            if size and rsize:
                sections.append((base + rva, size, rptr))
            if len(sections) > 64:
                break
    return sections


def flat_sections(path):
    """One section per mapped page run of the flat dump (skip trailing zeros)."""
    import os
    filesize = os.path.getsize(path)
    # The dump is dense code+data from the base; map it whole in 64 MB chunks.
    return [(FLAT_BASE + off, min(64 << 20, filesize - off), off)
            for off in range(0, filesize, 64 << 20)]


def read_va_from(path, sections, va, n):
    for sva, ssize, sraw in sections:
        if sva <= va < sva + ssize and va + n <= sva + ssize:
            with open(path, "rb") as f:
                f.seek(sraw + (va - sva))
                return f.read(n)
def read_va_live(uc, va, n):
    """Read VA bytes from the live emulator (image bytes mapped at their VAs)."""
    return bytes(uc.mem_read(va, n))

TRANSLATE_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                             "translate")


def sqrt_f32_bits(b):
    """Bits of sqrtss(b) for the float with bits b (the sqrt_xmm0 hook's model)."""
    x = struct.unpack("<f", struct.pack("<I", b))[0]
    if x != x:
        return (b | 0x00400000) & 0xFFFFFFFF  # NaN: returned quieted
    if x < 0.0:
        return 0xFFC00000  # default QNaN, sign set
    return struct.unpack("<I", struct.pack("<f", math.sqrt(x)))[0]


def resolve_stubs(spec):
    """Add the stubs.json model of every callee in stub_refs to spec["stubs"].
    The table is shared with the translator (translate/stubs.py)."""
    sys.path.insert(0, TRANSLATE_DIR)
    import stubs as stub_table
    table = stub_table.load()
    stubs = dict(spec.get("stubs") or {})
    for ref in sorted(set(spec.get("stub_refs") or [])):
        entry = table.get(int(ref, 16))
        if entry is None:
            raise SystemExit("stub %s: no entry in stubs.json" % ref)
        if entry["do"] == "unmodellable":
            raise SystemExit("unmodellable callee %s: %s" % (ref, entry["why"]))
        if entry["do"] is None:
            raise SystemExit("stub_refs %s is run-real, not a stub" % ref)
        stubs["stub_" + ref] = {"va": entry["va"], "do": entry["do"]}
    spec["stubs"] = stubs
    return spec


def load_spec(path):
    with open(path) as f:
        return json.load(f)


def check_spec(spec, sections, image_path):
    """Validate a spec against the image. Returns a list of error strings."""
    errors = []

    def is_mapped(va, n=1):
        return any(sva <= va and va + n <= sva + ssize for sva, _, ssize in
                   [(s[0], None, s[1]) for s in sections])

    for key in ("va", "entry", "end"):
        if key in spec and read_va_from(image_path, sections, int(spec[key], 16), 3) is None:
            errors.append("%s %s unreadable" % (key, spec[key]))
    # A packed exe is refused outright: its bytes differ from the unpacked
    # image's at every code VA, so emulating it computes garbage. The guard
    # is the file size, not probe bytes (those would be PES bytes in repo).
    if image_kind(image_path) != "flat":
        errors.append("refusing packed exe %s: unpack once to an image "
                      "(file offset = VA - 0x140000000) and set PES_EXE" % image_path)
    for name, stub in (spec.get("stubs") or {}).items():
        if read_va_from(image_path, sections, int(stub["va"], 16), 1) is None:
            errors.append("stub %s va %s unreadable" % (name, stub["va"]))
    for name, c in (spec.get("constants") or {}).items():
        if read_va_from(image_path, sections, int(c["va"], 16), 4) is None:
            errors.append("constant %s va %s unreadable" % (name, c["va"]))
    regions = spec.get("regions") or {}
    for rname, r in regions.items():
        base, size = int(r["base"], 16), int(r["size"], 16)
        for sva, ssize, _ in sections:
            if base < sva + ssize and sva < base + size:
                errors.append("region %s overlaps image section %#x" % (rname, sva))
    bases = sorted((int(r["base"], 16), int(r["size"], 16), n) for n, r in regions.items())
    for (b1, s1, n1), (b2, s2, n2) in zip(bases, bases[1:]):
        if b1 + s1 > b2:
            errors.append("regions %s and %s overlap" % (n1, n2))
    return errors


GPR = {"rcx": None, "rdx": None, "r8": None, "r9": None,
       "rax": None, "rbx": None, "rbp": None, "rsp": None,
       "eax": None, "ebx": None, "r8d": None, "edx": None, "ecx": None}
XMM = {"xmm0", "xmm1", "xmm2", "xmm3"}


def reg_const(name):
    from unicorn.x86_const import (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
                                   UC_X86_REG_R9, UC_X86_REG_RAX, UC_X86_REG_RBX,
                                   UC_X86_REG_RBP, UC_X86_REG_RSP, UC_X86_REG_RIP,
                                   UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2,
                                   UC_X86_REG_XMM3)
    return {"rcx": UC_X86_REG_RCX, "rdx": UC_X86_REG_RDX, "r8": UC_X86_REG_R8,
            "r9": UC_X86_REG_R9, "rax": UC_X86_REG_RAX, "rbx": UC_X86_REG_RBX,
            "rbp": UC_X86_REG_RBP, "rsp": UC_X86_REG_RSP, "rip": UC_X86_REG_RIP,
            "eax": UC_X86_REG_RAX, "ebx": UC_X86_REG_RBX, "r8d": UC_X86_REG_R8,
            "edx": UC_X86_REG_RDX, "ecx": UC_X86_REG_RCX,
            "xmm0": UC_X86_REG_XMM0, "xmm1": UC_X86_REG_XMM1,
            "xmm2": UC_X86_REG_XMM2, "xmm3": UC_X86_REG_XMM3}[name]


def encode(value, enc):
    if enc == "u8":
        return struct.pack("B", int(value, 0) if isinstance(value, str) else value)
    if enc == "u32":
        return struct.pack("<I", int(value, 0) if isinstance(value, str) else value)
    if enc == "u64":
        return struct.pack("<Q", int(value, 0) if isinstance(value, str) else value)
    if enc == "i32":
        return struct.pack("<i", int(value, 0) if isinstance(value, str) else value)
    if enc == "f32":
        if isinstance(value, dict) and "bits" in value:
            return struct.pack("<I", int(value["bits"], 16))
        return struct.pack("<f", float(value))
    raise ValueError("unknown enc %s" % enc)


def decode_blob(blob, enc):
    if enc == "u8":
        return struct.unpack("B", blob[:1])[0]
    if enc == "u32":
        return struct.unpack("<I", blob[:4])[0]
    if enc == "i32":
        return struct.unpack("<i", blob[:4])[0]
    if enc == "f32":
        bits = struct.unpack("<I", blob[:4])[0]
        return {"bits": "0x%08X" % bits, "f": struct.unpack("<f", blob[:4])[0]}
    raise ValueError("unknown enc %s" % enc)


def f32_bits(value):
    if isinstance(value, dict) and "bits" in value:
        bits = int(value["bits"], 16)
        return {"bits": "0x%08X" % bits, "f": struct.unpack("<f", struct.pack("<I", bits))[0]}
    bits = struct.unpack("<I", struct.pack("<f", float(value)))[0]
    return {"bits": "0x%08X" % bits, "f": float(value)}


def build_emu(spec, image_path, sections):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    filesize = os.path.getsize(image_path)
    with open(image_path, "rb") as f:
        for va, size, raw in sections:
            size = min(size, filesize - raw)
            base = va & ~(PAGE - 1)
            mu.mem_map(base, align_up(va + size) - base)
            # Stream each section in bounded chunks: the image is hundreds of
            # MB, and a whole-file read does not fit the per-process memory cap.
            f.seek(raw)
            done = 0
            while done < size:
                n = min(IMAGE_CHUNK, size - done)
                mu.mem_write(va + done, f.read(n))
                done += n
    for rname, r in (spec.get("regions") or {}).items():
        mu.mem_map(int(r["base"], 16), align_up(int(r["size"], 16)))
    stack = spec.get("stack", {"base": "0x10000000", "size": "0x20000"})
    mu.mem_map(int(stack["base"], 16), align_up(int(stack["size"], 16)))
    RET_SENTINEL = 0xDEAD0000
    mu.mem_map(RET_SENTINEL & ~(PAGE - 1), PAGE)
    return mu, RET_SENTINEL


def stack_top(spec):
    stack = spec.get("stack", {"base": "0x10000000", "size": "0x20000"})
    return int(stack["base"], 16) + int(stack["size"], 16) - 0x100


def set_inputs(mu, spec, inputs):
    regions = spec.get("regions") or {}
    for reg, val in (spec.get("args") or {}).items():
        if isinstance(val, str) and val in regions:
            mu.reg_write(reg_const(reg), int(regions[val]["base"], 16))
        else:
            mu.reg_write(reg_const(reg), int(val, 0) if isinstance(val, str) else val)
    labels = set(spec.get("labels") or [])
    for name, value in inputs.items():
        if name in labels:
            continue  # case label (provenance only, not fed to the emulator)
        loc = (spec.get("inputs") or {}).get(name)
        if loc is None:
            raise KeyError("input %s not in spec" % name)
        if "stub" in loc or loc.get("input"):
            continue  # stub-fed, or an offset consumed by an absfill entry
        if "absfill" in loc:
            # ["REGION", "off_input", "type"]: address from another input's value.
            rname, off_key, typ = loc["absfill"]
            addr = int(regions[rname]["base"], 16) + int(inputs[off_key])
            mu.mem_write(addr, encode(value, typ))
            continue
        blob = encode(value, loc.get("enc", "u32"))
        if "reg" in loc:
            reg = loc["reg"]
            if reg in XMM:
                # Unicorn takes the low 64 bits as int; the cone/wrap callees
                # read only the low float.
                mu.reg_write(reg_const(reg),
                             int.from_bytes(blob[:8].ljust(8, b"\0"), "little"))
            else:
                mu.reg_write(reg_const(reg), int.from_bytes(blob, "little"))
        elif "region" in loc:
            base = int(regions[loc["region"]]["base"], 16)
            mu.mem_write(base + int(loc["offset"], 16), blob)
        elif "stack_offset" in loc:
            mu.mem_write(stack_top(spec) + int(loc["stack_offset"], 16), blob)
        elif "absolute" in loc:
            # A global the code reads at a fixed image VA (case-varied; the
            # harness writes it per case, and the diff main writes the same bytes).
            mu.mem_write(int(loc["absolute"], 16), blob)
        else:
            raise KeyError("input %s has no location" % name)


def decode(mu, spec):
    from unicorn.x86_const import UC_X86_REG_RAX
    out = {}
    regions = spec.get("regions") or {}
    for name, loc in (spec.get("outputs") or {}).items():
        if "reg" in loc:
            reg = loc["reg"]
            if reg in XMM:
                bits = mu.reg_read(reg_const(reg)) & 0xFFFFFFFF
                out[name] = {"bits": "0x%08X" % bits,
                             "f": struct.unpack("<f", struct.pack("<I", bits))[0]}
            elif loc.get("enc") == "f32":
                bits = mu.reg_read(reg_const(reg)) & 0xFFFFFFFF
                out[name] = {"bits": "0x%08X" % bits,
                             "f": struct.unpack("<f", struct.pack("<I", bits))[0]}
            else:
                bits = mu.reg_read(reg_const(reg))
                out[name] = bits & 0xFFFFFFFF if reg in ("eax", "ebx", "ecx", "edx", "r8d") \
                    else bits & 0xFFFFFFFFFFFFFFFF
        elif "region" in loc:
            base = int(regions[loc["region"]]["base"], 16)
            blob = bytes(mu.mem_read(base + int(loc["offset"], 16), loc.get("size", 4)))
            out[name] = decode_blob(blob, loc.get("enc", "u32"))
    if getattr(mu, "_draws", None):
        out["_draws"] = mu._draws
    if getattr(mu, "_getters", None):
        out["getters_seen"] = mu._getters
    return out


def install_hooks(mu, spec, RET_SENTINEL):
    from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED
    from unicorn.x86_const import UC_X86_REG_RIP, UC_X86_REG_RAX, UC_X86_REG_RSP
    stubs = {int(s["va"], 16): (name, s) for name, s in (spec.get("stubs") or {}).items()}
    mu._draws = []
    mu._bad = None
    # --trace-calls: record (indirect site -> entered target) pairs. The
    # hook sees each entered RIP; when the previous RIP was an indirect
    # call/jump, the entered RIP is its runtime target (no operand
    # evaluation: the bytes already computed it). Stubbed callees pop
    # their return in-hook, so the fallthrough after them never records.
    mu._trace = {} if getattr(mu, "_trace_on", False) else None
    mu._prev = [None, 0]

    def _is_indirect_site(uc, va):
        try:
            blob = bytes(uc.mem_read(va, 8))
        except Exception:
            return False
        # Skip legacy/REX prefixes (vtable calls carry REX: 41 FF 51 20).
        i = 0
        while i < len(blob) and (blob[i] in (0x66, 0x67, 0xF0, 0xF2,
               0xF3, 0x2E, 0x3E, 0x26, 0x64, 0x65, 0x36) or
               0x40 <= blob[i] <= 0x4F):
            i += 1
        if i + 1 >= len(blob) or blob[i] != 0xFF:
            return False
        # FF /2 CALL, FF /4 JMP (group-5 indirect branch, any modrm).
        return (blob[i + 1] >> 3) & 7 in (2, 4)

    def hook_code(uc, address, size, data):
        uc._last_rip = address
        if uc._trace is not None:
            prev, psz = uc._prev
            # Entered address after an indirect call/jump is its runtime
            # target — including a switch case that falls through (target
            # == site + size). Only the stub-return transition (prev is a
            # stub entry, whose model pops the return in-hook) is skipped.
            if prev is not None and prev not in stubs and \
                    _is_indirect_site(uc, prev):
                uc._trace.setdefault("0x%X" % prev, {}).setdefault(
                    "0x%X" % address, 0)
                uc._trace["0x%X" % prev]["0x%X" % address] += 1
            uc._prev = [address, size]
        if address == RET_SENTINEL:
            uc.emu_stop()
            return
        hit = stubs.get(address)
        if hit is None:
            return
        name, stub = hit
        kind = stub.get("do", "return")
        if kind == "return":
            v = stub.get("value", 0)
            uc.reg_write(UC_X86_REG_RAX, int(v, 16) if isinstance(v, str) else v)
        elif kind == "return_xmm0":
            # FUN_140A3C050 returns a float clock in XMM0; minute comes from A6BD60.
            v = stub.get("value", "0x00000000")
            uc.reg_write(reg_const("xmm0"), int(v, 16))
        elif kind == "return_input":
            # Return a per-case input (the clock-minute stub reads `minute`).
            cur = getattr(uc, "_inputs", {}).get(stub["input"], 0)
            uc.reg_write(UC_X86_REG_RAX, int(cur, 0) if isinstance(cur, str) else cur)
        elif kind == "return_clock":
            # FUN_140A3C050(): XMM0 = clock_base (match clock seconds as float).
            cur = getattr(uc, "_inputs", {}).get("clock_base", 0.0)
            f = cur["f"] if isinstance(cur, dict) else float(cur)
            uc.reg_write(reg_const("xmm0"),
                         struct.unpack("<Q", struct.pack("<f", f) + b"\0\0\0\0")[0])
        elif kind == "support":
            # FUN_140A7F0F0(g, side): EDX=side; per-side inputs sup0/sup1.
            side = uc.reg_read(reg_const("edx")) & 0xFFFFFFFF
            cur = getattr(uc, "_inputs", {}).get("sup%d" % (side & 1), 0)
            uc.reg_write(UC_X86_REG_RAX, (int(cur, 0) if isinstance(cur, str) else cur) & 0xFF)
        elif kind == "teamword":
            # FUN_140488230(team+0x8E08, out): *out = teamwN by call order; returns out.
            n = len(getattr(uc, "_teamw", []))
            uc._teamw = getattr(uc, "_teamw", []) + [n]
            cur = getattr(uc, "_inputs", {}).get("teamw%d" % n, 0)
            out = uc.reg_read(reg_const("rdx"))
            uc.mem_write(out, struct.pack("<I", (int(cur, 0) if isinstance(cur, str) else cur) & 0xFFFFFFFF))
            uc.reg_write(UC_X86_REG_RAX, out)
        elif kind == "sheet_getter":
            # FUN_1408C9DF0(base, idx): RCX=base, EDX=idx; R8D carries the column
            # residue (arm1 loop sets R8D=2; arms 2/3/4 see entry r8_entry).
            # Keyed "blk,idx,col" (blk=1 when base==L11T+0x59650); actual keys
            # logged; missing keys read 0.
            base = uc.reg_read(reg_const("rcx"))
            idx = uc.reg_read(reg_const("edx")) & 0xFFFFFFFF
            col = uc.reg_read(reg_const("r8d")) & 0xFFFFFFFF
            l11t = int((spec.get("regions") or {})["L11T"]["base"], 16)
            blk = 1 if base == l11t + 0x59650 else 0
            key = "%d,%d,%d" % (blk, idx, col)
            uc._getters = getattr(uc, "_getters", []) + [key]
            sheet = getattr(uc, "_inputs", {}).get("sheet", {})
            uc.reg_write(UC_X86_REG_RAX, int(sheet.get(key, 0)) & 0xFFFFFFFFFFFFFFFF)
        elif kind == "rng_140A924E0_float32":
            # FUN_140A924E0(obj, range, stream): RCX=obj, EDX=range, R8D=stream
            # (traced at 0x14051EB01: R8D=2, the slot-2 GameRandom stream).
            # LCG step, then the float32 path: a=float32(ns*2^-31),
            # f=float32(a*range), trunc. Draws logged with
            # stream/range/result/state.
            from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8
            obj = uc.reg_read(UC_X86_REG_RCX)
            arange = uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF
            stream = uc.reg_read(UC_X86_REG_R8) & 0xFFFFFFFF
            state_addr = obj + 8 + 4 * stream
            (state,) = struct.unpack("<I", uc.mem_read(state_addr, 4))
            new = ((0x4513 - (state * 0x2D22B2E3 & 0xFFFFFFFF)) & 0x7FFFFFFF)
            uc.mem_write(state_addr, struct.pack("<I", new))
            scale = float(struct.unpack("<f", read_va_live(uc, 0x1426550D8, 4))[0])
            a = struct.unpack("<f", struct.pack("<f", float(new) * scale))[0]
            f = struct.unpack("<f", struct.pack("<f", a * float(arange)))[0]
            r = int(f)
            uc.reg_write(UC_X86_REG_RAX, r & 0xFFFFFFFF)
            uc._draws.append({"stream": stream, "range": arange, "result": r, "state": new})
        elif kind == "rng_140A924E0":
            # FUN_140A924E0(obj, range, stream): RCX=obj, EDX=range, R8D=stream.
            # Step *(obj+8+4*stream) and return trunc(state * 2^-31 * range).
            # immediates script-read: imul 0x2D22B2E3 at 0x140A92491
            # (69 44 91 08 E3 B2 22 2D), mov ecx 0x4513 at 0x140A92499,
            # & 0x7FFFFFFF, scale 2^-31 from .rdata.
            from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8
            obj = uc.reg_read(UC_X86_REG_RCX)
            arange = uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF
            stream = uc.reg_read(UC_X86_REG_R8) & 0xFFFFFFFF
            state_addr = obj + 8 + 4 * stream
            (state,) = struct.unpack("<I", uc.mem_read(state_addr, 4))
            new = ((0x4513 - (state * 0x2D22B2E3 & 0xFFFFFFFF)) & 0x7FFFFFFF)
            uc.mem_write(state_addr, struct.pack("<I", new))
            scale = float(struct.unpack("<f", read_va_live(uc, 0x1426550D8, 4))[0])
            r = int(float(new) * scale * arange)
            uc.reg_write(UC_X86_REG_RAX, r & 0xFFFFFFFF)
            uc._draws.append(r)
        elif kind == "return_region":
            # Return a synthetic region base (e.g. the edit manager for 1414B6A60).
            uc.reg_write(UC_X86_REG_RAX,
                         int((spec.get("regions") or {})[stub["region"]]["base"], 16))
        elif kind == "band_writer":
            # FUN_1414C6E90(rec, 0x30, band): RCX=rec, R8D=band (EDX=0x30).
            # Replicates rec[6]=(rec[6]&0x8FFFFFFF)|((band&7)<<28).
            rec = uc.reg_read(reg_const("rcx"))
            band = uc.reg_read(reg_const("r8d")) & 0xFFFFFFFF
            (w6,) = struct.unpack("<I", uc.mem_read(rec + 0x18, 4))
            uc.mem_write(rec + 0x18,
                         struct.pack("<I", (w6 & 0x8FFFFFFF) | ((band & 7) << 28)))
        elif kind == "counter_inc":
            # FUN_1414BCCD0(obj): EAX=[obj+off]; [obj+off]=EAX+1.
            obj = uc.reg_read(reg_const("rcx"))
            addr = obj + int(stub["offset"], 16)
            (v,) = struct.unpack("<i", uc.mem_read(addr, 4))
            uc.mem_write(addr, struct.pack("<i", v + 1))
            uc.reg_write(UC_X86_REG_RAX, v & 0xFFFFFFFFFFFFFFFF)
        elif kind == "store_edx_at_rcx_off":
            # FUN_1408C7710: [RCX+off] = EDX (the provider store).
            dst = uc.reg_read(reg_const("rcx")) + int(stub["offset"], 16)
            val = uc.reg_read(reg_const("edx")) & 0xFFFFFFFF
            uc.mem_write(dst, struct.pack("<I", val))
        elif kind == "noop":
            # engine-noop (stubs.json): no effect on the success path; the
            # call is dropped, registers untouched, the return is popped below.
            pass
        elif kind == "sqrt_xmm0":
            # return-input (stubs.json, op sqrtf): xmm0 low float = sqrtf(xmm0);
            # bits 32-127 kept, as sqrtss keeps them. IEEE-754 sqrt is correctly
            # rounded (double has >= 2*24+2 bits), so float(sqrt(double)) is exact.
            # x86 SSE sqrtss rules (Intel SDM Vol. 1 §4.8.3): a NaN input is
            # returned quieted; a negative input gives the default QNaN 0xFFC00000.
            xmm = uc.reg_read(reg_const("xmm0"))
            uc.reg_write(reg_const("xmm0"),
                         (xmm & ~0xFFFFFFFF) | sqrt_f32_bits(xmm & 0xFFFFFFFF))
        else:
            uc._bad = "stub %s has unknown do=%s" % (name, kind)
            uc.emu_stop()
            return
        rsp = uc.reg_read(UC_X86_REG_RSP)
        (ret,) = struct.unpack("<Q", uc.mem_read(rsp, 8))
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)
        uc.reg_write(UC_X86_REG_RIP, ret)

    def hook_bad(uc, access, address, size, value, data):
        uc._bad = ("unmodelled %s at %#x (size %d): no region or global covers it; "
                   "extend the spec" % ("read" if data == "r" else "write", address, size))
        uc.emu_stop()
        return True

    mu.hook_add(UC_HOOK_CODE, hook_code)
    mu.hook_add(UC_HOOK_MEM_READ_UNMAPPED, hook_bad, "r")
    mu.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, hook_bad, "w")


def run_case(mu, spec, RET_SENTINEL, inputs, timeout_ms=5000):
    from unicorn.x86_const import UC_X86_REG_RIP, UC_X86_REG_RSP
    entry = int(spec.get("entry", spec["va"]), 16)
    end = int(spec.get("end", "0x0"), 16) if spec.get("end") else RET_SENTINEL
    rsp = stack_top(spec) - 8
    mu.mem_write(rsp, struct.pack("<Q", RET_SENTINEL))
    mu.reg_write(UC_X86_REG_RSP, rsp)
    mu.reg_write(UC_X86_REG_RIP, entry)
    # Slices start mid-function: callee-saved registers the slice never writes
    # (RBP here: the slice spills EBX to [RBP+0x48]) must still be sane, so
    # reset every register the spec does not drive to a fixed default.
    for reg in ("rbx", "rbp", "rcx", "rdx", "r8", "r9", "rax",
                "xmm0", "xmm1", "xmm2", "xmm3", "xmm6", "xmm7"):
        try:
            mu.reg_write(reg_const(reg), 0)
        except Exception:
            pass
    # Fixed register / stack defaults (e.g. rbp into the stack for slices).
    for reg, val in (spec.get("reg_defaults") or {}).items():
        if val == "stack_top":
            mu.reg_write(reg_const(reg), stack_top(spec))
        else:
            mu.reg_write(reg_const(reg), int(val, 0) if isinstance(val, str) else val)
    set_inputs(mu, spec, inputs)
    mu._draws = []
    mu._prev = [None, 0]
    mu._bad = None
    mu._getters = []
    mu._teamw = []
    mu._inputs = dict(inputs)
    regions = spec.get("regions") or {}
    # Point image globals at synthetic objects (patch_addrs: VA -> region name).
    for va_s, rname in (spec.get("patch_addrs") or {}).items():
        mu.mem_write(int(va_s, 16),
                     struct.pack("<Q", int(regions[rname]["base"], 16)))
    # Static intra-region linkage (regions_init: "REGION+off" -> region base or int).
    for dst, val in (spec.get("regions_init") or {}).items():
        dname, doff = dst.split("+", 1)
        addr = int(regions[dname]["base"], 16) + int(doff, 16)
        if isinstance(val, str) and val in regions:
            mu.mem_write(addr, struct.pack("<Q", int(regions[val]["base"], 16)))
        else:
            mu.mem_write(addr, struct.pack("<I", int(val, 0) if isinstance(val, str) else val))
    # Fixed linkage the decider reads: G -> MI/setup, ctx -> stats base
    # (kept for the older minute-0-only spec shape; the full-closure spec
    # drives all linkage through regions_init/absfills).
    if "G" in regions and "MI" in regions and "setup" in regions:
        g = int(regions["G"]["base"], 16)
        mu.mem_write(g + 0x270, struct.pack("<Q", int(regions["MI"]["base"], 16)))
        mu.mem_write(g + 0x2AD0, struct.pack("<Q", int(regions["setup"]["base"], 16)))
    if "ctx" in regions and "stats" in regions:
        mu.mem_write(int(regions["ctx"]["base"], 16) + 0xE0,
                     struct.pack("<Q", int(regions["stats"]["base"], 16)))
    # Caller-supplied linkage (ctx_e0 etc.) overrides the defaults above.
    if "ctx_e0" in inputs:
        mu.mem_write(int(regions["ctx"]["base"], 16) + 0xE0,
                     struct.pack("<Q", inputs["ctx_e0"]))
    snap = {n: bytes(mu.mem_read(int(r["base"], 16), int(r["size"], 16)))
            for n, r in regions.items()}
    img_snap = {va_s: bytes(mu.mem_read(int(va_s, 16), 8))
                for va_s in (spec.get("patch_addrs") or {})}
    try:
        mu.emu_start(entry, end, timeout=timeout_ms * 1000)
    except Exception as e:
        return None, "emu error: %s (last RIP %#x)" % (e, getattr(mu, "_last_rip", 0))
    if mu._bad is not None:
        return None, mu._bad
    out = decode(mu, spec)
    for n, r in regions.items():
        mu.mem_write(int(r["base"], 16), snap[n])
    for va_s, blob in img_snap.items():
        mu.mem_write(int(va_s, 16), blob)
    return out, None


def read_image_f32(sections, image_path, va):
    """Read one float from the user's image (constants for boundary builders)."""
    blob = read_va_from(image_path, sections, va, 4)
    if blob is None:
        raise SystemExit("diffharness: boundary constant unreadable at %#x" % va)
    return struct.unpack("<f", blob)[0]


def build_boundary(spec, b, image):
    """One edge-vector case from image-read constants. kinds:
    - cone_edge: cand/hold geometry around the annulus + half-angle edges.
      args: side ("ahead"|"behind"), radius ("touch"|"inside"|"dust"),
      angle ("center"|"edge"|"flip"), with cone/outer/inner from post_fill.
    - strength_edge: own/chal floats straddling the 0.9 margin read from the image.
    """
    sections, image_path = image
    kind = b.get("boundary")
    if kind == "cone_edge":
        outer = read_image_f32(sections, image_path, 0x14259BE04)
        cone_deg = read_image_f32(sections, image_path, 0x14259BE5C)
        half = read_image_f32(sections, image_path, 0x142565600)
        case = {"name": b.get("name", kind)}
        dist = {"touch": outer, "inside": outer * (1 - 2**-20),
                "dust": 0.0}[b.get("radius", "inside")]
        if b.get("side", "ahead") == "ahead":
            case.update(cand_x=0.0, cand_z=-dist, hold_x=0.0, hold_z=0.0)
        else:
            case.update(cand_x=0.0, cand_z=dist, hold_x=0.0, hold_z=0.0)
        half_angle = half * cone_deg
        head = {"center": 0.0, "edge": half_angle, "flip": 180.0}[b.get("angle", "center")]
        case["headbits"] = head
        if b.get("inner", "zero") == "quirk":
            # T7a's inner-word quirk (0x1404F654F writes 0.0, 0x1404F6580
            # overwrites the low byte with 1): constructed, never committed.
            blob = bytearray(struct.pack("<f", 0.0))
            blob[0] = 1
            case["innerbits"] = {"bits": "0x%08X" % struct.unpack("<I", bytes(blob))[0]}
        else:
            case["innerbits"] = 0.0
        return case
    if kind == "strength_edge":
        margin = read_image_f32(sections, image_path, 0x14258C260)
        base = b.get("base", 75.0)
        delta = {"above": 1.0, "below": -1.0, "at": 0.0}[b.get("dirn", "above")]
        side, mode = b.get("team_side", 0), b.get("mode", 0)
        case = {"name": b.get("name", kind)}
        case.update(side=side, mode=mode, strat3_in=2, strat4=0, strat5=0,
                    strat6=0, rng2=1, r8_entry=0, mi116c=0, mi1168=0,
                    minute=0, clock_base=100.0, maskbit=4, sup0=3, sup1=4,
                    teamw0=1, teamw1=2, ma_fill=0,
                    ma_off=0x25FF4 + mode + side * 8,
                    own_float=float(base + delta), own_off=0xC404 + side * 0x2804,
                    chal_float=float(base), chal_off=0xC404 + (1 - side) * 0x2804,
                    sheet={})
        return case
    raise SystemExit("diffharness: unknown boundary %r" % (b,))


def gen_inputs(spec, seed, count, image=None):
    import random
    rnd = random.Random(seed)
    cases = [dict(c) for c in spec.get("fixed_cases") or []]
    schema = spec.get("input_schema") or {}
    # Algorithmic boundaries: edge vectors computed at generation time from
    # constants read out of the user's image (never committed).
    for b in spec.get("boundary_cases") or []:
        cases.append(build_boundary(spec, b, image))
    # Normalize fixed f32 cases to bits form.
    for c in cases:
        for name, value in list(c.items()):
            loc = (spec.get("inputs") or {}).get(name, {})
            if loc.get("enc") == "f32" and not isinstance(value, dict):
                c[name] = f32_bits(value)
    # Full-range kinds enumerate every value (band: all 256 bytes).
    full = []
    for name, rule in schema.items():
        if rule.get("kind") == "all_u8":
            full.append(name)
    if full:
        import itertools
        lo, hi = 0, 255
        for combo in itertools.product(range(lo, hi + 1), repeat=len(full)):
            case = {}
            for name, rule in schema.items():
                kind = rule.get("kind", "u8")
                if kind == "all_u8":
                    case[name] = combo[full.index(name)]
                elif kind == "choice":
                    case[name] = rule["values"][0]
                elif kind == "derived":
                    case[name] = eval(rule["expr"], {"__builtins__": {}}, dict(case))
            for name, val in (spec.get("post_fill") or {}).items():
                case.setdefault(name, val)
            cases.append(case)
    while len(cases) < count:
        case = {}
        for name, rule in schema.items():
            loc = (spec.get("inputs") or {}).get(name, {})
            kind = rule.get("kind", "u8")
            if kind in ("u8", "all_u8"):
                case[name] = rnd.randint(rule.get("lo", 0), rule.get("hi", 255))
            elif kind == "i32":
                case[name] = rnd.randint(rule.get("lo", -100), rule.get("hi", 100))
            elif kind == "f32":
                if "edges" in rule and rnd.random() < 0.3:
                    case[name] = f32_bits({"bits": rnd.choice(rule["edges"])})
                else:
                    case[name] = f32_bits(rnd.uniform(rule.get("lo", -50.0), rule.get("hi", 50.0)))
            elif kind == "quirk_or_zero":
                # T7a's inner word: usually 0.0, occasionally the low-byte-1
                # quirk word (constructed as in build_boundary, never committed).
                if rnd.random() < 0.1:
                    blob = bytearray(struct.pack("<f", 0.0))
                    blob[0] = 1
                    case[name] = {"bits": "0x%08X" % struct.unpack("<I", bytes(blob))[0]}
                else:
                    case[name] = f32_bits(0.0)
            elif kind == "choice":
                case[name] = rnd.choice(rule["values"])
            elif kind == "derived":
                # Address derived from already-drawn inputs (schema order):
                # side-strided words, never degenerate (provenance: RE notes).
                case[name] = eval(rule["expr"], {"__builtins__": {}}, dict(case))
        cases.append(case)
    # Static per-case defaults that must not consume the random stream.
    # {"from_image": VA} reads the float from the user's image at gen time.
    for case in cases:
        for name, val in (spec.get("post_fill") or {}).items():
            if isinstance(val, dict) and "from_image" in val:
                sections, image_path = image
                case.setdefault(name, f32_bits(read_image_f32(
                    sections, image_path, int(val["from_image"], 16))))
            else:
                case.setdefault(name, val)
    return cases[:count]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--spec", required=True)
    ap.add_argument("--out")
    ap.add_argument("--check-spec", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--trace-calls", default=None,
                    help="write {site: {target: count}} for harness-reached "
                    "indirect calls/jumps (coverage of the dispatch table)")
    args = ap.parse_args()
    spec = resolve_stubs(load_spec(args.spec))
    kind = image_kind(args.exe)
    sections = flat_sections(args.exe) if kind == "flat" else parse_pe_sections(args.exe)
    if args.check_spec:
        errors = check_spec(spec, sections, args.exe)
        for e in errors:
            print("spec error: %s" % e)
        return 1 if errors else 0
    errors = check_spec(spec, sections, args.exe)
    if errors:
        for e in errors:
            print("spec error: %s" % e, file=sys.stderr)
        return 1
    mu, RET = build_emu(spec, args.exe, sections)
    mu._trace_on = args.trace_calls is not None
    install_hooks(mu, spec, RET)
    seed = args.seed if args.seed != 1 or "seed" not in spec else spec["seed"]
    count = args.count if args.count != 300 or "count" not in spec else spec["count"]
    cases = gen_inputs(spec, seed, count, (sections, args.exe))
    out = open(args.out, "w") if args.out else sys.stdout
    for n, inputs in enumerate(cases):
        result, err = run_case(mu, spec, RET, inputs)
        if err is not None:
            print("case %d failed: %s" % (n, err), file=sys.stderr)
            print("  inputs: %s" % json.dumps(inputs)[:300], file=sys.stderr)
            return 1
        out.write(json.dumps({"va": spec["va"], "in": inputs, "out": result}) + "\n")
    if args.out:
        out.close()
    if args.trace_calls is not None:
        with open(args.trace_calls, "w") as tf:
            json.dump(mu._trace or {}, tf, indent=1, sort_keys=True)
    print("%d cases -> %s" % (len(cases), args.out or "stdout"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
