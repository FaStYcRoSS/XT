section .text
global xtCopyFromUser
global xtCopyToUser
global xtGetArgsCount
global xtUserGetStringLength

xtCopyFromUser:
    ; rcx = dst (ядро), rdx = src (пользователь), r8 = size
    test r8, r8
    jz .cfuSuccess
.cfuLoop:
.cfuFault1:
    mov al, [rdx]       ; читаем из пользователя
.cfuFault2:
    mov [rcx], al       ; пишем в ядро
    inc rcx
    inc rdx
    dec r8
    jnz .cfuLoop
.cfuSuccess:
    xor rax, rax
    ret
.cfuError:
    mov rax, 0xf00000000000000f
    ret

xtCopyToUser:
    ; rcx = dst (пользователь), rdx = src (ядро), r8 = size
    test r8, r8
    jz .ctuSuccess
.ctuLoop:
.ctuFault1:
    mov al, [rdx]       ; читаем из ядра
.ctuFault2:
    mov [rcx], al       ; пишем в пользователя
    inc rcx
    inc rdx
    dec r8
    jnz .ctuLoop
.ctuSuccess:
    xor rax, rax
    ret
.ctuError:
    mov rax, 0xf00000000000000f
    ret

xtGetArgsCount:
    ; rcx = const char** args, rdx = uint64_t* argc
    test rcx, rcx
    jz .gacSuccess
.gacFault:
    mov rax, [rcx]      ; берём указатель из массива
    test rax, rax       ; NULL = конец списка
    jz .gacSuccess
    add rcx, 8
    inc qword [rdx]
    jmp .gacFault
.gacSuccess:
    xor rax, rax
    ret
.gacError:
    mov rax, 0xf00000000000000f
    ret

xtUserGetStringLength:
    ; rcx = const char* str (пользователь), rdx = uint64_t* len (ядро)
    test rcx, rcx
    jz .uslSuccess
.uslFault:
    cmp byte [rcx], 0
    je .uslSuccess
    inc qword [rdx]
    inc rcx
    jmp .uslFault
.uslSuccess:
    xor rax, rax
    ret
.uslError:
    mov rax, 0xf00000000000000f
    ret

section .ex_table
    dq xtCopyFromUser.cfuFault1,  xtCopyFromUser.cfuError
    dq xtCopyToUser.ctuFault1,    xtCopyToUser.ctuError
    dq xtCopyFromUser.cfuFault2,  xtCopyFromUser.cfuError
    dq xtCopyToUser.ctuFault2,    xtCopyToUser.ctuError
    dq xtGetArgsCount.gacFault,  xtGetArgsCount.gacError
    dq xtUserGetStringLength.uslFault, xtUserGetStringLength.uslError