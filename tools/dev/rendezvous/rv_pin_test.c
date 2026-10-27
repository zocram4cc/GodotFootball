/* Regression for the 27-10-26 crash. Loads rv.dll as sider's ffi.load does, calls rv_init with an empty
 * config, frees the library as LuaJIT's GC does, then checks the DLL is still mapped. Exit 0 = pinned. */
#include <windows.h>
#include <stdio.h>

typedef int (*rv_init_t)(const char *, const char *, unsigned);

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    HMODULE h = LoadLibraryA(argv[1]);
    if (!h) { printf("load failed\n"); return 3; }
    rv_init_t init = (rv_init_t)GetProcAddress(h, "rv_init");
    int n = init(argv[2], argv[3], 1000);
    printf("rv_init -> %d\n", n);
    FreeLibrary(h);
    MEMORY_BASIC_INFORMATION mbi;
    int mapped = VirtualQuery((void *)init, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT;
    printf("after FreeLibrary: rv.dll %s\n", mapped ? "still mapped (pinned)" : "UNMAPPED");
    return mapped ? 0 : 1;
}
