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
import ctypes
import ctypes.util
import json
import math
import os
import struct
import sys

PAGE = 0x1000
IMAGE_CHUNK = 1 << 24  # bytes per read when streaming the image (16 MiB)
# Per-case emulation budget in instructions (deterministic; a wall-clock timeout varied with machine load). A case still
# running past it is PES not returning. Measured: the largest valid case of the recorded sets (pilot 252, cutscene 833,
# presentation 639, core 2570 specs, run.py valid_icounts, 26-10-26) is 2,377,688 instructions, VA 0x1405628D0 (core).
# The budget is 4x that. Pilot, cutscene and presentation ran every case; core ran under the non-return cap (9 functions
# capped, so their later cases were not measured).
CASE_INSN_BUDGET = 4 * 2_377_688
# A function gets at most this many non-returning cases; after the last one it generates no further cases.
NONRETURN_CAP = 3


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


_LIBM = ctypes.CDLL(ctypes.util.find_library("m"))
_LIBM.logf.restype, _LIBM.logf.argtypes = ctypes.c_float, [ctypes.c_float]
_LIBM.powf.restype, _LIBM.powf.argtypes = ctypes.c_float, [ctypes.c_float, ctypes.c_float]


def _f32(b):
    return struct.unpack("<f", struct.pack("<I", b))[0]


def _bits(f):
    return struct.unpack("<I", struct.pack("<f", f))[0]


def logf_f32_bits(b):
    """Bits of logf(x) for the float with bits b (the logf_xmm0 hook's model: the host C library, as pes_model_logf)."""
    return _bits(_LIBM.logf(_f32(b)))


def powf_f32_bits(b, e):
    """Bits of powf(x, y) for the floats with bits b, e (the powf_xmm0 hook's model, as pes_model_powf)."""
    return _bits(_LIBM.powf(_f32(b), _f32(e)))


def resolve_stubs(spec):
    """Install every stubs.json model in spec["stubs"]: the one table the translator reads too (translate/stubs.py).
    The choice is the table's, never the spec's: a legacy stub_refs field may only name modelled callees."""
    sys.path.insert(0, TRANSLATE_DIR)
    import stubs as stub_table
    table = stub_table.load()
    stub_table.check_spec_refs(spec, table, spec.get("name") or "spec")
    stubs = dict(spec.get("stubs") or {})
    for va, entry in stub_table.models(table).items():
        stubs["stub_%X" % va] = {"va": entry["va"], "do": entry["do"]}
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
    if enc == "u64":
        return struct.unpack("<Q", blob[:8])[0]
    raise ValueError("unknown enc %s" % enc)


def f32_bits(value):
    if isinstance(value, dict) and "bits" in value:
        bits = int(value["bits"], 16)
        return {"bits": "0x%08X" % bits, "f": struct.unpack("<f", struct.pack("<I", bits))[0]}
    bits = struct.unpack("<I", struct.pack("<f", float(value)))[0]
    return {"bits": "0x%08X" % bits, "f": float(value)}


def new_x86_64():
    """The harness emulator: unicorn x86-64 with the Broadwell CPU model. The default model rejects SSE4.1
    (pmovsxbd, pmulld: INVALID instruction), which PES's code uses. Every emulator in the harness is made here."""
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
    from unicorn.x86_const import UC_CPU_X86_BROADWELL
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.ctl_set_cpu_model(UC_CPU_X86_BROADWELL)
    return uc


def build_emu(spec, image_path, sections):
    mu = new_x86_64()
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


def apply_seed(mu, spec, seed_artifact):
    """Fill the spec regions with real object bytes (a seed.py artifact)
    before the per-case inputs: the seed is the base state (real vtables,
    class bytes, init flags, MI); the inputs and spec linkage are applied
    on top, so the per-case variations still win exactly as in the zero run."""
    regions = spec.get("regions") or {}
    for r in seed_artifact.get("regions") or []:
        rname = r.get("name")
        if rname not in regions:
            continue
        base = int(regions[rname]["base"], 16)
        mu.mem_write(base + int(r.get("off", 0)), bytes.fromhex(r["hex"]))


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
    mu._icount = 0  # the code hook below counts every executed instruction (budgets, valid_icounts)
    # --trace-addrs: every executed instruction address (union over cases),
    # for basic-block coverage in the driver. Stub entries record too (their
    # in-hook return pops the call, so no stub body pollutes the set).
    mu._addrs = set() if getattr(mu, "_addrs_on", False) else None

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
        uc._icount += 1
        if uc._addrs is not None:
            uc._addrs.add(address)
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
        if address == getattr(uc, "_ret_sentinel", RET_SENTINEL):  # snapshot replay sets it per call
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
        elif kind == "abort":
            # Tamper trap (stubs.json abort-trap): reaching it means the
            # tamper path fired. Loud failure, never a silent model.
            uc._bad = "abort trap %s reached" % name
            uc.emu_stop()
            return
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
        elif kind == "logf_xmm0":
            # libm (stubs.json, op logf): xmm0 low float = logf(xmm0); bits 32-127 kept.
            xmm = uc.reg_read(reg_const("xmm0"))
            uc.reg_write(reg_const("xmm0"), (xmm & ~0xFFFFFFFF) | logf_f32_bits(xmm & 0xFFFFFFFF))
        elif kind == "powf_xmm0":
            # libm (stubs.json, op powf): xmm0 low float = powf(xmm0, xmm1); bits 32-127 of xmm0 kept.
            xmm = uc.reg_read(reg_const("xmm0"))
            e = uc.reg_read(reg_const("xmm1")) & 0xFFFFFFFF
            uc.reg_write(reg_const("xmm0"), (xmm & ~0xFFFFFFFF) | powf_f32_bits(xmm & 0xFFFFFFFF, e))
        else:
            uc._bad = "stub %s has unknown do=%s" % (name, kind)
            uc.emu_stop()
            return
        rsp = uc.reg_read(UC_X86_REG_RSP)
        (ret,) = struct.unpack("<Q", uc.mem_read(rsp, 8))
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)
        uc.reg_write(UC_X86_REG_RIP, ret)

    def hook_bad(uc, access, address, size, value, data):
        kind = "read" if data == "r" else "write"
        lazy = getattr(uc, "_lazy", None)  # snapshot run: map the page, or stop at a hole
        if lazy is not None:
            if lazy.handle(address):
                return True
            uc._bad = (lazy.unmapped_reason(address)
                       or "unmapped %s at %#x (size %d): a snapshot hole or outside the process" % (kind, address, size))
        else:
            uc._bad = ("unmodelled %s at %#x (size %d): no region or global covers it; "
                       "extend the spec" % (kind, address, size))
        uc.emu_stop()
        return True

    mu.hook_add(UC_HOOK_CODE, hook_code)
    mu.hook_add(UC_HOOK_MEM_READ_UNMAPPED, hook_bad, "r")
    mu.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, hook_bad, "w")


