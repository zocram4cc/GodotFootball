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


IMAGE_BASE = 0x140000000  # the game image's load base; pes_full.bin's file offset is VA - IMAGE_BASE


def _data_extents(fd):
    """[start, end) byte ranges of the file that hold allocated data. A slim capture (rendezvous/slim.py) leaves the
    pages it dropped as holes; a full capture writes every page, so its only holes are the index holes."""
    size = os.fstat(fd).st_size
    out, pos = [], 0
    while pos < size:
        try:
            start = os.lseek(fd, pos, os.SEEK_DATA)
        except OSError:  # ENXIO: no data at or after pos
            break
        pos = os.lseek(fd, start, os.SEEK_HOLE)
        out.append((start, pos))
    return out


# Pages whose live bytes differ from the image file, measured: RVA 0x2647000 (VA 0x142647000, r--p) differs in 8 bytes
# (live a5d46853a5d46853 at 0x142647C64, file 00f0524700341E48), the same in eight full captures of one launch. Its
# cause is OPEN; it is never served, so a slim replay refuses a read of it. Guard: scratch verify_served.py over full captures.
UNSERVABLE_RVA = frozenset({0x2647000})

# The protected-code section `.impdata` (VA 0x143D14000, 380 MB, rwx in the process) holds the game's relocated code
# (docs/re/pes-re-protection.md §1): byte-identical in the file, the dump and every capture except the pages below, which
# hold state words and self-patched bytes that differ per launch. Measured by comparing the section of pes_full.bin with
# 20 full captures from 10 launches (97342 pages: 31 differ in any capture; 28 to 30 in each). A page in this set is
# never served. Regenerate: tools/pes21_import/volatile_pages.py (CAPTURE_DIR ...).
IMPDATA = b".impdata"
IMPDATA_VOLATILE_RVA = frozenset({0x3DCA000, 0x3F3D000, 0x3FE1000, 0x415B000, 0x445F000, 0x46D4000, 0x494F000, 0x4A92000,
                                  0x4AC3000, 0xDDC9000, 0xE8C2000, 0xEA7A000, 0xEA7B000, 0xED0C000, 0xF0BE000, 0x10542000,
                                  0x17245000, 0x1A893000, 0x1AB9A000, 0x1AE75000, 0x1AEAF000, 0x1B043000, 0x1B427000,
                                  0x1B59B000, 0x1B63C000, 0x1B798000, 0x1B8C6000, 0x1B8FE000, 0x1B928000, 0x1B92A000,
                                  0x1B93C000})


class PeImage:
    """The game image as a file (pes_full.bin: the unpacked image, file offset = VA - IMAGE_BASE). A slim capture drops
    pages; the file supplies the ones that are code or read-only data, and the pages of `.impdata` (the relocated game
    code, rwx in the process) outside IMPDATA_VOLATILE_RVA. Never served: any other writable section (the game writes
    it) and the import address table (the loader writes it with that launch's DLL addresses)."""

    def __init__(self, path):
        self.fd = os.open(path, os.O_RDONLY)
        head = os.pread(self.fd, PAGE, 0)
        e = struct.unpack_from("<I", head, 0x3C)[0]
        if head[e:e + 4] != b"PE\0\0":
            raise SystemExit("%s: not a PE image" % path)
        nsec, = struct.unpack_from("<H", head, e + 6)
        opt_size, = struct.unpack_from("<H", head, e + 20)
        opt = e + 24
        base, = struct.unpack_from("<Q", head, opt + 24)
        if base != IMAGE_BASE:
            raise SystemExit("%s: image base %#x, not %#x" % (path, base, IMAGE_BASE))
        self.size_of_image, = struct.unpack_from("<I", head, opt + 56)
        self.size_of_headers, = struct.unpack_from("<I", head, opt + 60)
        self.iat = struct.unpack_from("<II", head, opt + 112 + 12 * 8)  # data directory 12: the import address table
        self.sections = []  # (first page RVA, end page RVA, characteristics, is .impdata) per section
        for i in range(nsec):
            s = opt + opt_size + 40 * i
            vsize, va = struct.unpack_from("<II", head, s + 8)
            ch, = struct.unpack_from("<I", head, s + 36)
            self.sections.append((va, va + ((max(vsize, 1) + PAGE - 1) & ~(PAGE - 1)), ch,
                                  head[s:s + 8].rstrip(b"\0") == IMPDATA))

    def is_impdata(self, rva):
        return any(lo <= rva < hi and imp for lo, hi, _, imp in self.sections)

    def servable(self, rva):
        """True when the file's page at rva is the page a live capture holds: a header page, or a page of a section that
        is not writable, outside the import address table."""
        page = rva & ~(PAGE - 1)
        if page in UNSERVABLE_RVA:
            return False
        if page < self.size_of_headers:
            return True
        iat_va, iat_size = self.iat
        if iat_size and page < iat_va + iat_size and iat_va < page + PAGE:
            return False
        for lo, hi, ch, imp in self.sections:
            if lo <= page < hi:
                return not (ch & 0x80000000) or imp and page not in IMPDATA_VOLATILE_RVA
        return False

    def read(self, rva, n):
        return os.pread(self.fd, n, rva)


