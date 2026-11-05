#!/usr/bin/env python3
"""buildcache.py: the two caches the diff driver reuses between runs. A hit must be exactly what the uncached step gives.

Object cache (driver.build_and_diff): one object per translation unit. The key is sha256 of the compiler identity, the
flags, the TU's name and bytes, and the bytes of every build-tree header the TU includes, transitively (include_closure).
Identical TUs in different VAs' build trees share one object.

Corpus cache (driver.run_one): run.py's outputs for one VA. The key is its inputs: the digests of the image, the spec,
the seed artifact and bank for that VA, the harness interpreter, and the flags. An entry is reused only while every file
run.py loaded still has the digest it had when the entry was made (record_run: an audit hook on open and import, plus
the libraries the process maps), and every value run.py took from xlate (capstone_decode per call, CCMAP) still
computes the same. xlate.py is not a file dependency: run.py reads it only through those values, so a translator edit
that changes neither value hits.

CLI: buildcache.py record DEPS_JSON RUN_PY ARGS...   (run.py under the recorder; its stdout and exit code pass through)

ponytail: the include closure is a regex scan, not the preprocessor. It keeps every #include line, including ones under
#if 0 (over-keys, never under-keys), and it refuses (None, uncached) a quoted include it cannot resolve. Upgrade: take
the header set from the compiler's -MD depfile of the same TU.
"""
import hashlib
import json
import os
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
# run.py reaches the translator only through xlate.capstone_decode and xlate.CCMAP (both recorded by value and recomputed
# on reuse), so xlate.py, and the flow.py and pdata.py modules it imports, are not file dependencies.
VALUE_COVERED = frozenset(os.path.join(HERE, n) for n in ("xlate.py", "flow.py", "pdata.py"))
CHUNK_BYTES = 1 << 20
# A file whose mtime is this close to the moment we read it may be rewritten in the same clock tick (Linux file times
# come from a coarse clock), so its digest is recomputed, never memoized (the racy-timestamp rule of git's index).
RACY_NS = 2 * 10 ** 9
INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*(["<])([^">]+)[">]', re.M)

_LOCK = threading.Lock()
_DIGESTS = {}
_IDENTITIES = {}


def sha(text):
    return hashlib.sha256(text.encode("utf8", "surrogateescape")).hexdigest()


def file_digest(path):
    """sha256 of a file's bytes, or None when it does not exist. Memoized per (path, size, mtime) for this process, so a
    441 MB image is read once per run, and a file rewritten since (new mtime) is read again."""
    try:
        st = os.stat(path)
    except OSError:
        return None
    key = (os.path.abspath(path), st.st_size, st.st_mtime_ns)
    with _LOCK:
        hit = _DIGESTS.get(key)
    if hit is not None:
        return hit
    started = time.time_ns()
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(CHUNK_BYTES), b""):
                h.update(chunk)
    except FileNotFoundError:
        return None
    digest = h.hexdigest()
    if st.st_mtime_ns < started - RACY_NS:
        with _LOCK:
            _DIGESTS[key] = digest
    return digest


def _identity(key, probe):
    with _LOCK:
        if key in _IDENTITIES:
            return _IDENTITIES[key]
    ident = probe()
    with _LOCK:
        _IDENTITIES[key] = ident
    return ident


def compiler_id(cxx):
    """The compiler's identity: its real path and its version line."""
    def probe():
        version = subprocess.run([cxx, "--version"], capture_output=True, text=True, check=True).stdout.splitlines()[0]
        return "%s | %s" % (os.path.realpath(shutil.which(cxx) or cxx), version)
    return _identity(("cxx", cxx), probe)


def interpreter_id(py):
    """The harness interpreter's identity: its real path and its version."""
    def probe():
        version = subprocess.run([py, "-c", "import sys; print(sys.version)"], capture_output=True, text=True,
                                 check=True).stdout.strip()
        return "%s | %s" % (os.path.realpath(py), version)
    return _identity(("py", py), probe)


def include_closure(tu_path, include_dir):
    """[[name, digest]] of every header the TU includes from include_dir, transitively, sorted by name. A quoted include
    is looked up beside the including file, then in include_dir; an angle include only in include_dir (any other angle
    include is a system header, which the compiler identity covers). None when a quoted include resolves to nothing in
    include_dir, or to a file outside it: the TU is then compiled uncached."""
    include_dir = os.path.abspath(include_dir)
    found = {}
    stack = [os.path.abspath(tu_path)]
    while stack:
        path = stack.pop()
        with open(path, "rb") as f:
            text = f.read().decode("latin-1")
        for kind, name in INCLUDE_RE.findall(text):
            dirs = ([os.path.dirname(path)] if kind == '"' else []) + [include_dir]
            hit = next((os.path.join(d, name) for d in dirs if os.path.isfile(os.path.join(d, name))), None)
            if hit is None:
                if kind == '"':
                    return None
                continue
            rel = os.path.relpath(os.path.abspath(hit), include_dir)
            if rel.startswith(os.pardir + os.sep) or os.path.isabs(rel):
                return None
            if rel not in found:
                found[rel] = os.path.abspath(hit)
                stack.append(found[rel])
    return sorted([rel, file_digest(p)] for rel, p in found.items())


