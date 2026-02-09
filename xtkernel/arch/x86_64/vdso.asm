section .vdso progbits alloc exec nowrite
global xtUserExit

xtUserExit:
    mov rax, 0
    syscall
