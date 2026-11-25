#!/usr/bin/env python3
"""slim.py: footprint-slim rendezvous captures. A full original leaves live2 only once its slim copy is verified and
its full copy is archived; nothing is deleted.

Usage: GF_CAPTURE_ARCHIVE=<archive root> python3 tools/dev/rendezvous/slim.py --hooks HOOKS_CFG [--spec-dir DIR]
           [--min-age 600] [--work DIR] [CAPTURE_DIR ...]
       python3 tools/dev/rendezvous/slim.py --restore CAPTURE_DIR ...
Default captures: every seq dir (with index.json, call.bin and mem.bin) of <scratch>/live and live2 older than --min-age
seconds (the watcher writes new ones). watch.log and pauses.jsonl are never touched. Per capture, under the capture's
capz lock (auto_slim and agents pack or unpack the same dir):
  1. Footprint: replay the recorded call with the GF stand-ins off (frame_trace.py recorded --no-stubs). PES's own image
     code runs from the hook entry to its return, and every 4 KiB page of mem.bin it reads is logged. The replay must
     return cleanly: an unmodelled import, a fault or the step cap keeps the capture full.
  2. Keep = that read set plus the identity pages (identity_reads), each with a neighbour. mem.bin becomes a sparse file
     of the same size holding only the kept pages.
  3. Replay the slim copy. Verified only if it reads no page outside the kept set, returns cleanly, and its output and
     trace are byte-identical to step 1. Otherwise the full mem.bin is put back and the capture stays full.
  4. index.json "slim" records every dropped range as [VA, bytes] and the archive-relative path of the full copy.
  5. The full mem.bin is packed (zstd -3); the packed file is copied to ARCHIVE/<launch>/<seq>/mem.bin.zst and verified
     byte for byte. Then the packed name in the capture becomes a link to that copy, and the full mem.bin is removed.
A failure in steps 3 to 5 puts the full mem.bin and index.json back: the capture stays full and is reported.
--restore unpacks the full copy back into mem.bin (through the link) and drops the "slim" record.
"""
import argparse
import contextlib
import filecmp
import io
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "translate"))
import capz  # noqa: E402
import frame_trace as ft  # noqa: E402
import full_snapshot as fs  # noqa: E402

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
    """Replay cap's recorded call with the GF stand-ins off into out. -> (stdout, returned cleanly, file pages read)."""
    global _touch
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    buf, touched = io.StringIO(), set()
    _touch = touched
    os.pread = _logged_pread
    try:
        with contextlib.redirect_stdout(buf):
            rc = ft.recorded_main([cap, "--hooks", hooks, "--out", out, "--spec-dir", spec_dir, "--no-stubs"])
    finally:
        os.pread = _orig_pread
        _touch = None
    return buf.getvalue(), rc == 0, touched


def tree_hash(root):
    import hashlib
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


def identity_reads(cap):
    """File pages of cap's mem.bin that naming its modules reads: every mapping's first page (the 'MZ' scan), and each
    PE's header and export tables. A replay that runs only a module's code never reads these, so a slim built from the
    replay alone drops them, and the slimmed module names nothing."""
    global _touch
    sm = fs.SnapMem(cap)
    touched = set()
    _touch = touched
    os.pread = _logged_pread
    try:
        for base in sm.module_bases():
            _, exports = sm.entries_of(base)
            for entry, _ in exports:  # an export's page is replaced by its import model, which is mapped only if it exists
                sm.read(entry, 1)
    finally:
        os.pread = _orig_pread
        _touch = None
    return touched


def keep_for(cap, touched, size):
    """The pages a slim of cap keeps: the replay's read set and the identity pages, each with a neighbour."""
    return kept_pages(set(touched) | identity_reads(cap), size)


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


def dropped_ranges(index, size, keep):
    """[VA, bytes] runs of the mapped pages a slim keeps no bytes of: every page of a mapping that is not an index hole
    and whose file offset is outside keep."""
    holes = set()
    for va, n in index.get("holes", []):
        holes.update(range(va & ~(PAGE - 1), va + n, PAGE))
    runs = []
    for m in sorted(index["mappings"], key=lambda m: m["va"]):
        for i in range(0, m["size"], PAGE):
            va, off = m["va"] + i, m["off"] + i
            if off >= size or off in keep or va in holes:
                continue
            if runs and runs[-1][0] + runs[-1][1] == va:
                runs[-1][1] += PAGE
            else:
                runs.append([va, PAGE])
    return runs


def write_atomic(path, text):
    tmp = path + ".part"
    with open(tmp, "w") as f:
        f.write(text)
    os.replace(tmp, path)


def archive_rel(cap):
    """The archive-relative path of a capture's full copy: <launch>/<seq>/mem.bin.zst."""
    cap = os.path.abspath(cap)
    return os.path.join(os.path.basename(os.path.dirname(cap)), os.path.basename(cap), "mem.bin.zst")


def archive_full(cap, aside, archive):
    """Pack the full mem.bin (aside), copy the packed file into the archive and verify it byte for byte. Then the packed
    name in cap becomes a link to the archived copy, and the full mem.bin is removed. Nothing is removed before the copy
    is verified."""
    packed = os.path.join(cap, "mem.bin.zst")
    tmp = packed + ".part"
    subprocess.run(["zstd", "-q", "-f", "-T0", "-3", aside, "-o", tmp], check=True)
    subprocess.run(["zstd", "-q", "-t", tmp], check=True)
    dst = os.path.join(archive, archive_rel(cap))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(tmp, dst + ".part")
    if not filecmp.cmp(tmp, dst + ".part", shallow=False):
        raise OSError("the archived copy differs from the packed copy")
    os.replace(dst + ".part", dst)
    os.remove(tmp)
    os.symlink(dst, packed)
    os.remove(aside)


