#!/usr/bin/env python3
"""Backward liveness over the function CFG: entry live-in registers.

Linear forward walks lie about RET (one path's write hides another path's
read). This propagates liveness backward from every RET/JMP-target exit
through the branch graph to a fixed point, then reads the entry block's
live-in set. CALL contributes its callee-refined arg regs (see fix_calls).

Usage: livein2.py <asm-dir> <out.json>
"""
import re, glob, json, sys, os
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from livein import REGSET, PARENT, regs_in, mem_parts, fix_calls

BRANCH = {'jmp', 'jz', 'jnz', 'jc', 'jnc', 'ja', 'jbe', 'jl', 'jge',
          'jle', 'jg', 'js', 'jns', 'jp', 'jnp', 'jo', 'jno'}


def instr_rw(op, ops, rest):
    reads = set()
    writes = set()
    calls = []
    if op in ('mov', 'movsx', 'movsxd', 'movzx', 'lea',
              'movss', 'movsd', 'movd', 'movaps', 'movups'):
        if len(ops) >= 2:
            reads.update(regs_in(ops[1]))
            writes.update(regs_in(ops[0]))
    elif op == 'push':
        if ops:
            reads.update(regs_in(ops[0]))
    elif op == 'pop':
        if ops:
            writes.update(regs_in(ops[0]))
    elif op == 'call':
        t = re.search(r'0x([0-9a-f]+)', rest)
        if t:
            calls.append("0x" + t.group(1).upper())
        reads.update(['rcx', 'rdx', 'r8', 'r9',
                      'xmm0', 'xmm1', 'xmm2', 'xmm3'])
        writes.update(['rax', 'rcx', 'rdx', 'r8', 'r9', 'r10', 'r11',
                       'xmm0', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5'])
    elif op == 'ret':
        reads.add('rax')
        reads.add('xmm0')
    elif op in ('cmp', 'test'):
        for o in ops:
            reads.update(regs_in(o))
    elif op in ('add', 'sub', 'xor', 'and', 'or', 'imul', 'shl', 'shr',
                'sar', 'adc', 'sbb', 'bt', 'btc', 'btr', 'bts', 'neg',
                'not', 'inc', 'dec', 'mul', 'div', 'idiv'):
        if ops:
            reads.update(regs_in(ops[0]))
            writes.update(regs_in(ops[0]))
        for o in ops[1:]:
            reads.update(regs_in(o))
    elif op in ('addss', 'subss', 'mulss', 'divss',
                'andps', 'orps', 'xorps', 'maxss', 'minss',
                'comiss', 'ucomiss'):
        if ops:
            reads.update(regs_in(ops[0]))
            if op not in ('comiss', 'ucomiss'):
                writes.update(regs_in(ops[0]))
        for o in ops[1:]:
            reads.update(regs_in(o))
    elif op in ('cvtps2pd', 'cvtpd2ps', 'cvtdq2ps', 'cvtss2sd',
                'cvtsd2ss', 'cvttss2si', 'cvtsi2ss', 'cvtss2si'):
        if len(ops) >= 2:
            reads.update(regs_in(ops[1]))
            writes.update(regs_in(ops[0]))
    elif op.startswith('cmov') or op.startswith('set'):
        for o in ops:
            reads.update(regs_in(o))
        if ops:
            writes.update(regs_in(ops[0]))
    elif op in ('cdqe', 'cwd', 'cdq', 'cqo'):
        reads.update(['rax'])
        writes.update(['rax'])
    elif op == 'xchg':
        for o in ops:
            reads.update(regs_in(o))
            writes.update(regs_in(o))
    elif op == 'lea':
        if len(ops) >= 2:
            reads.update(regs_in(ops[1]))
            writes.update(regs_in(ops[0]))
    elif op in BRANCH:
        for o in ops:
            reads.update(regs_in(o))
    else:
        for o in ops:
            reads.update(regs_in(o))
    reads = {PARENT.get(r, r) for r in reads}
    writes = {PARENT.get(r, r) for r in writes}
    return reads, writes, calls


def analyze(lines):
    addrs = []
    parsed = []
    for l in lines:
        m = re.match(r'^([0-9a-f]+):\s*([A-Z0-9]+)\s*(.*)$', l)
        if not m:
            continue
        addr = int(m.group(1), 16)
        addrs.append(addr)
        parsed.append((addr, m.group(2).lower(),
                       [o.strip() for o in m.group(3).split(',')]
                       if m.group(3) else [], m.group(3)))
    idx = {a: i for i, a in enumerate(addrs)}
    n = len(parsed)
    succ = [set() for _ in range(n)]
    calls = []
    mems = []
    seen_mem = set()
    stack_args = set()
    tailjump = None
    switches = 0
    rws = []
    for i, (addr, op, ops, rest) in enumerate(parsed):
        reads, writes, c = instr_rw(op, ops, rest)
        rws.append((reads, writes))
        calls.extend(c)
        for mm in re.finditer(r'\[([^\]]+)\]', rest):
            mp = mem_parts(mm.group(1))
            if mp is None:
                continue
            key = json.dumps(mp, sort_keys=True)
            if key not in seen_mem:
                seen_mem.add(key)
                mems.append(mp)
            if mp["base"] in ("rsp", "rbp") and mp["index"] is None \
                    and mp["disp"] >= 0x28:
                stack_args.add(mp["disp"])
        if op == 'ret':
            pass
        elif op == 'jmp' and ops:
            m2 = re.search(r'0x([0-9a-f]+)', ops[0])
            if m2:
                tgt = int(m2.group(0), 16)
                if tgt in idx:
                    succ[i].add(idx[tgt])
                else:
                    tailjump = "0x%X" % tgt
            else:
                switches += 1
        elif op in BRANCH:
            if ops:
                m2 = re.search(r'0x([0-9a-f]+)', ops[0])
                if m2:
                    tgt = int(m2.group(0), 16)
                    if tgt in idx:
                        succ[i].add(idx[tgt])
            if i + 1 < n:
                succ[i].add(i + 1)
        else:
            if i + 1 < n:
                succ[i].add(i + 1)
    live_out = [set() for _ in range(n)]
    live_in = [set() for _ in range(n)]
    changed = True
    while changed:
        changed = False
        for i in range(n - 1, -1, -1):
            out = set()
            for s in succ[i]:
                out |= live_in[s]
            reads, writes = rws[i]
            inn = (out - writes) | reads
            if inn != live_in[i]:
                live_in[i] = inn
                changed = True
    entry = sorted(live_in[0] - {'rsp', 'rbp'})
    # return-value evidence: rax live at a RET with no writer on that path
    # is approximated by writes_rax (any RAX write in the body)
    writes_rax = any('rax' in w for _, w in rws)
    writes_xmm0 = any('xmm0' in w for _, w in rws)
    return {"live": entry, "mem": mems, "writes_rax": writes_rax,
            "writes_xmm0": writes_xmm0, "stack_args": sorted(stack_args),
            "tailjump": tailjump, "calls": sorted(set(calls)),
            "switches": switches}


def main():
    asm_dir, out = sys.argv[1], sys.argv[2]
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    result = {}
    for p in sorted(glob.glob(os.path.join(asm_dir, '*.json'))):
        va = '0x' + os.path.basename(p).split('.')[0].upper()
        raw = open(p, encoding='utf-8', errors='replace').read()
        lines = [l.strip() for l in raw.splitlines()
                 if l.strip() and re.match(r'^[0-9a-f]+:', l.strip())]
        if not lines:
            result[va] = {"empty": True}
            continue
        result[va] = analyze(lines)
    fix_calls(result)
    json.dump(result, open(out, 'w'), indent=1, sort_keys=True)
    print("wrote %s: %d functions" % (out, len(result)))


if __name__ == '__main__':
    main()
