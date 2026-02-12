section .vdso progbits alloc exec nowrite
global xtUserExit

xtUserExit:
    syscall
