/* rv.c: rendezvous entry hooks (prototype). Build: build.sh. Scratch only; not for PES yet.
 *
 * Per hook: a 14-byte `jmp [rip+0]` at the function entry goes to a per-hook trampoline:
 *     push r11 ; movabs r11, ctx ; call [slot] (= rv_common) ; lea rsp,[rsp+8] ;
 *     <displaced original bytes, copied as-is: caller guarantees no rip-relative or branch> ;
 *     jmp [rip+0] -> entry+len
 * rv_common saves the full register file (16 GPR, 16 XMM, MXCSR, RFLAGS) into a frame, calls
 * rv_handle(ctx, frame), restores everything and returns into the displaced code with rsp = entry rsp.
 * rv_handle samples (first N calls, then every K-th). A sampled call writes a CALL_REC (the layout of
 * tools/dev/translate entry_record: 16 GPR, 16 XMM lo/hi, return address) into the shared page,
 * writes capture_<seq>.txt, then spins until release_<seq> exists, or until timeout_ms (QPC) passes.
 * One rendezvous at a time: a sampled call that finds another rendezvous in progress is not sampled.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RV_MAX_HOOKS 64            /* the code pool holds RV_POOL_BYTES / RV_BLOCK_BYTES = 256 blocks */
#define RV_POOL_BYTES 0x10000
#define RV_BLOCK_BYTES 256
#define RV_SLOT_OFF 0xF8           /* block offset of the &rv_common slot */
#define RV_FRAME_SIZE 0x1A0
#define RV_MAGIC 0x3130565250ull   /* "PRV01" */

typedef struct rv_hook {
    char name[48];
    uint64_t va;
    uint32_t len, index;
    uint64_t first, every;          /* sample calls 1..first, then every `every`-th after */
    volatile LONG64 count;
    uint8_t *block;
} rv_hook_t;

/* Frame built by rv_common on the stack. Offsets are used by the asm below. */
typedef struct rv_frame {
    uint64_t flags;                  /* 0x00 */
    uint32_t mxcsr, pad;             /* 0x08 */
    uint64_t ctx;                    /* 0x10 rv_hook_t* (r11 at the trampoline) */
    uint64_t r11;                    /* 0x18 original r11 */
    uint64_t callsp;                 /* 0x20 rsp at the trampoline's call = entry rsp - 16 */
    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi, r8, r9, r10, r12, r13, r14, r15; /* 0x28..0x90 */
    uint64_t pad2;                   /* 0x98 */
    uint8_t xmm[16][16];             /* 0xA0..0x19F */
} rv_frame_t;

/* Shared page: read by the external side through /proc/<pid>/mem. */
typedef struct rv_page {
    uint64_t magic;                  /* 0x00 */
    volatile LONG state;             /* 0x08 0 idle, 1 captured, 2 released, 3 timeout */
    uint32_t hook;                   /* 0x0C */
    uint64_t seq;                    /* 0x10 */
    uint64_t hook_va;                /* 0x18 */
    uint64_t teb;                    /* 0x20 gs base (TEB) */
    uint32_t tid, pid;               /* 0x28 */
    uint32_t mxcsr;                  /* 0x30 */
    uint64_t flags;                  /* 0x38 */
    uint64_t qpc_freq;               /* 0x40 */
    uint64_t t_capture;              /* 0x48 QPC when the call was captured */
    uint64_t t_release;              /* 0x50 QPC when the spin ended (released or timeout) */
    uint64_t calls;                  /* 0x58 hook call count at the sample */
    uint8_t pad[0x100 - 0x60];
    uint64_t rec[49];                /* 0x100 CALL_REC */
} rv_page_t;

_Static_assert(offsetof(rv_frame_t, xmm) == 0xA0, "frame xmm offset");
_Static_assert(offsetof(rv_frame_t, r15) == 0x90, "frame r15 offset");
_Static_assert(offsetof(rv_frame_t, callsp) == 0x20, "frame callsp offset");
_Static_assert(sizeof(rv_frame_t) == RV_FRAME_SIZE, "frame size");
_Static_assert(offsetof(rv_page_t, rec) == 0x100, "page rec offset");

