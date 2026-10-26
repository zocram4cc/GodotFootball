#!/usr/bin/env python3
"""Frame trace (FrameTrace): run PES's match step from one full-frame capture and trace the
impure calls as real cases.

Loads a capture_frame.py capture into unicorn at its live VAs over the flat image, then runs
the match-step owner H's step FUN_14041ECF0 (H: vtable 0x142597580, slot 2; H = *(0x1436F3F98))
once per frame. The step calls the team tick FUN_1404F6C30 for each side. Callees run the
image bytes, with stubs.json's models stubbed (unmodellable entries are not stubbed: their bytes
run). At each entry of the 50 impure functions, the spec's argument registers are the roots, and
seed.build_live_seed + patch_live_ptrs snapshot the regions the spec declares from the emulated
memory at that instant. Reservoir sampling keeps --cap instances per spec. Output: <VA>.bank.json
per reached spec (the driver's --seed-bank format) and record.json.

Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> python3 frame_trace.py --capture DIR --out DIR
       [--frames N] [--cap N] [--timeout S]
"""
import argparse
import json
import os
import random
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DIFF = os.path.join(HERE, "..", "diffharness")
sys.path.insert(0, HERE)
sys.path.insert(0, DIFF)
import run as rn  # noqa: E402  (diffharness emulator core: maps, stubs, stack)
import seed  # noqa: E402
import stubs as stub_table  # noqa: E402

PAGE = 0x1000
H_GLOBAL = 0x1436F3F98      # H = *(H_GLOBAL), the team-tick owner
H_STEP = 0x14041ECF0        # H's vtable slot 2: per-frame step (team tick loop at 0x14041EDD7)
TEAM_TICK = 0x1404F6C30     # team tick, called by H once per side
MI_OFF, PER_OFF = 0x270, 0x1168
G_SIZE = 0x2E58
PER_EARLY_RETURN = 0xA      # H returns at once when MI+0x1168 == 0xa (pes-re-teamtick.md H.b)
REGS_RESET = ("rbx", "rbp", "rax", "r10", "r11", "r12", "r13", "r14", "r15")


def mem_reader(uc):
    """addr, n -> bytes of the emulated memory at that instant, or None when unmapped."""
    def read(addr, n):
        if not (0 < addr < (1 << 47)) or n <= 0:
            return None
        try:
            return bytes(uc.mem_read(addr, n))
        except Exception:
            return None
    return read


def map_capture(uc, cap_dir):
    """Map every mapping of a capture_frame.py capture at its live VA. Pages the image does not
    cover get fresh mappings; image pages keep the dump's mapping and take the live bytes."""
    meta = json.load(open(os.path.join(cap_dir, "live.json")))
    items = [(m["va"], os.path.join(cap_dir, m["file_out"])) for m in meta["mappings"]]
    need = set()
    for va, path in items:
        lo = va & ~(PAGE - 1)
        hi = (va + os.path.getsize(path) + PAGE - 1) & ~(PAGE - 1)
        need.update(range(lo, hi, PAGE))
    fresh = []
    for p in sorted(need):
        try:
            uc.mem_read(p, 1)
        except Exception:
            fresh.append(p)
    runs = []
    for p in fresh:
        if runs and runs[-1][1] == p:
            runs[-1][1] = p + PAGE
        else:
            runs.append([p, p + PAGE])
    for lo, hi in runs:
        uc.mem_map(lo, hi - lo)
    for va, path in items:
        with open(path, "rb") as f:
            uc.mem_write(va, f.read())
    # G's own blob (g.bin: patched, between-step zeros filled) is written last: a neighbouring
    # span can cover G's range with a stale read, and merge keeps the first span's bytes.
    with open(os.path.join(cap_dir, "g.bin"), "rb") as f:
        uc.mem_write(meta["g"], f.read())
    return meta, {"pages": len(need), "mapped_fresh": len(fresh), "runs": len(runs),
                  "mappings": len(items)}


