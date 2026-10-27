#!/usr/bin/env python3
"""Frame trace (FrameTrace): run PES's match step from one full-frame capture and trace the
impure calls as real cases.

Loads a capture_frame.py capture into unicorn at its live VAs over the flat image, then runs
the match-step owner H's step FUN_14041ECF0 (H: vtable 0x142597580, slot 2; H = *(0x1436F3F98))
once per frame. The step calls the team tick FUN_1404F6C30 for each side. Callees run the
image bytes, with stubs.json's models stubbed (unmodellable entries are not stubbed: their bytes
run). At each entry of the 50 impure functions, the spec's argument registers are the roots, and
seed.build_live_seed + patch_live_ptrs snapshot the regions the spec declares from the emulated
memory at that instant. Reservoir sampling keeps --cap instances per spec, on disk (one slot file
each, streamed into <VA>.bank.jsonl: the driver's --seed-bank format). Own-block coverage: each
function's executed live block leaders (driver.block_bounds), written to executed/<VA>.json.
Output: banks/, executed/ and record.json. With --snapshot, calls/<VA>.bin (entry states) and calls/<VA>.delta (each record's call-time delta id), with pool/ and deltas.jsonl, deltas.idx.jsonl (CallDelta).

Usage: GF_DIFF_PYTHON=<venv python> PES_EXE=<pes_full.bin> python3 frame_trace.py --capture DIR --out DIR
       [--frames N] [--cap N] [--budget INSTR]
"""
import argparse
import hashlib
import json
import os
import random
import shutil
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DIFF = os.path.join(HERE, "..", "diffharness")
sys.path.insert(0, HERE)
sys.path.insert(0, DIFF)
import run as rn  # noqa: E402  (diffharness emulator core: maps, stubs, stack)
import seed  # noqa: E402
import bankstore  # noqa: E402
import driver as drv  # noqa: E402
import stubs as stub_table  # noqa: E402
import unicorn.x86_const as xc  # noqa: E402
import full_snapshot as fs  # noqa: E402

PAGE = 0x1000
H_GLOBAL = 0x1436F3F98      # H = *(H_GLOBAL), the team-tick owner
H_STEP = 0x14041ECF0        # H's vtable slot 2: per-frame step (team tick loop at 0x14041EDD7)
TEAM_TICK = 0x1404F6C30     # team tick, called by H once per side
MI_OFF, PER_OFF = 0x270, 0x1168
G_SIZE = 0x2E58
PER_EARLY_RETURN = 0xA      # H returns at once when MI+0x1168 == 0xa (pes-re-teamtick.md H.b)
REGS_RESET = ("rbx", "rbp", "rax", "r10", "r11", "r12", "r13", "r14", "r15")
INSTR_BUDGET = 50_000_000    # instructions per H step: a deterministic runaway guard. No wall-clock
                             # timeout: unicorn returns from a timed-out emu_start with no error.


BLOCK = 0x40000             # snapshot bytes mapped per fault (256 KiB, clipped to the mapping)
PES_EXE = "PES2021.exe"     # the game image's mapping name: the one module a run may execute
STACK_BASE, STACK_SIZE = 0x10000000, 0x20000  # build_emu's default stack; rn.stack_top({}) is its top - 0x100


def mem_reader(uc):
    """addr, n -> bytes of the emulated memory at that instant, or None when unmapped. In a
    snapshot run an unmapped range is first mapped from the snapshot (LazyMap.ensure)."""
    def read(addr, n):
        if not (0 < addr < (1 << 47)) or n <= 0:
            return None
        try:
            return bytes(uc.mem_read(addr, n))
        except Exception:
            ensure = getattr(uc, "_ensure", None)
            if ensure is None or not ensure(addr, n):
                return None
        try:
            return bytes(uc.mem_read(addr, n))
        except Exception:
            return None
    return read


IMPORT_STUBS = os.path.join(HERE, "import_stubs.json")


