section .vdso progbits alloc exec nowrite
global xtUserExit

xtUserExit:
    mov r10, -1
    mov rdx, rax
    mov rax, 0x0
    syscall
    jmp $