NONRETURN = "PES did not return within the instruction budget"

def write_meta(out_path, requested, stats, skipped, valid_icounts):
    """The corpus's account beside it, never silent: what was valid, what was skipped and why, and the instruction count
    of every valid case. stdout carries the summary line the driver records."""
    if out_path:
        meta = {"requested": requested, "valid": len(valid_icounts), "nonreturn": stats["nonreturn"],
                "failed": stats["failed"], "capped": stats["capped"], "valid_icounts": valid_icounts,
                "skipped": skipped}
        with open(out_path + ".meta.json", "w") as f:
            json.dump(meta, f, indent=1)
    print("summary valid=%d requested=%d nonreturn=%d failed=%d capped=%d icount_max=%d" % (
        len(valid_icounts), requested, stats["nonreturn"], stats["failed"], stats["capped"],
        max(valid_icounts) if valid_icounts else 0))


# Real-instance cases (seed.py --bank-classes banks): a field that an instance
# holds takes the instance's real value, not the spec's draw.
_FIELD_SIZE = {"u8": 1, "u32": 4, "i32": 4, "f32": 4, "u64": 8}


def real_region_bytes(art, rname, off, size):
    """Bytes [off, off+size) of region rname in seed artifact art, or None when
    the instance does not hold that field (region not seeded, or outside its window)."""
    for r in art.get("regions") or []:
        if r.get("name") != rname:
            continue
        lo = int(r.get("off", 0))
        if off < lo or off + size > lo + int(r["size"]):
            return None
        return bytes.fromhex(r["hex"])[off - lo:off - lo + size]
    return None


def real_value(art, loc):
    """The value instance art holds at input location loc, or None."""
    enc = loc.get("enc", "u32")
    if "region" not in loc or enc not in _FIELD_SIZE:
        return None
    blob = real_region_bytes(art, loc["region"], int(loc["offset"], 16), _FIELD_SIZE[enc])
    return None if blob is None else decode_blob(blob, enc)


def nearby_value(real, enc, rnd):
    """A value near a real one: +-5% for f32 (a 0.0 field moves by 0.05), +-1 or
    +-2 for integers, kept inside the encoding's range. A u64 is a pointer or handle in every spec that holds
    one: a nearby pointer is no real state, so it keeps its real value."""
    if enc == "u64":
        return real
    if enc == "f32":
        f = real["f"]
        step = 0.05 * abs(f) if f else 0.05
        return f32_bits(f + rnd.choice((-1.0, 1.0)) * step)
    lo, hi = {"u8": (0, 0xFF), "u32": (0, 0xFFFFFFFF), "i32": (-0x80000000, 0x7FFFFFFF)}[enc]
    return min(max(real + rnd.choice((-2, -1, 1, 2)), lo), hi)


def held_value(art, name, loc):
    """The real value instance art holds for input name at loc, or None. A region field is read from
    the instance's bytes. A register or stack argument is the value recorded at the call's entry
    (artifact key "scalars"): a register in full, a stack slot masked to its size. XMM is not held."""
    if "region" in loc:
        return real_value(art, loc)
    enc = loc.get("enc", "u32")
    if enc not in _FIELD_SIZE or str(loc.get("reg", "")).startswith("xmm"):
        return None
    raw = (art.get("scalars") or {}).get(name)
    if raw is None:
        return None
    if "reg" in loc:
        return raw
    if "stack_offset" in loc:
        n = _FIELD_SIZE[enc]
        return decode_blob((raw & ((1 << (8 * n)) - 1)).to_bytes(n, "little"), enc)
    return None


def bank_case(spec, bank, n, inputs, seed, perturb):
    """Real-instance case n -> (inputs, instance artifact). Case n runs instance
    n % N. A field the instance holds takes its real value (mode 0), a nearby one
    (1), the same field of instance n+1 (2), or the spec's own draw (3). perturb
    picks the mode per (seed, case, field), deterministically; without it every
    held field is real. A field no instance holds keeps its draw."""
    import random
    k = n % len(bank)
    art = bank[k]
    labels = set(spec.get("labels") or [])
    out = dict(inputs)
    for name, value in inputs.items():
        loc = (spec.get("inputs") or {}).get(name)
        if name in labels or loc is None:
            continue
        real = held_value(art, name, loc)
        if real is None:
            continue
        rnd = random.Random("%s:%d:%s" % (seed, n, name))
        mode = rnd.randrange(4) if perturb else 0
        if mode == 0:
            out[name] = real
        elif mode == 1:
            out[name] = nearby_value(real, loc.get("enc", "u32"), rnd)
        elif mode == 2:
            other = real_value(bank[(k + 1) % len(bank)], loc)
            out[name] = real if other is None else other
        else:
            out[name] = value
    return out, art


