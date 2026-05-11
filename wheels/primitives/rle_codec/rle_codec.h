// SPDX-License-Identifier: MIT
/*
 * rle_codec — TODO: one-line description
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef RLE_CODEC_H
#define RLE_CODEC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    RLE_CODEC_OK        =  0,
    RLE_CODEC_ERR_PARAM = -1,
} rle_codec_err_t;

/* ---------- instance ---------- */

typedef struct {
    int _placeholder; /* internal: replace with real state */
} rle_codec_t;

/* ---------- API ---------- */

/** Initialize instance. @thread_safe: no @isr_safe: no */
rle_codec_err_t rle_codec_init(rle_codec_t *ctx);

/* TODO: add more API — see api-conventions.md for rules.
 * Add @thread_safe and @isr_safe annotations to every function declaration.
 * Mark internal struct fields with a comment. */

#ifdef __cplusplus
}
#endif

#endif /* RLE_CODEC_H */