class SnapMem:
    """Read-only view of a snapshot directory: mappings from index.json, bytes from mem.bin.

    A page that a slim capture dropped is a hole of mem.bin. It is never read as zeros. A dropped page the game image
    file serves (PeImage.servable: code or read-only data, not the import table) reads from that file; any other
    dropped page makes page_ok False and read returns None, so the replay refuses the case (page_dropped names it).
    image: the game image file (default: PES_EXE); "" for none. Index holes are unreadable at capture.
    """

    def __init__(self, snap_dir, restore=None, image=None):
        path = image if image is not None else os.environ.get("PES_EXE")
        idx = json.load(open(os.path.join(snap_dir, "index.json")))
        self.index = idx
        self.maps = idx["mappings"]
        self.starts = [m["va"] for m in self.maps]
        self.fd = os.open(os.path.join(snap_dir, "mem.bin"), os.O_RDONLY)
        self._data = _data_extents(self.fd)
        self._data_starts = [s for s, _ in self._data]
        self._exports = {}
        self._mz = None  # sorted 'MZ' mapping VAs (module_bases), scanned on first use
        self.holes = set()
        for va, n in idx["holes"]:
            self.holes.update(range(va & ~(PAGE - 1), va + n, PAGE))
        self.restore = dict(restore or {})  # rendezvous hook entry VA -> the original code it displaced
        self.image = PeImage(path) if path else None
        if self.image is not None and self.page_ok(IMAGE_BASE):
            cap = self._size_of_image(IMAGE_BASE)
            if cap and cap != self.image.size_of_image:
                raise SystemExit("%s is not this capture's game image: SizeOfImage %#x, capture %#x"
                                 % (path, self.image.size_of_image, cap))

    def mapping_of(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0 and va < self.maps[i]["va"] + self.maps[i]["size"]:
            return self.maps[i]
        return None

    def _captured(self, fpage):
        """True when mem.bin holds the whole page at file offset fpage (it is not a hole)."""
        i = bisect.bisect_right(self._data_starts, fpage) - 1
        return i >= 0 and fpage + PAGE <= self._data[i][1]

    def _captured_page(self, page):
        """True when page is mapped, not an index hole, and mem.bin holds its bytes (a slim capture did not drop it)."""
        m = self.mapping_of(page)
        return m is not None and page not in self.holes and self._captured(m["off"] + page - m["va"])

    def _served(self, page):
        """True when page is mapped, dropped by a slim capture, and the game image file holds its bytes: a page of a
        mapping that is not writable in the live process (a writable one may have been written) or of `.impdata`
        (writable in the process, but only its IMPDATA_VOLATILE_RVA pages ever differ), that PeImage.servable."""
        m = self.mapping_of(page)
        return (self.image is not None and IMAGE_BASE <= page < IMAGE_BASE + self.image.size_of_image
                and m is not None and ("w" not in m["perm"] or self.image.is_impdata(page - IMAGE_BASE))
                and page not in self.holes and self.image.servable(page - IMAGE_BASE))

    def page_dropped(self, va):
        """True when va's page is mapped and not an index hole, but a slim capture dropped its bytes and no image serves them."""
        page = va & ~(PAGE - 1)
        return self.mapping_of(page) is not None and page not in self.holes and not self._captured_page(page) and not self._served(page)

    def page_ok(self, page):
        page &= ~(PAGE - 1)
        return self._captured_page(page) or self._served(page)

    def read(self, va, n):
        """Bytes at [va, va+n) if every page is readable (captured, or served from the image file), else None. The range
        lies in one mapping: file offsets are per mapping. A restore entry (a rendezvous hook patch in the snapshot) reads
        as the original code it displaced."""
        if n <= 0:
            return None
        m = self.mapping_of(va)
        if m is None or va + n > m["va"] + m["size"]:  # one mapping only: file offsets are per mapping
            return None
        data = bytearray()
        cur, end = va, va + n
        while cur < end:
            page = cur & ~(PAGE - 1)
            stop = min(end, page + PAGE)
            if self._captured_page(page):
                data += os.pread(self.fd, stop - cur, m["off"] + (cur - m["va"]))
            elif self._served(page):
                data += self.image.read(cur - IMAGE_BASE, stop - cur)
            else:
                return None
            cur = stop
        for rva, code in self.restore.items():
            for i, b in enumerate(code):
                if va <= rva + i < va + n:
                    data[rva + i - va] = b
        return bytes(data)

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

    def module_bases(self):
        """VAs of the snapshot's mappings that begin with a PE header ('MZ'), sorted; scanned on first use."""
        if self._mz is None:
            self._mz = sorted(m["va"] for m in self.maps if self.read(m["va"], 2) == b"MZ")
        return self._mz

    def module_base(self, va):
        """Load base of the PE module whose image holds va, or None. The game image (IMAGE_BASE) is identified by its
        image file, so a slim capture that dropped its header still names it. Any other PE's sections map unnamed (only
        its header page is named), so it is the nearest preceding 'MZ' mapping whose SizeOfImage covers va."""
        if self.image is not None and IMAGE_BASE <= va < IMAGE_BASE + self.image.size_of_image:
            return IMAGE_BASE
        mz = self.module_bases()
        i = bisect.bisect_right(mz, va) - 1
        if i < 0:
            return None
        base = mz[i]
        return base if va < base + self._size_of_image(base) else None

    def _size_of_image(self, base):
        """SizeOfImage of the PE at base; 0 when its headers are not captured or it is not a PE."""
        e = self.read(base + 0x3C, 4)
        pe = struct.unpack("<I", e)[0] if e else 0
        if not pe or self.read(base + pe, 4) != b"PE\0\0":
            return 0
        size = self.read(base + pe + 80, 4)  # OptionalHeader.SizeOfImage
        return struct.unpack("<I", size)[0] if size else 0

    def _exports_of(self, base):
        """(module file name, sorted [(rva, name)]) of the PE at base, read from its mapped headers (cached)."""
        if base not in self._exports:
            self._exports[base] = self._parse_exports(base)
        return self._exports[base]

    def export_of(self, va):
        """(module file name, export name or None, offset from it) for a code address in a loaded PE module of the
        snapshot: the nearest named export at or below va."""
        base = self.module_base(va)
        if base is None:
            return None, None, None
        mod, exports = self._exports_of(base)
        i = bisect.bisect_right(exports, (va - base, "\uffff")) - 1
        if i < 0:
            return mod, None, None
        rva, name = exports[i]
        return mod, name, va - base - rva

    def _parse_exports(self, base):
        """(module file name, sorted [(rva, name)]) of a PE module's named exports, from its in-memory headers. The file
        name is the export directory's Name (the module names itself there); without one it is the mapping's name."""
        def rd(off, n):
            return self.read(base + off, n) or b""
        e_lfanew = struct.unpack("<I", rd(0x3C, 4).ljust(4, b"\0"))[0]
        dd0 = struct.unpack("<I", rd(e_lfanew + 24 + 112, 4).ljust(4, b"\0"))[0]
        ed = rd(dd0, 40) if dd0 else b""
        if len(ed) < 40:
            return self._mapping_name(base), []
        mod = self.cstr(base + struct.unpack_from("<I", ed, 12)[0]) or self._mapping_name(base)
        n_funcs, n_names, f_rva, n_rva, o_rva = struct.unpack_from("<IIIII", ed, 20)
        names, ords, funcs = rd(n_rva, 4 * n_names), rd(o_rva, 2 * n_names), rd(f_rva, 4 * n_funcs)
        if len(names) < 4 * n_names or len(ords) < 2 * n_names or len(funcs) < 4 * n_funcs:
            return mod, []  # the export tables are not captured: no entry is named
        out = []
        for i in range(n_names):
            name_rva = struct.unpack_from("<I", names, 4 * i)[0]
            ordinal = struct.unpack_from("<H", ords, 2 * i)[0]
            fn_rva = struct.unpack_from("<I", funcs, 4 * ordinal)[0]
            out.append((fn_rva, self.cstr(base + name_rva)))
        return mod, sorted(out)

    def _mapping_name(self, base):
        m = self.mapping_of(base)
        return os.path.basename(m["name"]) if m and m["name"] else ""

    def entries_of(self, va):
        """(module file name, [(entry VA, export name)]) of the PE module holding va; (None, []) outside a PE."""
        base = self.module_base(va)
        if base is None:
            return None, []
        mod, exports = self._exports_of(base)
        return mod, [(base + rva, nm) for rva, nm in exports]


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