def unheld_inputs(spec, art, inputs):
    """Input names the instance does not hold: a traced case must replay every input it read."""
    labels = set(spec.get("labels") or [])
    out = []
    for name in inputs:
        loc = (spec.get("inputs") or {}).get(name)
        if name not in labels and loc is not None and held_value(art, name, loc) is None:
            out.append(name)
    return out



def run_case(mu, spec, RET_SENTINEL, inputs, seed=None, zero=False):
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
    if zero:
        # Real-instance cases: every region starts from zero, so bytes an
        # earlier instance seeded cannot leak into this case.
        for r in (spec.get("regions") or {}).values():
            mu.mem_write(int(r["base"], 16), bytes(int(r["size"], 16)))
    if seed is not None:
        apply_seed(mu, spec, seed)
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
    # A run that does not return is rolled back out of the coverage and call traces: only returning cases count.
    pre_addrs = set(mu._addrs) if mu._addrs is not None else None
    pre_trace = {k: dict(v) for k, v in mu._trace.items()} if mu._trace is not None else None
    err = None
    mu._icount = 0
    try:
        # An instruction budget, not a wall-clock timeout: a timeout cuts a case at a load-dependent point and the
        # registers it leaves were recorded as PES's output (the gate failed 0x14008CE00 only under load).
        mu.emu_start(entry, end, count=CASE_INSN_BUDGET)
    except Exception as e:
        err = "emu error: %s (last RIP %#x)" % (e, getattr(mu, "_last_rip", 0))
    mu._last_icount = mu._icount
    # A case that does not return within the instruction budget is not an output: PES did not return on it.
    if err is None and mu.reg_read(UC_X86_REG_RIP) not in (end, RET_SENTINEL):
        err = NONRETURN
    # hook_bad records the unmapped access and stops emulation; unicorn then reports the stop as UC_ERR_MAP, so the
    # recorded reason takes precedence.
    if mu._bad is not None:
        err = mu._bad
    out = decode(mu, spec) if err is None else None
    for n, r in regions.items():
        mu.mem_write(int(r["base"], 16), snap[n])
    for va_s, blob in img_snap.items():
        mu.mem_write(int(va_s, 16), blob)
    # A skipped case (non-returning or failed) contributes nothing: its coverage and call traces are rolled back.
    if err is not None:
        if pre_addrs is not None:
            mu._addrs.clear()
            mu._addrs.update(pre_addrs)
        mu._trace = pre_trace
    return out, err


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


_JCC_SIMPLE = {
    # Jcc -> taken relation on (left - right). Only the integer compares
    # the steerer solves; FP guards fall through to mutation.
    "JE": "==", "JZ": "==",
    "JNE": "!=", "JNZ": "!=",
    "JL": "<s", "JNGE": "<s",
    "JGE": ">=s", "JNL": ">=s",
    "JLE": "<=s", "JNG": "<=s",
    "JG": ">s", "JNLE": ">s",
    "JB": "<u", "JC": "<u", "JNAE": "<u",
    "JAE": ">=u", "JNC": ">=u", "JNB": ">=u",
    "JA": ">u", "JNBE": ">u",
    "JBE": "<=u", "JNA": "<=u",
}


def _split_ops(op_str):
    return [p.strip() for p in op_str.split(",")] if op_str else []


def _reg_base(reg):
    r = reg.strip().lower()
    sub = {"al": "rax", "ah": "rax", "ax": "rax", "eax": "rax",
           "bl": "rbx", "bh": "rbx", "bx": "rbx", "ebx": "rbx",
           "cl": "rcx", "ch": "rcx", "cx": "rcx", "ecx": "rcx",
           "dl": "rdx", "dh": "rdx", "dx": "rdx", "edx": "rdx",
           "sil": "rsi", "si": "rsi", "esi": "rsi",
           "dil": "rdi", "di": "rdi", "edi": "rdi",
           "bpl": "rbp", "bp": "rbp", "ebp": "rbp",
           "spl": "rsp", "sp": "rsp", "esp": "rsp",
           "r8b": "r8", "r8w": "r8", "r8d": "r8",
           "r9b": "r9", "r9w": "r9", "r9d": "r9",
           "r10b": "r10", "r10w": "r10", "r10d": "r10",
           "r11b": "r11", "r11w": "r11", "r11d": "r11",
           "r12b": "r12", "r12w": "r12", "r12d": "r12",
           "r13b": "r13", "r13w": "r13", "r13d": "r13",
           "r14b": "r14", "r14w": "r14", "r14d": "r14",
           "r15b": "r15", "r15w": "r15", "r15d": "r15"}
    return sub.get(r, r)


def _mem_key(ops):
    """Canonical key of a memory operand: (base, index, scale, disp)."""
    import re
    m = re.match(r"^(?:byte|word|dword|qword|xmmword|ymmword)\s+ptr\s*\[(.*)\]$",
                 ops.strip(), re.I)
    if not m:
        return None
    inner = m.group(1).strip()
    if inner.startswith("rip"):
        return ("rip", inner)
    base, index, scale, disp = None, None, 1, 0
    for tok in [t.strip() for t in inner.split("+")]:
        tm = re.match(r"^([a-z][a-z0-9]*)\*([1248])$", tok, re.I)
        if tm:
            index, scale = _reg_base(tm.group(1)), int(tm.group(2))
            continue
        if re.match(r"^[a-z][a-z0-9]*$", tok, re.I):
            if base is None:
                base = _reg_base(tok)
            else:
                return None
            continue
        try:
            disp += int(tok, 0)
        except ValueError:
            return None
    return (base, index, scale, disp)

