section .text
global xtUserWriteFile
global xtUserReadFile
global xtUserCreatePipe
global xtUserDuplicateHandle
global xtUserExecuteProgram
global xtUserCreateProcess
global xtUserTerminateThread

xtUserTerminateThread:
    mov r10, rcx
    mov rax, 0x0
    syscall
    ret

xtUserWriteFile:
    mov r10, rcx
    mov rax, 0x1
    syscall
    ret

xtUserReadFile:
    mov r10, rcx
    mov rax, 0x2
    syscall
    ret

xtUserCreatePipe:
    mov r10, rcx
    mov rax, 0x3
    syscall
    ret

xtUserCreateProcess:
    mov r10, rcx
    mov rax, 0x4
    syscall
    ret

xtUserDuplicateHandle:
    mov r10, rcx
    mov rax, 0x5
    syscall
    ret

xtUserExecuteProgram:
    mov r10, rcx
    mov rax, 0x6
    syscall
    ret
