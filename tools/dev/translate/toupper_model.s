.intel_syntax noprefix
.text
.globl _start
# int toupper(int c = ecx): c - 0x20 for c in 'a'..'z', else c. The result is in eax (rax zero-extended); edx is the
# scratch the range test uses (c - 'a'). No stack use, no memory write.
_start:
  mov eax, ecx
  lea edx, [rax-0x61]
  cmp edx, 25
  ja done
  sub eax, 0x20
done:
  ret
