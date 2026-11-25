.intel_syntax noprefix
.text
.globl _start
# int strncmp(const char *s1 = rcx, const char *s2 = rdx, size_t n = r8): the difference of the first unsigned bytes that
# differ (s1 - s2), or 0 when the strings agree to a NUL or for n bytes. No stack use, no memory write. r9 holds the last
# byte of s2 read; rcx, rdx advance over the bytes that agree; r8 is the count left.
_start:
  test r8, r8
  jz zero
L:
  movzx eax, byte ptr [rcx]
  movzx r9d, byte ptr [rdx]
  cmp eax, r9d
  jne diff
  test eax, eax
  jz zero
  inc rcx
  inc rdx
  dec r8
  jnz L
zero:
  xor eax, eax
  ret
diff:
  sub eax, r9d
  ret
