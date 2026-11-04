# root-side reader: stdin lines "pid va:n,va:n,..." -> stdout hex per range ('x' if unreadable). read-only pread of /proc/pid/mem.
import sys,os
fds={}
for line in sys.stdin:
    p=line.split()
    if not p: continue
    pid=int(p[0])
    fd=fds.get(pid)
    if fd is None:
        try: fd=fds[pid]=os.open('/proc/%d/mem'%pid,os.O_RDONLY)
        except Exception: sys.stdout.write('E\n'); sys.stdout.flush(); continue
    out=[]
    for r in p[1].split(','):
        va,n=r.split(':')
        try: out.append(os.pread(fd,int(n),int(va,16)).hex())
        except Exception: out.append('x')
    sys.stdout.write(' '.join(out)+'\n'); sys.stdout.flush()
