#!/usr/bin/env python3
"""flow.py: structured control flow for translated PES functions.

The translator emits each function as a flat list: statements, labels
(`L_<VA>:;`), `if (s.cond("cc")) goto L;`, `goto L;` and `return;`. structure()
rebuilds that list as a tree of if/else over the same statements. Where two arms
reach one block, the block is copied into each arm (tail duplication). Every
path runs the same statements and evaluates the same conditions in the same
order. A function with a loop, a computed jump, or a control form not modelled
here raises Unstructurable and keeps its gotos.

  python3 flow.py --check            self-check: random CFGs, flat vs structured traces
  python3 flow.py --measure ROOT     how many committed functions structure
"""
import argparse
import glob
import os
import random
import re
import sys

LABEL = re.compile(r"^(L_\w+):;$")
IF = re.compile(r'^if \(s\.cond\("(\w+)"\)\) goto (L_\w+);$')
GOTO = re.compile(r"^goto (L_\w+);$")
CONTROL = re.compile(r"\b(goto|return|switch|case|default)\b|\bL_\w+")
ENTRY, EXIT = "ENTRY", "EXIT"
COPIES = 4  # emitted blocks may reach this many times the function's own block count


class Unstructurable(Exception):
    """A loop or a control form the structurer does not model: the function keeps its gotos."""


def parse(lines):
    """Flat items from body lines. Statement text keeps its relative indentation."""
    items = []
    for raw in lines:
        text = raw.strip()
        if not text:
            continue
        if m := LABEL.match(text):
            items.append(("label", m.group(1)))
        elif m := IF.match(text):
            items.append(("if", m.group(1), m.group(2)))
        elif m := GOTO.match(text):
            items.append(("goto", m.group(1)))
        elif text == "return;":
            items.append(("ret",))
        elif CONTROL.search(text):
            raise Unstructurable("control form not modelled: " + text)
        else:
            items.append(("stmt", raw[2:] if raw.startswith("  ") else raw))
    return items


def build(items):
    """Basic blocks: {id: {"stmts": [...], "term": ...}}. A term is ("goto", B), ("cond", cc, taken, fall), ("ret",)."""
    blocks = {}
    anon = [0]

    def new(bid):
        if bid in blocks:
            raise Unstructurable("duplicate label %s" % (bid,))
        blocks[bid] = {"stmts": [], "term": None}
        return bid

    def fresh(kind):
        anon[0] += 1
        return (kind, anon[0])

    cur = new(ENTRY)
    for it in items:
        if it[0] == "label":
            if blocks[cur]["term"] is None:
                blocks[cur]["term"] = ("goto", it[1])  # falls through into the label
            cur = new(it[1])
            continue
        if blocks[cur]["term"] is not None:  # after a jump or return: unreachable unless a label follows
            cur = new(fresh("dead"))
        if it[0] == "stmt":
            blocks[cur]["stmts"].append(it[1])
        elif it[0] == "if":
            fall = new(fresh("ft"))
            blocks[cur]["term"] = ("cond", it[1], it[2], fall)
            cur = fall
        elif it[0] == "goto":
            blocks[cur]["term"] = ("goto", it[1])
        else:  # ret
            blocks[cur]["term"] = ("ret",)
    if blocks[cur]["term"] is None:
        blocks[cur]["term"] = ("ret",)
    return blocks


def succs(blocks, b):
    t = blocks[b]["term"]
    if t[0] == "goto":
        return [t[1]]
    if t[0] == "cond":
        return [t[2], t[3]]
    return []


def order(blocks):
    """Reachable blocks in DFS postorder (successors first). Raises Unstructurable on a loop."""
    state, post = {ENTRY: 1}, []
    stack = [(ENTRY, iter(succs(blocks, ENTRY)))]
    while stack:
        b, it = stack[-1]
        nxt = next(it, None)
        if nxt is None:
            stack.pop()
            state[b] = 2
            post.append(b)
            continue
        if nxt not in blocks:
            raise Unstructurable("jump to unknown label %s" % (nxt,))
        if state.get(nxt) == 1:
            raise Unstructurable("loop through %s" % (nxt,))
        if nxt not in state:
            state[nxt] = 1
            stack.append((nxt, iter(succs(blocks, nxt))))
    return post


def postdoms(blocks, post):
    """{block: set of postdominators}, with EXIT reached from every return."""
    pd = {EXIT: frozenset([EXIT])}
    for b in post:
        ss = [pd[s] for s in (succs(blocks, b) or [EXIT])]
        pd[b] = frozenset.intersection(*ss) | {b}
    return pd


def ipdom(pd, b):
    """The nearest strict postdominator: the one whose own postdominator set is largest."""
    return max(pd[b] - {b}, key=lambda d: len(pd[d]))


def _region(blocks, pd, b, stop, out, budget):
    """Append the tree for the code from block b up to (excluding) stop."""
    while b is not None and b != EXIT and b != stop:
        budget[0] -= 1
        if budget[0] < 0:
            raise Unstructurable("too large once shared blocks are copied")
        blk = blocks[b]
        out.extend(("stmt", s) for s in blk["stmts"])
        t = blk["term"]
        if t[0] == "ret":
            out.append(("ret",))
            return
        if t[0] == "goto":
            b = t[1]
            continue
        _, cc, taken, fall = t
        join = ipdom(pd, b)
        if join == EXIT:
            join = stop  # the arms end at the enclosing join, or at the function's end
        taken_arm, fall_arm = [], []
        _region(blocks, pd, taken, join, taken_arm, budget)
        _region(blocks, pd, fall, join, fall_arm, budget)
        out.append(("if", cc, taken_arm, fall_arm))
        b = join


