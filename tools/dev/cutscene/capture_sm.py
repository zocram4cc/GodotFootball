import sys,os,json,struct
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'translate'))
# PES_EXE (the unpacked image, pes_full.bin) must be set by the caller, as for every snapshot tool.
import full_snapshot as fs
GVA=0x1436F3DC8
def open_snap(d):
    return fs.SnapMem(d)
def info(d):
    sm=open_snap(d)
    idx=sm.index
    rv=idx['rv']
    g=sm.u64(GVA); mi=sm.u64(g+0x270) if g else 0
    o={'seq':rv['seq'],'hook':rv['hook'][:24],'calls':rv['calls'],'tcap':rv['t_capture']}
    if mi:
        o.update(frame=sm.i32(mi+0x1198),per=sm.i32(mi+0x1168),play=sm.i32(mi+0x116c),restart=sm.i32(mi+0x1554),side=sm.i32(mi+0x1558),reason=sm.i32(mi+0x1574))
    return o,sm
if __name__=='__main__':
    for d in sys.argv[1:]:
        try:
            o,_=info(d); print(d.split('live2/')[-1],o)
        except Exception as e: print(d,'ERR',e)
