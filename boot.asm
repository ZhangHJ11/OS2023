    org 07c00h
    mov ax, cs
    mov ds, ax
    mov es, ax
    call DispStr
    jmp $
    
DispStr:
    mov ax, BootMess
    mov bp, ax
    mov cx, 10
    mov ax, 01301h
    mov bx, 000ch
    mov dl, 0
    int 10h
    ret
    
BootMess: db "Hello, os!"
times 510-($-$$) db 0
dw 0xaa55
