section .text
global xtCopyFromUser
global xtCopyToUser
xtCopyFromUser:
    test r8, r8
    jz .success

.loop:
.inv_inst:
    mov al, [rdx]
    mov [rcx], al
    inc rcx,
    inc rdx
    dec r8
    jnz .loop

.success:
    xor rax, rax
    ret

.error:
    mov rax, 0xf00000000000000f
    ret

xtCopyToUser:
    test r8, r8
    jz .success

.loop:
.inv_inst:
    mov al, [rcx]
    mov [rdx], al
    inc rcx,
    inc rdx
    dec r8
    jnz .loop

.success:
    xor rax, rax
    ret

.error:
    mov rax, 0xf00000000000000f
    ret

section .ex_table
    dq xtCopyFromUser.inv_inst, xtCopyFromUser.error
    dq xtCopyToUser.inv_inst, xtCopyToUser.error