static rv_hook_t g_hooks[RV_MAX_HOOKS];
static uint32_t g_nhooks;
static uint8_t *g_pool;
static uint32_t g_pool_used;
static rv_page_t *g_page;
static char g_run[MAX_PATH];
static uint32_t g_timeout_ms;
static volatile LONG g_busy;
static volatile LONG64 g_seq;
static LARGE_INTEGER g_freq;

extern void rv_common(void);

__asm__(
    ".intel_syntax noprefix\n"
    ".text\n"
    ".globl rv_common\n"
    "rv_common:\n"
    "  lea rsp, [rsp - 0x1A0]\n"
    "  mov [rsp+0x28], rax\n"
    "  mov [rsp+0x30], rcx\n"
    "  mov [rsp+0x38], rdx\n"
    "  mov [rsp+0x40], rbx\n"
    "  mov [rsp+0x48], rbp\n"
    "  mov [rsp+0x50], rsi\n"
    "  mov [rsp+0x58], rdi\n"
    "  mov [rsp+0x60], r8\n"
    "  mov [rsp+0x68], r9\n"
    "  mov [rsp+0x70], r10\n"
    "  mov [rsp+0x78], r12\n"
    "  mov [rsp+0x80], r13\n"
    "  mov [rsp+0x88], r14\n"
    "  mov [rsp+0x90], r15\n"
    "  mov [rsp+0x10], r11\n"
    "  mov rax, [rsp+0x1A8]\n"
    "  mov [rsp+0x18], rax\n"
    "  lea rax, [rsp+0x1A0]\n"
    "  mov [rsp+0x20], rax\n"
    "  stmxcsr [rsp+0x08]\n"
    "  movdqu [rsp+0xA0], xmm0\n"
    "  movdqu [rsp+0xB0], xmm1\n"
    "  movdqu [rsp+0xC0], xmm2\n"
    "  movdqu [rsp+0xD0], xmm3\n"
    "  movdqu [rsp+0xE0], xmm4\n"
    "  movdqu [rsp+0xF0], xmm5\n"
    "  movdqu [rsp+0x100], xmm6\n"
    "  movdqu [rsp+0x110], xmm7\n"
    "  movdqu [rsp+0x120], xmm8\n"
    "  movdqu [rsp+0x130], xmm9\n"
    "  movdqu [rsp+0x140], xmm10\n"
    "  movdqu [rsp+0x150], xmm11\n"
    "  movdqu [rsp+0x160], xmm12\n"
    "  movdqu [rsp+0x170], xmm13\n"
    "  movdqu [rsp+0x180], xmm14\n"
    "  movdqu [rsp+0x190], xmm15\n"
    "  pushfq\n"
    "  pop rax\n"
    "  mov [rsp+0x00], rax\n"
    "  mov rbx, rsp\n"
    "  and rsp, -16\n"
    "  sub rsp, 32\n"
    "  mov rcx, [rbx+0x10]\n"
    "  mov rdx, rbx\n"
    "  call rv_handle\n"
    "  mov rsp, rbx\n"
    "  ldmxcsr [rsp+0x08]\n"
    "  movdqu xmm0, [rsp+0xA0]\n"
    "  movdqu xmm1, [rsp+0xB0]\n"
    "  movdqu xmm2, [rsp+0xC0]\n"
    "  movdqu xmm3, [rsp+0xD0]\n"
    "  movdqu xmm4, [rsp+0xE0]\n"
    "  movdqu xmm5, [rsp+0xF0]\n"
    "  movdqu xmm6, [rsp+0x100]\n"
    "  movdqu xmm7, [rsp+0x110]\n"
    "  movdqu xmm8, [rsp+0x120]\n"
    "  movdqu xmm9, [rsp+0x130]\n"
    "  movdqu xmm10, [rsp+0x140]\n"
    "  movdqu xmm11, [rsp+0x150]\n"
    "  movdqu xmm12, [rsp+0x160]\n"
    "  movdqu xmm13, [rsp+0x170]\n"
    "  movdqu xmm14, [rsp+0x180]\n"
    "  movdqu xmm15, [rsp+0x190]\n"
    "  mov rax, [rsp+0x00]\n"
    "  push rax\n"
    "  popfq\n"
    "  mov rax, [rsp+0x28]\n"
    "  mov rcx, [rsp+0x30]\n"
    "  mov rdx, [rsp+0x38]\n"
    "  mov rbx, [rsp+0x40]\n"
    "  mov rbp, [rsp+0x48]\n"
    "  mov rsi, [rsp+0x50]\n"
    "  mov rdi, [rsp+0x58]\n"
    "  mov r8, [rsp+0x60]\n"
    "  mov r9, [rsp+0x68]\n"
    "  mov r10, [rsp+0x70]\n"
    "  mov r12, [rsp+0x78]\n"
    "  mov r13, [rsp+0x80]\n"
    "  mov r14, [rsp+0x88]\n"
    "  mov r15, [rsp+0x90]\n"
    "  mov r11, [rsp+0x18]\n"
    "  mov rsp, [rsp+0x20]\n"
    "  ret\n"
    ".att_syntax prefix\n");

