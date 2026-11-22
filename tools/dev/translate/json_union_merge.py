#!/usr/bin/env python3
"""json_union_merge.py BASE OURS THEIRS: three-way merge of the translate inputs (results_*.json, closure_*.json,
ro_tables.json), written over OURS. Exit 1 and leave OURS untouched on a real conflict.

Branches add records to the same lists far more often than they edit the same record, and git's line merge turns every such
addition into a conflict that was then resolved by hand (and wrongly: MergeBKBJ 4425ef0f, StallBreaker 55ff8f64). The
inputs are records keyed by "va" and lists of VA strings, so the merge is a keyed union:
  - a list of {"va": ...} records or of strings: each side's additions and removals apply; OURS order first, then THEIRS'
    new entries in their order; one record edited on both sides differently is merged as a dict, field by field;
  - a dict: per key, the side that changed it wins; both changed differently recurses, and scalars changed differently
    on both sides are the only conflict.
Then run gen_translated.py: the layer is regenerated from the merged inputs, never merged by hand.
"""
import json
import sys


class Conflict(Exception):
    pass


def key_of(item):
    if isinstance(item, dict):
        return ("va", item.get("va")) if "va" in item else None
    return ("s", item) if isinstance(item, str) else None


def keyed(items):
    """Records by key; a key repeated in one list (ro_tables.json declares 0x142581374 twice) is told apart by occurrence."""
    keys, seen = [], {}
    for item in items:
        k = key_of(item)
        if k is None:
            return None
        seen[k] = seen.get(k, -1) + 1
        keys.append(k + (seen[k],))
    return dict(zip(keys, items))


def merge(base, ours, theirs, path="$"):
    if ours == theirs:
        return ours
    if base == ours:
        return theirs
    if base == theirs:
        return ours
    if isinstance(ours, dict) and isinstance(theirs, dict):
        base = base if isinstance(base, dict) else {}
        out = {}
        for k in list(ours) + [k for k in theirs if k not in ours]:
            if k in ours and k in theirs:
                out[k] = merge(base.get(k), ours[k], theirs[k], "%s.%s" % (path, k))
            elif k in ours:
                if k not in base or base[k] != ours[k]:
                    out[k] = ours[k]          # added by ours, or edited by ours after theirs removed it
            elif k not in base or base[k] != theirs[k]:
                out[k] = theirs[k]
        return out
    if isinstance(ours, list) and isinstance(theirs, list):
        b, o, t = keyed(base if isinstance(base, list) else []), keyed(ours), keyed(theirs)
        if b is not None and o is not None and t is not None:
            out = []
            for k, v in o.items():
                if k in t:
                    out.append(merge(b.get(k), v, t[k], "%s[%s]" % (path, k[1])))
                elif k not in b or b[k] != v:
                    out.append(v)             # theirs removed an entry ours did not touch: keep only if ours changed it
            out += [v for k, v in t.items() if k not in o and (k not in b or b[k] != v)]
            return out
    raise Conflict("%s: both sides changed it differently" % path)


def dumps(doc, like):
    """Write doc the way its file is written: the form (record per line, or indent=1 with or without sorted keys, with or
    without a final newline) that reproduces the OURS text exactly. ro_tables.json is one compact record per line;
    universe.json is sorted with no final newline; the rest are indent=1 in insertion order."""
    if like.startswith("[\n  {"):
        return "[\n  " + ",\n  ".join(json.dumps(e) for e in doc) + "\n]\n"
    old = json.loads(like)
    for sort in (True, False):          # sorted first: a sorted file also reproduces unsorted, and new keys must sort in
        text = json.dumps(old, indent=1, sort_keys=sort)
        for end in ("\n", ""):
            if text + end == like:
                return json.dumps(doc, indent=1, sort_keys=sort) + end
    raise Conflict("unrecognised JSON layout; merge it by hand")


def main(base_path, ours_path, theirs_path):
    texts = [open(p).read() for p in (base_path, ours_path, theirs_path)]
    docs = [json.loads(t) if t.strip() else None for t in texts]
    try:
        out = merge(*docs)
        text = dumps(out, texts[1])
    except Conflict as e:
        print("json_union_merge: %s: %s" % (ours_path, e), file=sys.stderr)
        return 1
    with open(ours_path, "w") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
