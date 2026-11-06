// SPDX-License-Identifier: MIT
/*
 * printf_tiny — minimal printf for resource-constrained embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Supports %d %u %x %X %o %c %s %p %% with width, zero-pad, left-align,
 * and long modifier. No float, no malloc, no static buffers.
 * Anything else ends formatting: the first unsupported specifier and all that
 * follows it are copied literally, and no further argument is read.
 * Thread safety: all functions are reentrant (no global state).
 */

#ifndef PRINTF_TINY_H
#define PRINTF_TINY_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Largest field width, at most INT_MAX. A wider one in a format string is cut
 * to this, which bounds the padding one conversion can emit. 4095 is the least
 * a C99 printf has to support for one conversion.
 */
#ifndef PRINTF_TINY_MAX_WIDTH
#define PRINTF_TINY_MAX_WIDTH 4095
#endif

/* Lets the compiler check arguments against the format. It knows all of C's
 * conversions, not this subset, so it says nothing about %f or %zu. */
#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_TINY_FORMAT(fmt_index, first_arg) \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define PRINTF_TINY_FORMAT(fmt_index, first_arg)
#endif

/**
 * Character output callback type.
 *
 * @param ctx   User-supplied context (e.g. UART handle).
 * @param c     Character to output.
 */
typedef void (*printf_tiny_putc_t)(void *ctx, char c);

/**
 * Format string to output via putc callback.
 *
 * @param putc  Output callback. If NULL, returns 0.
 * @param ctx   User context passed to putc.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ...   Format arguments.
 * @return      Number of characters written.
 * @thread_safe: yes (no global state, reentrant) @isr_safe: yes
 */
int printf_tiny(printf_tiny_putc_t putc, void *ctx,
                const char *fmt, ...) PRINTF_TINY_FORMAT(3, 4);

/**
 * Format string to buffer (like snprintf).
 *
 * @param buf   Output buffer. If NULL, only counts characters.
 * @param size  Buffer size including NUL terminator.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ...   Format arguments.
 * @return      Total characters that would be written (excluding NUL),
 *              even if truncated.
 * @thread_safe: yes (no global state, reentrant) @isr_safe: yes
 */
int snprintf_tiny(char *buf, uint16_t size, const char *fmt, ...)
    PRINTF_TINY_FORMAT(3, 4);

/**
 * vprintf variant — takes va_list.
 *
 * @param putc  Output callback. If NULL, returns 0.
 * @param ctx   User context passed to putc.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ap    Argument list.
 * @return      Number of characters written.
 * @thread_safe: yes (no global state, reentrant) @isr_safe: yes
 */
int vprintf_tiny(printf_tiny_putc_t putc, void *ctx,
                 const char *fmt, va_list ap) PRINTF_TINY_FORMAT(3, 0);

/**
 * vsnprintf variant — takes va_list.
 *
 * @param buf   Output buffer. If NULL, only counts characters.
 * @param size  Buffer size including NUL terminator.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ap    Argument list.
 * @return      Total characters that would be written (excluding NUL),
 *              even if truncated.
 * @thread_safe: yes (no global state, reentrant) @isr_safe: yes
 */
int vsnprintf_tiny(char *buf, uint16_t size, const char *fmt, va_list ap)
    PRINTF_TINY_FORMAT(3, 0);

#ifdef __cplusplus
}
#endif

#endif /* PRINTF_TINY_H */