def structure(blocks):
    """The tree for a function's blocks (see render). Raises Unstructurable."""
    pd = postdoms(blocks, order(blocks))
    tree = []
    _region(blocks, pd, ENTRY, None, tree, [COPIES * len(blocks) + 8])
    return tree


def render(tree, depth=1):
    """C lines for a tree. An if with both arms empty is dropped: its condition has no effect."""
    lines = []
    pad = "  " * depth
    for node in tree:
        if node[0] == "stmt":
            lines.append(pad + node[1])
        elif node[0] == "ret":
            lines.append(pad + "return;")
        else:
            _, cc, taken, fall = node
            if not taken and not fall:
                continue
            if not taken:
                lines.append(pad + 'if (!s.cond("%s")) {' % cc)
                lines += render(fall, depth + 1)
            else:
                lines.append(pad + 'if (s.cond("%s")) {' % cc)
                lines += render(taken, depth + 1)
                if fall:
                    lines.append(pad + "} else {")
                    lines += render(fall, depth + 1)
            lines.append(pad + "}")
    return lines


def structure_lines(body):
    """Structured body lines for a function's body lines, or None when it keeps its gotos."""
    try:
        return render(structure(build(parse(body))))
    except Unstructurable:
        return None


def body_of(text):
    lines = text.split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("void pes_fn_") and l.endswith("{"))
    end = next(i for i in range(start + 1, len(lines)) if lines[i] == "}")
    return lines[start + 1:end]


def _random_program(rng):
    n = rng.randint(2, 12)
    items = []
    for i in range(n):
        items.append(("label", "L_%d" % i))
        for k in range(rng.randint(0, 2)):
            items.append(("stmt", "b%ds%d" % (i, k)))
        r = rng.random()
        if i + 1 < n and r < 0.6:
            items.append(("if", "cc", "L_%d" % rng.randint(i + 1, n - 1)))
        elif i + 1 < n and r < 0.8:
            items.append(("goto", "L_%d" % rng.randint(i + 1, n - 1)))
        else:
            items.append(("ret",))
    return items


def _run_items(items, bits):
    """Trace of the flat form. Each condition consumes one bit (missing bits read 0)."""
    at = {it[1]: k for k, it in enumerate(items) if it[0] == "label"}
    trace, pc, used = [], 0, 0
    while pc < len(items):
        it = items[pc]
        if it[0] == "stmt":
            trace.append(it[1])
            pc += 1
        elif it[0] == "label":
            pc += 1
        elif it[0] == "if":
            taken = bits[used] if used < len(bits) else 0
            used += 1
            pc = at[it[2]] if taken else pc + 1
        elif it[0] == "goto":
            pc = at[it[1]]
        else:
            trace.append("ret")
            break
    return trace


def _run_tree(tree, bits, st):
    """Same trace from the tree. An empty if still consumes its bit, as its condition did in the flat form."""
    for node in tree:
        if node[0] == "stmt":
            st["trace"].append(node[1])
        elif node[0] == "ret":
            st["trace"].append("ret")
            return False
        else:
            _, _, taken, fall = node
            bit = bits[st["used"]] if st["used"] < len(bits) else 0
            st["used"] += 1
            if not _run_tree(taken if bit else fall, bits, st):
                return False
    return True


def self_check(trials=400, seed=7):
    rng = random.Random(seed)
    done = refused = 0
    for _ in range(trials):
        items = _random_program(rng)
        try:
            tree = structure(build(items))
        except Unstructurable:
            refused += 1
            continue
        done += 1
        for _ in range(20):
            bits = [rng.randint(0, 1) for _ in range(64)]
            st = {"trace": [], "used": 0}
            _run_tree(tree, bits, st)
            want = _run_items(items, bits)
            assert st["trace"] == want, (items, bits, st["trace"], want)
        render(tree)
    print("flow self-check: %d random CFGs structured and trace-equal, %d refused" % (done, refused))
    return 0


def measure(root):
    tally = {"recorded": [0, 0, 0, 0], "closure": [0, 0, 0, 0]}
    for path in sorted(glob.glob(os.path.join(root, "*", "pes_fn_*.cpp"))):
        kind = "closure" if os.path.basename(os.path.dirname(path)) == "closure" else "recorded"
        body = body_of(open(path).read())
        t = tally[kind]
        t[0] += 1
        had_goto = any("goto " in l for l in body)
        t[1] += had_goto
        out = structure_lines(body)
        if out is not None and had_goto:
            t[2] += 1
            t[3] += any("goto " in l for l in out)
    for kind, (n, g, s, left) in tally.items():
        print("%-9s functions %4d  with gotos %4d  structured %4d  gotos left in those %d" % (kind, n, g, s, left))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--measure", metavar="ROOT")
    args = ap.parse_args()
    if args.check:
        return self_check()
    if args.measure:
        return measure(args.measure)
    ap.error("nothing to do: use --check or --measure ROOT")


if __name__ == "__main__":
    sys.exit(main())