def load_import_models(path=IMPORT_STUBS):
    """{(module file name, export name): stub code} from import_stubs.json. "return": mov rax,
    imm64; ret (the export returns value, no other effect). "memcpy": Win64 memcpy(dst, src, n)
    returns dst and copies n bytes forward (rep movsb; rdi and rsi are saved, as the ABI requires)."""
    models = {}
    for e in json.load(open(path))["imports"]:
        if e["do"] == "return":
            value = int(str(e["value"]), 0) & ((1 << 64) - 1)
            code = b"\x48\xB8" + struct.pack("<Q", value) + b"\xC3"  # mov rax, imm64; ret
        elif e["do"] == "memcpy":
            code = bytes.fromhex("48 89 C8 57 56 48 89 C7 48 89 D6 4C 89 C1 F3 A4 5E 5F C3")
        else:
            raise SystemExit("import_stubs.json: unknown do %r for %s!%s"
                             % (e["do"], e["module"], e["export"]))
        models[(e["module"].lower(), e["export"])] = code
    return models


class LazyMap:
    """A snapshot run's memory. Pages are mapped into unicorn on first touch, from the snapshot
    (full_snapshot.SnapMem: the whole process is never loaded). A fault maps the unmapped readable
    pages of its BLOCK. A hole (unreadable page) stays unmapped: touching it stops the run. The game
    image maps from the snapshot. A loaded OS module (a DLL) maps as import stubs, and only at an
    export entry: a modelled export runs its model (import_stubs.json); any other export is int3 and
    stops the run, named. Any other DLL address is refused."""

    def __init__(self, sm, models):
        self.sm = sm
        self.models = models
        self.uc = None
        self.refused = None
        self.faults = 0
        self.pages = 0
        self.import_calls = {}
        self.import_pages = set()  # pages mapped as import models: a replay maps them on fetch

    def attach(self, uc):
        from unicorn import UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_INTR
        self.uc = uc
        uc._lazy = self
        uc._ensure = self.ensure
        uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED, self._fetch)
        uc.hook_add(UC_HOOK_INTR, self._intr)

    def _mapped(self, page):
        try:
            self.uc.mem_read(page, 1)
            return True
        except Exception:
            return False

    def _map_block(self, addr):
        m = self.sm.mapping_of(addr)
        base = addr & ~(BLOCK - 1)
        lo, hi = max(base, m["va"]), min(base + BLOCK, m["va"] + m["size"])
        runs = []
        for p in range(lo, hi, PAGE):
            if not self.sm.page_ok(p) or self._mapped(p):
                continue
            if runs and runs[-1][1] == p:
                runs[-1][1] = p + PAGE
            else:
                runs.append([p, p + PAGE])
        for s, e in runs:
            self.uc.mem_map(s, e - s)
            self.uc.mem_write(s, self.sm.read(s, e - s))
            self.pages += (e - s) // PAGE
        self.faults += 1

    def handle(self, addr):
        """An unmapped data access: map its block if it is a snapshot page (the hook returns True)."""
        if not self.sm.page_ok(addr & ~(PAGE - 1)):
            return False
        self._map_block(addr)
        return True

    def ensure(self, addr, n):
        """Map every page under [addr, addr+n). False when one is a hole or outside the snapshot."""
        try:
            p = addr & ~(PAGE - 1)
            while p < addr + n:
                if not self._mapped(p) and not self.handle(p):
                    return False
                p += PAGE
            return True
        except Exception:
            return False

    def _refuse(self, uc, why):
        self.refused = why
        uc._bad = why
        return False

    def _counter(self, key):
        def count(uc, address, size, data):
            self.import_calls[key] += 1
        return count

    def _map_import_page(self, uc, page, mod, entries):
        from unicorn import UC_HOOK_CODE
        uc.mem_map(page, PAGE)
        self.import_pages.add(page)
        uc.mem_write(page, b"\xcc" * PAGE)  # int3: an export without a model stops the run
        for entry, name in entries:
            code = self.models.get((mod.lower(), name))
            if code is None or not page <= entry < page + PAGE:
                continue
            uc.mem_write(entry, code)
            key = "%s!%s" % (mod, name)
            self.import_calls.setdefault(key, 0)
            uc.hook_add(UC_HOOK_CODE, self._counter(key), None, begin=entry, end=entry)
        self.faults += 1
        self.pages += 1

    def _fetch(self, uc, access, addr, size, value, data):
        """A fetch from an unmapped page (see the class doc)."""
        m = self.sm.mapping_of(addr)
        page = addr & ~(PAGE - 1)
        if m is None or not self.sm.page_ok(page):
            return self._refuse(uc, "fetch from unmapped or unreadable snapshot memory at %#x" % addr)
        if not m["name"] or m["name"].endswith(PES_EXE):
            return self.handle(addr)
        mod, entries = self.sm.entries_of(addr)
        if not any(entry == addr for entry, _ in entries):
            return self._refuse(uc, "fetch inside %s at %#x, not an export entry" % (mod, addr))
        self._map_import_page(uc, page, mod, entries)
        return True

    def _intr(self, uc, intno, data):
        from unicorn.x86_const import UC_X86_REG_RIP
        rip = uc.reg_read(UC_X86_REG_RIP)
        mod, name, _ = self.sm.export_of(rip)
        what = "%s!%s" % (mod, name) if name else "interrupt %d" % intno
        self._refuse(uc, "unmodelled OS export %s at %#x: needs an import model" % (what, rip))
        uc.emu_stop()


