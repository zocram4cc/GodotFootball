#!/usr/bin/env python3
"""synth.py: SYNTHETIC states of one captured call, written in the rendezvous layout that frame_trace.py recorded and
snapdiff.py --bounded replay (index.json, call.bin, mem.bin), one state directory per case. Every state's index.json says
SYNTHETIC: the chosen values are the case; the program that ran it is PES's own bytes.

Source: a full rendezvous capture (unpack it first with rendezvous/capz.py): index.json (rv, mappings), call.bin (the entry
record), mem.bin. The spec (JSON, see SPEC_KEYS) names the fields to perturb and the values they take:

  "seed": 7                       required; the same spec and seed write the same bytes
  "count": 64 | "cases": [...]    random cases (count) or explicit ones; a case may set any field, and "name" labels its state
  "fields": [ ... ]               each: "name", "at", "type", and "values" (pick one) or "range" [lo, hi] (draw; floats uniform,
                                  ints inclusive); optional "plus": {"at", "type"} adds the drawn value to the word read there
  "entry": "0x140833CD0"          perturb at this function's first entry in the run (default: the capture's own entry state)
  "stop": "0x140A510F0"           write the state at the first arrival here after the perturbation (default: the entry)
  "reach": "0x140A510F0"          keep a case only if this VA is entered after the perturbation (a filter, e.g. "the wide-pass gate
                                  opened"); cases that never reach "stop" or "reach" are dropped and counted
  "start": "call" | "return"      in-call: run the capture's own call from its entry (default); "return": run the call to its return
                                  and continue in the caller until "entry" (a state the caller reaches after the call)
  "hook": "carrier_walk_210F110"  rv.hook of the written state (default: the capture's, or synthetic_<stop> when stop moves)
  "budget": 20000000              instructions per emulated run
  "as": "0x14212FE70"             a static spec writes the states as calls of this function (rv.entry_va), over the capture's memory

A field's "at" is an address or a register:
  "reg:rsp+0x28"                 rsp + 0x28
  "mem:0x1436F3DC8+0x40+0x88"    the word at 0x1436F3DC8, then the word at (that + 0x40), then that + 0x88
  "va:0x20100"                   a constant address
  "rcx" / "xmm3"                 a register (scalar types only; an xmm writes its low bytes, the rest zero)
Every offset but the last dereferences: the value becomes the 8-byte word at value+offset. Register fields are written after all
addresses are read, so a field never moves another field's address.

A static spec (no entry/stop/reach, or entry = the capture's entry) writes the states from the capture's memory with no run. Any
other spec runs PES's bytes (unicorn, the harness Python) for each case, with the rv.dll displaced bytes from --hooks, and
captures the state where the run reaches "stop". Emulated states hold every byte the run wrote into the capture's mappings.

Usage: python3 synth.py SRC_CAPTURE OUT_DIR --spec SPEC.json [--hooks HOOKS.cfg]
  Emulated specs need --hooks and the harness Python (unicorn). Output: one line of JSON (written states, dropped cases).
"""
import argparse
import json
import os
import random
import re
import shutil
import struct
import math
import subprocess
import sys

GPR_ORDER = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
REG32 = {"e" + g[1:]: g for g in GPR_ORDER[:8]}  # 32-bit names of the 64-bit registers: a write zero-extends
REG32.update({"r%dd" % n: "r%d" % n for n in range(8, 16)})
CALL_REC = struct.Struct("<49Q")  # frame_trace's call record: 16 GPRs, 16 XMM (lo, hi), the return address at [rsp]
XMM_SLOT = 16                     # the call record's slot of xmm0's low qword
RET_SLOT = 48
U64 = (1 << 64) - 1
XMM_BYTES = 16
GPR_BYTES = 8
RET_SENTINEL = 0xDEAD0000         # the forward run continues past the recorded call's return address
DEFAULT_BUDGET = 20_000_000       # instructions per emulated run (the carrier-walk runs' budget)
SCALAR = {"u8": "<B", "u16": "<H", "u32": "<I", "i32": "<i", "u64": "<Q", "f32": "<f", "f64": "<d"}
FLOAT_TYPES = ("f32", "f64")
TYPES = tuple(SCALAR) + ("hex",)
SPEC_KEYS = {"label", "seed", "count", "cases", "fields", "entry", "stop", "reach", "start", "hook", "budget", "as"}
FIELD_KEYS = {"name", "at", "type", "values", "range", "plus"}
PLUS_KEYS = {"at", "type"}
CASE_NAME_RE = re.compile(r"[A-Za-z0-9_.-]+")
XMM_RE = re.compile(r"xmm(1[0-5]|[0-9])")

