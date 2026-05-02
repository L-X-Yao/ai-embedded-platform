// SPDX-License-Identifier: MIT
/*
 * log_rtt — RTT-style output sink for log wheel via HAL injection.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_RTT_H
#define LOG_RTT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matches SEGGER_RTT_Write signature (unsigned len, not size_t). */
typedef unsigned (*log_rtt_write_fn)(void *ctx, unsigned channel,
                                     const void *data, unsigned len);

typedef struct {
    log_rtt_write_fn write;
    void            *ctx;
    unsigned         channel;
} log_rtt_t;

/* @thread_safe: no — call during init */
void log_rtt_init(log_rtt_t *r, log_rtt_write_fn write, void *ctx,
                  unsigned channel);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_rtt_t* as ctx when registering via log_add_sink().
 * @thread_safe: yes (delegates to user-provided write) */
void log_rtt_sink(void *ctx, const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_RTT_H */
