# PES import slots and cutscene prerequisites P0a–P0d: census, classes, models, verdicts, corrected unblock numbers

Scope: read-only RE (no repo edits). Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Ledger `tools/dev/ledger/universe.json` (37,855 rows). Live process: pid 53052 (PES2021.exe under Wine). Scratch: `<workdrive>/gf-ImportSlots-scratch/` (scripts `census.py`, `attr.py`, `blocked.py`, `unblock.py`, `models.py`, `full.py`, `cls2.py`, `p0*.py`; data `slots.json`, `slot_table.md`, `top30.json`, `rank_corr.json`, `blocked.json`). No PES bytes are quoted; VAs, names and behaviour only.

Labels: CONFIRMED = read from the image, live memory, the on-disk PE of a Wine module, or the ledger, by script. INF = inferred, reason given. OPEN = what would settle it.

Live-read log: one read-only pass (`sudo -n` pread of `/proc/<pid>/mem`, 401 qwords, printed to stdout, nothing written into PES). No SIGSTOP, no signal, no write. Pass ran while pid 53052 was alive.

## 0. Summary

- **P0d (slots):** 401 absolute slots are referenced by universe sites. Live, 355 hold Wine-DLL addresses (resolved to module!export from each module's on-disk export table, forwarders followed), 42 hold exe code (A_code), 4 are null (0x0). 134 DLL slots block at least one presentation (P) impure function; 125 block a cutscene one. Top blockers: `msvcp140!?_Xlength_error`, `ntdll!RtlEnterCriticalSection`/`RtlLeaveCriticalSection`, `vcruntime140!memcpy`, `msvcrt!memcpy`, `vcruntime140!memset`. The Fox report's count of 293 DLL slots is **not reproduced** (OPEN, §1.3).
- **Models:** the top 30 by blocked P functions are classed. 7 are exact (4 pure string/memory, 3 noreturn traps); 23 need a replay (Fox Layer 3) or a state model. §2.
- **P0a:** the three bodies are **engine**, not game (CONFIRMED for two by RTTI; INF for the third). **Unblock: 0.**
- **P0b:** `1403A72E0` and `14038E7F1` are **not function starts** (decode artifacts; CONFIRMED by `.pdata`). Removing the artifacts changes no pure or impure status. The 823 and 730 "blocks" are not real. **Unblock: 0** (population −8 cutscene rows).
- **P0c:** the two bodies are **engine** (cobra namespace, CONFIRMED by RTTI). The label `Callback@?A0x55ae6fab@editor` is **not supported**; the RTTI names `cobra::game::util::LoopholeCallback`. **Unblock: 0.**
- **Corrected numbers (§5):** modelling the top 30 slots exactly (not replayed) unblocks 5 cutscene functions (0.2%). Adding replay-dependent models unblocks 74 (2.4%) and grows hard-free cutscene functions by 609 (19.7%). Modelling all 355 DLL-space slots unblocks 110 (3.6%). The rest is vtable, dispatch, unwalked, boundary and callee-chain work, not DLL imports.

## 1. P0d: DLL-import slots

### 1.1 Method (CONFIRMED)

1. Universe sites: every row's `indirect` entry of text `CALL|JMP qword ptr [0x14…]` (absolute). 20,624 call sites and 101 jump sites, over 401 distinct slot VAs. Slot VAs all lie in the IAT window `0x14252F000–0x142531000` or in `.data`/`.impdata` (`0x1437…`, `0x143A…`).
2. Live qword: `pread(/proc/<pid>/mem, slot, 8)`, read-only.
3. Module: the live qword is attributed by PE image range: the module whose header mapping (offset 0, `MZ`) starts at `base` and spans `SizeOfImage` (`/proc/<pid>/maps`). Path-based lookup failed for anonymous `r-xp` sections of `msvcp140.dll` and `vcruntime140.dll`; image-range lookup resolved them.
4. Export: the on-disk PE (`/usr/lib/wine/x86_64-windows/*.dll` or the eFootball prefix `system32`) is parsed; export RVA → name/ordinal; forwarders followed to the final module.
5. Thunks: an export that is `E9 rel32` or `FF 25 disp32` (IAT thunk) is followed. Where pefile cannot read the import table of a Wine builtin, the chain is marked unresolved.

### 1.2 Slot classes (CONFIRMED by script)

| class | slots (of 355 DLL-space) | note |
|---|---|---|
| DLL-space (Wine/Windows DLL) | 355 | 134 block ≥1 P function |
| exe code (A_code) | 42 | soft (known target) |
| null (0x0) | 4 | unbound; 4 cut sites total; 80 P blocks via one slot `0x143AA0F20` |
| unmapped/other | 0 | |

Blockers per slot use the reverse closure through walked, non-boundary callers (same rule as the Fox report and `blocked.py`). Pure set: greatest fixed point; 8,300 universe-wide, 751 of 3,846 cutscene, so 3,095 cutscene impure and 4,093 P non-boundary impure. These reproduce the Fox report exactly (CONFIRMED).

### 1.3 Reconciliation with the Fox report (CONFIRMED / OPEN)

- Pure counts (8,300 / 751 / 3,095 / 4,093): **reproduced exactly**.
- DLL slots: Fox says 293 (dump-time image qwords). This census finds **355** outside the code range, both in dump-time and in live values. Their site set is the same universe; the difference is not the dump-vs-live choice. Which 62 are missing from the Fox 293 is **OPEN**; the Fox set is probably restricted to P-reached sites, but that is not confirmed.
- Fox says the top three slots are `142530380` (14,280 sites), `14252FA28` (1,303) and `14252FA30` (802). Here: `142530380` has 14,328 sites, but only 75 cutscene and 108 P sites, and only **176 cutscene / 281 P blocks**. It is not a P-blocker. The real top blockers are `14252FA28` (1,196 / 1,577), `14252F340` and `14252F6E8` (1,059 / 1,447 each), `14252FD68` (996 / 1,313).
- Hard-blocked (DLL slots + null + unwalked, cutscene 2,038 / P 3,004) versus Fox HARD (1,891 / 2,639): **not reconciled** (OPEN). The Fox HARD set appears to count boundary rows or closure differently. Here, boundary rows are reported separately as Fox services.

### 1.4 Table: DLL slots that block at least one P function (134)

Blocked = number of impure functions in the population whose reverse closure reaches a site through this slot. "sites all/cut/pres" counts call sites in all rows, cutscene rows and P rows. Classes: pure = memory/string, exact; libm = host float (bits must match); noreturn-trap = loud abort on both sides; CRT-state = errno/locale/env; heap; I/O; OS threading/TLS; OS time; OS memory; OS file/error/env; C++ runtime object (STL); other = unmodelled name.

| # | slot | module!export | sites all/cut/pres | blocked cut | blocked pres | class |
|---|---|---|---|---|---|---|
| 1 | 0x14252fa28 | msvcp140.dll!?_Xlength_error@std@@YAXPEBD@Z | 1303/235/294 | 1196 | 1577 | noreturn-trap |
| 2 | 0x14252f340 | ntdll.dll!RtlEnterCriticalSection | 47/0/0 | 1059 | 1447 | OS threading/TLS |
| 3 | 0x14252f6e8 | ntdll.dll!RtlLeaveCriticalSection | 52/0/0 | 1059 | 1447 | OS threading/TLS |
| 4 | 0x14252fd68 | vcruntime140.dll!memcpy | 298/17/46 | 996 | 1313 | pure |
| 5 | 0x14252fd70 | msvcrt.dll!memcpy | 1/0/0 | 980 | 1297 | pure |
| 6 | 0x14252fd98 | vcruntime140.dll!memset | 1/0/0 | 854 | 1247 | pure |
| 7 | 0x14252f440 | kernel32.dll!InitializeCriticalSectionAndSpinCount | 2/1/1 | 773 | 1018 | OS threading/TLS |
| 8 | 0x14252f6a8 | kernel32.dll!TlsGetValue | 12/1/1 | 841 | 1014 | OS threading/TLS |
| 9 | 0x14252f1c0 | kernel32.dll!SwitchToThread | 4/2/2 | 784 | 1014 | OS threading/TLS |
| 10 | 0x14252f390 | kernel32.dll!Sleep | 30/1/6 | 634 | 999 | OS threading/TLS |
| 11 | 0x14252f6d0 | kernel32.dll!VirtualFree | 6/1/1 | 752 | 995 | OS memory |
| 12 | 0x14252f6d8 | kernel32.dll!VirtualAlloc | 5/0/0 | 750 | 993 | OS memory |
| 13 | 0x14252f4b0 | kernel32.dll!GetSystemInfo | 3/0/0 | 686 | 926 | OS memory |
| 14 | 0x14252fa30 | msvcp140.dll!?_Xout_of_range@std@@YAXPEBD@Z | 803/132/144 | 598 | 831 | noreturn-trap |
| 15 | 0x14252f4b8 | kernel32.dll!TlsSetValue | 11/0/0 | 702 | 823 | OS threading/TLS |
| 16 | 0x14252f6b8 | kernel32.dll!SetLastError | 9/0/0 | 702 | 823 | OS file/error/env |
| 17 | 0x14252f328 | kernel32.dll!GetLastError | 62/0/0 | 702 | 823 | OS file/error/env |
| 18 | 0x14252f448 | kernel32.dll!FormatMessageA | 1/0/0 | 702 | 823 | OS file/error/env |
| 19 | 0x142530410 | ucrtbase.dll!_write | 1/0/0 | 702 | 823 | I/O |
| 20 | 0x142530650 | ucrtbase.dll!strncmp | 80/3/8 | 625 | 782 | pure |
| 21 | 0x1425300a0 | ucrtbase.dll!strtol | 10/1/1 | 624 | 749 | CRT-state |
| 22 | 0x14252fd80 | vcruntime140.dll!_purecall | 1/0/1 | 550 | 746 | noreturn-trap |
| 23 | 0x1425300c8 | ucrtbase.dll!getenv | 1/0/0 | 612 | 730 | CRT-state |
| 24 | 0x14252f6b0 | kernel32.dll!TlsAlloc | 1/0/0 | 612 | 730 | OS threading/TLS |
| 25 | 0x14252f388 | kernel32.dll!GetCurrentThreadId | 5/0/2 | 378 | 689 | OS threading/TLS |
| 26 | 0x14252f430 | kernel32.dll!QueryPerformanceCounter | 5/0/0 | 427 | 621 | OS time |
| 27 | 0x14252f368 | ntdll.dll!RtlDeleteCriticalSection | 3/0/1 | 369 | 572 | OS threading/TLS |
| 28 | 0x14252fe88 | winmm.dll!timeGetTime | 1/0/0 | 378 | 546 | OS time |
| 29 | 0x14252f6c8 | kernel32.dll!VirtualQuery | 1/0/0 | 336 | 528 | OS memory |
| 30 | 0x14252f1d0 | kernel32.dll!GetTickCount | 18/0/0 | 336 | 528 | OS time |
| 31 | 0x14252fda0 | vcruntime140.dll!memcmp | 1/0/0 | 357 | 463 | pure |
| 32 | 0x14252f3f8 | kernel32.dll!WaitForSingleObject | 17/3/3 | 269 | 376 | OS threading/TLS |
| 33 | 0x14252f408 | kernel32.dll!SetEvent | 5/1/1 | 257 | 373 | OS threading/TLS |
| 34 | 0x142530660 | ucrtbase.dll!strcpy_s | 82/32/34 | 320 | 368 | pure |
| 35 | 0x14252fdc8 | vcruntime140.dll!memchr | 48/9/10 | 239 | 283 | pure |
| 36 | 0x142530208 | ucrtbase.dll!cosf | 1/0/0 | 202 | 283 | libm |
| 37 | 0x142530380 | ucrtbase.dll!_invalid_parameter_noinfo_noreturn | 14328/75/108 | 176 | 281 | noreturn-trap |
| 38 | 0x142530438 | ucrtbase.dll!__stdio_common_vsprintf_s | 1/0/0 | 211 | 270 | I/O |
| 39 | 0x142530218 | ucrtbase.dll!sinf | 1/0/0 | 145 | 253 | libm |
| 40 | 0x14252fd60 | vcruntime140.dll!strchr | 10/0/4 | 183 | 249 | pure |
| 41 | 0x142530108 | ucrtbase.dll!free | 177/0/6 | 191 | 243 | heap |
| 42 | 0x142530118 | ucrtbase.dll!malloc | 170/1/4 | 191 | 241 | heap |
| 43 | 0x1425301f0 | ucrtbase.dll!sqrtf | 1/0/0 | 98 | 241 | libm |
| 44 | 0x14252fe90 | winmm.dll!timeGetSystemTime | 1/0/0 | 199 | 232 | OS time |
| 45 | 0x14252fa50 | msvcp140.dll!?_Xbad_alloc@std@@YAXXZ | 44/3/8 | 141 | 230 | noreturn-trap |
| 46 | 0x1425305a0 | ucrtbase.dll!strcmp | 1/0/1 | 111 | 226 | pure |
| 47 | 0x1425304d0 | ucrtbase.dll!__stdio_common_vsscanf | 2/0/0 | 186 | 225 | I/O |
| 48 | 0x1425305d8 | ucrtbase.dll!toupper | 13/2/2 | 180 | 211 | CRT-state |
| 49 | 0x14252f5f0 | kernel32.dll!GetCurrentThread | 1/1/1 | 155 | 211 | OS threading/TLS |
| 50 | 0x14252f5d0 | kernel32.dll!SleepConditionVariableCS | 1/0/0 | 154 | 210 | OS threading/TLS |
| 51 | 0x14252f5c8 | ntdll.dll!RtlWakeConditionVariable | 1/0/0 | 154 | 210 | OS threading/TLS |
| 52 | 0x14252f5c0 | ntdll.dll!RtlWakeAllConditionVariable | 1/0/0 | 154 | 210 | OS threading/TLS |
| 53 | 0x142530618 | ucrtbase.dll!strpbrk | 1/0/0 | 176 | 207 | pure |
| 54 | 0x14252fda8 | vcruntime140.dll!strstr | 10/0/0 | 173 | 204 | pure |
| 55 | 0x142530700 | ucrtbase.dll!_byteswap_ulong | 1/1/1 | 137 | 193 | pure |
| 56 | 0x14252f1a0 | kernel32.dll!ReleaseSemaphore | 2/1/1 | 130 | 185 | OS threading/TLS |
| 57 | 0x142530260 | ucrtbase.dll!floor | 1/1/1 | 155 | 171 | libm |
| 58 | 0x1425301f8 | ucrtbase.dll!tanf | 1/1/1 | 82 | 146 | libm |
| 59 | 0x14252f410 | kernel32.dll!CloseHandle | 50/0/1 | 96 | 136 | OS file/error/env |
| 60 | 0x142530238 | ucrtbase.dll!atan2f | 1/0/0 | 52 | 134 | libm |
| 61 | 0x142530058 | ucrtbase.dll!atoi | 44/27/28 | 114 | 132 | CRT-state |
| 62 | 0x14252f1b8 | kernel32.dll!CreateSemaphoreW | 2/0/0 | 96 | 131 | OS threading/TLS |
| 63 | 0x14252f330 | kernel32.dll!GetCurrentDirectoryW | 4/0/0 | 97 | 128 | OS file/error/env |
| 64 | 0x14252f438 | kernel32.dll!GetTempPathW | 1/0/0 | 97 | 128 | OS file/error/env |
| 65 | 0x14252fb28 | shell32.dll!SHGetSpecialFolderPathW | 1/0/0 | 97 | 128 | OS file/error/env |
| 66 | 0x14252f398 | kernel32.dll!WideCharToMultiByte | 3/0/0 | 97 | 128 | pure (codepage tables; INF) |
| 67 | 0x1425302a8 | ucrtbase.dll!sqrt | 1/0/0 | 113 | 127 | libm |
| 68 | 0x1425305b8 | ucrtbase.dll!isalpha | 3/1/3 | 85 | 124 | CRT-state |
| 69 | 0x142530560 | ucrtbase.dll!__stdio_common_vsnprintf_s | 6/0/3 | 65 | 124 | I/O |
| 70 | 0x1425301c8 | ucrtbase.dll!_isnan | 4/0/4 | 39 | 108 | pure |
| 71 | 0x1425301d0 | ucrtbase.dll!_finite | 4/0/4 | 39 | 108 | pure |
| 72 | 0x14252fd58 | vcruntime140.dll!strrchr | 7/2/2 | 98 | 104 | pure |
| 73 | 0x142530230 | ucrtbase.dll!asinf | 1/0/1 | 44 | 104 | libm |
| 74 | 0x1425301d8 | ucrtbase.dll!floorf | 1/0/1 | 63 | 103 | libm |
| 75 | 0x142530098 | ucrtbase.dll!strtoul | 3/1/1 | 65 | 91 | CRT-state |
| 76 | 0x1425304f8 | ucrtbase.dll!__stdio_common_vfprintf | 1/0/1 | 40 | 91 | I/O |
| 77 | 0x142530628 | ucrtbase.dll!isspace | 6/0/6 | 40 | 89 | CRT-state |
| 78 | 0x1425305c0 | ucrtbase.dll!tolower | 19/0/14 | 40 | 82 | CRT-state |
| 79 | 0x1425300a8 | ucrtbase.dll!_strtoi64 | 1/1/1 | 73 | 79 | CRT-state |
| 80 | 0x142530610 | ucrtbase.dll!isalnum | 2/0/1 | 40 | 79 | CRT-state |
| 81 | 0x1425305b0 | ucrtbase.dll!isdigit | 1/1/1 | 71 | 77 | CRT-state |
| 82 | 0x14252f968 | msvcp140.dll!??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ | 169/1/2 | 64 | 77 | C++ runtime object (STL) |
| 83 | 0x142530198 | ucrtbase.dll!fmodf | 1/0/1 | 59 | 75 | libm |
| 84 | 0x142530240 | ucrtbase.dll!atanf | 1/0/1 | 40 | 72 | libm |
| 85 | 0x1425305c8 | ucrtbase.dll!strncpy_s | 14/0/3 | 38 | 66 | pure |
| 86 | 0x14252fc78 | user32.dll!MessageBoxA | 1/0/1 | 36 | 64 | OS file/error/env |
| 87 | 0x142530638 | ucrtbase.dll!strlen | 1/0/1 | 4 | 49 | pure |
| 88 | 0x1425300b0 | ucrtbase.dll!atof | 3/2/2 | 41 | 47 | CRT-state |
| 89 | 0x14252fde0 | vcruntime140.dll!_CxxThrowException | 1/0/0 | 40 | 46 | noreturn-trap |
| 90 | 0x14252f948 | msvcp140.dll!??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ | 139/1/2 | 34 | 45 | C++ runtime object (STL) |
| 91 | 0x142530588 | ucrtbase.dll!_stricmp | 6/0/0 | 34 | 44 | CRT-state |
| 92 | 0x14252fa10 | msvcp140.dll!??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ | 82/1/1 | 31 | 41 | C++ runtime object (STL) |
| 93 | 0x14252fa08 | msvcp140.dll!??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ | 15/0/0 | 31 | 41 | C++ runtime object (STL) |
| 94 | 0x14252fab0 | msvcp140.dll!??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ | 13/0/0 | 31 | 41 | C++ runtime object (STL) |
| 95 | 0x14252fac8 | msvcp140.dll!??0?$basic_iostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z | 82/1/1 | 31 | 41 | C++ runtime object (STL) |
| 96 | 0x14252f8e8 | msvcp140.dll!?do_encoding@?$codecvt@_SDU_Mbstatet@@@std@@MEBAHXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 97 | 0x14252f8e0 | msvcp140.dll!?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 98 | 0x14252f8d8 | msvcp140.dll!?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 99 | 0x14252f7d0 | msvcp140.dll!?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 100 | 0x14252f7c8 | msvcp140.dll!?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 101 | 0x14252f7c0 | msvcp140.dll!?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 102 | 0x14252f7b8 | msvcp140.dll!??4?$_Iosb@H@std@@QEAAAEAV01@$$QEAV01@@Z | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 103 | 0x14252f7b0 | msvcp140.dll!?do_encoding@?$codecvt@_SDU_Mbstatet@@@std@@MEBAHXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 104 | 0x14252f7a8 | msvcp140.dll!?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ | 1/0/0 | 31 | 41 | C++ runtime object (STL) |
| 105 | 0x1425306d8 | ucrtbase.dll!_localtime64_s | 2/1/1 | 33 | 39 | OS time |
| 106 | 0x14252f8f8 | msvcp140.dll!?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ | 4/0/0 | 31 | 37 | C++ runtime object (STL) |
| 107 | 0x14252f958 | msvcp140.dll!?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z | 6/0/0 | 31 | 37 | C++ runtime object (STL) |
| 108 | 0x14252fa48 | msvcp140.dll!?uncaught_exception@std@@YA_NXZ | 4/0/0 | 31 | 37 | C++ runtime object (STL) |
| 109 | 0x14252f928 | msvcp140.dll!?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ | 4/0/0 | 31 | 37 | C++ runtime object (STL) |
| 110 | 0x14252f9f0 | msvcp140.dll!?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z | 8/0/0 | 31 | 37 | C++ runtime object (STL) |
| 111 | 0x14252f9e8 | msvcp140.dll!?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z | 4/0/0 | 31 | 37 | C++ runtime object (STL) |
| 112 | 0x14252f918 | msvcp140.dll!??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@H@Z | 43/1/1 | 31 | 37 | C++ runtime object (STL) |
| 113 | 0x14252f990 | msvcp140.dll!?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z | 5/1/1 | 27 | 33 | C++ runtime object (STL) |
| 114 | 0x14252f940 | msvcp140.dll!??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z | 57/0/1 | 25 | 32 | C++ runtime object (STL) |
| 115 | 0x1425301e0 | ucrtbase.dll!pow | 1/0/0 | 25 | 31 | libm |
| 116 | 0x1425304d8 | ucrtbase.dll!__stdio_common_vsprintf | 5/0/2 | 0 | 27 | I/O |
| 117 | 0x142530698 | ucrtbase.dll!_time64 | 4/0/1 | 0 | 27 | OS time |
| 118 | 0x1425302e0 | ucrtbase.dll!atan2 | 1/0/0 | 10 | 18 | libm |
| 119 | 0x1425302d8 | ucrtbase.dll!cos | 1/0/0 | 10 | 18 | libm |
| 120 | 0x1425302b8 | ucrtbase.dll!sin | 1/0/0 | 10 | 18 | libm |
| 121 | 0x1425301b8 | ucrtbase.dll!ceilf | 1/0/0 | 4 | 14 | libm |
| 122 | 0x142530160 | ucrtbase.dll!_create_locale | 1/0/1 | 0 | 13 | CRT-state |
| 123 | 0x142530158 | ucrtbase.dll!_free_locale | 1/0/1 | 0 | 13 | CRT-state |
| 124 | 0x142530640 | ucrtbase.dll!strcspn | 2/0/2 | 0 | 12 | pure |
| 125 | 0x142530678 | ucrtbase.dll!strspn | 1/0/1 | 0 | 12 | pure |
| 126 | 0x142530228 | ucrtbase.dll!acosf | 1/0/0 | 0 | 12 | libm |
| 127 | 0x14252f270 | kernel32.dll!CreateEventW | 4/0/1 | 0 | 11 | OS threading/TLS |
| 128 | 0x14252f910 | msvcp140.dll!??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@I@Z | 125/0/0 | 4 | 8 | C++ runtime object (STL) |
| 129 | 0x142530248 | ucrtbase.dll!ceil | 1/1/1 | 6 | 6 | libm |
| 130 | 0x142530110 | ucrtbase.dll!realloc | 25/5/5 | 5 | 5 | heap |
| 131 | 0x1425306f8 | ucrtbase.dll!qsort | 10/0/0 | 4 | 4 | other (callback-taking CRT) |
| 132 | 0x142530258 | ucrtbase.dll!expf | 1/0/0 | 4 | 4 | libm |
| 133 | 0x14252f540 | kernel32.dll!GetModuleFileNameW | 1/0/0 | 0 | 4 | OS file/error/env |
| 134 | 0x142530290 | ucrtbase.dll!powf | 1/0/0 | 3 | 3 | libm |

Class totals (134): C++ runtime object (STL) 25 · pure 20 · libm 20 · OS threading/TLS 19 · CRT-state 15 · OS file/error/env 9 · noreturn-trap 6 · I/O 6 · OS time 6 · OS memory 4 · heap 3 · other 1.

Note on the STL rows: the 25 `msvcp140` C++ objects (`basic_ios`, `basic_streambuf`, `basic_ostream` and their ctors/dtors) are real MSVC runtime bodies, not names the harness can execute directly. They need a body-level model or a replay; they are not "pure".

### 1.5 Thunk chains for the top 30 (CONFIRMED by script, on-disk PE)

- `msvcrt!memcpy` → `jmp` to `msvcrt` RVA A480 (the same copy routine).
- `ucrtbase!getenv` → `jmp` to `ucrtbase` RVA A02F0.
- `winmm!timeGetTime` → IAT thunk → `kernel32!timeGetTime` (RVA 2E1C0; Wine forwards winmm to kernel32).
- `kernel32!SetLastError` → IAT thunk → `ntdll` (import table unreadable with pefile for this Wine builtin; target **unresolved**).
- Others (`ntdll!RtlEnterCriticalSection`, `kernel32!Sleep`, …) are direct exports.

## 2. Models for the top 30 (stubs.json contract, proposals only)

The contract is per callee VA. For an IAT slot the VA is the slot itself (`0x14252…`). Proposed kinds: existing `return-input`, `abort-trap`, `libm`, `unmodellable`; plus `fox-replay` (Fox Layer 3, keyed by slot VA, from the Fox report §5) and a **new kind `state-model`** (per-thread cells: TLS, last-error, errno; needs `stubs.py` support). Nothing was written to `tools/dev/translate/stubs.json` (read-only RE).

Citations are the Win32/CRT function names and the standard named in column "why" (MS Learn reference for the function, or ISO C11 §7.24 for string/memory). The exact-semantics claims are from the documented contract, not checked against the harness; the harness check is the proof.

| rank | slot | module!export | class | blocked C/P | kind | model (exact semantics) | why |
|---|---|---|---|---|---|---|---|
| 1 | 0x14252fa28 | msvcp140 ?_Xlength_error | noreturn-trap | 1196/1577 | abort-trap | none; loud abort when reached | MSVC STL throws `std::length_error` (noreturn by name; INF from MSVC STL) |
| 2 | 0x14252f340 | ntdll RtlEnterCriticalSection | OS threading | 1059/1447 | fox-replay | single-thread exact model: LockCount/RecursionCount/OwningThread writes; else replay | CRITICAL_SECTION (MS EnterCriticalSection); Wine ntdll |
| 3 | 0x14252f6e8 | ntdll RtlLeaveCriticalSection | OS threading | 1059/1447 | fox-replay | as 2 (decrement, release at zero) | MS LeaveCriticalSection |
| 4 | 0x14252fd68 | vcruntime140 memcpy | pure | 996/1313 | return-input | copy n bytes src→dst; return dst; overlap is UB | C11 §7.24.2.1; MS memcpy |
| 5 | 0x14252fd70 | msvcrt memcpy (→ msvcrt RVA A480) | pure | 980/1297 | return-input | as 4 | C11 §7.24.2.1; thunk followed |
| 6 | 0x14252fd98 | vcruntime140 memset | pure | 854/1247 | return-input | write n bytes of (unsigned char)c; return dst | C11 §7.24.6.1; MS memset |
| 7 | 0x14252f440 | kernel32 InitializeCriticalSectionAndSpinCount | OS threading | 773/1018 | fox-replay | init CRITICAL_SECTION fields; return TRUE | MS InitializeCriticalSectionAndSpinCount |
| 8 | 0x14252f6a8 | kernel32 TlsGetValue | OS TLS | 841/1014 | state-model | per-thread slot table; return stored value; clear last error | MS TlsGetValue |
| 9 | 0x14252f1c0 | kernel32 SwitchToThread | OS threading | 784/1014 | fox-replay | return value from scheduler (host) | MS SwitchToThread |
| 10 | 0x14252f390 | kernel32 Sleep | OS threading | 634/999 | fox-replay | no return value; time effect; callers must not observe time | MS Sleep |
| 11 | 0x14252f6d0 | kernel32 VirtualFree | OS memory | 752/995 | fox-replay | BOOL; address protocol; not portable | MS VirtualFree |
| 12 | 0x14252f6d8 | kernel32 VirtualAlloc | OS memory | 750/993 | fox-replay | returns host address; not portable (replay pointers via allocation map) | MS VirtualAlloc |
| 13 | 0x14252f4b0 | kernel32 GetSystemInfo | OS memory | 686/926 | fox-replay | write SYSTEM_INFO (host values recorded once) | MS GetSystemInfo |
| 14 | 0x14252fa30 | msvcp140 ?_Xout_of_range | noreturn-trap | 598/831 | abort-trap | none | MSVC STL (noreturn by name; INF) |
| 15 | 0x14252f4b8 | kernel32 TlsSetValue | OS TLS | 702/823 | state-model | per-thread slot table store; BOOL TRUE | MS TlsSetValue |
| 16 | 0x14252f6b8 | kernel32 SetLastError | OS error | 702/823 | state-model | store dwErrCode in per-thread cell | MS SetLastError (thunk into ntdll; unresolved) |
| 17 | 0x14252f328 | kernel32 GetLastError | OS error | 702/823 | state-model | return per-thread cell | MS GetLastError |
| 18 | 0x14252f448 | kernel32 FormatMessageA | OS error | 702/823 | fox-replay | system message text; locale-dependent | MS FormatMessage |
| 19 | 0x142530410 | ucrtbase _write | I/O | 702/823 | fox-replay | bytes written; file side effect | MS _write |
| 20 | 0x142530650 | ucrtbase strncmp | pure | 625/782 | return-input | compare first n bytes as unsigned char; sign exact; magnitude per MS CRT (verify) | C11 §7.24.4.4 |
| 21 | 0x1425300a0 | ucrtbase strtol | CRT-state | 624/749 | state-model | parse with radix; ERANGE → errno cell; clamp to LONG_MAX/MIN | C11 §7.22.1.4; MS strtol |
| 22 | 0x14252fd80 | vcruntime140 _purecall | noreturn-trap | 550/746 | abort-trap | none | MS _purecall (noreturn by name; INF) |
| 23 | 0x1425300c8 | ucrtbase getenv | CRT-state | 612/730 | fox-replay | pointer into host env block; replay the environment | MS getenv; thunk → ucrtbase RVA A02F0 |
| 24 | 0x14252f6b0 | kernel32 TlsAlloc | OS TLS | 612/730 | state-model | allocate index in per-thread table | MS TlsAlloc |
| 25 | 0x14252f388 | kernel32 GetCurrentThreadId | OS thread | 378/689 | fox-replay | host thread id | MS GetCurrentThreadId |
| 26 | 0x14252f430 | kernel32 QueryPerformanceCounter | OS time | 427/621 | fox-replay | host clock; replay recorded values (a deterministic clock changes semantics) | MS QueryPerformanceCounter |
| 27 | 0x14252f368 | ntdll RtlDeleteCriticalSection | OS threading | 369/572 | fox-replay | release CRITICAL_SECTION fields | MS DeleteCriticalSection |
| 28 | 0x14252fe88 | winmm timeGetTime | OS time | 378/546 | fox-replay | host ms clock; replay | MS timeGetTime; → kernel32 timeGetTime |
| 29 | 0x14252f6c8 | kernel32 VirtualQuery | OS memory | 336/528 | fox-replay | write MEMORY_BASIC_INFORMATION (host layout) | MS VirtualQuery |
| 30 | 0x14252f1d0 | kernel32 GetTickCount | OS time | 336/528 | fox-replay | host ms count; replay | MS GetTickCount |

Exact now (no replay): ranks 1, 4, 5, 6, 14, 20, 22 (7 slots). Replay or state: ranks 2, 3, 7–13, 15–19, 21, 23–30 (23 slots).

Stub JSON shape proposal (one entry per slot, not yet applied):

```
"0x14252fd68": {"kind": "return-input", "why": ["C11 7.24.2.1", "MS memcpy", "census: vcruntime140!memcpy"], "model": "copy n bytes src->dst; return dst; overlap UB"}
"0x14252f340": {"kind": "fox-replay", "why": ["MS EnterCriticalSection", "census: ntdll!RtlEnterCriticalSection"], "model": "single-thread exact or replay recursion counter"}
```

## 3. P0a: engine or game for `141AF61C0`, `141AFB090`, `141EB6840`

Verdict: **engine** for all three. Unblock if game: not applicable; **unblock now: 0**. They stay Fox-service boundaries (families I and D).

- **`141AF61C0` (ENGINE, CONFIRMED by RTTI).** Row vtables `142545048` = `.?AV?$ObjectMemoryOperatorT@M@fox@@` and `1425E49D0` = `.?AV?$ObjectMemoryOperatorT@UMotionDesc@And@cobra@@@fox@@` (RTTI decoded from the image: COL → TypeDescriptor, sig 1). Namespace `fox` is an engine root (`universe.py` `ENGINE_ROOTS`). Docs: `pes-re-anim-blend.md:472` calls it SetMotion. Ledger rule `rtti`.
- **`141AFB090` (ENGINE, CONFIRMED by RTTI).** All four row vtables (`142544F58`, `142545048`, `142ABA240`, `142ABA268`) are `ObjectMemoryOperatorT<…>@fox` (Quat, float, EntityPtr<PoseOperatorRootRotParam@anim@fox>, SharedPtr<JobPacket@fox>). Docs: `pes-re-anim-blend.md:478` calls it the CobraAndBodyAnimControl constructor.
- **`141EB6840` (ENGINE, INF, med-high).** No vtable in the row. Docs: `pes-re-animselect.md:601` ("Engine side", creates a face GenAnimControl and two hand GenAnimControls with 4 tracks). The body makes six direct calls, all into the `141AFxxxx` block (`141AFEB40`, `141AFE2A0`, `141AFF8E0`, `141AFED20` ×2, `141AFF770`), the same engine anim family as the two confirmed ones. OPEN to upgrade to CONFIRMED: RTTI owner of those six callees.

Consequence: the Fox report's "unblocks 140 P (100 cut) if un-boundaried" does not apply; those functions stay Fox-service boundaries. This does not change the P0d numbers.

## 4. P0b: `1403A72E0` and `14038E7F1`

Verdict: **decode artifacts, not function starts.** CONFIRMED.

- `1403A72E0`: `.pdata` gives the containing function `1403A72D3`–`1403A7CBE`, and `1403A72E0` is not its begin. The row is absent from the universe as a function start. Its only caller is `1403A74CE` via a `tail` edge, and that caller is inside the same `.pdata` range (intra-function jump, CONFIRMED). `1403A74CE` is itself reached only by a `table` edge from `1403A7290`, a different `.pdata` function (cross-function jump-table target, so a table word read as code: INF).
- `14038E7F1`: containing `.pdata` function `14038E7B0`–`14038EE7B`; `14038E7F1` is not its begin. Its only caller `14038ED78` (tail) is in the same range (intra-function, CONFIRMED).

**Corrected counts (CONFIRMED by recount, `unblock.py`):**

| variant | pure universe | cutscene pure / pop. | cutscene impure | P impure |
|---|---|---|---|---|
| universe as is | 8,300 | 751 / 3,846 | 3,095 | 4,987 (4,093 non-boundary) |
| drop the 3 artifact rows and their edges | 8,300 | 751 / 3,846 | 3,095 | 4,987 |
| drop all 271 pdata-interior rows (235 tail-only; 33 table edges) | 8,300 | 751 / 3,838 | 3,087 | 4,979 |

So the 823 and 730 blocks are reverse-closure counts through artifact rows; none of the affected functions is impure **only** because of them. Dropping the artifacts changes no pure status. The broad variant removes 8 cutscene rows (population −8) that are not functions. **Unblock: 0.** The Fox report's "843 outside blocks" are not real blockers.

## 5. P0c: the `editor` callbacks `141B55940`, `141B55A80`

Verdict: **engine** (cobra). Unblock: 0.

- `141B55940` (CONFIRMED by RTTI): the body installs vtable `14259E7D0`, whose RTTI TypeDescriptor is `.?AVLoopholeCallback@util@game@cobra@@` (COL sig 1). Outer namespace `cobra` is an engine root (`ENGINE_ROOTS`). The `game` nested under `cobra` is still inside cobra's namespace (INF: a cobra callback helper, not PES gameplay logic).
- `141B55A80` (ENGINE, INF, med-high): a five-byte forwarder: loads the object pointer at `[rdx+0x10]`, then jumps to slot `0x58` (vtable entry 11) of that object. Its only caller is `141B559A0`, the same caller as `141B55940`. It has no RTTI of its own.
- **Label correction:** the Fox report's name `Callback@?A0x55ae6fab@editor` is **not reproduced**. The RTTI at the installed vtable names `LoopholeCallback@util@game@cobra`; no `editor` namespace appears. The `editor`/`edit::` out-of-scope category (`OUT_NS`) does not apply.

## 6. Corrected unblock numbers

Baseline: cutscene impure 3,095 (of 3,846 cutscene rows; pure 751). P impure 4,987 (non-boundary 4,093). Hard-free (no remaining DLL/null/unwalked blocker): cutscene 1,057, P 1,983 (`blocked.py`, same definitions).

| step | cutscene pure | cutscene impure (share of 3,095) | P impure (share of 4,987) | cutscene hard-free (share of impure) |
|---|---|---|---|---|
| baseline | 751 | 3,095 | 4,987 | 1,057 (34.2%) |
| P0a (engine) | 751 (0) | 3,095 (0%) | 4,987 (0%) | 1,057 (0) |
| P0b (artifacts removed) | 751 (0) | 3,087 (−8; −0.3%, population) | 4,979 (−8; −0.2%) | 1,057 (0) |
| P0c (engine) | 751 (0) | 3,087 (0%) | 4,979 (0%) | 1,057 (0) |
| P0d top 30, exact only (4 pure + 3 trap) | 756 (+5; 0.2%) | 3,090 (−5; −0.2%) | 4,979 (−8; −0.2%) | 1,490 (+433; 14.0%) |
| P0d top 30, all (exact + replay + state) | 825 (+74; 2.4%) | 3,021 (−74; −2.4%) | 4,876 (−111; −2.2%) | 1,666 (+609; 19.7%) |
| P0d all 355 DLL-space slots (upper bound) | 861 (+110; 3.6%) | 2,985 (−110; −3.6%) | 4,826 (−161; −3.2%) | 1,973 (+916; 29.6%) |

Note: "hard-free" = not in the reverse closure of a remaining DLL, null or unwalked row. The Fox report's 748 is not comparable (its HARD set differs; §1.3).

**What remains after every DLL slot is modelled** (cutscene impure 2,985): boundary Fox-service rows **456** (incl. the P0a/P0c bodies, which are engine); vtable install/call (soft V) **396**; own dispatch/table/A_code sites (soft D/T/A) **252**; unwalked bodies (HARD, need a crawl) **56**; callee chains above these **1,825** (INF on the split of own sites; the buckets are exclusive in the order listed). So DLL slots are not the binding constraint for cutscene purity. Replay models give the biggest jump in hard-free functions; vtables, dispatch and the 56 unwalked bodies remain.

## 7. Open items

1. **293 vs 355 DLL slots (OPEN):** find which 62 Fox slots are excluded and why (P-reached-only?). Test: recount with the Fox filter.
2. **Hard-blocked definition (OPEN):** reconcile hard-free 1,057 here with Fox 748 (boundary rows counted as HARD in Fox?).
3. **`141EB6840` callees (OPEN, to make P0a CONFIRMED):** RTTI owners of the six `141AFxxxx` callees.
4. **`SetLastError` thunk (OPEN):** the import table of the Wine `kernel32.dll` on disk is unreadable with pefile; the ntdll target is unresolved.
5. **Callback label (OPEN):** source of `Callback@?A0x55ae6fab@editor`; the RTTI says LoopholeCallback. Check whether a second vtable or symbol exists for those bodies.
6. **Exactness of models (OPEN):** each `return-input`/`abort-trap` entry needs one harness case; the replay entries need a Layer 3 recording (from the four cutscene stoppages, `pes-re-cutscene-snap.md`).
7. **Ghidra bridge (127.0.0.1:8080):** the two checks returned nothing in this session; all verdicts above are from the image, RTTI and the ledger.

## 8. Files (scratch only)

- `<workdrive>/gf-ImportSlots-scratch/slots.json`: 401 absolute slots (live qword, module, export, sites, classes).
- `slot_table.md`: the §1.4 table (134 rows). `top30.json`: the §2 rows with thunk chains.
- `rank_corr.json`: corrected blocked counts per slot. `blocked.json`: H-sets per impure function.
- Scripts: `census.py`, `attr.py`, `blocked.py`, `table.py`, `unblock.py`, `models.py`, `full.py`, `cls2.py`, `p0.py`, `p0_evidence.py`, `p0b.py`, `p0b2.py`, `pread_slots.py`.
