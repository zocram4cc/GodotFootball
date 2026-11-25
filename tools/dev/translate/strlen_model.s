.intel_syntax noprefix
.text
.globl _start
# size_t strlen(const char *s = rcx), the import model of import_stubs.json (code_hex): rax = the count of bytes before the NUL.
# It reads the string only: no write, and rcx is left as the caller passed it.
_start:
  xor eax, eax
scan:
  cmp byte ptr [rcx+rax], 0
  je done
  inc rax
  jmp scan
done:
  ret
