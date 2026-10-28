#!/usr/bin/env python3
"""slim.py: trim rendezvous captures to the pages their replay reads, verified before the original goes.

Usage: python3 tools/dev/rendezvous/slim.py --hooks HOOKS_CFG [--spec-dir DIR] [--min-age 600] [--only DIR ...]
       [--work DIR] [CAPTURE_DIR ...]
Default captures: every seq dir (with index.json, call.bin and mem.bin) of <gf-Rendezvous-scratch>/live and live2
that is older than --min-age seconds (the watcher writes new ones). watch.log and pauses.jsonl are never touched.
Per capture:
  1. Replay its recorded call (frame_trace.py recorded --hooks --out --spec-dir) with every pread of its mem.bin
     logged (4 KiB file pages). That set is the replay's read set; the replay must return cleanly.
  2. Keep = the read set plus one page either side. Write a sparse mem.bin of the same size holding only the kept
     pages (a hole reads as zeros; offsets and size are unchanged, so every loader reads it as before).
  3. Replay the slim capture (same path). Verified only if it reads no page outside the kept set, returns cleanly,
     and its output directory and stdout are byte-identical to step 1. Then the full mem.bin is deleted. Any other
     outcome restores the full mem.bin: the capture stays full and is reported.
Work outputs go to --work (default <gf-Rendezvous-scratch>/slim-work), one subdirectory per capture.
"""
import argparse
import contextlib
import hashlib
import io
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import frame_trace as ft  # noqa: E402

PAGE = 0x1000
DEFAULT_SPECS = os.path.join(HERE, "..", "diffharness", "specs", "impure")
_orig_pread = os.pread
_touch = None  # file pages of mem.bin read by the current replay (None: not recording)


def _is_mem_fd(fd):
    try:
        return os.readlink("/proc/self/fd/%d" % fd).endswith("/mem.bin")
    except OSError:
        return False


def _logged_pread(fd, n, off):
    if _touch is not None and n > 0 and _is_mem_fd(fd):
        _touch.update(range(off & ~(PAGE - 1), off + n, PAGE))
    return _orig_pread(fd, n, off)


def replay(cap, out, hooks, spec_dir):
    """Replay cap's recorded call into out. Returns (stdout, clean return, file pages read)."""
    global _touch
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    buf, clean, touched = io.StringIO(), False, set()
    _touch = touched
    os.pread = _logged_pread
    try:
        with contextlib.redirect_stdout(buf):
            try:
                ft.recorded_main([cap, "--hooks", hooks, "--out", out, "--spec-dir", spec_dir])
                clean = True
            except SystemExit as e:
                clean = e.code in (0, None)
    finally:
        os.pread = _orig_pread
        _touch = None
    return buf.getvalue(), clean, touched


def tree_hash(root):
    h = hashlib.sha256()
    for dp, dn, fn in os.walk(root):
        dn.sort()
        for name in sorted(fn):
            p = os.path.join(dp, name)
            h.update(os.path.relpath(p, root).encode() + b"\0")
            with open(p, "rb") as f:
                h.update(hashlib.sha256(f.read()).digest())
    return h.hexdigest()


def disk_bytes(path):
    return os.stat(path).st_blocks * 512


def kept_pages(touched, size):
    keep = set()
    for p in touched:
        for q in (p - PAGE, p, p + PAGE):
            if 0 <= q < size:
                keep.add(q)
    return keep


def sparse_copy(src, dst, keep):
    """A file of src's size holding only the kept pages (the rest is a hole that reads as zeros)."""
    size = os.path.getsize(src)
    with open(src, "rb") as fi, open(dst, "wb") as fo:
        fo.truncate(size)
        for p in sorted(keep):
            fi.seek(p)
            data = fi.read(min(PAGE, size - p))
            fo.seek(p)
            fo.write(data)


def trim(cap, hooks, spec_dir, work):
    """-> (status, before bytes, after bytes). status: trimmed | kept full: <reason>."""
    mem = os.path.join(cap, "mem.bin")
    full_aside = os.path.join(cap, "mem.full.tmp")
    slim_tmp = os.path.join(cap, "mem.slim.tmp")
    before = disk_bytes(mem)
    w = os.path.join(work, os.path.basename(cap.rstrip("/")))
    out1, clean1, t1 = replay(cap, os.path.join(w, "full"), hooks, spec_dir)
    if not clean1:
        return "kept full: recorded replay did not return cleanly", before, before
    keep = kept_pages(t1, os.path.getsize(mem))
    sparse_copy(mem, slim_tmp, keep)
    os.replace(mem, full_aside)
    os.replace(slim_tmp, mem)
    try:
        out2, clean2, t2 = replay(cap, os.path.join(w, "slim"), hooks, spec_dir)
        ok = clean2 and t2 <= keep and out2 == out1 and tree_hash(os.path.join(w, "full")) == tree_hash(os.path.join(w, "slim"))
        reason = "" if ok else ("slim replay: clean=%s reads_outside_kept=%d output_equal=%s"
                                % (clean2, len(t2 - keep), out2 == out1))
    except Exception as e:  # noqa: BLE001 - any failure restores the full capture
        ok, reason = False, "slim replay raised %r" % (e,)
    if ok:
        os.remove(full_aside)
        return "trimmed (%d pages read, %d kept)" % (len(t1), len(keep)), before, disk_bytes(mem)
    os.remove(mem)
    os.replace(full_aside, mem)
    return "kept full: " + reason, before, before


def default_captures(scratch, min_age):
    now, out = time.time(), []
    for d in ("live", "live2"):
        root = os.path.join(scratch, d)
        for name in sorted(os.listdir(root)):
            p = os.path.join(root, name)
            if not (name.startswith("seq") and os.path.isdir(p)):
                continue
            if not all(os.path.exists(os.path.join(p, f)) for f in ("mem.bin", "index.json", "call.bin")):
                continue
            if now - os.path.getmtime(p) < min_age:
                continue
            out.append(p)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("captures", nargs="*")
    ap.add_argument("--hooks", required=True, help="the rv.dll hook config the captures were taken with")
    ap.add_argument("--scratch", help="the capture root holding live/ and live2/ (default when no captures are named)")
    ap.add_argument("--spec-dir", default=DEFAULT_SPECS)
    ap.add_argument("--min-age", type=float, default=600.0)
    ap.add_argument("--work", help="scratch dir for replay outputs (default: SCRATCH/slim-work)")
    args = ap.parse_args()
    if not args.captures and not args.scratch:
        ap.error("name the captures or give --scratch")
    caps = args.captures or default_captures(args.scratch, args.min_age)
    args.work = args.work or os.path.join(args.scratch or os.path.dirname(caps[0].rstrip("/")), "slim-work")
    os.makedirs(args.work, exist_ok=True)
    tot_before = tot_after = 0
    for cap in caps:
        status, b, a = trim(cap, args.hooks, args.spec_dir, args.work)
        tot_before += b
        tot_after += a
        print("%s %s: %.2f GB -> %.2f GB" % (os.path.basename(cap.rstrip("/")), status, b / 1e9, a / 1e9), flush=True)
    print("total: %.2f GB -> %.2f GB over %d captures" % (tot_before / 1e9, tot_after / 1e9, len(caps)))


if __name__ == "__main__":
    main()
