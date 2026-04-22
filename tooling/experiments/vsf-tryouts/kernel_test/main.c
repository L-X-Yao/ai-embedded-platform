#include "vsf.h"
#include <stdio.h>

// VSF simple_libc replaces _exit / open / write / abort. To probe reachability
// independently of VSF, use raw linux x86_64 syscalls via inline asm.
static __attribute__((noreturn)) void __raw_exit(int code)
{
    asm volatile ("syscall" :: "a"(60 /* SYS_exit */), "D"(code) : "memory");
    __builtin_unreachable();
}

int VSF_USER_ENTRY(void)
{
    __raw_exit(42);
    return 0;
}
