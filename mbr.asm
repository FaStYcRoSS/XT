org 0x7c00
bits 16

    mov bx, text
l1:
    mov ah, 0x0e
    mov al, [bx]
    int 0x10
    cmp al, 0
    je l2
    inc bx
    jmp l1
l2:
    jmp $

text: db "This disk doesn't load from MBR", 0

times 510 - ($ - $$) db 0

dw 0xaa55