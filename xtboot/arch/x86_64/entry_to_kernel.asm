
bits 64                 ; Явно указываем 64-битную архитектуру
section .text           ; Объявляем секцию кода

global entry_to_kernel
entry_to_kernel:

    mov rsp, rcx
    mov rax, rdx
    mov rcx, r8
    jmp rax