static void rv_log(const char *fmt, ...)
{
    char path[MAX_PATH + 32], buf[512];
    va_list ap;
    int n;
    HANDLE h;
    DWORD w;

    snprintf(path, sizeof path, "%srv_dll.log", g_run);
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE || n < 0)
        return;
    WriteFile(h, buf, (DWORD)n, &w, NULL);
    CloseHandle(h);
}

/* Write-then-rename so the external side never reads a half-written capture file. */
static int rv_put_file(const char *path, const char *data, DWORD n)
{
    char tmp[MAX_PATH + 32];
    HANDLE h;
    DWORD w = 0;
    BOOL ok;

    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    ok = WriteFile(h, data, n, &w, NULL);
    CloseHandle(h);
    return ok && w == n && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING);
}

static int rv_sampled(const rv_hook_t *h, uint64_t n)
{
    if (n <= h->first)
        return 1;
    return h->every != 0 && ((n - h->first) % h->every) == 0;
}

static uint64_t rv_qpc(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}

static void rv_rendezvous(rv_hook_t *h, rv_frame_t *f, uint64_t n)
{
    rv_page_t *p = g_page;
    uint64_t *r = p->rec;
    uint64_t E = f->callsp + 16, seq;
    char path[MAX_PATH + 32], line[256];
    uint64_t t0;
    LONG outcome = 2;
    int len;

    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0)
        return;
    seq = (uint64_t)InterlockedIncrement64(&g_seq);
    r[0] = f->rax;  r[1] = f->rcx;  r[2] = f->rdx;  r[3] = f->rbx;
    r[4] = E;       r[5] = f->rbp;  r[6] = f->rsi;  r[7] = f->rdi;
    r[8] = f->r8;   r[9] = f->r9;   r[10] = f->r10; r[11] = f->r11;
    r[12] = f->r12; r[13] = f->r13; r[14] = f->r14; r[15] = f->r15;
    memcpy(&r[16], f->xmm, 256);
    r[48] = *(volatile uint64_t *)(uintptr_t)E;   /* return address at [entry rsp] */
    p->seq = seq;
    p->hook = h->index;
    p->hook_va = h->va;
    p->teb = (uint64_t)__readgsqword(0x30);
    p->tid = GetCurrentThreadId();
    p->pid = GetCurrentProcessId();
    p->mxcsr = f->mxcsr;
    p->flags = f->flags;
    p->qpc_freq = (uint64_t)g_freq.QuadPart;
    p->calls = n;
    p->t_capture = rv_qpc();
    p->magic = RV_MAGIC;
    InterlockedExchange(&p->state, 1);

    len = snprintf(line, sizeof line,
                   "seq=%llu hook=%s index=%u va=%llx page=%llx calls=%llu tid=%lu t_capture=%llu freq=%llu\n",
                   (unsigned long long)seq, h->name, h->index, (unsigned long long)h->va,
                   (unsigned long long)(uintptr_t)p, (unsigned long long)n,
                   (unsigned long)p->tid, (unsigned long long)p->t_capture,
                   (unsigned long long)g_freq.QuadPart);
    snprintf(path, sizeof path, "%scapture_%llu.txt", g_run, (unsigned long long)seq);
    rv_put_file(path, line, (DWORD)len);

    snprintf(path, sizeof path, "%srelease_%llu", g_run, (unsigned long long)seq);
    t0 = rv_qpc();
    for (;;) {
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            outcome = 2;
            break;
        }
        if ((rv_qpc() - t0) * 1000u / (uint64_t)g_freq.QuadPart >= g_timeout_ms) {
            outcome = 3;
            break;
        }
        Sleep(1);
    }
    p->t_release = rv_qpc();
    InterlockedExchange(&p->state, outcome);
    snprintf(path, sizeof path, "%sdone_%llu", g_run, (unsigned long long)seq);
    len = snprintf(line, sizeof line, "seq=%llu outcome=%ld t_release=%llu\n",
                   (unsigned long long)seq, (long)outcome, (unsigned long long)p->t_release);
    rv_put_file(path, line, (DWORD)len);
    if (outcome == 3)
        rv_log("TIMEOUT seq=%llu hook=%s after %lu ms; game continues unreleased\n",
               (unsigned long long)seq, h->name, (unsigned long)g_timeout_ms);
    InterlockedExchange(&g_busy, 0);
}