# patch_imports (forward and in-call runs): the float libm and the lock calls the replay harness has no model for
F32_MASK = 0xFFFFFFFF
F32_EXP_SHIFT = 23
F64_EXP_SHIFT = 52
EXP_ALL_ONES = 0x7FF
F32_POS_INF = 0x7F800000
F32_NEG_INF = 0xFF800000
LIBM1 = {"sinf": math.sin, "cosf": math.cos, "tanf": math.tan, "atanf": math.atan, "acosf": math.acos, "asinf": math.asin,
         "logf": math.log, "expf": math.exp, "floorf": math.floor, "ceilf": math.ceil}
LIBM2 = {"atan2f": math.atan2, "powf": math.pow, "fmodf": math.fmod}
NOOP1 = ("SetEvent", "ResetEvent", "EnterCriticalSection", "LeaveCriticalSection")  # return TRUE; the replay is one thread
ZERO1 = ("_crt_atexit", "atexit", "_onexit", "__stdio_common_vsprintf_s")  # a function-local static's registration: return 0


def is_reg(at):
    return at in GPR_ORDER or at in REG32 or XMM_RE.fullmatch(at) is not None


def num(v, typ):
    """A spec value as a Python number of the field's kind (hex strings allowed for integers)."""
    if typ in FLOAT_TYPES:
        return float(v)
    return int(v, 0) if isinstance(v, str) else int(v)