TEB_BASE = 0x7FF00000  # the replay's thread environment block (GS base); outside every snapshot mapping


def map_teb(uc, stack_top):
    """Model the TEB the game's gs-relative reads expect (NT_TIB). StackBase is the emulated stack
    top; StackLimit is 0, so the stack-probe prologue takes its no-probe path (no guard pages here).
    Self (+0x30) is the TEB itself. Other TEB fields are zero: a read of one is a stop."""
    from unicorn.x86_const import UC_X86_REG_GS_BASE
    assert not any(m["va"] <= TEB_BASE < m["va"] + m["size"] for m in uc._lazy.sm.maps)
    uc.mem_map(TEB_BASE, PAGE)
    uc.mem_write(TEB_BASE + 0x08, struct.pack("<Q", stack_top))
    uc.mem_write(TEB_BASE + 0x30, struct.pack("<Q", TEB_BASE))
    uc.reg_write(UC_X86_REG_GS_BASE, TEB_BASE)


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


class CallDelta:
    """The emulated run's memory as a delta from the snapshot, at each impure call's entry: the call-time
    state that a pristine-snapshot replay does not have.

    Baseline: what a replay maps before it applies a delta, i.e. the snapshot's page, or nothing for a page
    outside the snapshot. The delta is every page the run has written (unicorn's write hook, and Python-side
    mem_write: stub models, LazyMap, map_teb) whose content differs from its baseline, plus the stack and TEB
    pages in full (their initial contents are the emulator's, not the snapshot's). Import-model pages are left
    out: a replay maps them on fetch, with their call counters.

    Output, in the trace directory: pool/pages.bin holds each distinct page once (4 KiB, in pool index order);
    deltas.jsonl holds each distinct delta once as {"id", "pages": [[va, pool index], ...]}, and
    deltas.idx.jsonl maps its id to its byte offset there."""
    FIXED = frozenset(range(STACK_BASE, STACK_BASE + STACK_SIZE, PAGE)) | {TEB_BASE}

    def __init__(self, uc, sm, lazy, out):
        from unicorn import UC_HOOK_MEM_WRITE
        assert rn.stack_top({}) == STACK_BASE + STACK_SIZE - 0x100
        self.uc, self.sm, self.lazy = uc, sm, lazy
        self.pending = set()   # page-aligned addresses written since the last entry
        self.diff = {}         # page -> pool index, for pages that differ from their baseline
        self.pool_of = {}      # sha256 digest -> pool index
        os.makedirs(os.path.join(out, "pool"), exist_ok=True)
        self.pool_f = open(os.path.join(out, "pool", "pages.bin"), "wb")
        self.deltas_f = open(os.path.join(out, "deltas.jsonl"), "wb")
        self.index_f = open(os.path.join(out, "deltas.idx.jsonl"), "w")
        self.written = set()
        uc.hook_add(UC_HOOK_MEM_WRITE, lambda u, acc, addr, size, val, data: self._mark(addr, size))
        emu_write = uc.mem_write

        def mem_write(address, data):
            self._mark(address, len(data))
            return emu_write(address, data)
        uc.mem_write = mem_write

    def _mark(self, addr, n):
        self.pending.update(range(addr & ~(PAGE - 1), addr + n, PAGE))

    def _page(self, page):
        try:
            return bytes(self.uc.mem_read(page, PAGE))
        except Exception:
            return None

    def _pool(self, data):
        key = hashlib.sha256(data).digest()
        if key not in self.pool_of:
            self.pool_of[key] = len(self.pool_of)
            self.pool_f.write(data)
        return self.pool_of[key]

    def _update(self):
        for page in self.pending:
            if page in self.FIXED or page in self.lazy.import_pages:
                continue
            data = self._page(page)
            base = self.sm.read(page, PAGE) if self.sm.page_ok(page) else None
            if data is None or data == base:
                self.diff.pop(page, None)
            else:
                self.diff[page] = self._pool(data)
        self.pending.clear()

    def delta(self):
        """The id of the delta at this instant; its record is written on first sight."""
        self._update()
        pages = dict(self.diff)
        for page in self.FIXED:
            data = self._page(page)
            if data is not None:
                pages[page] = self._pool(data)
        listing = [[page, pages[page]] for page in sorted(pages)]
        did = hashlib.sha256(json.dumps(listing).encode()).hexdigest()
        if did not in self.written:
            self.written.add(did)
            self.index_f.write(json.dumps([did, self.deltas_f.tell()]) + "\n")
            self.deltas_f.write((json.dumps({"id": did, "pages": listing}) + "\n").encode())
        return did

    def close(self):
        for f in (self.pool_f, self.deltas_f, self.index_f):
            f.close()
        return {"distinct": len(self.written), "pool_pages": len(self.pool_of)}


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
    """Seed artifacts for one spec: a reservoir of at most cap instances, uniform over the calls,
    or, with cap 0, every distinct call (one instance per content digest). The kept instances
    live on disk, one slot file each (rewritten in place for the reservoir), so memory holds one
    instance at a time however large --cap is."""

    def __init__(self, cap, tag, slot_dir):
        self.cap, self.n, self.slots = cap, 0, 0
        self.dir = slot_dir
        shutil.rmtree(slot_dir, ignore_errors=True)
        os.makedirs(slot_dir)
        self.rng = random.Random("frametrace:" + tag)
        self.seen = set()  # cap 0: content digests of the kept instances

    def _path(self, i):
        return os.path.join(self.dir, "slot%d.json" % i)

    def offer(self, make):
        self.n += 1
        if self.cap == 0:
            art = make()
            dg = hashlib.sha256(json.dumps(art, sort_keys=True).encode()).hexdigest()
            if dg in self.seen:
                return
            self.seen.add(dg)
            i = self.slots
            self.slots += 1
            with open(self._path(i), "w") as f:
                json.dump(art, f)
            return
        if self.slots < self.cap:
            i = self.slots
            self.slots += 1
        else:
            i = self.rng.randrange(self.n)
            if i >= self.cap:
                return
        with open(self._path(i), "w") as f:
            json.dump(make(), f)

    def artifacts(self):
        """The kept instances in slot order; each slot file is read, then removed."""
        for i in range(self.slots):
            with open(self._path(i)) as f:
                art = json.load(f)
            os.remove(self._path(i))
            yield art
        os.rmdir(self.dir)


