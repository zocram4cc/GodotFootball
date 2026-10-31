.intel_syntax noprefix
.text
.globl _start
_start:
  push rbx
  push rsi
  push rdi
  test r8, r8
  jz bad
  mov rdi, rdx            # out cursor
  mov r10, rdx            # buffer start
  lea r8, [rdx+r8-1]      # last writable char (the NUL goes at r8 at most)
  mov rsi, r9             # format cursor
  mov r11, [rsp+0x48]     # va_list: entry [rsp+0x30] under three pushes
next:
  movzx eax, byte ptr [rsi]
  inc rsi
  test al, al
  jz done
  cmp al, '%'
  jne emit
  xor ebx, ebx            # bl = width, bit 8 = zero flag
  movzx eax, byte ptr [rsi]
  inc rsi
  cmp al, '%'
  je emit
  cmp al, '0'
  jne wloop
  mov ebx, 0x100
  movzx eax, byte ptr [rsi]
  inc rsi
wloop:
  lea ecx, [rax-'0']
  cmp ecx, 9
  ja wdone
  movzx edx, bl
  imul edx, edx, 10
  add edx, ecx
  and ebx, 0x100
  or ebx, edx
  movzx eax, byte ptr [rsi]
  inc rsi
  jmp wloop
wdone:
  cmp al, 's'
  je dostr
  cmp al, 'd'
  je dosigned
  cmp al, 'i'
  je dosigned
  cmp al, 'u'
  je dounsigned
  cmp al, 'x'
  je dohex
bad:
  int3
emit:
  cmp rdi, r8
  jae bad
  mov [rdi], al
  inc rdi
  jmp next
dostr:
  test ebx, 0xff
  jnz bad
  mov r9, [r11]
  add r11, 8
  test r9, r9
  jz bad
sloop:
  mov al, [r9]
  test al, al
  jz next
  cmp rdi, r8
  jae bad
  mov [rdi], al
  inc rdi
  inc r9
  jmp sloop
dosigned:
  mov eax, [r11]
  add r11, 8
  test eax, eax
  jns number
  cmp rdi, r8
  jae bad
  mov byte ptr [rdi], '-'
  inc rdi
  neg eax
  test bl, bl
  jz number
  test ebx, 0x100
  jz bad                  # a space-padded negative needs the sign after the padding: not modelled
  dec bl                  # the sign counts toward the width
  jmp number
dounsigned:
  mov eax, [r11]
  add r11, 8
number:
  mov r9d, 10
  jmp digits
dohex:
  mov eax, [r11]
  add r11, 8
  mov r9d, 16
digits:
  xor ecx, ecx
dloop:
  xor edx, edx
  div r9d
  cmp dl, 10
  jb dl9
  add dl, 'a'-10-'0'
dl9:
  add dl, '0'
  push rdx
  inc ecx
  test eax, eax
  jnz dloop
pad:
  movzx eax, bl
  sub eax, ecx
  jbe digitsout
  mov dl, ' '
  test ebx, 0x100
  jz padloop
  mov dl, '0'
padloop:
  cmp rdi, r8
  jae bad
  mov [rdi], dl
  inc rdi
  dec eax
  jnz padloop
digitsout:
  pop rax
  cmp rdi, r8
  jae bad
  mov [rdi], al
  inc rdi
  dec ecx
  jnz digitsout
  jmp next
done:
  mov byte ptr [rdi], 0
  mov rax, rdi
  sub rax, r10
  pop rdi
  pop rsi
  pop rbx
  ret
