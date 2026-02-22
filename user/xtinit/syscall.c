#include <xt/user.h>
#include <stdint.h>

XTResult xtUserWriteFile(uint64_t file, const void* str, uint64_t length, uint64_t* out) {
    uint64_t syscall_id = 0x1;
    XTResult ret = 0;

    // Явно привязываем переменные к регистрам согласно твоему ABI
    register uint64_t r10 asm("r10") = file;
    register const void* rdx asm("rdx") = str;
    register uint64_t r8  asm("r8")  = length;
    register uint64_t* r9  asm("r9")  = out;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(syscall_id), "r"(r10), "r"(rdx), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory" // syscall затирает rcx и r11
    );

    return ret;
}