def snapshot(uc, spec, roots):
    """The seed artifact of one call: the regions the spec declares, filled from emulated memory."""
    read = mem_reader(uc)
    g = struct.unpack("<Q", read(seed.GVA, 8) or bytes(8))[0]
    gbytes = (read(g, G_SIZE) or b"") if g else b""
    meta = {"g": g}
    regions, live = seed.build_live_seed(spec, meta, read, gbytes, roots)
    seed.patch_live_ptrs(spec, regions, live, meta, read)
    return seed.artifact_of(spec, regions)


IMAGE_LO, IMAGE_HI = 0x140000000, 0x15B97C000
STOP_REGS = ("rcx", "rdx", "rbx", "rsi", "rdi", "rbp", "r12", "r13", "r14", "r15")


def stack_images(uc, rsp, n=256):
    """Image addresses among the n qwords at rsp (the return addresses of the live call chain)."""
    try:
        vals = struct.unpack("<%dQ" % n, bytes(uc.mem_read(rsp, 8 * n)))
    except Exception:  # the stack's mapping ends before n qwords
        return []
    return [v for v in vals if IMAGE_LO <= v < IMAGE_HI]


def run_entry(uc, entry, regs, ret, budget):
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
        uc.emu_start(entry, ret, count=budget)
    except Exception as e:  # unicorn raises on a fault; the hooks record the reason
        err = "emu error: %s (last RIP %#x)" % (e, getattr(uc, "_last_rip", 0))
    if uc._bad is not None:
        err = uc._bad
    elif err is None and uc.reg_read(xc.UC_X86_REG_RIP) != ret:
        # A silent stop (budget spent) is a failure, never a clean frame.
        err = "did not return: RIP %#x after %d-instruction budget" % (
            uc.reg_read(xc.UC_X86_REG_RIP), budget)
    if err:  # diagnostic: the image return addresses on the stack, i.e. the call chain to the stop
        uc._rets = stack_images(uc, uc.reg_read(UC_X86_REG_RSP))
    return err


