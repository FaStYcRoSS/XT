section .text
global xtSwitchToThread
global xtSwitchTo
global xtDivizionException
global xtSyscallHandler
extern currentThread
extern xtExceptionHandler
extern xtSyscallArray

%macro isr_err_stub 1
global isr_stub_%1
isr_stub_%1:
    push qword %1
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr2
    push rax

    mov rax, [rel currentThread]
    mov [rax], rsp
    mov rcx, cr3
    mov rax, [rax+32]
    mov rcx, [rax+0x8]

    sub rsp, 40
    call xtExceptionHandler
    add rsp, 40

    mov rax, [rel currentThread]
    mov rsp, [rax]
    mov rax, [rax+32]
    mov rax, [rax+0x8]
    mov cr3, rax

    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq
%endmacro
; if writing for 64-bit, use iretq instead
%macro isr_no_err_stub 1
isr_stub_%1:
global isr_stub_%1
    push qword 0
    push qword %1
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rax, cr2
    push rax

    mov rax, [rel currentThread]
    mov [rax], rsp
    mov rcx, cr3
    mov rax, [rax+32]
    mov rcx, [rax+0x8]

    sub rsp, 40
    call xtExceptionHandler
    add rsp, 40

    mov rax, [rel currentThread]
    mov rsp, [rax]
    mov rax, [rax+32]
    mov rax, [rax+0x8]
    mov cr3, rax

    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq
%endmacro

isr_no_err_stub 0
isr_no_err_stub 1
isr_no_err_stub 2
isr_no_err_stub 3
isr_no_err_stub 4
isr_no_err_stub 5
isr_no_err_stub 6
isr_no_err_stub 7
isr_err_stub    8
isr_no_err_stub 9
isr_err_stub    10
isr_err_stub    11
isr_err_stub    12
isr_err_stub    13
isr_err_stub    14
isr_no_err_stub 15
isr_no_err_stub 16
isr_err_stub    17
isr_no_err_stub 18
isr_no_err_stub 19
isr_no_err_stub 20
isr_no_err_stub 21
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28
isr_no_err_stub 29
isr_err_stub    30
isr_no_err_stub 31

isr_no_err_stub 32

isr_no_err_stub 40

xtSyscallHandler:

    push rcx
    push r11
    mov r11, rsp
    mov rcx, [rel currentThread]
    mov rsp, [rcx+40]
    mov rcx, r10
    push r11
    lea r10, [rel xtSyscallArray]
    call [r10+rax*8]
    pop r11
    mov rsp, r11
    pop r11
    pop rcx
    o64 sysret




xtSwitchToThread:
    int 0x20
    ret

xtSwitchTo:
    mov rax, [rel currentThread]
    mov rsp, [rax]
    mov rax, [rax+32]
    mov rax, [rax+0x8]
    mov cr3, rax
    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq