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
 *
 * Thread safety: one producer and one reader may run at once with no lock,
 * for example a DMA-complete ISR and the main loop.
 *
 *   producer  double_buffer_swap, double_buffer_get_write
 *   reader    double_buffer_get_read
 *   either    double_buffer_is_ready, double_buffer_get_size
 *   neither   double_buffer_init, double_buffer_reset, which need the other
 *             side stopped
 *
 * A side is one context. Two contexts that swap must lock around the call.
 *
 * What no lock here can give: a swap hands the buffer the reader is holding
 * to the producer. The reader must finish before the next swap, by keeping up
 * with the swap rate or by masking that interrupt while it reads.
 *
 * is_ready stays true after the first swap, so it does not say a new frame
 * has come. A changed get_read pointer does, unless two swaps fell between
 * looks.
 *
 * The barrier below orders CPU accesses only. If DMA fills a buffer on a core
 * with a data cache, such as a Cortex-M7, cache upkeep is the caller's job.
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keeps buffer accesses on their side of the role change. A compiler barrier
 * is enough on a single core, because a core sees its own accesses in program
 * order and an interrupt does not change that. If the two sides run on
 * different cores, define this as a real fence, such as __DMB(), before
 * including this header or on the command line.
 */
#ifndef DOUBLE_BUFFER_BARRIER
#if defined(__GNUC__)
#define DOUBLE_BUFFER_BARRIER() __asm__ volatile("" ::: "memory")
#else
#error "define DOUBLE_BUFFER_BARRIER() as this compiler's memory barrier"
#endif
#endif

typedef enum {
    DOUBLE_BUFFER_OK        =  0,
    DOUBLE_BUFFER_ERR_PARAM = -1,
} double_buffer_err_t;

/* ready is uint8_t, not bool, because GCC 13's -fsanitize=bool reads a volatile
 * bool as a plain byte, and a poll then never sees the swap. */
typedef struct {
    uint8_t *buf_a;       /* first buffer (caller-owned)  */
    uint8_t *buf_b;       /* second buffer (caller-owned) */
    size_t   buf_size;    /* size of each buffer in bytes  */
    volatile uint8_t write_idx; /* 0 = buf_a is write, 1 = buf_b is write */
    volatile uint8_t ready;     /* non-zero after at least one swap */
} double_buffer_t;

/** Initialize double buffer with two caller-provided buffers.
 *  Both buffers must be the same size. Finish before either side starts.
 *  @thread_safe: no @isr_safe: no */
double_buffer_err_t double_buffer_init(double_buffer_t *ctx,
                                       uint8_t *buf_a,
                                       uint8_t *buf_b,
                                       size_t buf_size);

/** Get pointer to the current write buffer. Producer side only.
 *  Returns NULL on invalid ctx.
 *  @thread_safe: single-producer safe (one producer beside one reader) @isr_safe: yes */
uint8_t *double_buffer_get_write(const double_buffer_t *ctx);

/** Get pointer to the current read buffer. Reader side only. It stays the
 *  read buffer only until the next swap: see the top of this file.
 *  Returns NULL on invalid ctx.
 *  @thread_safe: single-reader safe (one reader beside one producer) @isr_safe: yes */
uint8_t *double_buffer_get_read(const double_buffer_t *ctx);

/** Get the size of each buffer. Either side: it does not change after init.
 *  Returns 0 on invalid ctx.
 *  @thread_safe: yes (read only) @isr_safe: yes */
size_t double_buffer_get_size(const double_buffer_t *ctx);

/** Swap write and read roles. Producer side only.
 *  After swap, previous write buffer becomes readable. Whatever the producer
 *  wrote before this call is in the buffer by the time the reader sees it.
 *  Sets the ready flag on first call.
 *  @thread_safe: single-producer safe (one producer beside one reader) @isr_safe: yes */
double_buffer_err_t double_buffer_swap(double_buffer_t *ctx);

/** Return true if at least one swap has occurred (read buffer has valid data).
 *  Either side, and safe to poll in a loop.
 *  @thread_safe: yes (read only) @isr_safe: yes */
bool double_buffer_is_ready(const double_buffer_t *ctx);

/** Reset to initial state (write_idx=0, ready=false). Stop the other side first.
 *  @thread_safe: no @isr_safe: no */
double_buffer_err_t double_buffer_reset(double_buffer_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DOUBLE_BUFFER_H */
