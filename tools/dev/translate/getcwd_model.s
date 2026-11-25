.intel_syntax noprefix
.text
.globl _start
# DWORD GetCurrentDirectoryW(DWORD size = ecx, LPWSTR buf = rdx), the import model of import_stubs.json (code_hex). When size holds the
# drive root "C:\" and its terminator (4 wide chars), those 8 bytes are stored at rdx and rax = 3 (the length without the terminator).
# When it does not (the length query GetCurrentDirectoryW(0, NULL)), nothing is written and rax = 4, the required size with the terminator.
_start:
  cmp ecx, 4
  jb nowrite
  mov rax, 0x5C003A0043
  mov [rdx], rax
  mov eax, 3
  ret
nowrite:
  mov eax, 4
  ret
