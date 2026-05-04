// SPDX-License-Identifier: MIT
/*
 * bitmap — fixed-size bitmap (bit array) for resource tracking
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "bitmap.h"
#include "contract.h"
#include <string.h>

/* ------- helpers ------- */

static uint16_t byte_count(uint16_t bits)
{
    return (uint16_t)((bits + 7u) / 8u);
}

/* Mask out padding bits in the last byte so they are always zero. */
static void sanitize_tail(bitmap_t *bm)
{
    uint8_t tail = (uint8_t)(bm->bits & 7u);
    if (tail != 0u) {
        bm->data[byte_count(bm->bits) - 1u] &= (uint8_t)((1u << tail) - 1u);
    }
}

/* ------- public API ------- */

bitmap_err_t bitmap_init(bitmap_t *bm, uint8_t *storage, uint16_t bits)
{
    if (bm == NULL || storage == NULL || bits == 0u) {
        return BITMAP_ERR_PARAM;
    }
    bm->data = storage;
    bm->bits = bits;
    memset(storage, 0, byte_count(bits));
    ENSURE(bm->bits > 0);
    return BITMAP_OK;
}

bitmap_err_t bitmap_set(bitmap_t *bm, uint16_t bit)
{
    if (bm == NULL || bm->data == NULL) { return BITMAP_ERR_PARAM; }
    if (bit >= bm->bits)               { return BITMAP_ERR_RANGE; }
    bm->data[bit / 8u] |= (uint8_t)(1u << (bit & 7u));
    return BITMAP_OK;
}

bitmap_err_t bitmap_clear(bitmap_t *bm, uint16_t bit)
{
    if (bm == NULL || bm->data == NULL) { return BITMAP_ERR_PARAM; }
    if (bit >= bm->bits)               { return BITMAP_ERR_RANGE; }
    bm->data[bit / 8u] &= (uint8_t)~(1u << (bit & 7u));
    return BITMAP_OK;
}

bool bitmap_test(const bitmap_t *bm, uint16_t bit)
{
    if (bm == NULL || bm->data == NULL || bit >= bm->bits) {
        return false;
    }
    return (bm->data[bit / 8u] & (uint8_t)(1u << (bit & 7u))) != 0u;
}

bitmap_err_t bitmap_toggle(bitmap_t *bm, uint16_t bit)
{
    if (bm == NULL || bm->data == NULL) { return BITMAP_ERR_PARAM; }
    if (bit >= bm->bits)               { return BITMAP_ERR_RANGE; }
    bm->data[bit / 8u] ^= (uint8_t)(1u << (bit & 7u));
    return BITMAP_OK;
}

void bitmap_set_all(bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return; }
    memset(bm->data, 0xFF, byte_count(bm->bits));
    sanitize_tail(bm);
}

void bitmap_clear_all(bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return; }
    memset(bm->data, 0, byte_count(bm->bits));
}

int bitmap_find_first_set(const bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return -1; }
    uint16_t bytes = byte_count(bm->bits);
    for (uint16_t i = 0; i <= bytes; i++) {
        if (bm->data[i] != 0u) {
            for (uint8_t b = 0; b < 8u; b++) {
                uint16_t idx = (uint16_t)(i * 8u + b);
                if (idx >= bm->bits) { return -1; }
                if ((bm->data[i] & (uint8_t)(1u << b)) != 0u) {
                    return (int)idx;
                }
            }
        }
    }
    return -1;
}

int bitmap_find_first_clear(const bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return -1; }
    uint16_t bytes = byte_count(bm->bits);
    for (uint16_t i = 0; i < bytes; i++) {
        if (bm->data[i] != 0xFFu) {
            for (uint8_t b = 0; b < 8u; b++) {
                uint16_t idx = (uint16_t)(i * 8u + b);
                if (idx >= bm->bits) { return -1; }
                if ((bm->data[i] & (uint8_t)(1u << b)) == 0u) {
                    return (int)idx;
                }
            }
        }
    }
    return -1;
}

uint16_t bitmap_count_set(const bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return 0; }
    uint16_t count = 0;
    uint16_t bytes = byte_count(bm->bits);
    for (uint16_t i = 0; i < bytes; i++) {
        uint8_t v = bm->data[i];
        while (v != 0u) {
            count++;
            v &= (uint8_t)(v - 1u);  /* clear lowest set bit */
        }
    }
    /* Tail bits are always zero (sanitize_tail), so count is exact. */
    ENSURE(count <= bm->bits);
    return count;
}

uint16_t bitmap_count_clear(const bitmap_t *bm)
{
    if (bm == NULL || bm->data == NULL) { return 0; }
    return bm->bits - bitmap_count_set(bm);
}

uint16_t bitmap_size(const bitmap_t *bm)
{
    if (bm == NULL) { return 0; }
    return bm->bits;
}

int bitmap_alloc(bitmap_t *bm)
{
    int idx = bitmap_find_first_clear(bm);
    if (idx < 0) { return -1; }
    bm->data[(uint16_t)idx / 8u] |= (uint8_t)(1u << ((uint16_t)idx & 7u));
    ENSURE(idx >= 0 && (uint16_t)idx < bm->bits);
    return idx;
}

bitmap_err_t bitmap_free(bitmap_t *bm, uint16_t bit)
{
    return bitmap_clear(bm, bit);
}
