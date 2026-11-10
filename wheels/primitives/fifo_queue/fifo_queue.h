// SPDX-License-Identifier: MIT
/*
 * fifo_queue — typed fixed-size FIFO queue with caller-provided storage
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Circular queue for fixed-size elements (structs, ints, etc.).
 * Complements byte_buffer (linear bytes) and ring_buffer (byte-level
 * circular). Caller provides backing storage. No heap, fully reentrant.
 *
 * Thread safety: one producer and one consumer may run at once with no lock,
 * for example an ISR and the main loop, in either direction.
 *
 *   producer  fifo_queue_push
 *   consumer  fifo_queue_pop, fifo_queue_peek, fifo_queue_clear
 *   either    fifo_queue_count, fifo_queue_free, fifo_queue_is_empty,
 *             fifo_queue_is_full
 *   neither   fifo_queue_init, which must finish before either side starts
 *
 * A side is one context. Two producers, or two consumers, must lock around
 * their calls. Calling a function from the wrong side corrupts the queue. From
 * a third context the "either" functions are safe, but the answer is only an
 * estimate within 0..capacity.
 *
 * This holds where a 32-bit aligned load or store is a single access, which
 * is every Cortex-M. On an 8-bit or 16-bit MCU the indexes can tear, so mask
 * interrupts around the calls there.
 */

#ifndef FIFO_QUEUE_H
#define FIFO_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keeps the element copy on its side of the index access. Neither side locks,
 * so the copy must not move past the index that tells the other side it is
 * done.
 *
 * A compiler barrier is enough on a single core, because a core sees its own
 * accesses in program order and an interrupt does not change that. If the two
 * sides run on different cores, define this as a real fence, such as __DMB(),
 * before including this header or on the command line.
 */
#ifndef FIFO_QUEUE_BARRIER
#if defined(__GNUC__)
#define FIFO_QUEUE_BARRIER() __asm__ volatile("" ::: "memory")
#else
#error "define FIFO_QUEUE_BARRIER() as this compiler's memory barrier"
#endif
#endif

typedef enum {
    FIFO_QUEUE_OK         =  0,
    FIFO_QUEUE_ERR_PARAM = -1,
    FIFO_QUEUE_ERR_FULL   = -2,
    FIFO_QUEUE_ERR_EMPTY  = -3,
} fifo_queue_err_t;

/*
 * head and tail run over 0 .. 2*capacity-1, not 0 .. capacity-1, so that a
 * full queue and an empty one differ and all of capacity is usable with no
 * shared count. Use the functions below to read the fill level.
 */
typedef struct {
    uint8_t *buf;            /* backing storage (caller-owned) */
    uint16_t capacity;       /* max number of elements          */
    uint16_t elem_size;      /* size of each element in bytes   */
    volatile uint32_t head;  /* read index: only the consumer stores it  */
    volatile uint32_t tail;  /* write index: only the producer stores it */
} fifo_queue_t;

/** Finish before either side starts. @thread_safe: no @isr_safe: no */
fifo_queue_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size);

/** Producer side only. @thread_safe: single-producer safe (one producer beside one consumer) @isr_safe: yes */
fifo_queue_err_t fifo_queue_push(fifo_queue_t *q, const void *item);

/** Consumer side only. @thread_safe: single-consumer safe (one consumer beside one producer) @isr_safe: yes */
fifo_queue_err_t fifo_queue_pop(fifo_queue_t *q, void *item);

/** Consumer side only. @thread_safe: single-consumer safe (one consumer beside one producer) @isr_safe: yes */
fifo_queue_err_t fifo_queue_peek(const fifo_queue_t *q, void *item);

/** Either side. The consumer may find more than this by the time it pops, never fewer. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
uint16_t fifo_queue_count(const fifo_queue_t *q);

/** Either side. The producer may find more than this by the time it pushes, never fewer. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
uint16_t fifo_queue_free(const fifo_queue_t *q);

/** Either side. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
bool fifo_queue_is_empty(const fifo_queue_t *q);

/** Either side. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
bool fifo_queue_is_full(const fifo_queue_t *q);

/**
 * Drop everything stored. Consumer side only: it moves the read index up to
 * the write index, so an element the producer adds meanwhile is either dropped
 * or kept whole. The indexes do not go back to 0.
 * @thread_safe: single-consumer safe (one consumer beside one producer) @isr_safe: yes
 */
void fifo_queue_clear(fifo_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_QUEUE_H */