/* Called from rv_common with the hook's ctx and the saved frame (MS x64 ABI). */
void rv_handle(rv_hook_t *h, rv_frame_t *f)
{
    LONG64 n = InterlockedIncrement64(&h->count);
    if (rv_sampled(h, (uint64_t)n))
        rv_rendezvous(h, f, (uint64_t)n);
}

static int rv_install(rv_hook_t *h, const uint8_t *expect)
{
    uint8_t *va = (uint8_t *)(uintptr_t)h->va;
    uint8_t *b;
    uint64_t cs, ctx, back, tb;
    int32_t rel;
    DWORD old, tmp;

    if (h->len < 14 || 23 + h->len + 14 > RV_SLOT_OFF || g_pool_used + RV_BLOCK_BYTES > RV_POOL_BYTES)
        return -1;
    if (expect && memcmp(va, expect, h->len) != 0) {
        rv_log("REFUSED %s: live bytes at %llx differ from the config's expected bytes\n",
               h->name, (unsigned long long)h->va);
        return -2;
    }
    b = g_pool + g_pool_used;
    g_pool_used += RV_BLOCK_BYTES;
    h->block = b;
    ctx = (uint64_t)(uintptr_t)h;
    b[0] = 0x41; b[1] = 0x53;                                  /* push r11 */
    b[2] = 0x49; b[3] = 0xBB; memcpy(b + 4, &ctx, 8);          /* movabs r11, ctx */
    b[12] = 0xFF; b[13] = 0x15;                                /* call [rip+slot] */
    rel = (int32_t)((b + RV_SLOT_OFF) - (b + 18)); memcpy(b + 14, &rel, 4);
    b[18] = 0x48; b[19] = 0x8D; b[20] = 0x64; b[21] = 0x24; b[22] = 0x08; /* lea rsp,[rsp+8] */
    memcpy(b + 23, va, h->len);                                /* displaced original bytes */
    back = h->va + h->len;
    b[23 + h->len] = 0xFF; b[24 + h->len] = 0x25;              /* jmp [rip+0] -> entry+len */
    memset(b + 25 + h->len, 0, 4); memcpy(b + 29 + h->len, &back, 8);
    cs = (uint64_t)(uintptr_t)rv_common;
    memcpy(b + RV_SLOT_OFF, &cs, 8);

    if (!VirtualProtect(va, h->len, PAGE_EXECUTE_READWRITE, &old))
        return -3;
    tb = (uint64_t)(uintptr_t)b;
    va[0] = 0xFF; va[1] = 0x25;                                /* jmp [rip+0] -> block */
    memset(va + 2, 0, 4); memcpy(va + 6, &tb, 8);
    memset(va + 14, 0xCC, h->len - 14);
    VirtualProtect(va, h->len, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), va, h->len);
    return 0;
}

