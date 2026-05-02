// SPDX-License-Identifier: MIT
/*
 * log_uart — UART output sink for log wheel via HAL injection.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      none
 * Instances: multi
 */

#ifndef LOG_UART_H
#define LOG_UART_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*log_uart_write_fn)(void *ctx, const uint8_t *data, size_t len);

typedef struct {
    log_uart_write_fn write;
    void             *ctx;
} log_uart_t;

/* @thread_safe: no — call during init @isr_safe: no */
void log_uart_init(log_uart_t *u, log_uart_write_fn write, void *ctx);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_uart_t* as ctx when registering via log_add_sink().
 * @thread_safe: yes (delegates to user-provided write) @isr_safe: no */
void log_uart_sink(void *ctx, const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_UART_H */
