"""Snapshot replay of traced calls (run.py --snapshot-dir): the harness side of the snapshot diff.

Each record is one traced call (frame_trace.py's entry_record: 16 GPRs, 16 XMM registers, and the
return address at [rsp]). Each call starts from the snapshot's own pages (full_snapshot.SnapMem through
frame_trace.LazyMap, copied on first touch), with its call-time delta mapped over them (load_deltas): the pages
the emulated run had changed, plus its stack and TEB. One unicorn engine is reused for the run; between calls
every mapped page is unmapped, so no call sees another call's writes. The call runs from its entry to its recorded return
address. Recorded per call: rax, the full xmm0, and every byte the call writes (address and value, in
order). A call that leaves the model is excluded, with its reason: an abort trap, a hole or unmapped
access, an OS import model (the translator has no import call), an untranslated (blocked) callee
entered, a budget overrun, or a return that does not come back to its caller.

Outputs in the out dir:
  corpus.jsonl  one line per record: {"case", "ok": true, "rax", "xmm0", "w"} or {"case", "ok": false, "reason"}
                w is the written bytes as hex: per byte, 16 hex digits of address then 2 of value.
  ok_calls.bin  the replayable records, in order (the translated diff main's input).
  ok_delta.bin  per replayable call, its delta: u64 page count, then per page u64 VA and u64 pool byte offset.
  maps.txt      the snapshot's readable runs (the translated side's pages).
  addrs.json    executed instruction addresses, union over the ok calls (own-block coverage).
  trace.json    indirect call sites and their entered targets, union over the ok calls (the closure).
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import run as rn  # noqa: E402
import full_snapshot as fs  # noqa: E402
import frame_trace as ft  # noqa: E402

CALL_REC = struct.Struct("<49Q")  # 16 GPRs, 16 XMM (lo, hi), the return address
GPR_NAMES = ("rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15")
INSTR_BUDGET = 2_000_000  # per call: a deterministic runaway guard (an overrun is excluded)
PAGE = 0x1000


def load_calls(path):
    """The call records of one calls file (frame_trace.py's calls/<VA>.bin), in order."""
    with open(path, "rb") as f:
        data = f.read()
    return [CALL_REC.unpack_from(data, i * CALL_REC.size) for i in range(len(data) // CALL_REC.size)]


def trace_dir(calls_path):
    return os.path.dirname(os.path.dirname(os.path.abspath(calls_path)))


def load_deltas(calls_path):
    """The call-time delta of each record of a calls file, as [(page VA, pool byte offset), ...]: the
    calls/<VA>.delta sidecar names each record's delta id, and deltas.jsonl (frame_trace.py CallDelta)
    holds it. A page the list does not name is the snapshot's (or unmapped)."""
    trace = trace_dir(calls_path)
    with open(os.path.splitext(calls_path)[0] + ".delta") as f:
        ids = [ln.strip() for ln in f]
    if len(ids) != os.path.getsize(calls_path) // CALL_REC.size:
        raise ValueError("%s: %d delta ids for the calls file" % (calls_path, len(ids)))
    need = set(ids)
    at = {}
    with open(os.path.join(trace, "deltas.idx.jsonl")) as f:
        for ln in f:
            did, off = json.loads(ln)
            if did in need:
                at[did] = off
    lists = {}
    with open(os.path.join(trace, "deltas.jsonl"), "rb") as f:
        for did, off in at.items():
            f.seek(off)
            lists[did] = [(va, idx * PAGE) for va, idx in json.loads(f.readline())["pages"]]
    return [lists[d] for d in ids]



def write_maps(snap_dir, path):
    """The readable runs of a snapshot (mapping pages minus holes) as "va size file_off" hex lines.
    The translated side reads its pages from mem.bin through these runs: the pages SnapMem reads."""
    idx = json.load(open(os.path.join(snap_dir, "index.json")))
    holes = set()
    for va, n in idx["holes"]:
        holes.update(range(va & ~0xFFF, va + n, 0x1000))
    with open(path, "w") as f:
        for m in sorted(idx["mappings"], key=lambda m: m["va"]):
            p, end = m["va"], m["va"] + m["size"]
            run = None
            while p <= end:
                ok = p < end and (p & ~0xFFF) not in holes
                if ok and run is None:
                    run = p
                if not ok and run is not None:
                    f.write("%x %x %x\n" % (run, p - run, m["off"] + run - m["va"]))
                    run = None
                p += 0x1000
    return path


def _entered(va):
    """A code hook that ends the call when it enters va (an untranslated callee of the build)."""
    def hook(uc, address, size, data):
        uc._bad = "unmodellable callee %#x entered" % va
        uc.emu_stop()
    return hook


class Replayer:
    """One unicorn engine over the snapshot, reused for every call of a run. (Each engine reserves a
    translator buffer: one engine per call would exhaust the address-space limit.) The hooks are
    installed once; a call resets the per-call state, unmaps every page the last call mapped, and
    starts from its recorded registers and return address."""

    def __init__(self, spec, sm, models, blocked, pool_path):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
        self.spec = spec
        self.pool = os.open(pool_path, os.O_RDONLY)  # the trace's pool: the call-time pages
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc._addrs_on = True
        uc._icount = 0  # run.py's shared code hook counts instructions (its valid-icount stats)
        uc._trace_on = True
        trace = os.path.dirname(os.path.dirname(pool_path))  # the trace dir: pool/pages.bin's parent
        self.teb = int(json.load(open(os.path.join(trace, "record.json")))["teb"], 16)
        self.lazy = ft.LazyMap(sm, models)
        self.lazy.attach(uc)
        rn.install_hooks(uc, spec, 0)  # stubs and abort traps as the harness runs them
        for va in blocked:
            uc.hook_add(UC_HOOK_CODE, _entered(va), begin=va, end=va)
        uc._writes = []
        uc.hook_add(UC_HOOK_MEM_WRITE, self._on_write)
        self.stub_vas = {int(s["va"], 16) for s in (spec.get("stubs") or {}).values()}
        import capstone
        import unicorn.x86_const as xc
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True
        self._rsp = xc.UC_X86_REG_RSP
        uc._call_pushes = 0
        uc._call_slots = set()
        uc._ret_slot_reads = 0
        uc.hook_add(UC_HOOK_MEM_READ, self._on_read)

    def _on_read(self, u, access, addr, size, value, data):
        """A read of a return-address slot the translation does not push (normalised above). The only
        legitimate read is the callee's own RET; any other is a function reading its return address,
        which the normalisation could hide. Counted in the case (retread)."""
        if addr in u._call_slots:
            ins = next(self.md.disasm(bytes(u.mem_read(u._last_rip, 16)), u._last_rip), None)
            if ins is None or ins.mnemonic != "ret":
                u._ret_slot_reads += 1

    def _on_write(self, u, access, addr, size, value, data):
        """A written byte run. The translator's CALL does not push the return address: a call to a
        stub pushes nothing (stubs.py's model stands for the call), and a call to a translated function
        pushes 0 (its return is a C++ return). So a CALL's return-address push is recorded as the
        translation does it: dropped for a stub, 0 otherwise. Counted in uc._call_pushes."""
        v = value & ((1 << (8 * size)) - 1)
        if size == 8 and addr == u.reg_read(self._rsp) - 8:
            ins = next(self.md.disasm(bytes(u.mem_read(u._last_rip, 16)), u._last_rip), None)
            if ins is not None and ins.mnemonic == "call":
                if ins.operands and ins.operands[0].type == 2 and ins.operands[0].imm in self.stub_vas:
                    return
                v = 0
                u._call_pushes += 1
                u._call_slots.add(addr)
        u._writes.append((addr, size, v))

    def run(self, rec, pages):
        """One call from its record. Returns (result, reason): result is None when the call is excluded
        (reason says why); otherwise a dict with rax, xmm0, w (see the module doc), addrs and trace."""
        import unicorn.x86_const as xc
        from unicorn import UcError
        uc, lazy = self.uc, self.lazy
        for begin, end, _perm in list(uc.mem_regions()):
            uc.mem_unmap(begin, end - begin + 1)
        for va, off in pages:  # the call-time memory, over the snapshot's own pages
            uc.mem_map(va, PAGE)
            uc.mem_write(va, os.pread(self.pool, PAGE, off))
        uc.reg_write(xc.UC_X86_REG_GS_BASE, self.teb)
        lazy.refused = None
        for k in lazy.import_calls:
            lazy.import_calls[k] = 0
        uc._writes, uc._addrs, uc._trace, uc._prev = [], set(), {}, [None, 0]
        uc._call_slots, uc._ret_slot_reads = set(), 0
        uc._bad, uc._last_rip = None, 0
        gpr = dict(zip(GPR_NAMES, rec[:16]))
        rsp, ret = gpr["rsp"], rec[48]
        uc._ret_sentinel = ret
        if not lazy.ensure(rsp, 8):
            return None, "stack slot at rsp not in the snapshot"
        uc.mem_write(rsp, struct.pack("<Q", ret))
        for i, name in enumerate(GPR_NAMES):
            uc.reg_write(getattr(xc, "UC_X86_REG_" + name.upper()), rec[i])
        for i in range(16):
            uc.reg_write(xc.UC_X86_REG_XMM0 + i, (rec[17 + 2 * i] << 64) | rec[16 + 2 * i])
        err = None
        entry = int(self.spec["va"], 16)
        try:
            uc.emu_start(entry, ret, count=INSTR_BUDGET)
        except UcError as e:
            err = "emu error: %s (last RIP %#x)" % (e, uc._last_rip)
        if uc._bad is not None:
            err = uc._bad
        elif lazy.refused is not None:
            err = lazy.refused
        # A modelled OS export runs its import model (import_stubs.json: the contract the translator
        # implements too, import_call_lines in xlate.py). An unmodelled export is int3 and refused above.
        elif err is None and (uc.reg_read(xc.UC_X86_REG_RIP) != ret or
                              uc.reg_read(xc.UC_X86_REG_RSP) != rsp + 8):
            err = "did not return to %#x (budget or a stray transfer)" % ret
        if err is not None:
            return None, err
        w = "".join("%016x%02x" % (addr + i, (val >> (8 * i)) & 0xFF)
                    for addr, size, val in uc._writes for i in range(size))
        return {"rax": "%016x" % uc.reg_read(xc.UC_X86_REG_RAX),
                "xmm0": "%032x" % uc.reg_read(xc.UC_X86_REG_XMM0), "w": w,
                "addrs": set(uc._addrs), "trace": uc._trace, "retread": uc._ret_slot_reads}, None


def run_dir(spec_path, snap_dir, calls_path, out_dir, blocked=()):
    """Replay every record of one calls file over one snapshot; write the outputs of the module doc.
    blocked: VAs of untranslated callees (driver.runnable_extra); a call that enters one is excluded."""
    spec = rn.resolve_stubs(json.load(open(spec_path)))
    sm = fs.SnapMem(snap_dir)
    models = ft.load_import_models()
    recs = load_calls(calls_path)
    deltas = load_deltas(calls_path)
    os.makedirs(out_dir, exist_ok=True)
    write_maps(snap_dir, os.path.join(out_dir, "maps.txt"))
    pool = os.path.join(trace_dir(calls_path), "pool", "pages.bin")
    rep = Replayer(spec, sm, models, [int(v, 16) for v in blocked], pool)
    corpus = open(os.path.join(out_dir, "corpus.jsonl"), "w")
    ok_f = open(os.path.join(out_dir, "ok_calls.bin"), "wb")
    ok_d = open(os.path.join(out_dir, "ok_delta.bin"), "wb")
    addrs, trace, reasons = set(), {}, {}
    n_ok = 0
    for i, rec in enumerate(recs):
        res, reason = rep.run(rec, deltas[i])
        if res is None:
            key = reason.split(":")[0].split(" (")[0]
            reasons[key] = reasons.get(key, 0) + 1
            corpus.write(json.dumps({"case": i, "ok": False, "reason": reason}) + "\n")
            continue
        n_ok += 1
        addrs |= res["addrs"]
        for site, tgts in res["trace"].items():
            for t, c in tgts.items():
                trace.setdefault(site, {})
                trace[site][t] = trace[site].get(t, 0) + c
        corpus.write(json.dumps({"case": i, "ok": True, "rax": res["rax"], "xmm0": res["xmm0"],
                                 "w": res["w"], "retread": res["retread"]}) + "\n")
        ok_f.write(CALL_REC.pack(*rec))
        ok_d.write(struct.pack("<Q", len(deltas[i])) + b"".join(struct.pack("<QQ", va, off) for va, off in deltas[i]))
    corpus.close()
    ok_f.close()
    ok_d.close()
    with open(os.path.join(out_dir, "addrs.json"), "w") as f:
        json.dump(sorted("0x%X" % a for a in addrs), f)
    with open(os.path.join(out_dir, "trace.json"), "w") as f:
        json.dump(trace, f)
    print("%s: %d calls, %d replayed, %d excluded %s" % (
        os.path.basename(snap_dir), len(recs), n_ok, len(recs) - n_ok, json.dumps(reasons)), flush=True)
    return n_ok, len(recs)