def trim(cap, hooks, spec_dir, work, archive):
    """-> (status, before bytes, after bytes). status: trimmed | kept full: <reason> | skipped: <reason>."""
    mem = os.path.join(cap, "mem.bin")
    index_path = os.path.join(cap, "index.json")
    aside, slim_tmp = os.path.join(cap, "mem.full"), os.path.join(cap, "mem.slim.tmp")
    before = disk_bytes(mem)
    with open(index_path) as f:
        index_text = f.read()
    index = json.loads(index_text)
    if "slim" in index:
        return "skipped: already slim", before, before
    if os.path.lexists(os.path.join(cap, "mem.bin.zst")):
        return "skipped: a packed full copy already exists", before, before
    w = os.path.join(work, os.path.basename(os.path.abspath(cap)))
    try:
        out1, clean1, t1 = replay(cap, os.path.join(w, "full"), hooks, spec_dir)
    except Exception as e:  # noqa: BLE001 - a footprint run that raises keeps the capture full
        return "kept full: footprint replay raised %r" % (e,), before, before
    if not clean1:
        return "kept full: footprint replay did not return cleanly", before, before
    size = os.path.getsize(mem)
    keep = keep_for(cap, t1, size)
    sparse_copy(mem, slim_tmp, keep)
    os.replace(mem, aside)
    os.replace(slim_tmp, mem)
    try:
        out2, clean2, t2 = replay(cap, os.path.join(w, "slim"), hooks, spec_dir)
        ok = (clean2 and t2 <= keep and out2 == out1
              and tree_hash(os.path.join(w, "full")) == tree_hash(os.path.join(w, "slim")))
        reason = "" if ok else ("slim replay: clean=%s reads_outside_kept=%d output_equal=%s"
                                % (clean2, len(t2 - keep), out2 == out1))
    except Exception as e:  # noqa: BLE001
        ok, reason = False, "slim replay raised %r" % (e,)
    if not ok:
        os.remove(mem)
        os.replace(aside, mem)
        return "kept full: " + reason, before, before
    index["slim"] = {"mode": "footprint", "kept_pages": len(keep), "read_pages": len(t1),
                     "dropped": dropped_ranges(index, size, keep), "full": archive_rel(cap)}
    write_atomic(index_path, json.dumps(index))
    try:
        archive_full(cap, aside, archive)
    except Exception as e:  # noqa: BLE001 - the full copy stays where it was until the archive holds a verified copy
        for p in (os.path.join(cap, "mem.bin.zst"), os.path.join(cap, "mem.bin.zst.part")):
            if os.path.islink(p) or os.path.isfile(p):
                os.remove(p)
        if os.path.exists(aside):
            os.remove(mem)
            os.replace(aside, mem)
        write_atomic(index_path, index_text)
        return "kept full: archive failed (%r)" % (e,), before, before
    return ("trimmed (%d pages read, %d kept, %d dropped ranges)"
            % (len(t1), len(keep), len(index["slim"]["dropped"])), before, disk_bytes(mem))


def restore(cap):
    """Bring a slimmed capture's full copy back: unpack it through the archive link into mem.bin, then drop the slim
    record. -> 'unpacked' or 'skip' (already full)."""
    index_path = os.path.join(cap, "index.json")
    with open(index_path) as f:
        index = json.load(f)
    status = capz.locked(capz.unpack, cap)
    if status == "skip" and "slim" in index:
        raise SystemExit("%s: no packed full copy to restore from (mem.bin.zst)" % cap)
    if "slim" in index:
        del index["slim"]
        write_atomic(index_path, json.dumps(index))
    return status


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
    ap.add_argument("--hooks", help="the rv.dll hook config the captures were taken with")
    ap.add_argument("--scratch", help="the capture root holding live/ and live2/ (default when no captures are named)")
    ap.add_argument("--spec-dir", default=DEFAULT_SPECS)
    ap.add_argument("--min-age", type=float, default=600.0)
    ap.add_argument("--work", help="scratch dir for replay outputs (default: SCRATCH/slim-work)")
    ap.add_argument("--archive", default=os.environ.get(fs.ARCHIVE_ENV),
                    help="archive root for the full copies (default: $%s)" % fs.ARCHIVE_ENV)
    ap.add_argument("--restore", action="store_true", help="restore the named captures' full copies")
    args = ap.parse_args()
    if args.restore:
        for cap in args.captures:
            print(os.path.basename(cap.rstrip("/")), capz.locked(lambda d: restore(d), cap), flush=True)
        return
    if not args.hooks:
        ap.error("--hooks is required to slim")
    if not args.archive:
        ap.error("no archive root: set $%s or give --archive (a slim never deletes a full copy)" % fs.ARCHIVE_ENV)
    if not args.captures and not args.scratch:
        ap.error("name the captures or give --scratch")
    caps = args.captures or default_captures(args.scratch, args.min_age)
    args.work = args.work or os.path.join(args.scratch or os.path.dirname(caps[0].rstrip("/")), "slim-work")
    os.makedirs(args.work, exist_ok=True)
    tot_before = tot_after = 0
    for cap in caps:
        status, b, a = capz.locked(
            lambda d: trim(d, args.hooks, args.spec_dir, args.work, args.archive), cap)
        tot_before += b
        tot_after += a
        print("%s %s: %.2f GB -> %.2f GB" % (os.path.basename(cap.rstrip("/")), status, b / 1e9, a / 1e9), flush=True)
    print("total: %.2f GB -> %.2f GB over %d captures" % (tot_before / 1e9, tot_after / 1e9, len(caps)))


if __name__ == "__main__":
    main()
