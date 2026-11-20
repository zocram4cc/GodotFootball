.intel_syntax noprefix
.text
.globl _start
# DWORD GetCurrentDirectoryW(DWORD nBufferLength = ecx, LPWSTR lpBuffer = rdx). The process's CurrentDirectory (PEB at gs:[0x60] ->
# RTL_USER_PROCESS_PARAMETERS at +0x20 -> the UNICODE_STRING at +0x38: Length in bytes at +0, Buffer at +0x40 (+8 of the string)) gives
# the length in characters (r11). A buffer that cannot hold the path and its NUL (nBufferLength <= len) returns len + 1 and writes
# nothing (the size query). Otherwise the path is copied to lpBuffer, NUL-terminated, and len is returned.
# Scratch: rax, r8, r10, r11 (r8 = the last character copied, 0 for an empty path; r10 = len on the copy path, the parameters on the
# size path; r11 = len on both paths).
_start:
  mov ecx, ecx
  mov r10, qword ptr gs:[0x60]
  mov r10, qword ptr [r10+0x20]
  movzx r11d, word ptr [r10+0x38]
  shr r11d, 1
  cmp rcx, r11
  ja copy
  lea rax, [r11+1]
  ret
copy:
  mov rax, qword ptr [r10+0x40]
  xor r8d, r8d
  xor r10d, r10d
next:
  cmp r10, r11
  jae done
  movzx r8d, word ptr [rax+r10*2]
  mov word ptr [rdx+r10*2], r8w
  inc r10
  jmp next
done:
  mov word ptr [rdx+r11*2], 0
  mov rax, r11
  ret