def object_key(compiler, flags, tu_name, tu_digest, headers):
    doc = {"v": 1, "compiler": compiler, "flags": list(flags), "tu": tu_name, "tu_sha": tu_digest, "headers": headers}
    return sha(json.dumps(doc, sort_keys=True))


def tu_object_key(tu_path, compiler, flags, include_dir):
    """The object-cache key of one translation unit, or None when it is not cacheable (see include_closure)."""
    headers = include_closure(tu_path, include_dir)
    if headers is None:
        return None
    with open(tu_path, "rb") as f:
        tu = hashlib.sha256(f.read()).hexdigest()
    return object_key(compiler, flags, os.path.basename(tu_path), tu, headers)


def corpus_key(run_args, inputs):
    """An entry's key: run.py's flags (paths are inputs, by digest, in inputs) and the digests of its inputs."""
    return sha(json.dumps({"v": 2, "args": list(run_args), "inputs": inputs}, sort_keys=True))


# The corpus records: one directory per key (corpus_key), holding one record per harness state: the run.py files, stubs and
# translator tables that record validates against. Checkouts whose harness files differ share a key, so a record is never
# overwritten by another state's. corpus_trim keeps the whole directory under CORPUS_CAP_BYTES.
CORPUS_DIR = "records"
CORPUS_CAP_BYTES = 10 * 2 ** 30  # ponytail: one trim walk per driver process, at start; a run's own records may exceed the cap until the next.


def _read_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except OSError:
        return None


def _touch(path):
    now = time.time_ns()
    os.utime(path, ns=(now, now))


def _records(root):
    """The record directories under root (a record holds run.json), most recently used first."""
    try:
        names = os.listdir(root)
    except OSError:
        return []
    found = []
    for name in names:
        try:
            found.append((os.stat(os.path.join(root, name, "run.json")).st_mtime_ns, os.path.join(root, name)))
        except OSError:  # not a record, or evicted since it was listed
            continue
    return [d for _, d in sorted(found, reverse=True)]


def corpus_lookup(root, gen):
    """The first record under root that still holds in this checkout, most recently used first: its outputs are copied into
    gen, it is touched as recently used, and its document (run.py's stdout as "out") is returned. None when none holds."""
    for record in _records(root):
        doc = _read_json(os.path.join(record, "run.json"))
        if doc is None or not entry_is_valid(doc["deps"]):
            continue
        try:
            for name in doc["files"]:
                shutil.copyfile(os.path.join(record, name), os.path.join(gen, name))
        except OSError:  # evicted by another run since it was listed: a miss
            continue
        _touch(os.path.join(record, "run.json"))
        return doc
    return None


def corpus_store(root, out, names, gen, record):
    """Commit a successful run under root as the record of its harness state: its outputs (copied from gen) and its
    validation document, stored under the digest of that document. run.json is written last, so its presence commits the
    record. The same state recorded again replaces its own record."""
    os.makedirs(root, exist_ok=True)
    deps = entry_doc(record)
    tmp = tempfile.mkdtemp(dir=root)
    for name in names:
        shutil.copyfile(os.path.join(gen, name), os.path.join(tmp, name))
    with open(os.path.join(tmp, "run.json"), "w") as f:
        json.dump({"out": out, "files": names, "deps": deps}, f)
    _touch(os.path.join(tmp, "run.json"))
    dest = os.path.join(root, sha(json.dumps(deps, sort_keys=True)))
    shutil.rmtree(dest, ignore_errors=True)
    try:
        os.replace(tmp, dest)
    except OSError:  # another run committed this state first (shared cache): its record is the same, keep it
        shutil.rmtree(tmp, ignore_errors=True)


def corpus_trim(corpus, cap=CORPUS_CAP_BYTES):
    """Evict the least recently used records, across every key, until the corpus holds at most cap bytes."""
    try:
        keys = os.listdir(corpus)
    except OSError:
        return
    found = []  # (last use, bytes, directory)
    for key in keys:
        for record in _records(os.path.join(corpus, key)):
            try:
                size = sum(os.path.getsize(os.path.join(record, n)) for n in os.listdir(record))
                last = os.stat(os.path.join(record, "run.json")).st_mtime_ns
            except OSError:  # evicted by another run meanwhile
                continue
            found.append((last, size, record))
    total = sum(size for _, size, _ in found)
    for _, size, record in sorted(found):
        if total <= cap:
            break
        shutil.rmtree(record, ignore_errors=True)
        total -= size


