#!/usr/bin/env python3
"""Complete process snapshot of PES (owner ruling 26-10-26: brief SIGSTOP snapshots allowed).

Per snapshot: wait (read-only pread polling) until the match is in open play (MI = *(G+0x270)
non-null and MI+0x116C == 1), then
  1. kill -STOP <pid>;
  2. read /proc/<pid>/maps, then every readable WRITABLE mapping (and every shared mapping)
     through /proc/<pid>/mem with process_vm_readv (threads, chunked), streamed to mem.bin in
     maps order. A page that cannot be read is zero-filled and listed as a hole. Device
     mappings (/dev/*: GPU memory the kernel refuses to read) are holes, not read;
  3. kill -CONT <pid> in a finally block: the game resumes even if the dump fails.
  4. After CONT, the private read-only mappings (code and rodata: r-xp, r--p) are read into the
     same mem.bin. They are not written by the process, so reading them after the pause is the
     same bytes; index.json marks them "deferred" and tools/dev/translate/verify_snapshot.py
     compares the PE image's code against the disk file to show this.
The paused window is logged (pauses.jsonl). The index (mappings, offsets, holes, pause times and
the anchors read back from the dump) is written after CONT, outside the pause.

No other signal, no write to the process, no debugger. Output is scratch and PES-derived:
never committed. Run with sudo (the process mem file is mode 600).

Usage: sudo -n python3 full_snapshot.py --out DIR [--count 3] [--gap 180] [--wait 3600]
       [--pid P] [--threads 4] [--chunk-mib 64]
"""
import argparse
import bisect
import ctypes
import json
import os
import signal
import struct
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import seed  # noqa: E402  GVA: the G global

PAGE = 0x1000
MI_OFF, PLAY_OFF, PER_OFF, FRAME_OFF = 0x270, 0x116C, 0x1168, 0x1198
POLL = 0.5
DEADLINE = [None]  # monotonic time after which a pause dump aborts (the game is CONTed at once)


class PauseOverrun(Exception):
    pass
_libc = ctypes.CDLL("libc.so.6", use_errno=True)


class _IOV(ctypes.Structure):
    _fields_ = [("base", ctypes.c_void_p), ("len", ctypes.c_size_t)]


_libc.process_vm_readv.restype = ctypes.c_ssize_t
_libc.process_vm_readv.argtypes = [ctypes.c_int, ctypes.POINTER(_IOV), ctypes.c_ulong,
                                   ctypes.POINTER(_IOV), ctypes.c_ulong, ctypes.c_ulong]


def pvr(pid, local_addr, va, n):
    """Read n bytes at va of pid into local_addr. Returns bytes read (0 on failure)."""
    loc = _IOV(local_addr, n)
    rem = _IOV(va, n)
    r = _libc.process_vm_readv(pid, ctypes.byref(loc), 1, ctypes.byref(rem), 1, 0)
    return r if r > 0 else 0


def parse_maps(pid):
    out = []
    with open("/proc/%d/maps" % pid) as f:
        for line in f:
            p = line.split()
            lo, hi = (int(x, 16) for x in p[0].split("-"))
            out.append((lo, hi, p[1], p[5] if len(p) > 5 else ""))
    return out


def probe(pid):
    """Read-only anchors from the live process (no pause)."""
    fd = os.open("/proc/%d/mem" % pid, os.O_RDONLY)
    try:
        def u64(va):
            b = os.pread(fd, 8, va)
            return struct.unpack("<Q", b)[0] if len(b) == 8 else 0

        def i32(va):
            b = os.pread(fd, 4, va)
            return struct.unpack("<i", b)[0] if len(b) == 4 else None
        g = u64(seed.GVA)
        mi = u64(g + MI_OFF) if g else 0
        if not mi:
            return {"G": g, "MI": 0}
        return {"G": g, "MI": mi, "play": i32(mi + PLAY_OFF), "per": i32(mi + PER_OFF),
                "frame": i32(mi + FRAME_OFF)}
    finally:
        os.close(fd)


def wait_open_play(pid, wait_s, log):
    t_end = time.time() + wait_s
    last = None
    while time.time() < t_end:
        st = probe(pid)
        key = (st.get("MI") != 0, st.get("play"))
        if key != last:
            log("wait state MI=%s play=%s per=%s" % (hex(st.get("MI", 0)), st.get("play"), st.get("per")))
            last = key
        if st.get("MI") and st.get("play") == 1:
            return st
        time.sleep(POLL)
    raise SystemExit("no open play within %.0f s" % wait_s)


