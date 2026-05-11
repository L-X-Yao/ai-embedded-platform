// SPDX-License-Identifier: MIT
/*
 * double_buffer — ping-pong double buffer for DMA / producer-consumer
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Provides two caller-owned buffers that alternate between write and
 * read roles. Typical use: DMA fills the write buffer while the
 * application processes the read buffer, then swap().
 *
 * No dynamic allocation — caller provides both buffers.
 * Thread safety: caller must synchronize swap() with get_write/get_read.
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DOUBLE_BUFFER_OK        =  0,
    DOUBLE_BUFFER_ERR_PARAM = -1,
} double_buffer_err_t;

typedef struct {
    uint8_t *buf_a;       /* first buffer (caller-owned)  */
    uint8_t *buf_b;       /* second buffer (caller-owned) */
    size_t   buf_size;    /* size of each buffer in bytes  */
    uint8_t  write_idx;   /* 0 = buf_a is write, 1 = buf_b is write */
    bool     ready;       /* true after at least one swap  */
} double_buffer_t;

/** Initialize double buffer with two caller-provided buffers.
 *  Both buffers must be the same size.
 *  @thread_safe: no @isr_safe: no */
double_buffer_err_t double_buffer_init(double_buffer_t *ctx,
                                       uint8_t *buf_a,
                                       uint8_t *buf_b,
                                       size_t buf_size);

/** Get pointer to the current write buffer.
 *  Returns NULL on invalid ctx.
 *  @thread_safe: no @isr_safe: yes */
uint8_t *double_buffer_get_write(const double_buffer_t *ctx);

/** Get pointer to the current read buffer.
 *  Returns NULL on invalid ctx.
 *  @thread_safe: no @isr_safe: yes */
uint8_t *double_buffer_get_read(const double_buffer_t *ctx);

/** Get the size of each buffer.
 *  Returns 0 on invalid ctx.
 *  @thread_safe: no @isr_safe: yes */
size_t double_buffer_get_size(const double_buffer_t *ctx);

/** Swap write and read roles.
 *  After swap, previous write buffer becomes readable.
 *  Sets the ready flag on first call.
 *  @thread_safe: no @isr_safe: yes */
double_buffer_err_t double_buffer_swap(double_buffer_t *ctx);

/** Return true if at least one swap has occurred (read buffer has valid data).
 *  @thread_safe: no @isr_safe: yes */
bool double_buffer_is_ready(const double_buffer_t *ctx);

/** Reset to initial state (write_idx=0, ready=false).
 *  @thread_safe: no @isr_safe: no */
double_buffer_err_t double_buffer_reset(double_buffer_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DOUBLE_BUFFER_H */
