#include "vsf.h"

// ARM semihosting: direct syscall to QEMU's host side, bypassing UART/trace.
// SYS_WRITE0 (0x04) = write zero-terminated string.
// Requires -semihosting on qemu-system-arm.
static void sh_write0(const char *s)
{
    register const char *r1 asm("r1") = s;
    asm volatile (
        "mov r0, #0x04\n"
        "bkpt 0xAB\n"
        : : "r"(r1) : "r0", "memory"
    );
}

// SYS_EXIT_EXTENDED (0x20) with app-exit reason (0x20026) + code.
static void sh_exit(int code)
{
    uint32_t args[2] = { 0x20026, (uint32_t)code };
    register uint32_t *r1 asm("r1") = args;
    asm volatile (
        "mov r0, #0x20\n"
        "bkpt 0xAB\n"
        : : "r"(r1) : "r0", "memory"
    );
    while (1) {}
}

int VSF_USER_ENTRY(void)
{
    sh_write0("[user_main] reached\n");
    sh_exit(42);
    return 0;
}
