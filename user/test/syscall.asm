section .text
global xtUserWriteFile

xtUserWriteFile:
    mov r10, rcx
    mov rax, 0x1
    syscall
    ret