def encode(typ, v):
    if typ == "hex":
        return bytes.fromhex(v)
    if typ in FLOAT_TYPES:
        return struct.pack(SCALAR[typ], float(v))
    size = struct.calcsize(SCALAR[typ])
    return (num(v, typ) & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def decode(typ, raw):
    return struct.unpack(SCALAR[typ], raw)[0]


def parse_at(expr):
    """'reg:rsp+0x10+0x20' -> ("reg", "rsp", [0x10, 0x20]); 'mem:0x1436F3DC8+0x40' -> ("mem", 0x1436F3DC8, [0x40]);
    'va:0x20100' -> ("va", 0x20100, [])."""
    toks = re.findall(r"[+-]?[^+-]+", expr.replace(" ", ""))
    kind, _, val = toks[0].partition(":")
    if kind not in ("reg", "mem", "va") or not val:
        raise SystemExit("synth: address %r must start with reg:, mem: or va:" % expr)
    if kind == "reg":
        if val not in GPR_ORDER:
            raise SystemExit("synth: %r is not a general register" % val)
        start = val
    else:
        start = int(val, 0)
    return kind, start, [int(t, 0) for t in toks[1:]]


def spans(index, va, n):
    """(file offset, offset in the request, length) for each mapping piece of [va, va+n) in the capture's mem.bin."""
    out, cur, end = [], va, va + n
    while cur < end:
        m = next((m for m in index["mappings"] if m["va"] <= cur < m["va"] + m["size"]), None)
        if m is None:
            raise SystemExit("synth: %#x is not inside the capture's mappings" % cur)
        hi = min(end, m["va"] + m["size"])
        out.append((m["off"] + cur - m["va"], cur - va, hi - cur))
        cur = hi
    return out


def resolve(view, expr):
    kind, start, offs = parse_at(expr)
    v = view.reg(start) if kind == "reg" else (view.u64(start) if kind == "mem" else start)
    for i, t in enumerate(offs):
        v = view.u64(v + t) if i + 1 < len(offs) else v + t
    return v & U64


def read_mem(f, index, va, n):
    """n bytes at va from the open capture mem.bin f (spans refuses an address outside the capture's mappings)."""
    buf = bytearray(n)
    for pos, rel, k in spans(index, va, n):
        f.seek(pos)
        buf[rel:rel + k] = f.read(k)
    return bytes(buf)


def read_capture(src, index, va, n):
    with open(os.path.join(src, "mem.bin"), "rb") as f:
        return read_mem(f, index, va, n)


class StaticView:
    """The capture as it stands: reads come from its mem.bin and call record; writes are kept for the state, never applied to
    the source."""

    def __init__(self, src, index, vals):
        self.index, self.vals, self.writes = index, list(vals), []
        self.f = open(os.path.join(src, "mem.bin"), "rb")

    def read(self, va, n):
        return read_mem(self.f, self.index, va, n)

    def u64(self, va):
        return decode("u64", self.read(va, 8))

    def reg(self, name):
        return self.vals[GPR_ORDER.index(name)]

    def write(self, va, data):
        spans(self.index, va, len(data))  # refuses an address outside the capture's mappings
        self.writes.append((va, data))

    def set_reg(self, name, data):
        name = REG32.get(name, name)
        if name in GPR_ORDER:
            self.vals[GPR_ORDER.index(name)] = int.from_bytes(data.ljust(GPR_BYTES, b"\0"), "little")
            return
        n = int(name[3:])
        raw = data.ljust(XMM_BYTES, b"\0")
        self.vals[XMM_SLOT + 2 * n] = int.from_bytes(raw[:8], "little")
        self.vals[XMM_SLOT + 2 * n + 1] = int.from_bytes(raw[8:], "little")


class EmuView:
    """A unicorn run of the capture (LazyMap pages): reads and writes the run's own memory and registers."""

    def __init__(self, uc, xc):
        self.u, self.xc = uc, xc

    def read(self, va, n):
        self.u._ensure(va, n)
        return bytes(self.u.mem_read(va, n))

    def u64(self, va):
        return decode("u64", self.read(va, 8))

    def reg(self, name):
        return self.u.reg_read(getattr(self.xc, "UC_X86_REG_" + name.upper()))

    def write(self, va, data):
        self.u._ensure(va, len(data))
        self.u.mem_write(va, data)

    def set_reg(self, name, data):
        name = REG32.get(name, name)
        if name in GPR_ORDER:
            self.u.reg_write(getattr(self.xc, "UC_X86_REG_" + name.upper()), int.from_bytes(data.ljust(GPR_BYTES, b"\0"), "little"))
        else:
            self.u.reg_write(self.xc.UC_X86_REG_XMM0 + int(name[3:]), data.ljust(XMM_BYTES, b"\0"))


def apply_fields(view, fields, vals):
    """Read every field's address (and each plus word) from the state as it stands, then write them all. Returns the record."""
    ops, record = [], {}
    for f in fields:
        typ, v = f["type"], vals[f["name"]]
        if "plus" in f:
            base = decode(f["plus"]["type"], view.read(resolve(view, f["plus"]["at"]), struct.calcsize(SCALAR[f["plus"]["type"]])))
            v = base + num(v, typ)
        data = encode(typ, v)
        if is_reg(f["at"]):
            ops.append(("reg", f["at"], data))
            where = f["at"]
        else:
            va = resolve(view, f["at"])
            ops.append(("mem", va, data))
            where = "%#x" % va
        record[f["name"]] = {"at": f["at"], "type": typ, "value": v, "address": where, "bytes": data.hex()}
    for kind, target, data in ops:
        if kind == "mem":
            view.write(target, data)
        else:
            view.set_reg(target, data)
    return record


def draw(rng, f):
    if "values" in f:
        return rng.choice(f["values"])
    lo, hi = f["range"]
    if f["type"] in FLOAT_TYPES:
        return rng.uniform(float(lo), float(hi))
    return rng.randint(num(lo, "u64"), num(hi, "u64"))


def case_values(rng, fields, case):
    """One case's value per field, in spec order: the explicit value if the case gives one, else a draw."""
    vals = {}
    for f in fields:
        if f["name"] in case:
            vals[f["name"]] = case[f["name"]]
        elif "values" in f or "range" in f:
            vals[f["name"]] = draw(rng, f)
        else:
            raise SystemExit("synth: case %r gives no value for field %r" % (case.get("name"), f["name"]))
    return vals


def load_spec(path):
    with open(path) as fh:
        spec = json.load(fh)
    bad = sorted(set(spec) - SPEC_KEYS)
    if bad:
        raise SystemExit("synth: unknown spec keys %s" % bad)
    if "seed" not in spec:
        raise SystemExit("synth: the spec needs a seed")
    if ("cases" in spec) == ("count" in spec):
        raise SystemExit("synth: give either cases or count")
    if spec.get("start", "call") not in ("call", "return"):
        raise SystemExit("synth: start must be call or return")
    names = set()
    for f in spec.get("fields", []):
        bad = sorted(set(f) - FIELD_KEYS)
        if bad or "name" not in f or "at" not in f or f.get("type") not in TYPES:
            raise SystemExit("synth: field %r needs name, at and a type of %s (keys %s)" % (f.get("name"), TYPES, bad))
        if f["name"] in names or f["name"] == "name":
            raise SystemExit("synth: field name %r repeats or is reserved" % f["name"])
        names.add(f["name"])
        if ("values" in f) + ("range" in f) > 1 or ("cases" not in spec and "values" not in f and "range" not in f):
            raise SystemExit("synth: field %r needs exactly one of values or range (or cases that give it)" % f["name"])
        if f["type"] == "hex" and "range" in f:
            raise SystemExit("synth: a hex field takes values, not a range")
        if is_reg(f["at"]) and (f["type"] == "hex" or "plus" in f):
            raise SystemExit("synth: a register field is a scalar with no plus")
        if "plus" in f and (f["type"] == "hex" or set(f["plus"]) != PLUS_KEYS or f["plus"]["type"] not in SCALAR):
            raise SystemExit("synth: plus needs a scalar field, and at and a scalar type")
    for c in spec.get("cases", []):
        bad = sorted(set(c) - names - {"name"})
        if bad:
            raise SystemExit("synth: case gives unknown fields %s" % bad)
    return spec


def load_frame_trace():
    import frame_trace as ft
    return ft


def patch_imports():
    """The replay harness stops at an unmodelled OS export; give the float libm and the lock calls these models (forward and
    in-call runs reach them). Installed once per process, on frame_trace.LazyMap."""
    import unicorn.x86_const as xc
    ft = load_frame_trace()
    real_intr = ft.LazyMap._intr

    def intr(self, uc, intno, data):
        rip = uc.reg_read(xc.UC_X86_REG_RIP)
        _, name, _ = self.sm.export_of(rip)
        if name == "_finite":  # int _finite(double): 1 unless the exponent field is all ones
            xmm0 = uc.reg_read(xc.UC_X86_REG_XMM0)
            bits = (int.from_bytes(xmm0, "little") if isinstance(xmm0, (bytes, bytearray)) else xmm0) & U64
            uc.reg_write(xc.UC_X86_REG_RAX, 0 if (bits >> F64_EXP_SHIFT) & EXP_ALL_ONES == EXP_ALL_ONES else 1)
            return_from_import(uc)
            return
        if name in LIBM1 or name in LIBM2 or name in NOOP1 or name in ZERO1:
            if name in NOOP1 or name in ZERO1:
                uc.reg_write(xc.UC_X86_REG_RAX, 1 if name in NOOP1 else 0)
            else:
                a, b = f32(xmm_low(uc, 0)), f32(xmm_low(uc, 1))
                try:
                    r = LIBM1[name](a) if name in LIBM1 else LIBM2[name](a, b)
                except (ValueError, ZeroDivisionError, OverflowError):
                    r = float("nan")
                uc.reg_write(xc.UC_X86_REG_XMM0, bits32(r))
            return_from_import(uc)
            return
        return real_intr(self, uc, intno, data)

    ft.LazyMap._intr = intr


def return_from_import(uc):
    import unicorn.x86_const as xc
    rsp = uc.reg_read(xc.UC_X86_REG_RSP)
    uc.reg_write(xc.UC_X86_REG_RIP, struct.unpack("<Q", bytes(uc.mem_read(rsp, 8)))[0])
    uc.reg_write(xc.UC_X86_REG_RSP, rsp + 8)


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits & F32_MASK))[0]


