// SPDX-License-Identifier: MIT
/*
 * bitmap — fixed-size bitmap (bit array) for resource tracking
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Caller provides storage. Set/clear/test/toggle individual bits,
 * find-first-set/clear, alloc/free for slot management.
 * All operations O(n/8) worst case. No heap, no OS.
 * Thread safety: caller owns the bitmap_t struct.
 */

#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BITMAP_OK        =  0,
    BITMAP_ERR_PARAM = -1,
    BITMAP_ERR_RANGE = -2,
    BITMAP_ERR_FULL  = -3,
} bitmap_err_t;

typedef struct {
    uint8_t  *data;   /* caller-provided storage, (bits+7)/8 bytes */
    uint16_t  bits;   /* total number of bits */
} bitmap_t;

/** Initialize bitmap with caller-provided storage. Clears all bits.
 *  Minimum storage size: (bits + 7) / 8 bytes.
 *  @thread_safe: no @isr_safe: no */
bitmap_err_t bitmap_init(bitmap_t *bm, uint8_t *storage, uint16_t bits);

/** Set bit at index. @thread_safe: no @isr_safe: yes */
bitmap_err_t bitmap_set(bitmap_t *bm, uint16_t bit);

/** Clear bit at index. @thread_safe: no @isr_safe: yes */
bitmap_err_t bitmap_clear(bitmap_t *bm, uint16_t bit);

/** Test if bit is set. Returns false on NULL/out-of-range. @thread_safe: no @isr_safe: yes */
bool bitmap_test(const bitmap_t *bm, uint16_t bit);

/** Toggle bit at index. @thread_safe: no @isr_safe: yes */
bitmap_err_t bitmap_toggle(bitmap_t *bm, uint16_t bit);

/** Set all bits. @thread_safe: no @isr_safe: yes */
void bitmap_set_all(bitmap_t *bm);

/** Clear all bits. @thread_safe: no @isr_safe: yes */
void bitmap_clear_all(bitmap_t *bm);

/** Find index of first set bit. Returns -1 if none. @thread_safe: no @isr_safe: yes */
int bitmap_find_first_set(const bitmap_t *bm);

/** Find index of first clear bit. Returns -1 if none. @thread_safe: no @isr_safe: yes */
int bitmap_find_first_clear(const bitmap_t *bm);

/** Count number of set bits. @thread_safe: no @isr_safe: yes */
uint16_t bitmap_count_set(const bitmap_t *bm);

/** Count number of clear bits. @thread_safe: no @isr_safe: yes */
uint16_t bitmap_count_clear(const bitmap_t *bm);

/** Return total number of bits. @thread_safe: no @isr_safe: yes */
uint16_t bitmap_size(const bitmap_t *bm);

/** Allocate: find first clear bit, set it, return its index.
 *  Returns -1 if bitmap is full. @thread_safe: no @isr_safe: yes */
int bitmap_alloc(bitmap_t *bm);

/** Free: clear bit at index (release a slot). @thread_safe: no @isr_safe: yes */
bitmap_err_t bitmap_free(bitmap_t *bm, uint16_t bit);

#ifdef __cplusplus
}
#endif

#endif /* BITMAP_H */