def _cap_lines(image_path, va):
    """Capstone decode with xlate's function windowing (stops at the real
    end instead of running into the next function)."""
    import sys as _sys
    import os as _os
    _sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                                      "..", "translate"))
    import xlate as _x
    base = int(va, 16) if isinstance(va, str) else va
    return [(a, mn.upper(), list(ops))
            for a, mn, ops in _x.capstone_decode(image_path, base)]


def _decode_guards(image_path, va):
    """[(branch_addr, target, fallthrough, mnemonic)] over one function."""
    import sys as _sys
    import os as _os
    _sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                                      "..", "translate"))
    import xlate as _x
    out = []
    lines = [(a, mn.upper(), list(ops))
             for a, mn, ops in _x.capstone_decode(
                 image_path, int(va, 16) if isinstance(va, str) else va)]
    aset = {a for a, _, _ in lines}
    for i, (addr, mn, ops) in enumerate(lines):
        if mn not in _x.CCMAP and mn != "JMP":
            continue
        try:
            tgt = int(ops[0].strip(), 16)
        except (ValueError, IndexError):
            continue  # indirect jump: no static target to steer to
        if tgt not in aset:
            continue  # tail jump out of the function, not a local branch
        fallthru = lines[i + 1][0] if i + 1 < len(lines) else None
        out.append((addr, tgt, fallthru, mn))
    return out


def _input_addrs(spec):
    """{input name: (region_base, offset, size)} for region-backed inputs."""
    sizes = {"u8": 1, "u32": 4, "i32": 4, "u64": 8, "f32": 4}
    regions = spec.get("regions") or {}
    out = {}
    for name, loc in (spec.get("inputs") or {}).items():
        if "reg" in loc or "stack_offset" in loc or "absolute" in loc \
                or "absfill" in loc or "stub" in loc or loc.get("input"):
            continue
        rname = loc.get("region")
        if rname not in regions:
            continue
        try:
            off = int(loc.get("offset", "0x0"), 16)
        except ValueError:
            continue
        out[name] = (int(regions[rname]["base"], 16), off,
                     sizes.get(loc.get("enc", "u32"), 4))
    return out


def _trace_guard_deps(image_path, va, in_addrs, max_ins=400):
    """{branch_addr: [input names]}: inputs feeding each guard's compare.

    Backward slice over the capstone decode: from the CMP/TEST at the
    branch, follow MOV/LEA/MOVSXD/SETcc/CMOVcc data flow through
    registers and memory until a spec input word (region offset) or an
    untracked source (call result, rip-global, immediate) is reached.
    Needs are ("reg", base) or ("mem", key); both halves of a compare
    are tracked (SETcc results resolve through their own flag-setter).
    """
    lines = _cap_lines(image_path, va)
    import sys as _sys2
    import os as _os2
    _sys2.path.insert(0, _os2.path.join(_os2.path.dirname(_os2.path.abspath(__file__)),
                                        "..", "translate"))
    import xlate as _x2
    out = {}
    for n, (addr, mn, ops) in enumerate(lines):
        if mn not in _x2.CCMAP:
            continue
        # The flag-setter: nearest CMP/TEST above the branch.
        sn = None
        for k in range(n - 1, max(n - 6, -1), -1):
            if lines[k][1] in ("CMP", "TEST"):
                sn = k
                break
        if sn is None:
            out[addr] = []
            continue
        _, smn, sops = lines[sn]
        needs = set()
        for sop in sops[:2]:
            if "[" in sop:
                mk = _mem_key(sop)
                if mk is not None and mk[0] != "rip":
                    needs.add(("mem", mk))
            else:
                try:
                    int(sop, 0)
                except ValueError:
                    needs.add(("reg", _reg_base(sop)))
        if smn == "TEST" and len(sops) == 2 and \
                sops[0].lower() == sops[1].lower():
            pass  # self-test: the one reg need already added
        for k in range(sn - 1, max(sn - max_ins, -1), -1):
            if not needs:
                break
            _, kmn, kops = lines[k]
            if kmn == "CALL" and kops:
                # Transparent callee: a single MOV reg,[base+disp];RET
                # forwards one input word (load-load-compare chains). Step
                # through it: the returned reg is fed by the callee's load.
                try:
                    tva = int(kops[0].strip(), 16)
                except (ValueError, IndexError):
                    tva = None
                if tva is not None:
                    try:
                        cl = _cap_lines(image_path, tva)
                    except (OSError, SystemExit, ValueError):
                        cl = []
                    if len(cl) == 2 and cl[0][1] == "MOV" and \
                            len(cl[0][2]) == 2 and cl[1][1] == "RET":
                        _d, _s = cl[0][2][0], cl[0][2][1]
                        if "[" not in _d and "[" in _s:
                            mk = _mem_key(_s)
                            if mk is not None and mk[0] != "rip":
                                for nd in list(needs):
                                    if nd[0] == "reg" and \
                                            nd[1] == _reg_base(_d):
                                        needs.discard(nd)
                                        needs.add(("mem", mk))
                                continue
                # Opaque call: memory needs survive (a call cannot rewrite
                # a synthetic region); register needs survive only when the
                # caller set them from entry state below (a MOV from an
                # entry/dedicated reg later in the slice re-resolves them).
                # Check the remainder of the slice: a need re-defined from
                # entry regs or stack survives; anything call-derived dies.
                keep = {nd for nd in needs if nd[0] == "mem"}
                for nd in list(needs):
                    if nd[0] != "reg":
                        continue
                    for kk in range(k - 1, max(k - max_ins, -1), -1):
                        _, smn2, sops2 = lines[kk]
                        if smn2 in ("MOV", "MOVZX", "MOVSX", "MOVSXD") and sops2 and \
                                _reg_base(sops2[0]) == nd[1]:
                            src2 = sops2[-1]
                            if "[" in src2:
                                mk2 = _mem_key(src2)
                                if mk2 is not None and mk2[0] != "rip":
                                    keep.add(nd)
                            else:
                                try:
                                    int(src2, 0)
                                except ValueError:
                                    keep.add(nd)
                            break
                        if smn2 in ("CALL", "RET"):
                            break
                needs = keep
                continue
            if kmn == "RET":
                needs = {nd for nd in needs if nd[0] == "mem"}
                continue
            if kmn in ("MOV", "MOVZX", "MOVSX", "MOVSXD", "LEA") and kops:
                dst, src = kops[0], kops[-1]
                dbase = _reg_base(dst) if "[" not in dst else None
                dmem = _mem_key(dst) if "[" in dst else None
                hit = dbase is not None and ("reg", dbase) in needs
                hitm = dmem is not None and ("mem", dmem) in needs
                if not hit and not hitm:
                    continue
                if hit:
                    needs.discard(("reg", dbase))
                if hitm:
                    needs.discard(("mem", dmem))
                if kmn == "LEA":
                    continue  # address math, no input content
                if "[" in src:
                    mk = _mem_key(src)
                    if mk is None:
                        continue
                    if mk[0] == "rip":
                        continue  # image global, not a case input
                    needs.add(("mem", mk))
                else:
                    try:
                        int(src, 0)
                    except ValueError:
                        needs.add(("reg", _reg_base(src)))
                continue
            if kmn in ("XOR", "SUB") and kops and len(kops) == 2 and \
                    kops[0].lower() == kops[1].lower():
                # reg = 0: a constant, not an input.
                needs.discard(("reg", _reg_base(kops[0])))
                continue
            if kmn.startswith("SET"):
                # SETcc dst: dst's value is the flag result; resolve the
                # setter's own compare inputs instead of the flags.
                if not kops or ("reg", _reg_base(kops[0])) not in needs:
                    continue
                needs.discard(("reg", _reg_base(kops[0])))
                for kk in range(k - 1, max(k - 6, -1), -1):
                    if lines[kk][1] in ("CMP", "TEST"):
                        for sop in lines[kk][2][:2]:
                            if "[" in sop:
                                mk = _mem_key(sop)
                                if mk is not None and mk[0] != "rip":
                                    needs.add(("mem", mk))
                            else:
                                try:
                                    int(sop, 0)
                                except ValueError:
                                    needs.add(("reg", _reg_base(sop)))
                        break
                continue
            if kmn.startswith("CMOV"):
                # CMOVcc dst, src: dst keeps its old value or takes src's;
                # both are inputs to the result.
                if not kops or ("reg", _reg_base(kops[0])) not in needs:
                    continue
                if "[" in kops[-1]:
                    mk = _mem_key(kops[-1])
                    if mk is not None and mk[0] != "rip":
                        needs.add(("mem", mk))
                else:
                    try:
                        int(kops[-1], 0)
                    except ValueError:
                        needs.add(("reg", _reg_base(kops[-1])))
                continue
        # Resolve surviving needs: memory by offset match (offsets are
        # unique per spec; the slice loses the base reg), registers by
        # spec reg location (scalar_rdx -> rdx at entry; arg regs that a
        # MOV reads before any write are the entry values).
        reg_inputs = {}
        for iname, loc in (getattr(_trace_guard_deps, "_spec_inputs", None) or {}).items():
            if "reg" in loc:
                try:
                    reg_inputs[_reg_base(loc["reg"])] = iname
                except (ValueError, AttributeError):
                    pass
        deps = set()
        for nd in needs:
            if nd[0] == "mem":
                disp = nd[1][3]
                for iname, (_, roff, _) in (in_addrs or {}).items():
                    if disp == roff:
                        deps.add(iname)
            else:
                if nd[1] in reg_inputs:
                    deps.add(reg_inputs[nd[1]])
        out[addr] = sorted(deps)
    return out