def plan(maps):
    """Index entries for every readable mapping, in maps order, with file offsets."""
    mappings, off = [], 0
    for lo, hi, perm, name in maps:
        if perm[0] != "r":
            continue
        device = name.startswith("/dev/")
        # Wine's shared memfd views (incl. KUSER_SHARED_DATA) change while the game runs, so they stay in the pause.
        deferred = (not device) and perm[1] != "w" and perm[3] == "p" and not name.startswith("/memfd:")
        mappings.append({"va": lo, "size": hi - lo, "perm": perm, "name": name, "off": off,
                         "deferred": deferred, "device": device})
        off += hi - lo
    return mappings, off


def run_jobs(pid, out_fd, mappings, sel, chunk, threads, holes):
    """Read the mappings selected by sel (True: deferred ones; False: the pause set)."""
    jobs = []
    for m in mappings:
        if m["deferred"] != sel:
            continue
        if m["device"]:
            holes.append([m["va"], m["size"]])
            continue
        c, hi = m["va"], m["va"] + m["size"]
        while c < hi:
            n = min(chunk, hi - c)
            jobs.append((c, n, m["off"] + (c - m["va"])))
            c += n
    if not jobs:
        return
    tl = threading.local()
    lock = threading.Lock()
    local_holes = []

    def write_all(buf_mv, o):
        done = 0
        n = len(buf_mv)
        while done < n:
            done += os.pwrite(out_fd, buf_mv[done:], o + done)

    def work(job):
        va, n, o = job
        if DEADLINE[0] is not None and time.monotonic() > DEADLINE[0]:
            raise PauseOverrun("pause passed its limit")
        buf = getattr(tl, "buf", None)
        if buf is None:
            buf = tl.buf = ctypes.create_string_buffer(chunk)
        base = ctypes.addressof(buf)
        if pvr(pid, base, va, n) == n:
            write_all(memoryview(buf)[:n], o)
            return
        # Slow path: page by page; unreadable pages stay zero and are recorded as holes.
        ctypes.memset(buf, 0, n)
        bad = []
        p = va
        while p < va + n:
            m = min(PAGE - p % PAGE, va + n - p)
            if pvr(pid, base + (p - va), p, m) != m:
                ctypes.memset(base + (p - va), 0, m)
                bad.append([p, m])
            p += m
        write_all(memoryview(buf)[:n], o)
        if bad:
            with lock:
                local_holes.extend(bad)

    with ThreadPoolExecutor(threads) as ex:
        list(ex.map(work, jobs))
    holes.extend(local_holes)