def load_impure(spec_dir):
    """{tag: spec} for every impure spec; tag is the VA without its 0x prefix (driver convention)."""
    specs = {}
    for f in sorted(os.listdir(spec_dir)):
        if f.endswith(".json"):
            s = json.load(open(os.path.join(spec_dir, f)))
            specs[s["va"][2:]] = s
    return specs


def stubs_for_run(image_root_stubs):
    """Every stubs.json model that is a stub (not run-real, not unmodellable), keyed like
    resolve_stubs. Unmodellable entries are left out so their image bytes run."""
    spec = {"stubs": {}}
    for va, e in image_root_stubs.items():
        if e["do"] in (None, "unmodellable"):
            continue
        spec["stubs"]["stub_%X" % va] = {"va": e["va"], "do": e["do"]}
    return spec


class Bank:
    """Reservoir of seed artifacts for one spec: at most cap instances, uniform over the calls."""

    def __init__(self, cap, tag):
        self.cap, self.n, self.items = cap, 0, []
        self.rng = random.Random("frametrace:" + tag)

    def offer(self, make):
        self.n += 1
        if len(self.items) < self.cap:
            self.items.append(make())
            return
        j = self.rng.randrange(self.n)
        if j < self.cap:
            self.items[j] = make()


def snapshot(uc, spec, roots):
    """The seed artifact of one call: the regions the spec declares, filled from emulated memory."""
    read = mem_reader(uc)
    g = struct.unpack("<Q", read(seed.GVA, 8) or bytes(8))[0]
    gbytes = (read(g, G_SIZE) or b"") if g else b""
    meta = {"g": g}
    regions, live = seed.build_live_seed(spec, meta, read, gbytes, roots)
    seed.patch_live_ptrs(spec, regions, live, meta, read)
    return seed.artifact_of(spec, regions)