def _rand_case(schema, rnd):
    """One random case from the schema (the main loop's draw, factored)."""
    case = {}
    for name, rule in schema.items():
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
            if rnd.random() < 0.1:
                blob = bytearray(struct.pack("<f", 0.0))
                blob[0] = 1
                case[name] = {"bits": "0x%08X" % struct.unpack("<I", bytes(blob))[0]}
            else:
                case[name] = f32_bits(0.0)
        elif kind == "choice":
            case[name] = rnd.choice(rule["values"])
        elif kind == "derived":
            case[name] = eval(rule["expr"], {"__builtins__": {}}, dict(case))
    return case


def guided_cases(spec, seed, count, image_path, want_addrs):
    """Deterministic extra cases steering uncovered branches both ways.

    For each unreached block, find the branch guarding it and the spec
    input it depends on (backward slice to region+offset words). Solve
    simple integer compares directly (taken + fallthrough values);
    otherwise mutate a seeded random case at the dependent input. Fixed
    seed: the same cases on every run.
    """
    import hashlib as _hl
    import random
    _gseed = int(_hl.sha256(repr(("guided", seed)).encode()).hexdigest(), 16) % (2**31)
    schema = spec.get("input_schema") or {}
    locs = spec.get("inputs") or {}
    try:
        guards = _decode_guards(image_path, spec["va"])
    except (OSError, ValueError):
        return []
    try:
        _trace_guard_deps._spec_inputs = spec.get("inputs") or {}
        deps = _trace_guard_deps(image_path, spec["va"], _input_addrs(spec))
    except (OSError, ValueError):
        deps = {}
    finally:
        _trace_guard_deps._spec_inputs = None
    if not guards or not want_addrs:
        return []
    want = {int(w, 16) if isinstance(w, str) else w for w in want_addrs}
    # Setter immediates: nearest CMP/TEST above each branch.
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    start = int(spec["va"], 16)
    with open(image_path, "rb") as imgf:
        imgf.seek(start - 0x140000000)
        code = imgf.read(0x10000)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    dlines = [(i.address, i.mnemonic.upper(), _split_ops(i.op_str))
              for i in md.disasm(bytes(code), start)]
    bya = {a: n for n, (a, _, _) in enumerate(dlines)}
    out = []
    base_rnd = random.Random(_gseed)
    seen_vals = set()
    for (baddr, tgt, fallthru, mn) in guards:
        if tgt not in want and fallthru not in want:
            continue
        names = deps.get(baddr, [])
        if mn == "JMP" or not names:
            continue  # unsteerable statically: the mutation pass covers it
        rel = _JCC_SIMPLE.get(mn)
        if rel is None:
            continue
        sop = None
        sop_mn = None
        if baddr in bya:
            for k in range(bya[baddr] - 1, max(bya[baddr] - 6, -1), -1):
                if dlines[k][1] in ("CMP", "TEST", "UCOMISS", "COMISS"):
                    sop = dlines[k][2]
                    sop_mn = dlines[k][1]
                    break
        if not sop:
            continue
        if sop_mn in ("UCOMISS", "COMISS"):
            # Float compare: the mem half is the input word; the xmm
            # half is case-fed or spilled. Steer the mem word to domain
            # edges and ±1.0 around the xmm half's drawn value.
            for half in sop[:2]:
                if "[" not in half:
                    continue
                mk = _mem_key(half)
                full = {v[1]: n for n, v in _input_addrs(spec).items()}
                hn = full.get(mk[3]) if mk else None
                if hn is None or schema.get(hn, {}).get("kind") != "f32":
                    continue
                lo = schema.get(hn, {}).get("lo", -50.0)
                hi = schema.get(hn, {}).get("hi", 50.0)
                for tag, fv in (("lo", lo), ("hi", hi), ("zero", 0.0),
                                ("one", 1.0), ("neg", -1.0)):
                    key = (hn, tag)
                    if key in seen_vals:
                        continue
                    seen_vals.add(key)
                    case = _rand_case(schema, base_rnd)
                    case[hn] = f32_bits(fv)
                    out.append(case)
                    if len(out) >= count:
                        return out
            continue
        # Solve the setter for each side: the imm half gets imm+deltas,
        # each input half gets domain edges; cases set ALL halves so
        # multi-input compares (eax==[rsi+8]) can actually flip.
        full_by_off = {}
        for iname, _loc in _input_addrs(spec).items():
            full_by_off.setdefault(_loc[1], iname)
        in_by_off = {off: nm for off, nm in full_by_off.items()
                     if nm in names}
        plan = {}
        skip = False
        for half in sop[:2]:
            if "[" in half:
                mk = _mem_key(half)
                hn = in_by_off.get(mk[3]) if mk else None
                if hn is None:
                    continue
                hrule = schema.get(hn, {})
                if hrule.get("kind") not in ("u8", "all_u8", "i32"):
                    skip = True
                    break
                plan[hn] = [hrule.get("lo", 0), hrule.get("hi", 255), 0, 1]
            else:
                try:
                    int(half, 0)
                except ValueError:
                    if names:
                        hrule = schema.get(names[0], {})
                        if hrule.get("kind") not in ("u8", "all_u8", "i32"):
                            skip = True
                            break
                        plan[names[0]] = [hrule.get("lo", 0),
                                          hrule.get("hi", 255), 0, 1]
        if skip or not plan:
            continue
        # Path condition: reaching this branch may require an EARLIER
        # branch's equality to hold (fallthrough into d8 needs d6's
        # eax==[rsi+8]). Constrain the earlier compare's input halves
        # to their equal values too.
        if fallthru is not None:
            for (pb, pt, pf, pm) in guards:
                if pb == baddr:
                    continue
                # Earlier branch enters baddr's block: its taken target is
                # baddr's block start, or its fallthrough block contains
                # baddr (fallthrough leader <= baddr < branch target).
                enters = (pt == baddr and pm == "JMP") or \
                    (pf is not None and pf <= baddr < pt)
                if enters:
                    psop = None
                    if pb in bya:
                        for k in range(bya[pb] - 1, max(bya[pb] - 6, -1), -1):
                            if dlines[k][1] in ('CMP', 'TEST'):
                                psop = dlines[k][2]
                                break
                    if psop:
                        for half in psop[:2]:
                            if '[' in half:
                                mk = _mem_key(half)
                                hn = full_by_off.get(mk[3]) if mk else None
                                if hn and hn not in plan:
                                    hrule = schema.get(hn, {})
                                    if hrule.get('kind') in ('u8', 'all_u8', 'i32'):
                                        plan[hn] = [0, 1]
                            else:
                                try:
                                    int(half, 0)
                                except ValueError:
                                    pass
        import itertools as _it
        keys = sorted(plan)
        vlist = []
        for k in keys:
            lo, hi = schema.get(k, {}).get("lo", 0), schema.get(k, {}).get("hi", 255)
            vlist.append(sorted({v for v in plan[k] if lo <= v <= hi})[:4])
        # Equalize: when both halves are the same input, only equal pairs
        # can take the fallthrough; when two inputs share a domain, try
        # equal pairs too (eax==[rsi+8] needs A0_8 in {0,1}).
        combos = list(_it.product(*vlist))[:8]
        if len(keys) == 2:
            lo0, hi0 = schema.get(keys[0], {}).get("lo", 0), schema.get(keys[0], {}).get("hi", 255)
            lo1, hi1 = schema.get(keys[1], {}).get("lo", 0), schema.get(keys[1], {}).get("hi", 255)
            shared = sorted(set(vlist[0]) & set(vlist[1]))
            shared = [v for v in shared if lo0 <= v <= hi0 and lo1 <= v <= hi1][:4]
            # Equal pairs first: they are the only ones that satisfy an
            # equality fallthrough (eax==[rsi+8]).
            eq = [(v, v) for v in shared if (v, v) not in combos][:4]
            combos = eq + [c for c in combos if c not in eq][:8 - len(eq)]
        for combo in combos[:8]:
            key = tuple(zip(keys, combo))
            if key in seen_vals:
                continue
            seen_vals.add(key)
            case = _rand_case(schema, base_rnd)
            for kk, vv in zip(keys, combo):
                case[kk] = vv
            out.append(case)
            if len(out) >= count:
                return out
    # Mutation pass: edge values at each dependent input for guards the
    # solver skipped (floats, TEST bits, call-fed values). Guards with no
    # traced deps mutate every schema input in turn (round-robin): the
    # frontier is data-dependent, but edges still flip hidden switches.
    mut_rnd = random.Random(_gseed + 1)
    mut_names = [n for n, rl in schema.items()
                 if rl.get("kind") in ("u8", "all_u8", "f32", "choice", "i32")]
    mut_idx = 0
    for (baddr, tgt, fallthru, mn) in guards:
        if tgt not in want and fallthru not in want:
            continue
        got = deps.get(baddr, [])[:1]
        if not got and mut_names:
            got = [mut_names[mut_idx % len(mut_names)]]
            mut_idx += 1
        for name in got:
            rule = schema.get(name, {})
            kind = rule.get("kind", "u8")
            if kind in ("u8", "all_u8"):
                edges = [rule.get("lo", 0), rule.get("hi", 255)]
            elif kind == "f32":
                edges = [f32_bits(rule.get("lo", -50.0)),
                         f32_bits(rule.get("hi", 50.0)),
                         f32_bits(0.0), f32_bits(1.0), f32_bits(-1.0)]
            elif kind == "choice":
                edges = list(rule.get("values", []))[:4]
            elif kind == "i32":
                edges = [rule.get("lo", -100), rule.get("hi", 100), 0, 1, -1]
            else:
                continue
            for v in edges[:4]:
                if (name, repr(v)) in seen_vals:
                    continue
                seen_vals.add((name, repr(v)))
                case = _rand_case(schema, base_rnd)
                case[name] = v
                # Second draw, so the mutation varies the background too.
                mut_rnd.random()
                out.append(case)
                if len(out) >= count:
                    return out
    return out

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
        cases.append(_rand_case(schema, rnd))
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
    ap.add_argument("--exe", default=None)
    ap.add_argument("--spec", required=True)
    ap.add_argument("--out")
    ap.add_argument("--check-spec", action="store_true")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--seed-state", default=None,
                    help="dir of per-spec seed artifacts (seed.py: "
                         "<name>.seed.json): real object bytes filling the "
                         "spec regions before the per-case inputs")
    ap.add_argument("--seed-bank", default=None,
                    help="dir of per-spec instance banks (bankstore.py: "
                         "<name>.bank.jsonl): case n runs instance n %% N with its real values")
    ap.add_argument("--perturb", action="store_true",
                    help="with --seed-bank: per (case, field) keep the real value, or take a "
                         "nearby one, the same field of another instance, or the spec's draw")
    ap.add_argument("--count", type=int, default=300)
    ap.add_argument("--nonreturn-cap", type=int, default=NONRETURN_CAP,
                    help="non-returning cases after which a function generates no further cases (0: no cap, measurement only)")
    ap.add_argument("--traced-cases", action="store_true",
                    help="with --seed-bank: every instance is one case (the traced calls), replayed "
                         "with its real values; no random or guided cases")
    ap.add_argument("--aborts", default=None,
                    help="write each case that reaches an abort trap (a PES abort) as a JSON line")
    ap.add_argument("--trace-calls", default=None,
                    help="write {site: {target: count}} for harness-reached "
                    "indirect calls/jumps (coverage of the dispatch table)")
    ap.add_argument("--trace-addrs", default=None,
                    help="write the sorted list of executed instruction "
                    "addresses (union over cases) for block coverage")
    ap.add_argument("--snapshot-dir", default=None,
                    help="replay the traced calls of --calls over this snapshot (snapcase.py): memory "
                         "is the snapshot's own, copied on first touch; outputs go to --out-dir")
    ap.add_argument("--calls", default=None, help="a calls file (frame_trace.py calls/<VA>.bin)")
    ap.add_argument("--out-dir", default=None, help="with --snapshot-dir: the output directory")
    ap.add_argument("--blocked", default=None,
                    help="with --snapshot-dir: a JSON list of untranslated callee VAs; a case that enters one is excluded")
    ap.add_argument("--hooks", default=None,
                    help="with --snapshot-dir: an rv.dll config; each hook entry replays as the code it displaced")
    args = ap.parse_args()
    if args.snapshot_dir:
        if not (args.calls and args.out_dir):
            ap.error("--snapshot-dir needs --calls and --out-dir")
        import snapcase
        import frame_trace as ft
        blocked = json.load(open(args.blocked)) if args.blocked else []
        snapcase.run_dir(args.spec, args.snapshot_dir, args.calls, args.out_dir, blocked,
                         restore=ft.hook_bytes(args.hooks) if args.hooks else None)
        return 0
    if args.exe is None:
        ap.error("--exe is required unless --snapshot-dir")
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
    mu._addrs_on = args.trace_addrs is not None
    install_hooks(mu, spec, RET)
    seed = args.seed if args.seed != 1 or "seed" not in spec else spec["seed"]
    count = args.count if args.count != 300 or "count" not in spec else spec["count"]
    cases = gen_inputs(spec, seed, count, (sections, args.exe))
    seed_art = None
    if args.seed_state:
        _sp = os.path.join(args.seed_state,
                           os.path.basename(args.spec)[:-5] + ".seed.json")
        if os.path.exists(_sp):
            seed_art = json.load(open(_sp))
        else:
            print("seed-state: no %s (continuing unseeded)" % os.path.basename(_sp),
                  file=sys.stderr)
    bank = None
    if args.seed_bank:
        _bk = os.path.join(args.seed_bank, os.path.basename(args.spec)[:-5] + ".bank.jsonl")
        if not os.path.exists(_bk):
            sys.exit("seed-bank: no %s" % _bk)  # a missing bank must not run unseeded
        import bankstore  # streamed: one instance is read per case, never the whole bank
        bank = bankstore.Bank(_bk)
    traced = args.traced_cases
    if traced:
        if bank is None:
            sys.exit("--traced-cases needs --seed-bank")
        base = gen_inputs(spec, seed, 1, (sections, args.exe))[0]
        cases = [dict(base) for _ in range(len(bank))]
        count = len(bank)

    def run_n(n, inputs):
        """Case n -> (inputs used, result, error). Bank mode: instance n % N's real values."""
        if bank is None:
            result, err = run_case(mu, spec, RET, inputs, seed=seed_art)
            return inputs, result, err
        used, art = bank_case(spec, bank, n, inputs, seed, args.perturb)
        if traced:
            unheld = unheld_inputs(spec, art, used)
            if unheld:
                return used, None, "unheld input %s: instance %d does not hold it" % (
                    unheld[0], n % len(bank))
        result, err = run_case(mu, spec, RET, used, seed=art, zero=True)
        return used, result, err

    def record(n, inputs, result):
        rec = {"va": spec["va"], "in": inputs, "out": result}
        if bank is not None:
            rec["inst"] = n % len(bank)
        return json.dumps(rec) + "\n"
    aborts_out = open(args.aborts, "w") if args.aborts else None

    # Every skipped case (non-returning or failed) contributes nothing: run_case rolled its coverage and call traces back.
    # After the cap's last non-returning case the function generates no further cases; the count of those is kept.
    stats = {"nonreturn": 0, "failed": 0, "capped": 0}
    skipped = []
    valid_icounts = []

    def run_round(planned, index0, out):
        for i, planned_inputs in enumerate(planned):
            if args.nonreturn_cap and stats["nonreturn"] >= args.nonreturn_cap:
                stats["capped"] += len(planned) - i
                return
            n = index0 + i
            used, result, err = run_n(n, planned_inputs)
            if err is not None:
                kind = "nonreturn" if err == NONRETURN else "failed"
                stats[kind] += 1
                skipped.append({"kind": kind, "index": n, "error": err, "inputs": used})
                print("case %d %s: %s" % (n, kind, err), file=sys.stderr)
                if err.startswith("abort trap ") and aborts_out is not None:
                    aborts_out.write(json.dumps({"inst": n % len(bank) if bank else n, "abort": err}) + "\n")
                continue
            valid_icounts.append(mu._last_icount)
            out.write(record(n, used, result))
    # Coverage-guided second round: base cases run first (streamed, so a
    # corpus is never held twice); unreached block leaders then steer
    # guided extras (same seed, appended, deterministic).
    guided_on = image_kind(args.exe) == "flat" and count and args.out and not args.traced_cases
    if guided_on:
        import sys as _sys
        import os as _os
        _sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                                          "..", "translate"))
        import xlate as _x
        _lines = _x.capstone_decode(args.exe, int(spec["va"], 16))
        _addrs = [a for a, _, _ in _lines]
        _aset = set(_addrs)
        _leaders = {_addrs[0]} if _addrs else set()
        for _i, (_a, _mn, _ops) in enumerate(_lines):
            _m = _mn.upper()
            _t = None
            if (_m in _x.CCMAP or _m == "JMP") and _ops:
                try:
                    _t = int(_ops[0].strip(), 16)
                except ValueError:
                    _t = None
            if _t is not None and _t in _aset:
                _leaders.add(_t)
            if _m in _x.CCMAP or _m in ("JMP", "CALL", "RET"):
                if _i + 1 < len(_addrs):
                    _leaders.add(_addrs[_i + 1])
        _bounds = sorted(_leaders)

        def _block_of(_addr):
            for _bi, _b in enumerate(_bounds):
                _end = _bounds[_bi + 1] if _bi + 1 < len(_bounds) else None
                if _addr >= _b and (_end is None or _addr < _end):
                    return _bi
            return None
        _pre = set(mu._addrs or set()) if mu._addrs is not None else set()
        out = open(args.out, "w")
        run_round(cases, 0, out)
        _hit = {_block_of(a) for a in ((mu._addrs or set()) - _pre)
                if _block_of(a) is not None}
        _want = {_b for _bi, _b in enumerate(_bounds) if _bi not in _hit}
        _guided = guided_cases(spec, seed, count, args.exe, _want)
        run_round(_guided[:count], len(cases), out)
        cases = cases + _guided[:count]
        out.close()
        if aborts_out is not None:
            aborts_out.close()
        if args.trace_calls is not None:
            with open(args.trace_calls, "w") as tf:
                json.dump(mu._trace or {}, tf, indent=1, sort_keys=True)
        if args.trace_addrs is not None:
            with open(args.trace_addrs, "w") as tf:
                json.dump(sorted("0x%X" % a for a in (mu._addrs or set())), tf)
        write_meta(args.out, count, stats, skipped, valid_icounts)
        return 0
    out = open(args.out, "w") if args.out else sys.stdout
    run_round(cases, 0, out)
    if args.out:
        out.close()
    if aborts_out is not None:
        aborts_out.close()
    if args.trace_calls is not None:
        with open(args.trace_calls, "w") as tf:
            json.dump(mu._trace or {}, tf, indent=1, sort_keys=True)
    if args.trace_addrs is not None:
        with open(args.trace_addrs, "w") as tf:
            json.dump(sorted("0x%X" % a for a in (mu._addrs or set())), tf)
    write_meta(args.out, count, stats, skipped, valid_icounts)
    return 0


if __name__ == "__main__":
    sys.exit(main())