class SnapMem:
    """Read-only view of a snapshot directory: mappings from index.json, bytes from mem.bin."""

    def __init__(self, snap_dir):
        idx = json.load(open(os.path.join(snap_dir, "index.json")))
        self.index = idx
        self.maps = idx["mappings"]
        self.starts = [m["va"] for m in self.maps]
        self.fd = os.open(os.path.join(snap_dir, "mem.bin"), os.O_RDONLY)
        self._exports = {}
        self.holes = set()
        for va, n in idx["holes"]:
            self.holes.update(range(va & ~(PAGE - 1), va + n, PAGE))

    def mapping_of(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0 and va < self.maps[i]["va"] + self.maps[i]["size"]:
            return self.maps[i]
        return None

    def page_ok(self, page):
        m = self.mapping_of(page)
        return m is not None and page not in self.holes

    def read(self, va, n):
        """Bytes at [va, va+n) if every page is a readable snapshot page, else None."""
        if n <= 0:
            return None
        p = va & ~(PAGE - 1)
        while p < va + n:
            if not self.page_ok(p):
                return None
            p += PAGE
        m = self.mapping_of(va)
        if va + n > m["va"] + m["size"]:  # one mapping only: file offsets are per mapping
            return None
        return os.pread(self.fd, n, m["off"] + (va - m["va"]))

    def u64(self, va):
        b = self.read(va, 8)
        return struct.unpack("<Q", b)[0] if b else 0

    def i32(self, va):
        b = self.read(va, 4)
        return struct.unpack("<i", b)[0] if b else None

    def u16(self, va):
        b = self.read(va, 2)
        return struct.unpack("<H", b)[0] if b else 0

    def cstr(self, va):
        out = b""
        while len(out) < 256:
            b = self.read(va + len(out), 1)
            if not b or b == b"\0":
                break
            out += b
        return out.decode("ascii", "replace")

    def export_of(self, va):
        """(module file name, export name or None, offset from it) for a code address in a loaded
        PE module of the snapshot: the nearest named export at or below va."""
        m = self.mapping_of(va)
        if m is None or not m["name"]:
            return None, None, None
        mod = os.path.basename(m["name"])
        base = min(x["va"] for x in self.maps if x["name"] == m["name"])
        if self.u16(base) != 0x5A4D:  # 'MZ'
            return mod, None, None
        if base not in self._exports:
            self._exports[base] = self._parse_exports(base)
        exports = self._exports[base]
        i = bisect.bisect_right(exports, (va - base, "\uffff")) - 1
        if i < 0:
            return mod, None, None
        rva, name = exports[i]
        return mod, name, va - base - rva

    def _parse_exports(self, base):
        """Sorted (rva, name) of a PE module's named exports, from its in-memory headers."""
        def rd(off, n):
            return self.read(base + off, n) or b""
        e_lfanew = struct.unpack("<I", rd(0x3C, 4).ljust(4, b"\0"))[0]
        dd0 = struct.unpack("<I", rd(e_lfanew + 24 + 112, 4).ljust(4, b"\0"))[0]
        ed = rd(dd0, 40) if dd0 else b""
        if len(ed) < 40:
            return []
        n_funcs, n_names, f_rva, n_rva, o_rva = struct.unpack_from("<IIIII", ed, 20)
        names, ords, funcs = rd(n_rva, 4 * n_names), rd(o_rva, 2 * n_names), rd(f_rva, 4 * n_funcs)
        out = []
        for i in range(n_names):
            name_rva = struct.unpack_from("<I", names, 4 * i)[0]
            ordinal = struct.unpack_from("<H", ords, 2 * i)[0]
            fn_rva = struct.unpack_from("<I", funcs, 4 * ordinal)[0]
            out.append((fn_rva, self.cstr(base + name_rva)))
        return sorted(out)

    def entries_of(self, va):
        """(module file name, [(entry VA, export name)]) of the PE module holding va; [] if none."""
        m = self.mapping_of(va)
        if m is None or not m["name"]:
            return None, []
        mod = os.path.basename(m["name"])
        base = min(x["va"] for x in self.maps if x["name"] == m["name"])
        if self.u16(base) != 0x5A4D:  # 'MZ'
            return mod, []
        if base not in self._exports:
            self._exports[base] = self._parse_exports(base)
        return mod, [(base + rva, nm) for rva, nm in self._exports[base]]


def anchors_from_dump(snap_dir):
    sm = SnapMem(snap_dir)
    g = sm.u64(seed.GVA)
    mi = sm.u64(g + MI_OFF) if g else 0
    a = {"G": g, "MI": mi}
    if mi:
        a.update({"play": sm.i32(mi + PLAY_OFF), "per": sm.i32(mi + PER_OFF),
                  "frame": sm.i32(mi + FRAME_OFF), "restart": sm.i32(mi + 0x1554),
                  "side": sm.i32(mi + 0x1558), "reason": sm.i32(mi + 0x1574)})
    return a


def take(pid, out_dir, label, args, log, pauses_log):
    snap = os.path.join(out_dir, label)
    os.makedirs(snap, exist_ok=True)
    pre = wait_open_play(pid, args.wait, log)
    log("%s pre-STOP MI=%#x play=%s per=%s frame=%s" % (label, pre["MI"], pre["play"], pre["per"], pre["frame"]))
    rec = {"label": label, "pid": pid, "pre": pre, "t_wall": time.time()}
    holes, stopped = [], False
    out_fd = os.open(os.path.join(snap, "mem.bin"), os.O_CREAT | os.O_WRONLY | os.O_TRUNC, 0o600)
    aborted = False
    try:
        t_stop = time.monotonic()
        DEADLINE[0] = t_stop + args.max_pause
        os.kill(pid, signal.SIGSTOP)
        stopped = True
        mappings, total = plan(parse_maps(pid))
        os.ftruncate(out_fd, total)
        run_jobs(pid, out_fd, mappings, False, args.chunk_mib << 20, args.threads, holes)
    except PauseOverrun:
        aborted = True
    finally:
        if stopped:
            os.kill(pid, signal.SIGCONT)
        secs = time.monotonic() - t_stop
        DEADLINE[0] = None
    if aborted:
        os.close(out_fd)
        log("%s ABORTED: pause passed %.1f s at %.3f s; game resumed" % (label, args.max_pause, secs))
        pauses_log.write(json.dumps({"label": label, "aborted": True, "pause_secs": round(secs, 3)}) + "\n")
        pauses_log.flush()
        return False, None
    try:
        run_jobs(pid, out_fd, mappings, True, args.chunk_mib << 20, args.threads, holes)
    finally:
        os.close(out_fd)
    holes.sort()
    rec.update({"pause_secs": round(secs, 3), "mappings_n": len(mappings), "bytes": total,
                "holes_n": len(holes), "holes_bytes": sum(n for _, n in holes),
                "deferred_n": sum(1 for m in mappings if m["deferred"])})
    pauses_log.write(json.dumps(rec) + "\n")
    pauses_log.flush()
    log("%s paused %.3f s: %d mappings, %.2f GiB, %d holes" % (label, secs, len(mappings), total / 2**30, len(holes)))
    idx = {"label": label, "pid": pid, "taken_unix": rec["t_wall"], "pause_secs": round(secs, 3),
           "pre": pre, "mappings": mappings, "holes": holes, "bytes": total}
    open(os.path.join(snap, "index.json"), "w").write(json.dumps(idx))
    post = anchors_from_dump(snap)
    idx["post"] = post
    open(os.path.join(snap, "index.json"), "w").write(json.dumps(idx))
    ok = bool(post.get("MI")) and post.get("play") == 1
    log("%s dump anchors: %s -> %s" % (label, json.dumps(post), "KEEP" if ok else "REJECT (not open play in dump)"))
    return ok, post


def own(root):
    uid, gid = os.environ.get("SUDO_UID"), os.environ.get("SUDO_GID")
    if uid and gid:
        subprocess.call(["chown", "-R", "%s:%s" % (uid, gid), root])


def _terminate(signum, frame):
    raise SystemExit("signal %d" % signum)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--count", type=int, default=3)
    ap.add_argument("--gap", type=float, default=180, help="s between accepted snapshots")
    ap.add_argument("--wait", type=float, default=3600, help="s to wait for open play per snapshot")
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--chunk-mib", type=int, default=64)
    ap.add_argument("--max-tries", type=int, default=10)
    ap.add_argument("--max-pause", type=float, default=2.0,
                    help="s: a pause dump that passes this aborts and resumes the game at once")
    a = ap.parse_args()
    if a.max_pause <= 0:
        ap.error("--max-pause must be positive")
    os.makedirs(a.out, exist_ok=True)
    signal.signal(signal.SIGTERM, _terminate)
    logf = open(os.path.join(a.out, "snapshot.log"), "a", buffering=1)

    def log(msg):
        line = "%s %s" % (time.strftime("%H:%M:%S"), msg)
        logf.write(line + "\n")
        print(line, flush=True)

    pid = a.pid or int(subprocess.check_output(["pgrep", "-x", "PES2021.exe"]).split()[0])
    pauses_log = open(os.path.join(a.out, "pauses.jsonl"), "a")
    kept, tries, last_ok = 0, 0, -1e9
    try:
        while kept < a.count and tries < a.max_tries:
            wait_gap = last_ok + a.gap - time.time()
            if wait_gap > 0:
                time.sleep(wait_gap)
            tries += 1
            label = "snap%d-%s" % (kept + 1, time.strftime("%H%M%S"))
            ok, _ = take(pid, a.out, label, a, log, pauses_log)
            if ok:
                kept += 1
                last_ok = time.time()
            else:
                os.remove(os.path.join(a.out, label, "mem.bin"))
    finally:
        own(a.out)
    log("done kept=%d tries=%d" % (kept, tries))
    return 0 if kept == a.count else 1


if __name__ == "__main__":
    sys.exit(main())