def bits32(f):
    try:
        return struct.unpack("<I", struct.pack("<f", f))[0]
    except OverflowError:
        return F32_POS_INF if f > 0 else F32_NEG_INF


def xmm_low(uc, n):
    import unicorn.x86_const as xc
    v = uc.reg_read(xc.UC_X86_REG_XMM0 + n)
    return (int.from_bytes(v, "little") if isinstance(v, (bytes, bytearray)) else v) & F32_MASK


def capture(u, xc, index):
    """The run's state now: the entry record (registers and return address), the flags, and every byte of the run's mapped
    memory that lies in the capture's mappings."""
    regs = [u.reg_read(getattr(xc, "UC_X86_REG_" + r.upper())) for r in GPR_ORDER]
    xmm = []
    for i in range(16):
        v = u.reg_read(xc.UC_X86_REG_XMM0 + i)
        v = int.from_bytes(v, "little") if isinstance(v, (bytes, bytearray)) else v
        xmm += [v & U64, v >> 64]
    ret = struct.unpack("<Q", bytes(u.mem_read(regs[GPR_ORDER.index("rsp")], 8)))[0]
    writes = []
    for begin, end, _ in u.mem_regions():  # end is inclusive
        stop = end + 1
        for m in index["mappings"]:
            lo, hi = max(begin, m["va"]), min(stop, m["va"] + m["size"])
            if lo < hi:
                writes.append((lo, bytes(u.mem_read(lo, hi - lo))))
    return {"call": CALL_REC.pack(*regs, *xmm, ret), "writes": writes,
            "flags": u.reg_read(xc.UC_X86_REG_EFLAGS), "mxcsr": u.reg_read(xc.UC_X86_REG_MXCSR)}