static int rv_hex(const char *s, uint8_t *out, uint32_t max)
{
    uint32_t i = 0;
    while (s[0] && s[1] && i < max) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1)
            return -1;
        out[i++] = (uint8_t)v;
        s += 2;
    }
    return (int)i;
}

/* Config lines: `hook <name> <va hex> <displaced len> <first> <every> [expected bytes hex]`.
 * Returns the number of hooks installed, or a negative error. Idempotent only per process. */
__declspec(dllexport) int rv_init(const char *cfg_path, const char *run_dir, uint32_t timeout_ms)
{
    HANDLE hf;
    DWORD size, got = 0;
    char *buf, *line, *next;
    uint8_t expect[256];
    int installed = 0;
    HMODULE self;

    /* Pin the DLL before any hook exists: the installed trampolines call into it for the life of the process,
     * so it must never unload. sider's LuaJIT ffi.load handle is garbage-collected (FreeLibrary) once the Lua
     * module's local goes out of scope; on 27-10-26 that unloaded rv.dll and the first hooked call at match load
     * executed an unmapped page. */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            (LPCSTR)(void *)rv_init, &self))
        return -11;

    strncpy(g_run, run_dir, sizeof g_run - 2);
    g_run[sizeof g_run - 2] = 0;
    size_t rl = strlen(g_run);
    if (rl == 0 || (g_run[rl - 1] != '\\' && g_run[rl - 1] != '/'))
        strcat(g_run, "\\");
    g_timeout_ms = timeout_ms;
    QueryPerformanceFrequency(&g_freq);
    g_page = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_pool = VirtualAlloc(NULL, RV_POOL_BYTES, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_page || !g_pool)
        return -10;

    hf = CreateFileA(cfg_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE)
        return -11;
    size = GetFileSize(hf, NULL);
    buf = HeapAlloc(GetProcessHeap(), 0, size + 1);
    ReadFile(hf, buf, size, &got, NULL);
    CloseHandle(hf);
    buf[got] = 0;

    for (line = buf; line && *line; line = next ? next + 1 : NULL) {
        char kw[16], name[48], exp[600];
        unsigned long long va, first, every;
        unsigned len;
        int nf;

        next = strchr(line, '\n');
        if (next)
            *next = 0;
        if (line[0] == '#' || line[0] == 0)
            continue;
        exp[0] = 0;
        nf = sscanf(line, "%15s %47s %llx %u %llu %llu %599s", kw, name, &va, &len, &first, &every, exp);
        if (nf < 6 || strcmp(kw, "hook") != 0)
            continue;
        if (g_nhooks >= RV_MAX_HOOKS) {
            rv_log("SKIPPED %s: more than %d hooks in the config\n", name, RV_MAX_HOOKS);
            continue;
        }
        rv_hook_t *h = &g_hooks[g_nhooks];
        memset(h, 0, sizeof *h);
        strncpy(h->name, name, sizeof h->name - 1);
        h->va = va;
        h->len = len;
        h->first = first;
        h->every = every;
        h->index = g_nhooks;
        if (exp[0]) {
            int n = rv_hex(exp, expect, sizeof expect);
            if (n != (int)len) {
                rv_log("BAD config %s: expected bytes %d != len %u\n", name, n, len);
                continue;
            }
        }
        int rc = rv_install(h, exp[0] ? expect : NULL);
        g_nhooks++;
        if (rc == 0)
            installed++;
        else
            rv_log("install %s failed: %d\n", name, rc);
    }
    HeapFree(GetProcessHeap(), 0, buf);
    rv_log("rv_init: %d of %u hooks installed, timeout %lu ms, page %p\n",
           installed, g_nhooks, (unsigned long)timeout_ms, (void *)g_page);
    return installed;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst; (void)reason; (void)reserved;
    return TRUE;
}