def scalars_at_entry(uc, spec):
    """The register and stack inputs of one call, read at its entry: the instance's "scalars", so a
    traced replay sets every input the call received. A stack slot sits above the return address
    (rsp + 8 + stack_offset). XMM inputs are not recorded (the replay leaves them unheld)."""
    rsp = uc.reg_read(rn.reg_const("rsp"))
    read = mem_reader(uc)
    out = {}
    for name, loc in (spec.get("inputs") or {}).items():
        if "reg" in loc and not str(loc["reg"]).startswith("xmm"):
            out[name] = uc.reg_read(rn.reg_const(loc["reg"]))
        elif "stack_offset" in loc:
            blob = read(rsp + 8 + int(str(loc["stack_offset"]), 0), 8)
            if blob:
                out[name] = struct.unpack("<Q", blob)[0]
    return out


def with_scalars(art, scalars):
    """The seed artifact with its entry scalars attached (artifact key "scalars")."""
    art["scalars"] = scalars
    return art


CALL_REC = struct.Struct("<49Q")  # 16 GPRs, 16 XMM (lo, hi), the return address at [rsp]


def entry_record(uc):
    """One call's full entry state: all 16 GPRs, all 16 XMM registers and the return address at
    [rsp]. The snapshot replay (run.py --snapshot-calls, the diff main's snapshot mode) starts each
    call from these registers over the snapshot's own memory; nothing else is recorded."""
    import unicorn.x86_const as xc
    names = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
    gprs = [uc.reg_read(getattr(xc, "UC_X86_REG_" + r.upper())) for r in names]
    xmm = []
    for i in range(16):
        v = uc.reg_read(xc.UC_X86_REG_XMM0 + i)
        if isinstance(v, (bytes, bytearray)):
            v = int.from_bytes(v, "little")
        xmm += [v & 0xFFFFFFFFFFFFFFFF, v >> 64]
    ret = struct.unpack("<Q", bytes(uc.mem_read(gprs[4], 8)))[0]
    return CALL_REC.pack(*gprs, *xmm, ret)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--capture", default=None, help="a capture_frame.py directory (walked capture)")
    ap.add_argument("--snapshot", default=None,
                    help="a full_snapshot.py directory (whole process, mapped lazily)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--image", default=os.environ.get("PES_EXE"))
    ap.add_argument("--spec-dir", default=os.path.join(DIFF, "specs", "impure"))
    ap.add_argument("--frames", type=int, default=50)
    ap.add_argument("--cap", type=int, default=32,
                    help="instances kept per spec (reservoir); 0 keeps every distinct call, "
                         "deduplicated by content")
    ap.add_argument("--budget", type=int, default=INSTR_BUDGET,
                    help="instructions per H step (deterministic guard)")
    ap.add_argument("--trace-addrs", action="store_true",
                    help="record every executed instruction address (diagnostic; slower)")
    a = ap.parse_args()
    if not a.image:
        ap.error("--image or PES_EXE")
    if bool(a.capture) == bool(a.snapshot):
        ap.error("give exactly one of --capture or --snapshot")
    os.makedirs(os.path.join(a.out, "banks"), exist_ok=True)
    t0 = time.time()
    sections = rn.flat_sections(a.image)
    if rn.image_kind(a.image) != "flat":
        ap.error("the frame trace needs the flat unpacked image (pes_full.bin)")
    cdelta = None  # the snapshot run's CallDelta; a capture run has none
    if a.snapshot:
        sm = fs.SnapMem(a.snapshot)
        uc, ret = rn.build_emu({"regions": {}}, a.image, [])
        lazy = LazyMap(sm, load_import_models())
        lazy.attach(uc)
        map_teb(uc, rn.stack_top({}))
        cdelta = CallDelta(uc, sm, lazy, a.out)
        if not lazy.ensure(H_GLOBAL, 8):
            raise SystemExit("H global is not readable in the snapshot")
        anc = sm.index["post"]
        meta = {"g": anc["G"], "mi": anc["MI"]}
        mapped = {"mode": "lazy snapshot", "maps": len(sm.maps), "holes": len(sm.holes)}
    else:
        lazy = None
        uc, ret = rn.build_emu({"regions": {}}, a.image, sections)
        meta, mapped = map_capture(uc, a.capture)
    t_map = time.time() - t0

    table = stub_table.load()
    spec_stubs = stubs_for_run(table)
    uc._addrs_on = a.trace_addrs
    uc._icount = 0  # run.py's shared code hook counts instructions (its valid-icount stats)
    rn.install_hooks(uc, spec_stubs, ret)
    stubbed = len(spec_stubs["stubs"])

    from unicorn import UC_HOOK_CODE
    impure = load_impure(a.spec_dir)
    banks, calls, roots, hit = {}, {}, {}, {}
    frame_log = []
    counters = {"tick": 0, "per_early": 0}

    def tick_hook(uc_, address, size, data):
        counters["tick"] += 1

    uc.hook_add(UC_HOOK_CODE, tick_hook, None, begin=TEAM_TICK, end=TEAM_TICK)

    os.makedirs(os.path.join(a.out, "calls"), exist_ok=True)
    seen_calls, call_files, call_delta_files = {}, {}, {}
    for tag, spec in impure.items():
        va = int(spec["va"], 16)
        args = {reg: root for reg, root in (spec.get("args") or {}).items()}
        roots[tag] = {root: "frametrace" for root in args.values()}
        banks[tag] = Bank(a.cap, tag, os.path.join(a.out, "slots", tag))
        calls[tag] = 0
        seen_calls[tag] = set()
        call_files[tag] = open(os.path.join(a.out, "calls", tag + ".bin"), "wb")
        call_delta_files[tag] = open(os.path.join(a.out, "calls", tag + ".delta"), "w")

        def on_entry(uc_, address, size, data, tag=tag, spec=spec, args=args):
            calls[tag] += 1
            regs = {reg: uc_.reg_read(rn.reg_const(reg)) for reg in args}
            names = {reg: root for reg, root in args.items()}
            call_roots = {names[r]: v for r, v in regs.items()}
            scal = scalars_at_entry(uc_, spec)
            banks[tag].offer(lambda: with_scalars(snapshot(uc_, spec, call_roots), scal))
            did = cdelta.delta() if cdelta is not None else ""
            rec = entry_record(uc_)
            if (rec, did) not in seen_calls[tag]:
                seen_calls[tag].add((rec, did))
                call_files[tag].write(rec)
                call_delta_files[tag].write(did + "\n")

        uc.hook_add(UC_HOOK_CODE, on_entry, None, begin=va, end=va)
        # Own-block coverage: the live leaders of this function's own blocks. The range is the
        # function's own decode, so a callee's blocks are never counted for it.
        lay = drv.block_bounds(a.image, spec["va"])
        if lay is not None:
            own_addrs, _, live = lay
            hit[tag] = set()

            def on_leader(uc_, address, size, data, tag=tag, leaders=frozenset(live)):
                if address in leaders:
                    hit[tag].add(address)
            uc.hook_add(UC_HOOK_CODE, on_leader, None, begin=min(own_addrs), end=max(own_addrs))

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
        err = run_entry(uc, H_STEP, {"rcx": h_obj}, ret, a.budget)
        frame_log.append({"frame": f, "H": hex(h_obj), "MI": hex(mi), "per": per,
                          "ticks": counters["tick"] - tick0,
                          "impure_calls": sum(calls.values()) - calls0,
                          "secs": round(time.time() - t_s, 3), "err": err})
        if err:
            stop = {"frame": f, "err": err, "last_rip": hex(getattr(uc, "_last_rip", 0)),
                    "rets": [hex(v) for v in getattr(uc, "_rets", [])],
                    "regs": {r: hex(uc.reg_read(getattr(xc, "UC_X86_REG_" + r.upper()))) for r in STOP_REGS}}
            break

    banks_written = []
    for tag, bank in banks.items():
        if bank.n:
            bankstore.write_bank(os.path.join(a.out, "banks", tag + ".bank.jsonl"), roots[tag],
                                 bank.artifacts())
            banks_written.append(tag)
    os.makedirs(os.path.join(a.out, "executed"), exist_ok=True)
    for tag, leaders in hit.items():
        if leaders:
            with open(os.path.join(a.out, "executed", tag + ".json"), "w") as f:
                json.dump(["0x%X" % x for x in sorted(leaders)], f)
    label_src = a.capture or a.snapshot
    record = {"capture": label_src, "label": os.path.basename(label_src.rstrip("/")),
              "meta": {k: meta.get(k) for k in ("g", "mi", "teams", "h", "m", "ma", "frame_delta")},
              "mapped": mapped, "stubbed_callees": stubbed, "map_secs": round(t_map, 2),
              "frames_requested": a.frames, "frames_ran": len(frame_log),
              "ticks_entered": counters["tick"], "per10_early_returns": counters["per_early"],
              "stop": stop, "impure_calls": calls,
              "impure_reached": sorted(t for t, n in calls.items() if n),
              "instances": {t: banks[t].n for t in banks if banks[t].n},
              "executed_leaders": {t: len(s) for t, s in hit.items() if s},
              "banks_written": banks_written, "frames": frame_log,
              "secs_total": round(time.time() - t0, 2)}
    if cdelta is not None:
        record["deltas"] = cdelta.close()
    if lazy is not None:
        record["lazy"] = {"faults": lazy.faults, "pages": lazy.pages, "refused": lazy.refused,
                          "import_calls": lazy.import_calls}
    if a.trace_addrs and getattr(uc, "_addrs", None) is not None:
        with open(os.path.join(a.out, "addrs.txt"), "w") as f:
            f.write("\n".join("%X" % x for x in sorted(uc._addrs)) + "\n")
    json.dump(record, open(os.path.join(a.out, "record.json"), "w"), indent=1)
    print("frames_ran=%d ticks=%d reached=%d/%d stop=%s" % (
        len(frame_log), counters["tick"], len(record["impure_reached"]), len(impure),
        stop), flush=True)


if __name__ == "__main__":
    sys.exit(main())