def decode_digest(image_path, va):
    """Digest of xlate.capstone_decode(image, va): one translator value run.py reads."""
    import xlate
    return sha(repr(xlate.capstone_decode(image_path, va)))


def translator_digest():
    """Digest of xlate.CCMAP: the other translator value run.py reads."""
    import xlate
    return sha(repr(sorted(xlate.CCMAP.items())))


def _abs(path):
    return os.path.abspath(os.fsdecode(path))


def _excluded(path):
    return path.endswith(".pyc") or path in VALUE_COVERED or path.startswith(("/proc/", "/dev/"))


def _mapped_files():
    """The files this process maps: the interpreter and the shared libraries it loaded."""
    out = set()
    try:
        with open("/proc/self/maps") as f:
            for line in f:
                parts = line.split(None, 5)
                if len(parts) == 6 and parts[5].startswith("/"):
                    out.add(parts[5].strip())
    except OSError:
        pass
    return out


def record_run(deps_path, run_py, run_args):
    """Run run.py as __main__ with argv run_args and write what it read to deps_path (JSON): every file it opened for
    reading (a failed open too), every module loaded (its source, even when bytecode was used), the libraries mapped, and
    every capstone_decode it asked xlate for (image, va, digest of the result). Its stdout and stderr match a direct
    `python run.py` run: sys.exit(text) prints the text, an uncaught exception prints its traceback from run.py's frame.
    Returns run.py's exit status."""
    files, decode = set(), []

    def hook(event, args):
        if event == "open":
            path, mode = args[0], (args[1] if len(args) > 1 else None)
            if isinstance(path, (str, bytes, os.PathLike)) and not (isinstance(mode, str) and set(mode) & set("wax+")):
                files.add(_abs(path))

    sys.addaudithook(hook)
    import xlate
    real_decode = xlate.capstone_decode

    def recording_decode(image_path, va):
        out = real_decode(image_path, va)
        decode.append((_abs(image_path), va, sha(repr(out))))
        return out

    xlate.capstone_decode = recording_decode
    sys.path[0] = os.path.dirname(os.path.abspath(run_py))
    sys.argv = [run_py] + list(run_args)
    rc = 0
    try:
        runpy.run_path(run_py, run_name="__main__")
    except SystemExit as e:
        if e.code is None or isinstance(e.code, int):
            rc = e.code or 0
        else:
            print(e.code, file=sys.stderr)
            rc = 1
    except BaseException as e:  # noqa: BLE001 - re-printed as the interpreter would, minus this wrapper's frames
        import traceback
        tb = e.__traceback__
        while tb is not None and tb.tb_frame.f_code.co_filename != run_py:
            tb = tb.tb_next
        traceback.print_exception(type(e), e, tb)
        rc = 1
    finally:
        files |= _mapped_files()
        files |= {_abs(m.__file__) for m in list(sys.modules.values()) if isinstance(getattr(m, "__file__", None), str)}
        with open(deps_path, "w") as f:
            json.dump({"files": sorted(p for p in files if not _excluded(p)),
                       "decode": [list(t) for t in sorted(set(decode), key=repr)]}, f, indent=1, sort_keys=True)
    return rc


REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
REPO_TAG = "<repo>/"


def _portable(path):
    """A recorded path as stored: under this checkout, relative to it (so another worktree validates its own copy of the
    file, never this one's); anything else (the image, banks, system libraries) absolute."""
    return REPO_TAG + os.path.relpath(path, REPO) if path.startswith(REPO + os.sep) else path


def _resolve(path):
    return os.path.join(REPO, path[len(REPO_TAG):]) if path.startswith(REPO_TAG) else path


def entry_doc(record):
    """The validation document of a recorded run, taken once it has finished: each file's digest (None: absent), the
    decoded values, and the translator table's digest. Files inside the checkout are stored relative to it."""
    return {"files": {_portable(p): file_digest(p) for p in record["files"]}, "decode": record["decode"],
            "ccmap": translator_digest()}


def entry_is_valid(doc):
    """True while every recorded file (resolved against THIS checkout) has its digest, every recorded decode recomputes,
    and the translator table is unchanged. A recorded input that cannot be recomputed is a miss, never a hit."""
    try:
        if any(file_digest(_resolve(p)) != d for p, d in doc["files"].items()):
            return False
        if any(decode_digest(image, va) != d for image, va, d in doc["decode"]):
            return False
        return translator_digest() == doc["ccmap"]
    except Exception:  # noqa: BLE001 - any failure to recompute a recorded input is a miss
        return False


def main(argv):
    if len(argv) < 3 or argv[0] != "record":
        sys.exit("usage: buildcache.py record DEPS_JSON RUN_PY ARGS...")
    return record_run(argv[1], argv[2], argv[3:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
