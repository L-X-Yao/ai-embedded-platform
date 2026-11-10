// SPDX-License-Identifier: MIT
/*
 * ring_buffer — byte-level circular buffer for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Circular buffer with a write index and a read index. Supports partial
 * write/read, peek, and skip. Suitable for UART RX buffering, protocol
 * framing, and producer-consumer byte streams. Caller provides backing
 * storage. No heap.
 *
 * Thread safety: one producer and one consumer may run at once with no lock,
 * for example a UART ISR and the main loop, in either direction.
 *
 *   producer  ring_buffer_write
 *   consumer  ring_buffer_read, ring_buffer_peek, ring_buffer_skip,
 *             ring_buffer_reset
 *   either    ring_buffer_count, ring_buffer_free, ring_buffer_is_empty,
 *             ring_buffer_is_full
 *   neither   ring_buffer_init, which must finish before either side starts
 *
 * A side is one context. Two writers, or two readers, must lock around their
 * calls. Calling a function from the wrong side corrupts the buffer. From a
 * third context the "either" functions are safe, but the answer is only an
 * estimate within 0..capacity.
 *
 * This holds where a 32-bit aligned load or store is a single access, which
 * is every Cortex-M. On an 8-bit or 16-bit MCU the indexes can tear, so mask
 * interrupts around the calls there.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keeps the byte copy on its side of the index access. Neither side locks, so
 * the copy must not move past the index that tells the other side it is done.
 *
 * A compiler barrier is enough on a single core, because a core sees its own
 * accesses in program order and an interrupt does not change that. If the two
 * sides run on different cores, define this as a real fence, such as __DMB(),
 * before including this header or on the command line.
 */
#ifndef RING_BUFFER_BARRIER
#if defined(__GNUC__)
#define RING_BUFFER_BARRIER() __asm__ volatile("" ::: "memory")
#else
#error "define RING_BUFFER_BARRIER() as this compiler's memory barrier"
#endif
#endif

typedef enum {
    RING_BUFFER_OK         =  0,
    RING_BUFFER_ERR_PARAM = -1,
    RING_BUFFER_ERR_FULL   = -2,
    RING_BUFFER_ERR_EMPTY  = -3,
} ring_buffer_err_t;

/*
 * head and tail run over 0 .. 2*capacity-1, not 0 .. capacity-1, so that a
 * full buffer and an empty one differ and all of capacity is usable with no
 * shared count. Use the functions below to read the fill level.
 */
typedef struct {
    uint8_t  *buf;               /* backing storage (caller-owned)      */
    uint16_t  capacity;          /* total size in bytes                 */
    volatile uint32_t head;      /* write index: only the producer stores it */
    volatile uint32_t tail;      /* read index: only the consumer stores it  */
} ring_buffer_t;

/* ---------- ACSL formal specifications (Frama-C WP) ---------- */

/* These state each call on its own, with the other side not running. */

/*@ logic integer rb_fill(ring_buffer_t *rb) =
      rb->head >= rb->tail ? rb->head - rb->tail
                           : rb->head + 2 * rb->capacity - rb->tail;
*/

/*@ predicate rb_valid(ring_buffer_t *rb) =
      \valid(rb) &&
      rb->capacity > 0 &&
      \valid(rb->buf + (0 .. rb->capacity - 1)) &&
      rb->head < 2 * rb->capacity &&
      rb->tail < 2 * rb->capacity &&
      rb_fill(rb) <= rb->capacity;
*/

/** Initialize ring buffer with caller-provided storage. Finish before either side starts. @thread_safe: no @isr_safe: no */
/*@ requires \valid(rb);
    requires \valid(buf + (0 .. capacity - 1));
    requires capacity > 0;
    assigns rb->buf, rb->capacity, rb->head, rb->tail;
    ensures rb->buf == buf;
    ensures rb->capacity == capacity;
    ensures rb->head == 0;
    ensures rb->tail == 0;
    ensures \result == RING_BUFFER_OK;
*/
ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t capacity);

/** Write up to len bytes. Returns bytes actually written. Producer side only. @thread_safe: single-producer safe (one writer beside one reader) @isr_safe: yes */
/*@ requires rb_valid(rb);
    requires \valid_read(data + (0 .. len - 1));
    assigns rb->buf[0 .. rb->capacity - 1], rb->head;
    ensures rb_fill(rb) <= rb->capacity;
    ensures rb->head < 2 * rb->capacity;
    ensures \result <= len;
    ensures \result <= rb->capacity;
*/
uint16_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint16_t len);

/** Read up to len bytes (consuming). Returns bytes actually read. Consumer side only. @thread_safe: single-consumer safe (one reader beside one writer) @isr_safe: yes */
/*@ requires rb_valid(rb);
    requires \valid(data + (0 .. len - 1));
    assigns data[0 .. len - 1], rb->tail;
    ensures rb_fill(rb) <= rb->capacity;
    ensures rb->tail < 2 * rb->capacity;
    ensures \result <= len;
    ensures \result <= rb->capacity;
*/
uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len);

/** Read up to len bytes without consuming. Returns bytes peeked. Consumer side only. @thread_safe: single-consumer safe (one reader beside one writer) @isr_safe: yes */
/*@ requires rb_valid(rb);
    requires \valid(data + (0 .. len - 1));
    assigns data[0 .. len - 1];
    ensures \result <= len;
    ensures \result <= rb_fill(rb);
*/
uint16_t ring_buffer_peek(const ring_buffer_t *rb, uint8_t *data, uint16_t len);

/** Discard up to len bytes. Returns bytes actually skipped. Consumer side only. @thread_safe: single-consumer safe (one reader beside one writer) @isr_safe: yes */
/*@ requires rb_valid(rb);
    assigns rb->tail;
    ensures rb_fill(rb) <= rb->capacity;
    ensures rb->tail < 2 * rb->capacity;
    ensures \result <= len;
*/
uint16_t ring_buffer_skip(ring_buffer_t *rb, uint16_t len);

/**
 * Return number of bytes currently stored. Either side. The consumer may find
 * more than this by the time it reads, never fewer.
 * @thread_safe: single-producer and single-consumer safe @isr_safe: yes
 */
/*@ requires rb_valid(rb);
    assigns \nothing;
    ensures \result == rb_fill(rb);
*/
uint16_t ring_buffer_count(const ring_buffer_t *rb);

/**
 * Return number of free bytes. Either side. The producer may find more than
 * this by the time it writes, never fewer.
 * @thread_safe: single-producer and single-consumer safe @isr_safe: yes
 */
/*@ requires rb_valid(rb);
    assigns \nothing;
    ensures \result == rb->capacity - rb_fill(rb);
*/
uint16_t ring_buffer_free(const ring_buffer_t *rb);

/** Return true if buffer is empty. Either side. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
/*@ requires rb_valid(rb);
    assigns \nothing;
    ensures \result == (rb_fill(rb) == 0);
*/
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/** Return true if buffer is full. Either side. @thread_safe: single-producer and single-consumer safe @isr_safe: yes */
/*@ requires rb_valid(rb);
    assigns \nothing;
    ensures \result == (rb_fill(rb) == rb->capacity);
*/
bool ring_buffer_is_full(const ring_buffer_t *rb);

/**
 * Drop everything stored. Consumer side only: it moves the read index up to
 * the write index, so bytes the producer adds meanwhile are either dropped or
 * kept whole. The indexes do not go back to 0.
 * @thread_safe: single-consumer safe (one reader beside one writer) @isr_safe: yes
 */
/*@ requires rb_valid(rb);
    assigns rb->tail;
    ensures rb->tail == rb->head;
*/
void ring_buffer_reset(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
