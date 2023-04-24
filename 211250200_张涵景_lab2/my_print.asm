section .text
global my_puts
; int 80h
; eax = 4 ebx = 1 ecx = [address] edx = length
my_puts:
    push eax
    push ebx
    push ecx
    push edx
    mov eax, [esp+20]
    call strlen
    mov edx, eax
    mov ecx, [esp+20]
    mov eax, 4
    mov ebx, 1
    int 80h
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

strlen:
    push ebx
    mov ebx, eax
.next:
    cmp byte[ebx], 0
    jz .finish
    inc ebx
    jmp .next
.finish:
    sub ebx, eax
    mov eax, ebx
    pop ebx
    ret