def trace_env(ft, out):
    """The environment a snapdiff trace runs in (import and stub models), with no impure spec recorded: the emulated
    replays run as the trace runs did (run_recorded installs the models and stubs only for a trace)."""
    import stubs as stub_table
    return {"out": out, "impure": {}, "stubs": ft.stubs_for_run(stub_table.load())}



def emulate_case(src, index, orig, budget, plan, fields, vals):
    """One run of the capture with the case's values written at plan["entry"]; the state is captured at plan["stop"] after
    that. Returns (state, record, reason): state None and a reason when the case is dropped."""
    import unicorn.x86_const as xc
    from unicorn import UC_HOOK_CODE
    ft = load_frame_trace()
    st = {"phase": 0, "state": None, "reached": False, "record": None}

    def on_code(u, address, size, data):
        if st["phase"] >= 1 and address == plan["reach"]:
            st["reached"] = True
        if st["phase"] == 0 and address == plan["entry"]:
            st["record"] = apply_fields(EmuView(u, xc), fields, vals)
            st["phase"] = 1
        if st["phase"] == 1 and address == plan["stop"]:
            st["state"] = capture(u, xc, index)
            st["phase"] = 2
        if st["phase"] == 2 and (plan["reach"] is None or st["reached"]):
            u.emu_stop()

    if plan["start"] == "call":
        _, err = ft.run_recorded(src, orig=orig, budget=budget, trace=trace_env(ft, plan["env"]), on_code=on_code)
    else:
        uc, err = ft.run_recorded(src, orig=orig, budget=budget, trace=trace_env(ft, plan["env"]))
        if err:
            raise SystemExit("synth: the capture's own call did not return: %s" % err)
        ret = plan["ret"]
        uc._ret_sentinel = RET_SENTINEL
        uc.hook_add(UC_HOOK_CODE, on_code)
        uc.emu_start(ret, 0, count=budget)
    if st["state"] is None:
        if err:
            sys.stderr.write("synth: the run stopped before the stop VA: %s\n" % err)
        return None, None, "stop not reached"
    if plan["reach"] is not None and not st["reached"]:
        if err:
            sys.stderr.write("synth: the run stopped before the reach VA: %s\n" % err)
        return None, None, "reach not entered"
    return st["state"], st["record"], None