def run_entry(uc, entry, regs, ret, timeout_ms):
    """One call of the function at entry under the Windows x64 convention, to the ret sentinel."""
    from unicorn.x86_const import UC_X86_REG_RSP
    rsp = rn.stack_top({}) - 8
    uc.mem_write(rsp, struct.pack("<Q", ret))
    uc.reg_write(UC_X86_REG_RSP, rsp)
    import unicorn.x86_const as xc
    for reg in REGS_RESET:
        uc.reg_write(getattr(xc, "UC_X86_REG_" + reg.upper()), 0)
    for reg, val in regs.items():
        uc.reg_write(rn.reg_const(reg), val)
    uc._bad = None
    err = None
    try:
        uc.emu_start(entry, ret, timeout=timeout_ms * 1000)
    except Exception as e:  # unicorn raises on a fault; the hooks record the reason
        err = "emu error: %s (last RIP %#x)" % (e, getattr(uc, "_last_rip", 0))
    if uc._bad is not None:
        err = uc._bad
    return err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--spec-dir", default=os.path.join(DIFF, "specs", "impure"))
    ap.add_argument("--frames", type=int, default=50)
    ap.add_argument("--cap", type=int, default=32, help="instances kept per spec (reservoir)")
    ap.add_argument("--timeout", type=int, default=120, help="seconds per step")
    ap.add_argument("--trace-addrs", action="store_true",
                    help="record every executed instruction address (diagnostic; slower)")
    a = ap.parse_args()
    if not a.image:
        ap.error("--image or PES_EXE")
    os.makedirs(os.path.join(a.out, "banks"), exist_ok=True)
    t0 = time.time()
    sections = rn.flat_sections(a.image)
    if rn.image_kind(a.image) != "flat":
        ap.error("the frame trace needs the flat unpacked image (pes_full.bin)")
    uc, ret = rn.build_emu({"regions": {}}, a.image, sections)
    meta, mapped = map_capture(uc, a.capture)
    t_map = time.time() - t0

    table = stub_table.load()
    spec_stubs = stubs_for_run(table)
    uc._addrs_on = a.trace_addrs
    rn.install_hooks(uc, spec_stubs, ret)
    stubbed = len(spec_stubs["stubs"])

    from unicorn import UC_HOOK_CODE
    impure = load_impure(a.spec_dir)
    banks, calls, roots = {}, {}, {}
    frame_log = []
    counters = {"tick": 0, "per_early": 0}

    def tick_hook(uc_, address, size, data):
        counters["tick"] += 1

    uc.hook_add(UC_HOOK_CODE, tick_hook, None, begin=TEAM_TICK, end=TEAM_TICK)

    for tag, spec in impure.items():
        va = int(spec["va"], 16)
        args = {reg: root for reg, root in (spec.get("args") or {}).items()}
        roots[tag] = {root: "frametrace" for root in args.values()}
        banks[tag] = Bank(a.cap, tag)
        calls[tag] = 0

        def on_entry(uc_, address, size, data, tag=tag, spec=spec, args=args):
            calls[tag] += 1
            regs = {reg: uc_.reg_read(rn.reg_const(reg)) for reg in args}
            names = {reg: root for reg, root in args.items()}
            call_roots = {names[r]: v for r, v in regs.items()}
            banks[tag].offer(lambda: snapshot(uc_, spec, call_roots))

        uc.hook_add(UC_HOOK_CODE, on_entry, None, begin=va, end=va)

    g_ptr = struct.unpack("<Q", bytes(uc.mem_read(H_GLOBAL, 8)))[0]
    h_obj = g_ptr
    stop = None
    for f in range(a.frames):
        read = mem_reader(uc)
        gobj = struct.unpack("<Q", read(seed.GVA, 8) or bytes(8))[0]
        mi_raw = read(gobj + MI_OFF, 8) if gobj else None
        mi = struct.unpack("<Q", mi_raw)[0] if mi_raw else 0
        per_raw = read(mi + PER_OFF, 4) if mi else None
        per = struct.unpack("<i", per_raw)[0] if per_raw else None
        if per == PER_EARLY_RETURN:
            counters["per_early"] += 1
        tick0, calls0 = counters["tick"], sum(calls.values())
        t_s = time.time()
        err = run_entry(uc, H_STEP, {"rcx": h_obj}, ret, a.timeout)
        frame_log.append({"frame": f, "H": hex(h_obj), "MI": hex(mi), "per": per,
                          "ticks": counters["tick"] - tick0,
                          "impure_calls": sum(calls.values()) - calls0,
                          "secs": round(time.time() - t_s, 3), "err": err})
        if err:
            stop = {"frame": f, "err": err, "last_rip": hex(getattr(uc, "_last_rip", 0))}
            break

    banks_written = []
    for tag, bank in banks.items():
        if bank.items:
            out = os.path.join(a.out, "banks", tag + ".bank.json")
            json.dump({"roots": roots[tag], "instances": bank.items}, open(out, "w"))
            banks_written.append(tag)
    record = {"capture": a.capture, "label": os.path.basename(a.capture.rstrip("/")),
              "meta": {k: meta.get(k) for k in ("g", "mi", "teams", "h", "m", "ma", "frame_delta")},
              "mapped": mapped, "stubbed_callees": stubbed, "map_secs": round(t_map, 2),
              "frames_requested": a.frames, "frames_ran": len(frame_log),
              "ticks_entered": counters["tick"], "per10_early_returns": counters["per_early"],
              "stop": stop, "impure_calls": calls,
              "impure_reached": sorted(t for t, n in calls.items() if n),
              "instances": {t: banks[t].n for t in banks if banks[t].n},
              "banks_written": banks_written, "frames": frame_log,
              "secs_total": round(time.time() - t0, 2)}
    if a.trace_addrs and getattr(uc, "_addrs", None) is not None:
        with open(os.path.join(a.out, "addrs.txt"), "w") as f:
            f.write("\n".join("%X" % x for x in sorted(uc._addrs)) + "\n")
    json.dump(record, open(os.path.join(a.out, "record.json"), "w"), indent=1)
    print("frames_ran=%d ticks=%d reached=%d/%d stop=%s" % (
        len(frame_log), counters["tick"], len(record["impure_reached"]), len(impure),
        stop), flush=True)


if __name__ == "__main__":
    sys.exit(main())
