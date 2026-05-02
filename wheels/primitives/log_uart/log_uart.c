// SPDX-License-Identifier: MIT
/*
 * log_uart — UART output sink for log wheel via HAL injection.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_uart.h"

void log_uart_init(log_uart_t *u, log_uart_write_fn write, void *ctx) {
    if (!u) return;
    u->write = write;
    u->ctx = ctx;
}

void log_uart_sink(void *ctx, const char *line, size_t len) {
    log_uart_t *u = (log_uart_t *)ctx;
    if (!u || !u->write || !line || len == 0) return;
    u->write(u->ctx, (const uint8_t *)line, len);
}