def write_state(out, name, src, index, state, meta):
    d = os.path.join(out, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    src_mem = os.path.join(src, "mem.bin")
    dst_mem = os.path.join(d, "mem.bin")
    if state["writes"]:
        subprocess.check_call(["cp", "--sparse=always", src_mem, dst_mem])  # sparse: a hole is a page the capture never had
        with open(dst_mem, "r+b") as f:
            for va, data in state["writes"]:
                for pos, rel, k in spans(index, va, len(data)):
                    f.seek(pos)
                    f.write(data[rel:rel + k])
    else:
        os.symlink(os.path.realpath(src_mem), dst_mem)
    with open(os.path.join(d, "call.bin"), "wb") as f:
        f.write(state["call"])
    idx = json.loads(json.dumps(index))
    idx["label"] = name
    idx["rv"].update(entry_va=meta["state_entry"], flags=state["flags"], mxcsr=state["mxcsr"])
    idx["rv"]["hook"] = meta["hook"]
    idx["synthetic"] = dict(label="SYNTHETIC", source=os.path.basename(src.rstrip("/")), seed=meta["seed"], case=name,
                            mode=meta["mode"], entry="%#x" % meta["entry"], stop="%#x" % meta["stop"],
                            reach=None if meta["reach"] is None else "%#x" % meta["reach"], fields=meta["record"])
    with open(os.path.join(d, "index.json"), "w") as f:
        json.dump(idx, f, sort_keys=True)


def main():
    ap = argparse.ArgumentParser(description="Write SYNTHETIC states of a captured call (see the module docstring).")
    ap.add_argument("src", help="a full rendezvous capture (unpacked): index.json, call.bin, mem.bin")
    ap.add_argument("out", help="output directory: one state directory per case")
    ap.add_argument("--spec", required=True, help="the JSON spec")
    ap.add_argument("--hooks", default=None, help="rv.dll config: its displaced bytes replace the hook patches of emulated runs")
    a = ap.parse_args()
    spec = load_spec(a.spec)
    src = a.src.rstrip("/")
    with open(os.path.join(src, "index.json")) as f:
        index = json.load(f)
    with open(os.path.join(src, "call.bin"), "rb") as f:
        raw = f.read()
    if len(raw) != CALL_REC.size:
        raise SystemExit("synth: call.bin is %d bytes, want %d" % (len(raw), CALL_REC.size))
    base_vals = list(CALL_REC.unpack(raw))
    rv = index["rv"]
    source_entry = rv["entry_va"]
    entry = num(spec["entry"], "u64") if "entry" in spec else source_entry
    stop = num(spec["stop"], "u64") if "stop" in spec else entry
    reach = num(spec["reach"], "u64") if "reach" in spec else None
    start = spec.get("start", "call")
    static = entry == source_entry and stop == entry and reach is None and start == "call"
    if "as" in spec and not static:
        raise SystemExit("synth: as retargets a static state; an emulated state takes its entry from stop")
    state_entry = num(spec["as"], "u64") if "as" in spec else stop
    plan = {"entry": entry, "stop": stop, "reach": reach, "start": start, "ret": base_vals[RET_SLOT],
            "env": a.out.rstrip("/") + ".env"}
    label = spec.get("label") or os.path.basename(a.out.rstrip("/"))
    cases = spec.get("cases") or [{} for _ in range(spec.get("count", 1))]
    rng = random.Random(spec["seed"])
    orig = None
    if not static:
        if not a.hooks:
            raise SystemExit("synth: an emulated spec needs --hooks (the rv.dll config of the capture)")
        orig = load_frame_trace().hook_bytes(a.hooks)
        patch_imports()
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(plan["env"], exist_ok=True)
    written, dropped = [], {}
    for i, case in enumerate(cases):
        name = case.get("name") or "%s_%04d" % (label, i)
        if not CASE_NAME_RE.fullmatch(name):
            raise SystemExit("synth: case name %r is not a plain directory name" % name)
        vals = case_values(rng, spec["fields"], case) if "fields" in spec else {}
        if static:
            view = StaticView(src, index, base_vals)
            record = apply_fields(view, spec.get("fields", []), vals)
            view.f.close()
            state = {"call": CALL_REC.pack(*view.vals), "writes": view.writes, "flags": rv["flags"], "mxcsr": rv["mxcsr"]}
            reason = None
        else:
            state, record, reason = emulate_case(src, index, orig, spec.get("budget", DEFAULT_BUDGET), plan,
                                                 spec.get("fields", []), vals)
        if state is None:
            dropped[reason] = dropped.get(reason, 0) + 1
            continue
        hook = spec.get("hook") or (rv["hook"] if state_entry == source_entry else "synthetic_%X" % state_entry)
        meta = {"state_entry": state_entry, "stop": stop, "entry": entry, "reach": reach, "seed": spec["seed"], "hook": hook,
                "record": record, "mode": "static" if static else start}
        write_state(a.out, name, src, index, state, meta)
        written.append(name)
    shutil.rmtree(plan["env"], ignore_errors=True)
    print(json.dumps({"out": a.out, "written": written, "dropped": dropped}, sort_keys=True))


if __name__ == "__main__":
    main()
