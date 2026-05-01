/*
 * printf_tiny — minimal printf for resource-constrained embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Supports %d %u %x %X %o %c %s %p %% with width, zero-pad, left-align,
 * and long modifier. No float, no malloc, no static buffers.
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
 * @thread_safe: yes (no global state, reentrant)
 */
int printf_tiny(printf_tiny_putc_t putc, void *ctx,
                const char *fmt, ...);

/**
 * Format string to buffer (like snprintf).
 *
 * @param buf   Output buffer. If NULL, only counts characters.
 * @param size  Buffer size including NUL terminator.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ...   Format arguments.
 * @return      Total characters that would be written (excluding NUL),
 *              even if truncated.
 * @thread_safe: yes (no global state, reentrant)
 */
int snprintf_tiny(char *buf, uint16_t size, const char *fmt, ...);

/**
 * vprintf variant — takes va_list.
 *
 * @param putc  Output callback. If NULL, returns 0.
 * @param ctx   User context passed to putc.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ap    Argument list.
 * @return      Number of characters written.
 * @thread_safe: yes (no global state, reentrant)
 */
int vprintf_tiny(printf_tiny_putc_t putc, void *ctx,
                 const char *fmt, va_list ap);

/**
 * vsnprintf variant — takes va_list.
 *
 * @param buf   Output buffer. If NULL, only counts characters.
 * @param size  Buffer size including NUL terminator.
 * @param fmt   Format string. If NULL, returns 0.
 * @param ap    Argument list.
 * @return      Total characters that would be written (excluding NUL),
 *              even if truncated.
 * @thread_safe: yes (no global state, reentrant)
 */
int vsnprintf_tiny(char *buf, uint16_t size, const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* PRINTF_TINY_H */
