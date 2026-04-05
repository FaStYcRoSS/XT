section .text
global xtUserWriteFile

xtUserWriteFile:
    mov r10, rcx
    syscall
    ret